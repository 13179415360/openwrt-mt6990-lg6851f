/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2017 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include "frame_engine.h"
#include "mcast_tbl.h"
#include "util.h"
#include "hnat_config.h"
#include "hnat_define.h"

int32_t mcast_entry_get(u16 vlan_id, uint8_t *dst_mac)
{
	int i;

	for (i = 0; i < MAX_MCAST_ENTRY; i++) {
		if ((GET_PPE_MCAST_H(i)->mc_vid == vlan_id) &&
		    GET_PPE_MCAST_L(i)->mc_mac_addr[3] == dst_mac[2] &&
			GET_PPE_MCAST_L(i)->mc_mac_addr[2] == dst_mac[3] &&
			GET_PPE_MCAST_L(i)->mc_mac_addr[1] == dst_mac[4] &&
			GET_PPE_MCAST_L(i)->mc_mac_addr[0] == dst_mac[5]) {
			if (GET_PPE_MCAST_H(i)->mc_mpre_sel == 0) {
				if (dst_mac[0] == 0x1 && dst_mac[1] == 0x00)
					return i;
			} else if (GET_PPE_MCAST_H(i)->mc_mpre_sel == 1) {
				if (dst_mac[0] == 0x33 && dst_mac[1] == 0x33)
					return i;
			} else {
				continue;
			}
		}
	}
	if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT)) {
		for (i = 0; i < MAX_MCAST_ENTRY16_63; i++) {
			if ((GET_PPE_MCAST_H10(i)->mc_vid == vlan_id) &&
			    GET_PPE_MCAST_L10(i)->mc_mac_addr[3] == dst_mac[2] &&
				GET_PPE_MCAST_L10(i)->mc_mac_addr[2] == dst_mac[3] &&
				GET_PPE_MCAST_L10(i)->mc_mac_addr[1] == dst_mac[4] &&
				GET_PPE_MCAST_L10(i)->mc_mac_addr[0] == dst_mac[5]) {
				if (GET_PPE_MCAST_H10(i)->mc_mpre_sel == 0) {
					if (dst_mac[0] == 0x1 && dst_mac[1] == 0x00)
						return (i + 16);
				} else if (GET_PPE_MCAST_H10(i)->mc_mpre_sel == 1) {
					if (dst_mac[0] == 0x33 && dst_mac[1] == 0x33)
						return (i + 16);
				} else {
					continue;
				}
			}
		}
	}
	return -1;
}

/*  mc_px_en: enable multicast to port x*/
/*  mc_px_qos_en: enable QoS for multicast to port x*/
/*  - multicast port0 map to PDMA*/
/*  - multicast port1 map to GMAC1*/
/*  - multicast port2 map to GMAC2*/
/*  - multicast port3 map to QDMA*/

