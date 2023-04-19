#include <linux/kernel.h>
#include <linux/module.h>
#include "slsi_bt_err.h"
#include "slsi_bt_log.h"

static void (*handler)(int reason);
static atomic_t err_cnt;

/* Store error history from first.
 * err_history[0] shows that is the first driver error after system booting.
 */
static int err_history[SLSI_BT_ERR_HISTORY_SIZE], sp, last_err;

void slsi_bt_err_inform(int reason)
{
	if (!handler)
		return;

	atomic_inc(&err_cnt);

	if (sp < SLSI_BT_ERR_HISTORY_SIZE)
		err_history[sp++] = reason;

	BT_ERR("count: %d, reason 0x%2x\n", atomic_read(&err_cnt), reason);
	last_err = reason;
}

void slsi_bt_err(int reason)
{
	slsi_bt_err_inform(reason);
	if (handler)
		handler(reason);
}

int slsi_bt_err_status(void)
{
	return handler ? atomic_read(&err_cnt) : 0;
}

int slsi_bt_err_proc_show(struct seq_file *m, void *v)
{
	int i = 0;
	seq_puts(m, "\nError history: \n");
	seq_printf(m, "  Error status           = %d\n", slsi_bt_err_status());
	seq_printf(m, "  Last error             = %d\n", last_err);
	seq_puts(m, "  Error History after module initialized\n");;
	for (i = 0; i < sp; i++)
		seq_printf(m, "    History stack[%d]       = %d\n", i,
			(unsigned int)err_history[i]);
	return 0;
}

static int force_crash_set(const char *val, const struct kernel_param *kp)
{
	int ret;
	u32 value;

	ret = kstrtou32(val, 0, &value);
	BT_DBG("ret=%d val=%s value=0x%X\n", ret, val, value);
	if (!ret && value == 0xDEADDEAD)
		slsi_bt_err(SLSI_BT_ERR_FORCE_CRASH);

	return ret;
}

static struct kernel_param_ops slsi_bt_force_crash_ops = {
	.set = force_crash_set,
	.get = NULL,
};

static struct kernel_param_ops slsi_bt_force_crash_ops;
module_param_cb(force_crash, &slsi_bt_force_crash_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(force_crash, "Forces a crash of the Bluetooth driver");

void slsi_bt_err_init(void (*callback)(int reason))
{
	atomic_set(&err_cnt, 0);
	handler = callback;
}

void slsi_bt_err_deinit(void)
{
	handler = NULL;
}
