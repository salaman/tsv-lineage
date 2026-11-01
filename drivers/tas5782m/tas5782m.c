// SPDX-License-Identifier: GPL-2.0-only
/*
 * snd-soc-tas5782m.c — ASoC CODEC driver for Texas Instruments TAS5782M
 *
 * Target hardware: Lenovo CD-18781Y (ThinkSmart View), APQ8053/MSM8953,
 * TAS5782M at I2C bus 1 address 0x49 (DT: "1-0049").
 *
 * Adapted from rickx/ubuntu-thinksmart and Kacper Adler's postmarketOS
 * submission; see drivers/tas5782m/README.md in the device tree for revisions
 * and changes.
 * Requires board DSP tuning and bounds digital volume by its configured level.
 * DSP updates use TI SLASEG8A's coefficient-swap protocol with checked I/O.
 *
 * Based on:
 *   - Earlier TI smart-amp ASoC driver patterns by Andy Liu <andy-liu@ti.com>,
 *     Daniel Beer <daniel.beer@igorinstitute.com>,
 *     Felix Kaechele <felix@kaechele.ca>
 *   - TAS5782M reference driver posted by TI on E2E forum
 *   - Lenovo CD-18781Y OSS package (Android kernel DTS)
 *
 * Programming must wait for BCLK after the CPU DAI starts. DAPM POST_PMU
 * precedes that clock and cannot perform this initialization.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tas5782m_priv.h"

/* Include debug layer AFTER priv struct so non-debug stubs see the full type */
#include "tas5782m_dbg.h"

/* ========================================================================= */
/* Book / Page multiplexer helper                                             */
/* ========================================================================= */

static int tas5782m_set_book_page(struct tas5782m_priv *priv,
				  u8 book, u8 page)
{
	int ret;

	ret = tas5782m_write_traced(priv, TAS5782M_REG_PAGE, 0x00);
	if (ret)
		return ret;

	ret = tas5782m_write_traced(priv, TAS5782M_REG_BOOK, book);
	if (ret)
		return ret;

	ret = tas5782m_write_traced(priv, TAS5782M_REG_PAGE, page);
	if (ret)
		return ret;

	tas5782m_trace(priv, TAS5782M_PHASE_REG,
		       "set book=0x%02x page=0x%02x\n", book, page);
	return 0;
}

/* ========================================================================= */
/* Volume                                                                     */
/* ========================================================================= */

/* Attenuate relative to the board tuning; never replace it with unity gain. */
static u32 tas5782m_gain_code(struct tas5782m_priv *priv, int channel,
			    unsigned int volume)
{
	if (volume == TAS5782M_VOLUME_MIN)
		return 0;
	return ((u64)priv->volume_max[channel] * tas5782m_volume[volume]) >> 23;
}

static int tas5782m_wait_swap(struct tas5782m_priv *priv)
{
	unsigned int flag;
	int ret;

	ret = tas5782m_set_book_page(priv, TAS5782M_VOL_BOOK, TAS5782M_SWAP_PAGE);
	if (ret)
		return ret;
	return regmap_read_poll_timeout(priv->regmap, TAS5782M_SWAP_REG + 3,
					flag, flag == 0, 100, 20000);
}

static int tas5782m_check_volume(struct tas5782m_priv *priv)
{
	u8 actual[8];
	u32 desired;
	int ret;
	int i;

	ret = tas5782m_set_book_page(priv, TAS5782M_VOL_BOOK, TAS5782M_VOL_PAGE);
	if (ret)
		return ret;
	ret = regmap_bulk_read(priv->regmap, TAS5782M_VOL_CH_LEFT,
			       actual, sizeof(actual));
	if (ret)
		return ret;

	for (i = 0; i < 8; ++i) {
		desired = tas5782m_gain_code(priv, i / 4, priv->vol[i / 4]);
		if (actual[i] != (u8)(desired >> (24 - 8 * (i % 4))))
			return -EIO;
	}
	return 0;
}

