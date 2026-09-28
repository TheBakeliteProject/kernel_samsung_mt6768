/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_BAKELITE_BURNIN_H
#define _LINUX_BAKELITE_BURNIN_H

#include <linux/types.h>

#ifdef CONFIG_BAKELITE_BURNIN_PROTECTION
unsigned int bakelite_burnin_limit(unsigned int level);
#else
static inline unsigned int bakelite_burnin_limit(unsigned int level)
{
	return level;
}
#endif

#endif
