// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
//#include <mt-plat/sync_write.h>
//#include <mt-plat/aee.h>
#include <linux/delay.h>
#include <linux/syscore_ops.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/pm_wakeup.h>

#include "medmcu_helper.h"
#include "medmcu_common.h"
#include "medmcu_ipi_pin.h"
#include "medmcu_feature_define.h"
#include "medmcu_err_info.h"
#include "medmcu_excep.h"
#include "medmcu_scpctl.h"
#include "medmcu_dvfs.h"
#include "ccci_fsm.h"

#define MEDHW_BASE                  g_medmcu.medhw_base
#define MEDHW_INT_MNG_BASE          (MEDHW_BASE + 0x3000)
#define MEDHW_INT_EXCEP_SET         (MEDHW_INT_MNG_BASE + 0x08)

#define FE_BASE                     g_medmcu.fe_base

#ifdef MED_V2
#define FDMA_CRTL                   (FE_BASE + 0x124)  //no change
#define CDMM_EG_CRTL                (FE_BASE + 0x1644)

#define FDMA_HNAT_INFO_RX_BASE      (FE_BASE + 0x5900)
#define FDMA_HNAT_INFO_RX_MAX_CNT   (FE_BASE + 0x5904)
#define FDMA_RX_CRX_IDX_0           (FE_BASE + 0x5908)

#define FDMA_GLO_CFG                (FE_BASE + 0x5A04)
#define FDMA_INT_MASK               (FE_BASE + 0x5A28)

#define TX_BASE_PTR0                (FE_BASE + MDMA_RELATED + 0x000)
#define TX_MAX_CNT0                 (FE_BASE + MDMA_RELATED + 0x004)
#define TX_CTX_IDX0                 (FE_BASE + MDMA_RELATED + 0x008)


#define MDMA_RELATED                 0x5400  /*for MDMA*/
#define RX_BASE_PTR0                (FE_BASE + MDMA_RELATED + 0x100) //0x15105500
#define RX_MAX_CNT0                 (FE_BASE + MDMA_RELATED + 0x104)
#define RX_CALC_IDX0                (FE_BASE + MDMA_RELATED + 0x108)
#define RX_BASE_PTR1                (FE_BASE + MDMA_RELATED + 0x110) //0x15105510
#define RX_MAX_CNT1                 (FE_BASE + MDMA_RELATED + 0x114)
#define RX_CALC_IDX1                (FE_BASE + MDMA_RELATED + 0x118)
#define MDMA_GLO_CFG                (FE_BASE + MDMA_RELATED + 0x204)
#define MDMA_RST_IDX                (FE_BASE + MDMA_RELATED + 0x208)
#define MDMA_RST_CFG                (MDMA_RST_IDX)
#define INT_MASK                    (FE_BASE + MDMA_RELATED + 0x228)
#define MDMA_INT_GRP1               (FE_BASE + MDMA_RELATED + 0x250)
#define MDMA_SDL_CFG                (FE_BASE + MDMA_RELATED + 0x270)
#define MDMA_PREF_TX_CFG            (FE_BASE + MDMA_RELATED + 0x2D0)
#define MDMA_PREF_RX_CFG            (FE_BASE + MDMA_RELATED + 0x2DC)

#define MDMA_PREF_ENABLE            (0)
#else
#define FDMA_CRTL                   (FE_BASE + 0x124)

#define FDMA_HNAT_INFO_RX_BASE      (FE_BASE + 0x4500)
#define FDMA_HNAT_INFO_RX_MAX_CNT   (FE_BASE + 0x4504)
#define FDMA_RX_CRX_IDX_0           (FE_BASE + 0x4508)

#define FDMA_GLO_CFG                (FE_BASE + 0x4604)
#define FDMA_INT_MASK               (FE_BASE + 0x4628)

#define TX_BASE_PTR0                (FE_BASE + MDMA_RELATED + 0x000)
#define TX_MAX_CNT0                 (FE_BASE + MDMA_RELATED + 0x004)
#define TX_CTX_IDX0                 (FE_BASE + MDMA_RELATED + 0x008)


#define MDMA_RELATED                 0x4000  /*for MDMA*/
#define RX_BASE_PTR0                (FE_BASE + MDMA_RELATED + 0x100)
#define RX_MAX_CNT0                 (FE_BASE + MDMA_RELATED + 0x104)
#define RX_CALC_IDX0                (FE_BASE + MDMA_RELATED + 0x108)
#define MDMA_GLO_CFG                (FE_BASE + MDMA_RELATED + 0x204)
#define MDMA_RST_IDX                (FE_BASE + MDMA_RELATED + 0x208)
#define MDMA_RST_CFG                (MDMA_RST_IDX)
#define INT_MASK                    (FE_BASE + MDMA_RELATED + 0x228)
#define MDMA_INT_GRP1               (FE_BASE + MDMA_RELATED + 0x250)
#endif

struct medmcu_globals g_medmcu;

#if !defined(MED_V2) && defined(ENABLE_MEDMCU_SUSPEND_RESUME)
phys_addr_t gMedmcuRxDescDupPhyBase;
unsigned long long gMedmcuRxDescDupSize;
void __iomem *medmcu_rx_desc_base_dup_virt;
#endif

#if TEST_WITHOUT_MODEM
phys_addr_t scp_emu_mem_base_phys;
phys_addr_t scp_emu_mem_base_virt;
phys_addr_t scp_emu_mem_size;
#endif  // TEST_WITHOUT_MODEM

#if CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#include "medmcu_reservedmem_define.h"
#endif

#if ENABLE_SCP_EMI_PROTECTION
#include "soc/mediatek/emi.h"
#endif

/* scp mbox/ipi related */
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include "medmcu_ipi.h"

/* scp semaphore timeout count definition */
#define SEMAPHORE_TIMEOUT 5000
#define SEMAPHORE_3WAY_TIMEOUT 5000
/* scp ready timeout definition */
#define SCP_READY_TIMEOUT (8 * HZ) /* 30 seconds*/
#define SCP_A_TIMER 0

#define MAKE_DESC_INFO(_desc, _id)                                            \
	do {                                                                      \
		_desc.base_virt = (void __iomem *)scp_reserve_mblock[_id].start_virt; \
		_desc.sz = scp_reserve_mblock[_id].size;                              \
		_desc.phy_base = scp_reserve_mblock[_id].start_phys;                  \
	} while (0)

/* scp ipi message buffer */
uint32_t msg_scp_ready0, msg_scp_ready1;
char msg_scp_err_info0[40], msg_scp_err_info1[40];
char msg_med_stop_ack[8];

#if ENABLE_MEDMCU_SUSPEND_RESUME
char msg_query_suspend_response[8];
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */

/* scp ready status for notify*/
unsigned int scp_ready[SCP_CORE_TOTAL];

/* scp enable status*/
unsigned int scp_enable[SCP_CORE_TOTAL];

/* scp dvfs variable*/
unsigned int scp_expected_freq;
unsigned int scp_current_freq;

/*scp awake variable*/
int scp_awake_counts[SCP_CORE_TOTAL];


unsigned int scp_recovery_flag[SCP_CORE_TOTAL];
#define SCP_A_RECOVERY_OK	0x44
/*  scp_reset_status
 *  0: scp not in reset status
 *  1: scp in reset status
 */
atomic_t scp_reset_status = ATOMIC_INIT(RESET_STATUS_STOP);
unsigned int scp_reset_by_cmd;
unsigned int scp_reset_by_md_excep;
bool pended_dpmaif_ready;

int scp_wdt_reset_enable;
int scp_wdt_reset_timeout;

struct scp_region_info_st *scp_region_info;
/* shadow it due to sram may not access during sleep */
struct scp_region_info_st scp_region_info_copy;

struct scp_work_struct scp_sys_reset_work;
struct scp_work_struct scp_sys_delay_reset_work;
struct wakeup_source scp_reset_lock;

DEFINE_SPINLOCK(scp_reset_spinlock);
#if ENABLE_MEDMCU_SUSPEND_RESUME
static DEFINE_MUTEX(medmcu_resume_mutex);
#endif

void __iomem *scp_loader_virt;
void __iomem *scp_regdump_virt;

phys_addr_t scp_mem_base_phys;
phys_addr_t scp_mem_base_virt;
phys_addr_t scp_mem_size;
struct scp_regs scpreg;

unsigned char *scp_send_buff[SCP_CORE_TOTAL];
unsigned char *scp_recv_buff[SCP_CORE_TOTAL];

static struct workqueue_struct *scp_workqueue;

static struct workqueue_struct *scp_reset_workqueue;
static struct workqueue_struct *scp_delay_reset_workqueue;
static bool is_aee_enabled;

#if SCP_LOGGER_ENABLE
static struct workqueue_struct *scp_logger_workqueue;
#endif
#if SCP_BOOT_TIME_OUT_MONITOR
static struct timer_list scp_ready_timer[SCP_CORE_TOTAL];
static unsigned int scp_timeout_times;
#endif
static struct scp_work_struct scp_A_notify_work;

#if ENABLE_MEDMCU_SUSPEND_RESUME
/* represent the suspend response from medmcu, 0: no response, 1: idle, -1: can't suspend */
atomic_t medmcu_suspend_response;
/* represent the medmcu is ready for suspend query or not */
atomic_t medmcu_suspend_query_ready;
/* represent the medmcu needs dpmaif ready event after resume reset */
atomic_t medmcu_resume_need_dpmaif_ready;

struct timespec64 start_query_med_time;
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */
/* re-init mdma device structure */
struct MDMA_END_DEVICE mdma_device;

static DEFINE_MUTEX(scp_A_notify_mutex);
static DEFINE_MUTEX(scp_feature_mutex);
//static DEFINE_MUTEX(scp_register_sensor_mutex);

char *core_ids[SCP_CORE_TOTAL] = {"SCP A"};
DEFINE_SPINLOCK(scp_awake_spinlock);
/* set flag after driver initial done */
static bool driver_init_done;
struct scp_ipi_irq {
	const char *name;
	int order;
	unsigned int irq_no;
};

struct scp_ipi_irq scp_ipi_irqs[] = {
	/* MBOX_0 */
	{ "mediatek,medmcu", 0, 0},
	/* MBOX_1 */
	{ "mediatek,medmcu", 1, 0},
	/* MBOX_2 */
	{ "mediatek,medmcu", 2, 0},
	/* MBOX_3 */
	{ "mediatek,medmcu", 3, 0},
	/* MBOX_4 */
	{ "mediatek,medmcu", 4, 0},
};
#define IRQ_NUMBER  (sizeof(scp_ipi_irqs)/sizeof(struct scp_ipi_irq))

#if ENABLE_MEDMCU_SUSPEND_RESUME
/* used in resume call back function to check if netsys powerdown before */
static int is_fdma_poweroff_before(void)
{
	unsigned int reg_val = reg_read(FDMA_HNAT_INFO_RX_BASE);

	if (reg_val == 0)
		return 1;

	return 0;
}

static void handle_query_medmcu_can_suspend(void)
{
	int ret = 0;
	int cnt = 0;
	uint32_t dummy;

	pr_notice("[MEDMCU]Query medmcu suspend state!\n");
	while (1) {
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_QUERY_MED_SUSPEND, 1 /*IPI_SEND_POLLING */ ,
				&dummy, PIN_OUT_SIZE_QUERY_MED_SUSPEND, 0);
		if (ret != IPI_PIN_BUSY)
			break;
		cnt++;
		if (cnt > 10) {
			pr_err("[MEDMCU] IPI_OUT_QUERY_MED_SUSPEND send 10 times fail!\n");
			break;
		}
	}
	if (ret != IPI_ACTION_DONE) {
		pr_err("[MEDMCU] IPI_OUT_QUERY_MED_SUSPEND send fail!\n");
	} else {
		pr_notice("[MEDMCU] IPI_OUT_QUERY_MED_SUSPEND send ok!\n");
	}
}

static void notify_medmcu_unmask_irq(void)
{
	int ret = 0;
	int cnt = 0;
	uint32_t dummy;

	pr_notice("[MEDMCU]notify_medmcu_unmask_irq\n");
	while (1) {
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_MED_UNMASK_IRQ, 1 /*IPI_SEND_POLLING */ ,
				&dummy, PIN_OUT_SIZE_MED_UNMASK_IRQ, 0);
		if (ret != IPI_PIN_BUSY)
			break;
		cnt++;
		if (cnt > 10) {
			pr_err("[MEDMCU] IPI_OUT_MED_UNMASK_IRQ send 10 times fail!\n");
			break;
		}
	}
	if (ret != IPI_ACTION_DONE) {
		pr_err("[MEDMCU] IPI_OUT_MED_UNMASK_IRQ send fail!\n");
	} else {
		pr_notice("[MEDMCU] IPI_OUT_MED_UNMASK_IRQ send ok!\n");
	}
	atomic_set(&medmcu_suspend_query_ready, 1);
}
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */

static void medmcu_set_ignore_stop_ipi_flag(unsigned int flag)
{
	g_medmcu.ignore_stop_ipi_flag = flag;
}

static bool medmcu_ignore_stop_ipi(void)
{
	return (g_medmcu.ignore_stop_ipi_flag == 1);
}

struct medmcu_desc_info_t *get_desc_info(void)
{
	return &g_medmcu.desc[0];
}
EXPORT_SYMBOL_GPL(get_desc_info);

void __iomem *get_medhw_base(void)
{
	return g_medmcu.medhw_base;
}
EXPORT_SYMBOL_GPL(get_medhw_base);

static void fdma_init(struct net_device *dev)
{
	unsigned int fdma_entry_num;

	fdma_entry_num = (unsigned int)scp_reserve_mblock[SCP_A_HNAT_INFO_MEM_ID].size
						/ FDMA_HNAT_INFO_ENTRY_SZ;

	reg_write(FDMA_HNAT_INFO_RX_BASE, scp_reserve_mblock[SCP_A_HNAT_INFO_MEM_ID].start_phys);
	reg_write(FDMA_HNAT_INFO_RX_MAX_CNT, fdma_entry_num);
	reg_write(FDMA_GLO_CFG, 0x40002004);
	reg_write(FDMA_RX_CRX_IDX_0, fdma_entry_num - 1);

	//reg_write(FDMA_INT_GRP1, LS_INT);
	reg_write(FDMA_INT_MASK, LS_INT);

	//Enable UL no data-ack: 0x15100124[2] = 1
	//Let MSS controlled by CR, not by TCP checksum field: 0x15100124[28] = 1
	//Set MSS: 0x15100124[27:17] = MSS(1380)
	reg_write(FDMA_CRTL, reg_read(FDMA_CRTL) | (1 << 2) | (1 << 28) | (1380 << 17) | (1 << 12));
}

static int fe_pdma_wait_dma_idle(void)
{
	unsigned int reg_val;
	unsigned int loop_cnt = 0;

	while (1) {
		if (loop_cnt++ > 1000)
			break;
		reg_val = reg_read(MDMA_GLO_CFG);
		if ((reg_val & RX_DMA_BUSY)) {
			pr_info("\n  RX_DMA_BUSY !!! ");
			continue;
		}
		if ((reg_val & TX_DMA_BUSY)) {
			pr_info("\n  TX_DMA_BUSY !!! ");
			continue;
		}
		return 0;
	}

	return -1;
}

#if SCP_RESERVED_MEM && defined(MED_V2)
static int lk_medmcu_info_parse(unsigned int *raw_ptr)
{
	struct medmcu_reserved_desc_mem_t desc_rmem;

	memcpy((void *)&desc_rmem,
			raw_ptr,
			sizeof(struct medmcu_reserved_desc_mem_t));

	scp_mem_base_phys = desc_rmem.phy_addr;
	scp_mem_size = desc_rmem.size;
	pr_notice("scp_mem_base_phys = 0x%llx, size = %llu\n",
			scp_mem_base_phys,
			scp_mem_size);

	return (scp_mem_size > 0) ? 0: -1;
}