static int tas5782m_set_volume(struct tas5782m_priv *priv)
{
	static const u8 swap[] = { 0, 0, 0, 1 };
	u8 values[8];
	u32 desired;
	int ret;
	int restore;
	int i;

	ret = tas5782m_wait_swap(priv);
	if (ret)
		goto out;
	for (i = 0; i < 8; ++i) {
		desired = tas5782m_gain_code(priv, i / 4, priv->vol[i / 4]);
		values[i] = desired >> (24 - 8 * (i % 4));
	}
	ret = tas5782m_set_book_page(priv, TAS5782M_VOL_BOOK, TAS5782M_VOL_PAGE);
	if (ret)
		goto out;
	ret = regmap_bulk_write(priv->regmap, TAS5782M_VOL_CH_LEFT,
				values, sizeof(values));
	if (ret)
		goto out;
	ret = tas5782m_check_volume(priv);
	if (ret)
		goto out;

	ret = tas5782m_set_book_page(priv, TAS5782M_VOL_BOOK, TAS5782M_SWAP_PAGE);
	if (ret)
		goto out;
	ret = regmap_bulk_write(priv->regmap, TAS5782M_SWAP_REG, swap, sizeof(swap));
	if (ret)
		goto out;
	ret = tas5782m_wait_swap(priv);
	if (!ret)
		ret = tas5782m_check_volume(priv);
out:
	restore = tas5782m_set_book_page(priv, 0, 0);
	return ret ? ret : restore;
}

/* ========================================================================= */
/* Refresh — apply current vol/mute state to the chip                        */
/* ========================================================================= */

/*
 * Caller must hold priv->lock and priv->is_powered must be true.
 * Volume is pushed first so an unmute does not pop to a stale level.
 */
static int tas5782m_refresh(struct tas5782m_priv *priv)
{
	bool muted = priv->is_muted || !priv->playback_enabled ||
		(priv->vol[0] == TAS5782M_VOLUME_MIN &&
		 priv->vol[1] == TAS5782M_VOLUME_MIN);
	int ret;

	gpiod_set_value_cansleep(priv->gpio_mute, 1);
	ret = tas5782m_set_volume(priv);
	if (ret)
		goto fail;
	ret = tas5782m_write_traced(priv, TAS5782M_CTRL3,
				   muted ? TAS5782M_MUTE : TAS5782M_NORMAL_VOLUME);
	if (ret)
		goto fail;
	if (!muted)
		gpiod_set_value_cansleep(priv->gpio_mute, 0);
	return 0;

fail:
	priv->is_powered = false;
	dev_err(&priv->client->dev,
		"gain programming/readback failed (%d); hardware mute retained\n", ret);
	return ret;
}

/* ========================================================================= */
/* Firmware handling                                                          */
/* ========================================================================= */

static int tas5782m_parse_volume_limits(struct tas5782m_priv *priv,
				       const u8 *data, size_t len)
{
	unsigned int book = 0;
	unsigned int page = 0;
	unsigned int present = 0;
	unsigned int shift;
	unsigned int byte;
	size_t i;
	u8 reg;

	if (len < 2 || (len & 1))
		return -EINVAL;
	priv->volume_max[0] = 0;
	priv->volume_max[1] = 0;
	for (i = 0; i < len; i += 2) {
		reg = data[i];
		if (reg == TAS5782M_REG_PAGE) {
			page = data[i + 1];
		} else if (!page && reg == TAS5782M_REG_BOOK) {
			book = data[i + 1];
		} else if (book == TAS5782M_VOL_BOOK && page == TAS5782M_VOL_PAGE &&
			   reg >= TAS5782M_VOL_CH_LEFT && reg < TAS5782M_VOL_CH_LEFT + 8) {
			byte = reg - TAS5782M_VOL_CH_LEFT;
			shift = 24 - 8 * (byte % 4);
			priv->volume_max[byte / 4] &= ~(0xffU << shift);
			priv->volume_max[byte / 4] |= (u32)data[i + 1] << shift;
			present |= 1U << byte;
		}
	}
	if (present != 0xff || !priv->volume_max[0] || !priv->volume_max[1] ||
	    priv->volume_max[0] > 0x00800000 || priv->volume_max[1] > 0x00800000)
		return -EINVAL;
	return 0;
}

