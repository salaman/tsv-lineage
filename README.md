# ThinkSmart View: mainline LineageOS

Unofficial device tree for **Lenovo ThinkSmart View CD-18781Y**.
Standard Android `repo`, `lunch` and `m`; no separate build system.

## Status

| Area | State in the existing port |
|---|---|
| Software | LineageOS 23.2 / Android 16; msm8953-mainline Linux 7.1.3 |
| Working | Display/touch, Wi-Fi, graphics, speaker/microphone, buttons, RTC, thermal, ALS/proximity |
| Partial | Initial AVC/HEVC decoding; Bluetooth profiles unqualified |
| Unsupported | Camera, accelerometer, full suspend, OTA installation |
| Security | Experimental userdebug; SELinux permissive; verified boot disabled |

## Contents and requirements

| Item | Location / requirement |
|---|---|
| Device build definitions | Root `*.mk` and `Android.bp` |
| Runtime configuration and firmware lists | `config/` |
| Platform / mainline sources | LineageOS `lineage-23.2` plus [17-project local manifest](manifests/thinksmart.xml) |
| Community kernel configuration | `upstream/xiaomi-mainline` Git submodule |
| Maintained codec sources | `drivers/tas5782m`; copied with attribution and local fixes |
| Changes to upstream projects | `patches/<Android project path>/0001-description.patch` |
| Private firmware | 29 files checked against [firmware.sha256](config/firmware.sha256); not distributed |
| Build host | x86-64 Linux; dependencies in [Dockerfile](Dockerfile); budget **350 GiB free** |
| Installation | Stock GPT, installed compatible lk2nd, original physical boot backup, working [EDL](https://github.com/bkerler/edl) setup |

See [PROVENANCE.md](PROVENANCE.md) for copied-source origins and licenses.

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
repo init -u https://github.com/LineageOS/android.git -b lineage-23.2 --git-lfs
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
lunch lineage_thinksmart bp4a userdebug
m bootimage recoveryimage systemimage vendorimage
```

Images are written to `out/target/product/thinksmart`.

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
