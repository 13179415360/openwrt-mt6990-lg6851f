// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_PORT_TT_H__
#define __CCCI_PORT_TT_H__



#include "ccci_core.h"
#include "ccci_list.h"

/* packet will be dropped if port's Rx buffer full */
#define PORT_F_ALLOW_DROP	(1<<0)
/* rx buffer has been full once */
#define PORT_F_RX_FULLED	(1<<1)
/* CCCI header will be provided by user, but not by CCCI */
#define PORT_F_USER_HEADER	(1<<2)
/* Rx queue only has this one port */
#define PORT_F_RX_EXCLUSIVE	(1<<3)
/* Check whether need remove ccci header while recv skb*/
#define PORT_F_ADJUST_HEADER	(1<<4)
/* Enable port channel traffic*/
#define PORT_F_CH_TRAFFIC	(1<<5)
/* Dump raw data if CH_TRAFFIC set*/
#define PORT_F_DUMP_RAW_DATA	(1<<6)
/* Need export char dev node for userspace*/
#define PORT_F_WITH_CHAR_NODE	(1<<7)

/* reused for net tx, Data queue, same bit as RX_FULLED */
#define PORT_F_TX_DATA_FULLED	(1<<1)
#define PORT_F_TX_ACK_FULLED	(1<<8)


#define MAX_QUEUE_LENGTH 32

enum {
	PORT_DBG_DUMP_RILD = 0,
	PORT_DBG_DUMP_AUDIO,
	PORT_DBG_DUMP_IMS,
};


struct port_t;




typedef int (* ext_ccci_dev_open_t)(
		struct port_t *port,
		struct inode *inode,
		struct file *file);

typedef int (* ext_ccci_dev_close_t)(
		struct port_t *port);

typedef ssize_t (* ext_ccci_dev_read_t)(struct port_t *port,
		char *buf,
		size_t count,
		loff_t *ppos);

typedef ssize_t (* ext_ccci_dev_write_t)(struct file *file,
		struct port_t *port,
		const char __user *buf,
		size_t count,
		loff_t *ppos);

typedef unsigned int (* ext_ccci_dev_poll_t)(struct port_t *port,
		struct file *fp,
		struct poll_table_struct *poll);

struct port_ops {
	/* must-have */
	int (*init)(struct port_t *port);

	int (*recv_data)(struct port_t *port, void *data);

	//int (*recv_skb)(struct port_t *port, struct sk_buff *skb);

	/* optional */
	//int (*recv_match)(struct port_t *port, struct sk_buff *skb);

};


typedef void (*port_skb_handler)(struct port_t *port, struct sk_buff *skb);



struct port_t {
	/* don't change the sequence unless
	 * you modified modem drivers as well
	 */
	/* identity */
	CCCI_CH_T tx_ch;
	CCCI_CH_T rx_ch;
	/*
	 *
	 * here is a nasty trick, we assume no modem provide
	 * more than 0xF0 queues, so we use
	 * the lower 4 bit to smuggle info for network ports.
	 * Attention, in this trick we assume hardware queue index
	 * for net port will not exceed 0xF.
	 * check NET_ACK_TXQ_INDEX@port_net.c
	 */
	unsigned char txq_index;
	unsigned char rxq_index;
//	unsigned char txq_exp_index;
//	unsigned char rxq_exp_index;
	unsigned char hif_id;
	unsigned short flags;
	struct port_ops *ops;
	/* device node related */
	unsigned int minor;
	char *name;

	ext_ccci_dev_open_t  port_dev_open;
	ext_ccci_dev_close_t port_dev_close;
	ext_ccci_dev_read_t  port_dev_read;
	ext_ccci_dev_write_t port_dev_write;
	ext_ccci_dev_poll_t  port_dev_poll;

	/* un-initiallized in defination, always put them at the end */
	//int md_id;
	//void *port_proxy;
	//void *private_data;
	atomic_t usage_cnt;
	//struct list_head entry;
	//struct list_head exp_entry;
	//struct list_head queue_entry;

	//unsigned int major; /*dynamic alloc*/
	//unsigned int minor_base;
	/*
	 * the Tx and Rx flow are asymmetric due to ports are
	 * mutilplexed on queues.
	 * Tx: data block are sent directly to queue's list,
	 * so port won't maitain a Tx list. It only
	 * provide a wait_queue_head for blocking write.
	 * Rx: due to modem needs to dispatch Rx packet
	 * as quickly as possible, so port needs a
	 * Rx list to hold packets.
	 */
	//struct sk_buff_head rx_skb_list;
	ext_ccci_data_list_t rx_data_list;
	/* add high prio rx list for udc */
	//struct sk_buff_head rx_skb_list_hp;
	//unsigned char skb_from_pool;
	spinlock_t rx_req_lock;
	wait_queue_head_t rx_wq;	/* for uplayer user */
	int rx_length;
	int rx_length_th;
	struct wakeup_source *rx_wakelock;
	unsigned int tx_busy_count;
	unsigned int rx_busy_count;
	int interception;
	unsigned int rx_pkg_cnt;
	unsigned int rx_drop_cnt;
	unsigned int tx_pkg_cnt;

	HOST_USERT_STATE_T host_user_state;

	//struct cdev   *scdev;
	//struct device *sdevice;
	//struct class  *sclass;
	//port_skb_handler skb_handler;

	int en_flag; /* for dipc port cfg */
};

/*
 * This cldma private port table is created for driver use only, so port user can't use it.
 */
struct pri_port_t {
	char *name;
	u32 ch_id:15;
	u32 en_flag:1;  //0: disable; 1: enable;
	u32 reserve:16;
};

#endif /* __CCCI_PORT_TT_H__ */
