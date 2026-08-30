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

#ifndef _MCAST_TBL_WANTED
#define _MCAST_TBL_WANTED

struct ppe_mcast_h {
	uint32_t	mc_vid:12;
	uint32_t	mc_qos_qid54:2; /* mt7622 only */
	uint32_t	valid:1;
	uint32_t	rev1:1;
	uint32_t	mc_px_en:4;
	uint32_t	mc_mpre_sel:2; /* 0=01:00, 2=33:33 */
	uint32_t	mc_vid_cmp:1;
	uint32_t	rev2:1;
	uint32_t	mc_px_qos_en:4;
	uint32_t	mc_qos_qid:4;
};

struct ppe_mcast_l {
	u8	mc_mac_addr[4]; /* mc_mac_addr[31:0] */
};

/* DEFINITIONS AND MACROS*/
#define MAX_MCAST_ENTRY	    16
#define MAX_MCAST_ENTRY16_63    48
#define MAX_MCAST_ENTRY_TOTOAL  64
/* #define MCAST_DEBUG */
#ifdef MCAST_DEBUG
#define MCAST_PRINT(fmt, args...) pr_info(fmt, ## args)
#else
#define MCAST_PRINT(fmt, args...) { }
#endif

#define GET_PPE_MCAST_H(idx)		((struct ppe_mcast_h *)(PPE_MCAST_H_0 + ((idx) * 8)))
#define GET_PPE_MCAST_L(idx)		((struct ppe_mcast_l *)(PPE_MCAST_L_0 + ((idx) * 8)))

#define GET_PPE_MCAST_H10(idx)		((struct ppe_mcast_h *)(PPE_MCAST_H_10 + ((idx) * 8)))
#define GET_PPE_MCAST_L10(idx)		((struct ppe_mcast_l *)(PPE_MCAST_L_10 + ((idx) * 8)))

/* EXPORT FUNCTION*/
int foe_mcast_entry_ins(u16 vlan_id, u8 *dst_mac,
			u8 mc_px_en, u8 mc_px_qos_en, u8 mc_qos_qid);
int foe_mcast_entry_qid(u16 vlan_id, u8 *dst_mac, u8 mc_qos_qid);
int foe_mcast_entry_del(u16 vlan_id, u8 *dst_mac, u8 mc_px_en,
			u8 mc_px_qos_en, u8 mc_qos_qid);
void foe_mcast_entry_dump(void);
void foe_mcast_entry_del_all(void);

#endif
