#include <linux/device.h>

#include <linux/notifier.h>
#include <linux/cable_type_notifier.h>

#define SET_CABLE_TYPE_NOTIFIER_BLOCK(nb, fn, dev) do {	\
		(nb)->notifier_call = (fn);		\
		(nb)->priority = (dev);			\
	} while (0)

#define DESTROY_CABLE_TYPE_NOTIFIER_BLOCK(nb)			\
		SET_CABLE_TYPE_NOTIFIER_BLOCK(nb, NULL, -1)

static struct cable_type_notifier_struct cable_type_notifier;
static int cable_type_notifier_init_done = 0;
static int cable_type_notifier_init(void);

int cable_type_notifier_register(struct notifier_block *nb, notifier_fn_t notifier,
			cable_type_notifier_device_t listener)
{
	int ret = 0;

	if (!cable_type_notifier_init_done)
		cable_type_notifier_init();

	pr_info("%s: listener=%d register\n", __func__, listener);

	SET_CABLE_TYPE_NOTIFIER_BLOCK(nb, notifier, listener);
	ret = blocking_notifier_chain_register(&(cable_type_notifier.notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_register error(%d)\n",
				__func__, ret);

	/* current cable_type's attached_device status notify */
	nb->notifier_call(nb, cable_type_notifier.cmd,
			&(cable_type_notifier.attached_dev));

	return ret;
}

int cable_type_notifier_unregister(struct notifier_block *nb)
{
	int ret = 0;

	pr_info("%s: listener=%d unregister\n", __func__, nb->priority);

	ret = blocking_notifier_chain_unregister(&(cable_type_notifier.notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_unregister error(%d)\n",
				__func__, ret);
	DESTROY_CABLE_TYPE_NOTIFIER_BLOCK(nb);

	return ret;
}

static int cable_type_notifier_notify(void)
{
	int ret = 0;

	pr_info("%s: CMD=%d, DATA=%d\n", __func__, cable_type_notifier.cmd,
			cable_type_notifier.attached_dev);

	ret = blocking_notifier_call_chain(&(cable_type_notifier.notifier_call_chain),
			cable_type_notifier.cmd, &(cable_type_notifier.attached_dev));

	switch (ret) {
	case NOTIFY_STOP_MASK:
	case NOTIFY_BAD:
		pr_err("%s: notify error occur(0x%x)\n", __func__, ret);
		break;
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s: notify done(0x%x)\n", __func__, ret);
		break;
	default:
		pr_info("%s: notify status unknown(0x%x)\n", __func__, ret);
		break;
	}

	return ret;
}

static void cable_type_notifier_attach_attached_dev(cable_type_attached_dev_t new_dev)
{
	pr_info("%s: %s(%d) attach\n", __func__, cable_type_names[new_dev], new_dev);

	cable_type_notifier.cmd = CABLE_TYPE_NOTIFY_CMD_ATTACH;
	cable_type_notifier.attached_dev = new_dev;

	/* cable_type's attached_device attach broadcast */
	cable_type_notifier_notify();
}

static void cable_type_notifier_detach_attached_dev(cable_type_attached_dev_t cur_dev)
{
	pr_info("%s: %s(%d) detach\n", __func__, cable_type_names[cur_dev], cur_dev);

	cable_type_notifier.cmd = CABLE_TYPE_NOTIFY_CMD_DETACH;

	if (cable_type_notifier.attached_dev != cur_dev)
		pr_warn("%s: attached_dev of cable_type_notifier(%d) != cable_type_data(%d)\n",
				__func__, cable_type_notifier.attached_dev, cur_dev);

	if (cable_type_notifier.attached_dev != CABLE_TYPE_NONE) {
		/* cable_type's attached_device detach broadcast */
		cable_type_notifier_notify();
	}

	cable_type_notifier.attached_dev = CABLE_TYPE_NONE;
}

void cable_type_notifier_set_attached_dev(cable_type_attached_dev_t new_dev)
{
	if (!cable_type_notifier_init_done)
		cable_type_notifier_init();

	/* skip duplicated cable type */
	if (cable_type_notifier.attached_dev == new_dev)
		return;

	if (cable_type_notifier.attached_dev != CABLE_TYPE_NONE)
		pr_info("%s: attached_dev of cable_type_notifier(%d) != new cable_type_data(%d)\n",
				__func__, cable_type_notifier.attached_dev, new_dev);

	if (new_dev != CABLE_TYPE_NONE)
		cable_type_notifier_attach_attached_dev(new_dev);
	else
		cable_type_notifier_detach_attached_dev(cable_type_notifier.attached_dev);
}

static int cable_type_notifier_init(void)
{
	int ret = 0;

	if (cable_type_notifier_init_done) {
		pr_info("%s already done\n", __func__);
		return 0;
	}
	cable_type_notifier_init_done = 1;

	pr_info("%s\n", __func__);

	BLOCKING_INIT_NOTIFIER_HEAD(&(cable_type_notifier.notifier_call_chain));
	cable_type_notifier.cmd = CABLE_TYPE_NOTIFY_CMD_DETACH;
	cable_type_notifier.attached_dev = CABLE_TYPE_UNKNOWN;

	return ret;
}
device_initcall(cable_type_notifier_init);

