#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Package the verified, partition-sized LG6851F B-slot images for LuCI/sysupgrade.

set -eu

OWRT="${OWRT:-$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)}"
SIGNED_DIR="$OWRT/B槽单刷boot和rootfs签名成功包"
OUT_DIR="$OWRT/bin/targets/mediatek/mt6990"
OUT="$OUT_DIR/lg6851f-mt6990-b-signed-sysupgrade.tar.gz"
BOOT="$SIGNED_DIR/boot_b.img"
ROOTFS="$SIGNED_DIR/rootfs_b.img"
CONNSYS_GNSS="$OWRT/target/linux/mediatek/mt6990/base-files/usr/lib/firmware/lg6851f/connsys_gnss_b.img"
SIGNED_MANIFEST="$SIGNED_DIR/lg6851f-signed-boot-rootfs.manifest.json"
WORK="$(mktemp -d /tmp/lg6851f-sysupgrade.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

die() { echo "ERROR: $*" >&2; exit 1; }
[ -f "$BOOT" ] || die "missing signed boot: $BOOT"
[ -f "$ROOTFS" ] || die "missing exported rootfs: $ROOTFS"
[ -f "$SIGNED_MANIFEST" ] || die "missing signing manifest: $SIGNED_MANIFEST"
[ -f "$CONNSYS_GNSS" ] || die "missing official connsys_gnss image: $CONNSYS_GNSS"
[ "$(stat -c%s "$BOOT")" = 33554432 ] || die "boot_b.img is not 32 MiB"
[ "$(stat -c%s "$ROOTFS")" = 134217728 ] || die "rootfs_b.img is not 128 MiB"
[ "$(stat -c%s "$CONNSYS_GNSS")" = 8388608 ] || die "connsys_gnss_b.img is not 8 MiB"
[ "$(dd if="$BOOT" bs=8 count=1 2>/dev/null)" = "ANDROID!" ] || die "bad Android boot magic"
[ "$(dd if="$ROOTFS" bs=4 count=1 2>/dev/null)" = "hsqs" ] || die "bad SquashFS magic"

boot_sha="$(sha256sum "$BOOT" | awk '{print $1}')"
rootfs_sha="$(sha256sum "$ROOTFS" | awk '{print $1}')"
connsys_gnss_sha="$(sha256sum "$CONNSYS_GNSS" | awk '{print $1}')"
[ "$connsys_gnss_sha" = 633a1e2a19593d9cf12db4e140d893da9d1817c721381848831d90afc0b02463 ] ||
	die "official connsys_gnss_b.img SHA256 mismatch"
created_utc="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
mkdir -p "$OUT_DIR"

# The package must represent this invocation only. A failed rebuild must not
# leave a stale Web-upgrade archive behind.
rm -f "$OUT"

cp -f "$BOOT" "$WORK/boot_b.img"
cp -f "$ROOTFS" "$WORK/rootfs_b.img"
cp -f "$CONNSYS_GNSS" "$WORK/connsys_gnss_b.img"
cp -f "$SIGNED_MANIFEST" "$WORK/signing-manifest.json"
printf '%s  %s\n%s  %s\n%s  %s\n' \
	"$boot_sha" boot_b.img "$rootfs_sha" rootfs_b.img \
	"$connsys_gnss_sha" connsys_gnss_b.img > "$WORK/SHA256SUMS"
cat > "$WORK/CONTROL" <<EOF
format=lg6851f-mt6990-b-signed-v1
slot=b
boot_partition=boot_b
rootfs_partition=rootfs_b
connsys_gnss_partition=connsys_gnss_b
boot_size=33554432
rootfs_size=134217728
connsys_gnss_size=8388608
boot_sha256=$boot_sha
rootfs_sha256=$rootfs_sha
connsys_gnss_sha256=$connsys_gnss_sha
touches_bootctrl=priority-and-b-retry
touches_slot_a=0
EOF

tar --sort=name --mtime="$created_utc" --owner=0 --group=0 --numeric-owner \
	-C "$WORK" -czf "$OUT" CONTROL SHA256SUMS signing-manifest.json connsys_gnss_b.img boot_b.img rootfs_b.img

tar -tzf "$OUT"
printf '%s\n' "RESULT=LG6851F_SIGNED_B_SYSUPGRADE_READY"
printf 'PACKAGE=%s\n' "$OUT"
printf 'SIZE=%s\n' "$(stat -c%s "$OUT")"
printf 'SHA256=%s\n' "$(sha256sum "$OUT" | awk '{print $1}')"
