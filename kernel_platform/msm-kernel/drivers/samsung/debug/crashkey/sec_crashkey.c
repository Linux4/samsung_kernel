// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2019-2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>

#include <linux/samsung/bsp/sec_key_notifier.h>
#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/debug/sec_debug.h>
#include <linux/samsung/debug/sec_log_buf.h>

#define CRASHKEY_MOCK_NAME	"Mock Crash Key"

struct crashkey_pattern {
	unsigned int keycode;
	bool down;
};

struct crashkey_kelog {
	struct crashkey_pattern *desired;
	struct crashkey_pattern *received;
	size_t nr_pattern;
	size_t sequence;
	unsigned int *used_key;
	size_t nr_used_key;
};

struct crashkey_timer {
	struct ratelimit_state rs;
	int interval;
};

struct crashkey_notify {
	struct atomic_notifier_head list;
	struct notifier_block panic;
	const char *panic_msg;
};

struct crashkey_drvdata {
	struct builder bd;
	struct list_head list;
	struct notifier_block nb;
	const char *name;
	struct crashkey_kelog keylog;
	struct crashkey_timer timer;
	struct crashkey_notify notify;
};

static DEFINE_MUTEX(crashkey_list_lock);
static LIST_HEAD(crashkey_dev_list);

static struct crashkey_drvdata *__crashkey_find_by_name_locked(const char *name)
{
	struct crashkey_drvdata *drvdata;
	size_t len = strlen(name) + 1;

	list_for_each_entry(drvdata, &crashkey_dev_list, list) {
		if (!memcmp(drvdata->name, name, len))
			return drvdata;
	}

	return ERR_PTR(-ENOENT);
}

static int __crashkey_add_preparing_panic(struct crashkey_drvdata *drvdata,
		struct notifier_block *nb)
{
	struct crashkey_notify *notify = &drvdata->notify;

	return atomic_notifier_chain_register(&notify->list, nb);
}

int sec_crashkey_add_preparing_panic(struct notifier_block *nb,
		const char *name)
{
	struct crashkey_drvdata *drvdata;
	int err;

	mutex_lock(&crashkey_list_lock);

	drvdata = __crashkey_find_by_name_locked(name);
	if (IS_ERR(drvdata)) {
		pr_warn("%s is not a valid drvdata device!\n", name);
		err = -ENODEV;
		goto err_invalid_name;
	}

	err = __crashkey_add_preparing_panic(drvdata, nb);
	if (err) {
		struct device *dev = drvdata->bd.dev;

		dev_warn(dev, "failed to add a notifier for %s (%d)!\n",
				name, err);
		dev_warn(dev, "Caller is %pS\n", __builtin_return_address(0));
	}

err_invalid_name:
	mutex_unlock(&crashkey_list_lock);
	return err;
}
EXPORT_SYMBOL(sec_crashkey_add_preparing_panic);

static int __crashkey_del_preparing_panic(struct crashkey_drvdata *drvdata,
		struct notifier_block *nb)
{
	struct crashkey_notify *notify = &drvdata->notify;

	return atomic_notifier_chain_unregister(&notify->list, nb);
}

int sec_crashkey_del_preparing_panic(struct notifier_block *nb,
		const char *name)
{
	struct crashkey_drvdata *drvdata;
	int err;

	mutex_lock(&crashkey_list_lock);

	drvdata = __crashkey_find_by_name_locked(name);
	if (IS_ERR(drvdata)) {
		pr_warn("%s is not a valid drvdata device!\n", name);
		err = -ENODEV;
		goto err_invalid_name;
	}

	err = __crashkey_del_preparing_panic(drvdata, nb);
	if (err) {
		struct device *dev = drvdata->bd.dev;

		dev_warn(dev, "failed to remove a notifier for %s!\n", name);
		dev_warn(dev, "Caller is %pS\n", __builtin_return_address(0));
	}

err_invalid_name:
	mutex_unlock(&crashkey_list_lock);
	return err;
}
EXPORT_SYMBOL(sec_crashkey_del_preparing_panic);

