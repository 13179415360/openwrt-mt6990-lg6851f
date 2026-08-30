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
#ifndef _FE_WANTED
#define _FE_WANTED

#include <linux/version.h>
#include <linux/ppp_defs.h>
#include <linux/etherdevice.h>

extern void __iomem *fe_base;
extern void __iomem *med_base;
extern void __iomem *netsys_base;


#define MTK_FE_RANGE			(0x20000)
#define FE_BASE			 	fe_base


#if defined(CONFIG_HNAT_V1)
#define MTK_FE_BASE			(0x15100000)
#define MTK_ETHDMA_BASE			(0x15000000)
#define MTK_MED_BASE			(0x15B38000)
#define MED_BASE                	(med_base)
#define WDMA_BASE			(fe_base + 0x2800)
#define MDMA_BASE			(fe_base + 0x4000)
#define PPE_BASE                	(FE_BASE + 0xc00)
#define PPE1_BASE   			(FE_BASE + 0x1000)
#endif

#if defined(CONFIG_HNAT_V2)
#define MTK_FE_BASE			(0x15100000)
#define MTK_ETHDMA_BASE			(0x15000000)
#define MTK_MED_BASE			(0x15B38000)
#define MED_BASE                	(med_base)
#define WDMA_BASE			(fe_base + 0x4800)
#define MDMA_BASE			(fe_base + 0x5400)
#define EDMA_BASE			(fe_base + 0x5c00)
#define PPE_BASE                	(FE_BASE + 0x2000)
#define PPE1_BASE			(FE_BASE + 0x2400)
#endif

#define WED_ACG			3
#define MED_ACG			4
#define ETH0_ACG		1
#define ETH1_ACG		2
#define NO_USE			(0x3f)

#define MAC_ARG(x) (((u8 *)(x))[0], ((u8 *)(x))[1], ((u8 *)(x))[2], \
		       ((u8 *)(x))[3], ((u8 *)(x))[4], ((u8 *)(x))[5])

#define IPV6_ADDR(x) (ntohs(x[0]), ntohs(x[1]), ntohs(x[2]), ntohs(x[3]), ntohs(x[4]),\
		     ntohs(x[5]), ntohs(x[6]), ntohs(x[7]))

#define IN
#define OUT
#define INOUT

#ifndef FALSE
#define FALSE 0
#endif

#define NAT_DEBUG

#ifdef NAT_DEBUG
#define NAT_PRINT(fmt, args...) printk(fmt, ## args)
#else
#define NAT_PRINT(fmt, args...) { }
#endif


#define FOE_TS		    (FE_BASE + 0x0010)
#define MTK_WDMA_BASE       (FE_BASE + 0x2800)
#define PSE_PPE0_DROP       (FE_BASE + 0x0110)
#define PSE_PPE1_DROP       (FE_BASE + 0x0114)
#define PPE_GLO_CFG	    (PPE_BASE + 0x200)
#define PPE_FLOW_CFG	    (PPE_BASE + 0x204)
#define PPE_FLOW_SET	    PPE_FLOW_CFG

#define PPE_IP_PROT_CHK	    (PPE_BASE + 0x208)

#define PPE_IP_PROT_0	    (PPE_BASE + 0x20C)
#define PPE_IP_PROT_1	    (PPE_BASE + 0x210)
#define PPE_IP_PROT_2	    (PPE_BASE + 0x214)
#define PPE_IP_PROT_3	    (PPE_BASE + 0x218)
#define PPE_TB_CFG	    (PPE_BASE + 0x21C)
#define PPE_FOE_CFG	    PPE_TB_CFG
#define PPE_TB_BASE	    (PPE_BASE + 0x220)
#define PPE_FOE_BASE	    (PPE_TB_BASE)

#define PPE_TB_USED	    (PPE_BASE + 0x224)
#define PPE_BNDR	    (PPE_BASE + 0x228)
#define PPE_FOE_BNDR	    PPE_BNDR
#define PPE_BIND_LMT_0	    (PPE_BASE + 0x22C)
#define PPE_FOE_LMT1	    (PPE_BIND_LMT_0)
#define PPE_BIND_LMT_1	    (PPE_BASE + 0x230)
#define PPE_FOE_LMT2	    PPE_BIND_LMT_1
#define PPE_KA		    (PPE_BASE + 0x234)
#define PPE_FOE_KA	    PPE_KA
#define PPE_UNB_AGE	    (PPE_BASE + 0x238)
#define PPE_FOE_UNB_AGE	    PPE_UNB_AGE
#define PPE_BND_AGE_0	    (PPE_BASE + 0x23C)
#define PPE_FOE_BND_AGE0    PPE_BND_AGE_0
#define PPE_BND_AGE_1	    (PPE_BASE + 0x240)
#define PPE_FOE_BND_AGE1    PPE_BND_AGE_1
#define PPE_HASH_SEED	    (PPE_BASE + 0x244)

