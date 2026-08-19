// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccmni.c
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   Cross Chip Modem Network Interface
 *
 * Author:
 * -------
 *   Anny.Hu(mtk80401)
 *
 ****************************************************************************/
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/addrconf.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/ip6_checksum.h>
#include <net/neighbour.h>
#include <net/sch_generic.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/sockios.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/preempt.h>
#include <linux/stacktrace.h>
#include <linux/ethtool.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include "ccmni.h"
#include "ccci_debug.h"
#include <linux/if_ether.h> /* LG6851F Stage5K: unconditional declaration */
#include <linux/if_vlan.h> /* LG6851F Stage5K: unconditional declaration */
#include <linux/u64_stats_sync.h> /* LG6851F Stage5K: unconditional declaration */
#include <net/gro.h> /* LG6851F Stage5K: unconditional declaration */

/* LG6851F Stage5K: net_device statistics use the public accessor. */
static long ccmni_dev_rx_dropped(struct net_device *dev)
{
	struct rtnl_link_stats64 temp;
	const struct rtnl_link_stats64 *stats = dev_get_stats(dev, &temp);

	return (long)stats->rx_dropped;
}

//#include "rps_perf.h"
#if defined(CCMNI_MET_DEBUG)
#include <mt-plat/met_drv.h>
#endif

#if defined(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT)
#include "medmcu_common.h"
void __iomem *med_base;
#define MTK_MED_BASE			(0x15B38000)
#define MTK_FE_RANGE			(0x20000)
#define MED_BASE                	(med_base)
#define MEDHW_SSR1_DST_RB0_WIDX		(MED_BASE + 0x90)
#define MEDHW_SSR1_DST_RB0_RIDX		(MED_BASE + 0x94)
#define MEDHW_SSR1_DST_RB0_DEC		(MED_BASE + 0xb4)
#define reg_read(phys)	(__raw_readl((void __iomem *)phys))
#define reg_write(phys, val)	(__raw_writel(val, (void __iomem *)phys))
struct MED_HNAT_INFO_HOST *med_info_base = NULL;
extern struct medmcu_desc_info_t *get_desc_info(void);
#endif

#if defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD) && defined(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT)
#include "mtk_ppe.h"
#include <linux/sysctl.h> /* LG6851F Stage5J: register_sysctl */
#include <linux/if_vlan.h> /* LG6851F Stage5J: VLAN declarations */
#include <net/gro.h> /* LG6851F Stage5J: GRO public API */
#endif

struct ccmni_ctl_block *ccmni_ctl_blk[MAX_MD_NUM];
struct dentry *dfs_dir;
struct proc_dir_entry *ccmni_proc_bridge_file;
static int br_mode = 0;
struct proc_dir_entry *ccmni_proc_eth_file;
static struct eth_pdu_info_t eth_pdu_info;
#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
struct proc_dir_entry *ccmni_proc_wan_devname;
static char ccmni_wan_devname[IFNAMSIZ] = { '\0' };
#endif
struct proc_dir_entry *ccmni_proc_dir;

/* Time in ns. This number must be less than 500ms. */
#ifdef ENABLE_WQ_GRO
long int gro_flush_timer __read_mostly = 2000000L;
#else
long int gro_flush_timer;
#endif

#define APP_VIP_MARK		0x80000000
#define DEV_OPEN                1
#define DEV_CLOSE               0

/* MED API to update TX/RX packets from HWNAT */
#if defined(CONFIG_HW_NAT) || defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD)
extern int (*ppe_get_dev_stats)(struct net_device *dev, struct rtnl_link_stats64 *storage);
extern void (*ppe_dev_register_hook)(struct net_device *dev);
extern void (*ppe_dev_unregister_hook)(struct net_device *dev);
#endif

struct eth_pdu_info_t get_eth_pdu_info(void)
{
	return eth_pdu_info;
}
EXPORT_SYMBOL(get_eth_pdu_info);

static unsigned long timeout_flush_num, clear_flush_num;

// enable by echo 1 > /sys/module/ccmni/parameters/ccmni_gro
static DEFINE_MUTEX(ccmni_gro_lock);
static int ccmni_gro = 1;
module_param(ccmni_gro, uint, 0644);
MODULE_PARM_DESC(ccmni_gro, "enable ccmni_gro");

// enable by echo 1 > /sys/module/ccmni/parameters/ccmni_stats_profile
static int ccmni_stats_profile;
module_param(ccmni_stats_profile, uint, 0644);
MODULE_PARM_DESC(ccmni_stats_profile, "enable ccmni_stats debugging");

// enable by echo 1 > /sys/module/ccmni/parameters/ccmni_rx_profile
static int ccmni_rx_profile;
module_param(ccmni_rx_profile, uint, 0644);
MODULE_PARM_DESC(ccmni_rx_profile, "enable ccmni_rx_profile debugging");

// enable by echo 1 > /sys/module/ccmni/parameters/ccmni_tx_profile
static int ccmni_tx_profile;
module_param(ccmni_tx_profile, uint, 0644);
MODULE_PARM_DESC(ccmni_tx_profile, "enable ccmni_tx_profile debugging");

/*
void set_ccmni_rps(unsigned long value)
{
	int i = 0;
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[0];

	for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++)
		set_rps_map(ctlb->ccmni_inst[i]->dev->_rx, value);
}
EXPORT_SYMBOL(set_ccmni_rps);
*/

/********************internal function*********************/
/*
 * Register the sysctl to set tcp_pacing_shift.
 */
static int sysctl_tcp_pacing_shift = 6;
static struct ctl_table tcp_pacing_table[] = {
	{
		.procname	= "tcp_pacing_shift",
		.data		= &sysctl_tcp_pacing_shift,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
	},
	{}
};
/* LG6851F Stage5J: removed legacy nested sysctl wrapper tcp_pacing_sysctl_root; path is supplied to register_sysctl(). */


static struct ctl_table_header *sysctl_header;
static int register_tcp_pacing_sysctl(void)
{
	/* ccmni_init() may be reached more than once by the CCCI modem paths. */
	if (sysctl_header)
		return 0;

	sysctl_header = register_sysctl("net", tcp_pacing_table);
	if (sysctl_header == NULL) {
		pr_warn("CCCI:CCMNI: optional tcp_pacing_shift sysctl unavailable; using default %d\n",
			sysctl_tcp_pacing_shift);
		return -EEXIST;
	}
	return 0;
}

static void unregister_tcp_pacing_sysctl(void)
{
	if (sysctl_header) {
		unregister_sysctl_table(sysctl_header);
		sysctl_header = NULL;
	}
}

#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
static void get_ccmni_orig_devname(struct net_device *dev, char orig_name[IFNAMSIZ]) {
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	int ret = 0;

	ret = scnprintf(orig_name, IFNAMSIZ, "ccmni%d", ccmni->index);
	if (ret < 0 || ret >= IFNAMSIZ) {
		CCMNI_INF_MSG(ccmni->md_id, "orig_name is invalid\n");
		return;
	}

	if (strncmp(orig_name, dev->name, strlen(dev->name)) != 0)
		CCMNI_INF_MSG(ccmni->md_id, "%s renamed from %s\n", dev->name, orig_name);

	return;
}
#endif

int is_ccmni_netdev(char *devname) {
	if (devname == NULL || !strlen(devname))
		return 0;
	if (!strncmp(devname, "ccmni", 5))
		return 1;
#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
	if (strlen(ccmni_wan_devname) && !strncmp(devname, ccmni_wan_devname, strlen(ccmni_wan_devname)))
		return 1;
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(is_ccmni_netdev);

int get_ccmni_orig_index(struct net_device *dev) {
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	return ccmni->index;
}
EXPORT_SYMBOL(get_ccmni_orig_index);

#ifdef ENABLE_DEFERRED_IP_ALLOCATION
static struct dhcp_addr_tbl dhcp_addr[DHCP_MAX_ENTRIES];
static int dhcp_num_entries = 0;
static void add_dhcp_addr(unsigned char *mac, __be32 ip) {
	int i;
	if (dhcp_num_entries < DHCP_MAX_ENTRIES) {
		for (i = 0; i < dhcp_num_entries; i++) {
			if (ip == dhcp_addr[i].ip_addr) {
				CCMNI_INF_MSG(0, "%s %d: Client IP=%pI4 MAC=%pM->%pM\n",
						__func__, i, &ip, dhcp_addr[i].mac_addr, mac);
				ether_addr_copy(dhcp_addr[i].mac_addr, mac);
				return;
			}
			if (ether_addr_equal(mac, dhcp_addr[i].mac_addr)) {
				CCMNI_INF_MSG(0, "%s %d: Client IP=%pI4->%pI4 MAC=%pM\n",
						__func__, i, &dhcp_addr[i].ip_addr, &ip, mac);
				dhcp_addr[i].ip_addr = ip;
				return;
			}
		}
		ether_addr_copy(dhcp_addr[dhcp_num_entries].mac_addr, mac);
		dhcp_addr[dhcp_num_entries].ip_addr = ip;
		CCMNI_INF_MSG(0, "%s %d: Client IP=%pI4 MAC=%pM\n",
				__func__, dhcp_num_entries, &ip, mac);
		dhcp_num_entries++;
	}
}

static void clear_dhcp_addr(void) {
	int i;
	for (i = 0; i < dhcp_num_entries; i++) {
		eth_zero_addr(dhcp_addr[i].mac_addr);
		dhcp_addr[dhcp_num_entries].ip_addr = 0;
		dhcp_num_entries++;
	}
	dhcp_num_entries = 0;
}

static int find_dhcp_index_by_ip(__be32 ipv4) {
	int i = 0;
	for (i = 0; i < dhcp_num_entries; i++) {
		//CCMNI_INF_MSG(0, "%s compare %pI4 to %pI4\n", __func__, &ipv4, &dhcp_addr[i].ip_addr);
		if (ipv4 == dhcp_addr[i].ip_addr)
			return i;
	}
	return -1;
}

static void handle_dhcp_offer(struct sk_buff *skb, struct ccmni_instance *ccmni)
{
	u32 packet_type;

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV4_VERSION) {
		struct iphdr *iph = (struct iphdr *)skb->data;
		struct udphdr *udph;

		if (iph && iph->protocol != IPPROTO_UDP)
			return;
		udph = (struct udphdr *)((char *)iph + iph->ihl * 4);
		// Check if the UDP packet is a DHCP reply

		if (udph && htons(udph->source) == DHCP_SERVER_PORT
				&& htons(udph->dest) == DHCP_CLIENT_PORT) {
			__be32 your_client_ip;
			unsigned char client_mac[ETH_ALEN];
			struct dhcp_packet *dhcp_pkt;

			dhcp_pkt = (struct dhcp_packet *)((u8 *)udph + sizeof(struct udphdr));

			/* DHCP REPLY */
			if (dhcp_pkt->op != BOOTREPLY)
				return; // Not a DHCP Offer

			// Extract client IP address and client MAC address
			your_client_ip = dhcp_pkt->yiaddr;
			ether_addr_copy(client_mac, dhcp_pkt->chaddr);
			add_dhcp_addr(client_mac, your_client_ip);
		}
	}
}
#endif

static unsigned char fake_ccmni_mac[6] = { 0x06, 0x16, 0x26, 0x36, 0x46, 0x56 };

static bool ccmni_is_neigh_solicit(struct ipv6hdr *iph)
{
	// TODO: need to consider ext. hdrs
	struct nd_msg *msg = (void *)(iph + 1);

	return (iph->nexthdr == IPPROTO_ICMPV6 && msg->icmph.icmp6_type ==
			NDISC_NEIGHBOUR_SOLICITATION && msg->icmph.icmp6_code == 0);
}

static int ccmni_is_multicast_addr(void *iph, int packet_type)
{
	if (packet_type == 0x60)
		return ipv6_addr_is_multicast(&((struct ipv6hdr *)iph)->daddr);
	else
		return ipv4_is_multicast(((struct iphdr *)iph)->daddr);
}

static void ccmni_make_multicast_frame(void* iph, struct ethhdr *eth_hdr,
		unsigned int packet_type)
{
	if (packet_type == 0x60) {
		ipv6_eth_mc_map(&((struct ipv6hdr *)iph)->daddr, eth_hdr->h_dest);
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IPV6);
	} else {
		ip_eth_mc_map(((struct iphdr *)iph)->daddr, eth_hdr->h_dest);
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IP);
	}
	ether_addr_copy(eth_hdr->h_source, fake_ccmni_mac);
}

static bool ccmni_is_arp_proxy_addr(int md_id, struct net_device *master, __be32 dest_ip)
{
	struct in_device *in_dev;
	const struct in_ifaddr *ifa;
	bool is_reply = false;

	in_dev = __in_dev_get_rtnl(master);
	if (!in_dev) {
		net_err_ratelimited("No inet device\n");
		return false;
	}

	/*
	* Only run ARP reply for gateway.
	* The gateway address is offset of ODU address by 1.
	* Fake ARP reply for requesting IP (Who has XXX IP?)
	* 1. not in same subnet
	* 2. all same subnet IP except itself
	* IP from modem is assigned to brlan
	* The host PC address is offset of modem IP by 2
	* The gateway address is offset of host PC/modem IP by 1 (in between).
	*
	* ebtables can be used to drop ARP reply
	* ebtables -t nat -I POSTROUTING -p arp --src 06:16:26:36:46:56 --arp-ip-src=192.168.0.1/24 --arp-opcode Reply -j DROP
	*/
	in_dev_for_each_ifa_rcu(ifa, in_dev) {
		u32 s_addr;  // ifa = brlan
		u32 d_addr;  // Who has XXX ip?
		bool same_subnet = false;

		same_subnet = inet_ifa_match(dest_ip, ifa);
		CCMNI_INF_MSG(md_id, "subnet:%pI4 dest_ip:%pI4 same_subnet=%d",
				&ifa->ifa_address, &dest_ip, same_subnet);

		/* No match for subnet */
		if (!same_subnet) {
			is_reply = true;
			break;
		}

		s_addr = be32_to_cpu(ifa->ifa_address);
		d_addr = be32_to_cpu(dest_ip);
		/* No match for address */
		if (d_addr == s_addr)  // dest_ip is brlan itself
			break;

		is_reply = true;
	}

	return is_reply;
}

static int ccmni_eth_pdu_arp_log(int md_id, struct net_device *dev,
		struct sk_buff *skb)
{
	u8 *arp_ptr, *src_hw;
	__be32 src_ip, dest_ip;
	struct arphdr *request;

	request = arp_hdr(skb);
	if (request->ar_pro != htons(ETH_P_IP) ||
			request->ar_op != htons(ARPOP_REQUEST) ||
			request->ar_hln != ETH_ALEN ||
			request->ar_pln != 4) {
		CCMNI_INF_MSG(md_id, "EthPDU: tx: abnormal arp\n");
		goto drop_abnormal_arp;
	}

	// Refer to arp_create for the addresses allocation
	arp_ptr = (u8 *)request + sizeof(struct arphdr);
	src_hw = arp_ptr;
	arp_ptr += dev->addr_len;
	memcpy(&src_ip, arp_ptr, sizeof(src_ip));
	arp_ptr += sizeof(src_ip);
	arp_ptr += dev->addr_len;
	memcpy(&dest_ip, arp_ptr, sizeof(dest_ip));

	CCMNI_INF_MSG(md_id, "EthPDU: tx: normal arp: src_hw:%pM src_ip:%x dest_ip:%x \n",
			src_hw, src_ip, dest_ip);

	// Skip ARP probe and announcement
	if (!src_ip || dest_ip == src_ip) {
		CCMNI_INF_MSG(md_id, "EthPDU: tx: arp probe and announcement\n");
	}

drop_abnormal_arp:
	return 1;
}

static int ccmni_handle_arp_request(int md_id, struct net_device *dev,
		struct sk_buff *skb)
{
	u8 *arp_ptr, *src_hw;
	__be32 src_ip, dest_ip;
	struct arphdr *request, *reply;
	struct sk_buff *reply_skb;
	struct ethhdr *new_eth;
	struct ccmni_instance *ccmni = (struct ccmni_instance *)netdev_priv(dev);
	struct net_device *master;
#ifdef ENABLE_DEFERRED_IP_ALLOCATION
	int idx = -1;
#endif

