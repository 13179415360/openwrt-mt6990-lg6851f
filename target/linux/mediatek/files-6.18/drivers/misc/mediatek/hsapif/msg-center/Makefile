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
ifeq ($(CONFIG_MTK_HSAPIF_DRIVER), y)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/hsapif/inc/


# msg center objects
#obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci_msg_center_all.o
obj-$(CONFIG_MTK_HSAPIF_DRIVER) := ccci_msg_center_all.o
ccci_msg_center_all-y += ccci_msg_center.o
ccci_msg_center_all-y += ccci_msg_data.o

else
obj-  := dummy.o # avoid build error
endif




