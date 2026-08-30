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

#ifndef _FOE_FDB_WANTED
#define _FOE_FDB_WANTED

#include <net/ip.h>
#include "hnat_ioctl.h"
/* #include "frame_engine.h" */
#include "ra_nat.h"

#define FOE_PPE_SIZE		2
/* DEFINITIONS AND MACROS*/
#define FOE_ENTRY_LIFE_TIME	5
#define FOE_THRESHOLD		1000
#define FOE_HASH_MASK		0x00001FFF
#define FOE_HASH_WAY		2
#define FOE_1K_SIZ_MASK		0x000001FF
#define FOE_2K_SIZ_MASK		0x000003FF
#define FOE_4K_SIZ_MASK		0x000007FF
#define FOE_8K_SIZ_MASK		0x00000FFF
#define FOE_16K_SIZ_MASK	0x00001FFF

#if defined(CONFIG_RA_HW_NAT_TBL_1K)
#define FOE_4TB_SIZ		1024
#define FOE_4TB_BIT		10
#elif defined(CONFIG_RA_HW_NAT_TBL_2K)
#define FOE_4TB_SIZ		2048
#define FOE_4TB_BIT             11
#elif defined(CONFIG_RA_HW_NAT_TBL_4K)
#define FOE_4TB_SIZ		4096
#define FOE_4TB_BIT             12
#elif defined(CONFIG_RA_HW_NAT_TBL_8K)
#define FOE_4TB_SIZ		8192
#define FOE_4TB_BIT             13
#elif defined(CONFIG_RA_HW_NAT_TBL_16K)
#define FOE_4TB_SIZ		16384
#define FOE_4TB_BIT             14
#elif defined(CONFIG_RA_HW_NAT_TBL_32K)
#define FOE_4TB_SIZ		32768
#define FOE_4TB_BIT             15
#endif

#define FOE_ENTRY_SIZ		128	/* for ipv6 backward compatible */

#define IP_FORMAT3(addr) (((unsigned char *)&addr)[3])
#define IP_FORMAT2(addr) (((unsigned char *)&addr)[2])
#define IP_FORMAT1(addr) (((unsigned char *)&addr)[1])
#define IP_FORMAT0(addr) (((unsigned char *)&addr)[0])

struct pkt_parse_result {
	/*layer2 header */
	u8 dmac[6];
	u8 smac[6];

	/*vlan header */
	u16 vlan_tag;
	u16 vlan1_gap;
	u16 vlan1;
	u16 vlan2_gap;
	u16 vlan2;
	u16 vlan_layer;

	/*pppoe header */
	u32 pppoe_gap;
	u16 ppp_tag;
	u16 pppoe_sid;

	/*layer3 header */
	u16 eth_type;
	struct iphdr iph;
	struct ipv6hdr ip6h;

	/*layer4 header */
	struct tcphdr th;
	struct udphdr uh;

	u32 pkt_type;
	u16 gre_call_id;
	u8 is_mcast;

};

struct pkt_rx_parse_result {
	/*layer2 header */
	u8 dmac[6];
	u8 smac[6];

	/*vlan header */
	u16 vlan_tag;
	u16 vlan1_gap;
	u16 vlan1;
	u16 vlan2_gap;
	u16 vlan2;
	u16 vlan_layer;

	/*pppoe header */
	u32 pppoe_gap;
	u16 ppp_tag;
	u16 pppoe_sid;

	/*layer3 header */
	u16 eth_type;
	struct iphdr iph;
	struct ipv6hdr ip6h;

	/*layer4 header */
	struct tcphdr th;
	struct udphdr uh;

	u32 pkt_type;
	u8 is_mcast;

};

 /* TYPEDEFS AND STRUCTURES*/
enum FOE_TBL_SIZE {
	FOE_TBL_SIZE_1K,
	FOE_TBL_SIZE_2K,
	FOE_TBL_SIZE_4K,
	FOE_TBL_SIZE_8K,
	FOE_TBL_SIZE_16K,
	FOE_TBL_SIZE_32K
};

enum VLAN_ACTION {
	NO_ACT = 0,
	MODIFY = 1,
	INSERT = 2,
	DELETE = 3
};

enum FOE_ENTRY_STATE {
	INVALID = 0,
	UNBIND = 1,
	BIND = 2,
	FIN = 3
};

enum FOE_TBL_TCP_UDP {
	TCP = 0,
	UDP = 1,
	ANY = 2
};

enum FOE_TBL_EE {
	NOT_ENTRY_END = 0,
	ENTRY_END_FP = 1,
	ENTRY_END_FOE = 2
};