static __always_inline bool __crashkey_is_same_pattern(
		struct crashkey_drvdata *drvdata, size_t len)
{
	struct crashkey_kelog *keylog = &drvdata->keylog;
	struct crashkey_pattern *desired = keylog->desired;
	struct crashkey_pattern *received = keylog->received;
	size_t i;

	for (i = 0; i < len; i++) {
		if ((desired[i].keycode != received[i].keycode) ||
		    (desired[i].down != received[i].down))
			return false;
	}

	return true;
}

static __always_inline void __crashkey_clear_received_pattern(
		struct crashkey_drvdata *drvdata)
{
	struct crashkey_kelog *keylog = &drvdata->keylog;

	keylog->sequence = 0;
	memset(keylog->received, 0x0,
			keylog->nr_pattern * sizeof(struct crashkey_pattern));
}

static __always_inline void __crashkey_call_crashkey_notify(
		struct crashkey_drvdata *drvdata)
{
	struct crashkey_notify *notify = &drvdata->notify;

	atomic_notifier_call_chain(&notify->list, 0, NULL);
}

static int sec_crashkey_notifier_call(struct notifier_block *this,
		unsigned long type, void *data)
{
	struct crashkey_drvdata *drvdata =
			container_of(this, struct crashkey_drvdata, nb);
	struct sec_key_notifier_param *param = data;
	struct crashkey_kelog *keylog = &drvdata->keylog;
	struct crashkey_timer *timer = &drvdata->timer;
	size_t idx = keylog->sequence;

	if (idx >= keylog->nr_pattern)
		goto clear_state;

	keylog->received[idx].keycode = param->keycode;
	keylog->received[idx].down = !!param->down;
	keylog->sequence++;

	if (!__crashkey_is_same_pattern(drvdata, keylog->sequence))
		goto clear_state;

	if (!timer->interval || !__ratelimit(&timer->rs)) {
		if (keylog->sequence == keylog->nr_pattern)
			__crashkey_call_crashkey_notify(drvdata);
		else if (timer->interval)
			goto clear_state;
	}

	return NOTIFY_OK;

clear_state:
	__crashkey_clear_received_pattern(drvdata);
	return NOTIFY_OK;
}

#if IS_ENABLED(CONFIG_KUNIT) && IS_ENABLED(CONFIG_UML)
int kunit_crashkey_mock_key_notifier_call(const char *name,
		unsigned long type, struct sec_key_notifier_param *data)
{
	struct crashkey_drvdata *drvdata;

	mutex_lock(&crashkey_list_lock);
	drvdata = __crashkey_find_by_name_locked(CRASHKEY_MOCK_NAME);
	mutex_unlock(&crashkey_list_lock);

	if (IS_ERR(drvdata))
		return -ENOEV;

	return sec_crashkey_notifier_call(&drvdata->nb, type, data);
}
#endif

static int __crashkey_parse_dt_name(struct builder *bd, struct device_node *np)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);

	return of_property_read_string(np, "sec,name", &drvdata->name);
}

static int __crashkey_parse_dt_check_debug_level(struct builder *bd,
		struct device_node *np)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct device *dev = bd->dev;
	int nr_dbg_level;
	unsigned int sec_dbg_level;
	u32 dbg_level;
	int i;

	nr_dbg_level = of_property_count_u32_elems(np, "sec,debug_level");
	if (nr_dbg_level <= 0) {
		dev_warn(dev, "this crashkey_dev (%s) will be enabled all sec debug levels!\n",
				drvdata->name);
		return 0;
	}

	sec_dbg_level = sec_debug_level();
	for (i = 0; i < nr_dbg_level; i++) {
		of_property_read_u32_index(np, "sec,debug_level", i,
				&dbg_level);
		if (sec_dbg_level == (unsigned int)dbg_level)
			return 0;
	}

	return -ENODEV;
}