static int __init collect_lk_medmcu_reserved_desc(void)
{
	/* Device tree method */
	struct device_node *node = NULL;
	unsigned int *raw_ptr;

	node = of_find_compatible_node(NULL, NULL, "mediatek,medmcu");
	if (!node) {
		pr_info("no mediatek,medmcu device node\n");
		return -1;
	}

	raw_ptr = (unsigned int *)of_get_property(node,
                              "reserve_memory_medmcu_desc",
			                  NULL);
	if (raw_ptr != NULL)
		return lk_medmcu_info_parse(raw_ptr);

    return 0;
}
#endif /* #if SCP_RESERVED_MEM && defined(MED_V2) */

static inline void *raeth_alloc_skb_data(size_t size, gfp_t flags)
{
#ifdef CONFIG_ETH_SLAB_ALLOC_SKB
	return kmalloc(size, flags);
#else
	return netdev_alloc_frag(size);
#endif
}

#ifdef MED_V2
static int fe_pdma_rx_dma_init(struct net_device *dev,
	struct MDMA_END_DEVICE *ei_local)
{
	int i;
	int err;
	unsigned int skb_size;
	//struct MDMA_END_DEVICE *ei_local = netdev_priv(dev);
	dma_addr_t dma_addr;

	//Set 0x15101664, TACK2RING_EN: bit1, TACK2RING_ID: bit2. Set both bit1 and bit2 to 1
	//bit2: Ring selection of TCP ACK to dedicate Rx Ring
	reg_write(CDMM_EG_CRTL, reg_read(CDMM_EG_CRTL) | (0x6));
	pr_info("CDMM_EG_CRTL=0x%x\n", reg_read(CDMM_EG_CRTL));
	pr_notice("[MDMA]MAX_RX_LENGTH = %d\n", MAX_RX_LENGTH);
	if (dev != NULL) {
		skb_size = SKB_DATA_ALIGN(MAX_RX_LENGTH + NET_IP_ALIGN + NET_SKB_PAD) +
			   SKB_DATA_ALIGN(sizeof(struct skb_shared_info));

		ei_local->phy_rx_ring[0] = scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].start_phys;
		ei_local->rx_ring[0] = (void __iomem *)scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].start_virt;
		ei_local->phy_rx_ring[1] = scp_reserve_mblock[SCP_A_RX_DESC_1_MEM_ID].start_phys;
		ei_local->rx_ring[1] = (void __iomem *)scp_reserve_mblock[SCP_A_RX_DESC_1_MEM_ID].start_virt;

		pr_info("phy_rx_ring[0] = 0x%08x, rx_ring[0] = 0x%p, skb_size = %u, rx ring size = %ld, medmcu rx desc size = %llu\n",
			 (unsigned int)ei_local->phy_rx_ring[0],
			 (void *)ei_local->rx_ring[0], skb_size, NUM_RX_DESC * sizeof(struct MDMA_rxdesc),
			 scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].size);
		pr_info("phy_rx_ring[1] = 0x%08x, rx_ring[1] = 0x%p, skb_size = %u, rx ring size = %ld, medmcu rx desc size = %llu\n",
			 (unsigned int)ei_local->phy_rx_ring[1],
			 (void *)ei_local->rx_ring[1], skb_size, NUM_RX_DESC * sizeof(struct MDMA_rxdesc),
			 scp_reserve_mblock[SCP_A_RX_DESC_1_MEM_ID].size);
		for (i = 0; i < MAX_RX_RING_NUM; i++)
			ei_local->netrx_skb_data[i] =
				kmalloc_array(NUM_RX_DESC, sizeof(void *), GFP_KERNEL);

		for (i = 0; i < NUM_RX_DESC; i++) {
			ei_local->netrx_skb_data[0][i] =
				raeth_alloc_skb_data(skb_size, GFP_KERNEL);
			ei_local->netrx_skb_data[1][i] =
				raeth_alloc_skb_data(skb_size, GFP_KERNEL);
			if (!ei_local->netrx_skb_data[0][i]) {
				pr_info("rx skbuff buffer allocation failed!");
				goto no_rx_mem;
			}
			memset(&ei_local->rx_ring[0][i], 0, sizeof(struct MDMA_rxdesc));
			ei_local->rx_ring[0][i].rxd_info2.DDONE_bit = 0;
			ei_local->rx_ring[0][i].rxd_info2.LS0 = 0;
			ei_local->rx_ring[0][i].rxd_info2.PLEN0 = MAX_RX_LENGTH;
			dma_addr = dma_map_single(dev->dev.parent,
						  ei_local->netrx_skb_data[0][i] +
						  NET_SKB_PAD,
						  MAX_RX_LENGTH,
						  DMA_FROM_DEVICE);
			ei_local->rx_ring[0][i].rxd_info1.PDP0 = dma_addr;
			ei_local->rx_ring[0][i].rxd_info2.PDP = (dma_addr >> 32);
			err = dma_mapping_error(dev->dev.parent, ei_local->rx_ring[0][i].rxd_info1.PDP0);
			if (unlikely(err)) {
				pr_info("[%s] rx_desc entry_%d dma_map_single() failed...\n", __func__, i);
				goto no_rx_mem;
			}

			memset(&ei_local->rx_ring[1][i], 0, sizeof(struct MDMA_rxdesc));
			ei_local->rx_ring[1][i].rxd_info2.DDONE_bit = 0;
			ei_local->rx_ring[1][i].rxd_info2.LS0 = 0;
			ei_local->rx_ring[1][i].rxd_info2.PLEN0 = MAX_RX_LENGTH;
			dma_addr = dma_map_single(dev->dev.parent,
						  ei_local->netrx_skb_data[1][i] +
						  NET_SKB_PAD,
						  MAX_RX_LENGTH,
						  DMA_FROM_DEVICE);
			ei_local->rx_ring[1][i].rxd_info1.PDP0 = dma_addr;
			ei_local->rx_ring[1][i].rxd_info2.PDP = (dma_addr >> 32);
			err = dma_mapping_error(dev->dev.parent, ei_local->rx_ring[1][i].rxd_info1.PDP0);
			if (unlikely(err)) {
				pr_info("[%s] rx_desc_1 entry_%d dma_map_single() failed...\n", __func__, i);
				goto no_rx_mem;
			}
		}
	} else
		pr_info("[MEDMCU] skip allocate mdma skb data");

	/* Tell the adapter where the RX rings are located. */
	reg_write(RX_BASE_PTR0, phys_to_bus((u32)ei_local->phy_rx_ring[0]));
	reg_write(RX_MAX_CNT0, cpu_to_le32((u32)NUM_RX_DESC));
	reg_write(RX_CALC_IDX0, cpu_to_le32((u32)(NUM_RX_DESC - 1)));

	reg_write(RX_BASE_PTR1, phys_to_bus((u32)ei_local->phy_rx_ring[1]));
	reg_write(RX_MAX_CNT1, cpu_to_le32((u32)NUM_RX_DESC));
	reg_write(RX_CALC_IDX1, cpu_to_le32((u32)(NUM_RX_DESC - 1)));

	reg_write(MDMA_RST_CFG, PST_DRX_IDX0|PST_DRX_IDX1/*bit:17*/);   //base+208 : MDMA_RST_IDX


	reg_write(MDMA_INT_GRP1, RX_DONE_INT0|RX_DONE_INT1);  //base+250 : MDMA_INT_GRP1
	reg_write(INT_MASK, RX_DONE_INT0|RX_DONE_INT1/*bit:17*/);  //base+228 : MDMA_INT_MASK

	reg_write(MDMA_SDL_CFG, MDMA_SDL_ENABLE | MAX_RX_LENGTH);  //base+270 : MDMA_SDL_CFG

	reg_write(MDMA_PREF_RX_CFG, (reg_read(MDMA_PREF_RX_CFG) | (1 << MDMA_PREF_ENABLE)) & (~(1 << 3))); //enable rx prefetech

	return 0;

no_rx_mem:
	return -ENOMEM;
}
#else
static int fe_pdma_rx_dma_init(struct net_device *dev,
	struct MDMA_END_DEVICE *ei_local)
{
	int i;
	int err;
	unsigned int skb_size;
	//struct MDMA_END_DEVICE *ei_local = netdev_priv(dev);
	dma_addr_t dma_addr;
	pr_notice("[MDMA]MAX_RX_LENGTH = %d\n", MAX_RX_LENGTH);
	if (dev != NULL) {
		skb_size = SKB_DATA_ALIGN(MAX_RX_LENGTH + NET_IP_ALIGN + NET_SKB_PAD) +
			   SKB_DATA_ALIGN(sizeof(struct skb_shared_info));

		ei_local->phy_rx_ring[0] = scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].start_phys;
		ei_local->rx_ring[0] = (void __iomem *)scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].start_virt;

		pr_info("phy_rx_ring[0] = 0x%08x, rx_ring[0] = 0x%p, skb_size = %u, rx ring size = %ld, medmcu rx desc size = %llu\n",
			 (unsigned int)ei_local->phy_rx_ring[0],
			 (void *)ei_local->rx_ring[0], skb_size, NUM_RX_DESC * sizeof(struct MDMA_rxdesc),
			 scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].size);
		for (i = 0; i < MAX_RX_RING_NUM; i++)
			ei_local->netrx_skb_data[i] =
				kmalloc_array(NUM_RX_DESC, sizeof(void *), GFP_KERNEL);

		for (i = 0; i < NUM_RX_DESC; i++) {
			ei_local->netrx_skb_data[0][i] =
				raeth_alloc_skb_data(skb_size, GFP_KERNEL);
			if (!ei_local->netrx_skb_data[0][i]) {
				pr_info("rx skbuff buffer allocation failed!");
				goto no_rx_mem;
			}
			memset(&ei_local->rx_ring[0][i], 0, sizeof(struct MDMA_rxdesc));
			ei_local->rx_ring[0][i].rxd_info2.DDONE_bit = 0;
			ei_local->rx_ring[0][i].rxd_info2.LS0 = 0;
			ei_local->rx_ring[0][i].rxd_info2.PLEN0 = MAX_RX_LENGTH;
			dma_addr = dma_map_single(dev->dev.parent,
						  ei_local->netrx_skb_data[0][i] +
						  NET_SKB_PAD,
						  MAX_RX_LENGTH,
						  DMA_FROM_DEVICE);
			ei_local->rx_ring[0][i].rxd_info1.PDP0 = dma_addr;
			err = dma_mapping_error(dev->dev.parent, ei_local->rx_ring[0][i].rxd_info1.PDP0);
			if (unlikely(err)) {
				pr_info("[%s] rx_desc entry_%d dma_map_single() failed...\n", __func__, i);
				goto no_rx_mem;
			}
		}
	} else
		pr_info("[MEDMCU] skip allocate mdma skb data");


	/* Tell the adapter where the RX rings are located. */
	reg_write(RX_BASE_PTR0, phys_to_bus((u32)ei_local->phy_rx_ring[0]));
	reg_write(RX_MAX_CNT0, cpu_to_le32((u32)NUM_RX_DESC));
	reg_write(RX_CALC_IDX0, cpu_to_le32((u32)(NUM_RX_DESC - 1)));

	reg_write(MDMA_RST_CFG, PST_DRX_IDX0);


	reg_write(MDMA_INT_GRP1, RX_DONE_INT0);
	reg_write(INT_MASK, RX_DONE_INT0);

	return 0;

no_rx_mem:
	return -ENOMEM;
}
#endif

static int fe_pdma_tx_dma_init(struct net_device *dev,
	struct MDMA_END_DEVICE *ei_local)
{
	int i;
	//struct MDMA_END_DEVICE *ei_local = netdev_priv(dev);

	ei_local->phy_tx_ring0 = scp_reserve_mblock[SCP_A_TX_DESC_MEM_ID].start_phys;
	ei_local->tx_ring0 = (void __iomem *)scp_reserve_mblock[SCP_A_TX_DESC_MEM_ID].start_virt;
	pr_info("phy_tx_ring = 0x%08x, tx_ring = 0x%p\n",
		 (unsigned int)ei_local->phy_tx_ring0,
		 (void *)ei_local->tx_ring0);
	for (i = 0; i < NUM_TX_DESC; i++) {
		memset(&ei_local->tx_ring0[i], 0, sizeof(struct MDMA_txdesc));
		ei_local->tx_ring0[i].txd_info2.LS0 = 1;
		ei_local->tx_ring0[i].txd_info2.DDONE = 1;
		ei_local->tx_ring0[i].txd_info5.FPORT = 4;
	}
	/* Tell the adapter where the TX rings are located. */
	reg_write(TX_BASE_PTR0, phys_to_bus((u32)ei_local->phy_tx_ring0));
	reg_write(TX_MAX_CNT0, cpu_to_le32((u32)NUM_TX_DESC));
	reg_write(TX_CTX_IDX0, 0);
	reg_write(MDMA_RST_CFG, PST_DTX_IDX0);
#ifdef MED_V2
	reg_write(MDMA_PREF_TX_CFG, (reg_read(MDMA_PREF_TX_CFG) | (1 << MDMA_PREF_ENABLE)) & (~(1 << 3))); //enable rx prefetech
#endif
	return 0;
}

static void set_fe_pdma_glo_cfg(void)
{
	//Disable check DDONE bit by 0x15104204[10]=0
	//Enable DMA check WDONE bit by 0x15104204[28]=1
	//Disable 2-byte offset by 0x15104204[31]=0
	//Disable TX_DMA writing back DDONE into TXD: 0x15104204[6]=0
	reg_write(MDMA_GLO_CFG, 0x10804825);
}

static int mdma_init(struct net_device *dev, struct MDMA_END_DEVICE *ei_local)
{
	int err;

	err = fe_pdma_wait_dma_idle();
	if (err) {
		pr_info("fe_pdma_wait_dma_idle err\n");
		return err;
	}

	err = fe_pdma_rx_dma_init(dev, ei_local);
	if (err) {
		pr_info("fe_pdma_rx_dma_init err\n");
		//return err;
	}

	err = fe_pdma_tx_dma_init(dev, ei_local);
	if (err) {
		pr_info("fe_pdma_tx_dma_init err\n");
		//return err;
	}

	set_fe_pdma_glo_cfg();

	return 0;
}

static int scp_ipi_syscore_dbg_suspend(void) { return 0; }
static void scp_ipi_syscore_dbg_resume(void)
{
	int i;
	int ret = 0;

	/*skip 0 because scp_ipi_irqs[0] is used for watch dog event*/
	for (i = 0; i < IRQ_NUMBER; i++) {
#ifdef CONFIG_MTK_GIC_V3_EXT
		ret = mt_irq_get_pending(scp_ipi_irqs[i].irq_no);
#endif
		if (ret) {
			if (i < 1)
				pr_info("[SCP] watch dog %d wakeup\n", i);
			else
				mt_print_scp_ipi_id(i);
			break;
		}
	}
}

/*
 * memory copy to scp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void memcpy_to_scp(void __iomem *trg, const void *src, int size)
{
	int i;
	u32 __iomem *t = trg;
	const u32 *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}


/*
 * memory copy from scp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void memcpy_from_scp(void *trg, const void __iomem *src, int size)
{
	int i;
	u32 *t = trg;
	const u32 __iomem *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}

/*
 * acquire a hardware semaphore
 * @param flag: semaphore id
 * return  1 :get sema success
 *        -1 :get sema timeout
 */