static int tas5782m_cache_firmware(struct tas5782m_priv *priv)
{
	const struct firmware *fw;
	char fw_name[128];
	int ret;

	snprintf(fw_name, sizeof(fw_name),
		 "tas5728m_dsp_%s.bin", priv->dsp_config_name);

	ret = request_firmware(&fw, fw_name, &priv->client->dev);
	if (ret)
		return dev_err_probe(&priv->client->dev, ret,
				     "required board DSP firmware '%s' unavailable\n", fw_name);

	if (fw->size < 2 || (fw->size & 1)) {
		dev_err(&priv->client->dev, "firmware '%s' is invalid\n", fw_name);
		release_firmware(fw);
		return -EINVAL;
	}

	ret = tas5782m_parse_volume_limits(priv, fw->data, fw->size);
	if (ret) {
		dev_err(&priv->client->dev,
			"firmware '%s' lacks valid TAS5782M DSP volume limits\n", fw_name);
		release_firmware(fw);
		return ret;
	}

	priv->dsp_cfg_data = devm_kmemdup(&priv->client->dev, fw->data,
					  fw->size, GFP_KERNEL);
	if (!priv->dsp_cfg_data) {
		release_firmware(fw);
		return -ENOMEM;
	}
	priv->dsp_cfg_len = fw->size;

	dev_info(&priv->client->dev,
		 "cached DSP firmware '%s' (%zu bytes)\n",
		 fw_name, fw->size);

	release_firmware(fw);
	return 0;
}

/* ========================================================================= */
/* Stream-start work — preboot + firmware + initial refresh                  */
/* ========================================================================= */

static int tas5782m_send_cfg(struct tas5782m_priv *priv,
			     const u8 *data, size_t len, const char *stage)
{
	size_t i;
	int ret;

	for (i = 0; i + 1 < len; i += 2) {
		ret = regmap_write(priv->regmap, data[i], data[i + 1]);
		if (ret) {
			dev_err(&priv->client->dev,
				"%s register %#02x=%#02x at pair %zu failed (%d)\n",
				stage, data[i], data[i + 1], i / 2, ret);
			return ret;
		}
	}

	return 0;
}

/*
 * tas5782m_do_work() — runs all chip programming that requires a live BCLK.
 *
 * Scheduled from .trigger(START) on the codec DAI, which fires after the
 * CPU DAI's trigger has called q6afe_port_start() and brought BCLK up on
 * the I2S pins. We still wait an extra 5–10 ms because BCLK takes a few
 * frames to settle and the chip latches a clock-error flag if the first
 * I2C write straddles that boundary.
 */
