#!/usr/bin/env python3
from pathlib import Path
import argparse, gzip, hashlib, json, struct

BOOT_MAGIC = b"ANDROID!"
BOOT_V2_HEADER_SIZE = 1660

def pad(data, page_size):
    rem = len(data) % page_size
    return data if rem == 0 else data + b"\0" * (page_size - rem)

def cstr(s, size):
    b = (s or "").encode("ascii", "ignore")
    if len(b) >= size:
        b = b[:size - 1]
    return b + b"\0" * (size - len(b))

def boot_id(*blobs):
    h = hashlib.sha1()
    for blob in blobs:
        h.update(blob)
        h.update(struct.pack("<I", len(blob)))
    return h.digest().ljust(32, b"\0")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--meta", required=True)
    ap.add_argument("--kernel", required=True)
    ap.add_argument("--dtb", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    meta = json.loads(Path(args.meta).read_text())
    page_size = int(meta["page_size"])

    raw_kernel = Path(args.kernel).read_bytes()
    if raw_kernel[0x38:0x3c] != b"ARM\x64":
        raise SystemExit(f"kernel is not arm64 Image: magic={raw_kernel[0x38:0x3c]!r}")

    text_offset, image_size, flags = struct.unpack_from("<QQQ", raw_kernel, 8)
    kernel = gzip.compress(raw_kernel, compresslevel=9, mtime=0)
    dtb = Path(args.dtb).read_bytes()
    ramdisk = b""
    second = b""
    recovery_dtbo = b""

    hdr = bytearray()
    hdr += BOOT_MAGIC
    hdr += struct.pack("<10I",
        len(kernel),
        int(meta["kernel_addr"]),
        len(ramdisk),
        int(meta["ramdisk_addr"]),
        len(second),
        int(meta["second_addr"]),
        int(meta["tags_addr"]),
        page_size,
        2,
        int(meta["os_version"]),
    )
    hdr += cstr(meta.get("name", ""), 16)
    cmdline = meta.get("cmdline", "")
    hdr += cstr(cmdline[:511], 512)
    hdr += boot_id(kernel, ramdisk, second, recovery_dtbo, dtb)
    hdr += cstr(cmdline[511:], 1024)
    hdr += struct.pack("<I", len(recovery_dtbo))
    hdr += struct.pack("<Q", 0)
    hdr += struct.pack("<I", BOOT_V2_HEADER_SIZE)
    hdr += struct.pack("<I", len(dtb))
    hdr += struct.pack("<Q", int(meta["dtb_addr"]))

    if len(hdr) != BOOT_V2_HEADER_SIZE:
        raise SystemExit(f"bad header size {len(hdr)}")

    out = pad(bytes(hdr), page_size)
    out += pad(kernel, page_size)
    out += pad(ramdisk, page_size)
    out += pad(second, page_size)
    out += pad(recovery_dtbo, page_size)
    out += pad(dtb, page_size)

    if len(out) > 32 * 1024 * 1024:
        raise SystemExit(f"boot.img too large for 32MiB boot partition: {len(out)}")

    Path(args.out).write_bytes(out)
    print(f"wrote {args.out}")
    print(f"kernel_raw=0x{len(raw_kernel):x} kernel_gzip=0x{len(kernel):x} dtb=0x{len(dtb):x} boot=0x{len(out):x}")
    print(f"text_offset=0x{text_offset:x} image_size=0x{image_size:x} flags=0x{flags:x}")
    print(f"kernel_addr=0x{int(meta['kernel_addr']):x} dtb_addr=0x{int(meta['dtb_addr']):x} page_size=0x{page_size:x}")

if __name__ == "__main__":
    main()