int get_scp_semaphore(int flag)
{
	int read_back;
	int count = 0;
	int ret = -1;
	unsigned long spin_flags;

	/* return 1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return -1;

	/* spinlock context safe*/
	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);

	flag = (flag * 2) + 1;

	read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 0) {
		writel((1 << flag), SCP_SEMAPHORE);

		while (count != SEMAPHORE_TIMEOUT) {
			/* repeat test if we get semaphore */
			read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;
			if (read_back == 1) {
				ret = 1;
				break;
			}
			writel((1 << flag), SCP_SEMAPHORE);
			count++;
		}

		if (ret < 0)
			pr_debug("[MEDMCU] get scp sema. %d TIMEOUT...!\n", flag);
	} else {
		pr_debug("[MEDMCU] already hold scp sema. %d\n", flag);
	}

	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(get_scp_semaphore);

#if ENABLE_MEDMCU_SUSPEND_RESUME
/* suspend/resume for index sync */
extern void ccci_dpmaif_reset_pit_nat_indexes(void);
extern void ccci_dpmaif_resend_ready(void);
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return  1 :release sema success
 *        -1 :release sema fail
 */
int release_scp_semaphore(int flag)
{
	int read_back;
	int ret = -1;
	unsigned long spin_flags;

	/* return 1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return -1;

	/* spinlock context safe*/
	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	flag = (flag * 2) + 1;

	read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 1) {
		/* Write 1 clear */
		writel((1 << flag), SCP_SEMAPHORE);
		read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;
		if (read_back == 0)
			ret = 1;
		else
			pr_debug("[MEDMCU] release scp sema. %d failed\n", flag);
	} else {
		pr_debug("[MEDMCU] try to release sema. %d not own by me\n", flag);
	}

	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(release_scp_semaphore);

#if (TEST_WITHOUT_CCCI == 0)
extern void ccci_dpmaif_register_notify(struct notifier_block *nb);

static void handle_dpmaif_ready(void)
{
	int ret = 0;
	int cnt = 0;
	struct med_ipi_addr_msg ipi_data;
	uint32_t non_support_len_to_med = MED_WAN_MTU; // mdma tso length
	uint32_t *msg_data = ipi_data.msg_data;

	pended_dpmaif_ready = false;

	pr_notice("[MEDMCU]Receive CCCI_DPMAIF_EVENT_READY!, tso length=%d\n", non_support_len_to_med);

	memset((void *)&ipi_data, 0, sizeof(ipi_data));
	ipi_data.msg_id = MED_MSG_INIT_START;
	msg_data[0] = (uint32_t) scp_reserve_mblock[SCP_A_TX_DESC_MEM_ID].start_phys;
	msg_data[1] = (uint32_t) scp_reserve_mblock[SCP_A_TX_DESC_MEM_ID].size;
	msg_data[2] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].start_phys;
	msg_data[3] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].size;
	msg_data[4] = (uint32_t) scp_reserve_mblock[SCP_A_HNAT_INFO_MEM_ID].start_phys;
	msg_data[5] = (uint32_t) scp_reserve_mblock[SCP_A_HNAT_INFO_MEM_ID].size;
	msg_data[6] = (uint32_t) scp_reserve_mblock[SCP_A_HNAT_INFO_HOST_MEM_ID].start_phys;
	msg_data[7] = (uint32_t) scp_reserve_mblock[SCP_A_HNAT_INFO_HOST_MEM_ID].size;
	msg_data[8] = (uint32_t) scp_reserve_mblock[SCP_A_PIT_NAT_MEM_ID].start_phys;
	msg_data[9] = (uint32_t) scp_reserve_mblock[SCP_A_PIT_NAT_MEM_ID].size;
	msg_data[10] = (uint32_t) scp_reserve_mblock[SCP_A_PIT_MEM_ID].start_phys;
	msg_data[11] = (uint32_t) scp_reserve_mblock[SCP_A_PIT_MEM_ID].size;
	msg_data[12] = (uint32_t) scp_reserve_mblock[SCP_A_DRB0_MEM_ID].start_phys;
	msg_data[13] = (uint32_t) scp_reserve_mblock[SCP_A_DRB0_MEM_ID].size +
				   scp_reserve_mblock[SCP_A_DRB1_MEM_ID].size;
#ifdef MED_V2
	msg_data[14] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_1_MEM_ID].start_phys;
	msg_data[15] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_1_MEM_ID].size;
	msg_data[16] = (uint32_t) non_support_len_to_med;
#else
	msg_data[14] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_DUP_MEM_ID].start_phys;
	msg_data[15] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_DUP_MEM_ID].size;
	msg_data[16] = (uint32_t) non_support_len_to_med;
#endif
	while (1) {
		/*IPI_SEND_POLLING */
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_MED_TABLE_ADDR, 1,
				&ipi_data, PIN_OUT_SIZE_MED_TABLE_ADDR, 1);
		if (ret != IPI_PIN_BUSY)
			break;
		cnt++;
		if (cnt > 10) {
			pr_err("[MEDMCU] IPI_OUT_MED_TABLE_ADDR send 10 times fail!\n");
			break;
		}
	}
	if (ret != IPI_ACTION_DONE)
		pr_err("[MEDMCU] IPI_OUT_MED_TABLE_ADDR send fail!\n");
	else
		pr_notice("[MEDMCU] IPI_OUT_MED_TABLE_ADDR send ok!\n");

	pr_info("hnat_info_rx_base = 0x%08x, hnat_info_rx_max_cnt = %d, fdma_glo_cfg = 0x%08x, fdma_rx_crx_idx_0 = 0x%08x, fdma_ctrl = 0x%08x, fdma_int_mask  = 0x%08x\n",
			(unsigned int)reg_read(FDMA_HNAT_INFO_RX_BASE),
			(unsigned int)reg_read(FDMA_HNAT_INFO_RX_MAX_CNT),
			(unsigned int)reg_read(FDMA_GLO_CFG),
			(unsigned int)reg_read(FDMA_RX_CRX_IDX_0),
			(unsigned int)reg_read(FDMA_CRTL),
			(unsigned int)reg_read(FDMA_INT_MASK));

	pr_info("rx_base = 0x%08x, rx_max_cnt0 = %d, rx_calc_idx0 = 0x%08x, mdma_rst_cfg = 0x%08x, mdma_int_grp1 = 0x%08x, int_mask  = 0x%08x\n",
			(unsigned int)reg_read(RX_BASE_PTR0),
			(unsigned int)reg_read(RX_MAX_CNT0),
			(unsigned int)reg_read(RX_CALC_IDX0),
			(unsigned int)reg_read(MDMA_RST_CFG),
			(unsigned int)reg_read(MDMA_INT_GRP1),
			(unsigned int)reg_read(INT_MASK));

	pr_info("tx_base = 0x%08x, tx_max_cnt0 = %d, tx_ctx_idx0 = 0x%08x, mdma_rst_cfg = 0x%08x\n",
			(unsigned int)reg_read(TX_BASE_PTR0),
			(unsigned int)reg_read(TX_MAX_CNT0),
			(unsigned int)reg_read(TX_CTX_IDX0),
			(unsigned int)reg_read(MDMA_RST_CFG));

#if ENABLE_MEDMCU_SUSPEND_RESUME
	atomic_set(&medmcu_suspend_query_ready, 1);
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */
}

static void handle_dpmaif_stop(void)
{
	int ret = 0;
	int cnt = 0;
	uint32_t dummy;

	pr_notice("[MEDMCU]Receive CCCI_DPMAIF_EVENT_STOP!\n");

	while (1) {
		/* IPI_SEND_POLLING */
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_MED_STOP, 1,
				&dummy, PIN_OUT_SIZE_MED_STOP, 0);
		if (ret != IPI_PIN_BUSY)
			break;
		cnt++;
		if (cnt > 10) {
			pr_err("[MEDMCU] IPI_OUT_MED_STOP send 10 times fail!\n");
			break;
		}
	}
	if (ret != IPI_ACTION_DONE)
		pr_err("[MEDMCU] IPI_OUT_MED_STOP send fail!\n");
	else
		pr_notice("[MEDMCU] IPI_OUT_MED_STOP send ok!\n");
}

static int ccci_apsync_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	switch (event) {
	case CCCI_DPMAIF_EVENT_STOP:
		if (!is_scp_ready(SCP_A_ID)) {
			pr_notice("[MEDMCU] %s(): scp_extern_notify SCP_EVENT_STOP_ACK\n",
					  __func__);
			scp_extern_notify(SCP_EVENT_STOP_ACK);
		} else if (medmcu_ignore_stop_ipi()) {
			medmcu_set_ignore_stop_ipi_flag(0);
			pr_notice("[MEDMCU] %s(): scp_extern_notify SCP_EVENT_STOP_ACK\n",
				__func__);
			scp_extern_notify(SCP_EVENT_STOP_ACK);
		} else
			handle_dpmaif_stop();
		break;
	case CCCI_DPMAIF_EVENT_READY:
		if (!is_scp_ready(SCP_A_ID)) {
			pr_notice("[MEDMCU] Pending CCCI_DPMAIF_EVENT_READY since SCP not ready!\n");
			pended_dpmaif_ready = true;
		} else
			handle_dpmaif_ready();
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block ccci_apsync_notifier = {
	.notifier_call = ccci_apsync_event,
};
#endif

static BLOCKING_NOTIFIER_HEAD(scp_A_notifier_list);
/*
 * register apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:   notifier block struct
 */
void scp_A_register_notify(struct notifier_block *nb)
{
	mutex_lock(&scp_A_notify_mutex);
	blocking_notifier_chain_register(&scp_A_notifier_list, nb);

	pr_debug("[MEDMCU] register scp A notify callback..\n");

	if (is_scp_ready(SCP_A_ID))
		nb->notifier_call(nb, SCP_EVENT_READY, NULL);
	mutex_unlock(&scp_A_notify_mutex);
}
EXPORT_SYMBOL_GPL(scp_A_register_notify);


/*
 * unregister apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:     notifier block struct
 */
void scp_A_unregister_notify(struct notifier_block *nb)
{
	mutex_lock(&scp_A_notify_mutex);
	blocking_notifier_chain_unregister(&scp_A_notifier_list, nb);
	mutex_unlock(&scp_A_notify_mutex);
}
EXPORT_SYMBOL_GPL(scp_A_unregister_notify);


void scp_schedule_work(struct scp_work_struct *scp_ws)
{
	queue_work(scp_workqueue, &scp_ws->work);
}

static void scp_schedule_reset_work(struct scp_work_struct *scp_ws)
{
	queue_work(scp_reset_workqueue, &scp_ws->work);
}

static void scp_schedule_delay_reset_work(struct scp_work_struct *scp_ws)
{
	queue_work(scp_delay_reset_workqueue, &scp_ws->work);
}


#if SCP_LOGGER_ENABLE
void scp_schedule_logger_work(struct scp_work_struct *scp_ws)
{
	queue_work(scp_logger_workqueue, &scp_ws->work);
}
#endif

/*
 * callback function for work struct
 * notify apps to start their tasks
 * or generate an exception according to flag
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void scp_A_notify_ws(struct work_struct *ws)
{
#if ENABLE_MEDMCU_SUSPEND_RESUME
	//unsigned long spin_flags;
#endif
	struct scp_work_struct *sws =
		container_of(ws, struct scp_work_struct, work);
	unsigned int scp_notify_flag = sws->flags;


	if (scp_notify_flag) {
		scp_recovery_flag[SCP_A_ID] = SCP_A_RECOVERY_OK;

		if (pended_dpmaif_ready) {
#if (TEST_WITHOUT_CCCI == 0)
			handle_dpmaif_ready();
#endif
		}

		mutex_lock(&scp_A_notify_mutex);

#if SCP_RECOVERY_SUPPORT
		atomic_set(&scp_reset_status, RESET_STATUS_STOP);
#endif
		scp_ready[SCP_A_ID] = 1;
		pr_debug("[MEDMCU] notify blocking call\n");
		blocking_notifier_call_chain(&scp_A_notifier_list
			, SCP_EVENT_READY, NULL);
		mutex_unlock(&scp_A_notify_mutex);
#if ENABLE_MEDMCU_SUSPEND_RESUME
		mutex_lock(&medmcu_resume_mutex);
		/* for resume medmcu, need dpamif ready event. */
		if (atomic_read(&medmcu_resume_need_dpmaif_ready) == 1) {
			/*
			 * ask ccci to send dpmaif ready since
             * dpmaif hw/sw is still alive after resume
			 */
			ccci_dpmaif_resend_ready();
			atomic_set(&medmcu_resume_need_dpmaif_ready, 0);
		}
		mutex_unlock(&medmcu_resume_mutex);
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */
	}

	/* register scp dvfs*/
#if SCP_DVFS_INIT_ENABLE
	msleep(2000);
	__pm_relax(&scp_reset_lock);
	scp_register_feature(RTOS_FEATURE_ID);
#endif

}

/*
 * mark notify flag to 1 to notify apps to start their tasks
 */
static void scp_A_set_ready(void)
{
#if (TEST_WITHOUT_MODEM || TEST_WITHOUT_CCCI)
	int ret = 0;
	int cnt = 0;
	struct med_ipi_addr_msg ipi_data;
	uint32_t *msg_data = ipi_data.msg_data;
#endif  // TEST_WITHOUT_MODEM

	pr_notice("[MEDMCU] %s()\n", __func__);

#if SCP_BOOT_TIME_OUT_MONITOR
	del_timer(&scp_ready_timer[SCP_A_ID]);
#endif
	scp_A_notify_work.flags = 1;
	scp_schedule_work(&scp_A_notify_work);

#if TEST_WITHOUT_MODEM
	memset((void *)&ipi_data, 0, sizeof(ipi_data));
	ipi_data.msg_id = MED_MSG_EMU_START;
    // TODO
#if TEST_WITHOUT_CCCI
	msg_data[0] = (uint32_t) scp_emu_reserve_mblock[SCP_A_EMU_BAT_MEM_ID].start_phys;
	msg_data[1] = (uint32_t) scp_emu_reserve_mblock[SCP_A_EMU_BAT_MEM_ID].size;
	msg_data[2] = (uint32_t) scp_emu_reserve_mblock[SCP_A_EMU_FRAG_BAT_MEM_ID].start_phys;
	msg_data[3] = (uint32_t) scp_emu_reserve_mblock[SCP_A_EMU_FRAG_BAT_MEM_ID].size;
	msg_data[4] = (uint32_t) scp_emu_reserve_mblock[SCP_A_EMU_BAT_SKB_MEM_ID].start_phys;
	msg_data[5] = (uint32_t) scp_emu_reserve_mblock[SCP_A_EMU_BAT_SKB_MEM_ID].size;
	msg_data[6] = (uint32_t) scp_emu_reserve_mblock[SCP_A_EMU_FRAG_BAT_SKB_MEM_ID].start_phys;
	msg_data[7] = (uint32_t) scp_emu_reserve_mblock[SCP_A_EMU_FRAG_BAT_SKB_MEM_ID].size;
#else
    // TODO: need to get table physical address from CCCI
	msg_data[0] = 0;
	msg_data[1] = 0;
	msg_data[2] = 0;
	msg_data[3] = 0;
	msg_data[4] = 0;
	msg_data[5] = 0;
	msg_data[6] = 0;
	msg_data[7] = 0;
#endif  // TEST_WITHOUT_CCCI
	msg_data[8] = (uint32_t) scp_reserve_mblock[SCP_A_PIT_MEM_ID].start_phys;
	msg_data[9] = (uint32_t) scp_reserve_mblock[SCP_A_PIT_MEM_ID].size;
	msg_data[10] = (uint32_t) scp_emu_reserve_mblock[SCP_A_TEMP_PIT_MEM_ID].start_phys;
	msg_data[11] = (uint32_t) scp_emu_reserve_mblock[SCP_A_TEMP_PIT_MEM_ID].size;
	while (1) {
		/*IPI_SEND_POLLING */
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_MED_TABLE_ADDR, 1,
				&ipi_data, PIN_OUT_SIZE_MED_TABLE_ADDR, 1);
		if (ret != IPI_PIN_BUSY)
			break;
		cnt++;
		if (cnt > 10) {
			pr_err("[MEDMCU] IPI_OUT_MED_TABLE_ADDR send 10 times fail!\n");
			break;
		}
	}
	if (ret != IPI_ACTION_DONE)
		pr_err("[MEDMCU] IPI_OUT_MED_TABLE_ADDR send fail!\n");
	else
		pr_notice("[MEDMCU] IPI_OUT_MED_TABLE_ADDR send ok!\n");
