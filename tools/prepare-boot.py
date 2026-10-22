#!/usr/bin/env python3
"""Compose physical boot while preserving this device's installed lk2nd."""
import argparse
import hashlib
import os
import struct
import sys
import tempfile
import zlib
from pathlib import Path

PREFIX_BYTES = 512 * 1024
PARTITION_BYTES = 32 * 1024 * 1024


def validate_payload(data, dtb):
    if not 2048 <= len(data) <= PARTITION_BYTES - PREFIX_BYTES:
        raise ValueError("Android payload does not fit after lk2nd")
    magic, kernel_size, _, ramdisk_size, _, second, _, _, page, version, _ = (
        struct.unpack_from("<8s10I", data)
    )
    if magic != b"ANDROID!" or version != 0 or page != 2048 or second:
        raise ValueError("Expected an lk2nd-compatible Android v0 payload")
    align = lambda size: ((size + page - 1) // page) * page
    if not kernel_size or not ramdisk_size or len(data) != page + align(kernel_size) + align(ramdisk_size):
        raise ValueError("Boot component sizes do not match the payload")
    decoder = zlib.decompressobj(16 + zlib.MAX_WBITS)
    image = decoder.decompress(data[page:page + kernel_size], 50 * 1024 * 1024 + 1)
    if not decoder.eof or len(image) < 64 or image[56:60] != b"ARM\x64":
        raise ValueError("Invalid or oversized ARM64 gzip kernel")
    text_offset, image_size = struct.unpack_from("<2Q", image, 8)
    if not image_size or max(image_size, len(image)) + text_offset > 50 * 1024 * 1024:
        raise ValueError("ARM64 kernel exceeds lk2nd's reserved boot window")
    appended = decoder.unused_data
    if (len(appended) < 40 or struct.unpack_from(">I", appended)[0] != 0xD00DFEED
            or struct.unpack_from(">I", appended, 4)[0] != len(appended)
            or appended != dtb):
        raise ValueError("Expected exactly the matching ThinkSmart DTB after the gzip kernel")
    ramdisk = page + align(kernel_size)
    if data[ramdisk:ramdisk + 4] not in (b"\x02\x21\x4c\x18", b"\x04\x22\x4d\x18"):
        raise ValueError("Expected an LZ4 ramdisk")


def prepare(existing, payload, dtb, output):
    if existing.stat().st_size != PARTITION_BYTES:
        raise ValueError("Use this device's full physical 32 MiB boot backup")
    if output.exists() or output.is_symlink():
        raise ValueError("Output exists; refusing to replace it")
    data = payload.read_bytes()
    validate_payload(data, dtb.read_bytes())
    with existing.open("rb") as stream:
        prefix = stream.read(PREFIX_BYTES)
    fields = struct.unpack_from("<8s10I", prefix)
    command_line = prefix[64:576].split(b"\0")[0].split()
    page = fields[8]
    if (fields[0] != b"ANDROID!" or not command_line or command_line[0] != b"lk2nd"
            or page not in (2048, 4096) or not fields[1]):
        raise ValueError("Backup does not contain a recognized installed lk2nd")
    aligned = lambda size: ((size + page - 1) // page) * page
    if page + sum(aligned(fields[index]) for index in (1, 3, 5, 9)) > PREFIX_BYTES:
        raise ValueError("Installed lk2nd exceeds the reserved 512 KiB prefix")
    with tempfile.TemporaryDirectory(prefix=".tsv-boot-", dir=output.parent) as directory:
        temporary = Path(directory) / "boot.img"
        with temporary.open("wb") as stream:
            stream.write(prefix)
            stream.write(data)
            stream.truncate(PARTITION_BYTES)
        with temporary.open("rb") as stream:
            if stream.read(PREFIX_BYTES) != prefix or stream.read(len(data)) != data:
                raise ValueError("Composed image differs from its inputs")
        os.link(temporary, output)
    digest = hashlib.sha256()
    with output.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--existing-boot", required=True, type=Path)
    parser.add_argument("--payload", required=True, type=Path)
    parser.add_argument("--dtb", required=True, type=Path, help="Matching built apq8053-lenovo-cd-18781y.dtb")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        digest = prepare(args.existing_boot, args.payload, args.dtb, args.output)
        print(f"{digest}  {args.output}")
        print("Preserved the exact lk2nd prefix; no device was accessed.")
    except (OSError, ValueError, zlib.error) as error:
        sys.exit(f"Boot preparation failed: {error}")
