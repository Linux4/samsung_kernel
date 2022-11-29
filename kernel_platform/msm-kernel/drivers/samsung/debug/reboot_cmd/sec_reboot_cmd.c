// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020-2022 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/debug/sec_reboot_cmd.h>

#include <asm/cacheflush.h>

struct reboot_cmd_stage {
	struct mutex lock;
	struct list_head list;
	struct sec_reboot_cmd *dflt;
	struct notifier_block nb;
};

struct reboot_cmd_drvdata {
	struct builder bd;
	struct reboot_cmd_stage stage[SEC_RBCMD_STAGE_MAX];
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *dbgfs;
#endif
};

static struct reboot_cmd_drvdata *reboot_cmd;

static const char * const rbcmd_stage_name[] = {
	[SEC_RBCMD_STAGE_REBOOT_NOTIFIER] = "Reboot Notifier",
	[SEC_RBCMD_STAGE_RESTART_HANDLER] = "Restart Handler"
};

static __always_inline bool __rbcmd_is_probed(void)
{
	return !!reboot_cmd;
}

static struct reboot_cmd_stage *__rbcmd_get_stage(enum sec_rbcmd_stage s,
		struct reboot_cmd_drvdata *drvdata)
{
	BUG_ON((unsigned long)s >= (unsigned long)SEC_RBCMD_STAGE_MAX);

	return &drvdata->stage[s];
}

static inline int __rbcmd_add_cmd(enum sec_rbcmd_stage s,
		struct sec_reboot_cmd *rc)
{
	struct reboot_cmd_stage *stage;

	stage = __rbcmd_get_stage(s, reboot_cmd);

	mutex_lock(&stage->lock);
	list_add(&rc->list, &stage->list);
	mutex_unlock(&stage->lock);

	return 0;
}

int sec_rbcmd_add_cmd(enum sec_rbcmd_stage s, struct sec_reboot_cmd *rc)
{
	if (!__rbcmd_is_probed())
		return -EBUSY;

	return __rbcmd_add_cmd(s, rc);
}
EXPORT_SYMBOL(sec_rbcmd_add_cmd);

static inline int __rbcmd_del_cmd(enum sec_rbcmd_stage s,
		struct sec_reboot_cmd *rc)
{
	struct reboot_cmd_stage *stage;

	stage = __rbcmd_get_stage(s, reboot_cmd);

	mutex_lock(&stage->lock);
	list_del(&rc->list);
	mutex_unlock(&stage->lock);

	return 0;
}

int sec_rbcmd_del_cmd(enum sec_rbcmd_stage s, struct sec_reboot_cmd *rc)
{
	if (!__rbcmd_is_probed())
		return -EBUSY;

	return __rbcmd_del_cmd(s, rc);
}
EXPORT_SYMBOL(sec_rbcmd_del_cmd);

static inline int __rbcmd_set_default_cmd(enum sec_rbcmd_stage s,
		struct sec_reboot_cmd *rc)
{
	struct reboot_cmd_stage *stage;

	stage = __rbcmd_get_stage(s, reboot_cmd);

	mutex_lock(&stage->lock);
	if (stage->dflt) {
		pr_warn("default reboot cmd for %s is already registered. ignored!\n",
				rbcmd_stage_name[s]);
		pr_warn("Caller is %pS\n", __builtin_return_address(0));
	} else {
		stage->dflt = rc;
	}
	mutex_unlock(&stage->lock);

	return 0;
}

int sec_rbcmd_set_default_cmd(enum sec_rbcmd_stage s,
		struct sec_reboot_cmd *rc)
{
	if (!__rbcmd_is_probed())
		return -EBUSY;

	return __rbcmd_set_default_cmd(s, rc);
}
EXPORT_SYMBOL(sec_rbcmd_set_default_cmd);

static inline int __rbcmd_unset_default_cmd(enum sec_rbcmd_stage s,
		struct sec_reboot_cmd *rc)
{
	struct reboot_cmd_stage *stage;

	stage = __rbcmd_get_stage(s, reboot_cmd);

	mutex_lock(&stage->lock);
	if (!stage->dflt) {
		pr_warn("default reason for %s is already unset - ignored.\n",
				rbcmd_stage_name[s]);
	} else if (stage->dflt != rc) {
		pr_warn("current default reason for %s is different from you requested - ignored.\n",
				rbcmd_stage_name[s]);
	} else {
		stage->dflt = NULL;
	}
	mutex_unlock(&stage->lock);

	return 0;
}

int sec_rbcmd_unset_default_cmd(enum sec_rbcmd_stage s,
		struct sec_reboot_cmd *rc)
{
	if (!__rbcmd_is_probed())
		return -EBUSY;