static int __crashkey_parse_dt_panic_msg(struct builder *bd,
		struct device_node *np)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct crashkey_notify *notify = &drvdata->notify;

	return of_property_read_string(np, "sec,panic_msg", &notify->panic_msg);
}

static int __crashkey_parse_dt_interval(struct builder *bd,
		struct device_node *np)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct crashkey_timer *timer = &drvdata->timer;
	s32 interval;
	int err;

	err = of_property_read_s32(np, "sec,interval", &interval);
	if (err)
		return -EINVAL;

	timer->interval = (int)interval * HZ;

	return 0;
}

static int __crashkey_parse_dt_desired_pattern(struct builder *bd,
		struct device_node *np)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct device *dev = bd->dev;
	struct crashkey_kelog *keylog = &drvdata->keylog;
	struct crashkey_pattern *desired;
	struct crashkey_pattern *received;
	int nr_pattern;
	u32 keycode, down;
	int i;

	nr_pattern = of_property_count_u32_elems(np, "sec,desired_pattern");
	nr_pattern /= 2;	/* <keycode, down> */
	if (nr_pattern <= 0)
		return -EINVAL;

	desired = devm_kmalloc_array(dev,
			nr_pattern, sizeof(*desired), GFP_KERNEL);
	received = devm_kcalloc(dev, nr_pattern, sizeof(*received), GFP_KERNEL);
	if (!desired || !received)
		return -ENOMEM;

	keylog->nr_pattern = (size_t)nr_pattern;
	keylog->desired = desired;
	keylog->received = received;

	for (i = 0; i < nr_pattern; i++) {
		of_property_read_u32_index(np, "sec,desired_pattern",
				2 * i, &keycode);
		of_property_read_u32_index(np, "sec,desired_pattern",
				2 * i + 1, &down);

		desired[i].keycode = keycode;
		desired[i].down = !!down;
	}

	return 0;
}

static struct dt_builder __crashkey_dt_builder[] = {
	DT_BUILDER(__crashkey_parse_dt_name),
	DT_BUILDER(__crashkey_parse_dt_check_debug_level),
	DT_BUILDER(__crashkey_parse_dt_panic_msg),
	DT_BUILDER(__crashkey_parse_dt_interval),
	DT_BUILDER(__crashkey_parse_dt_desired_pattern),
};

static int __crashkey_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __crashkey_dt_builder,
			ARRAY_SIZE(__crashkey_dt_builder));
}

static int __crashkey_probe_prolog(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct crashkey_notify *notify = &drvdata->notify;
	struct crashkey_kelog *keylog = &drvdata->keylog;
	struct crashkey_timer *timer = &drvdata->timer;

	ATOMIC_INIT_NOTIFIER_HEAD(&notify->list);

	ratelimit_state_init(&timer->rs, timer->interval,
			keylog->nr_pattern - 1);

	return 0;
}

static int __crashkey_add_to_crashkey_dev_list(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);

	mutex_lock(&crashkey_list_lock);
	list_add(&drvdata->list, &crashkey_dev_list);
	mutex_unlock(&crashkey_list_lock);

	return 0;
}

static void __crashkey_del_from_crashkey_dev_list(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);

	mutex_lock(&crashkey_list_lock);
	list_del(&drvdata->list);
	mutex_unlock(&crashkey_list_lock);
}

static int __crashkey_panic_on_matched(struct notifier_block *this,
		unsigned long type, void *data)
{
	struct crashkey_notify *notify =
			container_of(this, struct crashkey_notify, panic);

	panic("%s", notify->panic_msg);
}

static int __crashkey_set_panic_on_matched(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct crashkey_notify *notify = &drvdata->notify;
	int err;

	/* NOTE: register a calling kernel panic in the end of notifier chain */
	notify->panic.notifier_call = __crashkey_panic_on_matched;
	notify->panic.priority = INT_MIN;

	err = sec_crashkey_add_preparing_panic(&notify->panic, drvdata->name);
	if (err)
		return err;

	return 0;
}