int foe_mcast_entry_ins(u16 vlan_id, uint8_t *dst_mac, uint8_t mc_px_en,
			u8 mc_px_qos_en, uint8_t mc_qos_qid)
{
	int i = 0;
	int entry_num;
	struct ppe_mcast_h *mcast_h;
	struct ppe_mcast_l *mcast_l;

	pr_info("%s: vid=%x mac=%x:%x:%x:%x:%x:%x mc_px_en=%x mc_px_qos_en=%x, mc_qos_qid=%d\n",
		__func__, vlan_id, dst_mac[0], dst_mac[1], dst_mac[2], dst_mac[3], dst_mac[4],
		dst_mac[5], mc_px_en, mc_px_qos_en, mc_qos_qid);
	entry_num = mcast_entry_get(vlan_id, dst_mac);
    /* update exist entry */
	if (entry_num >= 0) {
		pr_info("update exist entry %d\n", entry_num);
		if (entry_num < 16) {
			mcast_h = GET_PPE_MCAST_H(entry_num);
			mcast_l = GET_PPE_MCAST_L(entry_num);

			if (dst_mac[0] == 0x1 && dst_mac[1] == 0x00)
				mcast_h->mc_mpre_sel = 0;
			else if (dst_mac[0] == 0x33 && dst_mac[1] == 0x33)
				mcast_h->mc_mpre_sel = 1;
			else
				return 0;

			mcast_h->mc_px_en = mc_px_en;
			mcast_h->mc_px_qos_en = mc_px_qos_en;
			/* mcast_h->mc_qos_qid = mc_qos_qid; */
			if (mc_qos_qid < 16) {
				mcast_h->mc_qos_qid = mc_qos_qid;
			} else if (mc_qos_qid > 15) {
				if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT)) {
					mcast_h->mc_qos_qid = mc_qos_qid & 0xf;
					mcast_h->mc_qos_qid54 = (mc_qos_qid & 0x30) >> 4;
				}
			}
			return 1;
		}
		if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT)) {
			if (entry_num >= 16) {
				mcast_h = GET_PPE_MCAST_H10(entry_num - 16);
				mcast_l = GET_PPE_MCAST_L10(entry_num - 16);

				if (dst_mac[0] == 0x1 && dst_mac[1] == 0x00)
					mcast_h->mc_mpre_sel = 0;
				else if (dst_mac[0] == 0x33 && dst_mac[1] == 0x33)
					mcast_h->mc_mpre_sel = 1;
				else
				    return 0;

				mcast_h->mc_px_en = mc_px_en;
				mcast_h->mc_px_qos_en = mc_px_qos_en;
				/* mcast_h->mc_qos_qid = mc_qos_qid; */
				if (mc_qos_qid < 16) {
					mcast_h->mc_qos_qid = mc_qos_qid;
				} else if (mc_qos_qid > 15) {
					mcast_h->mc_qos_qid = mc_qos_qid & 0xf;
					mcast_h->mc_qos_qid54 = (mc_qos_qid & 0x30) >> 4;
				}
			}
			return 1;
		}
	} else { /* create new entry */

		for (i = 0; i < MAX_MCAST_ENTRY; i++) {
			mcast_h = GET_PPE_MCAST_H(i);
			mcast_l = GET_PPE_MCAST_L(i);
			if (mcast_h->valid == 0) {
				if (dst_mac[0] == 0x1 && dst_mac[1] == 0x00)
					mcast_h->mc_mpre_sel = 0;
				else if (dst_mac[0] == 0x33 && dst_mac[1] == 0x33)
					mcast_h->mc_mpre_sel = 1;
				else
					return 0;

				mcast_h->mc_vid = vlan_id;
				mcast_h->mc_px_en = mc_px_en;
				mcast_h->mc_px_qos_en = mc_px_qos_en;
				mcast_l->mc_mac_addr[3] = dst_mac[2];
				mcast_l->mc_mac_addr[2] = dst_mac[3];
				mcast_l->mc_mac_addr[1] = dst_mac[4];
				mcast_l->mc_mac_addr[0] = dst_mac[5];
				mcast_h->valid = 1;
			/* mcast_h->mc_qos_qid = mc_qos_qid; */
				if (mc_qos_qid < 16) {
					mcast_h->mc_qos_qid = mc_qos_qid;
				} else if (mc_qos_qid > 15) {
					if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT)) {
						mcast_h->mc_qos_qid = mc_qos_qid & 0xf;
						mcast_h->mc_qos_qid54 = (mc_qos_qid & 0x30) >> 4;
					}
				}
				return 1;
			}
		}

		if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT)) {
			for (i = 0; i < MAX_MCAST_ENTRY16_63; i++) {
				mcast_h = GET_PPE_MCAST_H10(i);
				mcast_l = GET_PPE_MCAST_L10(i);

				if (mcast_h->valid == 0) {
					if (dst_mac[0] == 0x1 && dst_mac[1] == 0x00)
						mcast_h->mc_mpre_sel = 0;
					else if (dst_mac[0] == 0x33 && dst_mac[1] == 0x33)
						mcast_h->mc_mpre_sel = 1;
					else
						return 0;

					mcast_h->mc_vid = vlan_id;
					mcast_h->mc_px_en = mc_px_en;
					mcast_h->mc_px_qos_en = mc_px_qos_en;
					mcast_l->mc_mac_addr[3] = dst_mac[2];
					mcast_l->mc_mac_addr[2] = dst_mac[3];
					mcast_l->mc_mac_addr[1] = dst_mac[4];
					mcast_l->mc_mac_addr[0] = dst_mac[5];
					mcast_h->valid = 1;
					 /* mcast_h->mc_qos_qid = mc_qos_qid; */
					if (mc_qos_qid < 16) {
						mcast_h->mc_qos_qid = mc_qos_qid;
					} else if (mc_qos_qid > 15) {
						mcast_h->mc_qos_qid = mc_qos_qid & 0xf;
						mcast_h->mc_qos_qid54 = (mc_qos_qid & 0x30) >> 4;
					}
				    return 1;
			      }
			}
		}
	}
	MCAST_PRINT("HNAT: Multicast Table is FULL!!\n");
	return 0;
}