#endif  // TEST_WITHOUT_MODEM

#if TEST_WITHOUT_CCCI
	memset((void *)&ipi_data, 0, sizeof(ipi_data));
	ipi_data.msg_id = MED_MSG_INIT_START;
	msg_data[0] = (uint32_t) scp_reserve_mblock[SCP_A_TX_DESC_MEM_ID].start_phys;
	msg_data[1] = (uint32_t) scp_reserve_mblock[SCP_A_TX_DESC_MEM_ID].size;
	msg_data[2] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].start_phys;
	msg_data[3] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].size;
	msg_data[4] = (uint32_t) scp_reserve_mblock[SCP_A_HNAT_INFO_MEM_ID].start_phys;
	msg_data[5] = (uint32_t) scp_reserve_mblock[SCP_A_HNAT_INFO_MEM_ID].size;
	msg_data[6] = (uint32_t) scp_reserve_mblock[SCP_A_HNAT_INFO_HOST_MEM_ID].start_phys;
	msg_data[7] = (uint32_t) scp_reserve_mblock[SCP_A_HNAT_INFO_HOST_MEM_ID].size;
	msg_data[8] = (uint32_t) scp_reserve_mblock[SCP_A_PIT_NAT_MEM_ID].start_phys;
	msg_data[9] = (uint32_t) scp_reserve_mblock[SCP_A_PIT_NAT_MEM_ID].size;
	msg_data[10] = (uint32_t) scp_reserve_mblock[SCP_A_PIT_MEM_ID].start_phys;
	msg_data[11] = (uint32_t) scp_reserve_mblock[SCP_A_PIT_MEM_ID].size;
	msg_data[12] = (uint32_t) scp_reserve_mblock[SCP_A_DRB0_MEM_ID].start_phys;
	msg_data[13] = (uint32_t) scp_reserve_mblock[SCP_A_DRB0_MEM_ID].size +
				   scp_reserve_mblock[SCP_A_DRB1_MEM_ID].size;
#ifdef MED_V2
	msg_data[14] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_1_MEM_ID].start_phys;
	msg_data[15] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_1_MEM_ID].size;
	/*msg_data[16] = (uint32_t) scp_reserve_mblock[SCP_A_DRB2_MEM_ID].start_phys;*/  //may be no used
#else
	msg_data[14] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_DUP_MEM_ID].start_phys;
	msg_data[15] = (uint32_t) scp_reserve_mblock[SCP_A_RX_DESC_DUP_MEM_ID].size;
#endif

	while (1) {
		/*IPI_SEND_POLLING */
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_MED_TABLE_ADDR, 1,
				&ipi_data, PIN_OUT_SIZE_MED_TABLE_ADDR, 1);
		if (ret != IPI_PIN_BUSY)
			break;
		cnt++;
		if (cnt > 10) {
			pr_err("[MEDMCU] IPI_OUT_MED_TABLE_ADDR send 10 times fail!\n");
			break;
		}
	}
	if (ret != IPI_ACTION_DONE)
		pr_err("[MEDMCU] IPI_OUT_MED_TABLE_ADDR send fail!\n");
	else
		pr_notice("[MEDMCU] IPI_OUT_MED_TABLE_ADDR send ok!\n");

	pr_info("hnat_info_rx_base = 0x%08x, hnat_info_rx_max_cnt = %d, fdma_glo_cfg = 0x%08x, fdma_rx_crx_idx_0 = 0x%08x, fdma_ctrl = 0x%08x, fdma_int_mask  = 0x%08x\n",
			(unsigned int)reg_read(FDMA_HNAT_INFO_RX_BASE),
			(unsigned int)reg_read(FDMA_HNAT_INFO_RX_MAX_CNT),
			(unsigned int)reg_read(FDMA_GLO_CFG),
			(unsigned int)reg_read(FDMA_RX_CRX_IDX_0),
			(unsigned int)reg_read(FDMA_CRTL),
			(unsigned int)reg_read(FDMA_INT_MASK));

	pr_info("rx_base = 0x%08x, rx_max_cnt0 = %d, rx_calc_idx0 = 0x%08x, mdma_rst_cfg = 0x%08x, mdma_int_grp1 = 0x%08x, int_mask  = 0x%08x\n",
			(unsigned int)reg_read(RX_BASE_PTR0),
			(unsigned int)reg_read(RX_MAX_CNT0),
			(unsigned int)reg_read(RX_CALC_IDX0),
			(unsigned int)reg_read(MDMA_RST_CFG),
			(unsigned int)reg_read(MDMA_INT_GRP1),
			(unsigned int)reg_read(INT_MASK));

	pr_info("tx_base = 0x%08x, tx_max_cnt0 = %d, tx_ctx_idx0 = 0x%08x, mdma_rst_cfg = 0x%08x\n",
			(unsigned int)reg_read(TX_BASE_PTR0),
			(unsigned int)reg_read(TX_MAX_CNT0),
			(unsigned int)reg_read(TX_CTX_IDX0),
			(unsigned int)reg_read(MDMA_RST_CFG));
#endif  // TEST_WITHOUT_CCCI

}

/*
 * callback for reset timer
 * mark notify flag to 0 to generate an exception
 * @param data: unuse
 */
#if SCP_BOOT_TIME_OUT_MONITOR
static void scp_wait_ready_timeout(unsigned long data)
{
#if SCP_RECOVERY_SUPPORT
	if (scp_timeout_times < 10)
		scp_send_reset_wq(RESET_TYPE_TIMEOUT);
#endif
	scp_timeout_times++;
	pr_notice("[MEDMCU] scp_timeout_times=%x\n", scp_timeout_times);
}
#endif

/*
 * handle notification from scp
 * mark scp is ready for running tasks
 * It is important to call scp_ram_dump_init() in this IPI handler. This
 * timing is necessary to ensure that the region_info has been initialized.
 * @param id:   ipi id
 * @param prdata: ipi handler parameter
 * @param data: ipi data
 * @param len:  length of ipi data
 */
static int scp_A_ready_ipi_handler(unsigned int id, void *prdata, void *data,
				    unsigned int len)
{
	unsigned int scp_image_size = *(unsigned int *)data;

	if (!scp_ready[SCP_A_ID])
		scp_A_set_ready();

	/*verify scp image size*/
	if (scp_image_size != SCP_A_TCM_SIZE) {
		pr_err("[MEDMCU]image size ERROR! AP=0x%x,SCP=0x%x\n",
					SCP_A_TCM_SIZE, scp_image_size);
		WARN_ON(1);
	}

	pr_debug("[MEDMCU] ramdump init\n");
	scp_ram_dump_init();

	return 0;
}

/*
 * handle notification from scp
 * mark med is already stop
 * @param id:   ipi id
 * @param prdata: ipi handler parameter
 * @param data: ipi data
 * @param len:  length of ipi data
 */
static int med_stop_ack_ipi_handler(unsigned int id, void *prdata, void *data,
				    unsigned int len)
{
	int md_force_stop;

	scp_reset_counts = 9999;
	md_force_stop = ccci_fsm_get_md_force_stop_state();

	if (md_force_stop == 1) {
		pr_notice("[MEDMCU] %s(): scp_extern_notify SCP_EVENT_STOP_ACK, md force stop state = %d\n",
					  __func__, md_force_stop);
		scp_send_reset_wq(RESET_TYPE_MD_FORCE_STOP);
	} else {
		scp_reset_by_md_excep = 1;
		scp_send_reset_wq(RESET_TYPE_MD_EXCEP);
	}

	return 0;
}

#if ENABLE_MEDMCU_SUSPEND_RESUME
static int sync_medmcu_can_suspend(void) {
	struct timespec64 cur_med_driver_time, diff_med_time;
	uint32_t medmcu_core0_sta;
	uint32_t cnt = 100;

	if (scp_ready[SCP_A_ID] && atomic_read(&medmcu_suspend_query_ready) == 1) {
		atomic_set(&medmcu_suspend_response, 0);
		ktime_get_boottime_ts64(&start_query_med_time);
		handle_query_medmcu_can_suspend();
		/* wait handshake with scp */
		while (1) {
			ktime_get_boottime_ts64(&cur_med_driver_time);
			diff_med_time = timespec64_sub(cur_med_driver_time, start_query_med_time);
			if (atomic_read(&medmcu_suspend_response) != 0)
				break;
			if (timespec64_to_ns(&diff_med_time) > 5000000000) {
				pr_err("[MEDMCU] suspend query timeout\n");
				return -1;
			}
		}
		if (atomic_read(&medmcu_suspend_response) == 1) {
			/* scp idle */
			atomic_set(&medmcu_suspend_query_ready, 0);
			while (cnt--) {
				medmcu_core0_sta = reg_read(R_CORE0_STATUS);
				if (medmcu_core0_sta == 0x3) {
					pr_notice("medmcu is at WFI state\n");
					return 0;
                }
				mdelay(10);
			}
			pr_notice("medmcu is not at WFI state, medmcu_core0_sta = 0x%x\n",
					medmcu_core0_sta);
			return -1;
		} else {
			return -1;
		}
	} else {
		pr_info("[MEDMCU] scp not ready in suspend, %d, %d\n",
				scp_ready[SCP_A_ID], atomic_read(&medmcu_suspend_query_ready));
		return -1;
	}
}

/*
 * handle notification from scp
 * Check if scp idle or not
 * @param id:   ipi id
 * @param prdata: ipi handler parameter
 * @param data: ipi data
 * @param len:  length of ipi data
 */
static int med_query_suspend_response_ipi_handler(unsigned int id, void *prdata, void *data,
				    unsigned int len)
{
	unsigned int response = *(unsigned int *)data;

	pr_notice("[MEDMCU] query response %u\n", response);
	if (response == 0) {
		/* scp idle */
		atomic_set(&medmcu_suspend_response, 1);
	} else {
		atomic_set(&medmcu_suspend_response, -1);
	}
	return 0;
}
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */

/*
 * Handle notification from scp.
 * Report error from SCP to other kernel driver.
 * @param id:   ipi id
 * @param prdata: ipi handler parameter
 * @param data: ipi data
 * @param len:  length of ipi data
 */
static void scp_err_info_handler(int id, void *prdata, void *data,
				 unsigned int len)
{
	struct error_info *info = (struct error_info *)data;

	if (sizeof(*info) != len) {
		pr_notice("[MEDMCU] error: incorrect size %d of error_info\n",
				len);
		WARN_ON(1);
		return;
	}

	/* Ensure the context[] is terminated by the NULL character. */
	info->context[ERR_MAX_CONTEXT_LEN - 1] = '\0';
	pr_notice("[MEDMCU] Error_info: case id: %u\n", info->case_id);
	pr_notice("[MEDMCU] Error_info: sensor id: %u\n", info->sensor_id);
	pr_notice("[MEDMCU] Error_info: context: %s\n", info->context);

	/*if (report_hub_dmd)
		report_hub_dmd(info->case_id, info->sensor_id, info->context);
	else
		pr_debug("[MEDMCU] warning: report_hub_dmd() not defined.\n");*/
}

/*
 * @return: 1 if scp is ready for running tasks
 */
unsigned int is_scp_ready(enum scp_core_id id)
{
	if (scp_ready[id])
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL_GPL(is_scp_ready);

/*
 * 1. reset scp and create a timer waiting for scp notify
 * 2. notify apps to stop their tasks if needed
 * 3. generate error if reset fail
 * NOTE: this function may be blocked
 *       and should not be called in interrupt context
 * @param reset:    bit[0-3]=0 for scp enable, =1 for reboot
 *                  bit[4-7]=0 for All, =1 for scp_A, =2 for scp_B
 * @return:         0 if success
 */
int reset_scp(int reset)
{
	struct arm_smccc_res ares;
	uint64_t rst_time;

	mutex_lock(&scp_A_notify_mutex);
	blocking_notifier_call_chain(&scp_A_notifier_list, SCP_EVENT_STOP,
		NULL);
	mutex_unlock(&scp_A_notify_mutex);

	if (reset & 0x0f) { /* do reset */
		/* make sure scp is in idle state */
		scp_reset_wait_timeout();
	}

	if (scp_enable[SCP_A_ID]) {
		/* write scp reserved memory address/size to GRP1/GRP2
		 * to let scp setup MPU
		 */
#if SCP_RESERVED_MEM
		arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
			MTK_SIP_MEDMCU_GPR_RESV_MEM,
			scp_mem_base_phys, scp_mem_size, 0, 0, 0, 0,
			&ares);
#endif
		rst_time = (uint64_t)ktime_get();
		arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
			MTK_SIP_MEDMCU_RSTN_CLR,
			rst_time, 0, 0, 0, 0, 0,
			&ares);

#if SCP_BOOT_TIME_OUT_MONITOR
		scp_ready_timer[SCP_A_ID].expires = jiffies + SCP_READY_TIMEOUT;
		add_timer(&scp_ready_timer[SCP_A_ID]);
#endif
	}

	pr_notice("[MEDMCU] %s: done\n", __func__);

	return 0;
}

/*
 * TODO: what should we do when hibernation ?
 */
static int scp_pm_event(struct notifier_block *notifier
			, unsigned long pm_event, void *unused)
{
	int retval;

	switch (pm_event) {
	case PM_POST_HIBERNATION:
		pr_debug("[MEDMCU] %s: reboot\n", __func__);
		retval = reset_scp(SCP_ALL_REBOOT);
		if (retval < 0) {
			retval = -EINVAL;
			pr_err("[MEDMCU] %s: reboot fail\n", __func__);
		}
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block scp_pm_notifier_block = {
	.notifier_call = scp_pm_event,
	.priority = 0,
};

static inline ssize_t scp_A_status_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	if (scp_ready[SCP_A_ID])
		return scnprintf(buf, PAGE_SIZE, "SCP A is ready\n");
	else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR_RO(scp_A_status);

static inline ssize_t scp_A_reg_status_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	int len = 0;

//	scp_dump_last_regs();

	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_status = %08x\n", c0_m.status);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_pc = %08x\n", c0_m.pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_lr = %08x\n", c0_m.lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_sp = %08x\n", c0_m.sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_pc_latch = %08x\n", c0_m.pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_lr_latch = %08x\n", c0_m.lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_sp_latch = %08x\n", c0_m.sp_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_status = %08x\n", c1_m.status);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_pc = %08x\n", c1_m.pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_lr = %08x\n", c1_m.lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_sp = %08x\n", c1_m.sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_pc_latch = %08x\n", c1_m.pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_lr_latch = %08x\n", c1_m.lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_sp_latch = %08x\n", c1_m.sp_latch);
	return len;
}

DEVICE_ATTR_RO(scp_A_reg_status);

static inline ssize_t scp_A_db_test_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666) {
			scp_aed(RESET_TYPE_CMD, SCP_A_ID);
			if (scp_ready[SCP_A_ID])
				pr_debug("dumping SCP db\n");
			else
				pr_debug("SCP is not ready, try to dump EE\n");
		}
	}

	return count;
}

