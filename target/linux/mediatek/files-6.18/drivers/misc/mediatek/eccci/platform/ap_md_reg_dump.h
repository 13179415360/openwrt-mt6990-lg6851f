// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/*
 * This file is generated.
 * From 20190903_Colgin_MDReg_remap.xlsx
 * With ap_md_reg_dump_code_gentool.py v0.1
 * Date 2020-07-20 13:01:05.290732
 */

#ifndef __AP_MD_REG_DUMP_H__
#define __AP_MD_REG_DUMP_H__

/* dump_md_reg_io_remap, for internal dump*/
struct dump_reg_ioremap {
	void __iomem **dump_reg;
	unsigned long long addr;
	unsigned long size;
};

enum MD_REG_ID {
	MD_REG_PC_MONITOR_ADDR = 0,
	MD_REG_BUSMON_ADDR_0,
	MD_REG_BUSMON_ADDR_1,
	MD_REG_USIP_ADDR_0,
	MD_REG_USIP_ADDR_1,
	MD_REG_USIP_ADDR_2,
};

void internal_md_dump_debug_register(unsigned int md_index);

#endif
