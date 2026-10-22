#!/bin/bash
set -euo pipefail

usage() {
    echo "Usage: $0 --stock MODEM.img VENDOR.img SPEAKER.bin OUTPUT_DIR" >&2
    echo "       $0 --android-vendor VENDOR.img OUTPUT_DIR" >&2
    exit 2
}
[[ $# -gt 0 ]] || usage
mode=$1
case "$mode" in
    --stock)
        [[ $# == 5 ]] || usage
        modem=$(realpath "$2")
        vendor=$(realpath "$3")
        speaker=$(realpath "$4")
        output=$5
        command -v mcopy >/dev/null
        ;;
    --android-vendor)
        [[ $# == 3 ]] || usage
        vendor=$(realpath "$2")
        output=$3
        ;;
    *) usage ;;
esac
command -v debugfs >/dev/null
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
[[ ! -e "$output" && ! -L "$output" ]] || {
    echo "Output exists; refusing to replace firmware: $output" >&2
    exit 1
}
mkdir -p "$(dirname -- "$output")"
parent=$(CDPATH='' cd -- "$(dirname -- "$output")" && pwd)
output="$parent/$(basename -- "$output")"
work=$(mktemp -d "$parent/.tsv-firmware.XXXXXX")
trap 'if [[ -d "$work" ]]; then rm -r -- "$work"; fi' EXIT

dump_vendor() {
    local source=$1 destination=$2
    mkdir -p "$(dirname -- "$destination")"
    debugfs -R "cat $source" "$vendor" > "$destination"
    [[ -s "$destination" ]] || {
        echo "Missing vendor firmware: $source" >&2
        exit 1
    }
}
if [[ "$mode" == --android-vendor ]]; then
    while read -r _checksum name; do
        dump_vendor "/firmware/$name" "$work/$name"
    done < "$root/config/firmware.sha256"
else
    while read -r source name; do
        mkdir -p "$(dirname -- "$work/$name")"
        mcopy -i "$modem" "::/image/$source" "$work/$name"
    done < "$root/tools/stock-firmware.map"
    for name in a506_zap.mdt a506_zap.b02; do
        dump_vendor "/firmware/$name" "$work/qcom/msm8953/lenovo/cd-18781y/$name"
    done
    wifi="$work/ath10k/QCA9379/hw1.0"
    mkdir -p "$wifi" "$work/.wifi-raw"
    mcopy -i "$modem" ::/image/bdwlan30.bin "$wifi/board.bin"
    mcopy -i "$modem" ::/image/otp30.bin "$work/.wifi-raw/otp30.bin"
    mcopy -i "$modem" ::/image/qwlan30.bin "$work/.wifi-raw/qwlan30.bin"
    # Fixed metadata reproduces the qualified ath10k container byte-for-byte.
    python3 "$root/tools/ath10k-fwencoder" --create \
        --firmware-version=WLAN.NPL.1.6-00163-QCANPLSWPZ-1 --timestamp=1790577118 \
        --features=ignore-otp-result --set-wmi-op-version=tlv --set-htt-op-version=tlv \
        --otp="$work/.wifi-raw/otp30.bin" --firmware="$work/.wifi-raw/qwlan30.bin" \
        --output="$wifi/firmware-sdio-5.bin"
    rm -- "$work/.wifi-raw/otp30.bin" "$work/.wifi-raw/qwlan30.bin"
    rmdir "$work/.wifi-raw"
    cp "$speaker" "$work/tas5728m_dsp_lenovo_cd-18781y.bin"
fi
(cd "$work" && sha256sum --check "$root/config/firmware.sha256")
mv -T --no-clobber "$work" "$output"
if [[ -d "$work" ]]; then
    echo "Output appeared during extraction; refusing to replace it: $output" >&2
    exit 1
fi
echo "Extracted verified firmware to $output; input images were not modified."
