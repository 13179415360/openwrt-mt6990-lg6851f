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

#ifndef _UTIL_WANTED
#define _UTIL_WANTED

#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include "foe_fdb.h"
#include "frame_engine.h"

/*DEFINITIONS AND MACROS*/
#define reg_read(phys)	(__raw_readl((void __iomem *)phys))
#define reg_write(phys, val)	(__raw_writel(val, (void __iomem *)phys))

/* EXPORT FUNCTION*/
uint8_t *ip_to_str(uint32_t ip);
void mac_reverse(uint8_t *mac);
void reg_modify_bits(unsigned int *addr, uint32_t data, uint32_t offset, uint32_t len);
void cal_ip_range(u32 start_ip, uint32_t end_ip, uint8_t *M, uint8_t *E);
void foe_to_org_tcphdr(IN struct foe_entry *entry, IN struct iphdr *iph,
		       OUT struct tcphdr *th);
void foe_to_org_udphdr(IN struct foe_entry *entry, IN struct iphdr *iph,
		       OUT struct udphdr *uh);
void foe_to_org_iphdr(IN struct foe_entry *entry, OUT struct iphdr *iph);
void hwnat_memcpy(void *dest, void *src, u32 n);

#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
#define PROC_STRUCT proc_ops
#define PROC_OWNER /* empty */
#define PROC_OPEN proc_open
#define PROC_READ proc_read
#define PROC_SEEK proc_lseek
#define PROC_WRITE proc_write
#define PROC_RELEASE proc_release
#else /* (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE) */
#define PROC_STRUCT file_operations
#define PROC_OWNER .owner = THIS_MODULE,
#define PROC_OPEN open
#define PROC_READ read
#define PROC_SEEK llseek
#define PROC_WRITE write
#define PROC_RELEASE release
#endif /* (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE) */

#endif
