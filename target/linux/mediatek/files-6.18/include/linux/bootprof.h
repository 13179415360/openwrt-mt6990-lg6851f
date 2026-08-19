/* SPDX-License-Identifier: GPL-2.0 */
/* LG6851F Linux 6.18 minimal bootprof compatibility surface. */
#ifndef _LINUX_LG6851F_BOOTPROF_H
#define _LINUX_LG6851F_BOOTPROF_H

#ifdef CONFIG_MTPROF
void bootprof_log_boot(char *str);
#else
static inline void bootprof_log_boot(const char *str)
{
	(void)str;
}
#endif

#endif /* _LINUX_LG6851F_BOOTPROF_H */