	return __rbcmd_unset_default_cmd(s, rc);
}
EXPORT_SYMBOL(sec_rbcmd_unset_default_cmd);

static int __rbcmd_handle_each_cmd(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	size_t len = strlen(rc->cmd);

	if (likely(strncmp(param->cmd, rc->cmd, len)))
		return SEC_RBCMD_HANDLE_DONE;

	return rc->func(rc, param, one_of_multi);
}

static int __rbcmd_handle_single_cmd(struct reboot_cmd_stage *stage,
		struct sec_reboot_param *single, bool one_of_multi)
{
	struct sec_reboot_cmd *rc;
	int handled = SEC_RBCMD_HANDLE_DONE;

	/* NOTE: mutex_lock/_unlock may not be needed when this
	 * function works.
	 */
	list_for_each_entry(rc, &stage->list, list) {
		handled = __rbcmd_handle_each_cmd(rc, single,
				one_of_multi);
		if (handled & SEC_RBCMD_HANDLE_STOP_MASK)
			break;
	}

	return handled;
}

static int __rbcmd_handle_multi_cmd(struct reboot_cmd_stage *stage,
		struct sec_reboot_param *multi, const char *delim)
{
	const char *multi_cmd = multi->cmd;
	static char tmp_multi_cmd[256];	/* reserve .bss for strsep */
	char *stringp;
	struct sec_reboot_param __single;
	struct sec_reboot_param *single = &__single;
	int handled;
	int ret = SEC_RBCMD_HANDLE_DONE;

	strlcpy(tmp_multi_cmd, multi_cmd, ARRAY_SIZE(tmp_multi_cmd));

	single->mode = multi->mode;
	stringp = tmp_multi_cmd;
	while (true) {
		single->cmd = strsep(&stringp, delim);
		if (!single->cmd)
			break;

		handled = __rbcmd_handle_single_cmd(stage, single, true);
		if (handled == SEC_RBCMD_HANDLE_BAD)
			pr_warn("bad - %s", multi_cmd);

		ret |= handled;

		if (handled & SEC_RBCMD_HANDLE_ONESHOT_MASK)
			break;
	}

	/* clear oneshot bti before return
	 * because caller does not handle this
	 */
	ret &= ~(SEC_RBCMD_HANDLE_ONESHOT_MASK);

	return ret;
}

static bool __rbcmd_is_mulit_cmd(const char *cmd, const char *delim)
{
	char *pos;

	pos = strnstr(cmd, delim, strlen(cmd));

	return pos ? true : false;
}

static int __rbcmd_handle(struct reboot_cmd_stage *stage,
		struct sec_reboot_param *param)
{
	const char *cmd = param->cmd;
	const char *delim = ":";
	int handled = SEC_RBCMD_HANDLE_DONE;

	if (!cmd || !strlen(cmd) || !strncmp(cmd, "adb", 3))
		goto call_default_hanlder;

	if (__rbcmd_is_mulit_cmd(cmd, delim))
		handled = __rbcmd_handle_multi_cmd(stage, param, delim);
	else
		handled = __rbcmd_handle_single_cmd(stage, param, false);

	if ((handled & SEC_RBCMD_HANDLE_OK) == SEC_RBCMD_HANDLE_OK)
		return handled;

call_default_hanlder:
	if (stage->dflt) {
		pr_info("call default handler\n");
		handled = stage->dflt->func(stage->dflt, param, false);
	}

	return handled;
}

