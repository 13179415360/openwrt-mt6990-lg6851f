/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SCP_HELPER_H__
#define __SCP_HELPER_H__

#include <linux/notifier.h>
#include <linux/interrupt.h>
#include "medmcu_reg.h"
#include "medmcu_feature_define.h"

#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/of_net.h>
#include <linux/etherdevice.h>


#define ROUNDUP(a, b)		      (((a) + ((b)-1)) & ~((b)-1))

/* scp config reg. definition*/
#define SCP_TCM_SIZE              (scpreg.total_tcmsize)
#define SCP_A_TCM_SIZE            (scpreg.scp_tcmsize)
#define SCP_TCM                   (scpreg.sram)
#define SCP_REGION_INFO_OFFSET    0x4
#define SCP_RTOS_START            (0x800)

#define SCP_DRAM_MAPSIZE          (0x20000)

/* scp dvfs return status flag */
#define SET_PLL_FAIL		(1)
#define SET_PMIC_VOLT_FAIL	(2)

#if defined(CONFIG_MTK_WAN_MTU_SIZE)
#define MED_WAN_MTU          (CONFIG_MTK_WAN_MTU_SIZE)
#else
#define MED_WAN_MTU          (1500)
#endif

/* This structre need to sync with SCP-side */
struct SCP_IRQ_AST_INFO {
	unsigned int scp_irq_ast_time;
	unsigned int scp_irq_ast_pc_s;
	unsigned int scp_irq_ast_pc_e;
	unsigned int scp_irq_ast_lr_s;
	unsigned int scp_irq_ast_lr_e;
	unsigned int scp_irq_ast_irqd;
};

#if (TEST_WITHOUT_CCCI == 0)
enum CCCI_DPMAIF_NOTIFY_EVENT {
	CCCI_DPMAIF_EVENT_READY = 0,  //CCCI init DPAMIF done
	CCCI_DPMAIF_EVENT_STOP,  // CCCI is going to close DPMAIF (in MD assert)
};
#endif

/* scp notify event */
enum SCP_NOTIFY_EVENT {
	SCP_EVENT_READY = 0,
	SCP_EVENT_STOP,
	SCP_EVENT_STOP_ACK,
	SCP_EVENT_STOP_BY_WDT,
};

/* reset ID */
#define SCP_ALL_ENABLE	0x00
#define SCP_ALL_REBOOT	0x01

/* scp semaphore definition*/
enum SEMAPHORE_FLAG {
	SEMAPHORE_CLK_CFG_5 = 0,
	SEMAPHORE_PTP,
	SEMAPHORE_I2C0,
	SEMAPHORE_I2C1,
	SEMAPHORE_TOUCH,
	SEMAPHORE_APDMA,
	SEMAPHORE_SENSOR,
	SEMAPHORE_SCP_A_AWAKE,
	SEMAPHORE_SCP_B_AWAKE,
	NR_FLAG = 9,
};

/* scp reset status */
enum SCP_RESET_STATUS {
	RESET_STATUS_STOP = 0,
	RESET_STATUS_START = 1,
};

/* scp reset status */
enum SCP_RESET_TYPE {
	RESET_TYPE_WDT = 0,
	RESET_TYPE_AWAKE = 1,
	RESET_TYPE_CMD = 2,
	RESET_TYPE_TIMEOUT = 3,
	RESET_TYPE_MD_EXCEP = 4,
	RESET_TYPE_MD_FORCE_STOP = 5,
	RESET_TYPE_RESUME = 6,
};

struct scp_regs {
	void __iomem *scpsys;
	void __iomem *sram;
	void __iomem *cfg;
	void __iomem *clkctrl;
	void __iomem *l1cctrl;
	void __iomem *cfg_core0;
	void __iomem *cfg_core1;
	void __iomem *cfg_sec;
	void __iomem *bus_tracker;
	void __iomem *infra_clk;
	int irq;
	unsigned int total_tcmsize;
	unsigned int cfgregsize;
	unsigned int scp_tcmsize;
};

/* scp work struct definition*/
struct scp_work_struct {
	struct work_struct work;
	unsigned int flags;
	unsigned int id;
};

