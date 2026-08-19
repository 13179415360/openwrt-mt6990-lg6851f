#
# Copyright (C) 2019 MediaTek Inc.
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
ifeq ($(CONFIG_MTK_HSAPIF_DRIVER), y)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hsapif/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hsapif/port/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hsapif/hif/

obj-y := ccci_pub_all.o

ccci_pub_all-y += ccci_private_log.o
#ccci_pub_all-y += ccci_bm.o
ccci_pub_all-y += ccci_list.o
ccci_pub_all-y += ccci_kmem.o
ccci_pub_all-y += ccci_state_mgr.o
ccci_pub_all-y += cldma_util_broadcast.o

else
obj-  := dummy.o # avoid build error



endif
