ccflags-y += -Idrivers/net/ethernet/raeth
ccflags-y += -Idrivers/misc/mediatek/medmcu/rv33
ccflags-y += -Idrivers/misc/mediatek/nat46_util
ccflags-y += -Werror -Wno-error=implicit-fallthrough
obj-$(CONFIG_RA_HW_NAT) += hw_nat.o

hw_nat-objs := ra_nat.o foe_fdb.o util.o hwnat_ioctl.o mcast_tbl.o
hw_nat-objs += hnat_dbg_proc.o hnat_common.o hnat_monitor.o hnat_fast.o hnat_power.o

ifeq ($(CONFIG_TUNNEL_FAST_PATH),y)
	hw_nat-objs += hnat_tnl.o hnat_l2tp.o hnat_gre.o
endif

ifeq ($(CONFIG_HW_NAT_SW_DVFS),y)
	hw_nat-objs += hnat_swdvfs.o
endif

hw_nat-objs += hnat_hwdvfs.o

ifeq ($(CONFIG_PINCTRL_MT7622),y)
	EXTRA_CFLAGS += -DCONFIG_ARCH_MT7622
endif

ifeq ($(CONFIG_RALINK),y)
	EXTRA_CFLAGS += -DCONFIG_RALINK_MT7621
endif

ifeq ($(CONFIG_SOC_MT7621),y)
        EXTRA_CFLAGS += -DCONFIG_RALINK_MT7621
endif
        EXTRA_CFLAGS += -DCONFIG_ARCH_COLGIN
