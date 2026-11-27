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
| Camera | Working (S5KC505A) |
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
| Platform / mainline sources | LineageOS `lineage-24.0`; exact revisions in the [locked manifest](manifests/locked.xml) |
| Community kernel configuration | `upstream/xiaomi-mainline` Git submodule |
| Maintained codec sources | `drivers/tas5782m`; copied with attribution and local fixes |
| Changes to upstream projects | `patches/<Android project path>/0001-description.patch` |
| Private firmware | 29 files; recorded checksums for [200628.084](config/firmware-CD_18781Y.200628.084.sha256) and [210228.109](config/firmware-CD_18781Y.210228.109.sha256); not distributed |
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

## Crash logs

Ramoops uses 1 MiB of reserved RAM to retain crash records and the kernel
console across warm reboots, **not cold resets or power loss**. Kernel panics
request a warm reboot after 10 seconds; the hardware watchdog can also reset a
hung system.

The GPU watchdog handles a narrower failure: after a one-minute startup grace
period, it requests a warm reboot if MSM reports the same stalled ring and
fences for at least 25 seconds. It saves the ring and fence numbers in
`/data/vendor/thinksmart_gpu_watchdog/last_reboot` before rebooting. That file
also prevents another automatic GPU reboot until the monitor sees 15 minutes
without a hangcheck report. The regular hardware watchdog is unchanged.

Collect logs after an unexpected reboot, before rebooting again:

```sh
adb root
adb wait-for-device
adb pull /sys/fs/pstore crash-pstore
adb shell dumpsys dropbox --print SYSTEM_LAST_KMSG > last-kmsg.txt
adb unroot
```

Android also archives the console tail in `SYSTEM_LAST_KMSG`. These are
diagnostic logs, not a full memory dump; a hard lockup may leave only the last
console messages. Keep collected logs private.

## Source credits and licensing

External Android/kernel projects are synced with Android `repo`, not copied
here. Patch mailboxes carry authorship, source references, base revisions and
local adaptation notes; preserve upstream authors and signoffs when rebasing.

| Material | Source / licensing |
|---|---|
| TAS5782M codec | Origins, contributors and local changes in [drivers/tas5782m/README.md](drivers/tas5782m/README.md); GPL-2.0-only |
| Xiaomi kernel configuration | [android_device_xiaomi_mi89xx-mainline](https://github.com/LineageOS/android_device_xiaomi_mi89xx-mainline); pinned by the Git submodule; upstream notices apply |
| ath10k firmware encoder | Origin and ISC license in [tools/README.md](tools/README.md) and the script's original notice |
| libcamera and Android camera adapter | [Upstream libcamera](https://github.com/libcamera-org/libcamera), [TI Android integration](https://git.ti.com/cgit/android/external-libcamera/), and [Alessandro Fontana's Android fixes](https://github.com/alessandro-fontana/android_external_libcamera); original source notices retained |
| Android AIDL provider and GPU build integration | [Raspberry Vanilla](https://github.com/raspberry-vanilla/android_external_libcamera) and [haretic](https://github.com/haretic/android_external_libcamera); original source notices retained |
| S5KC505A helper, LSC/statistics and calibration conversion | Mei Tao's work distributed by [rickx/ubuntu-thinksmart](https://github.com/rickx/ubuntu-thinksmart/tree/6f5b14928a873d171e1deddb5edbdc6967c86aa2/sources/camera) and [jethac's Lenovo sensor-library decode](https://github.com/jethac/lenovo-thinksmart-view/blob/fe9c0a58a1a2b7c4893f2ff76ff5c0b90206d458/streams/camera/s5kc505a-sensor-lib-decode.txt); detailed provenance and limitations in the sensor patch; no proprietary-library redistribution rights asserted |

Unless otherwise noted, independently authored integration code, support scripts
and documentation are licensed under the [Apache License, Version 2.0](LICENSE).
Third-party code and derived modifications, including patch contents, retain
their applicable upstream licenses and notices. GPL text is in
[LICENSES/GPL-2.0.txt](LICENSES/GPL-2.0.txt). This license does not grant rights
to redistribute proprietary firmware.
