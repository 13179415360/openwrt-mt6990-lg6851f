#!/bin/sh
# Build OpenWrt and append only a compact artifact appendix to the repair journal.
# The repair journal's main entries are written as hypothesis/change/result/conclusion.

set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
record_file="$repo_dir/MT6990设备维修记录.md"
image_dir="$repo_dir/bin/targets/mediatek/mt6990"
signed_dir="$repo_dir/boot和rootfs签名成功包"
connsys_gnss="$repo_dir/target/linux/mediatek/mt6990/base-files/usr/lib/firmware/lg6851f/connsys_gnss_b.img"

[ -f "$connsys_gnss" ] || {
	printf 'ERROR: fixed MT6990 CONN_RO/GNSS recovery image is missing: %s\n' "$connsys_gnss" >&2
	exit 1
}
[ "$(stat -c '%s' "$connsys_gnss")" = 8388608 ] || {
	printf 'ERROR: fixed MT6990 CONN_RO/GNSS recovery image has wrong size.\n' >&2
	exit 1
}
[ "$(sha256sum "$connsys_gnss" | cut -d ' ' -f 1)" = 633a1e2a19593d9cf12db4e140d893da9d1817c721381848831d90afc0b02463 ] || {
	printf 'ERROR: fixed MT6990 CONN_RO/GNSS recovery image hash mismatch.\n' >&2
	exit 1
}

if [ "$#" -eq 0 ]; then
	set -- -j4 V=s
fi

printf '%s\n' "提醒：构建成功后仅追加精简产物信息；维修过程需按结构化主线记录。"
make -C "$repo_dir" "$@"

# A fresh LG6851F rootfs uses the unused tail of rootfs_b through a loop
# device as its persistent F2FS overlay.  Refuse to sign an image that would
# silently fall back to a volatile /tmp overlay on first boot.
if grep -qx 'CONFIG_TARGET_mediatek_mt6990_DEVICE_fiberhome_lg6851f=y' "$repo_dir/.config"; then
	rootfs_dir="$repo_dir/build_dir/target-aarch64_cortex-a55+neon-vfpv4_musl/root-mediatek"
	[ -x "$rootfs_dir/usr/sbin/mkfs.f2fs" ] || {
		printf 'ERROR: LG6851F image is missing /usr/sbin/mkfs.f2fs; refusing to sign.\n' >&2
		exit 1
	}
	[ -x "$rootfs_dir/usr/sbin/fsck.f2fs" ] || {
		printf 'ERROR: LG6851F image is missing /usr/sbin/fsck.f2fs; refusing to sign.\n' >&2
		exit 1
	}
fi

# The normal OpenWrt world target is the canonical LG6851F build entry point.
# It already signs and packages the final images.  This optional maintenance
# wrapper only verifies their presence and appends hashes to the repair log.
if grep -qx 'CONFIG_TARGET_mediatek_mt6990=y' "$repo_dir/.config" &&
	grep -qx 'CONFIG_TARGET_mediatek_mt6990_DEVICE_fiberhome_lg6851f=y' "$repo_dir/.config"; then
	[ -f "$signed_dir/boot_b.img" ] || {
		printf 'ERROR: standard make did not produce signed boot_b.img\n' >&2
		exit 1
	}
	[ -f "$signed_dir/rootfs_b.img" ] || {
		printf 'ERROR: standard make did not produce signed rootfs_b.img\n' >&2
		exit 1
	}
	[ -f "$image_dir/lg6851f-mt6990-b-signed-sysupgrade.tar.gz" ] || {
		printf 'ERROR: standard make did not produce signed Web sysupgrade package\n' >&2
		exit 1
	}
else
	printf '%s\n' "非 MT6990/LG6851F 配置，跳过设备产物验证。"
fi

timestamp=$(date '+%Y-%m-%d %H:%M:%S %Z')
{
	printf '\n#### %s — 构建产物（维修记录附属）\n\n' "$timestamp"
	printf -- '- 命令：`make'
	for arg in "$@"; do
		printf ' %s' "$arg"
	done
	printf '`\n- 结果：成功（退出码 0）\n'

	for image in boot_b.img rootfs_b.img; do
		image_path="$image_dir/$image"
		if [ -f "$image_path" ]; then
			size=$(stat -c '%s' "$image_path")
			hash=$(sha256sum "$image_path" | cut -d ' ' -f 1)
			mtime=$(date -r "$image_path" '+%Y-%m-%d %H:%M:%S %Z')
			printf -- '- `%s`: 时间 %s，大小 %s，SHA256 `%s`\n' \
				"bin/targets/mediatek/mt6990/$image" "$mtime" "$size" "$hash"
		fi
	done

	if [ -f "$signed_dir/boot_b.img" ] && [ -f "$signed_dir/rootfs_b.img" ]; then
		printf -- '- 自动签名：成功，固定目录 `%s`\n' "boot和rootfs签名成功包/"
		for image in boot_b.img rootfs_b.img; do
			image_path="$signed_dir/$image"
			size=$(stat -c '%s' "$image_path")
			hash=$(sha256sum "$image_path" | cut -d ' ' -f 1)
			printf -- '- 签名成品 `%s`: 大小 %s，SHA256 `%s`\n' \
				"boot和rootfs签名成功包/$image" "$size" "$hash"
		done
	fi

	printf '\n'
} >> "$record_file"

chmod 600 "$record_file"
printf '%s\n' "已更新：$record_file"