static int sec_rbcmd_notifier_call(struct notifier_block *this,
		unsigned long mode, void *cmd)
{
	struct reboot_cmd_stage *stage =
			container_of(this, struct reboot_cmd_stage, nb);
	struct sec_reboot_param __param = {
		.cmd = cmd,
		.mode = mode,
	};
	struct sec_reboot_param *param = &__param;
	int err;
	int ret = NOTIFY_DONE;

	pr_info("cmd : %s\n", param->cmd);

	err = __rbcmd_handle(stage, param);
	switch (err) {
	case SEC_RBCMD_HANDLE_DONE:
		pr_warn("any reboot reason did not handle this command - %s",
				param->cmd);
		break;
	case SEC_RBCMD_HANDLE_OK:
		ret = NOTIFY_OK;
		break;
	default:
		if (err & SEC_RBCMD_HANDLE_BAD)
			pr_err("error occured during iteration\n");
		else
			pr_err("unknown return value - %d\n", err);
		break;
	}

#if IS_BUILTIN(CONFIG_SEC_REBOOT_CMD)
	flush_cache_all();
#else
	dsb(sy);
#endif

#if !IS_ENABLED(CONFIG_ARM64)
	outer_flush_all();
#endif

	return ret;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static void __rbcmd_dbgfs_show_each_cmd_locked(struct seq_file *m,
		struct sec_reboot_cmd *rc)
{
	seq_printf(m, "[<%p>] %s\n", rc, rc->cmd);
	seq_printf(m, "  - func : [<%p>] %ps\n", rc->func, rc->func);

	seq_puts(m, "\n");
}

static void __rbcmd_dbgfs_show_default_cmd_locked(struct seq_file *m,
		struct reboot_cmd_stage *stage)
{
	seq_puts(m, "+ Default :\n");

	if (stage->dflt)
		__rbcmd_dbgfs_show_each_cmd_locked(m, stage->dflt);
	else
		seq_puts(m, "[WARN] default command is not set\n");

	seq_puts(m, "\n");
}

static void __rbcmd_dbgfs_show_each_stage(struct seq_file *m,
		struct reboot_cmd_stage *stage)
{
	struct sec_reboot_cmd *rc;

	seq_printf(m, "+ Priority : %d\n", stage->nb.priority);

	mutex_lock(&stage->lock);

	__rbcmd_dbgfs_show_default_cmd_locked(m, stage);

	list_for_each_entry(rc, &stage->list, list) {
		__rbcmd_dbgfs_show_each_cmd_locked(m, rc);
	}

	mutex_unlock(&stage->lock);
}

static int sec_rbcmd_dbgfs_show_all(struct seq_file *m, void *unsed)
{
	struct reboot_cmd_drvdata *drvdata = m->private;
	struct reboot_cmd_stage *stage;
	enum sec_rbcmd_stage s;

	for (s = SEC_RBCMD_STAGE_1ST; s < SEC_RBCMD_STAGE_MAX; s++) {
		seq_printf(m, "* STAGE : %s\n\n", rbcmd_stage_name[s]);
		stage = __rbcmd_get_stage(s, drvdata);
		__rbcmd_dbgfs_show_each_stage(m, stage);
	}

	return 0;
}

static int sec_rbcmd_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_rbcmd_dbgfs_show_all, inode->i_private);
}

static const struct file_operations sec_rbcmd_dgbfs_fops = {
	.open = sec_rbcmd_dbgfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int __rbcmd_debugfs_create(struct builder *bd)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);

	drvdata->dbgfs = debugfs_create_file("sec_reboot_cmd", 0440,
			NULL, drvdata, &sec_rbcmd_dgbfs_fops);

	return 0;
}

static void __rbcmd_debugfs_remove(struct builder *bd)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);

	debugfs_remove(drvdata->dbgfs);
}
#else
static int __rbcmd_debugfs_create(struct builder *bd) { return 0; }
static void __rbcmd_debugfs_remove(struct builder *bd) {}
#endif

static int __rbcmd_parse_dt_reboot_notifier_priority(struct builder *bd,
		struct device_node *np)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);
	struct reboot_cmd_stage *stage;
	s32 priority;
	int err;

	err = of_property_read_s32(np, "sec,reboot_notifier-priority",
			&priority);
	if (err)
		return -EINVAL;

	stage = __rbcmd_get_stage(SEC_RBCMD_STAGE_REBOOT_NOTIFIER, drvdata);
	stage->nb.priority = (int)priority;

	return 0;
}

static int __rbcmd_parse_dt_restart_handler_priority(struct builder *bd,
		struct device_node *np)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);
	struct reboot_cmd_stage *stage;
	s32 priority;
	int err;

	err = of_property_read_s32(np, "sec,restart_handler-priority",
			&priority);
	if (err)
		return -EINVAL;

	stage = __rbcmd_get_stage(SEC_RBCMD_STAGE_RESTART_HANDLER, drvdata);
	stage->nb.priority = (int)priority;

	return 0;
}

static const struct dt_builder __rbcmd_dt_builder[] = {
	DT_BUILDER(__rbcmd_parse_dt_reboot_notifier_priority),
	DT_BUILDER(__rbcmd_parse_dt_restart_handler_priority),
};

static int __rbcmd_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __rbcmd_dt_builder,
			ARRAY_SIZE(__rbcmd_dt_builder));
}

static void __rbcmd_probe_each_stage(struct reboot_cmd_stage *stage)
{
	mutex_init(&stage->lock);
	INIT_LIST_HEAD(&stage->list);
}

static void __rbcmd_remove_each_stage(struct reboot_cmd_stage *stage)
{
	mutex_destroy(&stage->lock);
}

static int __rbcmd_probe_prolog(struct builder *bd)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);
	struct reboot_cmd_stage *stage;
	enum sec_rbcmd_stage s;

	for (s = SEC_RBCMD_STAGE_1ST; s < SEC_RBCMD_STAGE_MAX; s++) {
		stage = __rbcmd_get_stage(s, drvdata);
		__rbcmd_probe_each_stage(stage);
	}

	return 0;
}

