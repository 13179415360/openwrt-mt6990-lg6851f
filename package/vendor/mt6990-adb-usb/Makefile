include $(TOPDIR)/rules.mk

PKG_NAME:=mt6990-adb-usb
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/mt6990-adb-usb
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=MT6990 dedicated delayed ADB USB gadget
  DEPENDS:=@TARGET_mediatek_mt6990 +kmod-usb-mtu3 +kmod-usb-gadget-fs
endef

define Package/mt6990-adb-usb/description
 Dedicated MT6990 FunctionFS ADB gadget.  It exposes no USB networking
 functions and waits 60 seconds after late boot before binding the UDC.
endef

define Build/Compile
endef

define Package/mt6990-adb-usb/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/usr/sbin $(1)/usr/libexec/mt6990-adb
	$(INSTALL_BIN) ./files/etc/init.d/mt6990-adb-usb $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/usr/sbin/mt6990-adb-usb $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/libexec/mt6990-adb/adbd_usb $(1)/usr/libexec/mt6990-adb/
	$(INSTALL_DATA) ./files/usr/libexec/mt6990-adb/libubox.so.20210516 $(1)/usr/libexec/mt6990-adb/
	$(INSTALL_DATA) ./files/usr/libexec/mt6990-adb/libuci.so $(1)/usr/libexec/mt6990-adb/
endef

$(eval $(call BuildPackage,mt6990-adb-usb))