#define PPE_MCAST_L_10       (PPE_BASE + 0x00)
#define PPE_MCAST_H_10       (PPE_BASE + 0x04)

#define PPE_DFT_CPORT       (PPE_BASE + 0x248)
#define PPE_DFT_CPORT1      (PPE_BASE + 0x24C)
#define PPE_MCAST_PPSE	    (PPE_BASE + 0x284)
#define PPE_MCAST_L_0       (PPE_BASE + 0x288)
#define PPE_MCAST_H_0       (PPE_BASE + 0x28C)
#define PPE_MCAST_L_1       (PPE_BASE + 0x290)
#define PPE_MCAST_H_1       (PPE_BASE + 0x294)
#define PPE_MCAST_L_2       (PPE_BASE + 0x298)
#define PPE_MCAST_H_2       (PPE_BASE + 0x29C)
#define PPE_MCAST_L_3       (PPE_BASE + 0x2A0)
#define PPE_MCAST_H_3       (PPE_BASE + 0x2A4)
#define PPE_MCAST_L_4       (PPE_BASE + 0x2A8)
#define PPE_MCAST_H_4       (PPE_BASE + 0x2AC)
#define PPE_MCAST_L_5       (PPE_BASE + 0x2B0)
#define PPE_MCAST_H_5       (PPE_BASE + 0x2B4)
#define PPE_MCAST_L_6       (PPE_BASE + 0x2BC)
#define PPE_MCAST_H_6       (PPE_BASE + 0x2C0)
#define PPE_MCAST_L_7       (PPE_BASE + 0x2C4)
#define PPE_MCAST_H_7       (PPE_BASE + 0x2C8)
#define PPE_MCAST_L_8       (PPE_BASE + 0x2CC)
#define PPE_MCAST_H_8       (PPE_BASE + 0x2D0)
#define PPE_MCAST_L_9       (PPE_BASE + 0x2D4)
#define PPE_MCAST_H_9       (PPE_BASE + 0x2D8)
#define PPE_MCAST_L_A       (PPE_BASE + 0x2DC)
#define PPE_MCAST_H_A       (PPE_BASE + 0x2E0)
#define PPE_MCAST_L_B       (PPE_BASE + 0x2E4)
#define PPE_MCAST_H_B       (PPE_BASE + 0x2E8)
#define PPE_MCAST_L_C       (PPE_BASE + 0x2EC)
#define PPE_MCAST_H_C       (PPE_BASE + 0x2F0)
#define PPE_MCAST_L_D       (PPE_BASE + 0x2F4)
#define PPE_MCAST_H_D       (PPE_BASE + 0x2F8)
#define PPE_MCAST_L_E       (PPE_BASE + 0x2FC)
#define PPE_MCAST_H_E       (PPE_BASE + 0x2E0)
#define PPE_MCAST_L_F       (PPE_BASE + 0x300)
#define PPE_MCAST_H_F       (PPE_BASE + 0x304)
#define PPE_MTU_DRP         (PPE_BASE + 0x308)
#define PPE_MTU_VLYR_0      (PPE_BASE + 0x30C)
#define PPE_MTU_VLYR_1      (PPE_BASE + 0x310)
#define PPE_MTU_VLYR_2      (PPE_BASE + 0x314)
#define PPE_VPM_TPID        (PPE_BASE + 0x318)

#define CAH_CTRL	    (PPE_BASE + 0x320)
#define CAH_TAG_SRH         (PPE_BASE + 0x324)
#define CAH_LINE_RW         (PPE_BASE + 0x328)
#define CAH_WDATA           (PPE_BASE + 0x32C)
#define CAH_RDATA           (PPE_BASE + 0x330)

#define CAH_CTRL	    (PPE_BASE + 0x320)
#define CAH_TAG_SRH         (PPE_BASE + 0x324)
#define CAH_LINE_RW         (PPE_BASE + 0x328)
#define CAH_WDATA           (PPE_BASE + 0x32C)
#define CAH_RDATA           (PPE_BASE + 0x330)
#define PPE_SBW_CTRL        (PPE_BASE + 0x374)

#define PS_CFG	            (PPE_BASE + 0x400)
#define PS_FBC		    (PPE_BASE + 0x404)
#define PS_TB_BASE	    (PPE_BASE + 0x408)
#define PS_TME_SMP	    (PPE_BASE + 0x40C)

#define MIB_CFG		    (PPE_BASE + 0x334)
#define MIB_TB_BASE	    (PPE_BASE + 0x338)
#define MIB_SER_CR	    (PPE_BASE + 0x33C)
#define MIB_SER_R0	    (PPE_BASE + 0x340)
#define MIB_SER_R1	    (PPE_BASE + 0x344)
#define MIB_SER_R2	    (PPE_BASE + 0x348)
#define MIB_SER_R3	    (PPE_BASE + 0x34C)
#define MIB_CAH_CTRL	    (PPE_BASE + 0x350)
#define PPE_6RD_ID	    (PPE_BASE + 0x36c)

