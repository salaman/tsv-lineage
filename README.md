# ThinkSmart View: mainline LineageOS

Unofficial device tree for **Lenovo ThinkSmart View CD-18781Y**.
Standard Android `repo`, `lunch` and `m`; no separate build system.

## Status

LineageOS 24.0 / Android 17 with msm8953-mainline Linux 7.3 develop.
Boots to the Android UI with root ADB.

| Hardware / feature | Status |
|---|---|
| Display and backlight | Working |
| Touchscreen and single-tap screen wake | Working |
| GPU acceleration | Working |
| Wi-Fi | Working |
| Bluetooth LE scanning | Working |
| Audio streaming and microphone capture | Working |
| Ambient-light and proximity sensor readings | Working |
| Thermal sensors | Working |
| RTC | Working |
| Internal storage | Working |
| Hardware AVC/H.264 and HEVC/H.265 decoding | Working |
| Camera | Not working |
| Accelerometer | Not working |
| Full system suspend | Not working |

OTA installation is not supported. This is an experimental userdebug build
with enforcing SELinux and verified boot disabled. The inherited userdebug
`su` and `backuptool` domains remain permissive.

## Contents and requirements

| Item | Location / requirement |
|---|---|
| Device build definitions | Root `*.mk` and `Android.bp` |
| Runtime configuration and firmware lists | `config/` |
| Platform / mainline sources | LineageOS `lineage-24.0` plus [20-project local manifest](manifests/thinksmart.xml) |
| Community kernel configuration | `upstream/xiaomi-mainline` Git submodule |
| Maintained codec sources | `drivers/tas5782m`; copied with attribution and local fixes |
| Changes to upstream projects | `patches/<Android project path>/0001-description.patch` |
| Private firmware | 29 files checked against [firmware.sha256](config/firmware.sha256); not distributed |
| Build host | x86-64 Linux; dependencies in [Dockerfile](Dockerfile); budget **350 GiB free** |
| Installation | Stock GPT, installed compatible lk2nd, original physical boot backup, working [EDL](https://github.com/bkerler/edl) setup |

## Build

Use Linux Bash and a real device directory, not a symlink.

### Build environment

```sh
docker compose build
docker compose run --rm builder
```

Docker starts in the persistent Linux source volume `/android`; `/integration`
is this checkout. Builds use Docker's available CPU, memory and disk.
Set `HOST_UID` / `HOST_GID` when needed. On Apple Silicon enable amd64/Rosetta
emulation. Native Linux hosts can instead install the [Dockerfile](Dockerfile)
dependencies.

### Sources

```sh
mkdir android-tsv && cd android-tsv
repo init -u https://github.com/LineageOS/android.git -b lineage-24.0 --git-lfs
DEVICE=device/lenovo/thinksmart
mkdir -p device/lenovo .repo/local_manifests
git clone --recurse-submodules https://github.com/salaman/tsv-lineage.git "$DEVICE"
cp "$DEVICE/manifests/thinksmart.xml" .repo/local_manifests/
repo sync -c --no-tags --no-clone-bundle --fail-fast -j4

bash "$DEVICE/tools/apply-patches.sh" "$PWD"
```

To use this checkout instead, replace the `git clone` line with:

```sh
cp -a /integration "$DEVICE"
git -C "$DEVICE" submodule update --init --recursive
```

Outside Docker, replace `/integration` with this checkout's absolute path.
Scripts and patches require LF line endings; `.gitattributes` enforces this for
new checkouts.

Patches apply in numbered filename order within each project, starting at `0001`;
there are no `series` files. Apply queues **once to clean projects**. The helper
leaves uncommitted changes and stops on errors without resetting sources.
Branch-tracking dependencies can change.

## Firmware

### Files and origins

| Input | Origin |
|---|---|
| ADSP, Venus, Bluetooth | Stock `modem` partition |
| GPU zap | Stock `vendor` partition; not `modem/image/gfx.mdt` |
| Wi-Fi | Stock modem board/firmware/OTP; helper creates the ath10k container |
| Speaker DSP tuning | [Community board firmware](https://gitlab.com/kaechele/postmarketos-vendor-lenovo-cd-18781y), revision `337032e864fd6abe29375997fc3683398a4d27c4`, `sound/tas5728m_dsp_lenovo_cd-18781y.bin` |

The speaker DSP register data is derived from Lenovo's released kernel sources,
not extracted from a stock firmware partition. Wi-Fi packaging uses the
credited ath10k encoder with fixed metadata to reproduce the qualified container.
The extraction helper verifies all 29 files and refuses existing output.
Keep firmware private; redistribution permission is not assumed.
Examples use `/private` for inputs; in Docker use `/integration/private`.

### Extract via EDL

Read the stock partitions with your configured EDL tool, then extract using the
speaker DSP file listed above:

```sh
edl r modem /private/modem.img
edl r vendor /private/vendor.img

bash "$DEVICE/tools/extract-firmware.sh" --stock \
  /private/modem.img /private/vendor.img /private/tas5728m_dsp_lenovo_cd-18781y.bin \
  "$DEVICE/firmware"
```

### Extract via ADB

Requires root ADB and a read-only `/vendor` mount. For a device running this port:

```sh
adb exec-out cat /dev/block/by-name/vendor > /private/vendor.img
adb exec-out cat /dev/block/by-name/boot > /private/original-physical-boot.img

bash "$DEVICE/tools/extract-firmware.sh" --android-vendor \
  /private/vendor.img "$DEVICE/firmware"
```

For stock Android, also read `modem` and use `--stock` as above:

```sh
adb exec-out cat /dev/block/by-name/modem > /private/modem.img
```

## Build images

From the source root, after firmware extraction:

```sh
source build/envsetup.sh
lunch lineage_thinksmart cp2a userdebug
m bootimage recoveryimage systemimage vendorimage
```

Images are written to `out/target/product/thinksmart`.

Boots are quiet by default; ADB `logcat` and `dmesg` remain available.
For on-screen boot diagnostics, export `THINKSMART_BOOT_CONSOLE=true` before
`lunch` and rebuild the images.

## Installation

**EDL partition writes only.** lk2nd fastboot writes have reported success while
incomplete. Fastboot is for identification or RAM recovery, not installation.

Compose physical boot offline, preserving this device's exact 512 KiB lk2nd prefix:

```sh
OUT=out/target/product/thinksmart
python3 "$DEVICE/tools/prepare-boot.py" \
  --existing-boot /private/original-physical-boot.img \
  --payload "$OUT/boot.img" \
  --dtb "$OUT/obj/KERNEL_OBJ/arch/arm64/boot/dts/qcom/apq8053-lenovo-cd-18781y.dtb" \
  --output /private/boot-edl.img
```

| Operation | Matching images to write with EDL |
|---|---|
| Update | `vendor.img` to `vendor`, `system.img` to `system`, composed boot to `boot`; preserve userdata/cache |
| Initial install | Same, plus prepared userdata and `cache.img`; **erases user data** |

Initial install additionally needs `m userdataimage cacheimage` and these
Android host tools (`out/host/linux-x86/bin` on `PATH`):

```sh
simg2img "$OUT/userdata.img" /private/userdata.raw.img
tune2fs -O encrypt,verity /private/userdata.raw.img
e2fsck -f -n /private/userdata.raw.img
img2simg /private/userdata.raw.img /private/userdata-fbe.img
```

- Confirm device identity, image hashes and stock partition sizes before writing.
- **Never write Android `boot.img` directly to physical `boot`.**
- Do not alter GPT, aboot, OEM (`/metadata`), persist, modem or other firmware partitions.
- Keep original backups. Stock-device lk2nd installation and OTA ZIPs are outside this port.

## Source credits and licensing

External Android/kernel projects are synced with Android `repo`, not copied
here. Patch mailboxes carry authorship, source references, base revisions and
local adaptation notes; preserve upstream authors and signoffs when rebasing.

| Material | Source / licensing |
|---|---|
| TAS5782M codec | Origins, contributors and local changes in [drivers/tas5782m/README.md](drivers/tas5782m/README.md); GPL-2.0-only |
| Xiaomi kernel configuration | [android_device_xiaomi_mi89xx-mainline](https://github.com/LineageOS/android_device_xiaomi_mi89xx-mainline); pinned by the Git submodule; upstream notices apply |
| ath10k firmware encoder | Origin and ISC license in [tools/README.md](tools/README.md) and the script's original notice |

Source-tree licenses and notices apply to modifications; this port does not
relicense third-party material. GPL text is in [LICENSES/GPL-2.0.txt](LICENSES/GPL-2.0.txt).
Choose a license for newly authored integration files before public distribution.