	if (!rtnl_is_locked()) {
		if (!rtnl_trylock())
			goto drop_abnormal_arp;
		master = netdev_master_upper_dev_get(dev);
		rtnl_unlock();
	} else {
		master = netdev_master_upper_dev_get(dev);
	}
	if (!master) {
		CCMNI_INF_MSG(md_id, "No master device\n");
		goto drop_abnormal_arp;
	}

	request = arp_hdr(skb);
	if (request->ar_pro != htons(ETH_P_IP) ||
			request->ar_op != htons(ARPOP_REQUEST) ||
			request->ar_hln != ETH_ALEN ||
			request->ar_pln != 4) {
		CCMNI_INF_MSG(md_id, "%s abnormal arp from %s\n", __func__, dev->name);
		goto drop_abnormal_arp;
	}

	// Refer to arp_create for the addresses allocation
	arp_ptr = (u8 *)request + sizeof(struct arphdr);
	src_hw = arp_ptr;
	arp_ptr += dev->addr_len;
	memcpy(&src_ip, arp_ptr, sizeof(src_ip));
	arp_ptr += sizeof(src_ip);
	arp_ptr += dev->addr_len;
	memcpy(&dest_ip, arp_ptr, sizeof(dest_ip));

	if (ipv4_is_loopback(dest_ip) ||
		ipv4_is_multicast(dest_ip)||
		ipv4_is_linklocal_169(dest_ip)) {
		CCMNI_INF_MSG(md_id, "%s abnormal arp mac:%pM Target:%pI4 from %s\n", __func__,
			src_hw, &dest_ip, dev->name);
			goto drop_abnormal_arp;
	}

	// Skip ARP probe and announcement
	if (!src_ip || dest_ip == src_ip) {
		CCMNI_INF_MSG(md_id, "%s ignore arp probe and announcement mac:%pM Target:%pI4 from %s\n", __func__,
			src_hw, &dest_ip, dev->name);
		goto drop_ignored_arp;
	}

#ifdef ENABLE_DEFERRED_IP_ALLOCATION
	idx = find_dhcp_index_by_ip(src_ip);
	if (idx >= 0 &&	ether_addr_equal(dhcp_addr[idx].mac_addr, src_hw))
		goto arp_reply;
#endif

	/*
	 * Only run ARP reply for gateway or
	 * nodes are not belonged to the same subnet
	 */
	if (!ccmni_is_arp_proxy_addr(md_id, master, dest_ip)) {
		CCMNI_INF_MSG(md_id, "%s ignore arp within same subnet mac:%pM Target:%pI4 from %s\n", __func__,
			src_hw, &dest_ip, dev->name);
		goto drop_ignored_arp;
	}

	// Store the src mac if this arp is not from the bridge
	if (!ether_addr_equal(src_hw, master->dev_addr))
		ether_addr_copy(ccmni->idu_mac_addr, src_hw);

	CCMNI_INF_MSG(-1, "%s IDU mac:%pM\n", __func__, ccmni->idu_mac_addr);

#ifdef ENABLE_DEFERRED_IP_ALLOCATION
arp_reply:
#endif
	CCMNI_INF_MSG(md_id, "%s Handle ARP mac:%pM Target:%pI4 from %s\n", __func__,
			src_hw, &dest_ip, dev->name);

	// Ack the REQUEST with the ARP_REPLY
	reply_skb = netdev_alloc_skb_ip_align(dev, arp_hdr_len(dev));
	if (!reply_skb) {
		CCMNI_INF_MSG(md_id, "%s abnormal arp reply mac:%pM Target:%pI4 from %s\n", __func__,
			src_hw, &dest_ip, dev->name);
		goto drop_abnormal_arp;
	}

	reply = (struct arphdr *)reply_skb->data;
	reply->ar_hrd = htons(ARPHRD_ETHER);
	reply->ar_pro = htons(ETH_P_IP);
	reply->ar_hln = ETH_ALEN;
	reply->ar_pln = 4;
	reply->ar_op = htons(ARPOP_REPLY);

	arp_ptr = (unsigned char *)(reply + 1);
	memcpy(arp_ptr, fake_ccmni_mac, dev->addr_len);
	arp_ptr += dev->addr_len;
	memcpy(arp_ptr, &dest_ip, 4);
	arp_ptr += 4;
	memcpy(arp_ptr, src_hw, dev->addr_len);
	arp_ptr += dev->addr_len;
	memcpy(arp_ptr, &src_ip, 4);

	new_eth = (struct ethhdr *)(reply_skb->data - ETH_HLEN);
	new_eth->h_proto = htons(ETH_P_ARP);
	ether_addr_copy(new_eth->h_dest, src_hw);
	ether_addr_copy(new_eth->h_source, fake_ccmni_mac);

	reply_skb->len = 28;
	skb_set_tail_pointer(reply_skb, reply_skb->len);

	skb_set_mac_header(reply_skb, -ETH_HLEN);
	skb_reset_network_header(reply_skb);
	reply_skb->mac_len = ETH_HLEN;
	reply_skb->protocol = htons(ETH_P_ARP);
	reply_skb->dev = dev;
	netif_receive_skb(reply_skb);
	dev_kfree_skb(skb);

	return 0;

drop_ignored_arp:
drop_abnormal_arp:
	return 1;
}

static bool ccmni_is_odu_v6_addr(struct net_device *dev, struct in6_addr *addr)
{
	struct net_device *master;
	struct inet6_dev *idev;
	bool is_match = false;

	if (!rtnl_is_locked()) {
		if (!rtnl_trylock())
			return false;
		master = netdev_master_upper_dev_get(dev);
		rtnl_unlock();
	} else {
		master = netdev_master_upper_dev_get(dev);
	}

	if (!master)
		return false;

	rcu_read_lock();
	idev = __in6_dev_get(master);
	if (idev != NULL) {
		struct inet6_ifaddr *ifp;

		read_lock_bh(&idev->lock);
		list_for_each_entry(ifp, &idev->addr_list, if_list) {
			if (ipv6_addr_equal(&ifp->addr, addr)) {
				is_match = true;
				break;
			}
		}
		read_unlock_bh(&idev->lock);
	}
	rcu_read_unlock();

	return is_match;
}

static int ccmni_handle_neigh_solicit(struct net_device *dev, struct sk_buff *skb)
{
	struct sk_buff *reply;
	struct nd_msg *ns, *na;
	struct ipv6hdr *ip6h;
	struct ccmni_instance *ccmni = (struct ccmni_instance *)netdev_priv(dev);
	int na_olen = 8; // opt hdr + ETH_ALEN for target
	int len;

	/* Ignore target with non-link local address */
	ns = (struct nd_msg *)(ipv6_hdr(skb) + 1);
	if (!(ipv6_addr_type(&ns->target) & IPV6_ADDR_LINKLOCAL))
		goto drop_ignored_ns;

	CCMNI_INF_MSG(-1, "%s Handle NS mac:%pM Target:%pI6 from %s\n", __func__,
			eth_hdr(skb)->h_source, &ns->target, dev->name);

	/* Store the src mac if this IPv6 NS is not from the ODU */
	if (!ether_addr_equal(eth_hdr(skb)->h_source, dev->dev_addr))
		ether_addr_copy(ccmni->idu_mac_addr, eth_hdr(skb)->h_source);

	CCMNI_INF_MSG(-1, "%s IDU mac:%pM\n", __func__, ccmni->idu_mac_addr);

	/* This is ODU packet. No need to reply */
	if (ccmni_is_odu_v6_addr(dev, &ns->target))
		goto drop_ignored_ns;

	len = LL_RESERVED_SPACE(dev) + sizeof(struct ipv6hdr) +
		sizeof(*na) + na_olen + dev->needed_tailroom;
	reply = netdev_alloc_skb_ip_align(dev, len);
	if (!reply)
		goto drop_abnormal_ns;

	reply->protocol = htons(ETH_P_IPV6);
	reply->dev = dev;
	skb_reserve(reply, LL_RESERVED_SPACE(dev));
	skb_push(reply, sizeof(struct ethhdr));
	skb_set_mac_header(reply, 0);

	// Ethernet header
	ether_addr_copy(eth_hdr(reply)->h_dest, eth_hdr(skb)->h_source);
	ether_addr_copy(eth_hdr(reply)->h_source, fake_ccmni_mac);
	eth_hdr(reply)->h_proto = htons(ETH_P_IPV6);
	reply->protocol = htons(ETH_P_IPV6);

	skb_pull(reply, sizeof(struct ethhdr));
	skb_set_network_header(reply, 0);
	skb_put(reply, sizeof(struct ipv6hdr));

	// IPv6 header
	ip6h = ipv6_hdr(reply);
	memset(ip6h, 0, sizeof(struct ipv6hdr));
	ip6h->version = 6;
	ip6h->priority = ipv6_hdr(skb)->priority;
	ip6h->nexthdr = IPPROTO_ICMPV6;
	ip6h->hop_limit = 255;
	ip6h->daddr = ipv6_hdr(skb)->saddr;
	ip6h->saddr = ns->target;

	skb_pull(reply, sizeof(struct ipv6hdr));
	skb_set_transport_header(reply, 0);

	// Neighbor Advertisement
	na = (struct nd_msg *)skb_put(reply, sizeof(*na) + na_olen);
	memset(na, 0, sizeof(*na) + na_olen);
	na->icmph.icmp6_type = NDISC_NEIGHBOUR_ADVERTISEMENT;
	na->icmph.icmp6_router = 1;
	na->icmph.icmp6_override = 1;
	na->icmph.icmp6_solicited = 1;
	na->target = ns->target;
	ether_addr_copy(&na->opt[2], fake_ccmni_mac);
	na->opt[0] = ND_OPT_TARGET_LL_ADDR;
	na->opt[1] = na_olen >> 3;

	na->icmph.icmp6_cksum = csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr,
			sizeof(*na) + na_olen, IPPROTO_ICMPV6, csum_partial(na, sizeof(*na)
				+ na_olen, 0));

	ip6h->payload_len = htons(sizeof(*na) + na_olen);

	skb_push(reply, sizeof(struct ipv6hdr));
	reply->ip_summed = CHECKSUM_UNNECESSARY;

	netif_receive_skb(reply);
	dev_kfree_skb(skb);
	return 0;

drop_ignored_ns:
drop_abnormal_ns:
	return 1;
}

#if defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD)
static void get_mac_address(unsigned char *dst_mac, struct sk_buff *skb) {
	struct neighbour *neigh = NULL;
	struct ipv6hdr *iph6 = (struct ipv6hdr *)skb->data;
	struct flowi6 flow = {};
	struct dst_entry *dst;
	struct net_device *br_dev = NULL;

	if (!br_dev)
		br_dev = dev_get_by_name(&init_net, "brlan0");
	if (!br_dev)
		br_dev = dev_get_by_name(&init_net, "br-lan");
	if (!br_dev)
		return;

	rcu_read_lock();
	flow.daddr = iph6->daddr;
#if IS_ENABLED(CONFIG_INET) && IS_ENABLED(CONFIG_IPV6)
	dst = ipv6_stub->ipv6_dst_lookup_flow(dev_net(br_dev), NULL, &flow, NULL);
	if (IS_ERR(dst)) {
		//CCMNI_INF_MSG(0, "%s dst(%pI6) is NULL\n", __func__, &iph6->daddr);
		goto fail_rcu_unlock;
	}
#else
	goto fail_rcu_unlock;
#endif

	neigh = dst_neigh_lookup(dst, &iph6->daddr);
	dst_release(dst);
	if (neigh == NULL) {
		//CCMNI_INF_MSG(0, "%s neigh is NULL\n", __func__);
		goto fail_rcu_unlock;
	}

	//CCMNI_INF_MSG(0, "%s nud_state=%d\n", __func__, neigh->nud_state);
	if (neigh->nud_state & NUD_VALID) {
		ether_addr_copy(dst_mac, neigh->ha);
		//CCMNI_INF_MSG(0, "%s mac:[%pM] ip:[%pI6]\n", __func__, dst_mac, &iph6->daddr);
	}
	// Release the reference to the neighbor
	neigh_release(neigh);

fail_rcu_unlock:
	rcu_read_unlock();
	dev_put(br_dev);
}
#endif

static void ccmni_make_etherframe(struct net_device *dev,
	struct sk_buff *skb, unsigned int packet_type)
{
	unsigned char mac_addr[ETH_HLEN] = {0};
	void *_eth_hdr = skb->data - ETH_HLEN;
	struct ethhdr *eth_hdr = _eth_hdr;

#if defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD)
	if (packet_type == IPV6_VERSION) {
		struct ipv6hdr *ipv6hdr = (struct ipv6hdr *)(skb->data);
		//CCMNI_INF_MSG(0, "%s protocol=%d\n", __func__, ipv6hdr->nexthdr);
		if (ipv6hdr->nexthdr == IPPROTO_UDP)
			get_mac_address(mac_addr, skb);
	}
#endif

	if (br_mode) {
		struct ccmni_instance *ccmni = (struct ccmni_instance *)netdev_priv(dev);
#ifdef ENABLE_DEFERRED_IP_ALLOCATION
		if (packet_type == IPV4_VERSION) {
			void *iph = skb->data;
			__be32 client_ip = ((struct iphdr *)iph)->daddr;
			int idx = find_dhcp_index_by_ip(client_ip);
			if (idx >= 0)
				ether_addr_copy(mac_addr, dhcp_addr[idx].mac_addr);
		}
#endif
		if (is_zero_ether_addr(mac_addr))
			ether_addr_copy(mac_addr, ccmni->idu_mac_addr);
	}
	if (is_zero_ether_addr(mac_addr))
		ether_addr_copy(eth_hdr->h_dest, dev->dev_addr);
	else
		ether_addr_copy(eth_hdr->h_dest, mac_addr);

	ether_addr_copy(eth_hdr->h_source, fake_ccmni_mac);
	//CCMNI_INF_MSG(0, "%s src_mac[%pM] dst_mac[%pM]\n", __func__, eth_hdr->h_source, eth_hdr->h_dest);

	if (packet_type == IPV6_VERSION)
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IPV6);
	else
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IP);
}

static inline int is_ack_skb(int md_id, struct sk_buff *skb)
{
	u32 packet_type;
	struct tcphdr *tcph;
	int ret = 0;
	struct md_tag_packet *tag = NULL;
	unsigned int count = 0;

	tag = (struct md_tag_packet *)skb->head;
	if (tag->guard_pattern == MDT_TAG_PATTERN)
		count = sizeof(tag->info);

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV6_VERSION) {
		struct ipv6hdr *iph = (struct ipv6hdr *)skb->data;
		u32 total_len =
			sizeof(struct ipv6hdr)
			+
			ntohs(iph->payload_len);

		/* copy md tag into skb->tail and
		 * skb->len > 128B(md recv buf size)
		 */
		/* this case will cause md EE */
		if (total_len <= 128 - sizeof(struct ccci_header) - count) {
			u8 nexthdr = iph->nexthdr;
			__be16 frag_off;
			u32 l4_off = ipv6_skip_exthdr(skb,
				sizeof(struct ipv6hdr),
				&nexthdr, &frag_off);

			if (nexthdr == IPPROTO_TCP) {
				tcph = (struct tcphdr *)(skb->data + l4_off);

				if (tcph->syn)
					ret = 1;
				else if (!tcph->fin && !tcph->rst &&
					((total_len - l4_off) ==
						(tcph->doff << 2)))
					ret = 1;
			}
		}
	} else if (packet_type == IPV4_VERSION) {
		struct iphdr *iph = (struct iphdr *)skb->data;

		if (ntohs(iph->tot_len) <=
				128 - sizeof(struct ccci_header) - count) {

			if (iph->protocol == IPPROTO_TCP) {
				tcph = (struct tcphdr *)(skb->data + (iph->ihl << 2));

				if (tcph->syn)
					ret = 1;
				else if (!tcph->fin && !tcph->rst &&
					ntohs(iph->tot_len) ==
					(iph->ihl << 2) + (tcph->doff << 2)) {
					ret = 1;
				}
			}
		}
	}

	return ret;
}

