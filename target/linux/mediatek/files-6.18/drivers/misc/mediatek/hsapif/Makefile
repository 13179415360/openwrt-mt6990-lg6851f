#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

#ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)
ifeq ($(CONFIG_MTK_HSAPIF_DRIVER)_$(CONFIG_MTK_PCIE_EP), y_y)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/hsapif/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hsapif/hif/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hsapif/port/inc/

obj-y := ccci_cldma_all.o

ccci_cldma_all-y += ccci_dvr_cldma.o


# sub modules
obj-y += pub/
obj-y += msg-center/
obj-y += hif/
obj-y += port/
obj-y += fsm/



else
obj-  := dummy.o # avoid build error
endif