static void tas5782m_do_work(struct work_struct *work)
{
	struct tas5782m_priv *priv =
		container_of(work, struct tas5782m_priv, work);
	int ret;

	mutex_lock(&priv->lock);
	gpiod_set_value_cansleep(priv->gpio_mute, 1);

	/* If chip is already in PLAY mode (e.g. DAPM pmdown_time kept it live),
	 * skip the full 545ms preboot+firmware sequence and just refresh state. */
	if (priv->is_powered) {
		ret = tas5782m_refresh(priv);
		if (ret)
			goto fail;
		tas5782m_trace(priv, TAS5782M_PHASE_PLAY,
			       "stream-start: chip already powered, refresh only\n");
		mutex_unlock(&priv->lock);
		return;
	}

	tas5782m_trace(priv, TAS5782M_PHASE_INIT,
		       "stream-start: waiting 5ms for BCLK to stabilize\n");
	usleep_range(5000, 10000);

#ifdef TAS5782M_DEBUG
	atomic_inc(&priv->dbg.n_play_attempted);
#endif

	ret = tas5782m_send_cfg(priv, tas5782m_dsp_init, tas5782m_dsp_init_len,
			       "preboot");
	if (ret)
		goto fail;

	/* DSP needs ~5 ms to boot internally before accepting firmware blob */
	usleep_range(5000, 15000);

	ret = tas5782m_send_cfg(priv, priv->dsp_cfg_data, priv->dsp_cfg_len,
			       "DSP firmware");
	if (ret)
		goto fail;

#ifdef TAS5782M_DEBUG
	atomic_inc(&priv->dbg.n_firmware_load);
#endif

	/* The required board firmware ends in book 0/page 0. */
	ret = tas5782m_write_traced(priv, TAS5782M_SIG_CH_CTRL, TAS5782M_CH_DAC_NORMAL);
	if (ret) {
		dev_err(&priv->client->dev,
			"DAC activation register %#02x failed (%d)\n",
			TAS5782M_SIG_CH_CTRL, ret);
		goto fail;
	}

	priv->is_powered = true;
	ret = tas5782m_refresh(priv);
	if (ret)
		goto fail;

#ifdef TAS5782M_DEBUG
	atomic_inc(&priv->dbg.n_play_success);
#endif

	tas5782m_trace(priv, TAS5782M_PHASE_PLAY,
		       "stream-start: chip in PLAY mode, refresh applied\n");

	mutex_unlock(&priv->lock);
	return;

fail:
	priv->is_powered = false;
	dev_err(&priv->client->dev,
		"stream initialization failed (%d); hardware mute retained\n", ret);
	mutex_unlock(&priv->lock);
}

/* ========================================================================= */
/* DAI callbacks                                                              */
/* ========================================================================= */

static int tas5782m_trigger(struct snd_pcm_substream *substream, int cmd,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_component *comp = dai->component;
	struct tas5782m_priv *priv = snd_soc_component_get_drvdata(comp);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		tas5782m_trace(priv, TAS5782M_PHASE_PLAY,
			       "trigger(START) — scheduling init work\n");
		schedule_work(&priv->work);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* HI-Z teardown is done in DAPM PRE_PMD */
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int tas5782m_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_component *comp = dai->component;
	struct tas5782m_priv *priv = snd_soc_component_get_drvdata(comp);
	int ret = 0;

	if (stream != SNDRV_PCM_STREAM_PLAYBACK)
		return 0;

	tas5782m_trace(priv, TAS5782M_PHASE_MUTE,
		       "mute_stream mute=%d\n", mute);

	mutex_lock(&priv->lock);
	priv->is_muted = mute;
	if (priv->is_powered)
		ret = tas5782m_refresh(priv);
	mutex_unlock(&priv->lock);

	return ret;
}

/* ========================================================================= */
/* DAPM event — PRE_PMD only, for HI-Z teardown                              */
/* ========================================================================= */

static int tas5782m_dac_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *k, int event)
{
	struct snd_soc_component *comp = snd_soc_dapm_to_component(w->dapm);
	struct tas5782m_priv *priv = snd_soc_component_get_drvdata(comp);
	int ret = 0;

	if (event & SND_SOC_DAPM_PRE_PMD) {
		cancel_work_sync(&priv->work);

		mutex_lock(&priv->lock);
		gpiod_set_value_cansleep(priv->gpio_mute, 1);
		if (priv->is_powered) {
			priv->is_powered = false;
			ret = tas5782m_set_book_page(priv, 0, 0);
			if (!ret)
				ret = tas5782m_write_traced(priv, TAS5782M_CTRL2,
							   TAS5782M_MODE_HIZ);
			if (ret)
				dev_err(&priv->client->dev,
					"standby write failed (%d); hardware mute retained\n", ret);
			tas5782m_trace(priv, TAS5782M_PHASE_DAPM,
				       "PRE_PMD: chip to standby (HIZ)\n");
		} else {
			tas5782m_trace(priv, TAS5782M_PHASE_DAPM,
				       "PRE_PMD: chip was not powered, no action\n");
		}
		mutex_unlock(&priv->lock);
	}

	return ret;
}

