#!/bin/sh
set -eu

TOPDIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUTDIR=${1:-"$TOPDIR/bin/mt6990-apk-repository"}
KERNEL_VERSION=$(sed -n 's/^LINUX_VERSION-6\.18 = //p' "$TOPDIR/target/linux/generic/kernel-6.18")
ARCH=aarch64_cortex-a55_neon-vfpv4

case "$OUTDIR" in
	"$TOPDIR"/bin/*|/tmp/mt6990-apk-repository-*) ;;
	*) echo "Refusing unsafe output path: $OUTDIR" >&2; exit 1;;
esac

[ "$KERNEL_VERSION" = .44 ] || {
	echo "Refusing repository build: expected Linux 6.18.44, got 6.18$KERNEL_VERSION" >&2
	exit 1
}
[ -f "$TOPDIR/public-key.pem" ] || {
	echo "Missing APK public key: $TOPDIR/public-key.pem" >&2
	exit 1
}

TARGET_SRC="$TOPDIR/bin/targets/mediatek/mt6990/packages"
BASE_SRC="$TOPDIR/bin/packages/$ARCH/base"
LUCI_SRC="$TOPDIR/bin/packages/$ARCH/luci"
PACKAGES_SRC="$TOPDIR/bin/packages/$ARCH/packages"

for source_dir in "$TARGET_SRC" "$BASE_SRC" "$LUCI_SRC" "$PACKAGES_SRC"; do
	[ -s "$source_dir/packages.adb" ] || {
		echo "Missing signed APK index: $source_dir/packages.adb" >&2
		exit 1
	}
done

rm -rf "$OUTDIR.new"
mkdir -p "$OUTDIR.new/target" "$OUTDIR.new/base" "$OUTDIR.new/luci" "$OUTDIR.new/packages"

copy_feed() {
	source_dir=$1
	dest_dir=$2
	find "$source_dir" -maxdepth 1 -type f \( -name '*.apk' -o -name 'packages.adb' \) \
		-exec cp -p '{}' "$dest_dir/" ';'
}

copy_feed "$TARGET_SRC" "$OUTDIR.new/target"
copy_feed "$BASE_SRC" "$OUTDIR.new/base"
copy_feed "$LUCI_SRC" "$OUTDIR.new/luci"
copy_feed "$PACKAGES_SRC" "$OUTDIR.new/packages"
cp -p "$TOPDIR/public-key.pem" "$OUTDIR.new/mt6990-apk-public-key.pem"

(
	cd "$OUTDIR.new"
	find target base luci packages -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
	TARGET_COUNT=$(find target -name '*.apk' | wc -l)
	BASE_COUNT=$(find base -name '*.apk' | wc -l)
	LUCI_COUNT=$(find luci -name '*.apk' | wc -l)
	PACKAGES_COUNT=$(find packages -name '*.apk' | wc -l)
	TOTAL_COUNT=$((TARGET_COUNT + BASE_COUNT + LUCI_COUNT + PACKAGES_COUNT))
	cat > repository.manifest <<EOF
board=lg6851f
target=mediatek/mt6990
kernel=6.18.44
architecture=$ARCH
openwrt_revision=r35183-10631d244c
target_packages=$TARGET_COUNT
base_packages=$BASE_COUNT
luci_packages=$LUCI_COUNT
packages_packages=$PACKAGES_COUNT
total_packages=$TOTAL_COUNT
public_key_sha256=$(sha256sum mt6990-apk-public-key.pem | awk '{print $1}')
generated_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
EOF
	cat > README.md <<'EOF'
# LG6851F / MT6990 dedicated APK repository

Packages in this branch are built with the LG6851F MT6990 firmware. They are
restricted to Linux 6.18.44 and `aarch64_cortex-a55_neon-vfpv4`.

Do not use these target/kernel packages on another OpenWrt device, and do not
add generic OpenWrt snapshot feeds to LG6851F/MT6990.
EOF
)

rm -rf "$OUTDIR"
mv "$OUTDIR.new" "$OUTDIR"
echo "MT6990_APK_REPOSITORY_READY=$OUTDIR"
cat "$OUTDIR/repository.manifest"
