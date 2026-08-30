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

#include "hnat_ioctl.h"
#include "hnat_define.h"
#include "foe_fdb.h"
#include <linux/socket.h>
#include <linux/inetdevice.h>
#include <net/rtnetlink.h>
#include <net/netevent.h>

#ifdef MTK_HNAT_WIFI_ROAMING_SUPPORT
static struct socket *_hnat_roam_sock;
static struct work_struct _hnat_roam_work;
static unsigned char _hnat_netlink_rcv_buf[1024];

static void hnat_roam_handler(struct work_struct *work)
{
	int len, ifindex;
	struct kvec iov;
	struct msghdr msg;
	struct nlmsghdr *nlh;
	struct ndmsg *ndm;
	struct nlattr *nla;
	u8 mac[ETH_ALEN];

	if (!_hnat_roam_sock)
		return;

	iov.iov_base = _hnat_netlink_rcv_buf;
	iov.iov_len = sizeof(_hnat_netlink_rcv_buf);
	memset(&msg, 0, sizeof(msg));
	msg.msg_namelen = sizeof(struct sockaddr_nl);

	len = kernel_recvmsg(_hnat_roam_sock, &msg, &iov, 1, iov.iov_len, 0);
	if (len <= 0)
		goto out;

	nlh = (struct nlmsghdr *)_hnat_netlink_rcv_buf;
	if (!NLMSG_OK(nlh, len) || nlh->nlmsg_type != RTM_NEWNEIGH)
		goto out;

	len = nlh->nlmsg_len - NLMSG_HDRLEN;
	ndm = (struct ndmsg *)NLMSG_DATA(nlh);
	if (ndm->ndm_family != PF_BRIDGE)
		goto out;

	ifindex = ndm->ndm_ifindex;
	nla = (struct nlattr *)((unsigned char *)ndm + sizeof(struct ndmsg));
	len -= NLMSG_LENGTH(sizeof(struct ndmsg));
	while (nla_ok(nla, len)) {
		if (nla_type(nla) == NDA_LLADDR) {
			ether_addr_copy(mac, nla_data(nla), ETH_ALEN);

			if (debug_level >= 3)
				pr_info("%s, foe_del_entry_by_mac\n", __func__);

			foe_del_entry_by_mac(mac);
		}
		nla = nla_next(nla, &len);
	}

out:
	schedule_work(&_hnat_roam_work);
}

static int hnat_roaming_enable(void)
{
	struct socket *sock = NULL;
	struct sockaddr_nl addr;
	int ret;

	ret = sock_create_kern(&init_net, AF_NETLINK, SOCK_RAW, NETLINK_ROUTE, &sock);
	if (ret < 0)
		goto out;

	_hnat_roam_sock = sock;

	INIT_WORK(&_hnat_roam_work, hnat_roam_handler);

	addr.nl_family = AF_NETLINK;
	addr.nl_pad = 0;
	addr.nl_pid = 65534;
	addr.nl_groups = 1 << (RTNLGRP_NEIGH - 1);
	ret = kernel_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0)
		goto out;

	schedule_work(&_hnat_roam_work);

	return 0;

out:
	if (sock)
		sock_release(sock);

	return ret;
}

static void hnat_roaming_disable(void)
{
	if (_hnat_roam_sock)
		sock_release(_hnat_roam_sock);
	_hnat_roam_sock = NULL;
}
#endif /* MTK_HNAT_WIFI_ROAMING_SUPPORT */

int hnat_netdevice_event(struct notifier_block *unused, unsigned long event, void *ptr)
{
	struct net_device *dev;

	dev = netdev_notifier_info_to_dev(ptr);
	switch (event) {
	case NETDEV_GOING_DOWN:

		if (debug_level >= 3)
			pr_info("%s(): intf (%s) is going down, clear foe entries\n", __func__, dev->name);

		foe_del_entry_by_dev(dev);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block hnat_netdevice_nb __read_mostly = {
	.notifier_call = hnat_netdevice_event,
};

void hnat_monitor_enable(void)
{
#ifdef MTK_HNAT_WIFI_ROAMING_SUPPORT
	hnat_roaming_enable();
#endif /* MTK_HNAT_WIFI_ROAMING_SUPPORT */

	register_netdevice_notifier(&hnat_netdevice_nb);
}

void hnat_monitor_disable(void)
{
#ifdef MTK_HNAT_WIFI_ROAMING_SUPPORT
	hnat_roaming_disable();
#endif /* MTK_HNAT_WIFI_ROAMING_SUPPORT */

	unregister_netdevice_notifier(&hnat_netdevice_nb);
}

