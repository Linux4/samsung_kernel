#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/printk.h>

/* #include <mach/eint.h> */

/* #include <cust_eint.h> */
/* #include <cust_gpio_usage.h> */
/* #include <mach/mt_gpio.h> */

#include "sw_tx_power.h"

/*#include <mach/mtk_ccci_helper.h> */
/* #include <mt-plat/mt_ccci_common.h> */

static struct task_struct *swtp_kthread;
static wait_queue_head_t swtp_isr_wait;
static int swtp_send_sig;

static struct timer_list swtp_timer;

static DEFINE_MUTEX(swtp_ctrl_lock);

#define SWTP_DEFAULT_MODE	MODE_SWTP(4G_TABLE0, 3G_TABLE0, 2G_TABLE0)

static swtp_state_type swtp_state_reg[SWTP_CTRL_MAX_STATE] = {
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_NONE, 2G_NONE)},	/* 0 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_NONE, 2G_TABLE0)},	/* 1 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_NONE, 2G_TABLE1)},	/* 2 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_NONE, 2G_TABLEX)},	/* 3 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLE0, 2G_NONE)},	/* 4 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLE0, 2G_TABLE0)},	/* 5 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLE0, 2G_TABLE1)},	/* 6 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLE0, 2G_TABLEX)},	/* 7 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLE1, 2G_NONE)},	/* 8 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLE1, 2G_TABLE0)},	/* 9 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLE1, 2G_TABLE1)},	/* 10 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLE1, 2G_TABLEX)},	/* 11 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLEX, 2G_NONE)},	/* 12 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLEX, 2G_TABLE0)},	/* 13 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLEX, 2G_TABLE1)},	/* 14 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_NONE, 3G_TABLEX, 2G_TABLEX)},	/* 15 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_NONE, 2G_NONE)},	/* 16 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_NONE, 2G_TABLE0)},	/* 17 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_NONE, 2G_TABLE1)},	/* 18 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_NONE, 2G_TABLEX)},	/* 19 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLE0, 2G_NONE)},	/* 20 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLE0, 2G_TABLE0)},	/* 21 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLE0, 2G_TABLE1)},	/* 22 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLE0, 2G_TABLEX)},	/* 23 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLE1, 2G_NONE)},	/* 24 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLE1, 2G_TABLE0)},	/* 25 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLE1, 2G_TABLE1)},	/* 26 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLE1, 2G_TABLEX)},	/* 27 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLEX, 2G_NONE)},	/* 28 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLEX, 2G_TABLE0)},	/* 29 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLEX, 2G_TABLE1)},	/* 30 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE0, 3G_TABLEX, 2G_TABLEX)},	/* 31 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_NONE, 2G_NONE)},	/* 32 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_NONE, 2G_TABLE0)},	/* 33 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_NONE, 2G_TABLE1)},	/* 34 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_NONE, 2G_TABLEX)},	/* 35 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLE0, 2G_NONE)},	/* 36 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLE0, 2G_TABLE0)},	/* 37 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLE0, 2G_TABLE1)},	/* 38 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLE0, 2G_TABLEX)},	/* 39 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLE1, 2G_NONE)},	/* 40 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLE1, 2G_TABLE0)},	/* 41 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLE1, 2G_TABLE1)},	/* 42 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLE1, 2G_TABLEX)},	/* 43 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLEX, 2G_NONE)},	/* 44 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLEX, 2G_TABLE0)},	/* 45 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLEX, 2G_TABLE1)},	/* 46 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLE1, 3G_TABLEX, 2G_TABLEX)},	/* 47 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_NONE, 2G_NONE)},	/* 48 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_NONE, 2G_TABLE0)},	/* 49 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_NONE, 2G_TABLE1)},	/* 50 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_NONE, 2G_TABLEX)},	/* 51 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLE0, 2G_NONE)},	/* 52 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLE0, 2G_TABLE0)},	/* 53 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLE0, 2G_TABLE1)},	/* 54 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLE0, 2G_TABLEX)},	/* 55 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLE1, 2G_NONE)},	/* 56 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLE1, 2G_TABLE0)},	/* 57 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLE1, 2G_TABLE1)},	/* 58 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLE1, 2G_TABLEX)},	/* 59 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLEX, 2G_NONE)},	/* 60 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLEX, 2G_TABLE0)},	/* 61 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLEX, 2G_TABLE1)},	/* 62 */
	{SWTP_MODE_OFF, SWTP_NORMAL_MODE, MODE_SWTP(4G_TABLEX, 3G_TABLEX, 2G_TABLEX)},	/* 63 */

	{SWTP_MODE_OFF, SWTP_SUPER_MODE, MODE_SWTP(4G_NONE, 3G_NONE, 2G_NONE)}	/* SUPER_SET */
};

