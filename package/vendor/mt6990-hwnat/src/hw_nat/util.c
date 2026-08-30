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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/in.h>

#include "util.h"

void mac_reverse(uint8_t *mac)
{
	u8 tmp;
	u8 i;

	for (i = 5; i > 2; i--) {
		tmp = mac[i];
		mac[i] = mac[5 - i];
		mac[5 - i] = tmp;
	}
}

int get_next(char *src, int separator, char *dest)
{
	char *c;
	int len = 0;

	if ((!src) || (!dest))
		return -1;

	c = strchr(src, separator);
	if (!c) {
		strncpy(dest, src, len);
		return -1;
	}
	len = c - src;
	strncpy(dest, src, len);
	dest[len] = '\0';
	return len + 1;
}

/* Convert IP address from Hex to string */
uint8_t *ip_to_str(IN uint32_t ip)
{
	static u8 buf[32];
	u8 *ptr = (char *)&ip;
	u8 c[4];

	c[0] = *(ptr);
	c[1] = *(ptr + 1);
	c[2] = *(ptr + 2);
	c[3] = *(ptr + 3);
	sprintf(buf, "%d.%d.%d.%d", c[3], c[2], c[1], c[0]);
	return buf;
}

/* calculate ip address range */
/* start_ip <= x < end_ip */
void cal_ip_range(u32 start_ip, uint32_t end_ip, uint8_t *M, uint8_t *E)
{
	u32 range = (end_ip + 1) - start_ip;
	u32 i;

	for (i = 0; i < 32; i++) {
		if ((range >> i) & 0x01)
			break;
	}

	if (i != 32) {
		*M = range >> i;
		*E = i;
	} else {
		*M = 0;
		*E = 0;
	}
}

void reg_modify_bits(unsigned int *addr, uint32_t data, uint32_t offset, uint32_t len)
{
	unsigned int mask = 0;
	unsigned int value;
	unsigned int i;

	for (i = 0; i < len; i++)
		mask |= 1 << (offset + i);

	value = reg_read(addr);
	value &= ~mask;
	value |= (data << offset) & mask;
	reg_write(addr, value);
}

static inline uint16_t csum_part(u32 o, uint32_t n, uint16_t old)
{
	u32 d[] = { o, n };

	return csum_fold(csum_partial((char *)d, sizeof(d), old ^ 0xFFFF));
}

/*KeepAlive with new header mode will pass the modified packet to cpu.*/
/* We must change to original packet to refresh NAT table.*/
/*Recover TCP Src/Dst Port and recalculate tcp checksum*/
void
foe_to_org_tcphdr(IN struct foe_entry *entry, IN struct iphdr *iph,
		  OUT struct tcphdr *th)
{
	/* TODO: how to recovery 6rd/dslite packet */
	th->check =
	    csum_part((th->source) ^ 0xffff,
		      htons(entry->ipv4_hnapt.sport), th->check);
	th->check =
	    csum_part((th->dest) ^ 0xffff,
		      htons(entry->ipv4_hnapt.dport), th->check);
	th->check =
	    csum_part(~(iph->saddr), htonl(entry->ipv4_hnapt.sip),
		      th->check);
	th->check =
	    csum_part(~(iph->daddr), htonl(entry->ipv4_hnapt.dip),
		      th->check);
	th->source = htons(entry->ipv4_hnapt.sport);
	th->dest = htons(entry->ipv4_hnapt.dport);
}

/* Recover UDP Src/Dst Port and recalculate udp checksum */

void
foe_to_org_udphdr(IN struct foe_entry *entry, IN struct iphdr *iph,
		  OUT struct udphdr *uh)
{
	/* TODO: how to recovery 6rd/dslite packet */

	uh->check =
	    csum_part((uh->source) ^ 0xffff,
		      htons(entry->ipv4_hnapt.sport), uh->check);
	uh->check =
	    csum_part((uh->dest) ^ 0xffff,
		      htons(entry->ipv4_hnapt.dport), uh->check);
	uh->check =
	    csum_part(~(iph->saddr), htonl(entry->ipv4_hnapt.sip),
		      uh->check);
	uh->check =
	    csum_part(~(iph->daddr), htonl(entry->ipv4_hnapt.dip),
		      uh->check);
	uh->source = htons(entry->ipv4_hnapt.sport);
	uh->dest = htons(entry->ipv4_hnapt.dport);
}

 /* Recover Src/Dst IP and recalculate ip checksum*/

void foe_to_org_iphdr(IN struct foe_entry *entry, OUT struct iphdr *iph)
{
	/* TODO: how to recovery 6rd/dslite packet */
	iph->saddr = htonl(entry->ipv4_hnapt.sip);
	iph->daddr = htonl(entry->ipv4_hnapt.dip);
	iph->check = 0;
	iph->check = ip_fast_csum((unsigned char *)(iph), iph->ihl);
}

void hwnat_memcpy(void *dest, void *src, u32 n)
{
	ether_addr_copy(dest, src);
}
