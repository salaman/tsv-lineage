# Flashing the ThinkSmart View

For **Lenovo ThinkSmart View CD-18781Y with its stock partition layout** only.
These are experimental mainline Android images, not an official LineageOS release.
Use EDL for persistent writes: lk2nd fastboot has reported success for incomplete
writes. **Do not unlock the stock bootloader**; this model's stock LK can crash.

## Before writing

Use a configured [bkerler/edl](https://github.com/bkerler/edl) installation and a
compatible Firehose programmer, with the device already in Qualcomm EDL.
Run commands from the image directory. Verify the stock partition layout below
and `SHA256SUMS` before writing; stop on any mismatch. If needed, add
`--loader=/private/programmer.bin` to the commands.

Keep original backups outside the image set, including GPT, OEM and persist.
Back up each partition before its first replacement:

```sh
edl r PARTITION /private/BACKUP.img
```

OEM is repurposed as Android `/metadata`. Clean installation replaces its
contents; updates preserve it and require its existing ext4 filesystem.
Do not restore another unit's OEM snapshot.

## Images and installation

| Image-set file | Physical partition | Role |
|---|---|---|
| `boot.img` | `boot`, 33,554,432 bytes | Complete generic boot: signed lk2nd, padding to 512 KiB, Android payload |
| `system.img` | `system`, 1,610,612,736 bytes | Raw EROFS system, including product/system_ext |
| `vendor.img` | `vendor`, 402,653,184 bytes | Raw ext4 vendor |
| `userdata-wipe.img` | `userdata`, 4,290,223,616 bytes | Fresh Android sparse ext4; **erases data** |
| `cache-wipe.img` | `cache`, 132,120,576 bytes | Fresh Android sparse ext4; **erases cache** |
| `metadata-wipe.img` | `oem`, 104,857,600 bytes | Fresh Android sparse ext4 for `/metadata`; **erases Android metadata** |
| `recovery.img` | `recovery`, 33,554,432 bytes | Separately AVB1-signed recovery; optional, check its boot qualification |

For **initial installation or a clean reinstall**, after taking backups:

```sh
edl w system system.img
edl w vendor vendor.img
edl w userdata userdata-wipe.img
edl w cache cache-wipe.img
edl w oem metadata-wipe.img
edl w boot boot.img
```

The EDL writer must **expand sparse userdata/cache/metadata images and zero
DONT_CARE ranges**; current bkerler/edl does this. Use the supplied formatted
wipe images, not blank/zero-filled partitions.

Recovery is optional and not needed for normal boot. Only replace it if you
accept the supplied image's boot qualification:

```sh
edl w recovery recovery.img
```

## Updating without wiping data

The generic `boot.img` also replaces lk2nd. Write only:

```sh
edl w system system.img
edl w vendor vendor.img
edl w boot boot.img
```

Do not write any userdata/cache/metadata wipe images during an update.

## Readback and preserved partitions

```sh
edl r PARTITION /private/READBACK.img
```

Before rebooting, compare raw-image readback hashes against `SHA256SUMS`.
For userdata/cache/metadata, compare the exact written ranges against the expanded
hashes and lengths supplied with the image set, not the sparse-file hashes.

Leave GPT, aboot, persist, modem and other stock firmware partitions alone.
Replace OEM (`/metadata`) only for a clean install alongside userdata/cache,
never as a standalone wipe while preserving userdata.
**Recovery factory reset also formats `/metadata` (OEM)** as well as data/cache.
Android recreates its metadata state on first boot.