#define MDMA_TX_BASE_PTR_0	(MDMA_BASE)
#define MDMA_TX_MAX_CNT_0	(MDMA_BASE + 0x4)
#define MDMA_RX_BASE_PTR_0	(MDMA_BASE + 0x100)
#define MDMA_RX_MAX_CNT_0	(MDMA_BASE + 0x104)
#define WDMA0_TX_BASE_PTR_0	(WDMA_BASE)
#define WDMA0_TX_MAX_CNT_0	(WDMA_BASE + 0x4)
#define WDMA0_RX_BASE_PTR_0	(WDMA_BASE + 0x100)
#define WDMA0_RX_MAX_CNT_0	(WDMA_BASE + 0x104)

#define WDMA1_TX_BASE_PTR_0	(WDMA_BASE + 0x400)
#define WDMA1_TX_MAX_CNT_0	(WDMA_BASE + 0x404)
#define WDMA1_RX_BASE_PTR_0	(WDMA_BASE + 0x500)
#define WDMA1_RX_MAX_CNT_0	(WDMA_BASE + 0x504)

#define EDMA0_TX_BASE_PTR_0	(EDMA_BASE)
#define EDMA0_TX_MAX_CNT_0	(EDMA_BASE + 0x4)
#define EDMA0_RX_BASE_PTR_0	(EDMA_BASE + 0x100)
#define EDMA0_RX_MAX_CNT_0	(EDMA_BASE + 0x104)

#define EDMA1_TX_BASE_PTR_0	(EDMA_BASE + 0x400)
#define EDMA1_TX_MAX_CNT_0	(EDMA_BASE + 0x404)
#define EDMA1_RX_BASE_PTR_0	(EDMA_BASE + 0x500)
#define EDMA1_RX_MAX_CNT_0	(EDMA_BASE + 0x504)

#define EDMA2_TX_BASE_PTR_0	(EDMA_BASE + 0x800)
#define EDMA2_TX_MAX_CNT_0	(EDMA_BASE + 0x804)
#define EDMA2_RX_BASE_PTR_0	(EDMA_BASE + 0x900)
#define EDMA2_RX_MAX_CNT_0	(EDMA_BASE + 0x904)


// 46240/8 = 5450
#define MED_INFO_SIZE			(5450)
//MED_HNAT_INFO_HOST_START_PTR (40b)
#define MEDHW_SSR1_DST_RB0_BASE		(MED_BASE + 0x80)
#define MEDHW_SSR1_DST_RB0_BASE_HI	(MED_BASE + 0x84)

//MED_HNAT_INFO_HOST_CNT (18bit) :
#define MEDHW_SSR1_DST_RB0_SIZE		(MED_BASE + 0x88)

//MED_HNAT_INFO_HOST_WIDX (18b) :
#define MEDHW_SSR1_DST_RB0_WIDX		(MED_BASE + 0x90)

//MED_HNAT_INFO_HOST_RIDX (18b):
#define MEDHW_SSR1_DST_RB0_RIDX		(MED_BASE + 0x94)
#define MEDHW_SSR1_DST_RB0_REMAIN	(MED_BASE + 0x98)
#define MEDHW_SSR1_DST_RB0_OCCUPY	(MED_BASE + 0x9c)

#define MEDHW_SSR1_DST_RB0_CFG		(MED_BASE + 0xa0)
#define MEDHW_SSR1_DST_RB0_STS		(MED_BASE + 0xa4)
//MED_HNAT_INFO_HOST_ADD_ENTRY_CNT (18):
#define MEDHW_SSR1_DST_RB0_INC		(MED_BASE + 0xb0)
#define MEDHW_SSR1_DST_RB0_DEC		(MED_BASE + 0xb4)

//MED_HNAT_INFO_HOST_START_IDX (18b)
#define MEDHW_SSR1_DST_RB0_RSTR		(MED_BASE + 0xb8)

/*CAH_RDATA[17:16] */
/*0: invalid */
/*1: valid */
/*2: dirty */
/*3: lock */
/*CAH_RDATA[15:0]: entry num*/
/* #define CAH_RDATA	    PPE_BASE + 0x330 */
/* TO PPE */
#define IPV4_PPE_MYUC	    BIT(0) /* my mac */
#define IPV4_PPE_MC	    BIT(1) /* multicast */
#define IPV4_PPE_IPM	    BIT(2) /* ip multicast */
#define IPV4_PPE_BC	    BIT(3) /* broadcast */
#define IPV4_PPE_UC	    BIT(4) /* ipv4 learned UC frame */
#define IPV4_PPE_UN	    BIT(5) /* ipv4 unknown  UC frame */