enum FOE_LINK_TYPE {
	LINK_TO_FOE = 0,
	LINK_TO_FP = 1
};

enum FOE_IP_ACT {
	IPV4_HNAPT = 0,
	IPV4_HNAT = 1,
	IPV6_1T_ROUTE = 2,
	IPV4_DSLITE = 3,
	IPV6_3T_ROUTE = 4,
	IPV6_5T_ROUTE = 5,
	IPV6_6RD = 7,
	IPV4_MAP_T = 8,
#if defined(CONFIG_HNAT_V2)
	IPV4_MAP_E = 9,
#endif
#if defined(CONFIG_HNAT_V1)
	IPV4_MAP_E = 3,
#endif
};

enum FOE_ENTRY_FMT {
	IPV4_NAPT = 0,
	IPV4_NAT = 1,
	IPV6_ROUTING = 5
};

#define IS_IPV4_HNAPT(x)	(((x)->bfib1.pkt_type == IPV4_HNAPT) ? 1 : 0)
#define IS_IPV4_HNAT(x)		(((x)->bfib1.pkt_type == IPV4_HNAT) ? 1 : 0)
#define IS_IPV6_1T_ROUTE(x)	(((x)->bfib1.pkt_type == IPV6_1T_ROUTE) ? 1 : 0)
#define IS_IPV4_DSLITE(x)	(((x)->bfib1.pkt_type == IPV4_DSLITE) ? 1 : 0)
#define IS_IPV4_MAPE(x)		(((x)->bfib1.pkt_type == IPV4_MAP_E) ? 1 : 0)
#define IS_IPV4_MAPT(x)		(((x)->bfib1.pkt_type == IPV4_MAP_T) ? 1 : 0)
#define IS_IPV6_3T_ROUTE(x)	(((x)->bfib1.pkt_type == IPV6_3T_ROUTE) ? 1 : 0)
#define IS_IPV6_5T_ROUTE(x)	(((x)->bfib1.pkt_type == IPV6_5T_ROUTE) ? 1 : 0)
#define IS_IPV6_6RD(x)		(((x)->bfib1.pkt_type == IPV6_6RD) ? 1 : 0)
#define IS_IPV4_GRP(x)		(IS_IPV4_HNAPT(x) | IS_IPV4_HNAT(x))
#define IS_IPV6_GRP(x) \
	(IS_IPV6_1T_ROUTE(x) | IS_IPV6_3T_ROUTE(x) | IS_IPV6_5T_ROUTE(x) | IS_IPV6_6RD(x) | \
	 IS_IPV4_DSLITE(x) | IS_IPV4_MAPE(x))
/************************************************************/

struct MED_HOST_INFO1_T {
	unsigned int PPE_ENTRY:15;
	unsigned int HOST:1;
	unsigned int CRSN:5;
	unsigned int LAST:1;
	unsigned int PIT_CNT:5;
	unsigned int PIT_IDX_L:5;
};

struct MED_HOST_INFO2_T {
	unsigned int HOST_CNT:6;
	unsigned int RSV:10;
	unsigned int BID:16;
};

//MED_HNAT_INFO struct
struct MED_HNAT_INFO_HOST {
	struct MED_HOST_INFO1_T dmad_info1;
	struct MED_HOST_INFO2_T dmad_info2;
};


struct MDMA_RXDMAD_INFO1{
	unsigned int PDP0;
};

struct MDMA_RXDMAD_INFO2 {
	unsigned int RSV0:6;
	unsigned int RSV1:2;
	unsigned int PLEN0:16;
	unsigned int RSV2:6;
	unsigned int LS0:1;
	unsigned int DDONE_bit:1;
};

struct MDMA_RXDMAD_INFO3 {
	unsigned int TAG:1;
	unsigned int L4F:1;
	unsigned int L4VLD:1;
	unsigned int TACK:1;
	unsigned int IP4F:1;
	unsigned int IP4:1;
	unsigned int IP6:1;
	unsigned int RSV0:5;
	unsigned int RSV1:20;
};

struct MDMA_RXDMAD_INFO4 {
	unsigned int VID:16;
	unsigned int VPID:16;
};

struct MDMA_RXDMAD_INFO5 {
	unsigned int FOE_ENTRY:15;
	unsigned int RSV0:3;
	unsigned int CRSN:5;
	unsigned int RSV1:3;
	unsigned int SP:4;
	unsigned int RSV2:2;
};

struct MDMA_RXDMAD_INFO6 {
	unsigned int LRO_FLUSH_RSN:3;
	unsigned int RSV0:2;
	unsigned int RSV1:11;
	unsigned int LRO_AGG_CNT:8;
	unsigned int RSV2:2;
	unsigned int RSV3:6;
};