static void __rbcmd_remove_epilog(struct builder *bd)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);
	struct reboot_cmd_stage *stage;
	enum sec_rbcmd_stage s;

	for (s = SEC_RBCMD_STAGE_1ST; s < SEC_RBCMD_STAGE_MAX; s++) {
		stage = __rbcmd_get_stage(s, drvdata);
		__rbcmd_remove_each_stage(stage);
	}
}

static int __rbcmd_register_reboot_notifier(struct builder *bd)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);
	struct reboot_cmd_stage *stage;

	stage = __rbcmd_get_stage(SEC_RBCMD_STAGE_REBOOT_NOTIFIER, drvdata);

	stage->nb.notifier_call = sec_rbcmd_notifier_call;

	return register_reboot_notifier(&stage->nb);
}

static void __rbcmd_unregister_reboot_notifier(struct builder *bd)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);
	struct reboot_cmd_stage *stage;

	stage = __rbcmd_get_stage(SEC_RBCMD_STAGE_REBOOT_NOTIFIER, drvdata);

	unregister_reboot_notifier(&stage->nb);
}

static int __rbcmd_register_restart_handler(struct builder *bd)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);
	struct reboot_cmd_stage *stage;

	stage = __rbcmd_get_stage(SEC_RBCMD_STAGE_RESTART_HANDLER, drvdata);

	stage->nb.notifier_call = sec_rbcmd_notifier_call;

	return register_restart_handler(&stage->nb);
}

static void __rbcmd_unregister_restart_handler(struct builder *bd)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);
	struct reboot_cmd_stage *stage;

	stage = __rbcmd_get_stage(SEC_RBCMD_STAGE_RESTART_HANDLER, drvdata);

	unregister_restart_handler(&stage->nb);
}

static int __rbcmd_probe_epilog(struct builder *bd)
{
	struct reboot_cmd_drvdata *drvdata =
			container_of(bd, struct reboot_cmd_drvdata, bd);
	struct device *dev = bd->dev;

	dev_set_drvdata(dev, drvdata);
	reboot_cmd = drvdata;	/* set a singleton */

	return 0;
}

static void __rbcmd_remove_prolog(struct builder *bd)
{
	/* FIXME: This is not a graceful exit. */
	reboot_cmd = NULL;
}

static int __rbcmd_probe(struct platform_device *pdev,
		const struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct reboot_cmd_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __rbcmd_remove(struct platform_device *pdev,
		const struct dev_builder *builder, ssize_t n)
{
	struct reboot_cmd_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

static const struct dev_builder __rbcmd_dev_builder[] = {
	DEVICE_BUILDER(__rbcmd_parse_dt, NULL),
	DEVICE_BUILDER(__rbcmd_probe_prolog, __rbcmd_remove_epilog),
	DEVICE_BUILDER(__rbcmd_register_reboot_notifier,
		       __rbcmd_unregister_reboot_notifier),
	DEVICE_BUILDER(__rbcmd_register_restart_handler,
		       __rbcmd_unregister_restart_handler),
	DEVICE_BUILDER(__rbcmd_debugfs_create, __rbcmd_debugfs_remove),
	DEVICE_BUILDER(__rbcmd_probe_epilog, __rbcmd_remove_prolog),
};

static int sec_rbcmd_probe(struct platform_device *pdev)
{
	return __rbcmd_probe(pdev, __rbcmd_dev_builder,
			ARRAY_SIZE(__rbcmd_dev_builder));
}

static int sec_rbcmd_remove(struct platform_device *pdev)
{
	return __rbcmd_remove(pdev, __rbcmd_dev_builder,
			ARRAY_SIZE(__rbcmd_dev_builder));
}

static const struct of_device_id sec_rbcmd_match_table[] = {
	{ .compatible = "samsung,reboot_cmd" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_rbcmd_match_table);

static struct platform_driver sec_rbcmd_driver = {
	.driver = {
		.name = "samsung,reboot_cmd",
		.of_match_table = of_match_ptr(sec_rbcmd_match_table),
	},
	.probe = sec_rbcmd_probe,
	.remove = sec_rbcmd_remove,
};

static __always_inline void __rbcmd_build_assert(void)
{
	BUILD_BUG_ON(ARRAY_SIZE(rbcmd_stage_name) != SEC_RBCMD_STAGE_MAX);
}

static __init int sec_rbcmd_init(void)
{
	__rbcmd_build_assert();

	return platform_driver_register(&sec_rbcmd_driver);
}
core_initcall(sec_rbcmd_init);

static __exit void sec_rbcmd_exit(void)
{
	platform_driver_unregister(&sec_rbcmd_driver);
}
module_exit(sec_rbcmd_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Reboot command iterator");
MODULE_LICENSE("GPL v2");
