include $(TOPDIR)/rules.mk

PKG_NAME:=mihomo-openclash-core
PKG_VERSION:=1.19.29
PKG_RELEASE:=1
PKG_SOURCE:=mihomo-linux-arm64-v$(PKG_VERSION).gz
PKG_SOURCE_URL:=https://github.com/MetaCubeX/mihomo/releases/download/v$(PKG_VERSION)/
PKG_HASH:=9a868b5e4e0ad91d9d71e1b41b0cfce78aaba44360c30df74a723f8e3926a86c
PKG_UNPACK:=true

PKG_MAINTAINER:=MT6990 LG6851F maintainers
PKG_LICENSE:=GPL-3.0-only

include $(INCLUDE_DIR)/package.mk

define Package/mihomo-openclash-core
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=Web Servers/Proxies
  TITLE:=Pinned Mihomo core for MT6990 OpenClash
  URL:=https://github.com/MetaCubeX/mihomo
  DEPENDS:=+libc +mt6990-userdata-storage
endef

define Package/mihomo-openclash-core/description
  Verified ARM64 Mihomo core for MT6990. The compressed core is kept on
  persistent storage and expanded into tmpfs on demand to protect overlay space.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) $(DL_DIR)/$(PKG_SOURCE) $(PKG_BUILD_DIR)/mihomo.gz
endef

define Build/Compile
endef

define Package/mihomo-openclash-core/install
	$(INSTALL_DIR) $(1)/usr/lib/mihomo-openclash $(1)/etc/openclash/core
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/mihomo.gz $(1)/usr/lib/mihomo-openclash/mihomo.gz
	$(INSTALL_BIN) ./files/clash_meta $(1)/etc/openclash/core/clash_meta
endef

define Package/mihomo-openclash-core/postinst
#!/bin/sh
	[ -n "$${IPKG_INSTROOT}" ] && exit 0
	/etc/init.d/mt6990-userdata-storage enable
	/etc/init.d/mt6990-userdata-storage start || exit 1
	dst=/opt/mt6990/apps/mihomo
	mkdir -p "$${dst}"
	if [ ! -L /usr/lib/mihomo-openclash/mihomo.gz ]; then
		mv -f /usr/lib/mihomo-openclash/mihomo.gz "$${dst}/mihomo-v$(PKG_VERSION).gz"
		ln -sf "$${dst}/mihomo-v$(PKG_VERSION).gz" /usr/lib/mihomo-openclash/mihomo.gz
	fi
	exit 0
endef

define Package/mihomo-openclash-core/postrm
#!/bin/sh
	[ -n "$${IPKG_INSTROOT}" ] && exit 0
	rm -f /opt/mt6990/apps/mihomo/mihomo-v$(PKG_VERSION).gz
	exit 0
endef

$(eval $(call BuildPackage,mihomo-openclash-core))
