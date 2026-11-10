# Building ThinkSmart View images

## 1. Set up the build host

Use x86-64 Linux, Bash and a case-sensitive filesystem with at least **350 GiB
free**. Dependencies are listed in the [Dockerfile](Dockerfile).

From this checkout, start the Docker builder:

```sh
docker compose build
docker compose run --rm builder
```

Docker starts in `/android`, a persistent source volume. This checkout is
mounted at `/integration`. Set `HOST_UID` / `HOST_GID` before building the
container if needed. On Apple Silicon, enable Docker's amd64/Rosetta emulation.

On native Linux, install the Dockerfile dependencies and create an empty
Android source directory instead:

```sh
mkdir android-tsv
cd android-tsv
```

Run the remaining build commands from the Android source root in the same Bash
session. In Docker, that is `/android`.

## 2. Fetch sources and apply patches

```sh
repo init -u https://github.com/LineageOS/android.git -b lineage-24.0 --git-lfs
DEVICE=device/lenovo/thinksmart
mkdir -p device/lenovo .repo/local_manifests
git clone --recurse-submodules https://github.com/salaman/tsv-lineage.git "$DEVICE"
```

To build from this checkout instead, replace the `git clone` command with:

```sh
cp -a /integration "$DEVICE"
git -C "$DEVICE" submodule update --init --recursive
```

Outside Docker, replace `/integration` with this checkout's absolute path.
Use a real device directory, not a symlink.

Sync the platform and kernel, then apply the patch queues:

```sh
cp "$DEVICE/manifests/thinksmart.xml" .repo/local_manifests/
repo sync -c --no-tags --no-clone-bundle --fail-fast -j4
bash "$DEVICE/tools/apply-patches.sh" "$PWD"
```

Apply queues **once to clean projects**. Patches run in numbered filename order
within each project, starting at `0001`; there are no `series` files. The helper
leaves uncommitted changes and stops on errors without resetting sources.
Branch-tracking dependencies can change.

Scripts and patches require LF line endings; `.gitattributes` enforces this
for new checkouts.

## 3. Extract firmware

### Firmware inputs

