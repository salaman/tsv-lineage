# Source provenance

External Android/kernel trees are synced with Android `repo`, not copied here.
The Xiaomi configuration is a Git submodule pinned by its gitlink.

| Material | Origin / revision | License / local changes |
|---|---|---|
| `drivers/tas5782m/*` | [rickx/ubuntu-thinksmart](https://github.com/rickx/ubuntu-thinksmart/tree/7b1a5f3e6b3cd85d10a2f81a56c0e778548375bd/sources/driver), `7b1a5f3e6b3cd85d10a2f81a56c0e778548375bd`; additional work by Kacper Adler in [pmaports MR !8928](https://gitlab.postmarketos.org/postmarketOS/pmaports/-/merge_requests/8928) (closed, not merged) | GPL-2.0-only; preserves TI/Andy Liu, Daniel Beer and Felix Kaechele attribution. Locally adapted for required board tuning, BCLK-dependent startup, correct GPIO behavior, bounded digital attenuation and checked DSP coefficient swaps. |
| `upstream/xiaomi-mainline/mi8953_a/kconfigs/config-postmarketos-qcom-msm8953.aarch64` | [LineageOS/android_device_xiaomi_mi89xx-mainline](https://github.com/LineageOS/android_device_xiaomi_mi89xx-mainline), `750b2890f6d7d8cde62966eda903301d9aeaa0f5` | Used unchanged from the submodule; upstream notices apply. |
| Kernel patches `0007-usb-gadget-configfs-notify-userspace.patch`, `0008-mm-memfd-introduce-ashmem-shim.patch`, `0010-cgroups-relax-task-migration-permissions.patch` | [kernel/common-patches](https://android.googlesource.com/kernel/common-patches/+/95c6537fa2f92733e8b9fe23ab4ace2446070f41/), `95c6537fa2f92733e8b9fe23ab4ace2446070f41` | Linux kernel licensing; original authors/signoffs preserved. USB configfs notifications, memfd/ashmem shim, cgroup delegation. Local Linux 7.1 adaptation is a separate patch. |
| `tools/ath10k-fwencoder` | [qca/qca-swiss-army-knife](https://github.com/qca/qca-swiss-army-knife/blob/6df4dae3e2f5e4c2903f3cafd40996fc1b3639ce/tools/scripts/ath10k/ath10k-fwencoder), `6df4dae3e2f5e4c2903f3cafd40996fc1b3639ce` | ISC; copied unchanged, including Qualcomm Atheros copyright/license. |
| Other device files and patches | This ThinkSmart View port; based on the repositories named in the manifest | Existing source-tree licenses/notices apply to modifications. Patch headers explain each fix; diagnostic-only patches are excluded. |

GPL text: [LICENSES/GPL-2.0.txt](LICENSES/GPL-2.0.txt).
Choose a license for newly authored integration files before public distribution;
this repository does not relicense third-party material.

## Firmware (not included)

Stock EDL-readable `modem` and `vendor` partitions supply ADSP, Venus, Bluetooth,
GPU and Wi-Fi inputs. Wi-Fi packaging uses the encoder above with fixed metadata
to reproduce the qualified ath10k container.

`tas5728m_dsp_lenovo_cd-18781y.bin` instead comes from
[postmarketos-vendor-lenovo-cd-18781y](https://gitlab.com/kaechele/postmarketos-vendor-lenovo-cd-18781y),
revision `337032e864fd6abe29375997fc3683398a4d27c4`; it contains board DSP register
data derived from Lenovo's released kernel sources. It is not extracted from a
stock firmware partition. Firmware redistribution rights are not assumed.