#ifdef ENABLE_WQ_GRO
static int is_skb_gro(struct sk_buff *skb)
{
	int result;
	u32 packet_type;

	packet_type = skb->data[0] & 0xF0;
	mutex_lock(&ccmni_gro_lock);

	if (ccmni_gro == 0)
		result = 0;
	else if (ccmni_gro == 1)
		result = 1;
	else if ((packet_type == IPV4_VERSION && ip_hdr(skb)->protocol != IPPROTO_UDP) ||
			 (packet_type == IPV6_VERSION && ipv6_hdr(skb)->nexthdr != IPPROTO_UDP))
		result = 1;
	else
		result = 0;

	mutex_unlock(&ccmni_gro_lock);
	return result;
}

void ccmni_clr_flush_timer(void)
{
	int i = 0;
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[0];

	if (ctlb == NULL)
		return;

	for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++)
		if (ctlb->ccmni_inst[i] && ctlb->ccmni_inst[i]->dev)
			if (ctlb->ccmni_inst[i]->dev->flags & IFF_UP)
				ktime_get_real_ts64(&ctlb->ccmni_inst[i]->flush_time);

}
EXPORT_SYMBOL(ccmni_clr_flush_timer);

/*
 * Linux 6.18 keeps normal GRO packets in struct gro_node.  Existing migrated
 * call sites pass &napi->gro, so this local adapter must accept gro_node.
 */
/*
 * Linux 6.18 keeps the GRO_NORMAL state in struct gro_node.  Preserve the
 * vendor helper's ccmni_instance interface and derive the real NAPI GRO node
 * from ccmni_instance::napi.
 */
static void napi_gro_list_flush(struct ccmni_instance *ccmni)
{
	gro_flush_normal(&ccmni->napi->gro, false);
}

static void ccmni_gro_flush(struct ccmni_instance *ccmni)
{
	struct timespec64 curr_time, diff;

	if (!gro_flush_timer)
		return;

	ktime_get_real_ts64(&curr_time);
	diff = timespec64_sub(curr_time, ccmni->flush_time);
	if ((diff.tv_sec > 0) || (diff.tv_nsec > gro_flush_timer)) {
		napi_gro_list_flush(ccmni);
		timeout_flush_num++;
		ktime_get_real_ts64(&ccmni->flush_time);
	}
}
#endif

static inline int ccmni_forward_rx(struct ccmni_instance *ccmni,
	struct sk_buff *skb)
{
	bool flt_ok = false;
	bool flt_flag = true;
	unsigned int pkt_type;
	struct iphdr *iph;
	struct ipv6hdr *iph6;
	struct ccmni_fwd_filter flt_tmp;
	unsigned int i, j;
	u16 mask;
	u32 *addr1, *addr2;

	if (ccmni->flt_cnt) {
		for (i = 0; i < CCMNI_FLT_NUM; i++) {
			flt_tmp = ccmni->flt_tbl[i];
			pkt_type = skb->data[0] & 0xF0;
			if (!flt_tmp.ver || (flt_tmp.ver != pkt_type))
				continue;

			if (pkt_type == IPV4_VERSION) {
				iph = (struct iphdr *)skb->data;
				mask = flt_tmp.s_pref;
				addr1 = &iph->saddr;
				addr2 = &flt_tmp.ipv4.saddr;
				flt_flag = true;
				for (j = 0; flt_flag && j < 2; j++) {
					if (mask &&
						(addr1[0] >> (32 - mask) !=
						addr2[0] >> (32 - mask))) {
						flt_flag = false;
						break;
					}
					mask = flt_tmp.d_pref;
					addr1 = &iph->daddr;
					addr2 = &flt_tmp.ipv4.daddr;
				}
			} else if (pkt_type == IPV6_VERSION) {
				iph6 = (struct ipv6hdr *)skb->data;
				mask = flt_tmp.s_pref;
				addr1 = iph6->saddr.s6_addr32;
				addr2 = flt_tmp.ipv6.saddr;
				flt_flag = true;
				for (j = 0; flt_flag && j < 2; j++) {
					if (mask == 0) {
						mask = flt_tmp.d_pref;
						addr1 = iph6->daddr.s6_addr32;
						addr2 = flt_tmp.ipv6.daddr;
						continue;
					}
					if (mask <= 32 &&
						(addr1[0] >> (32 - mask) !=
						addr2[0] >> (32 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 64 &&
						(addr1[0] != addr2[0] ||
						addr1[1] >> (64 - mask) !=
						addr2[1] >> (64 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 96 &&
						(addr1[0] != addr2[0] ||
						addr1[1] != addr2[1] ||
						addr1[2] >> (96 - mask) !=
						addr2[2] >> (96 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 128 &&
						(addr1[0] != addr2[0] ||
						addr1[1] != addr2[1] ||
						addr1[2] != addr2[2] ||
						addr1[3] >> (128 - mask) !=
						addr2[3] >> (128 - mask))) {
						flt_flag = false;
						break;
					}
					mask = flt_tmp.d_pref;
					addr1 = iph6->daddr.s6_addr32;
					addr2 = flt_tmp.ipv6.daddr;
				}
			}
			if (flt_flag) {
				flt_ok = true;
				break;
			}
		}

		if (flt_ok) {
			skb->ip_summed = CHECKSUM_NONE;
			skb_set_mac_header(skb, -ETH_HLEN);

			if (!in_interrupt())
				netif_rx(skb);
			else
				netif_rx(skb);
			return NETDEV_TX_OK;
		}
	}

	return -1;
}


/********************netdev register function********************/
static u16 ccmni_select_queue(struct net_device *dev, struct sk_buff *skb,
	struct net_device *sb_dev/*, select_queue_fallback_t fallback */)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_ctl_block *ctlb = NULL;

	if (ccmni->md_id < 0 || ccmni->md_id >= MAX_MD_NUM) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id = %d\n", __func__, ccmni->md_id);
		return CCMNI_TXQ_NORMAL;
	}
	ctlb = ccmni_ctl_blk[ccmni->md_id];
	if (ctlb == NULL) {
		CCMNI_INF_MSG(ccmni->md_id, "%s : invalid ctlb\n", __func__);
		return CCMNI_TXQ_NORMAL;
	}
	if (ctlb->ccci_ops->md_ability & MODEM_CAP_DATA_ACK_DVD) {
		if (skb->mark & APP_VIP_MARK)
			return CCMNI_TXQ_FAST;

		if (ccmni->ack_prio_en && is_ack_skb(ccmni->md_id, skb))
			return CCMNI_TXQ_FAST;
		else
			return CCMNI_TXQ_NORMAL;
	} else
		return CCMNI_TXQ_NORMAL;
}

static void ccmni_get_stats64(struct net_device *dev,
		struct rtnl_link_stats64 *storage) {
#if defined(CONFIG_HW_NAT) || defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD)
	if (ppe_get_dev_stats) {
		struct rtnl_link_stats64 ppe_stats = {0};

		if (ppe_get_dev_stats(dev, &ppe_stats) != 1) {
			pr_err("%s: %s ppe_get_dev_stats returns err.\n", __func__,
					dev->name);
			return;
		}

		if (ccmni_stats_profile) {
			CCMNI_INF_MSG(0, "%s ppe_rx_pkts=%llu, tx_pkts=%llu, rx_bytes=%llu, tx_bytes=%llu\n",
					dev->name, ppe_stats.rx_packets, ppe_stats.tx_packets,
					ppe_stats.rx_bytes, ppe_stats.tx_bytes);
		}

		dev->stats.rx_packets += ppe_stats.rx_packets;
		dev->stats.tx_packets += ppe_stats.tx_packets;
		dev->stats.rx_bytes += ppe_stats.rx_bytes;
		dev->stats.tx_bytes += ppe_stats.tx_bytes;
		dev->stats.multicast += ppe_stats.multicast;
	}
#endif

	if (ccmni_stats_profile) {
		CCMNI_INF_MSG(0, "%s dev_rx_pkts=%lu, tx_pkts=%lu, rx_bytes=%lu, tx_bytes=%lu\n",
				dev->name, dev->stats.rx_packets, dev->stats.tx_packets,
				dev->stats.rx_bytes, dev->stats.tx_bytes);
	}

	netdev_stats_to_stats64(storage, &dev->stats);
}


#if defined(CONFIG_TUNNEL_FAST_PATH)
extern int (*ppe_hook_rx_modem_thread)(struct sk_buff *skb);
#endif

static int ccmni_open(struct net_device *dev)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_ctl_block *ccmni_ctl = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
	int usage_cnt = 0;

	if (ccmni->md_id < 0 || ccmni->md_id >= MAX_MD_NUM || ccmni->index < 0) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id or index:md_id = %d,index = %d\n",
			__func__, ccmni->md_id, ccmni->index);
		return -1;
	}
	ccmni_ctl = ccmni_ctl_blk[ccmni->md_id];
	if (unlikely(ccmni_ctl == NULL)) {
		CCMNI_PR_DBG(ccmni->md_id,
			"%s_Open: MD%d ctlb is NULL\n",
			dev->name, ccmni->md_id);
		return -1;
	}

	if (gro_flush_timer)
		ktime_get_real_ts64(&ccmni->flush_time);

	netif_carrier_on(dev);

	netif_tx_start_all_queues(dev);

	if (unlikely(ccmni_ctl->ccci_ops->md_ability & MODEM_CAP_NAPI)) {
		napi_enable(ccmni->napi);
		napi_schedule(ccmni->napi);
	}

	atomic_inc(&ccmni->usage);
	ccmni_tmp = ccmni_ctl->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp) {
		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_tmp->usage, usage_cnt);
	}
	queue_delayed_work(ccmni->worker,
				&ccmni->pkt_queue_work,
				msecs_to_jiffies(500));

#if defined(CONFIG_HW_NAT) || defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD)
	if (ppe_dev_register_hook)
		ppe_dev_register_hook(dev);
#endif

#if defined(CONFIG_TUNNEL_FAST_PATH)
	if (!ppe_hook_rx_modem_thread)
		CCMNI_INF_MSG(ccmni->md_id, "ppe_hook_rx_modem_thread not exist\n");
#endif

	CCMNI_INF_MSG(ccmni->md_id,
		"%s_Open:cnt=(%d,%d), md_ab=0x%X, gro=(%llx, %ld), flt_cnt=%d\n",
		dev->name, atomic_read(&ccmni->usage),
		atomic_read(&ccmni_tmp->usage),
		ccmni_ctl->ccci_ops->md_ability,
		dev->features, gro_flush_timer, ccmni->flt_cnt);

	return 0;
}

static int ccmni_close(struct net_device *dev)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_ctl_block *ccmni_ctl = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
	char orig_name[IFNAMSIZ] = { '\0' };
#endif
	int usage_cnt = 0, ret = 0;

#ifdef ENABLE_DEFERRED_IP_ALLOCATION
	clear_dhcp_addr();
#endif

	if (ccmni->md_id < 0 || ccmni->md_id >= MAX_MD_NUM || ccmni->index < 0) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id or index:md_id = %d,index = %d\n",
			__func__, ccmni->md_id, ccmni->index);
		return -1;
	}
	ccmni_ctl = ccmni_ctl_blk[ccmni->md_id];
	if (unlikely(ccmni_ctl == NULL)) {
		CCMNI_PR_DBG(ccmni->md_id, "%s_Close: MD%d ctlb is NULL\n",
			dev->name, ccmni->md_id);
		return -1;
	}

	cancel_delayed_work(&ccmni->pkt_queue_work);
	flush_delayed_work(&ccmni->pkt_queue_work);

	atomic_dec(&ccmni->usage);
	ccmni_tmp = ccmni_ctl->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp) {
		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_tmp->usage, usage_cnt);
	}

	netif_tx_disable(dev);

	if (unlikely(ccmni_ctl->ccci_ops->md_ability & MODEM_CAP_NAPI))
		napi_disable(ccmni->napi);

	memset(ccmni->idu_mac_addr, 0, MAX_ADDR_LEN);

#if defined(CONFIG_HW_NAT) || defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD)
	if (ppe_dev_unregister_hook)
		ppe_dev_unregister_hook(dev);
#endif

#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
	get_ccmni_orig_devname(dev, orig_name);
	ret = ccmni_ctl->ccci_ops->ccci_handle_port_list(DEV_CLOSE,
			strlen(orig_name) ? orig_name : ccmni->dev->name);
#else
	ret = ccmni_ctl->ccci_ops->ccci_handle_port_list(DEV_CLOSE, dev->name);
#endif
	CCMNI_INF_MSG(ccmni->md_id, "%s_Close:cnt=(%d, %d)\n",
			dev->name, atomic_read(&ccmni->usage),
			atomic_read(&ccmni_tmp->usage));

#ifdef ENABLE_PKT_STATISTICS_CLEANUP
	dev->stats.rx_packets = 0;
	dev->stats.tx_packets = 0;
	dev->stats.rx_bytes = 0;
	dev->stats.tx_bytes = 0;
	dev->stats.multicast = 0;
#endif

	return 0;
}

struct timespec64 starttime_ccmni_tx;
static unsigned long long sen_data_len;

static netdev_tx_t ccmni_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret = 0;
	int skb_len = skb->len;
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_ctl_block *ctlb = NULL;
	unsigned int is_ack = 0;
	//int mac_len = 0;
	int mac_len = skb_network_offset(skb);  // has Ethernet header
	struct md_tag_packet *tag = NULL;
	struct md_drt_tag tag_info;
	unsigned int count = 0;
	struct iphdr *iph = NULL;
	struct timespec64 curr_time, diff;

#if defined(CCMNI_MET_DEBUG)
	char tag_name[32] = { '\0' };
	unsigned int tag_id = 0;
