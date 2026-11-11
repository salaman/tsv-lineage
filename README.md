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
| Accelerometer | Working |
| Thermal sensors | Working |
| RTC | Working |
| Internal storage | Working |
| Hardware AVC/H.264 and HEVC/H.265 decoding | Working |
| Camera | Not working |
| Full system suspend | Not working |

OTA installation is not supported. This is an experimental userdebug build
with enforcing SELinux and verified boot disabled. The inherited userdebug
`su` and `backuptool` domains remain permissive.

## Contents and requirements

| Item | Location / requirement |
|---|---|
| Device build definitions | Root `*.mk` and `Android.bp` |
| Runtime configuration and firmware lists | `config/` |
| Batteryless health HAL and device tests | `health/` |
| Platform / mainline sources | LineageOS `lineage-24.0` plus [20-project local manifest](manifests/thinksmart.xml) |
| Community kernel configuration | `upstream/xiaomi-mainline` Git submodule |
| Maintained codec sources | `drivers/tas5782m`; copied with attribution and local fixes |
| Changes to upstream projects | `patches/<Android project path>/0001-description.patch` |
| Private firmware | 29 files checked against [firmware.sha256](config/firmware.sha256); not distributed |
| Build host | x86-64 Linux; dependencies in [Dockerfile](Dockerfile); budget **350 GiB free** |
| Installation | Stock GPT and working [EDL](https://github.com/bkerler/edl) setup; see [FLASHING.md](FLASHING.md) |

## Build

Follow [BUILDING.md](BUILDING.md) for source setup, firmware extraction,
Android and lk2nd builds, wipe-image generation, and flashable-image preparation.

## Installation

**EDL partition writes only.** lk2nd fastboot writes have reported success while
incomplete. Fastboot is for identification or RAM recovery, not installation.

See [FLASHING.md](FLASHING.md) for initial installation, data-preserving updates,
and recovery factory-reset behavior. A generic physical `boot.img`
contains signed lk2nd in its first 512 KiB and the Android payload after it;
the Android build's bare `boot.img` is **not** directly EDL-flashable.

Updates write matching boot/system/vendor images and preserve userdata, cache
and OEM (`/metadata`). Clean installation additionally writes fresh prepared
userdata/cache/metadata images, **erasing user data and Android metadata**.
Both preserve GPT, aboot, persist and radio firmware. OTA ZIP installation is
not supported.

## Source credits and licensing

External Android/kernel projects are synced with Android `repo`, not copied
here. Patch mailboxes carry authorship, source references, base revisions and
local adaptation notes; preserve upstream authors and signoffs when rebasing.

| Material | Source / licensing |
|---|---|
| TAS5782M codec | Origins, contributors and local changes in [drivers/tas5782m/README.md](drivers/tas5782m/README.md); GPL-2.0-only |
| Xiaomi kernel configuration | [android_device_xiaomi_mi89xx-mainline](https://github.com/LineageOS/android_device_xiaomi_mi89xx-mainline); pinned by the Git submodule; upstream notices apply |
| ath10k firmware encoder | Origin and ISC license in [tools/README.md](tools/README.md) and the script's original notice |

Unless otherwise noted, independently authored integration code, support scripts
and documentation are licensed under the [Apache License, Version 2.0](LICENSE).
Third-party code and derived modifications, including patch contents, retain
their applicable upstream licenses and notices. GPL text is in
[LICENSES/GPL-2.0.txt](LICENSES/GPL-2.0.txt). This license does not grant rights
to redistribute proprietary firmware.
