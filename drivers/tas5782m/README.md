# TAS5782M codec sources

Adapted from [rickx/ubuntu-thinksmart](https://github.com/rickx/ubuntu-thinksmart/tree/7b1a5f3e6b3cd85d10a2f81a56c0e778548375bd/sources/driver),
revision `7b1a5f3e6b3cd85d10a2f81a56c0e778548375bd`, with additional work
by Kacper Adler in [postmarketOS pmaports MR !8928](https://gitlab.postmarketos.org/postmarketOS/pmaports/-/merge_requests/8928).
That submission was closed, not merged.

The files retain TI/Andy Liu, Daniel Beer and Felix Kaechele attribution and
their GPL-2.0-only licensing. See the per-file notices and
[GPL text](../../LICENSES/GPL-2.0.txt).

Local changes require the board's DSP tuning, wait for BCLK before startup,
correct mute-GPIO behavior, bound digital attenuation and check DSP
coefficient-swap I/O. The driver also references TI's TAS5782M reference
implementation, SLASEG8A coefficient-swap protocol and Lenovo's CD-18781Y
released kernel/DTS sources.

`tools/apply-patches.sh` copies these C/header files into the kernel codec
directory; the registration patch supplies the kernel build integration.