#endif

	ccmni = (struct ccmni_instance *)netdev_priv(dev);
	if (!ccmni) {
		CCMNI_INF_MSG(-1, "%s : invalid ccmni\n", __func__);
		return NETDEV_TX_BUSY;
	}

	if (ccmni->md_id < 0 || ccmni->md_id >= MAX_MD_NUM) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id = %d\n", __func__, ccmni->md_id);
		return NETDEV_TX_BUSY;
	}
	ctlb = ccmni_ctl_blk[ccmni->md_id];
	if (!ctlb) {
		CCMNI_INF_MSG(-1, "%s : ccmni ctlb is null\n", __func__);
		return NETDEV_TX_BUSY;
	}

	if (ccmni_forward_rx(ccmni, skb) == NETDEV_TX_OK)
		return NETDEV_TX_OK;

	//CCMNI_INF_MSG(ccmni->md_id, "EthPDU: tx: skb->protocol=0x%x\n", htons(skb->protocol));

	// For CPE project, add wake lock to prevent sleeping during TX
	__pm_wakeup_event(ctlb->ccmni_wakelock, jiffies_to_msecs(HZ));
	if (br_mode) {
		if (mac_len > 0) {
			// For IPv4
			if (skb->protocol == htons(ETH_P_ARP)) {
				if (ccmni_handle_arp_request(ccmni->md_id, dev, skb))
					goto arp_err;
				else
					return NETDEV_TX_OK;
			// For IPv6
			} else if (skb->protocol == htons(ETH_P_IPV6)) {
				struct ipv6hdr* ip6h = ipv6_hdr(skb);

				if (ccmni_is_neigh_solicit(ip6h)) {
					if (ipv6_addr_is_solict_mult(&ip6h->daddr) &&
							(ipv6_addr_type(&ip6h->saddr) & IPV6_ADDR_UNICAST)) {
						if (ccmni_handle_neigh_solicit(dev, skb))
							goto nd_err;
						else
							return NETDEV_TX_OK;
					} else {
						// IPv6 DAD packet, no need to send
						goto nd_err;
					}
				}
			}
		}
	}

	if (ccmni->index == eth_pdu_info.eth_id && eth_pdu_info.eth_mode) {  //EthPDU
		skb_reset_mac_len(skb);
	} else {
		if (mac_len > 0) {
			/* Need to remove L2 header for modem HW interface */
			skb_pull(skb, mac_len);
			skb_pop_mac_header(skb);
		}
	}

	//CCMNI_INF_MSG(ccmni->md_id, "EthPDU: tx: mac_header=%d network_header=%d transport_header=%d mac_len=%d->%d\n",
	//		skb->mac_header, skb->network_header, skb->transport_header, mac_len, skb->mac_len);
	//if (skb->mac_len > 0) {
	//	CCMNI_INF_MSG(ccmni->md_id, "EthPDU: tx: src_mac:%pM dest_mac:%pM proto:%x\n",
	//			eth_hdr(skb)->h_source, eth_hdr(skb)->h_dest, htons(eth_hdr(skb)->h_proto));
	//}

	if (skb->protocol == htons(ETH_P_ARP)) {
		ccmni_eth_pdu_arp_log(ccmni->md_id, dev, skb);
	}

	/* dev->mtu is changed  if dev->mtu is changed by upper layer */
	if (unlikely(skb->len > dev->mtu)) {
		CCMNI_PR_DBG(ccmni->md_id,
					"CCMNI%d write fail: len(0x%x)>MTU(0x%x, 0x%x)\n",
					ccmni->index, skb->len,
					CCMNI_MTU, dev->mtu);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (unlikely(skb_headroom(skb) < sizeof(struct ccci_header))) {
		CCMNI_PR_DBG(ccmni->md_id,
			"CCMNI%d write fail: header room(%d) < ccci_header(%d)\n",
			ccmni->index, skb_headroom(skb),
			dev->hard_header_len);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	tag = (struct md_tag_packet *)skb->head;
	if (tag->guard_pattern == MDT_TAG_PATTERN) {
		if (ccmni->md_id == MD_SYS1) {
			count = sizeof(tag->info);
			/* The tag lives in skb headroom, which may move while making
			 * tailroom.  Preserve it first, then use the skb API so both
			 * len and tail stay consistent.  Forwarded MTU-sized skbs often
			 * have no spare tailroom; the old raw memcpy wrote past skb->end
			 * and could corrupt adjacent slab objects.
			 */
			tag_info = tag->info;
			if (unlikely(skb_tailroom(skb) < count &&
				     pskb_expand_head(skb, 0,
					count - skb_tailroom(skb), GFP_ATOMIC))) {
				CCMNI_PR_DBG(ccmni->md_id,
					"CCMNI%d write fail: no tailroom for MDT tag\n",
					ccmni->index);
				dev_kfree_skb(skb);
				dev->stats.tx_dropped++;
				return NETDEV_TX_OK;
			}
			skb_put_data(skb, &tag_info, count);
		} else {
			CCMNI_DBG_MSG(ccmni->md_id,
				"%s: MD%d not support MDT tag\n",
				dev->name, (ccmni->md_id + 1));
		}
	}

	if (ctlb->ccci_ops->md_ability & MODEM_CAP_DATA_ACK_DVD) {
		iph = (struct iphdr *)skb_network_header(skb);
		if (skb->mark & APP_VIP_MARK)
			is_ack = 1;
		else if (ccmni->ack_prio_en)
			is_ack = is_ack_skb(ccmni->md_id, skb);
	}
	sk_pacing_shift_update(skb->sk, sysctl_tcp_pacing_shift);
	ret = ctlb->ccci_ops->send_pkt(ccmni->md_id, ccmni->index, skb, is_ack);
	if (ret == CCMNI_ERR_MD_NO_READY || ret == CCMNI_ERR_TX_INVAL) {
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		ccmni->tx_busy_cnt[is_ack] = 0;
		CCMNI_DBG_MSG(ccmni->md_id,
			"[TX]CCMNI%d send tx_pkt=%ld(ack=%d) fail: %d\n",
			ccmni->index, (dev->stats.tx_packets + 1),
			is_ack, ret);
		return NETDEV_TX_OK;
	} else if (ret == CCMNI_ERR_TX_BUSY) {
		goto tx_busy;
	}

	if (ccmni_tx_profile) {
		sen_data_len += skb_len;
		if (unlikely(starttime_ccmni_tx.tv_sec == 0)) {
			ktime_get_real_ts64(&starttime_ccmni_tx);
		} else {
			ktime_get_real_ts64(&curr_time);
			diff = timespec64_sub(curr_time, starttime_ccmni_tx);
			if ((diff.tv_sec == 1) ||
					((diff.tv_sec == 0) && (diff.tv_nsec > 980000000))) {
				CCMNI_INF_MSG(ccmni->md_id,
"[TX] ccmni%d sen_data_len=%llu duration=%llu(s)/%ld(ns) speed=%llu mbps/%llu kbps/%llu bps\n",
						ccmni->index,
						sen_data_len,
						(long long)diff.tv_sec,
						diff.tv_nsec,
						(sen_data_len >> 20) * 8,
						(sen_data_len >> 10) * 8,
						sen_data_len * 8);
				sen_data_len = 0;
				ktime_get_real_ts64(&starttime_ccmni_tx);
			}
			if (diff.tv_sec > 1) {
				CCMNI_INF_MSG(ccmni->md_id,
"[TX] ccmni%d sen_data_len=%llu duration=%llu(s) sen_bits=%llu mb/%llu kb/%llu b\n",
						ccmni->index,
						sen_data_len,
						(long long)diff.tv_sec,
						(sen_data_len >> 20) * 8,
						(sen_data_len >> 10) * 8,
						sen_data_len * 8);
				sen_data_len = 0;
				ktime_get_real_ts64(&starttime_ccmni_tx);
			}
		}
	}

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb_len;
	if (ccmni->tx_busy_cnt[is_ack] > 10) {
		CCMNI_DBG_MSG(ccmni->md_id,
			"[TX]CCMNI%d TX busy: tx_pkt=%ld(ack=%d) retry %ld times done\n",
			ccmni->index, dev->stats.tx_packets,
			is_ack, ccmni->tx_busy_cnt[is_ack]);
	}
	ccmni->tx_busy_cnt[is_ack] = 0;

#if defined(CCMNI_MET_DEBUG)
	if (ccmni->tx_met_time == 0) {
		ccmni->tx_met_time = jiffies;
		ccmni->tx_met_bytes = dev->stats.tx_bytes;
	} else if (time_after_eq(jiffies,
		ccmni->tx_met_time + msecs_to_jiffies(MET_LOG_TIMER))) {
		scnprintf(tag_name, 32, "%s_tx_bytes", dev->name);
		tag_id = CCMNI_TX_MET_ID + ccmni->index;
		met_tag_oneshot(tag_id, tag_name,
		(dev->stats.tx_bytes - ccmni->tx_met_bytes));
		ccmni->tx_met_bytes = dev->stats.tx_bytes;
		ccmni->tx_met_time = jiffies;
	}
#endif

	return NETDEV_TX_OK;

tx_busy:
	if (unlikely(!(ctlb->ccci_ops->md_ability & MODEM_CAP_TXBUSY_STOP))) {
		if ((ccmni->tx_busy_cnt[is_ack]++) % 100 == 0)
			CCMNI_DBG_MSG(ccmni->md_id,
				"[TX]CCMNI%d TX busy: tx_pkt=%ld(ack=%d) retry_times=%ld\n",
				ccmni->index, dev->stats.tx_packets,
				is_ack, ccmni->tx_busy_cnt[is_ack]);
	} else {
		ccmni->tx_busy_cnt[is_ack]++;
	}

	/* Add back removed L2 header */
	if (mac_len) {
		skb_push(skb, mac_len);
		skb_reset_mac_header(skb);
	}
	if (skb->len > skb_len)
		skb_trim(skb, skb_len);

	return NETDEV_TX_BUSY;

arp_err:
nd_err:
	dev_kfree_skb(skb);
	dev->stats.tx_dropped++;
	return NETDEV_TX_OK;
}

static int ccmni_change_mtu(struct net_device *dev, int new_mtu)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	if (new_mtu > MAX_MTU)
		return -EINVAL;
// Begin add ccmni MTU limit by chenhongzhai MBB0665-853
	if (new_mtu < 1280 && new_mtu > 68)
		pr_info("The mtu value is %d, not suitable for the minimum ipv6 length\n",new_mtu);
	pr_info("CCMNI%d change mtu_siz=%d\n", ccmni->index,
			new_mtu);
// End add ccmni MTU limit by chenhongzhai MBB0665-853
	dev->mtu = new_mtu;
	CCMNI_DBG_MSG(ccmni->md_id,
		"CCMNI%d change mtu_siz=%d\n", ccmni->index, new_mtu);
	return 0;
}

static void ccmni_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	CCMNI_DBG_MSG(ccmni->md_id,
		"ccmni%d_tx_timeout: usage_cnt=%d, timeout=%ds\n",
		ccmni->index,
		atomic_read(&ccmni->usage), (ccmni->dev->watchdog_timeo/HZ));

	dev->stats.tx_errors++;
	if (atomic_read(&ccmni->usage) > 0)
		netif_tx_wake_all_queues(dev);
}

static int ccmni_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int md_id, md_id_irat, usage_cnt;
	struct ccmni_instance *ccmni_irat;
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_instance *ccmni_tmp = NULL;
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_ctl_block *ctlb_irat = NULL;
	unsigned int timeout = 0;
	struct ccmni_fwd_filter flt_tmp;
	struct ccmni_flt_act flt_act;
	unsigned int i;
	unsigned int cmp_len;

	if (ccmni->md_id < 0 || ccmni->md_id >= MAX_MD_NUM || ccmni->index < 0) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id or index:md_id = %d,index = %d\n",
			__func__, ccmni->md_id, ccmni->index);
		return -EINVAL;
	}
	switch (cmd) {
	case SIOCSTXQSTATE:
		/* ifru_ivalue[3~0]:start/stop; ifru_ivalue[7~4]:reserve; */
		/* ifru_ivalue[15~8]:user id, bit8=rild, bit9=thermal */
		/* ifru_ivalue[31~16]: watchdog timeout value */
		ctlb = ccmni_ctl_blk[ccmni->md_id];
		if ((ifr->ifr_ifru.ifru_ivalue & 0xF) == 0) {
			/*ignore txq stop/resume if dev is not running */
			if (atomic_read(&ccmni->usage) > 0 &&
				netif_running(dev)) {
				atomic_dec(&ccmni->usage);

				netif_tx_disable(dev);
				/* stop queue won't stop Tx
				 * watchdog (ndo_tx_timeout)
				 */
				timeout = (ifr->ifr_ifru.ifru_ivalue &
					0xFFFF0000) >> 16;
				if (timeout == 0)
					dev->watchdog_timeo = 60 * HZ;
				else
					dev->watchdog_timeo = timeout*HZ;

				ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
				/* iRAT ccmni */
				if (ccmni_tmp != ccmni) {
					usage_cnt = atomic_read(&ccmni->usage);
					atomic_set(&ccmni_tmp->usage,
								usage_cnt);
				}
			}
		} else {
			if (atomic_read(&ccmni->usage) <= 0 &&
					netif_running(dev)) {
				netif_tx_wake_all_queues(dev);
				dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
				atomic_inc(&ccmni->usage);

				ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
				/* iRAT ccmni */
				if (ccmni_tmp != ccmni) {
					usage_cnt = atomic_read(&ccmni->usage);
					atomic_set(&ccmni_tmp->usage,
								usage_cnt);
				}
			}
		}
		if (likely(ccmni_tmp != NULL)) {
			CCMNI_DBG_MSG(ccmni->md_id,
				"SIOCSTXQSTATE: %s_state=0x%x, cnt=(%d, %d)\n",
				dev->name, ifr->ifr_ifru.ifru_ivalue,
				atomic_read(&ccmni->usage),
				atomic_read(&ccmni_tmp->usage));
		} else {
			CCMNI_DBG_MSG(ccmni->md_id,
				"SIOCSTXQSTATE: %s_state=0x%x, cnt=%d\n",
				dev->name, ifr->ifr_ifru.ifru_ivalue,
				atomic_read(&ccmni->usage));
		}
		break;

	case SIOCCCMNICFG:
		md_id_irat = ifr->ifr_ifru.ifru_ivalue;
		md_id = ccmni->md_id;
		if (md_id_irat < 0 || md_id_irat >= MAX_MD_NUM) {
			CCMNI_DBG_MSG(md_id,
				"SIOCSCCMNICFG: %s invalid md_id(%d)\n",
				dev->name, (ifr->ifr_ifru.ifru_ivalue + 1));
			return -EINVAL;
		}

		if (dev != ccmni->dev) {
			CCMNI_DBG_MSG(md_id,
				"SIOCCCMNICFG: %s iRAT on MD%d, diff dev(%s->%s)\n",
				dev->name, (ifr->ifr_ifru.ifru_ivalue + 1),
				ccmni->dev->name, dev->name);
			ccmni->dev = dev;
			atomic_set(&ccmni->usage, 0);
			ccmni->tx_busy_cnt[0] = 0;
			ccmni->tx_busy_cnt[1] = 0;
			break;
		}

		ctlb_irat = ccmni_ctl_blk[md_id_irat];
		if (ccmni->index >= ctlb_irat->ccci_ops->ccmni_num) {
			CCMNI_PR_DBG(md_id,
			"SIOCSCCMNICFG: %s iRAT(MD%d->MD%d) fail,index(%d)>max_num(%d)\n",
			dev->name, md_id, md_id_irat, ccmni->index,
			ctlb_irat->ccci_ops->ccmni_num);
			break;
		}
		ccmni_irat = ctlb_irat->ccmni_inst[ccmni->index];

		if (md_id_irat == ccmni->md_id) {
			if (ccmni_irat->dev != dev) {
				CCMNI_DBG_MSG(md_id,
					"SIOCCCMNICFG: %s iRAT on MD%d, diff dev(%s->%s)\n",
					dev->name,
					(ifr->ifr_ifru.ifru_ivalue + 1),
					ccmni_irat->dev->name, dev->name);
				ccmni_irat->dev = dev;
				usage_cnt = atomic_read(&ccmni->usage);
				atomic_set(&ccmni_irat->usage, usage_cnt);
			} else
				CCMNI_DBG_MSG(md_id,
					"SIOCCCMNICFG: %s iRAT on the same MD%d, cnt=%d\n",
					dev->name,
					(ifr->ifr_ifru.ifru_ivalue + 1),
					atomic_read(&ccmni->usage));
			break;
		}

		/* backup ccmni info of md_id into ctlb[md_id]->ccmni_inst */
		ctlb = ccmni_ctl_blk[md_id];
		ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_tmp->usage, usage_cnt);
		ccmni_tmp->tx_busy_cnt[0] = ccmni->tx_busy_cnt[0];
		ccmni_tmp->tx_busy_cnt[1] = ccmni->tx_busy_cnt[1];

		/* fix dev!=ccmni_irat->dev issue
		 * when MD3-CC3MNI -> MD3-CCMNI
		 */
		ccmni_irat->dev = dev;
		atomic_set(&ccmni_irat->usage, usage_cnt);
		memcpy(netdev_priv(dev), ccmni_irat,
			sizeof(struct ccmni_instance));

		CCMNI_DBG_MSG(md_id,
			"SIOCCCMNICFG: %s iRAT MD%d->MD%d, dev_cnt=%d, md_cnt=%d, md_irat_cnt=%d, irat_dev=%s\n",
			dev->name, (md_id + 1),
			(ifr->ifr_ifru.ifru_ivalue + 1),
			atomic_read(&ccmni->usage),
			atomic_read(&ccmni_tmp->usage),
			atomic_read(&ccmni_irat->usage),
			ccmni_irat->dev->name);
		break;

	case SIOCFWDFILTER:
		if (copy_from_user(&flt_act, ifr->ifr_ifru.ifru_data,
				sizeof(struct ccmni_flt_act))) {
			CCMNI_INF_MSG(ccmni->md_id,
				"SIOCFWDFILTER: %s copy data from user fail\n",
				dev->name);
			return -EFAULT;
		}

		flt_tmp = flt_act.flt;
		if (flt_tmp.ver != 0x40 && flt_tmp.ver != 0x60) {
			CCMNI_INF_MSG(ccmni->md_id,
				"SIOCFWDFILTER[%d]: %s invalid flt(%x, %x, %x, %x, %x)(%d)\n",
				flt_act.action, dev->name,
				flt_tmp.ver, flt_tmp.s_pref,
				flt_tmp.d_pref, flt_tmp.ipv4.saddr,
				flt_tmp.ipv4.daddr, ccmni->flt_cnt);
			return -EINVAL;
		}

		if (flt_act.action == CCMNI_FLT_ADD) { /* add new filter */
			if (ccmni->flt_cnt >= CCMNI_FLT_NUM) {
				CCMNI_INF_MSG(ccmni->md_id,
					"SIOCFWDFILTER[ADD]: %s flt table full\n",
					dev->name);
				return -ENOMEM;
			}
			for (i = 0; i < CCMNI_FLT_NUM; i++) {
				if (ccmni->flt_tbl[i].ver == 0)
					break;
			}
			if (i < CCMNI_FLT_NUM) {
				memcpy(&ccmni->flt_tbl[i], &flt_tmp,
					sizeof(struct ccmni_fwd_filter));
				ccmni->flt_cnt++;
			}
			CCMNI_INF_MSG(ccmni->md_id,
				"SIOCFWDFILTER[ADD]: %s add flt%d(%x, %x, %x, %x, %x)(%d)\n",
				dev->name, i, flt_tmp.ver,
				flt_tmp.s_pref, flt_tmp.d_pref,
				flt_tmp.ipv4.saddr, flt_tmp.ipv4.daddr,
				ccmni->flt_cnt);
		} else if (flt_act.action == CCMNI_FLT_DEL) {
			if (flt_tmp.ver == IPV4_VERSION)
				cmp_len = offsetof(struct ccmni_fwd_filter,
							ipv4.daddr) + 4;
			else
				cmp_len = sizeof(struct ccmni_fwd_filter);
			for (i = 0; i < CCMNI_FLT_NUM; i++) {
				if (ccmni->flt_tbl[i].ver == 0)
					continue;
				if (!memcmp(&ccmni->flt_tbl[i],
						&flt_tmp, cmp_len)) {
					CCMNI_INF_MSG(ccmni->md_id,
						"SIOCFWDFILTER[DEL]: %s del flt%d(%x, %x, %x, %x, %x)(%d)\n",
						dev->name, i, flt_tmp.ver,
						flt_tmp.s_pref, flt_tmp.d_pref,
						flt_tmp.ipv4.saddr,
						flt_tmp.ipv4.daddr,
						ccmni->flt_cnt);
					memset(
						&ccmni->flt_tbl[i],
						0,
						sizeof(struct ccmni_fwd_filter)
					);
					ccmni->flt_cnt--;
					break;
				}
			}
			if (i >= CCMNI_FLT_NUM) {
				CCMNI_INF_MSG(ccmni->md_id,
					"SIOCFWDFILTER[DEL]: %s no match flt(%x, %x, %x, %x, %x)(%d)\n",
					dev->name, flt_tmp.ver,
					flt_tmp.s_pref, flt_tmp.d_pref,
					flt_tmp.ipv4.saddr,
					flt_tmp.ipv4.daddr,
					ccmni->flt_cnt);
				return -ENXIO;
			}
		} else if (flt_act.action == CCMNI_FLT_FLUSH) {
			ccmni->flt_cnt = 0;
			memset(ccmni->flt_tbl, 0,
			CCMNI_FLT_NUM*sizeof(struct ccmni_fwd_filter));
			CCMNI_INF_MSG(ccmni->md_id,
				"SIOCFWDFILTER[FLUSH]: %s flush filter\n",
				dev->name);
		}
		break;

	case SIOCACKPRIO:
		/* ifru_ivalue[3~0]: enable/disable ack prio feature  */
		ctlb = ccmni_ctl_blk[ccmni->md_id];
		if ((ifr->ifr_ifru.ifru_ivalue & 0xF) == 0) {
			for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
				ccmni_tmp = ctlb->ccmni_inst[i];
				ccmni_tmp->ack_prio_en = 0;
			}
		} else {
			for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
				ccmni_tmp = ctlb->ccmni_inst[i];
				if (ccmni_tmp->ch.multiq)
					ccmni_tmp->ack_prio_en = 1;
			}
		}
		CCMNI_INF_MSG(ccmni->md_id,
			"SIOCACKPRIO: ack_prio_en=%d, ccmni0_ack_en=%d\n",
			ifr->ifr_ifru.ifru_ivalue,
			ctlb->ccmni_inst[i]->ack_prio_en);
		break;

	default:
		CCMNI_DBG_MSG(ccmni->md_id,
			"%s: unknown ioctl cmd=%x\n", dev->name, cmd);
		break;
	}

	return 0;
}

