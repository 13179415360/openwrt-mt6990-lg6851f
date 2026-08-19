include $(TOPDIR)/rules.mk

PKG_NAME:=lg6851f-modem-chain-only
PKG_VERSION:=0.6
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/lg6851f-modem-chain-only
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=LG6851F 5G modem chain only userspace/materials
  DEPENDS:=+libstdcpp +libgcc +libuci +libubox +libubus +libjson-c +libcurl +alsa-lib +jshn
endef

define Package/lg6851f-modem-chain-only/description
 5G modem chain only. WiFi7 is handled by OpenWrt mainline and no vendor WiFi material is packaged.
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-o $(PKG_BUILD_DIR)/modem5g-at ./src/modem5g-at.c
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-L./files/usr/lib -Wl,-rpath-link,./files/usr/lib \
		-o $(PKG_BUILD_DIR)/modem5g-sms-send ./src/modem5g-sms-send.c \
		-lqlril -lql_uinf
endef

define Package/lg6851f-modem-chain-only/install
	$(CP) ./files/* $(1)/
	# The official sample_nw binary is only reachable through the validated,
	# user-triggered modem-lock wrapper.  Keep the old polling wrapper absent.
	rm -f $(1)/usr/bin/lg6851f-modem-band
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/modem5g-at $(1)/usr/bin/modem5g-at
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/modem5g-sms-send $(1)/usr/bin/modem5g-sms-send
	# Keep the image build self-contained; company delivery must not depend on
	# the original developer workstation's sibling SDK checkout.
	$(INSTALL_DATA) ./files/etc/default_apn.json $(1)/etc/default_apn.json
	# The vendor SDK libraries use libubox symbols but were linked without a
	# DT_NEEDED entry.  Repair the ELF dependency graph at image build time so
	# musl loads the matching SDK ABI normally; do not rely on LD_PRELOAD.
	$(STAGING_DIR_HOST)/bin/patchelf --remove-rpath $(1)/usr/lib/libuci.so
	$(STAGING_DIR_HOST)/bin/patchelf --add-needed libubox.so.20210516 $(1)/usr/lib/libuci.so
	$(STAGING_DIR_HOST)/bin/patchelf --add-needed libubox.so.20210516 $(1)/usr/lib/libnvram.so
endef

$(eval $(call BuildPackage,lg6851f-modem-chain-only))
