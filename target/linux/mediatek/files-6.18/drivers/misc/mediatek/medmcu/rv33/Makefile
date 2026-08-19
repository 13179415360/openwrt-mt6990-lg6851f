obj-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu.o

medmcu-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu_feature_table.o medmcu_awake.o
#medmcu-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu_dvfs.o

medmcu-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu_helper.o
medmcu-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu_excep.o
medmcu-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu_irq.o
medmcu-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu_l1c.o
medmcu-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu_logger.o
medmcu-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu_scpctl.o
medmcu-$(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT) += medmcu_wrapper_ipi.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/medmcu/rv33
ccflags-y += -I$(srctree)/drivers/misc/mediatek/medmcu/include


# include emi_mpu.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dvfsrc/

# include mtk_spm_resource_req
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/

# include ccci_fsm.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc

#ifeq ($(CONFIG_MTK_ENG_BUILD),y)
CFLAGS_scp_excep.o       += -DDEBUG
CFLAGS_scp_helper.o      += -DDEBUG
CFLAGS_scp_irq.o         += -DDEBUG
CFLAGS_scp_logger.o      += -DDEBUG
CFLAGS_scp_scpctl.o      += -DDEBUG
CFLAGS_scp_wrapper_ipi.o += -DDEBUG
#endif