static int get_link_ksettings(struct net_device *dev,
			      struct ethtool_link_ksettings *cmd)
{
	cmd->base.speed = 5000;
	return 0;
}

#if defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD) && defined(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT)
static int ccmni_fill_forward_path(struct net_device_path_ctx *ctx,
				       struct net_device_path *path)
{
	int ccmni_idx = 1;
	path->dev = ctx->dev;
	if (is_ccmni_netdev(path->dev->name)) {
		ccmni_idx = get_ccmni_orig_index(path->dev);
	} else {
		CCMNI_INF_MSG(0, "%s: %s is not a ccmni device, set ccmni_idx to 1!\n",
			 __func__,  path->dev->name == NULL ? "NULL" : path->dev->name);
	}

	path->type = DEV_PATH_MTK_MDMA;
	path->mtk_mdma.ntype = 0;
	path->mtk_mdma.chid = ccmni_idx;
	ctx->dev = NULL;

	if (ccmni_tx_profile) {
		CCMNI_INF_MSG(0, "%s: dev->name=%s, type=%d, mtk_mdma.ntype=%d, mtk_mdma.chid=%d\n",
			 __func__,  path->dev->name, path->type, path->mtk_mdma.ntype, path->mtk_mdma.chid);
	}

	return 0;
}

static int ccmni_fill_receive_path(struct net_device_path_ctx *ctx,
				       struct net_device_path *path)
{
	int ccmni_idx = 1;
	path->dev = ctx->dev;
	if (is_ccmni_netdev(path->dev->name)) {
		ccmni_idx = get_ccmni_orig_index(path->dev);
	} else {
		CCMNI_INF_MSG(0, "%s: %s is not a ccmni device, set ccmni_idx to 1!\n",
			 __func__,  path->dev->name == NULL ? "NULL" : path->dev->name);
	}

	path->type = DEV_PATH_MTK_MDMA;
	path->mtk_mdma.ppe_idx = 1;

	if (ccmni_rx_profile) {
		CCMNI_INF_MSG(0, "%s: dev->name=%s, type=%d, mtk_mdma.ppe_idx=%d\n",
			 __func__,  path->dev->name, path->type, path->mtk_mdma.ppe_idx);
	}

	return 0;
}
#endif

static const struct net_device_ops ccmni_netdev_ops = {
	.ndo_open		= ccmni_open,
	.ndo_stop		= ccmni_close,
	.ndo_start_xmit	= ccmni_start_xmit,
	.ndo_tx_timeout	= ccmni_tx_timeout,
	.ndo_do_ioctl   = ccmni_ioctl,
	.ndo_change_mtu = ccmni_change_mtu,
	.ndo_select_queue = ccmni_select_queue,
	.ndo_get_stats64 = ccmni_get_stats64,
	.ndo_set_mac_address = eth_mac_addr,
#if defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD) && defined(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT)
	.ndo_fill_forward_path	= ccmni_fill_forward_path,
	.ndo_fill_receive_path	= ccmni_fill_receive_path,
	.ndo_setup_tc		= mtk_netdev_setup_tc,
#endif
};

static const struct ethtool_ops ethtool_ops = {
	.get_link_ksettings = get_link_ksettings,
};

static int ccmni_napi_poll(struct napi_struct *napi, int budget)
{
#ifdef ENABLE_WQ_GRO
	return 0;
#else
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(napi->dev);
	int md_id = ccmni->md_id;
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[md_id];

	del_timer(ccmni->timer);

	if (ctlb->ccci_ops->napi_poll)
		return ctlb->ccci_ops->napi_poll(md_id, ccmni->index,
					napi, budget);
	else
		return 0;
#endif
}

//static void ccmni_napi_poll_timeout(unsigned long data)
static void ccmni_napi_poll_timeout(struct timer_list *t)
{
	//struct ccmni_instance *ccmni = (struct ccmni_instance *)data;
	//struct ccmni_instance *ccmni = from_timer(ccmni, t, timer);

	//CCMNI_DBG_MSG(ccmni->md_id,
	//	"CCMNI%d lost NAPI polling\n", ccmni->index);
}

static void get_queued_pkts(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ccmni_instance *ccmni =
		container_of(dwork, struct ccmni_instance, pkt_queue_work);
	struct ccmni_ctl_block *ctlb = NULL;
#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
	char orig_name[IFNAMSIZ] = { '\0' };
#endif

	if (ccmni->md_id < 0 || ccmni->md_id >= MAX_MD_NUM) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id = %d\n", __func__, ccmni->md_id);
		return;
	}
	ctlb = ccmni_ctl_blk[ccmni->md_id];
	if (ctlb == NULL) {
		CCMNI_INF_MSG(ccmni->md_id, "%s : invalid ctlb\n", __func__);
		return;
	}

#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
	get_ccmni_orig_devname(ccmni->dev, orig_name);
	if (ctlb->ccci_ops->ccci_handle_port_list(DEV_OPEN,
			strlen(orig_name) ? orig_name : ccmni->dev->name))
#else
	if (ctlb->ccci_ops->ccci_handle_port_list(DEV_OPEN, ccmni->dev->name))
#endif
		CCMNI_INF_MSG(ccmni->md_id,
				"%s is failed to handle port list\n",
				ccmni->dev->name);
}

static int ccmni_dfs_show(struct seq_file *m, void *v)
{
	struct ccmni_ctl_block *ctlb;
	struct ccmni_instance *ccmni;
	struct net_device *dev;
	int i, j;

	seq_printf(m, "bridge mode enabled=%d\n", br_mode);
#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
	seq_printf(m, "ccmni wan devname=%s\n", ccmni_wan_devname);
#endif
	for (i = 0; i < MAX_MD_NUM; i++) {
		ctlb = ccmni_ctl_blk[i];
		if (!ctlb)
			continue;

		seq_printf(m, "------ MD%d ------\n", i+1);
		for (j = 0; j < ctlb->ccci_ops->ccmni_num; j++) {
			ccmni = ctlb->ccmni_inst[j];
			dev = ccmni->dev;
			seq_printf(m, "%s::\n", dev->name);
			seq_printf(m, "Dev MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
					*(dev->dev_addr+0), *(dev->dev_addr+1), *(dev->dev_addr+2),
					*(dev->dev_addr+3), *(dev->dev_addr+4), *(dev->dev_addr+5));
			seq_printf(m, "Dev state=0x%lx\n", dev->state);
			seq_printf(m, "Dev flags=0x%x\n", dev->flags);
			seq_printf(m, "Dev type=%d\n", dev->type);
			seq_printf(m, "Dev reg state=%d\n", dev->reg_state);
			seq_printf(m, "Dev ifindex=%d\n", dev->ifindex);
			seq_printf(m, "Dev header_ops=%p\n", dev->header_ops);
			seq_printf(m, "tx_busy_cnt=(%lu,%lu)\n", ccmni->tx_busy_cnt[0],
					ccmni->tx_busy_cnt[1]);
#if defined(CCMNI_MET_DEBUG)
			seq_printf(m, "(rx,tx)_met_time=(%lu,%lu) "
					"(rx,tx)_met_bytes=(%lu,%lu)\n", ccmni->rx_met_time,
					ccmni->tx_met_time, ccmni->rx_met_bytes,
					ccmni->tx_met_bytes);
#endif
			seq_printf(m, "usage=%d\n", atomic_read(&ccmni->usage));
			if (br_mode)
				seq_printf(m, "IDU MAC addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
						*(ccmni->idu_mac_addr+0), *(ccmni->idu_mac_addr+1),
						*(ccmni->idu_mac_addr+2), *(ccmni->idu_mac_addr+3),
						*(ccmni->idu_mac_addr+4), *(ccmni->idu_mac_addr+5));
			seq_printf(m, "\n");
		}
	}
	return 0;
}

static int ccmni_dfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, ccmni_dfs_show, NULL);
}

static const struct file_operations ccmni_dfs_fops = {
	.open		= ccmni_dfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t ccmni_proc_bridge_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *data)
{
	struct ccmni_ctl_block *ctlb;
	struct ccmni_instance *ccmni;
	struct net_device *dev;
	char buf[4];
	int i, j;
	bool val;

	count = min(count, sizeof(buf) - 1);
	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;
	buf[count] = '\0';

	if (kstrtobool(buf, &val))
		return -EINVAL;

	if (val == br_mode)
		return count;

	br_mode = val;
	CCMNI_INF_MSG(0, "%s: ccmni bridge mode:%d\n", __func__, br_mode);

	for (i = 0; i < MAX_MD_NUM; i++) {
		ctlb = ccmni_ctl_blk[i];
		if (!ctlb)
			continue;
		for (j = 0; j < ctlb->ccci_ops->ccmni_num; j++) {
			ccmni = ctlb->ccmni_inst[j];
			dev = ccmni->dev;
			if (br_mode) {
				dev->hard_header_len = ETH_HLEN;
				dev->header_ops = &eth_header_ops;
				dev->flags = dev->flags | IFF_NOARP | IFF_BROADCAST | IFF_MULTICAST;
				dev->type = ARPHRD_ETHER;
				dev->needed_headroom = sizeof(struct ccci_header);
			} else if (ccmni->index == eth_pdu_info.eth_id && eth_pdu_info.eth_mode) {  //EthPDU
				dev->hard_header_len = ETH_HLEN;
				dev->header_ops = &eth_header_ops;
				dev->flags = (dev->flags & ~IFF_NOARP) | IFF_BROADCAST | IFF_MULTICAST;
				dev->type = ARPHRD_ETHER;
				dev->needed_headroom = sizeof(struct ccci_header);
			} else {
				dev->hard_header_len = 0;
				dev->header_ops = NULL;
				dev->flags = (dev->flags | IFF_NOARP) & (~IFF_BROADCAST & ~IFF_MULTICAST);
				dev->type = ARPHRD_NONE;
				dev->needed_headroom = 0;
			}
		}
	}
	return count;
}

static int ccmni_proc_bridge_read(struct seq_file *m, void *v)
{
	CCMNI_INF_MSG(0, "%s: ccmni bridge mode:%d\n", __func__, br_mode);
	seq_printf(m, "%d\n", br_mode);
	return 0;
}

static int ccmni_proc_bridge_open(struct inode *inode, struct file *file)
{
	return single_open(file, ccmni_proc_bridge_read, NULL);
}

static const struct proc_ops ccmni_bridge_fops = {
//	.owner      = THIS_MODULE,
	.proc_open		= ccmni_proc_bridge_open,
	.proc_write      = ccmni_proc_bridge_write,
	.proc_read		= seq_read,
	.proc_release	= single_release,
};