struct MDMA_RXDMAD_INFO7 {
	unsigned int RSV;
};

struct MDMA_RXDMAD_INFO8 {
	unsigned int RSV;
};

struct MDMA_rxdmad {
	struct MDMA_RXDMAD_INFO1 rxd_info1;
	struct MDMA_RXDMAD_INFO2 rxd_info2;
	struct MDMA_RXDMAD_INFO3 rxd_info3;
	struct MDMA_RXDMAD_INFO4 rxd_info4;
	struct MDMA_RXDMAD_INFO5 rxd_info5;
	struct MDMA_RXDMAD_INFO6 rxd_info6;
	struct MDMA_RXDMAD_INFO7 rxd_info7;
	struct MDMA_RXDMAD_INFO8 rxd_info8;
};

/*=========================================
 *    PDMA TX Descriptor Format define
 *=========================================
 */

struct MDMA_TXDMAD_INFO1 {
	unsigned int SDP0;
};

struct MDMA_TXDMAD_INFO2 {
	unsigned int RSV0:6;
	unsigned int RSV1:2;
	unsigned int SDL0:16;
	unsigned int RSV2:6;
	unsigned int LS0:1;
	unsigned int DDONE:1;
};

struct MDMA_TXDMAD_INFO3 {
    unsigned int SDP1;
};

struct MDMA_TXDMAD_INFO4 {
	unsigned int RSV0:6;
	unsigned int RSV1:2;
	unsigned int SDL1:16;
	unsigned int RSV2:6;
	unsigned int LS1:1;
	unsigned int BURST:1;
};

struct MDMA_TXDMAD_INFO5 {
	unsigned int RSV0:16;
	unsigned int FPORT:4;
	unsigned int RSV1:2;
	unsigned int RSV2:6;
	unsigned int TUI_CO:3;
	unsigned int TSO:1;
};

struct MDMA_TXDMAD_INFO6 {
	unsigned int PID_IDX_LSB:5;
	unsigned int RSV0:3;
	unsigned int PID_CNT:5;
	unsigned int RSV1:3;
	unsigned int TO_AP:1;
	unsigned int RSV2:15;
};

struct MDMA_TXDMAD_INFO7 {
	unsigned int FRAG_PID:16;
	unsigned int PID:16;
};

struct MDMA_TXDMAD_INFO8 {
	unsigned int RSV;
};


struct MDMA_txdmad {
	struct MDMA_TXDMAD_INFO1 txd_info1;
	struct MDMA_TXDMAD_INFO2 txd_info2;
	struct MDMA_TXDMAD_INFO3 txd_info3;
	struct MDMA_TXDMAD_INFO4 txd_info4;
	struct MDMA_TXDMAD_INFO5 txd_info5;
	struct MDMA_TXDMAD_INFO6 txd_info6;
	struct MDMA_TXDMAD_INFO7 txd_info7;
	struct MDMA_TXDMAD_INFO8 txd_info8;
};

#if defined(CONFIG_HNAT_V2)
struct ud_info_blk1 {
	uint32_t time_stamp:8;
	uint32_t sp:4;
	uint32_t pcnt:8;
	uint32_t ilgf:1;
	uint32_t mc:1;
	uint32_t preb:1;
	uint32_t pkt_type:5;
	uint32_t state:2;
	uint32_t udp:1;
	uint32_t stc:1;		/* static entry */
};

/* state = bind & fin */
struct bf_info_blk1 {
	uint32_t time_stamp:8;
	uint32_t sp:4;
	uint32_t mc:1;
	uint32_t ka:1;/* keep alive */
	uint32_t vlan_layer:3;
	uint32_t psn:1;/* egress packet has PPPoE session */
	uint32_t vpm:1;/* 0:ethertype remark, 1:0x8100(CR default) */
	uint32_t ps:1;/* packet sampling */
	uint32_t cah:1;/* cacheable flag */
	uint32_t rmt:1;/* remove tunnel ip header (6rd/dslite only) */
	uint32_t ttl:1;
	uint32_t pkt_type:5;
	uint32_t state:2;
	uint32_t udp:1;
	uint32_t stc:1;		/* static entry */
};