/* scp reserve memory ID definition*/
#ifdef MED_V2
enum scp_reserve_mem_id_t {
	SCP_A_LOGGER_MEM_ID,
	SCP_A_TX_DESC_MEM_ID,
	SCP_A_RX_DESC_MEM_ID,
	SCP_A_RX_DESC_1_MEM_ID,
	SCP_A_HNAT_INFO_MEM_ID,
	SCP_A_HNAT_INFO_HOST_MEM_ID,
	SCP_A_PIT_NAT_MEM_ID,
	SCP_A_PIT_MEM_ID,
	SCP_A_DRB0_MEM_ID,
	SCP_A_DRB1_MEM_ID,
	SCP_A_DRB2_MEM_ID,
	NUMS_MEM_ID,
};
#else
enum scp_reserve_mem_id_t {
	SCP_A_LOGGER_MEM_ID,
	SCP_A_TX_DESC_MEM_ID,
	SCP_A_RX_DESC_MEM_ID,
	SCP_A_RX_DESC_DUP_MEM_ID,
	SCP_A_HNAT_INFO_MEM_ID,
	SCP_A_HNAT_INFO_HOST_MEM_ID,
	SCP_A_PIT_NAT_MEM_ID,
	SCP_A_PIT_MEM_ID,
	SCP_A_DRB0_MEM_ID,
	SCP_A_DRB1_MEM_ID,
	NUMS_MEM_ID,
};
#endif

struct scp_reserve_mblock {
	enum scp_reserve_mem_id_t num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};

#if TEST_WITHOUT_MODEM
enum scp_emu_reserve_mem_id_t {
#if TEST_WITHOUT_CCCI
	SCP_A_EMU_BAT_MEM_ID,
	SCP_A_EMU_FRAG_BAT_MEM_ID,
	SCP_A_EMU_BAT_SKB_MEM_ID,
	SCP_A_EMU_FRAG_BAT_SKB_MEM_ID,
#endif  // TEST_WITHOUT_CCCI
	SCP_A_TEMP_PIT_MEM_ID,
	NUMS_EMU_MEM_ID,
};

struct scp_emu_reserve_mblock {
	enum scp_emu_reserve_mem_id_t num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};
#endif

struct scp_region_info_st {
	uint32_t ap_loader_start;
	uint32_t ap_loader_size;
	uint32_t ap_firmware_start;
	uint32_t ap_firmware_size;
	uint32_t ap_medhw_gc0_pm_start;
	uint32_t ap_medhw_gc0_pm_size;
	uint32_t ap_medhw_gc0_dm_start;
	uint32_t ap_medhw_gc0_dm_size;
	uint32_t ap_medhw_gc1_pm_start;
	uint32_t ap_medhw_gc1_pm_size;
	uint32_t ap_medhw_gc1_dm_start;
	uint32_t ap_medhw_gc1_dm_size;
	uint32_t ap_medhw_gc2_pm_start;
	uint32_t ap_medhw_gc2_pm_size;
	uint32_t ap_medhw_gc2_dm_start;
	uint32_t ap_medhw_gc2_dm_size;
	/*	This is the size of the structure.
	 *	It can act as a version number if entries can only be
	 *	added to (not deleted from) the structure.
	 *	It should be the first entry of the structure, but for
	 *	compatibility reason, it is appended here.
	 */
	uint32_t struct_size;
	uint32_t scp_log_thru_ap_uart;
	uint32_t TaskContext_ptr;
	uint32_t scpctl;
	uint32_t regdump_start;
	uint32_t regdump_size;
};

//struct scp_region_info_st *scp_region_info;
/* shadow it due to sram may not access during sleep */
//struct scp_region_info_st scp_region_info_copy;

/* scp device attribute */
extern struct device_attribute dev_attr_scp_A_mobile_log_UT;
extern struct device_attribute dev_attr_scp_A_logger_wakeup_AP;
extern const struct file_operations medmcu_A_log_file_ops;

extern struct scp_regs scpreg;
extern struct device_attribute dev_attr_scp_A_mobile_log;
extern struct device_attribute dev_attr_scp_A_get_last_log;
extern struct device_attribute dev_attr_scp_A_status;
extern struct bin_attribute bin_attr_scp_dump;

/* scp loggger */
extern int scp_logger_init(phys_addr_t start, phys_addr_t limit);
extern void scp_logger_uninit(void);

extern void scp_logger_stop(void);
extern void scp_logger_cleanup(void);

extern ssize_t scp_A_log_read_to_kmesg(void);

/* scp exception */
int scp_excep_init(void);
void scp_ram_dump_init(void);
void scp_excep_cleanup(void);
void scp_reset_wait_timeout(void);

/* scp irq */
extern irqreturn_t scp_A_irq_handler(int irq, void *dev_id);
extern void scp_A_irq_init(void);

/* scp helper */
extern void scp_A_register_notify(struct notifier_block *nb);
extern void scp_A_unregister_notify(struct notifier_block *nb);
extern void scp_schedule_work(struct scp_work_struct *scp_ws);
extern void scp_schedule_logger_work(struct scp_work_struct *scp_ws);
extern int get_scp_semaphore(int flag);
extern int release_scp_semaphore(int flag);
extern int scp_get_semaphore_3way(int flag);
extern int scp_release_semaphore_3way(int flag);