static ssize_t ccmni_proc_eth_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	struct ccmni_ctl_block *ctlb;
	struct ccmni_instance *ccmni;
	struct net_device *dev;
	int i, j;
	char buf[32];
	char *p_buf;
	size_t len = count;
	char *p_token = NULL;
	char *p_delimiter = " \t";
	int ret;

	if (len >= sizeof(buf)) {
		pr_notice("input handling fail!\n");
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		return len;
	else
		ret = kstrtou8(p_token, 10, &eth_pdu_info.eth_mode);

	if (eth_pdu_info.eth_mode == 1) {
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			return len;
		else
			ret = kstrtou8(p_token, 10, &eth_pdu_info.eth_id);
		if (eth_pdu_info.eth_id > 20)
			return len;
	}

	for (i = 0; i < MAX_MD_NUM; i++) {
		ctlb = ccmni_ctl_blk[i];
		if (!ctlb)
			continue;
		for (j = 0; j < ctlb->ccci_ops->ccmni_num; j++) {
			ccmni = ctlb->ccmni_inst[j];
			dev = ccmni->dev;
			if (br_mode) {
				dev->hard_header_len = ETH_HLEN;
				dev->header_ops = &eth_header_ops;
				dev->flags = dev->flags | IFF_NOARP | IFF_BROADCAST | IFF_MULTICAST;
				dev->type = ARPHRD_ETHER;
				dev->needed_headroom = sizeof(struct ccci_header);
			} else if (ccmni->index == eth_pdu_info.eth_id && eth_pdu_info.eth_mode) {  //EthPDU
				dev->hard_header_len = ETH_HLEN;
				dev->header_ops = &eth_header_ops;
				dev->flags = (dev->flags & ~IFF_NOARP) | IFF_BROADCAST | IFF_MULTICAST;
				dev->type = ARPHRD_ETHER;
				dev->needed_headroom = sizeof(struct ccci_header);
			} else {
				dev->hard_header_len = 0;
				dev->header_ops = NULL;
				dev->flags = (dev->flags | IFF_NOARP) & (~IFF_BROADCAST & ~IFF_MULTICAST);
				dev->type = ARPHRD_NONE;
				dev->needed_headroom = 0;
			}
			CCMNI_INF_MSG(ccmni->md_id, "EthPDU: ccmni%d eth_mode=%d eth_id=%d br_mode=%d\n",
					ccmni->index, eth_pdu_info.eth_mode, eth_pdu_info.eth_id, br_mode);
		}
	}

	return len;
}

static int ccmni_proc_eth_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d %d\n", eth_pdu_info.eth_mode, eth_pdu_info.eth_id);
	return 0;
}

static int ccmni_proc_eth_open(struct inode *inode, struct file *file)
{
	return single_open(file, ccmni_proc_eth_read, NULL);
}

static const struct proc_ops ccmni_eth_fops = {
//	.owner      = THIS_MODULE,
	.proc_open		= ccmni_proc_eth_open,
	.proc_write      = ccmni_proc_eth_write,
	.proc_read		= seq_read,
	.proc_release	= single_release,
};

#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
static ssize_t ccmni_proc_wan_devname_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *data)
{
	char *buf = NULL;

	buf = kmalloc(sizeof(char) * (count + 1), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, count)) {
		kfree(buf);
		return -EFAULT;
	}
	buf[count] = '\0';
	if( (count > 0) && (buf[count-1] == '\n') ) {
		buf[count-1] = '\0';
	}
	scnprintf(ccmni_wan_devname, IFNAMSIZ, "%s", buf);
	CCMNI_INF_MSG(0, "%s: ccmni wan devname:%s\n", __func__, ccmni_wan_devname);

	kfree(buf);
	return count;
}

static int ccmni_proc_wan_devname_read(struct seq_file *m, void *v)
{
	CCMNI_INF_MSG(0, "%s: ccmni wan devname:%s\n", __func__, ccmni_wan_devname);
	seq_printf(m, "%s\n", ccmni_wan_devname);
	return 0;
}

static int ccmni_proc_wan_devname_open(struct inode *inode, struct file *file)
{
	return single_open(file, ccmni_proc_wan_devname_read, NULL);
}

static const struct proc_ops ccmni_wan_devname_fops = {
//	.owner      = THIS_MODULE,
	.proc_open		= ccmni_proc_wan_devname_open,
	.proc_write      = ccmni_proc_wan_devname_write,
	.proc_read		= seq_read,
	.proc_release	= single_release,
};
#endif

static int ccmni_proc_idu_mac_read(struct seq_file *m, void *v)
{
	struct ccmni_instance *ccmni = m->private;
	seq_printf(m, "%s IDU MAC addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
			ccmni->dev->name, *(ccmni->idu_mac_addr+0),
			*(ccmni->idu_mac_addr+1), *(ccmni->idu_mac_addr+2),
			*(ccmni->idu_mac_addr+3), *(ccmni->idu_mac_addr+4),
			*(ccmni->idu_mac_addr+5));
	return 0;
}

static ssize_t ccmni_proc_idu_mac_write(struct file *file,
	const char __user *buf, size_t count, loff_t *data)
{
	struct ccmni_instance *ccmni = pde_data(file_inode(file));
	int ret;
	char param[MAX_ADDR_LEN] = {0};
	unsigned char new_mac[ETH_ALEN] = {0};
	char *ptr = param;
	char *found = param;
	int i, j;

	ret = strncpy_from_user(param, buf, MAX_ADDR_LEN);
	if (ret < 0)
		return ret;

	for (i = 0, j = 0; i < MAX_ADDR_LEN; i++, ptr++) {
		if (j == ETH_ALEN)
			break;
		if (*ptr == ':' || *ptr == '\0') {
			*ptr = '\0';
			if (found == ptr) {
				// Handle AA::BB multiple ':' case
				found = ptr + 1;
				continue;
			}
			ret = kstrtou8(found, 16, &new_mac[j++]);
			if (ret) {
				pr_err("%s kstrtou8 ret=%d conversion failure at[%s] i=%d j=%d\n",
					__func__, ret, found, i, j);
				return count;
			}
			found = ptr + 1;
		}
	}

	ether_addr_copy(ccmni->idu_mac_addr, new_mac);

	return count;
}

static int ccmni_proc_idu_mac_open(struct inode *inode, struct file *file)
{
	return single_open(file, ccmni_proc_idu_mac_read, pde_data(inode));
}

static const struct proc_ops ccmni_idu_fops = {
//	.owner      = THIS_MODULE,
	.proc_open		= ccmni_proc_idu_mac_open,
	.proc_write      = ccmni_proc_idu_mac_write,
	.proc_read		= seq_read,
	.proc_release	= single_release,
};

/********************ccmni driver register  ccci function********************/
static inline int ccmni_inst_init(int md_id, struct ccmni_instance *ccmni,
	struct net_device *dev)
{
	struct ccmni_ctl_block *ctlb = NULL;
	int ret = 0;
	char proc_name[24] = {0};

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id = %d\n", __func__, md_id);
		return -EINVAL;
	}
	ctlb = ccmni_ctl_blk[md_id];
	if (!ctlb) {
		CCMNI_PR_DBG(md_id, "%s ctlb is null!\n",
			__func__);
		return -1;
	}

	ret = ctlb->ccci_ops->get_ccmni_ch(md_id, ccmni->index, &ccmni->ch);
	if (ret) {
		CCMNI_PR_DBG(md_id,
			"get ccmni%d channel fail\n",
			ccmni->index);
		return ret;
	}

	ccmni->dev = dev;
	ccmni->ctlb = ctlb;
	ccmni->md_id = md_id;
	ccmni->napi = kzalloc(sizeof(struct napi_struct), GFP_KERNEL);
	if (ccmni->napi == NULL) {
		CCMNI_PR_DBG(md_id, "%s kzalloc ccmni->napi fail\n",
			__func__);
		return -1;
	}
	ccmni->timer = kzalloc(sizeof(struct timer_list), GFP_KERNEL);
	if (ccmni->timer == NULL) {
		CCMNI_PR_DBG(md_id, "%s kzalloc ccmni->timer fail\n",
			__func__);
		return -1;
	}
	ccmni->spinlock = kzalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (ccmni->spinlock == NULL) {
		CCMNI_PR_DBG(md_id, "%s kzalloc ccmni->spinlock fail\n",
			__func__);
		return -1;
	}
	ccmni->ack_prio_en = ccmni->ch.multiq ? 1 : 0;

	scnprintf(proc_name, 24, "idu_mac_ccmni%d", ccmni->index);
	if (!ccmni->proc_idu_mac)
		ccmni->proc_idu_mac = proc_create_data(proc_name, 0640,
				ccmni_proc_dir, &ccmni_idu_fops, ccmni);

	/* register napi device */
	if (dev && (ctlb->ccci_ops->md_ability & MODEM_CAP_NAPI)) {
		//init_timer(ccmni->timer);
		//ccmni->timer->function = ccmni_napi_poll_timeout;
		//ccmni->timer->data = (unsigned long)ccmni;
		timer_setup(ccmni->timer, ccmni_napi_poll_timeout, 0);
		netif_napi_add_weight(dev, ccmni->napi, ccmni_napi_poll,
			ctlb->ccci_ops->napi_poll_weigh);
	}
#ifdef ENABLE_WQ_GRO
	if (dev)
		netif_napi_add_weight(dev, ccmni->napi, ccmni_napi_poll,
			ctlb->ccci_ops->napi_poll_weigh);
#endif

	atomic_set(&ccmni->usage, 0);
	spin_lock_init(ccmni->spinlock);

	ccmni->worker = alloc_workqueue("ccmni%d_rx_q_worker",
		WQ_UNBOUND | WQ_MEM_RECLAIM, 1, ccmni->index);
	if (!ccmni->worker) {
		CCMNI_PR_DBG(md_id, "%s alloc queue worker fail\n",
			__func__);
		return -1;
	}
	INIT_DELAYED_WORK(&ccmni->pkt_queue_work, get_queued_pkts);

	return ret;
}

static void *of_get_mac_addr(struct device_node *np, const char *name)
{
	struct property *pp = of_find_property(np, "mac-address", NULL);

	if (pp && pp->length == ETH_ALEN && is_valid_ether_addr(pp->value))
		return pp->value;
	return NULL;
}

static inline void ccmni_dev_init(int md_id, struct net_device *dev)
{
	struct ccmni_ctl_block *ctlb = NULL;
	struct device_node *node = NULL;
	const char *mac_addr;

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id = %d\n", __func__, md_id);
		return;
	}

	ctlb = ccmni_ctl_blk[md_id];
	if (!ctlb) {
		CCMNI_PR_DBG(md_id, "%s ctlb is null!\n",
			__func__);
		return;
	}

	dev->max_mtu = MAX_MTU;
	dev->mtu = CCMNI_MTU;
	dev->tx_queue_len = CCMNI_TX_QUEUE;
	dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;

	/* not support VLAN */
	// dev->features = NETIF_F_VLAN_CHALLENGED;  // EthPDU
	dev->features |= NETIF_F_GRO_FRAGLIST;
	if (ctlb->ccci_ops->md_ability & MODEM_CAP_SGIO) {
		dev->features |= NETIF_F_SG;
		dev->hw_features |= NETIF_F_SG;
	}
	if (ctlb->ccci_ops->md_ability & MODEM_CAP_NAPI) {
#ifdef ENABLE_NAPI_GRO
		dev->features |= NETIF_F_GRO;
		dev->hw_features |= NETIF_F_GRO;
#endif
	} else {
#ifdef ENABLE_WQ_GRO
		dev->features |= NETIF_F_GRO;
		dev->hw_features |= NETIF_F_GRO;
#endif
	}

#if defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD)
	dev->features |= NETIF_F_HW_TC;
	dev->hw_features |= NETIF_F_HW_TC;
#endif

	// need Ethernet HW addr to be shown in ifconfig
	// otherwise, set addr_len = 0
	dev->addr_len = ETH_ALEN; /* ethernet header size */
	dev->priv_destructor = free_netdev;
	dev->netdev_ops = &ccmni_netdev_ops;
	dev->ethtool_ops = &ethtool_ops;

	//random_ether_addr((u8 *) dev->dev_addr);
	node = of_find_compatible_node(NULL, NULL,
			"mediatek,mddriver");

	if (node) {
		mac_addr = of_get_mac_addr(node, "mac-address");
		if (!IS_ERR_OR_NULL(mac_addr)) {
			eth_hw_addr_set(dev, mac_addr);
			//CCMNI_INF_MSG(md_id, "load MAC address from dts %pM\n",
			//		dev->dev_addr);
		}
	}

	/* If the mac address is invalid, use random mac address  */
	if (!dev->dev_addr || !is_valid_ether_addr(dev->dev_addr)) {
		eth_hw_addr_random(dev);
		//CCMNI_INF_MSG(md_id, "generated random MAC address %pM for %s\n",
		//		dev->dev_addr, dev->name);
	}
}

static int ccmni_init(int md_id, struct ccmni_ccci_ops *ccci_info)
{
	int i = 0, j = 0, ret = 0;
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_ctl_block *ctlb_irat_src = NULL;
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_irat_src = NULL;
	struct net_device *dev = NULL;

	eth_pdu_info.eth_mode = 0;
	eth_pdu_info.eth_id = 0;

	/*
	 * tcp_pacing_shift is an optional tuning interface, not a prerequisite
	 * for creating the modem data netdevs.  The vendor 5.15 code returned
	 * success here on registration failure, silently skipping all CCMNI
	 * devices.  Keep the SDK default and continue if another component owns
	 * the same /proc/sys/net entry on newer kernels.
	 */
	register_tcp_pacing_sysctl();

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id = %d\n", __func__, md_id);
		return -EINVAL;
	}

#if defined(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT)
	CCMNI_INF_MSG(0, "%s: init med_base\n", __func__);
	med_base = ioremap(MTK_MED_BASE, MTK_FE_RANGE);
#endif

	if (unlikely(ccci_info->md_ability & MODEM_CAP_CCMNI_DISABLE)) {
		CCMNI_PR_DBG(md_id, "no need init ccmni: md_ability=0x%08X\n",
			ccci_info->md_ability);
		return 0;
	}

	// create proc files
	if (!ccmni_proc_dir)
		ccmni_proc_dir = proc_mkdir("ccmni", NULL);
	if (!ccmni_proc_dir)
		return -ENOMEM;
	if (!ccmni_proc_bridge_file)
		ccmni_proc_bridge_file = proc_create("bridge_mode_control", 0640,
				ccmni_proc_dir, &ccmni_bridge_fops);
	if (!ccmni_proc_bridge_file)
		return -ENOMEM;

	if (!ccmni_proc_eth_file)
		ccmni_proc_eth_file = proc_create("eth_pdu_support", 0640,
			 ccmni_proc_dir, &ccmni_eth_fops);
	if (!ccmni_proc_eth_file)
		return -ENOMEM;

#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
	if (!ccmni_proc_wan_devname)
		ccmni_proc_wan_devname = proc_create("ccmni_wan_devname", 0640,
				ccmni_proc_dir, &ccmni_wan_devname_fops);
	if (!ccmni_proc_wan_devname)
		return -ENOMEM;
#endif

	ctlb = kzalloc(sizeof(struct ccmni_ctl_block), GFP_KERNEL);
	if (unlikely(ctlb == NULL)) {
		CCMNI_PR_DBG(md_id, "alloc ccmni ctl struct fail\n");
		return -ENOMEM;
	}

	ctlb->ccci_ops = kzalloc(sizeof(struct ccmni_ccci_ops), GFP_KERNEL);
	if (unlikely(ctlb->ccci_ops == NULL)) {
		CCMNI_PR_DBG(md_id, "alloc ccmni_ccci_ops struct fail\n");
		ret = -ENOMEM;
		goto alloc_mem_fail;
	}

#if defined(CCMNI_MET_DEBUG)
	if (met_tag_init() != 0)
		CCMNI_INF_MSG(md_id, "%s:met tag init fail\n", __func__);