/* state = bind & fin */
struct _info_blk2 {
	uint32_t qid:7;		/* QID in Qos Port */
	uint32_t rsv:1;
	uint32_t fqos:1;	/* force to PSE QoS port */
	uint32_t dp:4;		/* force to PSE port x */
	uint32_t mcast:1;	/* multicast this packet to CPU */
	uint32_t pcpl:1;	/* OSBN */
	uint32_t mibf:1;
	uint32_t alen:1;
	uint32_t dr_idx:2;
	uint32_t fdr_en:1;
	uint32_t acnt:4;
	uint32_t dscp:8;	/* DSCP value */
};
#endif /* CONFIG_HNAT_V2 */
#if defined(CONFIG_HNAT_V1)
/* state = unbind & dynamic */
struct ud_info_blk1 {
	uint32_t time_stamp:8;
	uint32_t pcnt:14;
	uint32_t ilgf:1;
	uint32_t mc:1;
	uint32_t preb:1;
	uint32_t pkt_type:3;
	uint32_t state:2;
	uint32_t udp:1;
	uint32_t stc:1;		/* static entry */
};

/* state = bind & fin */
struct bf_info_blk1 {
	uint32_t time_stamp:14;
	uint32_t mc:1;
	uint32_t ka:1;		/* keep alive */
	uint32_t vlan_layer:3;
	uint32_t psn:1;		/* egress packet has PPPoE session */

	uint32_t vpm:1;		/* 0:ethertype remark, 1:0x8100(CR default) */
	uint32_t ps:1;		/* packet sampling */
	uint32_t cah:1;		/* cacheable flag */
	uint32_t rmt:1;		/* remove tunnel ip header (6rd/dslite only) */
	uint32_t ttl:1;
	uint32_t pkt_type:3;
	uint32_t state:2;
	uint32_t udp:1;
	uint32_t stc:1;		/* static entry */
};