static int swtp_set_tx_power(unsigned int mode)
{
	int ret1 = 0, ret2 = 0;

	if (get_modem_is_enabled(0))
		ret1 = switch_MD1_Tx_Power(mode);

	if (get_modem_is_enabled(1))
		ret2 = switch_MD2_Tx_Power(mode);

	return ((ret1 == 0) && (ret2 == 0));
}

int swtp_reset_tx_power(void)
{
	int ret1 = 0, ret2 = 0;

	if (get_modem_is_enabled(0))
		ret1 = switch_MD1_Tx_Power(SWTP_DEFAULT_MODE);

	if (get_modem_is_enabled(1))
		ret2 = switch_MD2_Tx_Power(SWTP_DEFAULT_MODE);

	return ((ret1 == 0) && (ret2 == 0));
}

int swtp_rfcable_tx_power(void)
{
	int ret1 = 0, ret2 = 0;

	if (get_modem_is_enabled(0))
		ret1 = switch_MD1_Tx_Power(swtp_state_reg[SWTP_CTRL_SUPER_SET].setvalue);

	if (get_modem_is_enabled(1))
		ret2 = switch_MD2_Tx_Power(swtp_state_reg[SWTP_CTRL_SUPER_SET].setvalue);

	return ((ret1 == 0) && (ret2 == 0));
}

int swtp_change_mode(unsigned int ctrid, unsigned int mode)
{
	if (ctrid >= SWTP_CTRL_MAX_STATE)
		return -1;

	swtp_state_reg[ctrid].mode = mode;

	return 0;
}

unsigned int swtp_get_mode(swtp_state_type *swtp_super_state, swtp_state_type *swtp_normal_state)
{
	unsigned int ctrid, run_mode = SWTP_CTRL_MAX_STATE;

	memset(swtp_super_state, 0, sizeof(swtp_state_type));
	memset(swtp_normal_state, 0, sizeof(swtp_state_type));

	mutex_lock(&swtp_ctrl_lock);
	for (ctrid = 0; ctrid < SWTP_CTRL_MAX_STATE; ctrid++) {
		if (swtp_state_reg[ctrid].enable == SWTP_MODE_ON) {
			if (swtp_state_reg[ctrid].mode == SWTP_SUPER_MODE) {
				memcpy(swtp_super_state, &swtp_state_reg[ctrid],
				       sizeof(swtp_state_type));
				run_mode = ctrid;
			} else if (swtp_state_reg[ctrid].mode == SWTP_NORMAL_MODE) {
				memcpy(swtp_normal_state, &swtp_state_reg[ctrid],
				       sizeof(swtp_state_type));
				if (run_mode >= SWTP_CTRL_MAX_STATE)
					run_mode = ctrid;
			}
		}
	}
	mutex_unlock(&swtp_ctrl_lock);

	return run_mode;
}

static int swtp_clear_mode(unsigned int mode)
{
	unsigned int ctrid;

	for (ctrid = 0; ctrid < SWTP_CTRL_MAX_STATE; ctrid++) {
		if (swtp_state_reg[ctrid].mode == mode)
			swtp_state_reg[ctrid].enable = SWTP_MODE_OFF;
	}

	return 0;
}

int swtp_set_mode(unsigned int ctrid, unsigned int enable)
{
	if (ctrid >= SWTP_CTRL_MAX_STATE)
		return -1;

	mutex_lock(&swtp_ctrl_lock);
	swtp_clear_mode(swtp_state_reg[ctrid].mode);
	swtp_state_reg[ctrid].enable = enable;
	mutex_unlock(&swtp_ctrl_lock);

	swtp_send_sig++;
	wake_up_interruptible(&swtp_isr_wait);

	return 0;
}