int foe_mcast_entry_qid(u16 vlan_id, uint8_t *dst_mac, uint8_t mc_qos_qid)
{
	int entry_num;
	struct ppe_mcast_h *mcast_h;

	if (debug_level >= 1)
		pr_info("%s: vid=%x mac=%x:%x:%x:%x:%x:%x mc_qos_qid=%d\n",
			__func__, vlan_id, dst_mac[0], dst_mac[1], dst_mac[2],
			dst_mac[3], dst_mac[4], dst_mac[5], mc_qos_qid);

	entry_num = mcast_entry_get(vlan_id, dst_mac);
	/* update exist entry */
	if (entry_num >= 0) {
		if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT)) {
			if (entry_num <= 15)
				mcast_h = GET_PPE_MCAST_H(entry_num);
			else
				mcast_h = GET_PPE_MCAST_H10(entry_num - 16);

					if (mc_qos_qid < 16) {
						/* mcast_h->mc_qos_qid = mc_qos_qid; */
					} else if (mc_qos_qid > 15) {
						/* mcast_h->mc_qos_qid = mc_qos_qid & 0xf; */
						/* mcast_h->mc_qos_qid54 = (mc_qos_qid & 0x30) >> 4; */
					} else {
						pr_info("Error qid = %d\n", mc_qos_qid);
						return 0;
					}
		} else {
				mcast_h = GET_PPE_MCAST_H(entry_num);
				mcast_h->mc_qos_qid = mc_qos_qid;
		}
		return 1;
	}

	return 0;
}

/* Return:*/
/*	    0: entry found*/
/*	    1: entry not found*/
int foe_mcast_entry_del(u16 vlan_id, uint8_t *dst_mac, uint8_t mc_px_en,
			u8 mc_px_qos_en, uint8_t mc_qos_qid)
{
	int entry_num;
	struct ppe_mcast_h *mcast_h;
	struct ppe_mcast_l *mcast_l;

	pr_info("%s: vid=%x mac=%x:%x:%x:%x:%x:%x mc_px_en=%x mc_px_qos_en=%x mc_qos_qid=%d\n",
		__func__, vlan_id, dst_mac[0], dst_mac[1], dst_mac[2], dst_mac[3], dst_mac[4],
		dst_mac[5], mc_px_en, mc_px_qos_en, mc_qos_qid);
	entry_num = mcast_entry_get(vlan_id, dst_mac);
	if (entry_num >= 0) {
		pr_info("entry_num = %d\n", entry_num);
		if (entry_num <= 15) {
			mcast_h = GET_PPE_MCAST_H(entry_num);
			mcast_l = GET_PPE_MCAST_L(entry_num);
			mcast_h->mc_px_en &= ~mc_px_en;
			mcast_h->mc_px_qos_en &= ~mc_px_qos_en;
			if (mcast_h->mc_px_en == 0 && mcast_h->mc_px_qos_en == 0) {
				mcast_h->valid = 0;
				mcast_h->mc_vid = 0;
				mcast_h->mc_qos_qid = 0;
				if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT))
					mcast_h->mc_qos_qid54 = 0;
				memset(&mcast_l->mc_mac_addr, 0, 4);
			}
		}
			if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT)) {
				if (entry_num > 15) {
					mcast_h = GET_PPE_MCAST_H10(entry_num - 16);
					mcast_l = GET_PPE_MCAST_L10(entry_num - 16);
					mcast_h->mc_px_en &= ~mc_px_en;
					mcast_h->mc_px_qos_en &= ~mc_px_qos_en;
					if (mcast_h->mc_px_en == 0 && mcast_h->mc_px_qos_en == 0) {
						mcast_h->valid = 0;
						mcast_h->mc_vid = 0;
						mcast_h->mc_qos_qid = 0;
						mcast_h->mc_qos_qid54 = 0;
						memset(&mcast_l->mc_mac_addr, 0, 4);
					}
				}
			}
		} else {
			pr_info("foe_mcast_entry_del fail: entry_number = %d\n", entry_num);
			return 1;
		}
	return 0;
}