/* state = bind & fin */
struct _info_blk2 {
	uint32_t qid:4;		/* QID in Qos Port */
	uint32_t fqos:1;	/* force to PSE QoS port */
	uint32_t dp:3;		/* force to PSE port x */
				/*0:PSE,1:GSW, 2:GMAC,4:PPE,5:QDMA,7=DROP */
	uint32_t mcast:1;	/* multicast this packet to CPU */
	uint32_t pcpl:1;	/* OSBN */
	uint32_t mibf:1;
	uint32_t alen:1;
	uint32_t qid1:2;
	uint32_t dp1:1;
	uint32_t dr_idx:2;
	uint32_t fdr_en:1;
	uint32_t acnt:6;
	uint32_t dscp:8;	/* DSCP value */
};
#endif /* CONFIG_HNAT_V1 */
/* Foe Entry (64B) */
/*      IPV4:			     IPV6: */
/*	+-----------------------+    +-----------------------+ */
/*	|  Information Block 1  |    |  Information Block 1  | */
/*	+-----------------------+    +-----------------------+ */
/*	|	SIP(4B)		|    |     IPv6_DIP0(4B)     | */
/*	+-----------------------+    +-----------------------+ */
/*	|	DIP(4B)		|    |     IPv6_DIP1(4B)     | */
/*	+-----------------------+    +-----------------------+ */
/*	| SPORT(2B) | DPORT(2B) |    |	      Rev(4B)        | */
/*	+-----------+-----------+    +-----------------------+ */
/*	| Information Block 2   |    |	Information Block 2  | */
/*	+-----------------------+    +-----------------------+ */
/*	|      New SIP(4B)	|    |     IPv6_DIP2(4B)     | */
/*	+-----------------------+    +-----------------------+ */
/*	|      New DIP(4B)	|    |     IPv6_DIP3(4B)     | */
/*	+-----------------------+    +-----------------------+ */
/*	| New SPORT | New DPORT |    |	       Rev(4B)       | */
/*	+-----------+-----------+    +-----------------------+ */
/*	| VLAN1(2B) |DMAC[47:32]|    | VLAN1(2B) |DMAC[47:32]| */
/*	+-----------|-----------+    +-----------|-----------+ */
/*	|	DMAC[31:0]      |    |       DMAC[31:0]      | */
/*	+-----------------------+    +-----------------------+ */
/*	| PPPoE_ID  |SMAC[47:32]|    | PPPoE_ID  |SMAC[47:32]| */
/*	+-----------+-----------+    +-----------+-----------+ */
/*	|       SMAC[31:0]      |    |       SMAC[31:0]      | */
/*	+-----------------------+    +-----------------------+ */
/*	| Rev |  SNAP_Ctrl(3B)  |    | Rev |  SNAP_Ctrl(3B)  | */
/*	+-----------------------+    +-----------------------+ */
/*	|    Rev    | VLAN2(2B) |    |   Rev     | VLAN2(2B) | */
/*	+-----------------------+    +-----------------------+ */
/*	|     Rev(4B)           |    |       Rev(4B)         | */
/*	+-----------------------+    +-----------------------+ */
/*	|     tmp_buf(4B)       |    |       tmp_buf(4B)     | */
/*	+-----------------------+    +-----------------------+ */
/* Foe Entry (80) */
/* */
/*      IPV4 HNAPT:			     IPV4: */
/*	+-----------------------+    +-----------------------+ */
/*	|  Information Block 1  |    |  Information Block 1  | */
/*	+-----------------------+    +-----------------------+ */
/*	|	SIP(4B)		|    |		SIP(4B)      | */
/*	+-----------------------+    +-----------------------+ */
/*	|	DIP(4B)		|    |		DIP(4B)      | */
/*	+-----------------------+    +-----------------------+ */
/*	| SPORT(2B) | DPORT(2B) |    |	      Rev(4B)        | */
/*	+-----------+-----------+    +-----------------------+ */
/*	| EG DSCP| Info Block 2 |    |	Information Block 2  | */
/*	+-----------------------+    +-----------------------+ */
/*	|      New SIP(4B)	|    |     New SIP (4B)      | */
/*	+-----------------------+    +-----------------------+ */
/*	|      New DIP(4B)	|    |     New DIP (4B)      | */
/*	+-----------------------+    +-----------------------+ */
/*	| New SPORT | New DPORT |    | New SPORT | New DPORT | */
/*	+-----------+-----------+    +-----------------------+ */
/*	|          REV          |    |		REV	     | */
/*	+-----------------------+    +-----------------------+ */
/*	|Act_dp|mode|    REV    |    |Act_dp|mode|   REV     | */
/*	+-----------------------+    +-----------------------+ */
/*	|      tmp_buf(4B)      |    |	     temp_buf(4B)    | */
/*	+-----------------------+    +-----------|-----------+ */
/*	| ETYPE     | VLAN1 ID  |    | ETYPE     |  VLAN1    | */
/*	+-----------+-----------+    +-----------+-----------+ */
/*	|       DMAC[47:16]     |    |       SMAC[47:16]     | */
/*	+-----------------------+    +-----------------------+ */
/*	| DMAC[15:0]| VLAN2 ID  |    | DMAC[15:0]|  VLAN2    | */
/*	+-----------------------+    +-----------------------+ */
/*	|       SMAC[47:16]     |    |       SMAC[47:16]     | */
/*	+-----------------------+    +-----------------------+ */
/*	| SMAC[15:0]| PPPOE ID  |    | SMAC[15:0]| PPPOE ID  | */
/*	+-----------------------+    +-----------------------+ */
/*								 */
struct _ipv4_hnapt {
	union {
		struct ud_info_blk1 udib1;
		struct bf_info_blk1 bfib1;
		u32 info_blk1;
	};
	u32 sip;
	u32 dip;
	u16 dport;
	u16 sport;
	union {
	struct _info_blk2 iblk2;
		u32 info_blk2;
	};
	u32 new_sip;
	u32 new_dip;
	u16 new_dport;
	u16 new_sport;
	u32 resv1;
	u32 resv2;
	uint32_t resv3:24;
	uint32_t act_mode:2;	/* swnat mode */
	uint32_t act_dp:6;	/* index of dst_port */
	u16 vlan1;
	u16 etype;
	u8 dmac_hi[4];
	u16 vlan2_winfo;
	u8 dmac_lo[2];
	u8 smac_hi[4];
	u16 pppoe_id;
	u8 smac_lo[2];
	u16 minfo;
	u16 winfo;
	u32 tbd0;
	u32 tbd1;
	u32 resv4;
	u32 resv5;
	u32 resv6;
	u32 resv7;
	u16 rxif_idx;
	u16 tbd6;
};

struct _ipv4_dslite {
	union {
		struct ud_info_blk1 udib1;
		struct bf_info_blk1 bfib1;
		u32 info_blk1;
	};
	u32 sip;
	u32 dip;
	u16 dport;
	u16 sport;

	u32 tunnel_sipv6_0;
	u32 tunnel_sipv6_1;
	u32 tunnel_sipv6_2;
	u32 tunnel_sipv6_3;

	u32 tunnel_dipv6_0;
	u32 tunnel_dipv6_1;
	u32 tunnel_dipv6_2;
	u32 tunnel_dipv6_3;

	u8 flow_lbl[3];	/* in order to consist with Linux kernel (should be 20bits) */
	uint16_t priority:4;	/* in order to consist with Linux kernel (should be 8bits) */
	uint16_t rxif_idx:4;
	uint32_t hop_limit:8;
	uint32_t resv2:16;
	uint32_t act_mode:2;	/* swnat mode */
	uint32_t act_dp:6;	/* index of dst_port */

