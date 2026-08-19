/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SCP_RESERVEDMEM_DEFINE_H__
#define __SCP_RESERVEDMEM_DEFINE_H__

#include "medmcu_feature_define.h"

static struct scp_reserve_mblock scp_reserve_mblock[] = {
	{
		.num = SCP_A_LOGGER_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size       = 0x180000,  /* 1.5 MB */
	},
	{
		.num = SCP_A_TX_DESC_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size       = 0x5EEC0,  /* 388800B */
	},
	{
		.num = SCP_A_RX_DESC_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x20D00,  /* 134400B */
	},
	{
#ifdef MED_V2
		.num = SCP_A_RX_DESC_1_MEM_ID,
#else
		.num = SCP_A_RX_DESC_DUP_MEM_ID,
#endif
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x20D00,  /* 134400B */
	},
	{
		.num = SCP_A_HNAT_INFO_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x38720,  /* 231200B */
	},
	{
		.num = SCP_A_HNAT_INFO_HOST_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0xB4A0,  /* 46240B */
	},
	{
		.num = SCP_A_PIT_NAT_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x2A940,  /* 174400B */
	},
	{
		.num = SCP_A_PIT_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x80F80,  /* 528256B */
	},
	{
		.num = SCP_A_DRB0_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x10680,  /* 67200B */
	},
	{
		.num = SCP_A_DRB1_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x8340,  /* 33600B */
	},
#ifdef MED_V2
	{
		.num = SCP_A_DRB2_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x10680,  /* 67200B */
	},
#endif
};

#if TEST_WITHOUT_MODEM
static struct scp_emu_reserve_mblock scp_emu_reserve_mblock[] = {
#if TEST_WITHOUT_CCCI
	{
		.num = SCP_A_EMU_BAT_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size       = 0x10000,  /* 64KB */
	},
	{
		.num = SCP_A_EMU_FRAG_BAT_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x10000,	/* 64KB */
	},
	{
		.num = SCP_A_EMU_BAT_SKB_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x1100000,	/* 16MB */
	},
	{
		.num = SCP_A_EMU_FRAG_BAT_SKB_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x2100000,	/* 32MB */
	},
#endif  // TEST_WITHOUT_CCCI
	{
		.num = SCP_A_TEMP_PIT_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size		= 0x80F80,	/* 528256B */
	},
};
#endif  // TEST_WITHOUT_MODEM
#endif
