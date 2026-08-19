/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __CCCI_VENDOR_COMPAT_H__
#define __CCCI_VENDOR_COMPAT_H__

#include <linux/types.h>

struct sk_buff;

bool spm_is_md1_sleep(void);
void spm_ap_mdsrc_req(u8 lock);
int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf,
				unsigned int len);
int switch_sim_mode(int id, char *buf, unsigned int len);
unsigned int get_sim_switch_type(void);
unsigned int get_modem_is_enabled(int md_id);
int register_ccci_sys_call_back(int md_id, unsigned int id,
				int (*func)(int, int));
unsigned int mt_irq_get_pending(unsigned int irq);
unsigned long ccci_get_md_boot_count(int md_id);
char *ccci_get_ap_platform(void);
bool is_clk_buf_from_pmic(void);
void clk_buf_get_swctrl_status(void *swctrl_status);
void clk_buf_get_rf_drv_curr(void *rf_drv_curr);
void clk_buf_save_afc_val(unsigned int afcdac);
int rawbulk_push_upstream_buffer(int transfer_id, const void *buffer,
				 unsigned int length);
int mbim_start_xmit(struct sk_buff *skb, int ifid);
void trigger_mrdump_after_mdee(void);

#endif /* __CCCI_VENDOR_COMPAT_H__ */