/* ========================================================================= */
/* DAPM topology                                                              */
/* ========================================================================= */

static const struct snd_soc_dapm_widget tas5782m_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DAC IN", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC_E("DAC", NULL, SND_SOC_NOPM, 0, 0,
			   tas5782m_dac_event, SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("OUT"),
};

static const struct snd_soc_dapm_route tas5782m_dapm_routes[] = {
	{ "DAC", NULL, "DAC IN" },
	{ "OUT", NULL, "DAC"    },
};

/* ========================================================================= */
/* Volume kcontrol                                                            */
/* ========================================================================= */

static int tas5782m_vol_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = TAS5782M_VOLUME_MIN;
	uinfo->value.integer.max = TAS5782M_VOLUME_MAX;
	return 0;
}

static int tas5782m_vol_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct tas5782m_priv *priv = snd_soc_component_get_drvdata(comp);
	int ret = 0;

	mutex_lock(&priv->lock);
	if (priv->is_powered) {
		int restore;

		ret = tas5782m_check_volume(priv);
		restore = tas5782m_set_book_page(priv, 0, 0);
		if (!ret)
			ret = restore;
		if (ret) {
			gpiod_set_value_cansleep(priv->gpio_mute, 1);
			priv->is_powered = false;
			dev_err(&priv->client->dev,
				"gain readback failed (%d); hardware mute retained\n", ret);
			goto out;
		}
	}
	ucontrol->value.integer.value[0] = priv->vol[0];
	ucontrol->value.integer.value[1] = priv->vol[1];
out:
	mutex_unlock(&priv->lock);
	return ret;
}

static int tas5782m_vol_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct tas5782m_priv *priv = snd_soc_component_get_drvdata(comp);
	int v0 = ucontrol->value.integer.value[0];
	int v1 = ucontrol->value.integer.value[1];
	int changed = 0;
	int ret;
	int old[2];

	if (v0 < TAS5782M_VOLUME_MIN || v0 > TAS5782M_VOLUME_MAX ||
	    v1 < TAS5782M_VOLUME_MIN || v1 > TAS5782M_VOLUME_MAX)
		return -EINVAL;

	mutex_lock(&priv->lock);
	if (priv->vol[0] != v0 || priv->vol[1] != v1) {
		old[0] = priv->vol[0];
		old[1] = priv->vol[1];
		priv->vol[0] = v0;
		priv->vol[1] = v1;
		changed = 1;
		if (priv->is_powered) {
			ret = tas5782m_refresh(priv);
			if (ret) {
				priv->vol[0] = old[0];
				priv->vol[1] = old[1];
				changed = ret;
			}
		}

		tas5782m_trace(priv, TAS5782M_PHASE_VOL,
			       "vol_put L=%d R=%d\n", v0, v1);
	}
	mutex_unlock(&priv->lock);
	return changed;
}

static int tas5782m_switch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct tas5782m_priv *priv = snd_soc_component_get_drvdata(comp);

	mutex_lock(&priv->lock);
	ucontrol->value.integer.value[0] = priv->playback_enabled;
	mutex_unlock(&priv->lock);
	return 0;
}

static int tas5782m_switch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct tas5782m_priv *priv = snd_soc_component_get_drvdata(comp);
	bool enabled = !!ucontrol->value.integer.value[0];
	int ret = 0;

	mutex_lock(&priv->lock);
	if (enabled != priv->playback_enabled) {
		priv->playback_enabled = enabled;
		ret = 1;
		if (priv->is_powered) {
			ret = tas5782m_refresh(priv);
			if (ret)
				priv->playback_enabled = !enabled;
			else
				ret = 1;
		}
	}
	mutex_unlock(&priv->lock);
	return ret;
}

static const DECLARE_TLV_DB_SCALE(tas5782m_volume_tlv, -8500, 100, 1);