DEVICE_ATTR_WO(scp_A_db_test);

static ssize_t scp_wdt_reset_enable_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", scp_wdt_reset_enable);
}

static ssize_t scp_wdt_reset_enable_store(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		scp_wdt_reset_enable = value;
		pr_debug("[MEDMCU] scp_wdt_reset_enable = %d(1:enable, 0:disable)\n"
				, scp_wdt_reset_enable);
#if SCP_RECOVERY_SUPPORT
		if (atomic_read(&scp_reset_status) == RESET_STATUS_START) {
			scp_reset_by_cmd = 1;
			scp_send_reset_wq(RESET_TYPE_WDT);
		}
#endif
	}
	return n;
}
DEVICE_ATTR_RW(scp_wdt_reset_enable);

static ssize_t scp_wdt_reset_timeout_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", scp_wdt_reset_timeout);
}

static ssize_t scp_wdt_reset_timeout_store(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		scp_wdt_reset_timeout = value;
		pr_debug("[MEDMCU] scp_wdt_reset_timeout = %d\n", scp_wdt_reset_timeout);
	}
	return n;
}
DEVICE_ATTR_RW(scp_wdt_reset_timeout);

#ifdef CONFIG_MTK_ENG_BUILD
static ssize_t scp_ee_enable_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", scp_ee_enable);
}

static ssize_t scp_ee_enable_store(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		scp_ee_enable = value;
		pr_debug("[MEDMCU] scp_ee_enable = %d(1:enable, 0:disable)\n"
				, scp_ee_enable);
	}
	return n;
}
DEVICE_ATTR_RW(scp_ee_enable);

enum ipi_debug_opt {
	IPI_TRACKING_OFF,
	IPI_TRACKING_ON,
	IPIMON_SHOW,
};

static inline ssize_t scp_ipi_test_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned int value = 0x5A5A;

	if (scp_ready[SCP_A_ID]) {
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_TEST_0, 0, &value,
				   PIN_OUT_SIZE_TEST_0, 0);

		return scnprintf(buf, PAGE_SIZE,
				"SCP A ipi send ret=%d\n", ret);
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

static inline ssize_t scp_ipi_test_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int opt;

	if (kstrtouint(buf, 10, &opt) != 0)
		return -EINVAL;

	switch (opt) {
	case IPI_TRACKING_ON:
	case IPI_TRACKING_OFF:
		mtk_ipi_tracking(&scp_ipidev, opt);
		break;
	case IPIMON_SHOW:
		ipi_monitor_dump(&scp_ipidev);
		break;
	default:
		pr_info("cmd '%d' is not supported.\n", opt);
		break;
	}

	return n;
}

DEVICE_ATTR_RW(scp_ipi_test);

#endif

#if SCP_RECOVERY_SUPPORT
void scp_wdt_reset(int cpu_id)
{
	struct arm_smccc_res ares;

	arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
		MTK_SIP_MEDMCU_WDT_SET,
		cpu_id, 0, 0, 0, 0, 0,
		&ares);
}
EXPORT_SYMBOL(scp_wdt_reset);

#if ENABLE_MEDMCU_SUSPEND_RESUME
static void reset_scp_without_restart_md(void) {
	int is_netsys_poweroff_before = is_fdma_poweroff_before();
	struct MDMA_END_DEVICE *ei_local = &mdma_device;
#ifndef MED_V2
	struct medmcu_desc_info_t *desc_info = get_desc_info();
#endif
	pr_info("[MEDMCU] reset_scp_without_restart_md\n");
	/* reset ccci pit_nat indexes */
	ccci_dpmaif_reset_pit_nat_indexes();

	/* reinit fdma, mdma */
	if (is_netsys_poweroff_before == 1) {
		pr_info("[MEDMCU] reset_scp_without_restart_md: reset f_mdma\n");
		/* copy dup rx_desc value prevent re-allocate skb data */
	#ifndef MED_V2
		memcpy(desc_info[RX_DESC].base_virt, medmcu_rx_desc_base_dup_virt,
				desc_info[RX_DESC].sz);
	#endif
		fdma_init(NULL);
		mdma_init(NULL, ei_local);
	}

	/* reset medmcu */
	scp_reset_counts = 9999;
	scp_reset_by_md_excep = 1;
	scp_send_reset_wq(RESET_TYPE_RESUME);
}
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */

/*
 * trigger wdt manually (debug use)
 * Warning! watch dog may be refresh just after you set
 */
static ssize_t scp_wdt_reset_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;
	pr_debug("[MEDMCU] %s: %8s\n", __func__, buf);
	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666)
			scp_wdt_reset(0);
		else if (value == 667)
			scp_wdt_reset(1);
	}
	return count;
}

DEVICE_ATTR_WO(scp_wdt_reset_test);


/*
 * trigger wdt manually (debug use)
 * Warning! watch dog may be refresh just after you set
 */
static ssize_t medhw_ee_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	int i;
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;
	pr_debug("[MEDMCU] %s: %8s\n", __func__, buf);
	if (kstrtouint(buf, 10, &value) == 0) {
		for (i = 0; i < 32; i++) {
			if ((1 << i) == (value & (1 << i))) {  //check event bit is set
				pr_notice("[MEDMCU] %s:  %d-th MEDHW_INT_EXCEP_W1C bit is set\n",
						  __func__, i);
				writel(1 << i, MEDHW_INT_EXCEP_SET);
			}
		}
	}
	return count;
}

DEVICE_ATTR_WO(medhw_ee);

/*
 * trigger scp reset manually (debug use)
 */
static ssize_t scp_reset_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t n)
{
	int magic, trigger, counts;

	if (sscanf(buf, "%d %d %d", &magic, &trigger, &counts) != 3)
		return -EINVAL;
	pr_notice("%s %d %d %d\n", __func__, magic, trigger, counts);

	if (magic != 666)
		return -EINVAL;

	scp_reset_counts = counts;
	if (trigger == 1) {
		scp_reset_by_cmd = 1;
		scp_send_reset_wq(RESET_TYPE_CMD);
	}
	return n;
}

DEVICE_ATTR_WO(scp_reset);
/*
 * trigger wdt manually
 * debug use
 */

static ssize_t recovery_flag_show(struct device *dev
			, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", scp_recovery_flag[SCP_A_ID]);
}
static ssize_t recovery_flag_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, tmp;

	ret = kstrtoint(buf, 10, &tmp);
	if (kstrtoint(buf, 10, &tmp) < 0) {
		pr_err("scp_recovery_flag error\n");
		return count;
	}
	scp_recovery_flag[SCP_A_ID] = tmp;
	return count;
}

DEVICE_ATTR_RW(recovery_flag);

#endif


/******************************************************************************
 *****************************************************************************/
static ssize_t scp_log_filter_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	uint32_t filter;

	if (sscanf(buf, "0x%08x", &filter) != 1)
		return -EINVAL;

	ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_SCP_LOG_FILTER_1, 0, &filter,
			   PIN_OUT_SIZE_SCP_LOG_FILTER_1, 0);

	switch (ret) {
	case IPI_ACTION_DONE:
		pr_notice("[MEDMCU] Set log filter to 0x%08x\n", filter);
		return count;

	case IPI_PIN_BUSY:
		pr_notice("[MEDMCU] IPI busy. Set log filter failed!\n");
		return -EBUSY;

	default:
		pr_notice("[MEDMCU] IPI error. Set log filter failed!\n");
		return -EIO;
	}
}
DEVICE_ATTR_WO(scp_log_filter);


/******************************************************************************
 *****************************************************************************/
static struct miscdevice scp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "medmcu",
	.fops = &medmcu_A_log_file_ops
};


/*
 * register /dev and /sys files
 * @return:     0: success, otherwise: fail
 */
static int create_files(void)
{
	int ret;

	ret = misc_register(&scp_device);
	if (unlikely(ret != 0)) {
		pr_err("[MEDMCU] misc register failed\n");
		return ret;
	}

#if SCP_LOGGER_ENABLE
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_mobile_log);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_logger_wakeup_AP);
	if (unlikely(ret != 0))
		return ret;

#ifdef CONFIG_MTK_ENG_BUILD
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_mobile_log_UT);
	if (unlikely(ret != 0))
		return ret;
#endif  // CONFIG_MTK_ENG_BUILD

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_get_last_log);
	if (unlikely(ret != 0))
		return ret;
#endif  // SCP_LOGGER_ENABLE

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_status);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_bin_file(scp_device.this_device
					, &bin_attr_scp_dump);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_reg_status);
	if (unlikely(ret != 0))
		return ret;

	/*only support debug db test in engineer build*/
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_db_test);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_wdt_reset_enable);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_wdt_reset_timeout);
	if (unlikely(ret != 0))
		return ret;

#ifdef CONFIG_MTK_ENG_BUILD
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_ee_enable);
	if (unlikely(ret != 0))
		return ret;

	/* SCP IPI Debug sysfs*/
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_ipi_test);
	if (unlikely(ret != 0))
		return ret;
#endif  // CONFIG_MTK_ENG_BUILD

#if SCP_RECOVERY_SUPPORT
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_wdt_reset_test);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_medhw_ee);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_reset);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_recovery_flag);
	if (unlikely(ret != 0))
		return ret;
#endif

	ret = device_create_file(scp_device.this_device, &dev_attr_scp_log_filter);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scpctl);

	if (unlikely(ret != 0))
		return ret;

	return 0;
}

#if SCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM)
#ifndef MED_V2
#define SCP_MEM_RESERVED_KEY "mediatek,reserve-memory-medmcu_share"
int scp_reserve_mem_of_init(struct reserved_mem *rmem)
{
	pr_notice("[MEDMCU]%s %pa %pa\n", __func__, &rmem->base, &rmem->size);

	scp_mem_base_phys = (phys_addr_t) rmem->base;
	scp_mem_size = (phys_addr_t) rmem->size;

	pr_notice("[MEDMCU]%s 0x%llx 0x%llx\n", __func__,
			(uint64_t)scp_mem_base_phys, (uint64_t)scp_mem_size);

	return 0;
}

RESERVEDMEM_OF_DECLARE(scp_reserve_mem_init
			, SCP_MEM_RESERVED_KEY, scp_reserve_mem_of_init);
#endif /* #ifndef MED_V2 */

#if TEST_WITHOUT_MODEM
#define SCP_EMU_MEM_RESERVED_KEY "mediatek,reserve-memory-medmcu_emu_share"
int scp_emu_reserve_mem_of_init(struct reserved_mem *rmem)
{
	pr_notice("[MEDMCU]%s %pa %pa\n", __func__, &rmem->base, &rmem->size);

	scp_emu_mem_base_phys = (phys_addr_t) rmem->base;
	scp_emu_mem_size = (phys_addr_t) rmem->size;

	pr_notice("[MEDMCU]%s 0x%llx 0x%llx\n", __func__,
			(uint64_t)scp_emu_mem_base_phys, (uint64_t)scp_emu_mem_size);

	return 0;
}

RESERVEDMEM_OF_DECLARE(scp_emu_reserve_mem_init
			, SCP_EMU_MEM_RESERVED_KEY, scp_emu_reserve_mem_of_init);
#endif  // TEST_WITHOUT_MODEM
#endif

phys_addr_t scp_get_reserve_mem_phys(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[MEDMCU] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_phys);

phys_addr_t scp_get_reserve_mem_virt(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[MEDMCU] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_virt);

phys_addr_t scp_get_reserve_mem_size(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[MEDMCU] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_size);

#if SCP_RESERVED_MEM
static int scp_reserve_memory_ioremap(void)
{
	unsigned int num = (unsigned int)(sizeof(scp_reserve_mblock)
			/ sizeof(scp_reserve_mblock[0]));
	enum scp_reserve_mem_id_t id;
	phys_addr_t accumlate_memory_size = 0;

	if ((scp_mem_base_phys <= 0x0)) {
		/* The scp remapped region is fixed, only
		 * 0x4000_0000ULL ~ 0x8FFF_FFFFULL is accessible.
		 */
		pr_err("[MEDMCU] Error: Wrong Address (0x%llx)\n",
			    (uint64_t)scp_mem_base_phys);
		//BUG_ON(1);
		return -1;
	}

	if (num != NUMS_MEM_ID) {
		pr_err("[MEDMCU] number of entries of reserved memory %u / %u\n",
			num, NUMS_MEM_ID);
		//BUG_ON(1);
		return -1;
	}

	scp_mem_base_virt = (phys_addr_t)(size_t)ioremap_wc(scp_mem_base_phys,
		scp_mem_size);

	pr_debug("[MEDMCU] rsrv_phy_base = 0x%llx, len:0x%llx\n",
		(uint64_t)scp_mem_base_phys, (uint64_t)scp_mem_size);

	pr_debug("[MEDMCU] rsrv_vir_base = 0x%llx, len:0x%llx\n",
		(uint64_t)scp_mem_base_virt, (uint64_t)scp_mem_size);

	for (id = 0; id < NUMS_MEM_ID; id++) {
		scp_reserve_mblock[id].start_phys = scp_mem_base_phys +
			accumlate_memory_size;
		scp_reserve_mblock[id].start_virt = scp_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += scp_reserve_mblock[id].size;

#ifdef DEBUG
		pr_debug("[MEDMCU] [%d] phys:0x%llx, virt:0x%llx, len:0x%llx\n",
			id, (uint64_t)scp_reserve_mblock[id].start_phys,
			(uint64_t)scp_reserve_mblock[id].start_virt,
			(uint64_t)scp_reserve_mblock[id].size);
#endif
	}

	memset((void __iomem *)scp_mem_base_virt, 0, scp_mem_size);

	MAKE_DESC_INFO(g_medmcu.desc[TX_DESC], SCP_A_TX_DESC_MEM_ID);
	MAKE_DESC_INFO(g_medmcu.desc[RX_DESC], SCP_A_RX_DESC_MEM_ID);
	MAKE_DESC_INFO(g_medmcu.desc[HNAT_INFO], SCP_A_HNAT_INFO_MEM_ID);
	MAKE_DESC_INFO(g_medmcu.desc[HNAT_INFO_HOST], SCP_A_HNAT_INFO_HOST_MEM_ID);
	MAKE_DESC_INFO(g_medmcu.desc[PIT], SCP_A_PIT_MEM_ID);
	MAKE_DESC_INFO(g_medmcu.desc[PIT_NAT], SCP_A_PIT_NAT_MEM_ID);
	MAKE_DESC_INFO(g_medmcu.desc[DRB0], SCP_A_DRB0_MEM_ID);
	MAKE_DESC_INFO(g_medmcu.desc[DRB1], SCP_A_DRB1_MEM_ID);
#ifdef MED_V2
	MAKE_DESC_INFO(g_medmcu.desc[DRB2], SCP_A_DRB2_MEM_ID);
	MAKE_DESC_INFO(g_medmcu.desc[RX_DESC_1], SCP_A_RX_DESC_1_MEM_ID);
#endif

#if !defined(MED_V2) && defined(ENABLE_MEDMCU_SUSPEND_RESUME)
	/* Medmcu Resume: copy dup to rxdesc address */
	gMedmcuRxDescDupPhyBase = scp_reserve_mblock[SCP_A_RX_DESC_DUP_MEM_ID].start_phys;
	medmcu_rx_desc_base_dup_virt = (void __iomem *)scp_reserve_mblock[SCP_A_RX_DESC_DUP_MEM_ID].start_virt;
	gMedmcuRxDescDupSize = scp_reserve_mblock[SCP_A_RX_DESC_DUP_MEM_ID].size;
#endif

	pr_notice("[MEDMCU] logger:0x%llx(0x%llx), TxDesc:0x%llx(0x%llx), RxDesc:0x%llx(0x%llx), HnatInfo:0x%llx(0x%llx)\n",
			  scp_reserve_mblock[SCP_A_LOGGER_MEM_ID].start_phys,
			  scp_reserve_mblock[SCP_A_LOGGER_MEM_ID].size,
			  scp_reserve_mblock[SCP_A_TX_DESC_MEM_ID].start_phys,
			  scp_reserve_mblock[SCP_A_TX_DESC_MEM_ID].size,
			  scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].start_phys,
			  scp_reserve_mblock[SCP_A_RX_DESC_MEM_ID].size,
			  scp_reserve_mblock[SCP_A_HNAT_INFO_MEM_ID].start_phys,
			  scp_reserve_mblock[SCP_A_HNAT_INFO_MEM_ID].size);

