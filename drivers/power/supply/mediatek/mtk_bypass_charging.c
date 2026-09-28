// SPDX-License-Identifier: GPL-2.0
/*
 * mtk_bypass_charging.c - MediaTek bypass charging support
 * Powers system directly from charger without charging battery
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

static bool bypass_charging_enabled;
module_param_named(bypass_charging, bypass_charging_enabled, bool, 0644);
MODULE_PARM_DESC(bypass_charging, "Bypass battery charging, power directly from charger");

static struct kobject *bypass_kobj;

static ssize_t bypass_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bypass_charging_enabled);
}

static ssize_t bypass_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;
	bypass_charging_enabled = !!val;
	pr_info("bypass_charging set to %d\n", bypass_charging_enabled);
	return count;
}

static struct kobj_attribute bypass_attr = __ATTR(bypass_charging, 0644, bypass_show, bypass_store);

static int __init mtk_bypass_charging_init(void)
{
	int ret;

	bypass_kobj = kobject_create_and_add("bypass_charging", kernel_kobj);
	if (!bypass_kobj)
		return -ENOMEM;
	ret = sysfs_create_file(bypass_kobj, &bypass_attr.attr);
	if (ret) {
		kobject_put(bypass_kobj);
		return ret;
	}
	pr_info("MTK bypass charging init, enabled=%d\n", bypass_charging_enabled);
	return 0;
}

static void __exit mtk_bypass_charging_exit(void)
{
	sysfs_remove_file(bypass_kobj, &bypass_attr.attr);
	kobject_put(bypass_kobj);
}

module_init(mtk_bypass_charging_init);
module_exit(mtk_bypass_charging_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("The Bakelite Project");
MODULE_DESCRIPTION("MediaTek bypass charging support");
