// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "medmcu_ipi.h"
#include "medmcu_ipi_table.h"
#include "medmcu_helper.h"

#define IPI_NO_USE 0xFF

void mbox_setup_pin_table(int mbox)
{
	int i, last_ofs = 0, last_idx = 0, last_slot = 0, last_sz = 0;

	for (i = 0; i < scp_mboxdev.recv_count; i++) {
		if (mbox == scp_mbox_pin_send[i].mbox) {
			scp_mbox_pin_send[i].offset = last_ofs + last_slot;
			scp_mbox_pin_send[i].pin_index = last_idx + last_sz;
			last_idx = scp_mbox_pin_send[i].pin_index;
			if (scp_mbox_info[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
					   scp_mbox_pin_send[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = scp_mbox_pin_send[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < scp_mbox_pin_send[i].mbox)
			break; /* no need to search the rest ipi */
	}

	for (i = 0; i < scp_mboxdev.recv_count; i++) {
		if (mbox == scp_mbox_pin_recv[i].mbox) {
			scp_mbox_pin_recv[i].offset = last_ofs + last_slot;
			scp_mbox_pin_recv[i].pin_index = last_idx + last_sz;
			last_idx = scp_mbox_pin_recv[i].pin_index;
			if (scp_mbox_info[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
					   scp_mbox_pin_recv[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = scp_mbox_pin_recv[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < scp_mbox_pin_recv[i].mbox)
			break; /* no need to search the rest ipi */
	}


	if (last_idx > 32 ||
	   (last_ofs + last_slot) > (scp_mbox_info[mbox].is64d + 1) * 32) {
		pr_err("mbox%d ofs(%d)/slot(%d) exceed the maximum\n",
			mbox, last_idx, last_ofs + last_slot);
		WARN_ON(1);
	}
}

/*
 * API to dump wakeup reason for ipi in detailed
 */
void mt_print_scp_ipi_id(unsigned int mbox)
{
	unsigned int irq_status, i;

	irq_status = mtk_mbox_read_recv_irq(&scp_mboxdev, mbox);

	for (i = 0; i < scp_mboxdev.recv_count; i++) {
		if (scp_mbox_pin_recv[i].mbox == mbox) {
			if (irq_status & (1 << scp_mbox_pin_recv[i].pin_index)) {
				pr_info("[SCP] mbox%u, ipi id %u\n",
					mbox,
					scp_mbox_pin_recv[i].chan_id);
			}
		}
	}
}