static const struct snd_kcontrol_new tas5782m_controls[] = {
	{
		.iface  = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name   = "Master Playback Volume",
		.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			  SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info   = tas5782m_vol_info,
		.get    = tas5782m_vol_get,
		.put    = tas5782m_vol_put,
		.tlv.p  = tas5782m_volume_tlv,
	},
	{
		.iface  = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name   = "Master Playback Switch",
		.info   = snd_ctl_boolean_mono_info,
		.get    = tas5782m_switch_get,
		.put    = tas5782m_switch_put,
	},
};

static const struct snd_soc_dai_ops tas5782m_dai_ops = {
	.trigger         = tas5782m_trigger,
	.mute_stream     = tas5782m_mute_stream,
	.no_capture_mute = 1,
};

struct snd_soc_dai_driver tas5782m_dai = {
	/* Name kept as "tas5805m-amplifier" for ABI compatibility with the
	 * existing machine-driver/DT setup that was wired for the legacy
	 * tas5805m driver fork. The chip is a TAS5782M; only the DAI string
	 * is preserved. */
	.name           = "tas5805m-amplifier",
	.playback = {
		.stream_name  = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates        = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
		.formats      = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &tas5782m_dai_ops,
};

const struct snd_soc_component_driver tas5782m_component = {
	.controls         = tas5782m_controls,
	.num_controls     = ARRAY_SIZE(tas5782m_controls),
	.dapm_widgets     = tas5782m_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tas5782m_dapm_widgets),
	.dapm_routes      = tas5782m_dapm_routes,
	.num_dapm_routes  = ARRAY_SIZE(tas5782m_dapm_routes),
	.use_pmdown_time  = 1,
	.endianness       = 1,
};

/* ========================================================================= */
/* I2C probe / remove                                                         */
/* ========================================================================= */

static int tas5782m_i2c_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct tas5782m_priv *priv;
	const char *config_name;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->client = i2c;
	mutex_init(&priv->lock);
	INIT_WORK(&priv->work, tas5782m_do_work);
	priv->vol[0] = TAS5782M_VOLUME_MIN;
	priv->vol[1] = TAS5782M_VOLUME_MIN;
	priv->is_muted = false;
	priv->is_powered = false;

#ifdef TAS5782M_DEBUG
	priv->dbg.trace_mask = TAS5782M_PHASE_PROBE | TAS5782M_PHASE_INIT |
			       TAS5782M_PHASE_DAPM  | TAS5782M_PHASE_PLAY |
			       TAS5782M_PHASE_MUTE;
	atomic_set(&priv->dbg.n_probe, 1);
