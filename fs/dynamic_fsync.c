// SPDX-License-Identifier: GPL-2.0
/* Display-aware fsync deferral. Inspired by the dynamic fsync approach. */

#include <linux/dynamic_fsync.h>
#include <linux/atomic.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kobject.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/syscalls.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>

static bool dynamic_fsync_enabled = true;
static bool screen_on = true;
static struct kobject *dynamic_fsync_kobj;
static atomic_t deferred_fsyncs = ATOMIC_INIT(0);

#define DYNAMIC_FSYNC_MAX_DELAY_MS 5000

static void dynamic_fsync_timeout(struct work_struct *work);
static DECLARE_DELAYED_WORK(dynamic_fsync_timeout_work,
			    dynamic_fsync_timeout);

bool dynamic_fsync_should_defer(struct file *file)
{
	bool defer = READ_ONCE(dynamic_fsync_enabled) && READ_ONCE(screen_on) &&
		(file->f_mode & FMODE_WRITE) &&
		file->f_op && file->f_op->fsync &&
		S_ISREG(file_inode(file)->i_mode);

	/* Never swallow an existing writeback error by deferring past it. */
	if (defer && (filemap_check_errors(file->f_mapping) ||
		      filemap_check_wb_err(file->f_mapping, 0)))
		defer = false;

	if (defer) {
		atomic_set(&deferred_fsyncs, 1);
		schedule_delayed_work(&dynamic_fsync_timeout_work,
			msecs_to_jiffies(DYNAMIC_FSYNC_MAX_DELAY_MS));
	}
	return defer;
}

static void dynamic_fsync_timeout(struct work_struct *work)
{
	if (atomic_xchg(&deferred_fsyncs, 0))
		sys_sync();
}

static void dynamic_fsync_flush(struct work_struct *work)
{
	sys_sync();
}
static DECLARE_WORK(dynamic_fsync_work, dynamic_fsync_flush);

void dynamic_fsync_screen_event(bool on)
{
	WRITE_ONCE(screen_on, on);
	if (!on && READ_ONCE(dynamic_fsync_enabled) &&
	    atomic_xchg(&deferred_fsyncs, 0))
		schedule_work(&dynamic_fsync_work);
}

static int dynamic_fsync_fb_event(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct fb_event *fb_event = data;
	int blank;

	if (event != FB_EVENT_BLANK || !fb_event || !fb_event->data)
		return NOTIFY_DONE;

	blank = *(int *)fb_event->data;
	if (blank == FB_BLANK_UNBLANK)
		dynamic_fsync_screen_event(true);
	else if (blank == FB_BLANK_POWERDOWN)
		dynamic_fsync_screen_event(false);
	return NOTIFY_OK;
}

static struct notifier_block dynamic_fsync_fb_notifier = {
	.notifier_call = dynamic_fsync_fb_event,
};

static int dynamic_fsync_pm_event(struct notifier_block *nb,
				unsigned long event, void *data)
{
	if (event == PM_SUSPEND_PREPARE || event == PM_HIBERNATION_PREPARE) {
		WRITE_ONCE(screen_on, false);
		/* Only sync when a deferral is actually outstanding. */
		if (READ_ONCE(dynamic_fsync_enabled) &&
		    atomic_xchg(&deferred_fsyncs, 0))
			sys_sync();
	} else if (event == PM_POST_SUSPEND) {
		/* An aborted suspend never unblanks; don't latch the feature off. */
		WRITE_ONCE(screen_on, true);
	}
	return NOTIFY_OK;
}

static struct notifier_block dynamic_fsync_pm_notifier = {
	.notifier_call = dynamic_fsync_pm_event,
};

static ssize_t enabled_show(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", READ_ONCE(dynamic_fsync_enabled));
}

static ssize_t enabled_store(struct kobject *kobj,
			     struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	bool value;
	int ret = kstrtobool(buf, &value);

	if (ret)
		return ret;
	WRITE_ONCE(dynamic_fsync_enabled, value);
	if (!value) {
		atomic_set(&deferred_fsyncs, 0);
		sys_sync();
	}
	return count;
}
static struct kobj_attribute enabled_attr =
	__ATTR(Dyn_fsync_active, 0644, enabled_show, enabled_store);

static int __init dynamic_fsync_init(void)
{
	int ret;

	dynamic_fsync_kobj = kobject_create_and_add("dyn_fsync", kernel_kobj);
	if (!dynamic_fsync_kobj)
		return -ENOMEM;
	ret = sysfs_create_file(dynamic_fsync_kobj, &enabled_attr.attr);
	if (ret)
		goto put_kobj;
	ret = fb_register_client(&dynamic_fsync_fb_notifier);
	if (ret)
		goto remove_file;
	ret = register_pm_notifier(&dynamic_fsync_pm_notifier);
	if (ret)
		goto unregister_fb;
	return 0;

unregister_fb:
	fb_unregister_client(&dynamic_fsync_fb_notifier);
remove_file:
	sysfs_remove_file(dynamic_fsync_kobj, &enabled_attr.attr);
put_kobj:
	kobject_put(dynamic_fsync_kobj);
	return ret;
}
late_initcall(dynamic_fsync_init);