#ifdef MED_V2
	pr_notice("[MEDMCU] HnatInfoHost:0x%llx(0x%llx), PitNat:0x%llx(0x%llx), Pit:0x%llx(0x%llx), Drb0:0x%llx(0x%llx), Drb1:0x%llx(0x%llx), Drb2:0x%llx(0x%llx), RxDesc1:0x%llx(0x%llx)\n",
			   scp_reserve_mblock[SCP_A_HNAT_INFO_HOST_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_HNAT_INFO_HOST_MEM_ID].size,
			   scp_reserve_mblock[SCP_A_PIT_NAT_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_PIT_NAT_MEM_ID].size,
			   scp_reserve_mblock[SCP_A_PIT_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_PIT_MEM_ID].size,
			   scp_reserve_mblock[SCP_A_DRB0_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_DRB0_MEM_ID].size,
			   scp_reserve_mblock[SCP_A_DRB1_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_DRB1_MEM_ID].size,
			   scp_reserve_mblock[SCP_A_DRB2_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_DRB2_MEM_ID].size,
			   scp_reserve_mblock[SCP_A_RX_DESC_1_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_RX_DESC_1_MEM_ID].size);
#else
	pr_notice("[MEDMCU] HnatInfoHost:0x%llx(0x%llx), PitNat:0x%llx(0x%llx), Pit:0x%llx(0x%llx), Drb0:0x%llx(0x%llx), Drb1:0x%llx(0x%llx)\n",
			   scp_reserve_mblock[SCP_A_HNAT_INFO_HOST_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_HNAT_INFO_HOST_MEM_ID].size,
			   scp_reserve_mblock[SCP_A_PIT_NAT_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_PIT_NAT_MEM_ID].size,
			   scp_reserve_mblock[SCP_A_PIT_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_PIT_MEM_ID].size,
			   scp_reserve_mblock[SCP_A_DRB0_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_DRB0_MEM_ID].size,
			   scp_reserve_mblock[SCP_A_DRB1_MEM_ID].start_phys,
			   scp_reserve_mblock[SCP_A_DRB1_MEM_ID].size);
#endif

#ifdef CONFIG_MTK_ENG_BUILD
	WARN_ON(accumlate_memory_size > scp_mem_size);
#endif

#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		uint64_t start_phys = (uint64_t)scp_get_reserve_mem_phys(id);
		uint64_t start_virt = (uint64_t)scp_get_reserve_mem_virt(id);
		uint64_t len = (uint64_t)scp_get_reserve_mem_size(id);

		pr_notice("[MEDMCU][rsrv_mem-%d] phy:0x%llx - 0x%llx, len:0x%llx\n",
			id, start_phys, start_phys + len - 1, len);
		pr_notice("[MEDMCU][rsrv_mem-%d] vir:0x%llx - 0x%llx, len:0x%llx\n",
			id, start_virt, start_virt + len - 1, len);
	}
#endif

	return 0;
}

#if TEST_WITHOUT_MODEM
static int scp_emu_reserve_memory_ioremap(void)
{
	unsigned int num = (unsigned int)(sizeof(scp_emu_reserve_mblock)
			/ sizeof(scp_emu_reserve_mblock[0]));
	enum scp_emu_reserve_mem_id_t id;
	phys_addr_t accumlate_memory_size = 0;

	if ((scp_emu_mem_base_phys <= 0x0)) {
		/* The scp remapped region is fixed, only
		 * 0x4000_0000ULL ~ 0x8FFF_FFFFULL is accessible.
		 */
		pr_err("[MEDMCU] Emu Error: Wrong Address (0x%llx)\n",
			    (uint64_t)scp_emu_mem_base_phys);
		//BUG_ON(1);
		return -1;
	}

	if (num != NUMS_EMU_MEM_ID) {
		pr_err("[MEDMCU] Emu number of entries of reserved memory %u / %u\n",
			num, NUMS_EMU_MEM_ID);
		//BUG_ON(1);
		return -1;
	}

	scp_emu_mem_base_virt = (phys_addr_t)(size_t)ioremap_wc(scp_emu_mem_base_phys,
		scp_emu_mem_size);

	pr_debug("[MEDMCU] Emu rsrv_phy_base = 0x%llx, len:0x%llx\n",
		(uint64_t)scp_emu_mem_base_phys, (uint64_t)scp_emu_mem_size);

	pr_debug("[MEDMCU] Emu rsrv_vir_base = 0x%llx, len:0x%llx\n",
		(uint64_t)scp_emu_mem_base_virt, (uint64_t)scp_emu_mem_size);

	for (id = 0; id < NUMS_EMU_MEM_ID; id++) {
		scp_emu_reserve_mblock[id].start_phys = scp_emu_mem_base_phys +
			accumlate_memory_size;
		scp_emu_reserve_mblock[id].start_virt = scp_emu_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += scp_emu_reserve_mblock[id].size;

#ifdef DEBUG
		pr_debug("[MEDMCU] Emu [%d] phys:0x%llx, virt:0x%llx, len:0x%llx\n",
			id, (uint64_t)scp_emu_reserve_mblock[id].start_phys,
			(uint64_t)scp_emu_reserve_mblock[id].start_virt,
			(uint64_t)scp_emu_reserve_mblock[id].size);
#endif
	}

	memset((void __iomem *)scp_emu_mem_base_virt, 0, scp_emu_mem_size);

#ifdef CONFIG_MTK_ENG_BUILD
	WARN_ON(accumlate_memory_size > scp_emu_mem_size);
#endif

	return 0;
}
#endif  // TEST_WITHOUT_MODEM
#endif

#if ENABLE_SCP_EMI_PROTECTION
void set_scp_mpu(void)
{
	struct emimpu_region_t md_region;

	mtk_emimpu_init_region(&md_region, MPU_REGION_ID_SCP_SMEM);
	mtk_emimpu_set_addr(&md_region, scp_mem_base_phys,
		scp_mem_base_phys + scp_mem_size - 1);
	mtk_emimpu_set_apc(&md_region, MPU_DOMAIN_D0,
		MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&md_region, MPU_DOMAIN_D3,
		MTK_EMIMPU_NO_PROTECTION);
	if (mtk_emimpu_set_protection(&md_region))
		pr_notice("[MEDMCU]mtk_emimpu_set_protection fail\n");
	mtk_emimpu_free_region(&md_region);
}
#endif

void scp_register_feature(enum feature_id id)
{
	uint32_t i;
	int ret = 0;

	/*prevent from access when scp is down*/
	if (!scp_ready[SCP_A_ID]) {
		pr_err("[MEDMCU] %s: not ready, scp=%u\n", __func__,
			scp_ready[SCP_A_ID]);
		return;
	}

	/* because feature_table is a global variable,
	 * use mutex lock to protect it from accessing in the same time
	 */
	mutex_lock(&scp_feature_mutex);

	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id)
			feature_table[i].enable = 1;
	}
#if SCP_DVFS_INIT_ENABLE
	scp_expected_freq = scp_get_freq();
#endif

	scp_current_freq = readl(CURRENT_FREQ_REG);
	writel(scp_expected_freq, EXPECTED_FREQ_REG);

	/* send request only when scp is not down */
	if (scp_ready[SCP_A_ID]) {
		if (scp_current_freq != scp_expected_freq) {
			/* set scp freq. */
#if SCP_DVFS_INIT_ENABLE
			ret = scp_request_freq();
#endif
			if (ret == -1) {
				pr_err("[MEDMCU]%s request_freq fail\n", __func__);
				WARN_ON(1);
			}
		}
	} else {
		pr_err("[MEDMCU]Not send SCP DVFS request because SCP is down\n");
		WARN_ON(1);
	}

	mutex_unlock(&scp_feature_mutex);
}

void scp_deregister_feature(enum feature_id id)
{
	uint32_t i;
	int ret = 0;

	/* prevent from access when scp is down */
	if (!scp_ready[SCP_A_ID]) {
		pr_err("[MEDMCU] %s:not ready, scp=%u\n", __func__,
			scp_ready[SCP_A_ID]);
		return;
	}

	mutex_lock(&scp_feature_mutex);

	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id)
			feature_table[i].enable = 0;
	}
#if SCP_DVFS_INIT_ENABLE
	scp_expected_freq = scp_get_freq();
#endif

	scp_current_freq = readl(CURRENT_FREQ_REG);
	writel(scp_expected_freq, EXPECTED_FREQ_REG);

	/* send request only when scp is not down */
	if (scp_ready[SCP_A_ID]) {
		if (scp_current_freq != scp_expected_freq) {
			/* set scp freq. */
#if SCP_DVFS_INIT_ENABLE
			ret = scp_request_freq();
#endif
			if (ret == -1) {
				pr_err("[MEDMCU] %s: req_freq fail\n", __func__);
				WARN_ON(1);
			}
		}
	} else {
		pr_err("[MEDMCU]Not send SCP DVFS request because SCP is down\n");
		WARN_ON(1);
	}

	mutex_unlock(&scp_feature_mutex);
}

/*
 * apps notification
 */
void scp_extern_notify(enum SCP_NOTIFY_EVENT notify_status)
{
	blocking_notifier_call_chain(&scp_A_notifier_list, notify_status, NULL);
}

/*
 * reset awake counter
 */
void scp_reset_awake_counts(void)
{
	int i;

	/* scp ready static flag initialise */
	for (i = 0; i < SCP_CORE_TOTAL ; i++)
		scp_awake_counts[i] = 0;
}

void scp_awake_init(void)
{
	scp_reset_awake_counts();
}

#if SCP_RECOVERY_SUPPORT
/*
 * scp_set_reset_status, set and return scp reset status function
 * return value:
 *   0: scp not in reset status
 *   1: scp in reset status
 */
unsigned int scp_set_reset_status(void)
{
	unsigned long spin_flags;

	spin_lock_irqsave(&scp_reset_spinlock, spin_flags);
	if (atomic_read(&scp_reset_status) == RESET_STATUS_START) {
		spin_unlock_irqrestore(&scp_reset_spinlock, spin_flags);
		return 1;
	}

	/* scp not in reset status, set it and return*/
	atomic_set(&scp_reset_status, RESET_STATUS_START);

	spin_unlock_irqrestore(&scp_reset_spinlock, spin_flags);
	return 0;
}

/******************************************************************************
 *****************************************************************************/
static void print_clk_registers(void)
{
	void __iomem *cfg = scpreg.cfg;
	void __iomem *clkctrl = scpreg.clkctrl;
	void __iomem *cfg_core0 = scpreg.cfg_core0;
	void __iomem *cfg_core1 = scpreg.cfg_core1;

	unsigned int offset;
	unsigned int value;

	// 0x24000 ~ 0x24160 (inclusive)
	for (offset = 0x0000; offset <= 0x0160; offset += 4) {
		value = (unsigned int)readl(cfg + offset);
		pr_notice("[MEDMCU] cfg[0x%04x]: 0x%08x\n", offset, value);
	}
	// 0x21000 ~ 0x210120 (inclusive)
	for (offset = 0x0000; offset < 0x0120; offset += 4) {
		value = (unsigned int)readl(clkctrl + offset);
		pr_notice("[MEDMCU] clk[0x%llx]: 0x%08x\n", (uint64_t)(clkctrl + offset), value);
	}
	// 0x30000 ~ 0x30114 (inclusive)
	for (offset = 0x0000; offset <= 0x0114; offset += 4) {
		value = (unsigned int)readl(cfg_core0 + offset);
		pr_notice("[MEDMCU] cfg_core0[0x%04x]: 0x%08x\n", offset, value);
	}
	// 0x40000 ~ 0x40114 (inclusive)
	for (offset = 0x0000; offset <= 0x0114; offset += 4) {
		value = (unsigned int)readl(cfg_core1 + offset);
		pr_notice("[MEDMCU] cfg_core1[0x%04x]: 0x%08x\n", offset, value);
	}

}
#endif

void scp_reset_wait_timeout(void)
{
	uint32_t core0_halt = 0;
	/* make sure scp is in idle state */
	int timeout = 50; /* max wait 1s */

	while (timeout--) {
		core0_halt = readl(R_CORE0_STATUS) & B_CORE_HALT;
		if (core0_halt) {
			/* SCP stops any activities
			 * and parks at wfi
			 */
			break;
		}
		mdelay(20);
	}

	if (timeout == -1)
		pr_notice("[MEDMCU] reset timeout, still reset scp\n");

}

/*
 * callback function for work struct
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
#if SCP_RECOVERY_SUPPORT
static void scp_reset_flow(unsigned int scp_reset_type)
{
	unsigned long spin_flags;
	struct arm_smccc_res ares;
	uint64_t rst_time;

	/* scp reset by CMD, WDT or awake fail */
	if ((scp_reset_type == RESET_TYPE_TIMEOUT) ||
		(scp_reset_type == RESET_TYPE_AWAKE) ||
		 scp_reset_type == RESET_TYPE_RESUME) {
		/* stop scp */
		arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
			MTK_SIP_MEDMCU_RSTN_SET,
			0, 0, 0, 0, 0, 0,
			&ares);
		pr_notice("[MEDMCU] rstn core0 %x\n",
		readl(R_CORE0_SW_RSTN_SET));
#if ENABLE_MEDMCU_SUSPEND_RESUME
		if (scp_reset_type == RESET_TYPE_RESUME) {
#if SCP_RESERVED_MEM
			arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
				MTK_SIP_MEDMCU_GPR_RESV_MEM,
				scp_mem_base_phys, scp_mem_size, 0, 0, 0, 0,
				&ares);
			pr_notice("[MEDMCU]%s set mem: 0x%llx 0x%llx\n", __func__,
				(uint64_t)scp_mem_base_phys, (uint64_t)scp_mem_size);
#endif
		}
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */
	} else {
		/* 
		 * reset type scp WDT, CMD or MD_EXCEP
		 * make sure scp is in idle state
		 */
		if (scp_reset_type == RESET_TYPE_WDT)
			medmcu_set_ignore_stop_ipi_flag(1);
		scp_reset_wait_timeout();
		arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
			MTK_SIP_MEDMCU_RSTN_SET,
			0, 0, 0, 0, 0, 0,
			&ares);
		arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
			MTK_SIP_MEDMCU_REBOOT_STATUS,
			0, CORE_REBOOT_OK, 0, 0, 0, 0,
			&ares);
		pr_notice("[MEDMCU] rstn core0 %x\n",
		readl(R_CORE0_SW_RSTN_SET));
	}

	/* scp reset */
	scp_sys_full_reset();

	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	scp_reset_awake_counts();
	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	/* start scp */
	pr_notice("[MEDMCU] start scp\n");
#if ENABLE_MEDMCU_SUSPEND_RESUME
	if (scp_reset_type == RESET_TYPE_RESUME) {
		pr_notice("[MEDMCU] get spinlock for atomic medmcu resume rstn sequence\n");
		mutex_lock(&medmcu_resume_mutex);
	}
