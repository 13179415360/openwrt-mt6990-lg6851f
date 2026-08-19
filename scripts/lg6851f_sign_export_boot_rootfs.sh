#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# LG6851F / MT6990 OpenWrt post-build signer/exporter.
#
# Run after OpenWrt build finishes:
#   cd /home/zjl-pc/openwrt
#   ./scripts/lg6851f_sign_export_boot_rootfs.sh
#
# All persistent outputs are written only to:
#   /home/zjl-pc/openwrt/boot和rootfs签名成功包/
#
# Notes:
# - This script uses the MTK signing toolchain only for post-build boot signing.
# - It does not use SDK DTS/SOC/driver content.
# - It signs boot.img with mt2737/sec_level_0 cert1 + ROOT/IMAGE hsm_test_keys,
#   matching the previously verified LG6851F boot_b signing chain.

set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
OWRT="${OWRT:-$(dirname "$SCRIPT_DIR")}"
BUNDLED_SDK="$OWRT/lg6851f_sign_tool/fg370-2305-opensdk"
SDK="${SDK:-$BUNDLED_SDK}"
BOOT_REF="${BOOT_REF:-$OWRT/lg6851f_sign_tool/reference/boot_b-reference.img}"

KDIR="$OWRT/build_dir/target-aarch64_cortex-a55+neon-vfpv4_musl/linux-mediatek_mt6990"

BOOT_PART_SIZE=33554432
ROOTFS_PART_SIZE=134217728

FINAL_DIR="$OWRT/boot和rootfs签名成功包"
WORK="$(mktemp -d /tmp/lg6851f-sign.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

UNSIGNED_BOOT="$WORK/boot.img.unsigned"
FINAL_BOOT="$FINAL_DIR/boot_b.img"
FINAL_ROOTFS="$FINAL_DIR/rootfs_b.img"
MANIFEST="$FINAL_DIR/lg6851f-signed-boot-rootfs.manifest.json"
SHA_FILE="$FINAL_DIR/SHA256SUMS.txt"

CERT1="$SDK/mtk/tools/common/security/config/mt2737/cert_config/sec_level_0/cert1/boot_cert1.der"
SIGN_TOOL_DIR="$SDK/mtk/tools/common/security/tool"
ROOT_KEY="$SDK/mtk/tools/common/security/sign_master/hsm_test_keys/RSA2048/ROOT"
IMAGE_KEY="$SDK/mtk/tools/common/security/sign_master/hsm_test_keys/RSA2048/IMAGE"

KERNEL_IMAGE="$KDIR/Image"
DTB_IMAGE="$KDIR/image-mt6990-fiberhome-lg6851f.dtb"
ROOTFS_IMAGE="$KDIR/root.squashfs"

die() {
	echo "ERROR: $*" >&2
	exit 1
}

need_file() {
	[ -f "$1" ] || die "missing file: $1"
}

sha256_one() {
	sha256sum "$1" | awk '{print $1}'
}

need_file "$BOOT_REF"
need_file "$KERNEL_IMAGE"
need_file "$DTB_IMAGE"
need_file "$ROOTFS_IMAGE"
need_file "$CERT1"
need_file "$SIGN_TOOL_DIR/sign_flow.py"
need_file "${ROOT_KEY}_prvk.pem"
need_file "${IMAGE_KEY}_prvk.pem"

mkdir -p "$FINAL_DIR" "$WORK/in" "$WORK/log"

# Never let a failed signing attempt leave an older boot image looking like
# the result of the current build.
rm -f "$FINAL_BOOT" "$MANIFEST" "$SHA_FILE"

echo "===== LG6851F one-key signed boot/rootfs exporter ====="
echo "OWRT=$OWRT"
echo "SDK=$SDK"
echo "BOOT_REF=$BOOT_REF"
echo "KERNEL_IMAGE=$KERNEL_IMAGE"
echo "DTB_IMAGE=$DTB_IMAGE"
echo "ROOTFS_IMAGE=$ROOTFS_IMAGE"
echo "WORK=$WORK"

echo
echo "===== 1/5 export rootfs_b.img, padded to 128MiB ====="
cp -f "$ROOTFS_IMAGE" "$FINAL_ROOTFS"
truncate -s "$ROOTFS_PART_SIZE" "$FINAL_ROOTFS"

echo
echo "===== 2/5 build unsigned Android boot image from current Image + DTB ====="
python3 - "$BOOT_REF" "$KERNEL_IMAGE" "$DTB_IMAGE" "$UNSIGNED_BOOT" "$WORK/boot-v2-meta.json" <<'PY'
from pathlib import Path
import gzip
import hashlib
import json
import struct
import sys

boot_ref = Path(sys.argv[1])
kernel_path = Path(sys.argv[2])
dtb_path = Path(sys.argv[3])
out_path = Path(sys.argv[4])
meta_path = Path(sys.argv[5])

