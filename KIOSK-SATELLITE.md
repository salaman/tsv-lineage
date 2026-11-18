# Kiosk Satellite on the ThinkSmart View

[Kiosk Satellite](https://github.com/jxlarrea/kiosk-satellite) is a Home
Assistant wall-panel app that runs on this build. For how to set it up, see its
[own documentation](https://kiosksatellite.com/docs/). This page only covers
what is specific to this device.

Tested with Kiosk Satellite 2026.11.23 on a LineageOS 24.0 build of this
repository.

## Which APK to install

This build is `arm64-v8a` only (`adb shell getprop ro.product.cpu.abilist`).
Each Kiosk Satellite release has a CPU-specific APK. Use that one:

| APK | Size | Use it? |
|---|---|---|
| `kiosk-satellite-<version>.arm64-v8a.apk` | about 115 MB | **Yes** |
| `kiosk-satellite-<version>.apk` (universal) | about 255 MB | No: the same app with code for CPU types this device does not have |

```sh
adb install -r kiosk-satellite-<version>.arm64-v8a.apk
```

## WebView

No extra WebView is needed. The build ships Chromium 157
(`com.android.webview`).

## Use the legacy renderer

With the default renderer (Impeller) the app's own screens are janky on the
Adreno 506. In the app, turn on **Settings > Device > Legacy renderer** and
restart the app.

Measured on a CD-18781Y: open and close the app menu 8 times and read
`dumpsys gfxinfo`.

| Setting | Janky frames | Median / 90th percentile frame time |
|---|---|---|
| Default (Impeller) | 30.9% | 22 / 40 ms |
| **Legacy renderer** | **4.2%** | **9 / 13 ms** |
| Legacy WebView renderer only | 37% | 10 / 32 ms |
| Legacy renderer + legacy WebView renderer | 2.2% | 5 / 7 ms |

The legacy WebView renderer on its own is worse, so I would not enable it. I
only measured the app's own screens, not scrolling inside a dashboard.

## Permissions for a hands-off panel

Everything below is done once over adb and needs no root and no Shizuku. I
checked each step on this build (Android 17, `userdebug`).

```sh
P=me.jxl.kiosk_satellite
adb shell pm grant $P android.permission.POST_NOTIFICATIONS
adb shell pm grant $P android.permission.ACCESS_LOCAL_NETWORK   # Android 16+: needed to reach Home Assistant on the LAN
adb shell appops set $P SYSTEM_ALERT_WINDOW allow               # relaunch after boot and after an update
adb shell appops set $P WRITE_SETTINGS allow                    # screen brightness
adb shell appops set $P REQUEST_INSTALL_PACKAGES allow          # install its own updates
adb shell cmd deviceidle whitelist +$P                          # not stopped in the background
```

Nothing here gives it the camera, microphone, location or storage. Grant those
only for the features you turn on.

### Device owner: restart and silent updates

Without root or Shizuku, Android only lets an app restart the device (and
install updates without a confirmation) when it is the **device owner**.

```sh
adb shell dpm set-device-owner me.jxl.kiosk_satellite/.KioskAdminReceiver
adb shell dpm list-owners
```

This works on this build while there is one user and no accounts, which is the
case after a clean install. Then:

- The **Restart device** button (drawer, remote admin and the Home Assistant
  entity) works. I tested it: the device restarts and the app comes back to the
  front.
- Updates install without a confirmation tap. I could not test this yet,
  because no newer release was available.
- The app only starts an update when something asks it to. To make it hands-off,
  call `update.install` on its Home Assistant update entity from an automation,
  for example at night.

**Device owner can only be removed with a factory reset, and the app cannot be
uninstalled while it is the owner.** A clean install with the images from this
repository wipes userdata, so you have to run the command again after every
clean install. Only do this on a panel that stays a panel.

### No lock screen

After a restart the Android lock screen covers the app. If you use no PIN or
pattern:

```sh
adb shell locksettings set-disabled true
```