#define IPV6_PPE_MYUC	    BIT(8) /* my mac */
#define IPV6_PPE_MC	    BIT(9) /* multicast */
#define IPV6_PPE_IPM	    BIT(10) /* ipv6 multicast */
#define IPV6_PPE_BC	    BIT(11) /* broadcast */
#define IPV6_PPE_UC	    BIT(12) /* ipv6 learned UC frame */
#define IPV6_PPE_UN	    BIT(13) /* ipv6 unknown  UC frame */

#if defined(CONFIG_HNAT_V1)
#define AC_BASE		    (FE_BASE + 0x2000)
#elif defined(CONFIG_HNAT_V2)
#define PPE0_AC_BASE		(FE_BASE + 0x1800)
#define PPE1_AC_BASE		(FE_BASE + 0x1900)
#endif /* CONFIG_HNAT_V2 */

#define PSE_Q_RES (0x36)
#define PSE_IQ_RLS (0x1b)

#define	FE_GLO_CFG2		(FE_BASE + 0x24)
#define	PSE_IQ_REV3		(FE_BASE + 0x148)
#define	PSE_IQ_REV5		(FE_BASE + 0x150)
#define	PSE_IQ_REV6		(FE_BASE + 0x154)
#define	PSE_OQ_TH1		(FE_BASE + 0x160)
#define	PSE_OQ_TH2		(FE_BASE + 0x164)
#define	PSE_OQ_TH4		(FE_BASE + 0x16c)
#define	PSE_OQ_TH5		(FE_BASE + 0x170)	/* p9(wdma1),p8 (wdma0), F, F */
#define	PSE_OQ_TH6		(FE_BASE + 0x174)	/* p11(edma0),p10 (mdma, no need), 6, F */
#define	PSE_OQ_TH7		(FE_BASE + 0x178)	/* p13 (wdma2),p12(edma1), F, 6 */

#define	PSE_IQ_REV_RLS1 (FE_BASE + 0xC0)
#define	PSE_IQ_REV_RLS2 (FE_BASE + 0xC4)
#define	PSE_IQ_REV_RLS3 (FE_BASE + 0xC8)
#define	PSE_IQ_REV_RLS4 (FE_BASE + 0xCC)
#define	PSE_IQ_REV_RLS5 (FE_BASE + 0xD0)
#define	PSE_IQ_REV_RLS6 (FE_BASE + 0xD4)
#define	PSE_IQ_REV_RLS7 (FE_BASE + 0xD8)


#define FE_GDMA1_FWD_CFG    	(FE_BASE + 0x500)
#define FE_GDMA2_FWD_CFG    	(FE_BASE + 0x1500)

/* GDMA1 My MAC unicast frame destination port */
#if defined(CONFIG_RAETH_QDMATX_QDMARX)
#define GDM1_UFRC_P_CPU     (5 << 12)
#else
#define GDM1_UFRC_P_CPU     (0 << 12)
#endif


/* GDMA1 broadcast frame MAC address destination port */
#if defined(CONFIG_RAETH_QDMATX_QDMARX)
#define GDM1_BFRC_P_CPU     (5 << 8)
#else
#define GDM1_BFRC_P_CPU     (0 << 8)
#endif


/* GDMA1 multicast frame MAC address destination port */
#if defined(CONFIG_RAETH_QDMATX_QDMARX)
#define GDM1_MFRC_P_CPU     (5 << 4)
#else
#define GDM1_MFRC_P_CPU     (0 << 4)
#endif


/* GDMA1 other MAC address frame destination port */
#if defined(CONFIG_RAETH_QDMATX_QDMARX)
#define GDM1_OFRC_P_CPU     (5 << 0)
#else
#define GDM1_OFRC_P_CPU     (0 << 0)
#endif

#define	GDMA1_PSE_PORT       1
#define	GDMA2_PSE_PORT       2
#define	QDMA_PSE_PORT        5
#define	WDMA0_PSE_PORT       8
#define	WDMA1_PSE_PORT       9
#define	MDMA_PSE_PORT        10
#define	EDMA0_PSE_PORT       11
#define	EDMA1_PSE_PORT       12
#define	WDMA2_PSE_PORT       13
#define	EDMA2_PSE_PORT       14
#define	ADMA_PSE_PORT        0

#if !defined(CONFIG_ARCH_COLGIN)
#define GDM1_UFRC_P_PPE     (4 << 12)
#define GDM1_BFRC_P_PPE     (4 << 8)
#define GDM1_MFRC_P_PPE     (4 << 4)
#define GDM1_OFRC_P_PPE     (4 << 0)
#else
#define GDM1_UFRC_P_PPE     (3 << 12)
#define GDM1_BFRC_P_PPE     (3 << 8)
#define GDM1_MFRC_P_PPE     (3 << 4)
#define GDM1_OFRC_P_PPE     (3 << 0)
#define GDM1_UFRC_P_PPE1     (4 << 12)
#define GDM1_BFRC_P_PPE1     (4 << 8)
#define GDM1_MFRC_P_PPE1     (4 << 4)
#define GDM1_OFRC_P_PPE1     (4 << 0)
#endif