#endif

	ccmni_ctl_blk[md_id] = ctlb;

	memcpy(ctlb->ccci_ops, ccci_info, sizeof(struct ccmni_ccci_ops));

	for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
		/* allocate netdev */
		if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ)
			/* alloc multiple tx queue, 2 txq and 1 rxq */
			dev =
			alloc_etherdev_mqs(
					sizeof(struct ccmni_instance),
					2,
					1);
		else
			dev =
			alloc_etherdev(sizeof(struct ccmni_instance));
		if (unlikely(dev == NULL)) {
			CCMNI_PR_DBG(md_id, "alloc netdev fail\n");
			ret = -ENOMEM;
			goto alloc_netdev_fail;
		}

		/* init net device */
		ccmni_dev_init(md_id, dev);

		scnprintf(dev->name, sizeof(dev->name), "%s%d", ctlb->ccci_ops->name, i);

		/* init private structure of netdev */
		ccmni = netdev_priv(dev);
		ccmni->index = i;

		// when skb hasn't ethernet header, GRO needs hard_header_len == 0
		/*
		* check gro_list_prepare,
		* GRO needs hard_header_len == ETH_HLEN.
		* CCCI header can use ethernet header and
		* padding bytes' region.
		*/
		if (br_mode) {
			dev->hard_header_len = ETH_HLEN;
			dev->header_ops = &eth_header_ops;
			dev->flags = dev->flags | IFF_NOARP | IFF_BROADCAST | IFF_MULTICAST;
			dev->type = ARPHRD_ETHER;
			dev->needed_headroom = sizeof(struct ccci_header);
		} else if (ccmni->index == eth_pdu_info.eth_id && eth_pdu_info.eth_mode) {  //EthPDU
			dev->hard_header_len = ETH_HLEN;
			dev->header_ops = &eth_header_ops;
			dev->flags = (dev->flags & ~IFF_NOARP) | IFF_BROADCAST | IFF_MULTICAST;
			dev->type = ARPHRD_ETHER;
			dev->needed_headroom = sizeof(struct ccci_header);
		} else {  // used to support auto add ipv6 mroute
			dev->hard_header_len = 0;
			dev->header_ops = NULL;
			dev->flags = (dev->flags | IFF_NOARP) & (~IFF_BROADCAST & ~IFF_MULTICAST);
			dev->type = ARPHRD_NONE;
			dev->needed_headroom = 0;
		}
		ret = ccmni_inst_init(md_id, ccmni, dev);
		if (ret) {
			CCMNI_PR_DBG(md_id,
				"initial ccmni instance fail\n");
			goto alloc_netdev_fail;
		}
		ctlb->ccmni_inst[i] = ccmni;

		/* register net device */
		ret = register_netdev(dev);
		if (ret)
			goto alloc_netdev_fail;
		ctlb->ccci_ops->ccci_net_init(dev->name);
	}


	if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_IRAT) {
		if (ctlb->ccci_ops->irat_md_id >= MAX_MD_NUM) {
			CCMNI_PR_DBG(md_id,
				"md%d IRAT fail: invalid irat md(%d)\n",
				md_id, ctlb->ccci_ops->irat_md_id);
			ret = -EINVAL;
			goto alloc_mem_fail;
		}

		ctlb_irat_src = ccmni_ctl_blk[ctlb->ccci_ops->irat_md_id];
		if (!ctlb_irat_src) {
			CCMNI_PR_DBG(md_id,
					"md%d IRAT fail: irat md%d ctlb is NULL\n",
					md_id, ctlb->ccci_ops->irat_md_id);
			ret = -EINVAL;
			goto alloc_mem_fail;
		}

		if (unlikely(ctlb->ccci_ops->ccmni_num >
				ctlb_irat_src->ccci_ops->ccmni_num)) {
			CCMNI_PR_DBG(md_id,
			"IRAT fail: ccmni number not match(%d, %d)\n",
			ctlb_irat_src->ccci_ops->ccmni_num,
			ctlb->ccci_ops->ccmni_num);
			ret = -EINVAL;
			goto alloc_mem_fail;
		}

		for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
			ccmni = ctlb->ccmni_inst[i];
			ccmni_irat_src = kzalloc(sizeof(struct ccmni_instance),
								GFP_KERNEL);
			if (unlikely(ccmni_irat_src == NULL)) {
				CCMNI_PR_DBG(md_id,
					"alloc ccmni_irat instance fail\n");
				kfree(ccmni);
				ret = -ENOMEM;
				goto alloc_mem_fail;
			}

			/* initial irat ccmni instance */
			dev = ctlb_irat_src->ccmni_inst[i]->dev;
			/* initial irat source ccmni instance */
			memcpy(ccmni_irat_src, ctlb_irat_src->ccmni_inst[i],
			sizeof(struct ccmni_instance));
			ctlb_irat_src->ccmni_inst[i] = ccmni_irat_src;
		}
	}

	scnprintf(ctlb->wakelock_name, sizeof(ctlb->wakelock_name),
			"ccmni_md%d", (md_id + 1));
	ctlb->ccmni_wakelock = wakeup_source_register(NULL,
		ctlb->wakelock_name);
	if (!ctlb->ccmni_wakelock) {
		CCMNI_PR_DBG(md_id, "%s %d: init wakeup source fail!",
			__func__, __LINE__);
		return -1;
	}

	// create debugFs files
	if (!dfs_dir)
		dfs_dir = debugfs_create_dir("ccmni", NULL);
	if (!dfs_dir)
		return -ENOMEM;
	if (!debugfs_create_file(ctlb->wakelock_name, S_IRUGO, dfs_dir, NULL,
			&ccmni_dfs_fops))
		return -ENOMEM;
	return 0;

alloc_netdev_fail:
	if (dev) {
		free_netdev(dev);
		ctlb->ccmni_inst[i] = NULL;
	}

	for (j = i - 1; j >= 0; j--) {
		ccmni = ctlb->ccmni_inst[j];
		unregister_netdev(ccmni->dev);
		/* free_netdev(ccmni->dev); */
		ctlb->ccmni_inst[j] = NULL;
	}

alloc_mem_fail:
	kfree(ctlb->ccci_ops);
	kfree(ctlb);

	ccmni_ctl_blk[md_id] = NULL;
	return ret;
}

static void ccmni_exit(int md_id)
{
	int i = 0;
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_instance *ccmni = NULL;

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id = %d\n", __func__, md_id);
		return;
	}
	CCMNI_DBG_MSG(md_id, "%s\n", __func__);

	unregister_tcp_pacing_sysctl();

	ctlb = ccmni_ctl_blk[md_id];
	if (ctlb) {
		if (ctlb->ccci_ops == NULL)
			goto ccmni_exit_ret;

		for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
			ccmni = ctlb->ccmni_inst[i];
			if (ccmni) {
				CCMNI_DBG_MSG(md_id,
					"%s: unregister ccmni%d dev\n",
					__func__, i);
				unregister_netdev(ccmni->dev);
				/* free_netdev(ccmni->dev); */
				ctlb->ccmni_inst[i] = NULL;
				if (ccmni->proc_idu_mac)
					proc_remove(ccmni->proc_idu_mac);
			}
		}

		kfree(ctlb->ccci_ops);

ccmni_exit_ret:
		kfree(ctlb);
		ccmni_ctl_blk[md_id] = NULL;

		if (dfs_dir)
			debugfs_remove_recursive(dfs_dir);
		if (ccmni_proc_bridge_file)
			proc_remove(ccmni_proc_bridge_file);
		if (ccmni_proc_eth_file)
			proc_remove(ccmni_proc_eth_file);
#ifdef ENABLE_RENAME_CCMNI_WAN_DEVNAME
		if (ccmni_proc_wan_devname)
			proc_remove(ccmni_proc_wan_devname);
#endif
	}
}

struct timespec64 starttime_ccmni_rx;
static unsigned long long rec_data_len;

#if defined(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT)
void ccmni_recycle_med_hnat_info(void)
{
	int count = 0;

	if (unlikely(!med_base))
		return;

	reg_write(MEDHW_SSR1_DST_RB0_DEC, BIT(31) | 1);
	while (reg_read(MEDHW_SSR1_DST_RB0_DEC) & BIT(31)) {
		if (++count >= 1600000) {
			pr_err("%s: recycle MED_HNAT_INFO_HOST fail\n", __func__);
			return;
		}
		cpu_relax();
	}
}
EXPORT_SYMBOL(ccmni_recycle_med_hnat_info);
#endif

#if defined(CONFIG_NETFILTER_XT_TARGET_FLOWOFFLOAD) && defined(CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT)
void ccmni_set_ppe_rx_info(struct sk_buff *skb, u8 drop, u8 channel)
{
	struct MED_HNAT_INFO_HOST *med_dmad;
	unsigned int foe_entry_num/*hash*/, cpu_reason, wdix, rdix;
	int count = 0;

	if (med_info_base == NULL) {
		CCMNI_INF_MSG(0, "%s: init med_info_base\n", __func__);
		struct medmcu_desc_info_t *desc_info = get_desc_info();
		med_info_base = desc_info[HNAT_INFO_HOST].base_virt;
	}

	rdix = reg_read(MEDHW_SSR1_DST_RB0_RIDX) & 0x3ffff;
	wdix = reg_read(MEDHW_SSR1_DST_RB0_WIDX) & 0x3ffff;

	if ((rdix == wdix) && (wdix == 0)) {
		CCMNI_INF_MSG(0, "HNAT HOST INFO index error rdix=0x%x, wdix=0x%x\n", rdix, wdix);
	}

	med_dmad = &med_info_base[rdix];

	foe_entry_num = med_dmad->dmad_info1.PPE_ENTRY;
	cpu_reason = med_dmad->dmad_info1.CRSN;

	if (ccmni_rx_profile) {
		CCMNI_INF_MSG(0,
			"%s: cpu_reason=0x%x, foe_entry_num=0x%x, rdix=0x%x, wdix=0x%x, chn=%u, drop=%d\n",
			__func__,  cpu_reason, foe_entry_num, rdix, wdix, channel, drop);
	}

	reg_write(MEDHW_SSR1_DST_RB0_DEC, ((0x1 << 31) | 0x1));

	while (1) {
		if ((reg_read(MEDHW_SSR1_DST_RB0_DEC)&(1<<31)) == 0) {
			break;
		}
		if (++count >= 1600000) {
			pr_err("%s: recycle MED_HNAT_INFO_HOST fail\n", __func__);
			return;
		}
	}

	if (drop == 1) {
		return;
	}

	if (cpu_reason == MTK_PPE_CPU_REASON_HIT_UNBIND_RATE_REACHED)
		mtk_ppe_check_skb(nf_ppe.ppe[1], skb, foe_entry_num);
}
EXPORT_SYMBOL(ccmni_set_ppe_rx_info);
#endif

static int ccmni_rx_callback(int md_id, int ccmni_idx, struct sk_buff *skb,
		void *priv_data)
{
	struct ccmni_ctl_block *ctlb = NULL;
	/* struct ccci_header *ccci_h = (struct ccci_header*)skb->data; */
	struct ccmni_instance *ccmni = NULL;
	struct net_device *dev = NULL;
	unsigned int pkt_type, skb_len;
#if defined(CCCI_SKB_TRACE)
	struct iphdr *iph;
#endif
#if defined(CCMNI_MET_DEBUG)
	char tag_name[32] = { '\0' };
	unsigned int tag_id = 0;
#endif
#if defined(CONFIG_TUNNEL_FAST_PATH)
	int eogre_ppe_result = 1;
#endif
	struct timespec64 curr_time, diff;
	__be16 type, trans_proto;

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCMNI_DBG_MSG(-1, "%s : invalid md_id = %d\n", __func__, md_id);
		return -1;
	}

	ctlb = ccmni_ctl_blk[md_id];

	if (unlikely(ctlb == NULL || ctlb->ccci_ops == NULL)) {
		CCMNI_DBG_MSG(md_id, "%s invalid CCMNI%d ctrl/ops struct\n", __func__, ccmni_idx);
		dev_kfree_skb(skb);
		return -1;
	}

	if (ccmni_idx < 0 || ccmni_idx >= ctlb->ccci_ops->ccmni_num) {
		CCMNI_DBG_MSG(md_id, "%s : invalid index = %d\n", __func__, ccmni_idx);
		return -1;
	}

	ccmni = ctlb->ccmni_inst[ccmni_idx];
	dev = ccmni->dev;

#if defined(CONFIG_TUNNEL_FAST_PATH)
	if (ppe_hook_rx_modem_thread) {
		eogre_ppe_result = ppe_hook_rx_modem_thread(skb);
	}
	if (eogre_ppe_result != 1) {
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += skb->len;
		return 0; /*pkt is delivered to hwnat, skip pkt handle*/
	}
#endif

#ifdef ENABLE_DEFERRED_IP_ALLOCATION
	handle_dhcp_offer(skb, ccmni);
#endif

	if (ccmni->index == eth_pdu_info.eth_id && eth_pdu_info.eth_mode) {  //EthPDU
		skb->mac_len = ETH_HLEN;
		type = eth_hdr(skb)->h_proto;
		while (eth_type_vlan(type)) {
			struct vlan_hdr *vh;

			if ((skb->mac_len + VLAN_HLEN) > skb_headlen(skb))
				return -EFAULT;

			vh = (struct vlan_hdr *)(skb->data + skb->mac_len);
			type = vh->h_vlan_encapsulated_proto;
			skb->mac_len += VLAN_HLEN;
		}

		skb_set_mac_header(skb, 0);
		skb_set_network_header(skb, skb->mac_len);
		skb_set_transport_header(skb, skb->mac_len);
		skb->protocol = type;

		trans_proto = eth_type_trans(skb, dev);
		CCMNI_INF_MSG(ccmni->md_id, "EthPDU: rx: trans_proto=0x%x, proto=0x%x->0x%x \n",
				htons(trans_proto), htons(eth_hdr(skb)->h_proto), htons(type));
	} else {
		pkt_type = skb->data[0] & 0xF0;

		if (br_mode) {
			if (ccmni_is_multicast_addr(skb->data, pkt_type)) {
				ccmni_make_multicast_frame(skb->data, (struct ethhdr *) (skb->data -
						ETH_HLEN), pkt_type);
				skb->pkt_type = PACKET_MULTICAST;
				dev->stats.multicast++;
			} else if (pkt_type == 0x60 && ccmni_is_odu_v6_addr(dev,
					&((struct ipv6hdr *)(skb->data))->daddr)) {
				struct ethhdr *eth_hdr = (struct ethhdr *) (skb->data - ETH_HLEN);

				ether_addr_copy(eth_hdr->h_dest, dev->dev_addr);
				ether_addr_copy(eth_hdr->h_source, fake_ccmni_mac);
				eth_hdr->h_proto = cpu_to_be16(ETH_P_IPV6);
			} else {
				ccmni_make_etherframe(dev, skb, pkt_type);
			}
			skb_set_mac_header(skb, -ETH_HLEN);
		} else {
//			skb_set_mac_header(skb, 0);
			ccmni_make_etherframe(dev, skb, pkt_type);
			skb_set_mac_header(skb, -ETH_HLEN);
		}

		skb_reset_network_header(skb);
		skb_reset_transport_header(skb);
		skb_reset_mac_len(skb);

		if (pkt_type == 0x60)
			skb->protocol  = htons(ETH_P_IPV6);
		else
			skb->protocol  = htons(ETH_P_IP);
	}
	skb->dev = dev;

	//CCMNI_INF_MSG(ccmni->md_id, "EthPDU: rx: mac_header=%d network_header=%d transport_header=%d mac_len=%d\n",
	//		skb->mac_header, skb->network_header, skb->transport_header, skb->mac_len);
	//if (skb->mac_len > 0) {
	//	CCMNI_INF_MSG(ccmni->md_id, "EthPDU: rx: src_mac:%pM dest_mac:%pM proto:%x\n",
	//			eth_hdr(skb)->h_source, eth_hdr(skb)->h_dest, htons(eth_hdr(skb)->h_proto));
	//}

	//CCMNI_INF_MSG(ccmni->md_id, "EthPDU: rx: skb->protocol=0x%x\n", htons(skb->protocol));

	//skb->ip_summed = CHECKSUM_NONE;
	skb_len = skb->len;

	if (ccmni_rx_profile) {
		rec_data_len += skb->len;
		if (unlikely(starttime_ccmni_rx.tv_sec == 0)) {
			ktime_get_real_ts64(&starttime_ccmni_rx);
		} else {
			ktime_get_real_ts64(&curr_time);
			diff = timespec64_sub(curr_time, starttime_ccmni_rx);
			if ((diff.tv_sec == 1) ||
				((diff.tv_sec == 0) && (diff.tv_nsec > 980000000))) {
				CCMNI_INF_MSG(md_id,
"[RX] ccmni%d rec_data_len=%llu duration=%llu(s)/%ld(ns) speed=%llu mbps/%llu kbps/%llu bps\n",
						ccmni_idx,
						rec_data_len,
						(long long)diff.tv_sec,
						diff.tv_nsec,
						(rec_data_len >> 20) * 8,
						(rec_data_len >> 10) * 8,
						rec_data_len * 8);
				rec_data_len = 0;
				ktime_get_real_ts64(&starttime_ccmni_rx);
			}
			if (diff.tv_sec > 1) {
				CCMNI_INF_MSG(md_id,
"[RX] ccmni%d rec_data_len=%llu duration=%llu(s) rec_bits=%llu mb/%llu kb/%llu b\n",
						ccmni_idx,
						rec_data_len,
						(long long)diff.tv_sec,
						(rec_data_len >> 20) * 8,
						(rec_data_len >> 10) * 8,
						rec_data_len * 8);
				rec_data_len = 0;
				ktime_get_real_ts64(&starttime_ccmni_rx);
			}
		}
	}

