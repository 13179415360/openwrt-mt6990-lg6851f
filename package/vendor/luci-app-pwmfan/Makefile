include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI MT6990 LG6851F PWM fan status card
LUCI_DESCRIPTION:=Monitor and control the MT6990 LG6851F PWM cooling fan
LUCI_DEPENDS:=@TARGET_mediatek_mt6990 +luci-base +luci-mod-status +rpcd-mod-ucode
LUCI_PKGARCH:=all

PKG_LICENSE:=Apache-2.0
PKG_MAINTAINER:=OpenWrt MT6990 LG6851F integration

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
