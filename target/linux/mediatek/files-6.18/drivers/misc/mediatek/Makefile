# MediaTek vendor modem driver stack for LG6851F / MT6990.
#
# Imported from vendor 5.15 gem6xxx tree.  This root Makefile only exposes
# subdirectories to the kernel build; per-driver Makefiles still decide
# objects from CONFIG_MTK_* symbols.
# LG6851F Stage5C: imported vendor sources warning policy
# Linux 6.18 enables prototype/declaration diagnostics globally.  The imported
# vendor 5.15 stack is not fully annotation-clean, but these warnings do not
# indicate ABI or runtime incompatibility.  Keep real compiler errors fatal.
subdir-ccflags-y += -Wno-error=missing-prototypes
subdir-ccflags-y += -Wno-error=missing-declarations


obj-$(CONFIG_MTK_ECCCI_DRIVER) += ccci_util/
obj-$(CONFIG_MTK_ECCCI_DRIVER) += eccci/
obj-$(CONFIG_MTK_ECCCI_DRIVER) += md_tx_power.o
obj-$(CONFIG_MTK_NET_CCMNI) += ccmni/
obj-$(CONFIG_MTK_HSAPIF_DRIVER) += hsapif/
obj-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu/
