include $(TOPDIR)/rules.mk

PKG_NAME:=mt6990-userdata-storage
PKG_VERSION:=1
PKG_RELEASE:=2

PKG_MAINTAINER:=MT6990 LG6851F maintainers
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/mt6990-userdata-storage
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=Safe MT6990 user_data application storage
  DEPENDS:=+block-mount
endef

define Package/mt6990-userdata-storage/description
  Mounts the existing LG6851F user_data partition at /mnt/mt6990-data without
  changing GPT or A/B slots. It exposes /opt/mt6990 as the global dedicated
  area for large application payloads, persistent data, caches and logs.
endef

define Build/Compile
endef

define Package/mt6990-userdata-storage/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/usr/sbin
	$(INSTALL_BIN) ./files/mt6990-userdata-storage.init $(1)/etc/init.d/mt6990-userdata-storage
	$(INSTALL_BIN) ./files/mt6990-software-overlay $(1)/usr/sbin/mt6990-software-overlay
endef

$(eval $(call BuildPackage,mt6990-userdata-storage))