#if defined(NETDEV_TRACE) && defined(NETDEV_DL_TRACE)
	skb->dbg_flag = 0x1;
#endif

#if defined(CCCI_SKB_TRACE)
	iph = (struct iphdr *)skb->data;
	ctlb->net_rx_delay[2] = iph->id;
	ctlb->net_rx_delay[0] = dev->stats.rx_bytes + skb_len;
	ctlb->net_rx_delay[1] = dev->stats.tx_bytes;
#endif

	if (likely(ctlb->ccci_ops->md_ability & MODEM_CAP_NAPI)) {
#ifdef ENABLE_NAPI_GRO
		napi_gro_receive(ccmni->napi, skb);
#else
		netif_receive_skb(skb);
#endif
	} else {
#ifdef ENABLE_WQ_GRO
		if (is_skb_gro(skb)) {
			preempt_disable();
			spin_lock_bh(ccmni->spinlock);
			napi_gro_receive(ccmni->napi, skb);
			ccmni_gro_flush(ccmni);
			spin_unlock_bh(ccmni->spinlock);
			preempt_enable();
		} else {
			netif_rx(skb);
		}
#else
		if (!in_interrupt())
			netif_rx(skb);
		else
			netif_rx(skb);
#endif
	}
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb_len;

#if defined(CCMNI_MET_DEBUG)
	if (ccmni->rx_met_time == 0) {
		ccmni->rx_met_time = jiffies;
		ccmni->rx_met_bytes = dev->stats.rx_bytes;
	} else if (time_after_eq(jiffies,
		ccmni->rx_met_time + msecs_to_jiffies(MET_LOG_TIMER))) {
		scnprintf(tag_name, 32, "%s_rx_bytes", dev->name);
		tag_id = CCMNI_RX_MET_ID + ccmni_idx;
		met_tag_oneshot(tag_id, tag_name,
			(dev->stats.rx_bytes - ccmni->rx_met_bytes));
		ccmni->rx_met_bytes = dev->stats.rx_bytes;
		ccmni->rx_met_time = jiffies;
	}
#endif

	__pm_wakeup_event(ctlb->ccmni_wakelock, jiffies_to_msecs(HZ));

	return 0;
}

static void ccmni_queue_state_callback(int md_id, int ccmni_idx,
	enum HIF_STATE state, int is_ack)
{
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
	struct net_device *dev = NULL;
	struct netdev_queue *net_queue = NULL;

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCMNI_DBG_MSG(-1, "%s : invalid md_id = %d\n", __func__, md_id);
		return;
	}

	ctlb = ccmni_ctl_blk[md_id];
	if (unlikely(ctlb == NULL)) {
		CCMNI_DBG_MSG(md_id,
			"%s : invalid ccmni ctrl when ccmni%d_hif_sta=%d\n",
			__func__, ccmni_idx, state);
		return;
	}

	if (ccmni_idx < 0 || ccmni_idx >= ctlb->ccci_ops->ccmni_num) {
		CCMNI_DBG_MSG(md_id, "%s : invalid index = %d\n", __func__, ccmni_idx);
		return;
	}

	ccmni_tmp = ctlb->ccmni_inst[ccmni_idx];
	dev = ccmni_tmp->dev;
	ccmni = (struct ccmni_instance *)netdev_priv(dev);

	switch (state) {
#ifdef ENABLE_WQ_GRO
	case RX_FLUSH:
		preempt_disable();
		spin_lock_bh(ccmni->spinlock);
		ccmni->rx_gro_cnt++;
		napi_gro_list_flush(ccmni);
		spin_unlock_bh(ccmni->spinlock);
		preempt_enable();
		break;
#else
	case RX_IRQ:
		mod_timer(ccmni->timer, jiffies + HZ);
		napi_schedule(ccmni->napi);
		__pm_wakeup_event(ctlb->ccmni_wakelock, jiffies_to_msecs(HZ));
		break;
#endif

	case TX_IRQ:
		if (netif_running(ccmni->dev) &&
				atomic_read(&ccmni->usage) > 0) {
			if (likely(ctlb->ccci_ops->md_ability &
					MODEM_CAP_CCMNI_MQ)) {
				if (is_ack)
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_FAST);
				else
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_NORMAL);
				if (netif_tx_queue_stopped(net_queue))
					netif_tx_wake_queue(net_queue);
			} else {
				is_ack = 0;
				if (netif_queue_stopped(ccmni->dev))
					netif_wake_queue(ccmni->dev);
			}
			ccmni->tx_irq_cnt[is_ack]++;
			if ((ccmni->flags[is_ack] & CCMNI_TX_PRINT_F) ||
				time_after(jiffies,
					ccmni->tx_irq_tick[is_ack] + 2)) {
				ccmni->flags[is_ack] &= ~CCMNI_TX_PRINT_F;
				CCMNI_INF_MSG(md_id,
					"%s(%d), idx=%d, md_sta=TX_IRQ, ack=%d, cnt(%u, %u), time=%lu\n",
					ccmni->dev->name,
					atomic_read(&ccmni->usage),
					ccmni->index,
					is_ack, ccmni->tx_full_cnt[is_ack],
					ccmni->tx_irq_cnt[is_ack],
					(jiffies - ccmni->tx_irq_tick[is_ack]));
			}
		}
		break;

	case TX_FULL:
		if (atomic_read(&ccmni->usage) > 0) {
			if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ) {
				if (is_ack)
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_FAST);
				else
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_NORMAL);
				netif_tx_stop_queue(net_queue);
			} else {
				is_ack = 0;
				netif_stop_queue(ccmni->dev);
			}
			ccmni->tx_full_cnt[is_ack]++;
			ccmni->tx_irq_tick[is_ack] = jiffies;
			if (time_after(jiffies,
					ccmni->tx_full_tick[is_ack] + 4)) {
				ccmni->tx_full_tick[is_ack] = jiffies;
				ccmni->flags[is_ack] |= CCMNI_TX_PRINT_F;
				CCMNI_DBG_MSG(md_id,
					"%s(%d), idx=%d, hif_sta=TX_FULL, ack=%d, cnt(%u, %u)\n",
					ccmni->dev->name,
					atomic_read(&ccmni->usage),
					ccmni->index,
					is_ack, ccmni->tx_full_cnt[is_ack],
					ccmni->tx_irq_cnt[is_ack]);
			}
		}
		break;
	default:
		break;
	}
}

static void ccmni_md_state_callback(int md_id, int ccmni_idx,
	enum MD_STATE state)
{
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
	struct net_device *dev = NULL;
	int i = 0;

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCMNI_DBG_MSG(-1, "%s : invalid md_id = %d\n", __func__, md_id);
		return;
	}

	ctlb = ccmni_ctl_blk[md_id];
	if (unlikely(ctlb == NULL)) {
		CCMNI_DBG_MSG(md_id,
			"%s : invalid ccmni ctrl when ccmni%d_md_sta=%d\n",
			__func__, ccmni_idx, state);
		return;
	}

	if (ccmni_idx < 0 || ccmni_idx >= ctlb->ccci_ops->ccmni_num) {
		CCMNI_DBG_MSG(md_id, "%s : invalid index = %d\n", __func__, ccmni_idx);
		return;
	}

	ccmni_tmp = ctlb->ccmni_inst[ccmni_idx];
	dev = ccmni_tmp->dev;
	ccmni = (struct ccmni_instance *)netdev_priv(dev);
	if (atomic_read(&ccmni->usage) > 0)
		CCMNI_DBG_MSG(md_id,
			"md_state_cb: CCMNI%d, md_sta=%d, usage=%d\n",
			ccmni_idx, state, atomic_read(&ccmni->usage));
	switch (state) {
	case READY:
		for (i = 0; i < 2; i++) {
			ccmni->tx_seq_num[i] = 0;
			ccmni->tx_full_cnt[i] = 0;
			ccmni->tx_irq_cnt[i] = 0;
			ccmni->tx_full_tick[i] = 0;
			ccmni->flags[i] &= ~CCMNI_TX_PRINT_F;
		}
		ccmni->rx_seq_num = 0;
		spin_lock_bh(ccmni->spinlock);
		ccmni->rx_gro_cnt = 0;
		spin_unlock_bh(ccmni->spinlock);
		break;

	case EXCEPTION:
	case RESET:
	case WAITING_TO_STOP:
		netif_tx_disable(ccmni->dev);
		netif_carrier_off(ccmni->dev);
		break;
	default:
		break;
	}
}

static void ccmni_dump(int md_id, int ccmni_idx, unsigned int flag)
{
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
	struct net_device *dev = NULL;
	struct netdev_queue *dev_queue = NULL;
	struct netdev_queue *ack_queue = NULL;
	struct Qdisc *qdisc = NULL;
	struct Qdisc *ack_qdisc = NULL;

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCMNI_DBG_MSG(-1, "%s : invalid md_id = %d\n", __func__, md_id);
		return;
	}

	ctlb = ccmni_ctl_blk[md_id];
	if (unlikely(ctlb == NULL)) {
		CCMNI_DBG_MSG(md_id, "%s : invalid ctlb\n", __func__);
		return;
	}

	if (ccmni_idx < 0 || ccmni_idx >= ctlb->ccci_ops->ccmni_num) {
		CCMNI_DBG_MSG(md_id, "%s : invalid index = %d\n", __func__, ccmni_idx);
		return;
	}

	ccmni_tmp = ctlb->ccmni_inst[ccmni_idx];
	if (unlikely(ccmni_tmp == NULL))
		return;

	if ((ccmni_tmp->dev->stats.rx_packets == 0) &&
			(ccmni_tmp->dev->stats.tx_packets == 0))
		return;
	dev = ccmni_tmp->dev;
	/* ccmni diff from ccmni_tmp for MD IRAT */
	ccmni = (struct ccmni_instance *)netdev_priv(dev);
	dev_queue = netdev_get_tx_queue(dev, 0);
	CCMNI_INF_MSG(md_id, "to:clr(%lu:%lu)\r\n",
		timeout_flush_num, clear_flush_num);
	if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ) {
		ack_queue = netdev_get_tx_queue(dev, CCMNI_TXQ_FAST);
		qdisc = dev_queue->qdisc;
		ack_qdisc = ack_queue->qdisc;
		/* stats.rx_dropped is dropped in ccmni,
		 * ccmni_dev_rx_dropped(dev) is dropped in net device layer
		 */
		/* stats.tx_packets is count by ccmni, bstats.
		 * packets is count by qdisc in net device layer
		 */
		CCMNI_INF_MSG(md_id,
			"%s(%d,%d), irat_MD%d, rx=(%ld,%ld,%d), tx=(%ld,%llu,%llu), txq_len=(%d,%d), tx_drop=(%ld,%d,%d), rx_drop=(%ld,%ld), tx_busy=(%ld,%ld), sta=(0x%lx,0x%x,0x%lx,0x%lx)\n",
				dev->name,
				atomic_read(&ccmni->usage),
				atomic_read(&ccmni_tmp->usage),
				(ccmni->md_id + 1),
				(long)(dev->stats.rx_packets),
				(long)(dev->stats.rx_bytes),
				ccmni->rx_gro_cnt,
				(long)(dev->stats.tx_packets), (unsigned long long)u64_stats_read(&(qdisc->bstats.packets)),
				(unsigned long long)u64_stats_read(&(ack_qdisc->bstats.packets)),
				qdisc->q.qlen, ack_qdisc->q.qlen,
				(long)(dev->stats.tx_dropped), qdisc->qstats.drops,
				ack_qdisc->qstats.drops,
				(long)(dev->stats.rx_dropped),
				(long)(ccmni_dev_rx_dropped(dev)),
				(long)(ccmni->tx_busy_cnt[0]), (long)(ccmni->tx_busy_cnt[1]),
				dev->state, dev->flags, dev_queue->state,
				ack_queue->state);
	} else
		CCMNI_INF_MSG(md_id,
			"%s(%d,%d), irat_MD%d, rx=(%ld,%ld,%d), tx=(%ld,%ld), txq_len=%d, tx_drop=(%ld,%d), rx_drop=(%ld,%ld), tx_busy=(%ld,%ld), sta=(0x%lx,0x%x,0x%lx)\n",
				dev->name, atomic_read(&ccmni->usage),
				atomic_read(&ccmni_tmp->usage),
						(ccmni->md_id + 1),
				dev->stats.rx_packets, dev->stats.rx_bytes,
				ccmni->rx_gro_cnt,
				dev->stats.tx_packets, dev->stats.tx_bytes,
				dev->qdisc->q.qlen, dev->stats.tx_dropped,
				dev->qdisc->qstats.drops,
				dev->stats.rx_dropped,
				ccmni_dev_rx_dropped(dev),
				ccmni->tx_busy_cnt[0],
				ccmni->tx_busy_cnt[1], dev->state, dev->flags,
				dev_queue->state);
}

static void ccmni_dump_rx_status(int md_id, unsigned long long *status)
{
	struct ccmni_ctl_block *ctlb = NULL;

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCMNI_INF_MSG(-1, "%s : invalid md_id = %d\n", __func__, md_id);
		return;
	}
	ctlb = ccmni_ctl_blk[md_id];
	if (ctlb == NULL) {
		CCMNI_INF_MSG(md_id, "%s : invalid ctlb\n", __func__);
		return;
	}
	status[0] = ctlb->net_rx_delay[0];
	status[1] = ctlb->net_rx_delay[1];
	status[2] = ctlb->net_rx_delay[2];
}

static struct ccmni_ch *ccmni_get_ch(int md_id, int ccmni_idx)
{
	struct ccmni_ctl_block *ctlb = NULL;

	if (md_id < 0 || md_id >= MAX_MD_NUM) {
		CCMNI_DBG_MSG(-1, "%s : invalid md_id = %d\n", __func__, md_id);
		return NULL;
	}

	ctlb = ccmni_ctl_blk[md_id];
	if (unlikely(ctlb == NULL)) {
		CCMNI_DBG_MSG(md_id, "%s invalid ctlb\n", __func__);
		return NULL;
	}

	if (ccmni_idx < 0 || ccmni_idx >= ctlb->ccci_ops->ccmni_num) {
		CCMNI_DBG_MSG(md_id, "%s : invalid index = %d\n", __func__, ccmni_idx);
		return NULL;
	}

	return &ctlb->ccmni_inst[ccmni_idx]->ch;
}

struct ccmni_dev_ops ccmni_ops = {
	.skb_alloc_size = 1600,
	.init = &ccmni_init,
	.rx_callback = &ccmni_rx_callback,
	.md_state_callback = &ccmni_md_state_callback,
	.queue_state_callback = &ccmni_queue_state_callback,
	.exit = ccmni_exit,
	.dump = ccmni_dump,
	.dump_rx_status = ccmni_dump_rx_status,
	.get_ch = ccmni_get_ch,
	.is_ack_skb = is_ack_skb,
};
EXPORT_SYMBOL(ccmni_ops);

MODULE_AUTHOR("MTK CCCI");
MODULE_DESCRIPTION("CCCI ccmni driver v0.1");
MODULE_LICENSE("GPL");