enum FOE_SMA {
	DROP = 0,		/* Drop the packet */
	DROP2 = 1,		/* Drop the packet */
	ONLY_FWD_CPU = 2,	/* Only Forward to CPU */
	FWD_CPU_BUILD_ENTRY = 3	/* Forward to CPU and build new FOE entry */
};

enum DIR {
	DIR_NONE = 0,		/* default:none */
	DIR_BIDIRECTION = 1,	/* bi-direction */
	DIR_DOWNLINK = 2,	/* downlink direction */
	DIR_UPLINK = 3,		/* uplink direction */
};

enum BIND_DIR {
	UPSTREAM_ONLY = 0,	/* only speed up upstream flow */
	DOWNSTREAM_ONLY = 1,	/* only speed up downstream flow */
	BIDIRECTION = 2		/* speed up bi-direction flow */
};

/* PPE_GLO_CFG, Offset=0x200 */
#define DFL_TTL0_DRP		(0)	/* 1:Drop, 0: Alert CPU */
/* PPE Flow Set*/
#define BIT_FBC_FOE		BIT(0)	/* PPE engine for broadcast flow */
#define BIT_FMC_FOE		BIT(1)	/* PPE engine for multicast flow */
#define BIT_FUC_FOE		BIT(2)	/* PPE engine for multicast flow */
#define BIT_UDP_IP4F_NAT_EN	BIT(7)  /*Enable IPv4 fragment + UDP packet NAT*/
#define BIT_IPV6_3T_ROUTE_EN	BIT(8)	/* IPv6 3-tuple route */
#define BIT_IPV6_5T_ROUTE_EN	BIT(9)	/* IPv6 5-tuple route */
#define BIT_IPV6_6RD_EN		BIT(10)	/* IPv6 6RD */
#define BIT_IPV4_464XLAT_EN	BIT(11)	/* IPv4 464XLAT */
#define BIT_IPV4_NAT_EN		BIT(12)	/* IPv4 NAT */
#define BIT_IPV4_NAPT_EN	BIT(13)	/* IPv4 NAPT */
#define BIT_IPV4_DSL_EN		BIT(14)	/* IPv4 DS-Lite */
#define BIT_IP_PROT_CHK_BLIST	BIT(16)	/* IP protocol check is black/white list */
#define BIT_IPV4_NAT_FRAG_EN	BIT(17)	/* Enable fragment support for IPv4 NAT flow */
#define BIT_IPV6_HASH_FLAB	BIT(18)
/* For IPv6 5-tuple and 6RD flow, using flow label instead of sport and dport to do HASH */
#define BIT_IPV4_HASH_GREK	BIT(19)	/* For IPv4 NAT, adding GRE key into HASH */
#define BIT_IPV6_HASH_GREK	BIT(20)	/* For IPv6 3-tuple, adding GRE key into HASH */
#define BIT_IPV4_MAPE_EN	BIT(21)	/*MAPE*/
#define BIT_IPV4_MAPT_EN	BIT(22)	/*MAPT*/

#define IS_IPV6_FLAB_EBL()	((reg_read(PPE_FLOW_SET) & BIT_IPV6_HASH_FLAB) ? 1 : 0)

/* PPE FOE Bind Rate*/
/* packet in a time stamp unit */
#define DFL_FOE_BNDR		30
/*config  RA_HW_NAT_PBND_RD_LMT*/
/*        int "max retyr count"*/
/*	default 10*/
/*	depends on RA_HW_NAT_PREBIND*/
#define DFL_PBND_RD_LMT		10
/*config  RA_HW_NAT_PBND_RD_PRD*/
/*int "check interval in pause state (us) Max:65535"*/
/*	default 1000*/
/*	depends on RA_HW_NAT_PREBIND*/
#define DFL_PBND_RD_PRD		1000

/* PPE_FOE_LMT */
/* smaller than 1/4 of total entries */
#define DFL_FOE_QURT_LMT	16383 /* CONFIG_RA_HW_NAT_QURT_LMT */

/* between 1/2 and 1/4 of total entries */
#define DFL_FOE_HALF_LMT	16383 /* CONFIG_RA_HW_NAT_HALF_LMT */

/* between full and 1/2 of total entries */
#define DFL_FOE_FULL_LMT	32767 /* CONFIG_RA_HW_NAT_FULL_LMT */

/* PPE_FOE_KA*/
/* visit a FOE entry every FOE_KA_T * 1 msec */
#define DFL_FOE_KA_T		1