| Input | Origin |
|---|---|
| ADSP, Venus, Bluetooth | Stock `modem` partition |
| GPU zap | Stock `vendor` partition; not `modem/image/gfx.mdt` |
| Wi-Fi | Stock modem board/firmware/OTP; the helper creates the ath10k container |
| Speaker DSP tuning | [Community board firmware](https://gitlab.com/kaechele/postmarketos-vendor-lenovo-cd-18781y), revision `337032e864fd6abe29375997fc3683398a4d27c4`, `sound/tas5728m_dsp_lenovo_cd-18781y.bin` |

The speaker DSP register data comes from Lenovo's released kernel sources.
Wi-Fi packaging uses the [credited ath10k encoder](tools/README.md) with fixed
metadata. The extraction helper checks all 29 files against
[firmware.sha256](config/firmware.sha256) and refuses an existing output directory.
Keep firmware private; redistribution permission is not assumed.

With Docker, keep inputs in this checkout's `private/` directory, mounted at
`/integration/private` in the builder. In the build shell, set:

```sh
PRIVATE=/integration/private
```

On native Linux, set `PRIVATE` to the directory holding your private inputs.

### Read stock partitions with EDL

Run these on the host from this checkout, using your configured EDL installation:

```sh
mkdir -p private
edl r modem private/modem.img
edl r vendor private/vendor.img
```

Put the community speaker DSP file listed above in the same directory. Then,
in the build shell:

```sh
bash "$DEVICE/tools/extract-firmware.sh" --stock \
  "$PRIVATE/modem.img" "$PRIVATE/vendor.img" "$PRIVATE/tas5728m_dsp_lenovo_cd-18781y.bin" \
  "$DEVICE/firmware"
```

### Read firmware with root ADB

Alternatively, for a device already running this port, read its vendor partition
on the host. This requires root ADB and a read-only `/vendor` mount:

```sh
mkdir -p private
adb exec-out cat /dev/block/by-name/vendor > private/vendor.img
```

Then, in the build shell:

```sh
bash "$DEVICE/tools/extract-firmware.sh" --android-vendor \
  "$PRIVATE/vendor.img" "$DEVICE/firmware"
```

For stock Android, also read `modem` on the host and use the `--stock` extraction
command above with the speaker DSP file:

```sh
adb exec-out cat /dev/block/by-name/modem > private/modem.img
```

## 4. Build Android images

```sh
source build/envsetup.sh
lunch lineage_thinksmart cp2a userdebug
m bootimage recoveryimage systemimage vendorimage
```

Boots are quiet by default; ADB `logcat` and `dmesg` remain available. For
on-screen diagnostics, export `THINKSMART_BOOT_CONSOLE=true` before `lunch`.

Set the output paths and add Android's host tools to `PATH`:

```sh
PRODUCT="$PWD/out/target/product/thinksmart"
FLASH="$PWD/flash-images"
export PATH="$PWD/out/host/linux-x86/bin:$PATH"
mkdir "$FLASH"
```

## 5. Build lk2nd (optional)

Skip this step if you will preserve the installed lk2nd. To build a new loader,
install the ARM32 toolchain and Python signing dependencies listed in the
[upstream build guide](https://github.com/msm8916-mainline/lk2nd/blob/main/Documentation/building.md).
Clone lk2nd, or set `LK2ND` to an existing checkout:

```sh
LK2ND="$HOME/lk2nd"
git clone https://github.com/msm8916-mainline/lk2nd.git "$LK2ND"
make -C "$LK2ND" SIGN_BOOTIMG=1 LK2ND_QCDTBS= DEBUG=0 lk2nd-msm8953
```

Signing and empty QCDT are required for this model. `DEBUG` defaults to `1`;
display logging is off unless `DEBUG_FBCON=1`, so `DEBUG_FBCON=0` is unnecessary.
The loader is `build-lk2nd-msm8953/lk2nd.img` inside the lk2nd checkout.
Upstream uses public test signing credentials, not a private secure-boot key.

## 6. Prepare boot, system and vendor images

The Android build's `boot.img` is a payload, not a full physical boot image.
Use the matching DTB from the same build:

```sh
DTB="$PRODUCT/obj/KERNEL_OBJ/arch/arm64/boot/dts/qcom/apq8053-lenovo-cd-18781y.dtb"
```

Choose one of the following boot-image recipes.

### Preserve the installed lk2nd

Use this device's full physical 32 MiB boot backup containing lk2nd, never
another unit's. Reuse an existing backup while the installed lk2nd is unchanged.

If you need a new backup, read it on the host from this checkout with root ADB,
or use the EDL backup command in [FLASHING.md](FLASHING.md):

```sh
mkdir -p private
adb exec-out cat /dev/block/by-name/boot > private/original-physical-boot.img
```

Then, in the build shell:

```sh
python3 "$DEVICE/tools/prepare-boot.py" \
  --existing-boot "$PRIVATE/original-physical-boot.img" \
  --payload "$PRODUCT/boot.img" --dtb "$DTB" --output "$FLASH/boot.img"
```

### Include a new lk2nd

Use the signed loader from step 5. The helper checks layout, not signature
authenticity.

```sh
python3 "$DEVICE/tools/prepare-boot.py" \
  --lk2nd "$LK2ND/build-lk2nd-msm8953/lk2nd.img" \
  --payload "$PRODUCT/boot.img" --dtb "$DTB" --output "$FLASH/boot.img"
```

### Convert system and vendor to raw images

```sh
simg2img "$PRODUCT/system.img" "$FLASH/system.img"
simg2img "$PRODUCT/vendor.img" "$FLASH/vendor.img"
```

## 7. Generate clean-install wipe images

Skip this step for data-preserving updates. Generate fresh filesystems, never
use device userdata or metadata snapshots.

```sh
m userdataimage cacheimage
```

### Userdata

Enable ext4 encryption and per-file verity before first boot:

```sh
simg2img "$PRODUCT/userdata.img" "$PRODUCT/userdata-fbe.raw.img"
tune2fs -O encrypt,verity "$PRODUCT/userdata-fbe.raw.img"
e2fsck -f -n "$PRODUCT/userdata-fbe.raw.img"
img2simg "$PRODUCT/userdata-fbe.raw.img" "$FLASH/userdata-wipe.img"
rm -- "$PRODUCT/userdata-fbe.raw.img"
```

### Cache

```sh
cp "$PRODUCT/cache.img" "$FLASH/cache-wipe.img"
```

### Metadata (OEM)

Create a journaled ext4 filesystem for the stock 100 MiB OEM partition, using
the compiled SELinux contexts from this Android build. Android initializes its
metadata state on first boot.

```sh
mkdir "$PRODUCT/empty-metadata"
mkuserimg_mke2fs -s -T 0 -I 256 -L metadata \
  "$PRODUCT/empty-metadata" "$FLASH/metadata-wipe.img" ext4 /metadata 104857600 \
  "$PRODUCT/obj/ETC/file_contexts.bin_intermediates/file_contexts.bin"
rmdir "$PRODUCT/empty-metadata"
```

Wipe OEM (`/metadata`) only alongside userdata/cache for a clean install,
never on its own during a data-preserving update.

## 8. Prepare recovery (optional)

Recovery does not contain lk2nd. Sign it separately for `/recovery` using the
lk2nd checkout and signing dependencies from step 5:

```sh
python3 "$LK2ND/lk2nd/scripts/bootsignature.py" -s -t /recovery \
  -k "$LK2ND/lk2nd/certs/verity.pk8" -c "$LK2ND/lk2nd/certs/verity.x509.pem" \
  -i "$PRODUCT/recovery.img" -o "$FLASH/recovery.img"
printf %s SEANDROIDENFORCE >> "$FLASH/recovery.img"
test "$(stat -c %s "$FLASH/recovery.img")" -le 33554432 \
  && truncate -s 33554432 "$FLASH/recovery.img"
```

This uses upstream public test credentials. Confirm recovery boots on this
device before distributing it; signing alone does not make it boot-compatible.

## 9. Add flashing instructions and checksums

Copy the flashing guide:

```sh
cp "$DEVICE/FLASHING.md" "$FLASH/FLASHING.md"
```

If you generated wipe images, append their expanded sizes and hashes for
readback verification:

```sh
(
  set -e
  printf '\n## Expanded wipe-image checksums\n\n'
  printf '| Image | Expanded bytes | SHA-256 |\n|---|---:|---|\n'
  for image in userdata cache metadata; do
    raw="$PRODUCT/$image-readback.raw.img"
    simg2img "$FLASH/$image-wipe.img" "$raw"
    bytes=$(stat -c %s "$raw")
    digest=$(sha256sum "$raw")
    printf '| `%s-wipe.img` | %s | `%s` |\n' "$image" \
      "$bytes" "${digest%% *}"
    rm -- "$raw"
  done
) >> "$FLASH/FLASHING.md"
```

Hash only the recorded expanded lengths when checking wipe-image readbacks;
userdata has an unwritten tail outside its filesystem.

Finally, checksum the prepared images and guide:

```sh
(cd "$FLASH" && sha256sum *.img FLASHING.md > SHA256SUMS)
```

The prepared files are in `flash-images/`. Follow [FLASHING.md](FLASHING.md)
for installation or updates. Keep firmware inputs and device backups outside
that directory.