void foe_mcast_entry_dump(void)
{
	int i;
	struct ppe_mcast_h *mcast_h;
	struct ppe_mcast_l *mcast_l;

	pr_info("MAC | VID | PortMask | QosPortMask\n");
	for (i = 0; i < MAX_MCAST_ENTRY; i++) {
		mcast_h = GET_PPE_MCAST_H(i);
		mcast_l = GET_PPE_MCAST_L(i);
		if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT)) {
			pr_info("%x:%x:%x:%x  %d  %c%c%c%c %c%c%c%c (QID=%d, mc_mpre_sel=%d)\n",
				mcast_l->mc_mac_addr[3],
				mcast_l->mc_mac_addr[2],
				mcast_l->mc_mac_addr[1],
				mcast_l->mc_mac_addr[0],
				mcast_h->mc_vid,
				(mcast_h->mc_px_en & 0x08) ? '1' : '-',
				(mcast_h->mc_px_en & 0x04) ? '1' : '-',
				(mcast_h->mc_px_en & 0x02) ? '1' : '-',
				(mcast_h->mc_px_en & 0x01) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x08) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x04) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x02) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x01) ? '1' : '-',
				mcast_h->mc_qos_qid + ((mcast_h->mc_qos_qid54) << 4),
				mcast_h->mc_mpre_sel);
		} else {
			pr_info("%x:%x:%x:%x  %d  %c%c%c%c %c%c%c%c (QID=%d, mc_mpre_sel=%d)\n",
				mcast_l->mc_mac_addr[3],
				mcast_l->mc_mac_addr[2],
				mcast_l->mc_mac_addr[1],
				mcast_l->mc_mac_addr[0],
				mcast_h->mc_vid,
				(mcast_h->mc_px_en & 0x08) ? '1' : '-',
				(mcast_h->mc_px_en & 0x04) ? '1' : '-',
				(mcast_h->mc_px_en & 0x02) ? '1' : '-',
				(mcast_h->mc_px_en & 0x01) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x08) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x04) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x02) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x01) ? '1' : '-',
				mcast_h->mc_qos_qid,
				mcast_h->mc_mpre_sel);
		}
	}
	if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT)) {
		for (i = 0; i < MAX_MCAST_ENTRY16_63; i++) {
			mcast_h = GET_PPE_MCAST_H10(i);
			mcast_l = GET_PPE_MCAST_L10(i);

			pr_info("%x:%x:%x:%x  %d  %c%c%c%c %c%c%c%c (QID=%d, mc_mpre_sel=%d)\n",
				mcast_l->mc_mac_addr[3],
				mcast_l->mc_mac_addr[2],
				mcast_l->mc_mac_addr[1],
				mcast_l->mc_mac_addr[0],
				mcast_h->mc_vid,
				(mcast_h->mc_px_en & 0x08) ? '1' : '-',
				(mcast_h->mc_px_en & 0x04) ? '1' : '-',
				(mcast_h->mc_px_en & 0x02) ? '1' : '-',
				(mcast_h->mc_px_en & 0x01) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x08) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x04) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x02) ? '1' : '-',
				(mcast_h->mc_px_qos_en & 0x01) ? '1' : '-',
				mcast_h->mc_qos_qid + ((mcast_h->mc_qos_qid54) << 4),
				mcast_h->mc_mpre_sel);
		}
	}
}

void foe_mcast_entry_del_all(void)
{
	int i;
	struct ppe_mcast_h *mcast_h;
	struct ppe_mcast_l *mcast_l;

	for (i = 0; i < MAX_MCAST_ENTRY; i++) {
		mcast_h = GET_PPE_MCAST_H(i);
		mcast_l = GET_PPE_MCAST_L(i);
		mcast_h->mc_px_en = 0;
		mcast_h->mc_px_qos_en = 0;
		mcast_h->valid = 0;
		mcast_h->mc_vid = 0;
		mcast_h->mc_qos_qid = 0;
		mcast_h->mc_mpre_sel = 0;
		memset(&mcast_l->mc_mac_addr, 0, 4);
	}
	if (hnat_chip_name & (MT7622_HWNAT | LEOPARD_HWNAT)) {
		for (i = 0; i < MAX_MCAST_ENTRY16_63; i++) {
			mcast_h = GET_PPE_MCAST_H10(i);
			mcast_l = GET_PPE_MCAST_L10(i);
			mcast_h->mc_px_en = 0;
			mcast_h->mc_px_qos_en = 0;
			mcast_h->valid = 0;
			mcast_h->mc_vid = 0;
			mcast_h->mc_qos_qid = 0;
			mcast_h->mc_mpre_sel = 0;
			memset(&mcast_l->mc_mac_addr, 0, 4);
		}
	}
}
