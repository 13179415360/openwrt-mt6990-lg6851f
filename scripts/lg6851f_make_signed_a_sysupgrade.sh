#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu

OWRT="${OWRT:-$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)}"
SIGNED_DIR="$OWRT/A槽单刷boot和rootfs签名成功包"
OUT_DIR="$OWRT/bin/targets/mediatek/mt6990"
OUT="$OUT_DIR/lg6851f-mt6990-a-signed-sysupgrade.tar.gz"
BOOT="$SIGNED_DIR/boot_a.img"
ROOTFS="$SIGNED_DIR/rootfs_a.img"
CONNSYS_GNSS="$OWRT/target/linux/mediatek/mt6990/base-files/usr/lib/firmware/lg6851f/connsys_gnss_b.img"
MANIFEST="$SIGNED_DIR/lg6851f-signed-boot-rootfs.manifest.json"
WORK="$(mktemp -d /tmp/lg6851f-a-sysupgrade.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM
die() { echo "ERROR: $*" >&2; exit 1; }

[ -f "$BOOT" ] && [ "$(stat -c%s "$BOOT")" = 33554432 ] || die "invalid boot_a.img"
[ -f "$ROOTFS" ] && [ "$(stat -c%s "$ROOTFS")" = 134217728 ] || die "invalid rootfs_a.img"
[ -f "$MANIFEST" ] || die "missing signing manifest"
[ -f "$CONNSYS_GNSS" ] && [ "$(stat -c%s "$CONNSYS_GNSS")" = 8388608 ] || die "invalid shared connsys_gnss image"
[ "$(dd if="$BOOT" bs=8 count=1 2>/dev/null)" = "ANDROID!" ] || die "bad boot magic"
[ "$(dd if="$ROOTFS" bs=4 count=1 2>/dev/null)" = "hsqs" ] || die "bad rootfs magic"
boot_sha="$(sha256sum "$BOOT" | awk '{print $1}')"
rootfs_sha="$(sha256sum "$ROOTFS" | awk '{print $1}')"
connsys_gnss_sha="$(sha256sum "$CONNSYS_GNSS" | awk '{print $1}')"
[ "$connsys_gnss_sha" = 633a1e2a19593d9cf12db4e140d893da9d1817c721381848831d90afc0b02463 ] ||
	die "official shared connsys_gnss image SHA256 mismatch"
created_utc="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
mkdir -p "$OUT_DIR"
rm -f "$OUT"
cp -f "$BOOT" "$WORK/boot_a.img"
cp -f "$ROOTFS" "$WORK/rootfs_a.img"
cp -f "$CONNSYS_GNSS" "$WORK/connsys_gnss_a.img"
cp -f "$MANIFEST" "$WORK/signing-manifest.json"
printf '%s  %s\n%s  %s\n%s  %s\n' \
	"$boot_sha" boot_a.img "$rootfs_sha" rootfs_a.img \
	"$connsys_gnss_sha" connsys_gnss_a.img > "$WORK/SHA256SUMS"
cat > "$WORK/CONTROL" <<EOF
format=lg6851f-mt6990-a-signed-v1
slot=a
boot_partition=boot_a
rootfs_partition=rootfs_a
connsys_gnss_partition=connsys_gnss_a
boot_size=33554432
rootfs_size=134217728
connsys_gnss_size=8388608
boot_sha256=$boot_sha
rootfs_sha256=$rootfs_sha
connsys_gnss_sha256=$connsys_gnss_sha
touches_bootctrl=priority-and-a-retry
touches_slot_b=0
EOF
tar --sort=name --mtime="$created_utc" --owner=0 --group=0 --numeric-owner \
	-C "$WORK" -czf "$OUT" CONTROL SHA256SUMS signing-manifest.json connsys_gnss_a.img boot_a.img rootfs_a.img
tar -tzf "$OUT"
echo RESULT=LG6851F_SIGNED_A_SYSUPGRADE_READY
echo PACKAGE="$OUT"
echo SIZE="$(stat -c%s "$OUT")"
echo SHA256="$(sha256sum "$OUT" | awk '{print $1}')"