	union {
	struct _info_blk2 iblk2;
		u32 info_blk2;
	};

	u16 vlan1;
	u16 etype;
	u8 dmac_hi[4];
	u16 vlan2_winfo;
	u8 dmac_lo[2];
	u8 smac_hi[4];
	u16 pppoe_id;
	u8 smac_lo[2];
	u16 minfo;
	u16 winfo;
	u32 new_sip;
	u32 new_dip;
	u16 new_dport;
	u16 new_sport;
	//u16 rxif_idx;
};

struct _ipv6_1t_route {
	union {
		struct ud_info_blk1 udib1;
		struct bf_info_blk1 bfib1;
		u32 info_blk1;
	};
	u32 ipv6_dip0;
	u32 ipv6_dip1;
	u32 resv;

	union {
		struct _info_blk2 iblk2;
		u32 info_blk2;
	};

	u32 ipv6_dip2;
	u32 ipv6_dip3;

	uint32_t resv1:30;
	uint32_t act_mode:2;	/* swnat mode */
	uint32_t act_dp:6;	/* index of dst_port */
	u16 vlan1;
	u16 etype;
	u8 dmac_hi[4];
	u16 vlan2_winfo;
	u8 dmac_lo[2];
	u8 smac_hi[4];
	u16 pppoe_id;
	u8 smac_lo[2];
	u16 minfo;
	u16 winfo;
	u32 tbd0;
	u32 tbd1;
	u32 tbd2;
	u32 tbd3;
	u32 tbd4;
	u32 tbd5;
	u16 rxif_idx;
	u16 tbd6;
};

struct _ipv6_3t_route {
	union {
		struct ud_info_blk1 udib1;
		struct bf_info_blk1 bfib1;
		u32 info_blk1;
	};
	u32 ipv6_sip0;
	u32 ipv6_sip1;
	u32 ipv6_sip2;
	u32 ipv6_sip3;
	u32 ipv6_dip0;
	u32 ipv6_dip1;
	u32 ipv6_dip2;
	u32 ipv6_dip3;
	uint32_t prot:8;
	uint32_t resv:24;

	u32 resv1;
	u32 resv2;
	u32 resv3;
	uint32_t resv4:24;
	uint32_t act_mode:2;	/* swnat mode */
	uint32_t act_dp:6;	/* index of dst_port */

	union {
		struct _info_blk2 iblk2;
		u32 info_blk2;
	};
	u16 vlan1;
	u16 etype;
	u8 dmac_hi[4];
	u16 vlan2_winfo;
	u8 dmac_lo[2];
	u8 smac_hi[4];
	u16 pppoe_id;
	u8 smac_lo[2];
	u16 minfo;
	u16 winfo;
	u32 tbd0;
	u32 tbd1;
	u16 rxif_idx;
	u16 tbd2;
};

struct _ipv6_5t_route {
	union {
		struct ud_info_blk1 udib1;
		struct bf_info_blk1 bfib1;
		u32 info_blk1;
	};
	u32 ipv6_sip0;
	u32 ipv6_sip1;
	u32 ipv6_sip2;
	u32 ipv6_sip3;
	u32 ipv6_dip0;
	u32 ipv6_dip1;
	u32 ipv6_dip2;
	u32 ipv6_dip3;
	u16 dport;
	u16 sport;

	u32 resv1;
	u32 resv2;
	u32 resv3;
	uint32_t resv4:24;
	uint32_t act_mode:2;	/* swnat mode */
	uint32_t act_dp:6;	/* index of dst_port */

	union {
		struct _info_blk2 iblk2;
		u32 info_blk2;
	};

	u16 vlan1;
	u16 etype;
	u8 dmac_hi[4];
	u16 vlan2_winfo;
	u8 dmac_lo[2];
	u8 smac_hi[4];
	u16 pppoe_id;
	u8 smac_lo[2];
	u16 minfo;
	u16 winfo;
	u32 tbd0;
	u32 tbd1;
	u16 rxif_idx;
	u16 tbd2;
};

struct _ipv6_6rd {
	union {
		struct ud_info_blk1 udib1;
		struct bf_info_blk1 bfib1;
		u32 info_blk1;
	};
	u32 ipv6_sip0;
	u32 ipv6_sip1;
	u32 ipv6_sip2;
	u32 ipv6_sip3;
	u32 ipv6_dip0;
	u32 ipv6_dip1;
	u32 ipv6_dip2;
	u32 ipv6_dip3;
	u16 dport;
	u16 sport;