int swtp_reset_mode(void)
{
	mutex_lock(&swtp_ctrl_lock);
	swtp_clear_mode(SWTP_SUPER_MODE);
	swtp_clear_mode(SWTP_NORMAL_MODE);
	mutex_unlock(&swtp_ctrl_lock);

	swtp_send_sig++;
	wake_up_interruptible(&swtp_isr_wait);

	return 0;
}

int swtp_set_mode_unlocked(unsigned int ctrid, unsigned int enable)
{
	if (ctrid >= SWTP_CTRL_MAX_STATE)
		return -1;

	swtp_clear_mode(swtp_state_reg[ctrid].mode);
	swtp_state_reg[ctrid].enable = enable;

	swtp_send_sig++;
	wake_up(&swtp_isr_wait);

	return 0;
}

#define SWTP_MAX_TIMEOUT_SEC	50
#define SWTP_MAX_TIMER_CNT	8

static void swtp_mod_eint_read_timer(unsigned long data)
{
	SWTX_DBG("[SWTP] swtp_mod_eint_read_timer [cnt[%ld]\n", data);
	swtp_mod_eint_read();

	if (data) {
		swtp_timer.data = data - 1;	/* retry count */
		swtp_timer.expires = jiffies + 2 * HZ;
		add_timer(&swtp_timer);
	}
}

static int swtp_state_machine(void *handle)
{
	int run_mode, super_mode, normal_mode;
	int i = 0;
/* int timeout; */

	init_waitqueue_head(&swtp_isr_wait);

#if 0
	/* waiting modem working & set cable status value */
	while (i < SWTP_MAX_TIMEOUT_SEC) {
		timeout = wait_event_interruptible_timeout(swtp_isr_wait, swtp_send_sig, HZ);
		if (swtp_mod_eint_read())
			break;
		i++;
	}

	/* ccci channel is ready, but MD L1 part is not ready yet, need to re-send more. */
	swtp_timer.expires = jiffies + 2 * HZ;
	add_timer(&swtp_timer);
#endif

	swtp_mod_eint_enable(1);

	/* state machine */
	while (1) {
		wait_event_interruptible(swtp_isr_wait, swtp_send_sig || kthread_should_stop());

		if (kthread_should_stop())
			break;

		swtp_send_sig--;

		run_mode = super_mode = normal_mode = SWTP_CTRL_MAX_STATE;

		mutex_lock(&swtp_ctrl_lock);
		for (i = 0; i < SWTP_CTRL_MAX_STATE; i++) {
			if (swtp_state_reg[i].enable == SWTP_MODE_OFF)
				continue;

			if (swtp_state_reg[i].mode == SWTP_SUPER_MODE)
				super_mode = i;
			else if (swtp_state_reg[i].mode == SWTP_NORMAL_MODE)
				normal_mode = i;
			else
				SWTX_ERR("[SWTP] need to check 0x%x 0x%x\n", i,
					 swtp_state_reg[i].mode);
		}
		mutex_unlock(&swtp_ctrl_lock);

		if (super_mode < SWTP_CTRL_MAX_STATE)
			run_mode = super_mode;
		else if (normal_mode < SWTP_CTRL_MAX_STATE)
			run_mode = normal_mode;
		else if (run_mode == SWTP_CTRL_MAX_STATE) {
			swtp_reset_tx_power();
			SWTX_ERR("[SWTP] swtp_reset_tx_power : 0x%x\n", SWTP_DEFAULT_MODE);
			continue;
		}

		swtp_set_tx_power(swtp_state_reg[run_mode].setvalue);
		SWTX_INF("[SWTP] swtp_set_tx_power [%d]: 0x%x\n", run_mode,
			 swtp_state_reg[run_mode].setvalue);
/* LGE_API_test(); */
	}

	return 0;
}

void swtp_mode_restart(void)
{
	SWTX_INF("[SWTP] swtp_mode_restart.\n");

	kthread_stop(swtp_kthread);
	del_timer(&swtp_timer);

/* swtp_mod_eint_init(); */

	init_timer(&swtp_timer);
	swtp_timer.function = (void *)&swtp_mod_eint_read_timer;
	swtp_timer.data = SWTP_MAX_TIMER_CNT;	/* retry count  */

	swtp_kthread = kthread_run(swtp_state_machine, 0, "swtp kthread");
}

static int swtp_mode_update_handler(int md_id, int data)
{
	SWTX_DBG("[SWTP] swtp_mode_update_handler  md_id[%d], data[%d]\n", md_id, data);

	swtp_mod_eint_read();

	return 0;
}