#endif

	tas5782m_trace(priv, TAS5782M_PHASE_PROBE,
		       "probe start (i2c 0x%02x)\n", i2c->addr);

	ret = of_property_read_string(dev->of_node,
				      "ti,dsp-config-name", &config_name);
	if (ret) {
		dev_info(dev, "no ti,dsp-config-name; using 'default'\n");
		config_name = "default";
	}
	strscpy(priv->dsp_config_name, config_name,
		sizeof(priv->dsp_config_name));

	priv->pvdd = devm_regulator_get(dev, "pvdd");
	if (IS_ERR(priv->pvdd))
		return dev_err_probe(dev, PTR_ERR(priv->pvdd),
				     "failed to get pvdd supply\n");

	priv->gpio_pdn = devm_gpiod_get_optional(dev, "pdn", GPIOD_OUT_LOW);
	if (IS_ERR(priv->gpio_pdn))
		return dev_err_probe(dev, PTR_ERR(priv->gpio_pdn),
				     "failed to get pdn gpio\n");

	priv->gpio_mute = devm_gpiod_get(dev, "mute", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->gpio_mute))
		return dev_err_probe(dev, PTR_ERR(priv->gpio_mute),
				     "required board hardware mute unavailable\n");

	priv->regmap = devm_regmap_init_i2c(i2c, &tas5782m_regmap_config);
	if (IS_ERR(priv->regmap))
		return dev_err_probe(dev, PTR_ERR(priv->regmap),
				     "failed to create regmap\n");

	i2c_set_clientdata(i2c, priv);

	ret = tas5782m_cache_firmware(priv);
	if (ret)
		return ret;

	ret = regulator_enable(priv->pvdd);
	if (ret)
		return dev_err_probe(dev, ret, "failed to enable pvdd\n");

	/*
	 * FelixKa note: hold the chip in PDN for ~100 ms after PVDD comes up
	 * so the ADR pin samples cleanly. Otherwise the I2C address can come
	 * up unpredictable.
	 */
	usleep_range(100000, 150000);

	if (priv->gpio_pdn)
		gpiod_set_value_cansleep(priv->gpio_pdn, 1);

	/* Allow internal regulator/PLL domains to settle after PDN release. */
	usleep_range(10000, 15000);

	tas5782m_trace(priv, TAS5782M_PHASE_INIT,
		       "probe: firmware cached, deferring chip init until trigger(START)\n");

	tas5782m_dbg_init(priv);

	ret = snd_soc_register_component(dev, &tas5782m_component,
					 &tas5782m_dai, 1);
	if (ret) {
		dev_err(dev, "failed to register ASoC component: %d\n", ret);
		tas5782m_dbg_remove(priv);
		goto err_disable_pvdd;
	}

	dev_info(dev, "TAS5782M ready (i2c=0x%02x config='%s', DSP volume limits %08x/%08x)\n",
		 i2c->addr, priv->dsp_config_name,
		 priv->volume_max[0], priv->volume_max[1]);
	return 0;

err_disable_pvdd:
	if (priv->gpio_pdn)
		gpiod_set_value_cansleep(priv->gpio_pdn, 0);
	regulator_disable(priv->pvdd);
	return ret;
}

static void tas5782m_i2c_remove(struct i2c_client *i2c)
{
	struct tas5782m_priv *priv = i2c_get_clientdata(i2c);

	cancel_work_sync(&priv->work);
	snd_soc_unregister_component(&i2c->dev);
	gpiod_set_value_cansleep(priv->gpio_mute, 1);

	if (priv->gpio_pdn)
		gpiod_set_value_cansleep(priv->gpio_pdn, 0);

	tas5782m_dbg_remove(priv);

	usleep_range(10000, 15000);
	regulator_disable(priv->pvdd);
	dev_info(&i2c->dev, "TAS5782M removed\n");
}

/* ========================================================================= */
/* Module scaffolding                                                         */
/* ========================================================================= */

static const struct of_device_id tas5782m_of_ids[] = {
	{ .compatible = "ti,tas5782m" },
	{ }
};
MODULE_DEVICE_TABLE(of, tas5782m_of_ids);

static const struct i2c_device_id tas5782m_id[] = {
	{ "tas5782m", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas5782m_id);

static struct i2c_driver tas5782m_driver = {
	.driver = {
		.name           = "tas5782m",
		.of_match_table = tas5782m_of_ids,
	},
	.probe    = tas5782m_i2c_probe,
	.remove   = tas5782m_i2c_remove,
	.id_table = tas5782m_id,
};

/*
 * Only register the module entry points in the production build.
 * The debug build (tas5782m_dbgbuild.c, compiled with -DTAS5782M_DEBUG)
 * #includes this file and must define its own module_init/module_exit so
 * that the KCFI type hash for init_module matches what the running kernel
 * expects.  If module_i2c_driver() were expanded here under
 * -DTAS5782M_DEBUG, the altered compilation environment changes the KCFI
 * annotation and causes a CFI failure at do_one_initcall.
 */
#ifndef TAS5782M_DEBUG
module_i2c_driver(tas5782m_driver);

MODULE_AUTHOR("Based on FelixKa / Andy Liu / Daniel Beer");
MODULE_DESCRIPTION("TAS5782M Smart Amplifier ASoC CODEC Driver");
MODULE_LICENSE("GPL v2");
#endif /* !TAS5782M_DEBUG */
