/* Copyright  2016 MediaTek Inc.
 * Author: Nelson Chang <nelson.chang@mediatek.com>
 * Author: Carlos Huang <carlos.huang@mediatek.com>
 * Author: Harry Huang <harry.huang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef HNAT_DBG_PROC_H
#define HNAT_DBG_PROC_H

#include <linux/ctype.h>
#include <linux/proc_fs.h>
#define HNAT_PROCREG_DIR             "hnat"
#define PROCREG_PPE_USAGE              	"hnat_usage"
#define PROCREG_CPU_REASON              "cpu_reason"
#define PROCREG_PPE_ENTRY               "hnat_entry"
#define PROCREG_PPE_SETTING             "hnat_setting"
#define PROCREG_PPE_MULTICAST		"hnat_multicast"
#define PROCREG_PPE_WHITELIST		"hnat_whitelist"
#define PROCREG_PPE_TYPE		"hnat_type"
#define PROCREG_PPE_QOS			"hnat_qos"
#define PROCREG_PPE_MIB			"hnat_mib"
#define PROCREG_PPE_MED			"hnat_med"
#define PROCREG_PPE_DMA			"hnat_dma"
#define PROCREG_PPE_DBG 		"hnat_dbg"
#define PROCREG_PPE_TUNNEL		"hnat_tunnel"

extern struct proc_dir_entry *hnat_proc_reg_dir;
extern unsigned int dbg_cpu_reason_cnt[32];
extern int hwnat_dbg_entry;

struct hwnat_interface {
	struct net_device *dev;
	unsigned long long rx_byte_cnt;
	unsigned long long rx_pkt_cnt;
	unsigned long long tx_byte_cnt;
	unsigned long long tx_pkt_cnt;
	unsigned long long rx_mcast_cnt;
};

struct hnat_mib_struct {
	unsigned long long byte_count;
	unsigned long long pkt_count;
};

int hnat_debug_proc_init(void);
void hnat_debug_proc_exit(void);
void dbg_dump_entry(uint32_t index, struct foe_entry *entry);
void dbg_dump_cr(struct seq_file *seq);

extern struct hwnat_interface hnat_if[64];
extern struct hnat_mib_struct hnat_entry_mib[FOE_4TB_SIZ];
extern struct hnat_mib_struct hnat_entry1_mib[FOE_4TB_SIZ];
#endif
