#!/bin/sh
set -eu

OWRT=/home/zjl-pc/openwrt
SIGNER="$OWRT/scripts/lg6851f_sign_export_boot_rootfs.sh"
FINAL_DIR="$OWRT/boot和rootfs签名成功包"

cd "$OWRT"

echo "============================================================"
echo "LG6851F / MT6990 手动签名 boot/rootfs"
echo "============================================================"
echo "唯一永久输出目录：$FINAL_DIR"
echo

if command -v zenity >/dev/null 2>&1; then
	if ! zenity --question --title="LG6851F 手动签名" --width=620 \
		--text="开始签名最新编译的 boot/rootfs？\n\n唯一输出目录：\n$FINAL_DIR"; then
		echo "用户取消。"
		exit 0
	fi
else
	printf "确认开始？输入 y 回车继续："
	read -r ans
	case "$ans" in
		y|Y|yes|YES) ;;
		*) echo "用户取消。"; exit 0 ;;
	esac
fi

mkdir -p "$FINAL_DIR"

# 每次只留下这一轮固定名称的结果，避免新旧文件混淆。
rm -f \
	"$FINAL_DIR/boot_b.img" \
	"$FINAL_DIR/rootfs_b.img" \
	"$FINAL_DIR/SHA256SUMS.txt" \
	"$FINAL_DIR/lg6851f-signed-boot-rootfs.manifest.json"

"$SIGNER"

test -f "$FINAL_DIR/boot_b.img"
test -f "$FINAL_DIR/rootfs_b.img"
test "$(stat -c%s "$FINAL_DIR/boot_b.img")" = 33554432
test "$(stat -c%s "$FINAL_DIR/rootfs_b.img")" = 134217728

echo
echo "============================================================"
echo "签名成功；其他目录未导出签名成品"
echo "============================================================"
ls -lh "$FINAL_DIR/boot_b.img" "$FINAL_DIR/rootfs_b.img"
echo
cat "$FINAL_DIR/SHA256SUMS.txt"

if command -v zenity >/dev/null 2>&1; then
	zenity --info --title="LG6851F 签名完成" --width=620 \
		--text="签名成功。\n\n唯一输出目录：\n$FINAL_DIR\n\nboot_b.img: 32MiB\nrootfs_b.img: 128MiB" || true
fi

echo
echo "按回车关闭窗口..."
read -r dummy || true