	u32 tunnel_sipv4;
	u32 tunnel_dipv4;
	uint32_t hdr_chksum:16;
	uint32_t dscp:8;
	uint32_t ttl:8;
	uint32_t flag:3;
	uint32_t resv1:11;
	uint32_t act_mode:2;	/* swnat mode */
	uint32_t per_flow_6rd_id:1;
	uint32_t rxif_idx:9;
	uint32_t act_dp:6;	/* index of dst_port */

	union {
	struct _info_blk2 iblk2;
		u32 info_blk2;
	};

	u16 vlan1;
	u16 etype;
	u8 dmac_hi[4];
	u16 vlan2_winfo;
	u8 dmac_lo[2];
	u8 smac_hi[4];
	u16 pppoe_id;
	u8 smac_lo[2];
	u16 minfo;
	u16 winfo;
	u32 tbd0;
	u32 tbd1;
	u16 new_dport;
	u16 new_sport;
	//u16 tbd2;
	//u16 rxif_idx;
};

struct foe_entry {
	union {
		struct ud_info_blk1 udib1;
		struct bf_info_blk1 bfib1;
		struct _ipv4_hnapt ipv4_hnapt;	/* nat & napt share same data structure */
		struct _ipv4_dslite ipv4_dslite;
		struct _ipv6_1t_route ipv6_1t_route;
		struct _ipv6_3t_route ipv6_3t_route;
		struct _ipv6_5t_route ipv6_5t_route;
		struct _ipv6_6rd ipv6_6rd;
	};
};

struct ps_entry {
	u8 en;
	u8 acl;
	u16 pkt_len;
	u16 pkt_cnt;
	u8 time_period;
	u8 resv0;
	u32 resv1;
	u16 hw_pkt_cnt;
	u16 hw_time;

};

#if defined(CONFIG_HNAT_V1)
struct mib_entry {
	u32 byt_cnt_l;
	u16 byt_cnt_h;
	u32 pkt_cnt_l;
	u8 pkt_cnt_h;
	u8 resv0;
	u32 resv1;
} __packed;
#elif defined(CONFIG_HNAT_V2)
struct mib_entry {
	u32 byt_cnt_l;
	u32 byt_cnt_h;
	u32 pkt_cnt_l;
	u32 pkt_cnt_h;
} __packed;
#endif

struct foe_pri_key {
	/* TODO: add new primary key to support dslite, 6rd */
	unsigned short hash_index;
	/* Ipv4 */
	struct {
		u32 sip;
		u32 dip;
		u16 sport;
		u16 dport;
		uint32_t is_udp:1;
	} ipv4_hnapt;

	struct {
		u32 sip;
		u32 dip;
		/* TODO */
	} ipv4_hnat;

	struct {
		u32 sip_v4;
		u32 dip_v4;
		u32 sip0_v6;
		u32 sip1_v6;
		u32 sip2_v6;
		u32 sip3_v6;
		u32 dip0_v6;
		u32 dip1_v6;
		u32 dip2_v6;
		u32 dip3_v6;
		u16 sport;
		u16 dport;
		uint32_t is_udp:1;
		uint32_t rmt:1;
	} ipv4_dslite;

	/* IPv6 */
	struct {
		u32 sip0;
		u32 sip1;
		u32 sip2;
		u32 sip3;
		u32 dip0;
		u32 dip1;
		u32 dip2;
		u32 dip3;
		u16 sport;
		u16 dport;
		uint32_t is_udp:1;
	} ipv6_routing;

	struct {
		u32 sip_v4;
		u32 dip_v4;
		u32 sip0_v6;
		u32 sip1_v6;
		u32 sip2_v6;
		u32 sip3_v6;
		u32 dip0_v6;
		u32 dip1_v6;
		u32 dip2_v6;
		u32 dip3_v6;
		u16 sport;
		u16 dport;
		uint32_t is_udp:1;
		uint32_t rmt:1;
	} ipv6_6rd;

	u32 pkt_type;	/* entry format */
};

u32 get_hash_ipv4(u32 saddr, u32 daddr, u32 sport, u32 dport);
u32 get_hash_ipv6(u32 sip0, u32 sip1, u32 sip2, u32 sip3,
	u32 dip0, u32 dip1, u32 dip2, u32 dip3, u32 sport, u32 dport);
