#!/bin/bash
set -euo pipefail
export LC_ALL=C

if [[ $# != 1 ]]; then
    echo "Usage: $0 ANDROID_ROOT" >&2
    exit 2
fi
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
android=$(CDPATH='' cd -- "$1" && pwd -P)
queues=()
projects=()
while IFS= read -r queue; do
    project=${queue#"$root/patches/"}
    for patch in "$queue"/*.patch; do
        if [[ ! "${patch##*/}" =~ ^[0-9]{4}-.+\.patch$ ]]; then
            echo "Expected a numbered patch (0001-description.patch): $patch" >&2
            exit 1
        fi
    done
    source="$android/$project"
    if [[ "$(git -C "$source" rev-parse --show-toplevel)" != "$source" ]]; then
        echo "Not an Android project checkout: $source" >&2
        exit 1
    fi
    if [[ -n "$(git -C "$source" status --porcelain)" ]]; then
        echo "Apply once to clean source projects; preserve existing edits in $source." >&2
        exit 1
    fi
    queues+=("$queue")
    projects+=("$source")
done < <(find "$root/patches" -type f -name '*.patch' -exec dirname {} \; | sort -u)
if [[ ${#queues[@]} == 0 ]]; then
    echo "No patch queues found in $root/patches" >&2
    exit 1
fi

codec="$android/kernel/mainline/msm8953-mainline/sound/soc/codecs"
for file in "$root"/drivers/tas5782m/*.[ch]; do
    if [[ -e "$codec/${file##*/}" || -L "$codec/${file##*/}" ]]; then
        echo "Refusing to replace existing codec source: $codec/${file##*/}" >&2
        exit 1
    fi
done
cp "$root"/drivers/tas5782m/*.[ch] "$codec/"
for ((i = 0; i < ${#queues[@]}; i++)); do
    echo "Patching ${projects[$i]}"
    for patch in "${queues[$i]}"/*.patch; do
        git -C "${projects[$i]}" apply --check "$patch"
        git -C "${projects[$i]}" apply "$patch"
    done
done