d = boot_ref.read_bytes()
if d[:8] != b"ANDROID!":
    raise SystemExit(f"reference is not Android boot image: {boot_ref}")

kernel_size, kernel_addr, ramdisk_size, ramdisk_addr, second_size, second_addr, tags_addr, page_size, header_version, os_version = struct.unpack_from("<10I", d, 8)
if header_version < 2:
    raise SystemExit(f"expected Android boot header v2 reference, got v{header_version}")

name = d[48:64].rstrip(b"\0").decode("ascii", "ignore")
cmdline = (d[64:576] + d[608:1632]).split(b"\0", 1)[0].decode("ascii", "ignore")
dtb_addr = struct.unpack_from("<Q", d, 1652)[0]

raw_kernel = kernel_path.read_bytes()
if raw_kernel[0x38:0x3c] != b"ARM\x64":
    raise SystemExit(f"kernel is not ARM64 Image: magic={raw_kernel[0x38:0x3c]!r}")

text_offset, image_size, flags = struct.unpack_from("<QQQ", raw_kernel, 8)
kernel = gzip.compress(raw_kernel, compresslevel=9, mtime=0)
dtb = dtb_path.read_bytes()
ramdisk = b""
second = b""
recovery_dtbo = b""

def pad(blob: bytes) -> bytes:
    r = len(blob) % page_size
    return blob if r == 0 else blob + b"\0" * (page_size - r)

def cstr(s: str, size: int) -> bytes:
    b = (s or "").encode("ascii", "ignore")
    if len(b) >= size:
        b = b[:size - 1]
    return b + b"\0" * (size - len(b))

def boot_id(*blobs: bytes) -> bytes:
    h = hashlib.sha1()
    for blob in blobs:
        h.update(blob)
        h.update(struct.pack("<I", len(blob)))
    return h.digest().ljust(32, b"\0")

hdr = bytearray()
hdr += b"ANDROID!"
hdr += struct.pack("<10I",
    len(kernel), kernel_addr,
    len(ramdisk), ramdisk_addr,
    len(second), second_addr,
    tags_addr, page_size,
    2, os_version,
)
hdr += cstr(name, 16)
hdr += cstr(cmdline[:511], 512)
hdr += boot_id(kernel, ramdisk, second, recovery_dtbo, dtb)
hdr += cstr(cmdline[511:], 1024)
hdr += struct.pack("<I", len(recovery_dtbo))
hdr += struct.pack("<Q", 0)
hdr += struct.pack("<I", 1660)
hdr += struct.pack("<I", len(dtb))
hdr += struct.pack("<Q", dtb_addr)

if len(hdr) != 1660:
    raise SystemExit(f"bad Android boot v2 header size: {len(hdr)}")

out = pad(bytes(hdr)) + pad(kernel) + pad(ramdisk) + pad(second) + pad(recovery_dtbo) + pad(dtb)
out_path.write_bytes(out)

meta = {
    "boot_ref": str(boot_ref),
    "kernel": str(kernel_path),
    "dtb": str(dtb_path),
    "unsigned_boot": str(out_path),
    "header_version": 2,
    "page_size": page_size,
    "kernel_addr": kernel_addr,
    "ramdisk_addr": ramdisk_addr,
    "second_addr": second_addr,
    "tags_addr": tags_addr,
    "dtb_addr": dtb_addr,
    "cmdline": cmdline,
    "name": name,
    "kernel_raw_size": len(raw_kernel),
    "kernel_gzip_size": len(kernel),
    "dtb_size": len(dtb),
    "unsigned_size": len(out),
    "arm64_text_offset": text_offset,
    "arm64_image_size": image_size,
    "arm64_flags": flags,
    "unsigned_sha256": hashlib.sha256(out).hexdigest(),
}
meta_path.write_text(json.dumps(meta, indent=2, sort_keys=True) + "\n")
print(json.dumps(meta, indent=2, sort_keys=True))
PY

UNSIGNED_SHA="$(sha256_one "$UNSIGNED_BOOT")"

echo
echo "===== 3/5 require a fresh boot signature for this build ====="
# Delivery contract: every successful full MT6990 build must sign both the
# boot image assembled from that build's Image+DTB and that build's rootfs.
# Never reuse an older signed boot even when the unsigned bytes are identical.
DECISION=SIGN
echo "DECISION=$DECISION (forced; signed boot reuse is forbidden)"

SIGNED_BOOT="$FINAL_BOOT"
CERT2=""

