/* SPDX-License-Identifier: GPL-2.0 */
/*
 * LG6851F Linux 6.18 minimal MediaTek EMI compatibility surface.
 *
 * This deliberately exposes only the types and entry points referenced by
 * the imported CCCI stack. It is not a replacement for the real MT6990 EMI
 * MPU driver.
 */
#ifndef _SOC_MEDIATEK_LG6851F_EMI_H
#define _SOC_MEDIATEK_LG6851F_EMI_H

struct reg_info_t {
	unsigned int offset;
	unsigned int value;
	unsigned int leng;
};

typedef void (*emimpu_md_handler)(unsigned int emi_id,
				  struct reg_info_t *dump,
				  unsigned int leng);

int mtk_emimpu_md_handling_register(emimpu_md_handler md_handling_func);
void mtk_clear_md_violation(void);

#endif /* _SOC_MEDIATEK_LG6851F_EMI_H */
