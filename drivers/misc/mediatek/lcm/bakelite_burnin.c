// SPDX-License-Identifier: GPL-2.0
/* Conservative brightness profile for the Samsung OLED panel family. */

#include <linux/kernel.h>
#include <linux/bakelite_burnin.h>
#include <linux/module.h>

static bool enabled = true;
static unsigned int max_level = 220;
module_param(enabled, bool, 0644);
MODULE_PARM_DESC(enabled, "Enable OLED brightness cap");
module_param(max_level, uint, 0644);
MODULE_PARM_DESC(max_level, "Maximum normal brightness (1..255)");

unsigned int bakelite_burnin_limit(unsigned int level)
{
	unsigned int cap;

	if (!READ_ONCE(enabled))
		return level;
	cap = clamp(READ_ONCE(max_level), 1U, 255U);
	return min(level, cap);
}
