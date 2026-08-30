include $(TOPDIR)/rules.mk

PKG_NAME:=mt6990-hwnat
PKG_VERSION:=2023.05
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

define KernelPackage/mt6990-hwnat
  SUBMENU:=Network Devices
  TITLE:=MediaTek MT6990 vendor HW-NAT provider
  DEPENDS:=@TARGET_mediatek_mt6990 +kmod-nft-offload
  FILES:=$(PKG_BUILD_DIR)/hwnat.ko
  AUTOLOAD:=$(call AutoProbe,hwnat)
endef

define KernelPackage/mt6990-hwnat/description
 Complete MT6990 HNAT/PPE provider ported from the FG370 OpenSDK.  It binds
 the built-in MediaTek FOE hooks used by Ethernet, Wi-Fi and MED/CCMNI.
endef

define Build/Prepare
	$(RM) -r $(PKG_BUILD_DIR)
	$(CP) ./src/. $(PKG_BUILD_DIR)/
endef

define Build/Compile
	+$(KERNEL_MAKE) $(PKG_JOBS) \
		M="$(PKG_BUILD_DIR)" \
		CONFIG_MT6990_HWNAT=m \
		modules
endef

$(eval $(call KernelPackage,mt6990-hwnat))
