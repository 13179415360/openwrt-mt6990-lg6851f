/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * This file is generated.
 * From 20220930_MT6980_MDReg_remap.xlsx
 * With ap_md_reg_dump_code_gentool.py v0.1
 * Date 2022-09-30 12:33:40.728566
 */

#ifndef __AP_MD_REG_DUMP_V1_H__
#define __AP_MD_REG_DUMP_V1_H__

/* dump_md_reg_io_remap, for internal dump*/
struct dump_reg_ioremap {
	void __iomem **dump_reg;
	unsigned long long addr;
	unsigned long size;
};

enum MD_REG_ID {
	MD_REG_MD_DBG_SYS_TIMEOUT_ADDR = 0,
	MD_REG_PC_MONITOR_ADDR,
	MD_REG_BUSMON_ADDR_0,
	MD_REG_BUSMON_ADDR_1,
	MD_REG_USIP_ADDR,
	MD_REG_SONIC_ADDR_0,
	MD_REG_SONIC_ADDR_1,
	MD_REG_SONIC_ADDR_2,
	MD_REG_SONIC_ADDR_3,
	MD_REG_SONIC_ADDR_4,
	MD_REG_SONIC_ADDR_5,
	MD_REG_SONIC_ADDR_6,
	MD_REG_SONIC_ADDR_7,
	MD_REG_SONIC_ADDR_8,
	MD_REG_SONIC_ADDR_9,
	MD_REG_SONIC_ADDR_10,
	MD_REG_SONIC_ADDR_11,
	MD_REG_SONIC_ADDR_12,
	MD_REG_SONIC_ADDR_13,
	MD_REG_SONIC_ADDR_14,
	MD_REG_SONIC_ADDR_15,
	MD_REG_SONIC_ADDR_16,
};

extern void md_io_remap_internal_dump_register(struct ccci_modem *md);
void internal_md_dump_debug_register_v1(unsigned int md_index);

#endif