#if defined(CONFIG_RA_HW_NAT_TBL_1K)
/* FOE_TCP_KA * FOE_KA_T * FOE_4TB_SIZ */
/*TCP KeepAlive Interval(Unit:1Sec)*/
#define DFL_FOE_TCP_KA		5
/* FOE_UDP_KA * FOE_KA_T * FOE_4TB_SIZ */
/*UDP KeepAlive Interval(Unit:1Sec)*/
#define DFL_FOE_UDP_KA		5
/* FOE_NTU_KA * FOE_KA_T * FOE_4TB_SIZ */
/*Non-TCP/UDP KeepAlive Interval(Unit:1Sec)*/
#define DFL_FOE_NTU_KA		5
#elif defined(CONFIG_RA_HW_NAT_TBL_2K)
/*(Unit:2Sec)*/
#define DFL_FOE_TCP_KA		3
#define DFL_FOE_UDP_KA		3
#define DFL_FOE_NTU_KA		3
#elif defined(CONFIG_RA_HW_NAT_TBL_4K)
/*(Unit:4Sec)*/
#define DFL_FOE_TCP_KA		1
#define DFL_FOE_UDP_KA		1
#define DFL_FOE_NTU_KA		1
#elif defined(CONFIG_RA_HW_NAT_TBL_8K)
/*(Unit:8Sec)*/
#define DFL_FOE_TCP_KA		1
#define DFL_FOE_UDP_KA		1
#define DFL_FOE_NTU_KA		1
#elif defined(CONFIG_RA_HW_NAT_TBL_16K)
/*(Unit:16Sec)*/
#define DFL_FOE_TCP_KA		1
#define DFL_FOE_UDP_KA		1
#define DFL_FOE_NTU_KA		1
#elif defined(CONFIG_RA_HW_NAT_TBL_32K)
/*(Unit:16Sec)*/
#define DFL_FOE_TCP_KA		1
#define DFL_FOE_UDP_KA		1
#define DFL_FOE_NTU_KA		1
#endif

/*PPE_FOE_CFG*/
#if defined(CONFIG_RA_HW_NAT_HASH0)
#define DFL_FOE_HASH_MODE	0
#elif defined(CONFIG_RA_HW_NAT_HASH1)
#define DFL_FOE_HASH_MODE	1
#elif defined(CONFIG_RA_HW_NAT_HASH2)
#define DFL_FOE_HASH_MODE	2
#elif defined(CONFIG_RA_HW_NAT_HASH3)
#define DFL_FOE_HASH_MODE	3
#elif defined(CONFIG_RA_HW_NAT_HASH_DBG)
#define DFL_FOE_HASH_MODE	0 /* don't care */
#endif

#define HASH_SEED		0x12345678
#define DFL_FOE_UNB_AGE		1	/* Unbind state age enable */
#define DFL_FOE_TCP_AGE		1	/* Bind TCP age enable */
#define DFL_FOE_NTU_AGE		1	/* Bind TCP age enable */
#define DFL_FOE_UDP_AGE		1	/* Bind UDP age enable */
#define DFL_FOE_FIN_AGE		1	/* Bind TCP FIN age enable */
#define DFL_FOE_KA		3	/* 0:disable 1:unicast old 2: multicast new 3. duplicate old */

/*PPE_FOE_UNB_AGE*/
/*The min threshold of packet count for aging out at unbind state */
/*An unbind flow whose pkt counts < Min threshold and idle time > Life time*/
/*=> This unbind entry would be aged out*/
/*[Notes: Idle time = current time - last packet receive time] (Pkt count)*/
#define DFL_FOE_UNB_MNP		1000
/* Delta time for aging out an unbind FOE entry */
/*set ageout time for bind Unbind entry(Unit:1Sec)*/
#define DFL_FOE_UNB_DLTA	3
/* Delta time for aging out an bind Non-TCP/UDP FOE entry */
#define DFL_FOE_NTU_DLTA	5

/* PPE_FOE_BND_AGE1*/
/* Delta time for aging out an bind UDP FOE entry */
/*Set ageout time for bind UDP entry(Unit:1Sec)*/
#define DFL_FOE_UDP_DLTA	5

/*PPE_FOE_BND_AGE2*/
/* Delta time for aging out an bind TCP FIN entry */
/*Set ageout time for FIN entry*/
#define DFL_FOE_FIN_DLTA	5
/* Delta time for aging out an bind TCP entry */
/*Set ageout time for bind TCP entry (Unit:1Sec)*/
#define DFL_FOE_TCP_DLTA	5

#define DFL_FOE_TTL_REGEN	1	/* TTL = TTL -1 */

#define PPE1_GLO_CFG	    (PPE1_BASE + 0x200)
#define PPE1_FLOW_CFG	    (PPE1_BASE + 0x204)
#define PPE1_FLOW_SET	    PPE1_FLOW_CFG

