// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_PORT_BASE_H__
#define __CCCI_PORT_BASE_H__

//#include "<linux/types.h>"
//#include "<linux/compiler.h>"

#include "ccci_port_t.h"



typedef struct {
	int port_count;
	//unsigned int major;
	//unsigned int minor_base;

} port_base_info_t;


extern
int ext_ccci_all_port_init(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data);

extern
struct port_t *ext_ccci_get_port_from_minor(int minor);

extern
void ext_ccci_port_base_init(void);

extern
void ext_ccci_port_init(struct port_t *port);

extern
int ext_ccci_dev_create(struct port_t *port,
		const struct file_operations *file_ops);


extern
port_base_info_t *ext_ccci_get_port_base_info(void);

extern
int ext_ccci_comm_dev_open(struct port_t *port,
		struct inode *inode,
		struct file *file);

extern
int ext_ccci_dev_open(struct inode *inode,
		struct file *file);

extern
int ext_ccci_comm_dev_close(struct port_t *port);

extern
int ext_ccci_dev_close(struct inode *inode,
		struct file *file);

extern
ssize_t ext_ccci_comm_dev_read(struct file *file,
		struct port_t *port,
		char *buf,
		size_t count,
		loff_t *ppos);

extern
ssize_t ext_ccci_dev_read(struct file *file,
		char *buf,
		size_t count,
		loff_t *ppos);

extern
ssize_t ext_ccci_dev_write(struct file *file,
		const char __user *buf,
		size_t count,
		loff_t *ppos);

extern
int ext_ccci_port_recv_check(struct port_t *port,
		void *data);

extern
void ext_ccci_port_recv_wakeup(struct port_t *port);

extern
int ext_ccci_normal_data_recv(struct port_t *port,
		void *data);

extern
unsigned int ext_ccci_dev_poll(struct file *fp,
		struct poll_table_struct *poll);

extern
int ext_ccci_data_recv(int msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data);

extern
int ext_ccci_dipc_port_cfg(int msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data);

extern
long ext_ccci_dev_ioctl(struct file *file,
		unsigned int cmd,
		unsigned long arg);

#ifdef CONFIG_COMPAT
extern
long ext_ccci_dev_compat_ioctl(
		struct file *filp,
		unsigned int cmd,
		unsigned long arg);
#endif

extern
void ccci_append_pri_port_cfg(char *data, int *data_len);

#endif /* __CCCI_PORT_BASE_H__ */