static void __crashkey_unset_panic_on_matched(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct crashkey_notify *notify = &drvdata->notify;

	sec_crashkey_del_preparing_panic(&notify->panic, drvdata->name);
}

static int __crashkey_init_used_key(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct device *dev = bd->dev;
	struct crashkey_kelog *keylog = &drvdata->keylog;
	struct crashkey_pattern *desired = keylog->desired;
	static unsigned int *used_key;
	size_t nr_used_key;
	bool is_new;
	size_t i, j;

	used_key = devm_kmalloc_array(dev,
			keylog->nr_pattern, sizeof(*used_key), GFP_KERNEL);
	if (!used_key)
		return -ENOMEM;

	used_key[0] = desired[0].keycode;
	nr_used_key = 1;

	for (i = 1; i < keylog->nr_pattern; i++) {
		for (j = 0, is_new = true; j < nr_used_key; j++) {
			if (used_key[j] == desired[i].keycode)
				is_new = false;
		}

		if (is_new)
			used_key[nr_used_key++] = desired[i].keycode;
	}

	keylog->used_key = used_key;
	keylog->nr_used_key = nr_used_key;

	return 0;
}

static int __crashkey_install_keyboard_notifier(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct crashkey_kelog *keylog = &drvdata->keylog;
	int err;

	drvdata->nb.notifier_call = sec_crashkey_notifier_call;
	err = sec_kn_register_notifier(&drvdata->nb,
			keylog->used_key, keylog->nr_used_key);

	return err;
}

static void __crashkey_uninstall_keyboard_notifier(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct crashkey_kelog *keylog = &drvdata->keylog;

	sec_kn_unregister_notifier(&drvdata->nb,
			keylog->used_key, keylog->nr_used_key);
}

static int __crashkey_probe_epilog(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct device *dev = bd->dev;

	dev_set_drvdata(dev, drvdata);

	return 0;
}