#endif
	rst_time = (uint64_t)ktime_get();
	arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
		MTK_SIP_MEDMCU_RSTN_CLR,
		rst_time, 0, 0, 0, 0, 0,
		&ares);

#if ENABLE_MEDMCU_SUSPEND_RESUME
	atomic_set(&medmcu_resume_need_dpmaif_ready, 0);
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */

	if (scp_reset_type == RESET_TYPE_MD_EXCEP) {
		/* notify scp functions stop */
		pr_notice("[MEDMCU] %s(): scp_extern_notify SCP_EVENT_STOP_ACK\n", __func__);
		scp_extern_notify(SCP_EVENT_STOP_ACK);
	} else if (scp_reset_type == RESET_TYPE_WDT) {
		/* notify scp functions stop */
		pr_notice("[MEDMCU] %s(): scp_extern_notify SCP_EVENT_STOP_BY_WDT\n", __func__);
		scp_extern_notify(SCP_EVENT_STOP_BY_WDT);
	}
#if ENABLE_MEDMCU_SUSPEND_RESUME
	else if (scp_reset_type == RESET_TYPE_RESUME) {
		/* wait scp ready and ask for dpmaif_ready_event */
		atomic_set(&medmcu_resume_need_dpmaif_ready, 1);
		mutex_unlock(&medmcu_resume_mutex);
		pr_notice("[MEDMCU] %s(): dpmaif rdy flag set in RESET_TYPE_RESUME\n", __func__);
	}
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */
	pr_notice("[MEDMCU] rstn core0 %x\n", readl(R_CORE0_SW_RSTN_CLR));

#if SCP_BOOT_TIME_OUT_MONITOR
	mod_timer(&scp_ready_timer[SCP_A_ID], jiffies + SCP_READY_TIMEOUT);
#endif

	/* clear scp reset by cmd flag*/
	scp_reset_by_cmd = 0;
	scp_reset_by_md_excep = 0;
}

static void scp_sys_delay_reset_ws(struct work_struct *ws)
{
	struct scp_work_struct *sws = container_of(ws
					, struct scp_work_struct, work);
	unsigned int scp_reset_type = sws->flags;
	int timeout = scp_wdt_reset_timeout; /* max wait 15s */

	while (timeout--) {
		if (is_medhw_logged_ready()) {
			clear_medhw_logged();
			break;
		}
		msleep(1000);
	}
	if (timeout == -1)
		pr_notice("[MEDMCU] medhw log not ready in %ds, still reset scp\n",
				  scp_wdt_reset_timeout);

	scp_reset_flow(scp_reset_type);
}

static void scp_sys_reset_ws(struct work_struct *ws)
{
	struct scp_work_struct *sws = container_of(ws
					, struct scp_work_struct, work);
	unsigned int scp_reset_type = sws->flags;

	pr_notice("[MEDMCU] %s(): remain %d times\n", __func__, scp_reset_counts);
	if (scp_reset_type == RESET_TYPE_MD_FORCE_STOP) {
		scp_extern_notify(SCP_EVENT_STOP_ACK);
		return;
	}

	/* notify scp functions stop */
	pr_debug("[MEDMCU] %s(): scp_extern_notify SCP_EVENT_STOP\n", __func__);
	scp_extern_notify(SCP_EVENT_STOP);
	/*
	 *   scp_ready:
	 *   SCP_PLATFORM_STOP  = 0,
	 *   SCP_PLATFORM_READY = 1,
	 */
	scp_ready[SCP_A_ID] = 0;

	clear_medhw_logged();

	if (scp_reset_type == RESET_TYPE_MD_EXCEP || scp_reset_type == RESET_TYPE_RESUME)
		goto scp_reset;

	/* print_clk and scp_aed before pll enable to keep ori CLK_SEL */
	print_clk_registers();
	/* workqueue for scp ee, scp reset by cmd will not trigger scp ee */
	if (scp_reset_by_cmd == 0 && scp_reset_by_md_excep == 0) {
		pr_notice("[MEDMCU] %s(): scp_aed_reset\n", __func__);
		scp_aed(scp_reset_type, SCP_A_ID);
	}

scp_reset:
	pr_debug("[MEDMCU] %s(): disable logger\n", __func__);
	/* logger disable must after scp_aed() */
	scp_logger_init_set(0);

	pr_notice("[MEDMCU] %s(): scp_reset_type=%d, scp_wdt_reset_enable=%d\n", __func__,
			scp_reset_type, scp_wdt_reset_enable);

	if (scp_reset_type == RESET_TYPE_WDT) {
		if (scp_wdt_reset_enable == 0) {
			pr_notice("[MEDMCU] skip WDT reset\n");
#if SCP_BOOT_TIME_OUT_MONITOR
			mod_timer(&scp_ready_timer[SCP_A_ID], jiffies + SCP_READY_TIMEOUT);
#endif
			/* clear scp reset by cmd flag*/
			scp_reset_by_cmd = 0;
			scp_reset_by_md_excep = 0;
			return;
		} else if ((scp_wdt_reset_enable == 1) && is_aee_enabled) {
			scp_sys_delay_reset_work.flags = scp_sys_reset_work.flags;
			scp_sys_delay_reset_work.id = scp_sys_reset_work.id;
			scp_schedule_delay_reset_work(&scp_sys_delay_reset_work);
			return;
		}
	}

	scp_reset_flow(scp_reset_type);
}

/*
 * schedule a work to reset scp
 * @param type: exception type
 */
void scp_send_reset_wq(enum SCP_RESET_TYPE type)
{
	scp_sys_reset_work.flags = (unsigned int) type;
	scp_sys_reset_work.id = SCP_A_ID;
	if (scp_reset_counts > 0) {
		scp_reset_counts--;
		scp_schedule_reset_work(&scp_sys_reset_work);
	}
}
#endif

int scp_check_resource(void)
{
	/* called by lowpower related function
	 * main purpose is to ensure main_pll is not disabled
	 * because scp needs main_pll to run at vcore 1.0 and 354Mhz
	 * return value:
	 * 1: main_pll shall be enabled
	 *    26M shall be enabled, infra shall be enabled
	 * 0: main_pll may disable, 26M may disable, infra may disable
	 */
	int scp_resource_status = 0;
#ifdef CONFIG_MACH_MT6799
	unsigned long spin_flags;

	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	scp_current_freq = readl(CURRENT_FREQ_REG);
	scp_expected_freq = readl(EXPECTED_FREQ_REG);
	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	if (scp_expected_freq == FREQ_416MHZ || scp_current_freq == FREQ_416MHZ)
		scp_resource_status = 1;
	else
		scp_resource_status = 0;
#endif

	return scp_resource_status;
}

#if SCP_RECOVERY_SUPPORT
static void scp_region_info_init(void)
{
	/*get scp loader/firmware info from scp sram*/
	scp_region_info = (SCP_TCM + SCP_REGION_INFO_OFFSET);
	pr_debug("[MEDMCU] scp_region_info = 0x%llx\n", (uint64_t)scp_region_info);
	memcpy_from_scp(&scp_region_info_copy,
		scp_region_info, sizeof(scp_region_info_copy));
}
#else
static void scp_region_info_init(void) {}
#endif

static void scp_recovery_init(void)
{
#if SCP_RECOVERY_SUPPORT
	/*create wq for scp reset*/
	scp_reset_workqueue = create_singlethread_workqueue("SCP_RESET_WQ");
	scp_delay_reset_workqueue = create_singlethread_workqueue("SCP_DELAY_RESET_WQ");

	/*init reset work*/
	INIT_WORK(&scp_sys_reset_work.work, scp_sys_reset_ws);
	INIT_WORK(&scp_sys_delay_reset_work.work, scp_sys_delay_reset_ws);

	scp_loader_virt = ioremap_wc(
		scp_region_info_copy.ap_loader_start,
		scp_region_info_copy.ap_loader_size);

	pr_notice("[MEDMCU] loader image mem: virt:0x%llx - 0x%llx\n",
		(uint64_t)(phys_addr_t)scp_loader_virt,
		(uint64_t)(phys_addr_t)scp_loader_virt +
		(phys_addr_t)scp_region_info_copy.ap_loader_size);

	/* init reset by cmd flag */
	scp_reset_by_cmd = 0;
	scp_reset_by_md_excep = 0;
	pended_dpmaif_ready = false;

	scp_regdump_virt = ioremap_wc(
			scp_region_info_copy.regdump_start,
			scp_region_info_copy.regdump_size);

	pr_debug("[MEDMCU] scp_regdump_virt map: 0x%x + 0x%x\n",
		scp_region_info_copy.regdump_start,
		scp_region_info_copy.regdump_size);
#endif
}

static bool scp_ipi_table_init(struct mtk_mbox_device *scp_mboxdev, struct platform_device *pdev)
{
	enum table_item_num {
		send_item_num = 3,
		recv_item_num = 4
	};
	u32 i, ret, mbox_id = 0, recv_opt = 0, sz;

	of_property_read_u32(pdev->dev.of_node, "mbox_count"
						, &scp_mboxdev->count);
	if (!scp_mboxdev->count) {
		pr_notice("[SCP] mbox count not found\n");
		return false;
	}

	scp_mboxdev->send_count = of_property_count_u32_elems(
				pdev->dev.of_node, "send_table")
				/ send_item_num;
	if (scp_mboxdev->send_count <= 0) {
		pr_notice("[SCP] scp send table not found\n");
		return false;
	}

	scp_mboxdev->recv_count = of_property_count_u32_elems(
				pdev->dev.of_node, "recv_table")
				/ recv_item_num;
	if (scp_mboxdev->recv_count <= 0) {
		pr_notice("[SCP] scp recv table not found\n");
		return false;
	}
	/* alloc and init scp_mbox_info */
	sz = sizeof(struct mtk_mbox_info) * scp_mboxdev->count;
	scp_mboxdev->info_table = vzalloc(sz);
	if (!scp_mboxdev->info_table)
		return false;

	scp_mbox_info = scp_mboxdev->info_table;
	for (i = 0; i < scp_mboxdev->count; ++i) {
		scp_mbox_info[i].id = i;
		scp_mbox_info[i].slot = 64;
		scp_mbox_info[i].enable = 1;
		scp_mbox_info[i].is64d = 1;
	}
	/* alloc and init send table */
	sz = sizeof(struct mtk_mbox_pin_send) * scp_mboxdev->send_count;
	scp_mboxdev->pin_send_table = vzalloc(sz);
	if (!scp_mboxdev->pin_send_table)
		return false;

	scp_mbox_pin_send = scp_mboxdev->pin_send_table;
	for (i = 0; i < scp_mboxdev->send_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send_table",
				i * send_item_num,
				&scp_mbox_pin_send[i].chan_id);
		if (ret) {
			pr_notice("[SCP]%s:Cannot get ipi id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send_table",
				i * send_item_num + 1,
				&mbox_id);
		if (ret) {
			pr_notice("[SCP] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		scp_mbox_pin_send[i].mbox = mbox_id;
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send_table",
				i * send_item_num + 2,
				&scp_mbox_pin_send[i].msg_size);
		if (ret) {
			pr_notice("[SCP]%s:Cannot get pin size (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
	}
	/* alloc and init recv table */
	sz = sizeof(struct mtk_mbox_pin_recv) * scp_mboxdev->recv_count;
	scp_mboxdev->pin_recv_table = vzalloc(sz);
	if (!scp_mboxdev->pin_recv_table)
		return false;

	scp_mbox_pin_recv = scp_mboxdev->pin_recv_table;
	for (i = 0; i < scp_mboxdev->recv_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num,
				&scp_mbox_pin_recv[i].chan_id);
		if (ret) {
			pr_notice("[SCP]%s:Cannot get ipi id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num + 1,
				&mbox_id);
		if (ret) {
			pr_notice("[SCP] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		scp_mbox_pin_recv[i].mbox = mbox_id;
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num + 2,
				&scp_mbox_pin_recv[i].msg_size);
		if (ret) {
			pr_notice("[SCP]%s:Cannot get pin size (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num + 3,
				&recv_opt);
		if (ret) {
			pr_notice("[SCP]%s:Cannot get recv opt (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		scp_mbox_pin_recv[i].recv_opt = recv_opt;
	}

	return true;
}

static int medmcu_device_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0, err = 0;
	struct resource *res;
	const char *core_status = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *node;

	struct net_device *netdev;
	struct MDMA_END_DEVICE *ei_local = &mdma_device;
	unsigned int flag;

#ifdef MED_V2
    /* Power on medsys mtcmos */
    pm_runtime_enable(&pdev->dev);
    ret = pm_runtime_get_sync(&pdev->dev);
    if (ret != 0) {
        pr_err("[MEDMCU]%s:Fail to power on medsys mtcmos, ret = %d\n",
            __func__, ret);
        return -1;
    }

    /* Get medsys sel */
    g_medmcu.medsys_clk = devm_clk_get(&pdev->dev, "medmcu_sel");
    if (IS_ERR(g_medmcu.medsys_clk)) {
        pr_err("[MEDMCU]%s:Fail to get medmcu sel", __func__);
        g_medmcu.medsys_clk = NULL;
        return -1;
    }

    /* Enable medmcu sel */
    ret = clk_prepare_enable(g_medmcu.medsys_clk);
    if (ret != 0) {
        pr_err("[MEDMCU]%s:Fail to enable medmcu sel, ret = %d\n",
            __func__, ret);
        clk_disable_unprepare(g_medmcu.medsys_clk);
        return -1;
    }
#endif

	g_medmcu.dl_wakelock = wakeup_source_register(NULL, "medmcu_dl_wakelock");
	if (!g_medmcu.dl_wakelock) {
		pr_err("%s %d: init dl wakeup source fail!", __func__, __LINE__);
		return -1;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scpreg.sram = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.sram)) {
		pr_err("[MEDMCU] scpreg.sram error\n");
		return -1;
	}
	scpreg.total_tcmsize = (unsigned int)resource_size(res);
	pr_notice("[MEDMCU] sram base = 0x%llx %x\n"
		, (uint64_t)scpreg.sram, scpreg.total_tcmsize);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	scpreg.cfg = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg)) {
		pr_err("[MEDMCU] scpreg.cfg error\n");
		return -1;
	}
	pr_debug("[MEDMCU] cfg base = 0x%llx\n", (uint64_t)scpreg.cfg);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	scpreg.clkctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.clkctrl)) {
		pr_err("[MEDMCU] scpreg.clkctrl error\n");
		return -1;
	}
	pr_debug("[MEDMCU] clkctrl base = 0x%llx\n", (uint64_t)scpreg.clkctrl);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	scpreg.cfg_core0 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg_core0)) {
		pr_err("[MEDMCU] scpreg.cfg_core0 error\n");
		return -1;
	}
	pr_debug("[MEDMCU] cfg_core0 base = 0x%p\n", scpreg.cfg_core0);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	scpreg.cfg_core1 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg_core1)) {
		pr_err("[MEDMCU] scpreg.cfg_core1 error\n");
		return -1;
	}
	pr_debug("[MEDMCU] cfg_core1 base = 0x%p\n", scpreg.cfg_core1);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 5);
	scpreg.bus_tracker = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.bus_tracker)) {
		pr_err("[MEDMCU] scpreg.bus_tracker error\n");
		return -1;
	}
	pr_debug("[MEDMCU] bus_tracker base = 0x%p\n", scpreg.bus_tracker);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 6);
	scpreg.l1cctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.l1cctrl)) {
		pr_err("[MEDMCU] scpreg.l1cctrl error\n");
		return -1;
	}
	pr_debug("[MEDMCU] l1cctrl base = 0x%p\n", scpreg.l1cctrl);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 7);
	scpreg.cfg_sec = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg_sec)) {
		pr_err("[MEDMCU] scpreg.cfg_sec error\n");
		return -1;
	}
	pr_debug("[MEDMCU] cfg_sec base = 0x%p\n", scpreg.cfg_sec);