if [ "$DECISION" = "SIGN" ]; then
	echo
	echo "===== 4/5 sign boot.img.unsigned with MTK sign_flow.py ====="
	cp -f "$UNSIGNED_BOOT" "$WORK/in/boot.img"
	cp -f "$UNSIGNED_BOOT" "$WORK/in/boot.orig.img"

	PYTHON_BIN="$(command -v python3.10 || command -v python3 || true)"
	[ -n "$PYTHON_BIN" ] || die "python3.10 or python3 is required"
	mkdir -p "$WORK/python-wrap"
	ln -sf "$PYTHON_BIN" "$WORK/python-wrap/python3"
	export PATH="$WORK/python-wrap:$PATH"

	cd "$SIGN_TOOL_DIR"

	PYTHONDONTWRITEBYTECODE=True \
	PRODUCT_OUT="$WORK/in" \
	BOARD_AVB_ENABLE= \
	python3 sign_flow.py image mt2737 default \
	  hsm=1 \
	  root_key_path="$ROOT_KEY" \
	  oem_key_path="$IMAGE_KEY" \
	  root_key_padding=pss \
	  2>&1 | tee "$WORK/log/sign_boot.log"

	cd "$OWRT"

	SIGNED_BOOT="$WORK/in/boot-verified.img"
	CERT2="$WORK/in/resign/cert/boot/boot/cert2/intermediate/boot_cert2.der"

	need_file "$SIGNED_BOOT"
	need_file "$CERT2"

	cp -f "$SIGNED_BOOT" "$FINAL_BOOT"
	truncate -s "$BOOT_PART_SIZE" "$FINAL_BOOT"
fi

echo
echo "===== 5/5 verify final files ====="
python3 - "$UNSIGNED_BOOT" "$FINAL_BOOT" "$FINAL_ROOTFS" "$CERT1" "$CERT2" "$MANIFEST" "$WORK" "$BOOT_PART_SIZE" "$ROOTFS_PART_SIZE" <<'PY'
from pathlib import Path
import hashlib
import json
import sys

unsigned = Path(sys.argv[1])
boot = Path(sys.argv[2])
rootfs = Path(sys.argv[3])
cert1 = Path(sys.argv[4]).read_bytes()
cert2_arg = sys.argv[5]
manifest = Path(sys.argv[6])
work = Path(sys.argv[7])
boot_part_size = int(sys.argv[8])
rootfs_part_size = int(sys.argv[9])

def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

boot_data = boot.read_bytes()
rootfs_data = rootfs.read_bytes()

off = boot_data.find(cert1)
cert1_ok = off >= 0 and boot_data[off:off + len(cert1)] == cert1

if cert2_arg:
    cert2 = Path(cert2_arg).read_bytes()
    cert2_ok = off >= 0 and boot_data[off + len(cert1):off + len(cert1) + len(cert2)] == cert2
else:
    cert2_ok = off >= 0 and b"cert2" in boot_data[off + len(cert1):off + len(cert1) + 4096]

if boot.stat().st_size != boot_part_size:
    raise SystemExit(f"BAD boot_b.img size: {boot.stat().st_size}")
if rootfs.stat().st_size != rootfs_part_size:
    raise SystemExit(f"BAD rootfs_b.img size: {rootfs.stat().st_size}")
if not cert1_ok or not cert2_ok:
    raise SystemExit(f"BAD cert check: cert1_ok={cert1_ok} cert2_ok={cert2_ok}")
if not rootfs_data.startswith(b"hsqs"):
    raise SystemExit("BAD rootfs_b.img: does not start with squashfs magic")

info = {
    "work": str(work),
    "unsigned_boot": str(unsigned),
    "boot_b": str(boot),
    "rootfs_b": str(rootfs),
    "unsigned_boot_size": unsigned.stat().st_size,
    "boot_b_size": boot.stat().st_size,
    "rootfs_b_size": rootfs.stat().st_size,
    "cert1_off": off,
    "cert1_ok": cert1_ok,
    "cert2_ok": cert2_ok,
    "unsigned_boot_sha256": sha(unsigned),
    "boot_b_sha256": sha(boot),
    "rootfs_b_sha256": sha(rootfs),
}
manifest.write_text(json.dumps(info, indent=2, sort_keys=True) + "\n")
print(json.dumps(info, indent=2, sort_keys=True))
PY

sha256sum "$FINAL_BOOT" "$FINAL_ROOTFS" | tee "$SHA_FILE"

echo
echo "===== final files ====="
file "$FINAL_BOOT" "$FINAL_ROOTFS" "$UNSIGNED_BOOT"
stat -c '%n size=%s' "$FINAL_BOOT" "$FINAL_ROOTFS" "$UNSIGNED_BOOT"
ls -lh "$FINAL_BOOT" "$FINAL_ROOTFS" "$UNSIGNED_BOOT"

echo
echo "RESULT=LG6851F_SIGNED_BOOT_ROOTFS_READY"
echo "BOOT=$FINAL_BOOT"
echo "ROOTFS=$FINAL_ROOTFS"
echo "MANIFEST=$MANIFEST"