#define PPE1_IP_PROT_CHK    (PPE1_BASE + 0x208)
#define PPE1_IP_PROT_0	    (PPE1_BASE + 0x20C)
#define PPE1_IP_PROT_1	    (PPE1_BASE + 0x210)
#define PPE1_IP_PROT_2	    (PPE1_BASE + 0x214)
#define PPE1_IP_PROT_3	    (PPE1_BASE + 0x218)
#define PPE1_TB_CFG	    (PPE1_BASE + 0x21C)
#define PPE1_FOE_CFG	    PPE1_TB_CFG
#define PPE1_TB_BASE	    (PPE1_BASE + 0x220)
#define PPE1_FOE_BASE	    (PPE1_TB_BASE)
#define PPE1_TB_USED	    (PPE1_BASE + 0x224)
#define PPE1_BNDR	    (PPE1_BASE + 0x228)
#define PPE1_FOE_BNDR	    PPE1_BNDR
#define PPE1_BIND_LMT_0	    (PPE1_BASE + 0x22C)
#define PPE1_FOE_LMT1	    (PPE1_BIND_LMT_0)
#define PPE1_BIND_LMT_1	    (PPE1_BASE + 0x230)
#define PPE1_FOE_LMT2	    PPE1_BIND_LMT_1
#define PPE1_KA		    (PPE1_BASE + 0x234)
#define PPE1_FOE_KA	    PPE1_KA
#define PPE1_UNB_AGE	    (PPE1_BASE + 0x238)
#define PPE1_FOE_UNB_AGE	    PPE1_UNB_AGE
#define PPE1_BND_AGE_0	    (PPE1_BASE + 0x23C)
#define PPE1_FOE_BND_AGE0    PPE1_BND_AGE_0
#define PPE1_BND_AGE_1	    (PPE1_BASE + 0x240)
#define PPE1_FOE_BND_AGE1    PPE1_BND_AGE_1
#define PPE1_HASH_SEED	    (PPE1_BASE + 0x244)

#define PPE1_MCAST_L_10       (PPE1_BASE + 0x00)
#define PPE1_MCAST_H_10       (PPE1_BASE + 0x04)

#define PPE1_DFT_CPORT       (PPE1_BASE + 0x248)
#define PPE1_DFT_CPORT1      (PPE1_BASE + 0x24c)
#define PPE1_MCAST_PPSE	     (PPE1_BASE + 0x284)
#define PPE1_MCAST_L_0       (PPE1_BASE + 0x288)
#define PPE1_MCAST_H_0       (PPE1_BASE + 0x28C)
#define PPE1_MCAST_L_1       (PPE1_BASE + 0x290)
#define PPE1_MCAST_H_1       (PPE1_BASE + 0x294)
#define PPE1_MCAST_L_2       (PPE1_BASE + 0x298)
#define PPE1_MCAST_H_2       (PPE1_BASE + 0x29C)
#define PPE1_MCAST_L_3       (PPE1_BASE + 0x2A0)
#define PPE1_MCAST_H_3       (PPE1_BASE + 0x2A4)
#define PPE1_MCAST_L_4       (PPE1_BASE + 0x2A8)
#define PPE1_MCAST_H_4       (PPE1_BASE + 0x2AC)
#define PPE1_MCAST_L_5       (PPE1_BASE + 0x2B0)
#define PPE1_MCAST_H_5       (PPE1_BASE + 0x2B4)
#define PPE1_MCAST_L_6       (PPE1_BASE + 0x2BC)
#define PPE1_MCAST_H_6       (PPE1_BASE + 0x2C0)
#define PPE1_MCAST_L_7       (PPE1_BASE + 0x2C4)
#define PPE1_MCAST_H_7       (PPE1_BASE + 0x2C8)
#define PPE1_MCAST_L_8       (PPE1_BASE + 0x2CC)
#define PPE1_MCAST_H_8       (PPE1_BASE + 0x2D0)
#define PPE1_MCAST_L_9       (PPE1_BASE + 0x2D4)
#define PPE1_MCAST_H_9       (PPE1_BASE + 0x2D8)
#define PPE1_MCAST_L_A       (PPE1_BASE + 0x2DC)
#define PPE1_MCAST_H_A       (PPE1_BASE + 0x2E0)
#define PPE1_MCAST_L_B       (PPE1_BASE + 0x2E4)
#define PPE1_MCAST_H_B       (PPE1_BASE + 0x2E8)
#define PPE1_MCAST_L_C       (PPE1_BASE + 0x2EC)
#define PPE1_MCAST_H_C       (PPE1_BASE + 0x2F0)
#define PPE1_MCAST_L_D       (PPE1_BASE + 0x2F4)
#define PPE1_MCAST_H_D       (PPE1_BASE + 0x2F8)
#define PPE1_MCAST_L_E       (PPE1_BASE + 0x2FC)
#define PPE1_MCAST_H_E       (PPE1_BASE + 0x2E0)
#define PPE1_MCAST_L_F       (PPE1_BASE + 0x300)
#define PPE1_MCAST_H_F       (PPE1_BASE + 0x304)
#define PPE1_MTU_DRP         (PPE1_BASE + 0x308)
#define PPE1_MTU_VLYR_0      (PPE1_BASE + 0x30C)
#define PPE1_MTU_VLYR_1      (PPE1_BASE + 0x310)
#define PPE1_MTU_VLYR_2      (PPE1_BASE + 0x314)
#define PPE1_VPM_TPID        (PPE1_BASE + 0x318)