void foe_set_mac_hi_info(u8 *dst, uint8_t *src);
void foe_set_mac_lo_info(u8 *dst, uint8_t *src);
void foe_revert_mac_hi_info(u8 *dst, uint8_t *src);
void foe_revert_mac_lo_info(u8 *dst, uint8_t *src);
void foe_dump_entry(uint32_t index, struct foe_entry *entry, uint32_t ppe_index);
int foe_get_all_entries(struct hwnat_args *opt);
int foe_bind_entry(struct hwnat_args *opt);
int foe_un_bind_entry(struct hwnat_args *opt);
int foe_drop_entry(struct hwnat_args *opt);
int foe_del_entry_by_num(uint32_t entry_num, uint32_t ppe_entry);
int foe_del_entry_by_mac(unsigned char *mac);
void foe_del_entry_by_dev(struct net_device *dev);
void foe_tbl_clean(void);
bool foe_has_bind_entry(void);
int foe_dump_cache_entry(void);
/* EXPORT FUNCTION*/
int32_t get_pppoe_sid(struct sk_buff *skb, uint32_t vlan_gap, uint16_t *sid, uint16_t *ppp_tag);
int ppe_set_bind_threshold(uint32_t threshold);
int ppe_set_max_entry_limit(u32 full, uint32_t half, uint32_t qurt);
int ppe_set_ka_interval(u8 tcp_ka, uint8_t udp_ka);
int ppe_set_unbind_lifetime(uint8_t lifetime);
int ppe_set_bind_lifetime(u16 tcp_fin, uint16_t udp_life, uint16_t fin_life);
void ppe_set_entry_bind(struct sk_buff *skb, struct foe_entry *entry);
int32_t ppe_fill_L2_info(struct sk_buff *skb, struct foe_entry *entry,
			 struct pkt_parse_result *ppe_parse_result);
int32_t ppe_fill_L3_info(struct sk_buff *skb, struct foe_entry *entry,
			 struct pkt_parse_result *ppe_parse_result);
int32_t ppe_fill_L4_info(struct sk_buff *skb, struct foe_entry *entry,
			 struct pkt_parse_result *ppe_parse_result);
int32_t ppe_setforce_port_info(struct sk_buff *skb, struct foe_entry *entry, int gmac_no);
struct net_device *ra_dev_get_by_name(const char *name);
int32_t is8021Q(u16 eth_type, struct pkt_parse_result *ppe_parse_result);
int32_t is_special_tag(u16 eth_type, struct pkt_parse_result *ppe_parse_result);
int32_t is_hw_vlan_tx(struct sk_buff *skb, struct pkt_parse_result *ppe_parse_result);
#if defined(CONFIG_SUPPORT_WLAN_OPTIMIZE)
int32_t ppe_rx_parse_layer_info(struct sk_buff *skb);
#endif
void ppe_set_cache_ebl(u32 ppe_index);
void update_foe_ac_timer_handler(unsigned long unused);
int foe_add_entry(struct hwnat_tuple *opt);
int foe_del_entry(struct hwnat_tuple *opt);
void set_qid(struct sk_buff *skb);
void foe_clear_entry(struct neighbour *neigh);
int foe_add_entry_dvt(struct hwnat_tuple *opt);
int get_ppe_entry_idx(struct foe_pri_key *key, struct foe_entry *entry, int del);
int get_mib_entry_idx(struct foe_pri_key *key, struct foe_entry *entry);
void ppe_mib_dump_ppe0(unsigned int entry_num, u64 *pkt_cnt, u64 *byte_cnt);
void ppe_mib_dump_ppe1(unsigned int entry_num, u64 *pkt_cnt, u64 *byte_cnt);
int ppe_get_dev_stats_handler(struct net_device *dev, struct rtnl_link_stats64 *storage);
int ppe_get_conn_stats_handler(struct hnat_flow_tuple *tuple, u64 *pkt_cnt, u64 *byte_cnt);
void ppe_init_mib_counter(void);
void ppe_start_mib_timer(void);
int ppe_mib_update(void);
void ppe_reset_dev_mib(struct net_device *dev);
void ppe_reset_entry_mib(struct foe_entry *entry);
void set_rxif_idx(struct foe_entry *entry, u16 value);
uint32_t get_act_mode(struct foe_entry *entry);
uint32_t get_act_dp(struct foe_entry *entry);
uint32_t get_rxif_idx(struct foe_entry *entry);
uint32_t get_vlan1(struct foe_entry *entry);
bool ppe_entry_check_state(struct foe_entry *entry, int state);
u32 get_hash_ipv4(u32 saddr, u32 daddr, u32 sport, u32 dport);
u32 get_hash_ipv6(u32 sip0, u32 sip1, u32 sip2, u32 sip3,
	u32 dip0, u32 dip1, u32 dip2, u32 dip3, u32 sport, u32 dport);
int ppe_force_port(struct foe_entry *entry);
int ppe_fqos(struct foe_entry *entry);

#endif
