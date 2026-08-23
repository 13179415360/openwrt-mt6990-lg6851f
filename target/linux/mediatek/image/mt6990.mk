define Build/lg6851f-android-boot
	python3 $(TOPDIR)/scripts/lg6851f_mkbootimg_v2.py \
		--meta $(TOPDIR)/target/linux/mediatek/image/lg6851f-boot-v2.json \
		--kernel $(KDIR)/Image \
		--dtb $(KDIR)/image-mt6990-fiberhome-lg6851f.dtb \
		--out $@
	cp -f $@ $(BIN_DIR)/boot_b.img
endef

# SPDX-License-Identifier: GPL-2.0-only
#
# MediaTek MT6990 / T830 devices

define Build/lg6851f-copy-rootfs-shortname
	cp -f $@ $(BIN_DIR)/rootfs_b.img
endef

define Device/fiberhome_lg6851f
  DEVICE_VENDOR := FiberHome
  DEVICE_MODEL := Pro LG6851F
  SOC := mt6990
  KERNEL_LOADADDR := 0x80080000
  KERNEL_ENTRY := 0x80080000
  DEVICE_DTS := mt6990-fiberhome-lg6851f
  DEVICE_DTS_DIR := ../dts
	DEVICE_PACKAGES := kmod-mt7996e kmod-mt7990-firmware luci-app-5gmodem luci-app-pwmfan \
	mt6990-adb-usb \
	libf2fs mkf2fs f2fsck
  IMAGES += boot_b.img rootfs_b.img
  IMAGE/boot_b.img := lg6851f-android-boot
  IMAGE/rootfs_b.img := append-rootfs | lg6851f-copy-rootfs-shortname
endef
TARGET_DEVICES += fiberhome_lg6851f