static int __crashkey_probe(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct crashkey_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __crashkey_remove(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct crashkey_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

#if IS_ENABLED(CONFIG_KUNIT) && IS_ENABLED(CONFIG_UML)
static int __crashkey_mock_parse_dt_name(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);

	drvdata->name = CRASHKEY_MOCK_NAME;

	return 0;
}

static int __crashkey_mock_parse_dt_interval(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct crashkey_timer *timer = &drvdata->timer;

	/* FIXME: time interval of mock-crashkey is fixed to 1s */
	timer->interval = 1 * HZ;

	return 0;
}

static const struct sec_key_notifier_param *__crashkey_mock_kn_param;
static size_t __crashkey_nr_mock_kn_param;

/* TODO: This function must be called before calling
 * 'kunit_crashkey_mock_probe' function in unit test.
 */
void kunit_crashkey_mock_set_desired_pattern(
		const struct sec_key_notifier_param *kn_param,
		const size_t nr_kn_param)
{
	__crashkey_mock_kn_param = kn_param;
	__crashkey_nr_mock_kn_param = nr_kn_param;
}

static int __crashkey_mock_parse_dt_desired_pattern(struct builder *bd)
{
	struct crashkey_drvdata *drvdata =
			container_of(bd, struct crashkey_drvdata, bd);
	struct device *dev = drvdata->bd.dev;
	struct crashkey_kelog *keylog = &drvdata->keylog;
	const struct sec_key_notifier_param *kn_param =
			__crashkey_mock_kn_param;
	size_t nr_pattern = __crashkey_nr_mock_kn_param;
	struct crashkey_pattern *desired;
	struct crashkey_pattern *received;
	int i;

	desired = devm_kmalloc_array(dev,
			nr_pattern, sizeof(*desired), GFP_KERNEL);
	received = devm_kcalloc(dev, nr_pattern, sizeof(*received), GFP_KERNEL);
	if (!desired || !received)
		return -ENOMEM;

	keylog->nr_pattern = nr_pattern;
	keylog->desired = desired;
	keylog->received = received;

	for (i = 0; i < nr_pattern; i++) {
		desired[i].keycode = kn_param[i].keycode;
		desired[i].down = !!kn_param[i].down;
	}

	return 0;
}

static struct dev_builder __crashkey_mock_dev_builder[] = {
	DEVICE_BUILDER(__crashkey_mock_parse_dt_name, NULL),
	DEVICE_BUILDER(__crashkey_mock_parse_dt_interval, NULL),
	DEVICE_BUILDER(__crashkey_mock_parse_dt_desired_pattern, NULL),
	DEVICE_BUILDER(__crashkey_probe_prolog, NULL),
	DEVICE_BUILDER(__crashkey_add_to_crashkey_dev_list,
		       __crashkey_del_from_crashkey_dev_list),
	DEVICE_BUILDER(__crashkey_probe_epilog, NULL),
};

int kunit_crashkey_mock_probe(struct platform_device *pdev)
{
	return __crashkey_probe(pdev, __crashkey_mock_dev_builder,
			ARRAY_SIZE(__crashkey_mock_dev_builder));
}

int kunit_crashkey_mock_remove(struct platform_device *pdev)
{
	return __crashkey_remove(pdev, __crashkey_mock_dev_builder,
			ARRAY_SIZE(__crashkey_mock_dev_builder));
}
#endif

static struct dev_builder __crashkey_dev_builder[] = {
	DEVICE_BUILDER(__crashkey_parse_dt, NULL),
	DEVICE_BUILDER(__crashkey_probe_prolog, NULL),
	DEVICE_BUILDER(__crashkey_add_to_crashkey_dev_list,
		       __crashkey_del_from_crashkey_dev_list),
	DEVICE_BUILDER(__crashkey_set_panic_on_matched,
		       __crashkey_unset_panic_on_matched),
	DEVICE_BUILDER(__crashkey_init_used_key, NULL),
	DEVICE_BUILDER(__crashkey_install_keyboard_notifier,
		       __crashkey_uninstall_keyboard_notifier),
	DEVICE_BUILDER(__crashkey_probe_epilog, NULL),
};

static int sec_crashkey_probe(struct platform_device *pdev)
{
	return __crashkey_probe(pdev, __crashkey_dev_builder,
			ARRAY_SIZE(__crashkey_dev_builder));
}

static int sec_crashkey_remove(struct platform_device *pdev)
{
	return __crashkey_remove(pdev, __crashkey_dev_builder,
			ARRAY_SIZE(__crashkey_dev_builder));
}

static const struct of_device_id sec_crashkey_match_table[] = {
	{ .compatible = "samsung,crashkey" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_crashkey_match_table);

static struct platform_driver sec_crashkey_driver = {
	.driver = {
		.name = "samsung,crashkey",
		.of_match_table = of_match_ptr(sec_crashkey_match_table),
	},
	.probe = sec_crashkey_probe,
	.remove = sec_crashkey_remove,
};

static int sec_crashkey_suspend(void)
{
	struct crashkey_drvdata *drvdata;

	list_for_each_entry(drvdata, &crashkey_dev_list, list) {
		__crashkey_clear_received_pattern(drvdata);
	}

	return 0;
}

static struct syscore_ops sec_crashkey_syscore_ops = {
	.suspend = sec_crashkey_suspend,
};

static int __init sec_crashkey_init(void)
{
	int err;

	err = platform_driver_register(&sec_crashkey_driver);
	if (err)
		return err;

	register_syscore_ops(&sec_crashkey_syscore_ops);

	return 0;
}
core_initcall_sync(sec_crashkey_init);

static void __exit sec_crashkey_exit(void)
{
	unregister_syscore_ops(&sec_crashkey_syscore_ops);
	platform_driver_unregister(&sec_crashkey_driver);
}
module_exit(sec_crashkey_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Force key crash driver");
MODULE_LICENSE("GPL v2");
