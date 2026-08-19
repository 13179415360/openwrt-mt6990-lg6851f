include $(TOPDIR)/rules.mk

PKG_NAME:=lg6851f-memtool
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/lg6851f-memtool
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=LG6851F physical memory diagnostic tool
endef

define Package/lg6851f-memtool/description
 Small /dev/mem mmap-based diagnostic utility for LG6851F MT6990 bring-up.
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-Wall -Wextra -Werror \
		-o $(PKG_BUILD_DIR)/lg6851f-memtool \
		$(PKG_BUILD_DIR)/lg6851f-memtool.c
endef

define Package/lg6851f-memtool/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lg6851f-memtool $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/etc/hotplug.d/iface
	$(INSTALL_BIN) ./files/99-mt6990-phy-led $(1)/etc/hotplug.d/iface/99-mt6990-phy-led
endef

$(eval $(call BuildPackage,lg6851f-memtool))