#if 1
static int swtx_probe(struct platform_device *dev)
{
	int ret = 0;

	SWTX_INF("[SWTP] swtx_probe.\n");

	swtp_mod_eint_init(dev);

	if (get_modem_is_enabled(0)) {
		ret =
		    register_ccci_sys_call_back(0, MD_SW_MD1_TX_POWER_REQ,
						swtp_mode_update_handler);
		SWTX_DBG("[SWTP] register MD1 call back [%d]\n", ret);
	}

	if (get_modem_is_enabled(1)) {
		ret =
		    register_ccci_sys_call_back(1, MD_SW_MD2_TX_POWER_REQ,
						swtp_mode_update_handler);
		SWTX_DBG("[SWTP] register MD2call back [%d]\n", ret);
	}

	swtp_kthread = kthread_run(swtp_state_machine, 0, "swtp kthread");

	init_timer(&swtp_timer);
	swtp_timer.function = (void *)&swtp_mod_eint_read_timer;
	swtp_timer.data = SWTP_MAX_TIMER_CNT;	/* retry count */

	SWTX_INF("[SWTP] running swtp thread\n");
	SWTX_INF("[SWTP] swtx_probe done.\n");

	return 0;
}

static int swtx_remove(struct platform_device *dev)
{
	SWTX_DBG("[SWTP] swtx_remove.\n");

	swtp_mod_eint_enable(0);
	kthread_stop(swtp_kthread);
	del_timer(&swtp_timer);

	return 0;
}

struct of_device_id swtp_of_match[] = {
	{.compatible = "mediatek,mt6735-swtp",},
	{},
};

static struct platform_driver swtp_driver = {
	.probe = swtx_probe,
	/* .suspend = swtx_suspend, */
	/* .resume = swtx_resume, */
	.remove = swtx_remove,
	.driver = {
		   .name = "SWTP_Driver",
#ifdef CONFIG_OF
		   .of_match_table = swtp_of_match,
#endif
		   },
};

static int swtp_mod_init(void)
{
	int ret = 0;

	SWTX_ERR("[SWTP] swtx_mod_init begin!\n");
	ret = platform_driver_register(&swtp_driver);
	if (ret)
		SWTX_ERR("[SWTP] platform_driver_register error:(%d)\n", ret);
	else
		SWTX_ERR("[SWTP] platform_driver_register done!\n");

	SWTX_ERR("[SWTP] swtp_mod_init done!\n");
	return ret;
}

static void __exit swtp_mod_exit(void)
{
	SWTX_DBG("[SWTP] swtp_mod_exit.\n");
	platform_driver_unregister(&swtp_driver);
	SWTX_DBG("[SWTP] swtp_mod_exit done.\n");
}

#else
static int __init swtp_mod_init(void)
{
	int ret = 0;

	SWTX_DBG("[SWTP] swtp_mod_init.\n");

	swtp_mod_eint_init();

	if (get_modem_is_enabled(0)) {
		ret =
		    register_ccci_sys_call_back(0, MD_SW_MD1_TX_POWER_REQ,
						swtp_mode_update_handler);
		SWTX_DBG("[SWTP] register MD1 call back [%d]\n", ret);
	}

	if (get_modem_is_enabled(1)) {
		ret =
		    register_ccci_sys_call_back(1, MD_SW_MD2_TX_POWER_REQ,
						swtp_mode_update_handler);
		SWTX_DBG("[SWTP] register MD2call back [%d]\n", ret);
	}

	swtp_kthread = kthread_run(swtp_state_machine, 0, "swtp kthread");

	init_timer(&swtp_timer);
	swtp_timer.function = (void *)&swtp_mod_eint_read_timer;
	swtp_timer.data = SWTP_MAX_TIMER_CNT;	/* retry count */

	SWTX_DBG("[SWTP] running swtp thread\n");

	return 0;
}

static void __exit swtp_mod_exit(void)
{
	SWTX_DBG("[SWTP] swtp_mod_exit.\n");

	kthread_stop(swtp_kthread);
	del_timer(&swtp_timer);
}
#endif
module_init(swtp_mod_init);
module_exit(swtp_mod_exit);

MODULE_DESCRIPTION("SWTP Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTK");