#if ENABLE_MEDMCU_SUSPEND_RESUME
	res = platform_get_resource(pdev, IORESOURCE_MEM, 28);
	scpreg.infra_clk = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.infra_clk)) {
		pr_err("[MEDMCU] scpreg.infra_clk error\n");
		return -1;
	}
	pr_debug("[MEDMCU] infra_clk base = 0x%p\n", scpreg.infra_clk);
#endif

	of_property_read_u32(pdev->dev.of_node, "scp_sramSize"
						, &scpreg.scp_tcmsize);
	if (!scpreg.scp_tcmsize) {
		pr_err("[MEDMCU] total_tcmsize not found\n");
		return -ENODEV;
	}
	pr_debug("[MEDMCU] scpreg.scp_tcmsize = %d\n", scpreg.scp_tcmsize);

	/* scp core 0 */
	err = of_property_read_string(pdev->dev.of_node, "core_0", &core_status);
	if (err < 0 || strcmp(core_status, "enable") != 0)
		pr_err("[MEDMCU] core_0 not enable\n");
	else {
		pr_err("[MEDMCU] core_0 enable\n");
		scp_enable[SCP_A_ID] = 1;
	}

	scpreg.irq = platform_get_irq_byname(pdev, "mbox0");
	ret = request_irq(scpreg.irq, scp_A_irq_handler,
					IRQF_TRIGGER_NONE, "SCP WDT", NULL);

	/* probe mbox info from dts */
	if (!scp_ipi_table_init(&scp_mboxdev, pdev))
		return -ENODEV;
	/* create mbox dev */
	/* Due to mbox0 for WDT, start from mbox1 */
	flag = (MBOX_REG_RESOURCE_BASE |
		MBOX_REG_RESOURCE_SET |
		MBOX_REG_RESOURCE_CLR);
	for (i = 1; i < scp_mboxdev.count; i++) {
		scp_mbox_info[i].mbdev = &scp_mboxdev;
		mtk_mbox_probe(pdev, scp_mbox_info[i].mbdev, i, flag);
		mbox_setup_pin_table(i);
	}

	for (i = 0; i < IRQ_NUMBER; i++) {
		if (scp_ipi_irqs[i].name == NULL)
			continue;

		node = of_find_compatible_node(NULL, NULL,
					      scp_ipi_irqs[i].name);
		if (!node) {
			pr_info("[MEDMCU] find '%s' node failed\n",
				scp_ipi_irqs[i].name);
			continue;
		}

		scp_ipi_irqs[i].irq_no =
			irq_of_parse_and_map(node, scp_ipi_irqs[i].order);

		if (!scp_ipi_irqs[i].irq_no)
			pr_info("[MEDMCU] get '%s' fail\n", scp_ipi_irqs[i].name);
	}

	ret = mtk_ipi_device_register(&scp_ipidev, pdev, &scp_mboxdev,
				      SCP_IPI_COUNT);

	if (ret)
		pr_err("[MEDMCU] ipi_dev_register fail, ret %d\n", ret);

	g_medmcu.medhw_base = ioremap(MTK_MEDHW_BASE, MTK_MEDHW_RANGE);

	g_medmcu.fe_base = ioremap(MTK_FE_BASE, MTK_FE_RANGE);

#if defined(MED_V2)
	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(&pdev->dev, "dma_set_coherent_mask: %d\n", ret);
		return ret;
	}

	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(33));
	if (ret) {
		dev_err(&pdev->dev, "dma_set_mask: %d\n", ret);
		return ret;
	}
#endif

	netdev = alloc_etherdev_mqs(sizeof(struct MDMA_END_DEVICE), 1, 1);
	if (!netdev) {
		pr_info("!!!!!!!!!netdev failed!!!!!!!!!!\n");
		return -ENOMEM;
	}
	SET_NETDEV_DEV(netdev, &pdev->dev);

	fdma_init(NULL);
	mdma_init(netdev, ei_local);

	free_netdev(netdev);
	pr_info("[MEDMCU] probe done\n");

	return ret;
}

#if ENABLE_MEDMCU_SUSPEND_RESUME
static int medmcu_pm_resume_early(struct device *dev)
{
	int is_netsys_poweroff_before = is_fdma_poweroff_before();
	pr_info("[MEDMCU] medmcu_pm_resume_early: %d, %d\n",
			is_netsys_poweroff_before, atomic_read(&medmcu_suspend_response));
	if (is_netsys_poweroff_before == 1 && atomic_read(&medmcu_suspend_response) == 1) {
		reset_scp_without_restart_md();
	} else if (is_netsys_poweroff_before == 0 && atomic_read(&medmcu_suspend_response) == 1) {
		/* unmask irqs */
		notify_medmcu_unmask_irq();
	} else
		pr_err("[MEDMCU] unknown resume state: %d, %d\n",
			is_netsys_poweroff_before, atomic_read(&medmcu_suspend_response));
	atomic_set(&medmcu_suspend_response, 0);
	return 0;
}
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */

static int medmcu_pm_suspend(struct device *device)
{
	struct arm_smccc_res ares;

#if ENABLE_MEDMCU_SUSPEND_RESUME
	if (sync_medmcu_can_suspend() == -1) {
		__pm_wakeup_event(g_medmcu.dl_wakelock, jiffies_to_msecs(HZ));
		return -1;
	}
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */
	arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
		MTK_SIP_MEDMCU_WDT_DISABLE,
		0, 0, 0, 0, 0, 0,
		&ares);

#ifdef MED_V2
	pr_notice("[MEDMCU]%s: disable medmcu clk\n", __func__);
	clk_disable_unprepare(g_medmcu.medsys_clk);
#endif

	pr_notice("medmcu suspend done\n");

	return 0;
}

static int medmcu_pm_resume(struct device *device)
{
	struct arm_smccc_res ares;
	int ret;

	pr_notice("medmcu resume start\n");

#ifdef MED_V2
	/* Enable medmcu sel */
	ret = clk_prepare_enable(g_medmcu.medsys_clk);
	if (ret != 0) {
		pr_err("[MEDMCU]%s:Fail to enable medmcu sel, ret = %d\n",
			__func__, ret);
		clk_disable_unprepare(g_medmcu.medsys_clk);
		return -1;
	}
	pr_notice("[MEDMCU]%s: enable medmcu clk\n", __func__);
#endif

	arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
		MTK_SIP_MEDMCU_PRE_CTRL,
		0, 0, 0, 0, 0, 0,
		&ares);

	arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
		MTK_SIP_MEDMCU_WDT_START,
		0, 0, 0, 0, 0, 0,
		&ares);

	pr_notice("medmcu resume kick start\n");
#if ENABLE_MEDMCU_SUSPEND_RESUME
	medmcu_pm_resume_early(NULL);
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */
	pr_notice("medmcu resume done\n");

	return 0;
}

static const struct dev_pm_ops medmcu_pm_ops = {
	.suspend = medmcu_pm_suspend,
	.resume = medmcu_pm_resume,
};

static void medmcu_device_remove(struct platform_device *dev)
{
	kfree(scp_mbox_info);
	scp_mbox_info = NULL;

	kfree(scp_mbox_pin_recv);
	scp_mbox_pin_recv = NULL;

	kfree(scp_mbox_pin_send);
	scp_mbox_pin_send = NULL;

}

static const struct of_device_id medmcu_of_ids[] = {
	{ .compatible = "mediatek,medmcu", },
	{}
};

static struct platform_driver mtk_medmcu_device = {
	.probe  = medmcu_device_probe,
	.remove = medmcu_device_remove,
	.driver = {
		.name = "medmcu",
		.pm = &medmcu_pm_ops,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = medmcu_of_ids,
#endif
	},
};

static struct syscore_ops scp_ipi_dbg_syscore_ops = {
	.suspend = scp_ipi_syscore_dbg_suspend,
	.resume = scp_ipi_syscore_dbg_resume,
};

static int aee_parse_chosen(void)
{
	struct device_node *node;
	const char *aee_enable;
	int ret = 0;

	node = of_find_node_by_path("/chosen");
	if (node) {
		if (of_property_read_string(node, "aee,enable", &aee_enable) == 0) {
			if (strnstr(aee_enable, "mini", 4))
				ret = 1;
			else if (strnstr(aee_enable, "full", 4))
				ret = 2;
		}
		of_node_put(node);
	} else {
		pr_notice("%s: Can't find aee chosen node\n", __func__);
	}

	return ret;
}


/*
 * driver initialization entry point
 */
static int __init medmcu_init(void)
{
	int ret = 0;
	int i = 0;

	pr_notice("[MEDMCU] %s Init_Start.\n", __func__);

#if SCP_BOOT_TIME_OUT_MONITOR
	init_timer(&scp_ready_timer[SCP_A_ID]);
	scp_ready_timer[SCP_A_ID].function = &scp_wait_ready_timeout;
	scp_ready_timer[SCP_A_ID].data = (unsigned long) SCP_A_TIMER;
#endif

	/* Ready static flag initialise */
	for (i = 0; i < SCP_CORE_TOTAL ; i++) {
		scp_enable[i] = 0;
		scp_ready[i]  = 0;
	}

#if SCP_DVFS_INIT_ENABLE
	scp_dvfs_init();
	wait_scp_dvfs_init_done();

	/* pll maybe gate, request pll before access any reg/sram */
	scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif

#ifndef CONFIG_FPGA_EARLY_PORTING
	/* keep Univpll */
//	scp_resource_req(SCP_REQ_26M);
#endif

#if SCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM) && !defined(MED_V2)
	/* make sure the reserved memory for scp is ready */
	if (scp_mem_size == 0) {
		pr_err("[MEDMCU]Reserving memory by of_device for SCP failed.\n");
		return -1;
	}
#endif

#if SCP_RESERVED_MEM && defined(MED_V2)
    if (collect_lk_medmcu_reserved_desc()) {
        pr_err("[MEDMCU] error occurs during collecting reserved desc mem");
        return -1;
    }
#endif

#if SCP_RESERVED_MEM
		/*scp resvered memory*/
		pr_notice("[MEDMCU] scp_reserve_memory_ioremap\n");
		ret = scp_reserve_memory_ioremap();
		if (ret) {
			pr_err("[MEDMCU]scp_reserve_memory_ioremap failed\n");
			goto err;
		}
#if TEST_WITHOUT_MODEM
		pr_notice("[MEDMCU] scp_emu_reserve_memory_ioremap\n");
		ret = scp_emu_reserve_memory_ioremap();
		if (ret) {
			pr_err("[MEDMCU]scp_emu_reserve_memory_ioremap failed\n");
			goto err;
		}
#endif  // TEST_WITHOUT_MODEM
#endif

	if (platform_driver_register(&mtk_medmcu_device))
		pr_err("[MEDMCU] scp probe fail\n");

	/* skip initial if dts status = "disable" */
	if (!scp_enable[SCP_A_ID]) {
		pr_err("[MEDMCU] scp disabled!!\n");
		goto err;
	}

	scp_region_info_init();

	pr_debug("[MEDMCU] platform init\n");

	scp_awake_init();
	scp_workqueue = create_singlethread_workqueue("SCP_WQ");

	ret = scp_excep_init();
	if (ret) {
		pr_err("[MEDMCU]Excep Init Fail\n");
		goto err;
	}

	INIT_WORK(&scp_A_notify_work.work, scp_A_notify_ws);

	mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_READY_0,
			(void *)scp_A_ready_ipi_handler, NULL, &msg_scp_ready0);

	mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_READY_1,
			(void *)scp_A_ready_ipi_handler, NULL, &msg_scp_ready1);

	mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_ERROR_INFO_0,
			(void *)scp_err_info_handler, NULL, msg_scp_err_info0);

	mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_ERROR_INFO_1,
			(void *)scp_err_info_handler, NULL, msg_scp_err_info1);

	mtk_ipi_register(&scp_ipidev, IPI_IN_MED_STOP_ACK,
			(void *)med_stop_ack_ipi_handler, NULL, &msg_med_stop_ack);

#if ENABLE_MEDMCU_SUSPEND_RESUME
	mtk_ipi_register(&scp_ipidev,
					 IPI_IN_MED_SUSPEND_RESPONSE,
					 (void *)med_query_suspend_response_ipi_handler,
					 NULL,
					 &msg_query_suspend_response);
#endif /* ENABLE_MEDMCU_SUSPEND_RESUME */

#if (TEST_WITHOUT_CCCI == 0)
	ccci_dpmaif_register_notify(&ccci_apsync_notifier);
#endif

	ret = register_pm_notifier(&scp_pm_notifier_block);
	if (ret)
		pr_err("[MEDMCU] failed to register PM notifier %d\n", ret);

	/* scp sysfs initialise */
	pr_debug("[MEDMCU] sysfs init\n");
	ret = create_files();
	if (unlikely(ret != 0)) {
		pr_err("[MEDMCU] create files failed\n");
		goto err;
	}

#if SCP_LOGGER_ENABLE
	/* scp logger initialise */
	pr_debug("[MEDMCU] logger init\n");

	/*create wq for scp logger*/
	scp_logger_workqueue = create_singlethread_workqueue("SCP_LOG_WQ");

	if (scp_logger_init(scp_get_reserve_mem_virt(SCP_A_LOGGER_MEM_ID),
				scp_get_reserve_mem_size(SCP_A_LOGGER_MEM_ID)) == -1) {
		pr_err("[MEDMCU] scp_logger_init_fail\n");
		goto err;
	}
#endif

#if ENABLE_SCP_EMI_PROTECTION
	set_scp_mpu();
#endif

	scp_recovery_init();

	register_syscore_ops(&scp_ipi_dbg_syscore_ops);

	driver_init_done = true;

	reset_scp(SCP_ALL_ENABLE);

	scp_wdt_reset_enable = 1;
	scp_wdt_reset_timeout = 30;
	is_aee_enabled = (aee_parse_chosen() == 0 ? false : true);

	pr_notice("[MEDMCU] %s Init_End.\n", __func__);

	return ret;

err:
	return -1;
}

/*
 * driver exit point
 */
static void __exit medmcu_exit(void)
{
#if SCP_BOOT_TIME_OUT_MONITOR
	int i = 0;
#endif

#if SCP_LOGGER_ENABLE
	scp_logger_uninit();
#endif

	free_irq(scpreg.irq, NULL);
	misc_deregister(&scp_device);

	flush_workqueue(scp_workqueue);
	destroy_workqueue(scp_workqueue);

#if SCP_RECOVERY_SUPPORT
	flush_workqueue(scp_reset_workqueue);
	destroy_workqueue(scp_reset_workqueue);
	flush_workqueue(scp_delay_reset_workqueue);
	destroy_workqueue(scp_delay_reset_workqueue);
#endif

#if SCP_LOGGER_ENABLE
	flush_workqueue(scp_logger_workqueue);
	destroy_workqueue(scp_logger_workqueue);
#endif

#if SCP_BOOT_TIME_OUT_MONITOR
	for (i = 0; i < SCP_CORE_TOTAL ; i++)
		del_timer(&scp_ready_timer[i]);
#endif

	iounmap(g_medmcu.medhw_base);

	iounmap(g_medmcu.fe_base);
}

/*
 * MT6990 MEDMCU programs the MDMA rings in the shared frame-engine block.
 * Keep this after the Ethernet driver's FE reset, otherwise that later reset
 * clears RX_BASE_PTR/MAX_CNT and the MED firmware waits forever for MDMA.
 */
late_initcall_sync(medmcu_init);
module_exit(medmcu_exit);

MODULE_DESCRIPTION("MEDIATEK Module MEDMCU driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL v2");