extern void memcpy_to_scp(void __iomem *trg,
		const void *src, int size);
extern void memcpy_from_scp(void *trg, const void __iomem *src,
		int size);
extern int reset_scp(int reset);

extern phys_addr_t scp_get_reserve_mem_phys(enum scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_virt(enum scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_size(enum scp_reserve_mem_id_t id);
extern int scp_check_resource(void);
void set_scp_mpu(void);
extern phys_addr_t scp_mem_base_phys;
extern phys_addr_t scp_mem_base_virt;
extern phys_addr_t scp_mem_size;
extern atomic_t scp_reset_status;

extern void mbox_setup_pin_table(int mbox);
extern void mt_print_scp_ipi_id(unsigned int irq_no);
int scp_awake_lock(void *scp_id);
int scp_awake_unlock(void *scp_id);
int scp_clr_spm_reg(void *unused);
void scp_wdt_reset(int cpu_id);
#ifdef CONFIG_MTK_GIC_V3_EXT
extern u32 mt_irq_get_pending(unsigned int irq);
#endif

/*extern scp notify*/
extern void scp_send_reset_wq(enum SCP_RESET_TYPE type);
extern void scp_extern_notify(enum SCP_NOTIFY_EVENT notify_status);

extern void scp_status_set(unsigned int value);
extern int print_medhw_dram(struct seq_file *seq);
extern int print_medhw_cr_dump(struct seq_file *seq);
extern void clear_medhw_logged(void);
extern bool is_medhw_logged_ready(void);
extern void scp_logger_init_set(unsigned int value);
extern unsigned int scp_set_reset_status(void);
extern void scp_enable_sram(void);
extern int scp_sys_full_reset(void);
extern void scp_reset_awake_counts(void);
extern void scp_awake_init(void);

#if SCP_RECOVERY_SUPPORT
extern unsigned int scp_reset_by_cmd;
extern unsigned int scp_reset_by_md_excep;
extern struct scp_region_info_st scp_region_info_copy;
extern struct scp_region_info_st *scp_region_info;
extern void __iomem *scp_loader_virt;
extern void __iomem *scp_regdump_virt;
#endif

struct med_ipi_addr_msg {    // should be less than [SHARE_BUF_SIZE - 16] (48bytes) in scp_ipi.h
	uint32_t msg_id;
	uint32_t msg_data[17];  //tx_desc, rx_desc, hnat_info, hnat_info_host, pit_nat, pit, drb
} __packed;


enum med_msg_id {
	MED_MSG_INIT_START,
	MED_MSG_EMU_START,
};

/*DEFINITIONS AND MACROS*/
#define reg_read(phys)	(__raw_readl((void __iomem *)phys))
#define reg_write(phys, val)	(__raw_writel(val, (void __iomem *)phys))

#define phys_to_bus(a) (a)


#define MAX_RX_RING_NUM    4
struct MDMA_END_DEVICE {
	unsigned int tx_cpu_owner_idx0;
	/* PDMA TX  PTR */
	dma_addr_t phy_tx_ring0;

	unsigned int tx_dma_ptr;
	unsigned int tx_cpu_ptr;
	unsigned int tx_cpu_idx;

	/* struct sk_buff *skb_free[NUM_TX_DESC]; */
	struct sk_buff **skb_free;
	void **netrx_skb_data[MAX_RX_RING_NUM];

	struct MDMA_txdesc *tx_ring0;
	struct MDMA_rxdesc *rx_ring[MAX_RX_RING_NUM];
	dma_addr_t phy_rx_ring[MAX_RX_RING_NUM];

	/* struct sk_buff *netrx0_skbuf[NUM_RX_DESC]; */
	/*struct sk_buff **netrx0_skbuf;*/
	void **netrx0_skb_data;

	unsigned int tx_mask;
	unsigned int rx_mask;
};

struct MDMA_RXD_INFO1_T {
	unsigned int PDP0;
};

struct MDMA_RXD_INFO2_T {
	unsigned int PDP:4;
	unsigned int RSV1:4;
	unsigned int PLEN0:16;
	unsigned int RSV2:6;
	unsigned int LS0:1;
	unsigned int DDONE_bit:1;
};

struct MDMA_RXD_INFO3_T {
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

struct MDMA_RXD_INFO4_T {
	unsigned int VID:16;
	unsigned int VPID:16;
};

struct MDMA_RXD_INFO5_T {
	unsigned int FOE_ENTRY:15;
	unsigned int RSV0:3;
	unsigned int CRSN:5;
	unsigned int RSV1:3;
	unsigned int SP:4;
	unsigned int RSV2:2;
};

struct MDMA_RXD_INFO6_T {
	unsigned int LRO_FLUSH_RSN:3;
	unsigned int RSV0:2;
	unsigned int RSV1:11;
	unsigned int LRO_AGG_CNT:8;
	unsigned int RSV2:2;
	unsigned int RSV3:6;
};

struct MDMA_RXD_INFO7_T {
	unsigned int RSV;
};

struct MDMA_RXD_INFO8_T {
	unsigned int RSV;
};

struct MDMA_rxdesc {
	struct MDMA_RXD_INFO1_T rxd_info1;
	struct MDMA_RXD_INFO2_T rxd_info2;
	struct MDMA_RXD_INFO3_T rxd_info3;
	struct MDMA_RXD_INFO4_T rxd_info4;
	struct MDMA_RXD_INFO5_T rxd_info5;
	struct MDMA_RXD_INFO6_T rxd_info6;
	struct MDMA_RXD_INFO7_T rxd_info7;
	struct MDMA_RXD_INFO8_T rxd_info8;
};

/*=========================================
 *    PDMA TX Descriptor Format define
 *=========================================
 */

struct MDMA_TXD_INFO1_T {
	unsigned int SDP0;
};

struct MDMA_TXD_INFO2_T {
	unsigned int SDP:4;
	unsigned int RSV1:4;
	unsigned int SDL0:16;
	unsigned int RSV2:6;
	unsigned int LS0:1;
	unsigned int DDONE:1;
};

struct MDMA_TXD_INFO3_T {
    unsigned int SDP1;
};

struct MDMA_TXD_INFO4_T {
	unsigned int RSV0:6;
	unsigned int RSV1:2;
	unsigned int SDL1:16;
	unsigned int RSV2:6;
	unsigned int LS1:1;
	unsigned int BURST:1;
};

struct MDMA_TXD_INFO5_T {
	unsigned int RSV0:16;
	unsigned int FPORT:4;
	unsigned int RSV1:2;
	unsigned int RSV2:6;
	unsigned int TUI_CO:3;
	unsigned int TSO:1;
};

struct MDMA_TXD_INFO6_T {
	unsigned int PID_IDX_LSB:5;
	unsigned int RSV0:3;
	unsigned int PID_CNT:5;
	unsigned int RSV1:3;
	unsigned int TO_AP:1;
	unsigned int RSV2:15;
};

struct MDMA_TXD_INFO7_T {
	unsigned int FRAG_PID:16;
	unsigned int PID:16;
};

struct MDMA_TXD_INFO8_T {
	unsigned int RSV;
};

struct MDMA_txdesc {
	struct MDMA_TXD_INFO1_T txd_info1;
	struct MDMA_TXD_INFO2_T txd_info2;
	struct MDMA_TXD_INFO3_T txd_info3;
	struct MDMA_TXD_INFO4_T txd_info4;
	struct MDMA_TXD_INFO5_T txd_info5;
	struct MDMA_TXD_INFO6_T txd_info6;
	struct MDMA_TXD_INFO7_T txd_info7;
	struct MDMA_TXD_INFO8_T txd_info8;
};

#if SCP_RESERVED_MEM && defined(MED_V2)
struct medmcu_reserved_desc_mem_t {
    phys_addr_t phy_addr;
    uint32_t size;
};
#endif

#define FDMA_HNAT_INFO_ENTRY_SZ  8
#define NUM_RX_DESC              4200
#define NUM_TX_DESC              12150
#if defined(CONFIG_MTK_WAN_MTU_SIZE)
#define NEXT_MULTIPLE_OF_128(x) (((x) + 127) & ~127)
#define MAX_RX_LENGTH            NEXT_MULTIPLE_OF_128(MED_WAN_MTU + 14 + 12)
#else
#define MAX_RX_LENGTH            1536
#endif


#define MTK_FE_BASE       (0x15100000)
#define MTK_FE_RANGE      (0x20000)

#define LS_INT            BIT(31)

#define PST_DRX_IDX0      BIT(16)
#define PST_DTX_IDX0      BIT(0)

#define RX_DMA_BUSY       BIT(3)
#define TX_DMA_BUSY       BIT(1)

#define RX_DONE_INT0      BIT(16)
#define TX_DONE_INT0      BIT(0)

#ifdef MED_V2
#define PST_DRX_IDX1      BIT(17)
#define RX_DONE_INT1      BIT(17)
#define MDMA_SDL_ENABLE   BIT(16)
#endif

#define MTK_MEDHW_BASE       (0x15B10000)
#define MTK_MEDHW_RANGE      (0x40000)

#endif