#define CAH_CTRL_PPE1	    (PPE1_BASE + 0x320)
#define CAH_TAG_SRH_PPE1    (PPE1_BASE + 0x324)
#define CAH_LINE_RW_PPE1    (PPE1_BASE + 0x328)
#define CAH_WDATA_PPE1      (PPE1_BASE + 0x32C)
#define CAH_RDATA_PPE1      (PPE1_BASE + 0x330)



#define MIB_CFG_PPE1	    (PPE1_BASE + 0x334)
#define MIB_TB_BASE_PPE1    (PPE1_BASE + 0x338)
#define MIB_SER_CR_PPE1	    (PPE1_BASE + 0x33C)
#define MIB_SER_R0_PPE1	    (PPE1_BASE + 0x340)
#define MIB_SER_R1_PPE1	    (PPE1_BASE + 0x344)
#define MIB_SER_R2_PPE1	    (PPE1_BASE + 0x348)
#define MIB_SER_R3_PPE1	    (PPE1_BASE + 0x34C)
#define MIB_CAH_CTRL_PPE1   (PPE1_BASE + 0x350)
#define PPE1_6RD_ID	    (PPE1_BASE + 0x36c)
#define PPE1_SBW_CTRL       (PPE1_BASE + 0x374)

/* NETSYS DVFS Configuration 0 */
#define NETSYS_DVFS_CFG0		(netsys_base + 0xCC)
#define NETSYS_DVFS_EN			BIT(0)

/* NETSYS DVFS Configuration 1 */
#define NETSYS_DVFS_CFG1		(netsys_base + 0xD0)
#define NETSYS_SW_VC_DVFS_EN		BIT(16)
#define NETSYS_SW_VC_DVFS_REQ		BIT(17)
#define NETSYS_SW_VC_DVFS_ACK		BIT(19)
#define NETSYS_SW_VC_DVFS_VAL_OFFSET	20
#define NETSYS_SW_BW_DVFS_EN		BIT(24)
#define NETSYS_SW_BW_DVFS_REQ		BIT(25)
#define NETSYS_SW_BW_DVFS_ACK		BIT(27)
#define NETSYS_SW_BW_DVFS_VAL_OFFSET	28

/* NETSYS DVFS Configuration 2 */
#define NETSYS_DVFS_CFG2		(netsys_base + 0xD4)
#define NETSYS_DVFS0_VC_VAL_OFFSET	0
#define NETSYS_DVFS1_VC_VAL_OFFSET	4
#define NETSYS_DVFS2_VC_VAL_OFFSET	8
#define NETSYS_DVFS3_VC_VAL_OFFSET	12
#define NETSYS_DVFS0_BW_VAL_OFFSET	16
#define NETSYS_DVFS1_BW_VAL_OFFSET	20
#define NETSYS_DVFS2_BW_VAL_OFFSET	24
#define NETSYS_DVFS3_BW_VAL_OFFSET	28

/* NETSYS DVFS Accounting Configuration */
#define NETSYS_DVFS_ACC_CFG		(netsys_base + 0xD8)
#define NETSYS_DVFS_ACC_PERIOD_OFFSET	0
#define NETSYS_DVFS_ACC_BYTE_CNT_EN	BIT(30)
#define NETSYS_DVFS_ACC_PKT_CNT_EN	BIT(31)

/* NETSYS Byte Threshold */
#define NETSYS_DVFS_BYTE_TH1		(netsys_base + 0xDC)
#define NETSYS_DVFS_BYTE_TH2		(netsys_base + 0xE0)
#define NETSYS_DVFS_BYTE_TH3		(netsys_base + 0xE4)

/* NETSYS Packet Threshold */
#define NETSYS_DVFS_PKT_TH1		(netsys_base + 0xE8)
#define NETSYS_DVFS_PKT_TH2		(netsys_base + 0xEC)
#define NETSYS_DVFS_PKT_TH3		(netsys_base + 0xF0)

/* NETSYS DVFS Level Debug */
#define NETSYS_DVFS_LEVEL_DBG		(netsys_base + 0x130)
#define NETSYS_DVFS_BYTE_LEVEL_OFFSET	8
#define NETSYS_DVFS_PKT_LEVEL_OFFSET	10
#define NETSYS_DVFS_ALL_LEVEL_OFFSET	12

#endif
