// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"
#include "mtk_disp_notify.h"

#include <linux/string.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/sched/clock.h>

/*device tree mode*/
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/of_address.h>
#endif

#include <linux/interrupt.h>
#include <linux/ratelimit.h>
#include <linux/sched/clock.h>
#include <linux/timer.h>

struct uarthub_ops_struct *g_plat_ic_ops;
struct uarthub_core_ops_struct *g_plat_ic_core_ops;
struct uarthub_debug_ops_struct *g_plat_ic_debug_ops;
struct uarthub_ut_test_ops_struct *g_plat_ic_ut_test_ops;

static atomic_t g_uarthub_probe_called = ATOMIC_INIT(0);
struct platform_device *g_uarthub_pdev;
UARTHUB_CORE_IRQ_CB g_core_irq_callback;
int g_uarthub_disable;
static atomic_t g_uarthub_open = ATOMIC_INIT(0);
int g_is_ut_testing;
static struct notifier_block uarthub_fb_notifier;
struct workqueue_struct *uarthub_workqueue;
static int g_last_err_type = -1;
static struct timespec64 tv_now_assert, tv_end_assert;

int g_dev0_irq_sta;
int g_dev0_inband_irq_sta;

struct mutex g_lock_dump_log;
struct mutex g_clear_trx_req_lock;
struct mutex g_uarthub_reset_lock;

#define ISR_CLEAR_ALL_IRQ 1

static int mtk_uarthub_probe(struct platform_device *pdev);
static int mtk_uarthub_remove(struct platform_device *pdev);
static int mtk_uarthub_suspend(struct device *dev);
static int mtk_uarthub_resume(struct device *dev);
static int uarthub_core_init(void);
static void uarthub_core_exit(void);
static irqreturn_t uarthub_irq_isr(int irq, void *arg);
static void trigger_uarthub_error_worker_handler(struct work_struct *work);
static void debug_info_worker_handler(struct work_struct *work);
static void debug_clk_info_work_worker_handler(struct work_struct *work);
static int uarthub_fb_notifier_callback(struct notifier_block *nb, unsigned long value, void *v);

#if IS_ENABLED(CONFIG_OF)
struct uarthub_ops_struct __weak undef_plat_data = {};
struct uarthub_ops_struct __weak mt6985_plat_data = {};
struct uarthub_ops_struct __weak mt6989_plat_data = {};

const struct of_device_id apuarthub_of_ids[] = {
	{ .compatible = "mediatek,mt6835-uarthub", .data = &undef_plat_data },
	{ .compatible = "mediatek,mt6878-uarthub", .data = &undef_plat_data },
	{ .compatible = "mediatek,mt6886-uarthub", .data = &undef_plat_data },
	{ .compatible = "mediatek,mt6897-uarthub", .data = &undef_plat_data },
	{ .compatible = "mediatek,mt6983-uarthub", .data = &undef_plat_data },
	{ .compatible = "mediatek,mt6985-uarthub", .data = &mt6985_plat_data },
	{ .compatible = "mediatek,mt6989-uarthub", .data = &mt6989_plat_data },
	{}
};
#endif

static const struct dev_pm_ops uarthub_drv_pm_ops = {
	.suspend_noirq = mtk_uarthub_suspend,
	.resume_noirq = mtk_uarthub_resume,
};

struct platform_driver mtk_uarthub_dev_drv = {
	.probe = mtk_uarthub_probe,
	.remove = mtk_uarthub_remove,
	.driver = {
			.name = "mtk_uarthub",
			.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
			.of_match_table = apuarthub_of_ids,
#endif
			.pm = &uarthub_drv_pm_ops,
	}
};

struct assert_ctrl uarthub_assert_ctrl;
struct debug_info_ctrl uarthub_debug_info_ctrl;
struct debug_info_ctrl uarthub_debug_clk_info_ctrl;

static int mtk_uarthub_probe(struct platform_device *pdev)
{
	pr_info("[%s] DO UARTHUB PROBE\n", __func__);

	if (pdev)
		g_uarthub_pdev = pdev;

	g_plat_ic_ops = uarthub_core_get_platform_ic_ops(pdev);

	if (g_plat_ic_ops) {
		g_plat_ic_core_ops = g_plat_ic_ops->core_ops;
		g_plat_ic_debug_ops = g_plat_ic_ops->debug_ops;
		g_plat_ic_ut_test_ops = g_plat_ic_ops->ut_test_ops;
	}

	if (g_plat_ic_core_ops) {
		if (g_plat_ic_core_ops->uarthub_plat_init_remap_reg)
			g_plat_ic_core_ops->uarthub_plat_init_remap_reg();

		if (g_plat_ic_ut_test_ops && g_plat_ic_ut_test_ops->uarthub_plat_is_ut_testing) {
			g_is_ut_testing = g_plat_ic_ut_test_ops->uarthub_plat_is_ut_testing();
			if (g_is_ut_testing)
				pr_info("[%s] ut_testing is enabled.\n", __func__);
		}
	}

	atomic_set(&g_uarthub_probe_called, 1);
	return 0;
}

static int mtk_uarthub_remove(struct platform_device *pdev)
{
	pr_info("[%s] DO UARTHUB REMOVE\n", __func__);

	if (g_plat_ic_core_ops && g_plat_ic_core_ops->uarthub_plat_deinit_unmap_reg)
		g_plat_ic_core_ops->uarthub_plat_deinit_unmap_reg();

	if (g_uarthub_pdev)
		g_uarthub_pdev = NULL;

	atomic_set(&g_uarthub_probe_called, 0);
	return 0;
}

static int mtk_uarthub_resume(struct device *dev)
{
#if UARTHUB_DEBUG_LOG
	const char *prefix = "##########";
	const char *postfix = "##############";

	pr_info("[%s] %s uarthub enter resume %s\n",
		__func__, prefix, postfix);
#endif

#if UARTHUB_INFO_LOG
	uarthub_core_debug_clk_info("HUB_DBG_RESUME");
#endif

	return 0;
}

static int mtk_uarthub_suspend(struct device *dev)
{
#if UARTHUB_INFO_LOG
	const char *prefix = "##########";
	const char *postfix = "##############";

	pr_info("[%s] %s uarthub enter suspend %s\n",
		__func__, prefix, postfix);
#endif

#if UARTHUB_DEBUG_LOG
	uarthub_core_debug_clk_info("HUB_DBG_RESUME");
#endif

	return 0;
}

struct uarthub_ops_struct *uarthub_core_get_platform_ic_ops(struct platform_device *pdev)
{
	const struct of_device_id *of_id = NULL;

	if (!pdev) {
		pr_notice("[%s] pdev is NULL\n", __func__);
		return NULL;
	}

	of_id = of_match_node(apuarthub_of_ids, pdev->dev.of_node);
	if (!of_id || !of_id->data) {
		pr_notice("[%s] failed to look up compatible string\n", __func__);
		return NULL;
	}

	return (struct uarthub_ops_struct *)of_id->data;
}

static int uarthub_core_init(void)
{
	int ret = -1, retry = 0;

#if UARTHUB_INFO_LOG
	pr_info("[%s] g_uarthub_disable=[%d]\n", __func__, g_uarthub_disable);
#endif

	ret = platform_driver_register(&mtk_uarthub_dev_drv);
	if (ret)
		pr_notice("[%s] Uarthub driver registered failed(%d)\n", __func__, ret);
	else {
		while (atomic_read(&g_uarthub_probe_called) == 0 && retry < 100) {
			msleep(50);
			retry++;
			pr_info("[%s] g_uarthub_probe_called = 0, retry = %d\n", __func__, retry);
		}
	}

	if (!g_uarthub_pdev) {
		pr_notice("[%s] g_uarthub_pdev is NULL\n", __func__);
		goto ERROR;
	}

	uarthub_core_check_disable_from_dts(g_uarthub_pdev);

	if (g_uarthub_disable == 1)
		return 0;

	if (!g_plat_ic_core_ops) {
		pr_notice("[%s] g_plat_ic_core_ops is NULL\n", __func__);
		goto ERROR;
	}

	uarthub_workqueue = create_singlethread_workqueue("uarthub_wq");
	if (!uarthub_workqueue) {
		pr_notice("[%s] workqueue create failed\n", __func__);
		goto ERROR;
	}

	INIT_WORK(&uarthub_assert_ctrl.trigger_assert_work, trigger_uarthub_error_worker_handler);
	INIT_WORK(&uarthub_debug_info_ctrl.debug_info_work, debug_info_worker_handler);
	INIT_WORK(&uarthub_debug_clk_info_ctrl.debug_info_work, debug_clk_info_work_worker_handler);

	uarthub_fb_notifier.notifier_call = uarthub_fb_notifier_callback;
	ret = mtk_disp_notifier_register("uarthub_driver", &uarthub_fb_notifier);
	if (ret)
		pr_notice("uarthub register fb_notifier failed! ret(%d)\n", ret);
	else
		pr_info("uarthub register fb_notifier OK!\n");

	if (g_plat_ic_core_ops->uarthub_plat_uarthub_init) {
		if (g_plat_ic_core_ops->uarthub_plat_uarthub_init(g_uarthub_pdev) != 0)
			goto ERROR;
	}

	if (g_is_ut_testing == 1)
		uarthub_core_sync_uarthub_irq_sta(0);

	mutex_init(&g_lock_dump_log);
	mutex_init(&g_clear_trx_req_lock);
	mutex_init(&g_uarthub_reset_lock);

#if UARTHUB_DBG_SUPPORT
	uarthub_dbg_setup();
#endif

	return 0;

ERROR:
	g_uarthub_disable = 1;
	return -1;
}

static void uarthub_core_exit(void)
{
	if (g_uarthub_disable == 1)
		return;

	if (g_plat_ic_core_ops->uarthub_plat_uarthub_exit)
		g_plat_ic_core_ops->uarthub_plat_uarthub_exit();

#if UARTHUB_DBG_SUPPORT
	uarthub_dbg_remove();
#endif

	mtk_disp_notifier_unregister(&uarthub_fb_notifier);
	platform_driver_unregister(&mtk_uarthub_dev_drv);
}

static int uarthub_fb_notifier_callback(struct notifier_block *nb, unsigned long value, void *v)
{
	int data = 0;
	const char *prefix = "@@@@@@@@@@";
	const char *postfix = "@@@@@@@@@@@@@@";

	if (!v)
		return 0;

	data = *(int *)v;

	if (value == MTK_DISP_EVENT_BLANK) {
		if (data == MTK_DISP_BLANK_UNBLANK) {
			pr_info("[%s] %s uarthub enter UNBLANK %s\n",
				__func__, prefix, postfix);
#if UARTHUB_DEBUG_LOG
			uarthub_core_debug_info_with_tag_worker("UNBLANK_CB");
#endif
#if UARTHUB_INFO_LOG
		uarthub_core_debug_clk_info_worker("HUB_DBG_UNBLANK_CB");
#endif
		} else if (data == MTK_DISP_BLANK_POWERDOWN) {
#if UARTHUB_DEBUG_LOG
			uarthub_core_debug_info_with_tag_worker("POWERDOWN_CB");
#endif
#if UARTHUB_INFO_LOG
			uarthub_core_debug_clk_info_worker("HUB_DBG_PWRDWN_CB");
#endif
			pr_info("[%s] %s uarthub enter early POWERDOWN %s\n",
				__func__, prefix, postfix);
		} else {
			pr_info("[%s] %s data(%d) is not UNBLANK or POWERDOWN %s\n",
				__func__, prefix, data, postfix);
		}
	}

	return 0;
}

static irqreturn_t uarthub_irq_isr(int irq, void *arg)
{
	int err_type = -1;
	unsigned long err_ts;

	if (uarthub_core_is_apb_bus_clk_enable() == 0)
		return IRQ_HANDLED;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_irq_mask_ctrl == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_irq_clear_ctrl == NULL)
		return IRQ_HANDLED;

	/* mask dev0 irq */
	g_plat_ic_core_ops->uarthub_plat_irq_mask_ctrl(1);

	if (uarthub_core_handle_ut_test_irq() == 1)
		return IRQ_HANDLED;

	err_type = uarthub_core_check_irq_err_type();
	err_ts = sched_clock();
	if (err_type > 0) {
		uarthub_core_set_trigger_uarthub_error_worker(err_type, err_ts);
	} else {
		/* clear irq */
		g_plat_ic_core_ops->uarthub_plat_irq_clear_ctrl(BIT_0xFFFF_FFFF);
		/* unmask irq */
		g_plat_ic_core_ops->uarthub_plat_irq_mask_ctrl(0);
	}

	return IRQ_HANDLED;
}

int uarthub_core_irq_mask_ctrl(int mask)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_irq_mask_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	return g_plat_ic_core_ops->uarthub_plat_irq_mask_ctrl(mask);
}

int uarthub_core_irq_clear_ctrl(int err_type)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_irq_clear_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	return g_plat_ic_core_ops->uarthub_plat_irq_clear_ctrl(err_type);
}

int uarthub_core_check_irq_err_type(void)
{
	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_get_irq_err_type == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	return g_plat_ic_core_ops->uarthub_plat_get_irq_err_type();
}

int uarthub_core_irq_register(struct platform_device *pdev)
{
	struct device_node *node = NULL;
	int ret = 0;
	int irq_num = 0;
	int irq_flag = 0;
	struct timespec64 now;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		irq_num = irq_of_parse_and_map(node, 0);
		irq_flag = irq_get_trigger_type(irq_num);
#if UARTHUB_DEBUG_LOG
		pr_info("[%s] get irq id(%d) and irq trigger flag(%d) from DT\n",
			__func__, irq_num, irq_flag);
#endif

		ktime_get_real_ts64(&now);
		tv_end_assert.tv_sec = now.tv_sec;
		tv_end_assert.tv_nsec = now.tv_nsec;
		tv_end_assert.tv_sec += 1;

		ret = request_irq(irq_num, uarthub_irq_isr, irq_flag,
			"UARTHUB_IRQ", NULL);

		if (ret) {
			pr_notice("[%s] UARTHUB IRQ(%d) not available!!\n", __func__, irq_num);
			return -1;
		}

		uarthub_core_irq_mask_ctrl(0);
	} else {
		pr_notice("[%s] can't find CONSYS compatible node\n", __func__);
		return -1;
	}

	return 0;
}

int uarthub_core_irq_free(struct platform_device *pdev)
{
	struct device_node *node = NULL;
	int irq_num = 0;

	uarthub_core_irq_mask_ctrl(0);
	uarthub_core_irq_clear_ctrl(-1);

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		irq_num = irq_of_parse_and_map(node, 0);
		pr_info("[%s] get irq id(%d) from DT\n",
			__func__, irq_num);

		free_irq(irq_num, NULL);
	} else {
		pr_notice("[%s] can't find CONSYS compatible node\n", __func__);
		return -1;
	}

	return 0;
}

int uarthub_core_check_disable_from_dts(struct platform_device *pdev)
{
	int uarthub_disable = 0;
	struct device_node *node = NULL;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		if (of_property_read_u32(node, "uarthub_disable", &uarthub_disable)) {
			if (of_property_read_u32(node, "uarthub-disable", &uarthub_disable)) {
				pr_notice("[%s] unable to get uarthub_disable from dts\n",
					__func__);
				return -1;
			}
		}
		pr_info("[%s] Get uarthub_disable(%d)\n", __func__, uarthub_disable);
		g_uarthub_disable = ((uarthub_disable == 0) ? 0 : 1);
	} else {
		pr_notice("[%s] can't find UARTHUB compatible node\n", __func__);
		return -1;
	}

	return 0;
}

int uarthub_core_open(void)
{
	int ret = 0;

	if (g_uarthub_disable == 1) {
		pr_info("[%s] g_uarthub_disable=[1]\n", __func__);
		return 0;
	}

	if (g_plat_ic_core_ops->uarthub_plat_uarthub_open)
		g_plat_ic_core_ops->uarthub_plat_uarthub_open();

	if (atomic_read(&g_uarthub_open) == 1) {
		pr_info("[%s] g_uarthub_open=[1]\n", __func__);
		return 0;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	uarthub_core_irq_clear_ctrl(-1);
	ret = uarthub_core_irq_register(g_uarthub_pdev);
	if (ret)
		return -1;

	atomic_set(&g_uarthub_open, 1);

	return 0;
}

int uarthub_core_close(void)
{
	if (g_uarthub_disable == 1) {
		pr_info("[%s] g_uarthub_disable=[1]\n", __func__);
		return 0;
	}

	if (atomic_read(&g_uarthub_open) == 0) {
		pr_info("[%s] g_uarthub_open=[0]\n", __func__);
		return 0;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	uarthub_core_irq_free(g_uarthub_pdev);
	uarthub_core_irq_clear_ctrl(-1);

	uarthub_core_dev0_clear_txrx_request();

	if (g_plat_ic_core_ops->uarthub_plat_uarthub_close)
		g_plat_ic_core_ops->uarthub_plat_uarthub_close();

	atomic_set(&g_uarthub_open, 0);

	return 0;
}

int uarthub_core_dev0_is_uarthub_ready(const char *tag)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_is_ready_state == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_core_ops->uarthub_plat_is_ready_state();

	if (state == 1) {
		uarthub_core_crc_ctrl(1);
#if UARTHUB_DEBUG_LOG
		uarthub_core_debug_clk_info(tag);
#endif
#if UARTHUB_INFO_LOG
		uarthub_core_debug_byte_cnt_info(tag);
#endif
	}

	return (state == 1) ? 1 : 0;
}

int uarthub_core_get_host_wakeup_status(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_get_host_wakeup_status == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_core_ops->uarthub_plat_get_host_wakeup_status();

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_get_host_set_fw_own_status(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_get_host_set_fw_own_status == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_core_ops->uarthub_plat_get_host_set_fw_own_status();

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_dev0_is_txrx_idle(int rx)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_is_host_trx_idle == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_core_ops->uarthub_plat_is_host_trx_idle(
		0, (rx == 1) ? RX : TX);

#if UARTHUB_INFO_LOG
	pr_info("[%s] rx=[%d], state=[%d]\n", __func__, rx, state);
#endif

	return (state == 0) ? 1 : 0;
}

int uarthub_core_dev0_set_tx_request(void)
{
	int retry = 0;
	int val = 0;
#if UARTHUB_DEBUG_LOG
	uint32_t timer_h = 0, timer_l = 0;
#endif

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_set_host_trx_request == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	if (mutex_lock_killable(&g_uarthub_reset_lock)) {
		pr_notice("[%s] mutex_lock_killable g_uarthub_reset_lock fail\n", __func__);
		return UARTHUB_ERR_MUTEX_LOCK_FAIL;
	}

#if UARTHUB_INFO_LOG
	uarthub_core_debug_clk_info("HUB_DBG_SetTX_B");
#endif

	g_plat_ic_core_ops->uarthub_plat_set_host_trx_request(0, TX);
	mutex_unlock(&g_uarthub_reset_lock);

#if UARTHUB_DEBUG_LOG
	if (g_plat_ic_core_ops->uarthub_plat_get_spm_sys_timer) {
		g_plat_ic_core_ops->uarthub_plat_get_spm_sys_timer(&timer_h, &timer_l);
		pr_info("[%s] sys_timer=[%x][%x]\n", __func__, timer_h, timer_l);
	}
#endif

	/* only for SSPM not support case */
	if (g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl) {
		g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl(BIT_0xFFFF_FFFF);
		pr_info("[%s] is_ready=[%d]\n",
			__func__, uarthub_core_dev0_is_uarthub_ready("HUB_DBG_SetTX_E"));
#if UARTHUB_DEBUG_LOG
		uarthub_core_debug_info(__func__);
#endif
	}

	if (g_plat_ic_core_ops->uarthub_plat_get_hwccf_univpll_on_info) {
		retry = 20;
		while (retry-- > 0) {
			val = g_plat_ic_core_ops->uarthub_plat_get_hwccf_univpll_on_info();
			if (val == 1) {
#if UARTHUB_DEBUG_LOG
				pr_info("[%s] hw_ccf_pll on pass, retry=[%d]\n",
					__func__, retry);
#endif
				break;
			}
			usleep_range(1000, 1100);
		}

		if (val == 0) {
			pr_notice("[%s] hw_ccf_pll_on fail, retry=[%d]\n",
				__func__, retry);
		} else if (val < 0) {
			pr_notice("[%s] hw_ccf_pll_on info cannot be read, retry=[%d]\n",
				__func__, retry);
		}
	}

	return 0;
}

int uarthub_core_dev0_set_rx_request(void)
{
	int retry = 0;
	int val = 0;
#if UARTHUB_DEBUG_LOG
	uint32_t timer_h = 0, timer_l = 0;
#endif

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_set_host_trx_request == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	if (mutex_lock_killable(&g_uarthub_reset_lock)) {
		pr_notice("[%s] mutex_lock_killable g_uarthub_reset_lock fail\n", __func__);
		return UARTHUB_ERR_MUTEX_LOCK_FAIL;
	}

#if UARTHUB_DEBUG_LOG
	if (g_plat_ic_core_ops->uarthub_plat_get_spm_sys_timer) {
		g_plat_ic_core_ops->uarthub_plat_get_spm_sys_timer(&timer_h, &timer_l);
		pr_info("[%s] sys_timer=[%x][%x]\n", __func__, timer_h, timer_l);
	}
#endif

	g_plat_ic_core_ops->uarthub_plat_set_host_trx_request(0, RX);
	mutex_unlock(&g_uarthub_reset_lock);

	/* only for SSPM not support case */
	if (g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl) {
		g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl(BIT_0xFFFF_FFFF);
		pr_info("[%s] is_ready=[%d]\n",
			__func__, uarthub_core_dev0_is_uarthub_ready("HUB_DBG_SetRX_E"));
#if UARTHUB_DEBUG_LOG
		uarthub_core_debug_info(__func__);
#endif
	}

	if (g_plat_ic_core_ops->uarthub_plat_get_hwccf_univpll_on_info) {
		retry = 20;
		while (retry-- > 0) {
			val = g_plat_ic_core_ops->uarthub_plat_get_hwccf_univpll_on_info();
			if (val == 1) {
#if UARTHUB_DEBUG_LOG
				pr_info("[%s] hw_ccf_pll on pass, retry=[%d]\n",
					__func__, retry);
#endif
				break;
			}
			usleep_range(1000, 1100);
		}

		if (val == 0) {
			pr_notice("[%s] hw_ccf_pll_on fail, retry=[%d]\n",
				__func__, retry);
		} else if (val < 0) {
			pr_notice("[%s] hw_ccf_pll_on info cannot be read, retry=[%d]\n",
				__func__, retry);
		}
	}

	return 0;
}

int uarthub_core_dev0_set_txrx_request(void)
{
	int retry = 0;
	int val = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_set_host_trx_request == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	if (mutex_lock_killable(&g_uarthub_reset_lock)) {
		pr_notice("[%s] mutex_lock_killable g_uarthub_reset_lock fail\n", __func__);
		return UARTHUB_ERR_MUTEX_LOCK_FAIL;
	}

	g_plat_ic_core_ops->uarthub_plat_set_host_trx_request(0, TRX);
	mutex_unlock(&g_uarthub_reset_lock);

	/* only for SSPM not support case */
	if (g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl) {
		g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl(BIT_0xFFFF_FFFF);
		pr_info("[%s] is_ready=[%d]\n",
			__func__, uarthub_core_dev0_is_uarthub_ready("HUB_DBG_SetTRX_E"));
#if UARTHUB_DEBUG_LOG
		uarthub_core_debug_info(__func__);
#endif
	}

	if (g_plat_ic_core_ops->uarthub_plat_get_hwccf_univpll_on_info) {
		retry = 20;
		while (retry-- > 0) {
			val = g_plat_ic_core_ops->uarthub_plat_get_hwccf_univpll_on_info();
			if (val == 1) {
				pr_info("[%s] hw_ccf_pll on pass, retry=[%d]\n",
					__func__, retry);
				break;
			}
			usleep_range(1000, 1100);
		}

		if (val == 0) {
			pr_notice("[%s] hw_ccf_pll_on fail, retry=[%d]\n",
				__func__, retry);
		} else if (val < 0) {
			pr_notice("[%s] hw_ccf_pll_on info cannot be read, retry=[%d]\n",
				__func__, retry);
		}
	}

	return 0;
}

int uarthub_core_dev0_clear_tx_request(void)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_clear_host_trx_request == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	if (mutex_lock_killable(&g_uarthub_reset_lock)) {
		pr_notice("[%s] mutex_lock_killable g_uarthub_reset_lock fail\n", __func__);
		return UARTHUB_ERR_MUTEX_LOCK_FAIL;
	}

#if UARTHUB_DEBUG_LOG
	uarthub_core_debug_byte_cnt_info("HUB_DBG_ClrTX");
#endif

	g_plat_ic_core_ops->uarthub_plat_clear_host_trx_request(0, TX);
	mutex_unlock(&g_uarthub_reset_lock);

	/* only for SSPM not support case */
	if (g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl) {
		g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl(BIT_0xFFFF_FFFF);
		pr_info("[%s] is_ready=[%d]\n",
			__func__, uarthub_core_dev0_is_uarthub_ready("HUB_DBG_ClrTX_E"));
#if UARTHUB_DEBUG_LOG
		uarthub_core_debug_info(__func__);
#endif
	}

	return 0;
}

int uarthub_core_dev0_clear_rx_request(void)
{
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int need_lock = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_clear_host_trx_request == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_get_host_status == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	if (mutex_lock_killable(&g_uarthub_reset_lock)) {
		pr_notice("[%s] mutex_lock_killable g_uarthub_reset_lock fail\n", __func__);
		return UARTHUB_ERR_MUTEX_LOCK_FAIL;
	}

	dev0_sta = g_plat_ic_core_ops->uarthub_plat_get_host_status(0);
	dev1_sta = g_plat_ic_core_ops->uarthub_plat_get_host_status(1);
	dev2_sta = g_plat_ic_core_ops->uarthub_plat_get_host_status(2);
	if (dev0_sta == 0x301 && dev1_sta == 0x300 && dev2_sta == dev1_sta)
		need_lock = 1;

	if (need_lock == 1) {
		if (mutex_lock_killable(&g_clear_trx_req_lock)) {
			pr_notice("[%s] mutex_lock_killable g_clear_trx_req_lock fail\n", __func__);
			mutex_unlock(&g_uarthub_reset_lock);
			return UARTHUB_ERR_MUTEX_LOCK_FAIL;
		}
	}

#if UARTHUB_INFO_LOG
	uarthub_core_debug_byte_cnt_info("HUB_DBG_ClrRX");
#endif

	g_plat_ic_core_ops->uarthub_plat_clear_host_trx_request(0, RX);

	if (need_lock == 1)
		mutex_unlock(&g_clear_trx_req_lock);

	mutex_unlock(&g_uarthub_reset_lock);

	/* only for SSPM not support case */
	if (g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl) {
		g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl(BIT_0xFFFF_FFFF);
		pr_info("[%s] is_ready=[%d]\n",
			__func__, uarthub_core_dev0_is_uarthub_ready("HUB_DBG_ClrRX_E"));
#if UARTHUB_DEBUG_LOG
		uarthub_core_debug_info(__func__);
#endif
	}

	return 0;
}

int uarthub_core_dev0_clear_txrx_request(void)
{
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int need_lock = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_clear_host_trx_request == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_get_host_status == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	if (mutex_lock_killable(&g_uarthub_reset_lock)) {
		pr_notice("[%s] mutex_lock_killable g_uarthub_reset_lock fail\n", __func__);
		return -5;
	}

	dev0_sta = g_plat_ic_core_ops->uarthub_plat_get_host_status(0);
	dev1_sta = g_plat_ic_core_ops->uarthub_plat_get_host_status(1);
	dev2_sta = g_plat_ic_core_ops->uarthub_plat_get_host_status(2);
	if (dev1_sta == 0x300 && dev2_sta == dev1_sta)
		need_lock = 1;

	if (need_lock == 1) {
		if (mutex_lock_killable(&g_clear_trx_req_lock)) {
			pr_notice("[%s] mutex_lock_killable g_clear_trx_req_lock fail\n", __func__);
			mutex_unlock(&g_uarthub_reset_lock);
			return UARTHUB_ERR_MUTEX_LOCK_FAIL;
		}
	}

#if UARTHUB_INFO_LOG
	uarthub_core_debug_byte_cnt_info("HUB_DBG_ClrTRX");
#endif

	g_plat_ic_core_ops->uarthub_plat_clear_host_trx_request(0, TRX);

	if (need_lock == 1)
		mutex_unlock(&g_clear_trx_req_lock);

	mutex_unlock(&g_uarthub_reset_lock);

	/* only for SSPM not support case */
	if (g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl) {
		g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl(BIT_0xFFFF_FFFF);
		pr_info("[%s] is_ready=[%d]\n",
			__func__, uarthub_core_dev0_is_uarthub_ready("HUB_DBG_ClrTRX_E"));
#if UARTHUB_DEBUG_LOG
		uarthub_core_debug_info(__func__);
#endif
	}

	return 0;
}

int uarthub_core_get_uart_cmm_rx_count(void)
{
	int cmm_rx_cnt = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_get_cmm_byte_cnt == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		return UARTHUB_ERR_HUB_CLK_DISABLE;
	}

	cmm_rx_cnt = g_plat_ic_core_ops->uarthub_plat_get_cmm_byte_cnt(RX);
#if UARTHUB_DEBUG_LOG
	pr_info("[%s] cmm_rx_cnt=[%d]\n", __func__, cmm_rx_cnt);
#endif

	return cmm_rx_cnt;
}

int uarthub_core_is_assert_state(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
			g_plat_ic_core_ops->uarthub_plat_is_assert_state == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_core_ops->uarthub_plat_is_assert_state();

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_irq_register_cb(UARTHUB_CORE_IRQ_CB irq_callback)
{
	if (g_uarthub_disable == 1)
		return 0;

	g_core_irq_callback = irq_callback;
	return 0;
}

int uarthub_core_is_univpll_on(void)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_get_hwccf_univpll_on_info == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	return g_plat_ic_core_ops->uarthub_plat_get_hwccf_univpll_on_info();
}

int uarthub_core_crc_ctrl(int enable)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_config_crc_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif

	g_plat_ic_core_ops->uarthub_plat_config_crc_ctrl(enable);

	return 0;
}

int uarthub_core_bypass_mode_ctrl(int enable)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_config_bypass_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif

	if (enable == 1) {
		if (g_is_ut_testing == 0) {
			uarthub_core_irq_mask_ctrl(1);
			uarthub_core_irq_clear_ctrl(-1);
		}
		g_plat_ic_core_ops->uarthub_plat_config_bypass_ctrl(1);
	} else {
		g_plat_ic_core_ops->uarthub_plat_config_bypass_ctrl(0);
		if (g_is_ut_testing == 0) {
			uarthub_core_irq_clear_ctrl(-1);
			uarthub_core_irq_mask_ctrl(0);
		}
	}

	return 0;
}

int uarthub_core_md_adsp_fifo_ctrl(int enable)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_config_host_fifoe_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		return UARTHUB_ERR_HUB_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif

	if (enable == 1) {
		uarthub_core_reset_to_ap_enable_only(1);
	} else {
		g_plat_ic_core_ops->uarthub_plat_config_host_fifoe_ctrl(1, 1);
		g_plat_ic_core_ops->uarthub_plat_config_host_fifoe_ctrl(2, 1);
#if UARTHUB_INFO_LOG
		uarthub_core_debug_info(__func__);
#endif
	}

	return 0;
}

int uarthub_core_is_bypass_mode(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
			g_plat_ic_core_ops->uarthub_plat_is_bypass_mode == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_core_ops->uarthub_plat_is_bypass_mode();

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_rx_error_crc_info(int dev_index, int *p_crc_error_data, int *p_crc_result)
{
	int crc_error_data = 0, crc_result = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
			g_plat_ic_core_ops->uarthub_plat_get_rx_error_crc_info == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (p_crc_error_data == NULL && p_crc_result == NULL)
		return -1;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	g_plat_ic_core_ops->uarthub_plat_get_rx_error_crc_info(
		dev_index, &crc_error_data, &crc_result);

	if (p_crc_error_data)
		*p_crc_error_data = crc_error_data;

	if (p_crc_result)
		*p_crc_result = crc_result;

#if UARTHUB_DEBUG_LOG
		pr_info("[%s] dev_index=[%d], crc_error_data=[%d], crc_result=[%d]\n",
			__func__, dev_index, crc_error_data, crc_result);
#endif

	return 0;
}

int uarthub_core_timeout_info(int dev_index, int rx,
	int *p_timeout_counter, int *p_pkt_counter)
{
	int timeout_counter = 0, pkt_counter = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
			g_plat_ic_core_ops->uarthub_plat_get_trx_timeout_info == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	g_plat_ic_core_ops->uarthub_plat_get_trx_timeout_info(
		0, ((rx == 1) ? RX : TX), &timeout_counter, &pkt_counter);

	if (p_timeout_counter)
		*p_timeout_counter = timeout_counter;
	if (p_pkt_counter)
		*p_pkt_counter = pkt_counter;

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] dev_index=[%d], rx=[%d], timeout_counter=[%d], pkt_counter=[%d]\n",
		__func__, dev_index, rx, timeout_counter, pkt_counter);
#endif

	return 0;
}

int uarthub_core_config_internal_baud_rate(int dev_index, int rate_index)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_config_host_baud_rate == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
#if UARTHUB_DEBUG_LOG
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
#endif
		return UARTHUB_ERR_HUB_CLK_DISABLE;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] dev_index=[%d], rate_index=[%d]\n",
		__func__, dev_index, rate_index);
#endif

	return g_plat_ic_core_ops->uarthub_plat_config_host_baud_rate(
		dev_index, rate_index);
}

int uarthub_core_config_external_baud_rate(int rate_index)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_config_cmm_baud_rate == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
#if UARTHUB_DEBUG_LOG
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
#endif
		return UARTHUB_ERR_HUB_CLK_DISABLE;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] rate_index=[%d]\n", __func__, rate_index);
#endif

	return g_plat_ic_core_ops->uarthub_plat_config_cmm_baud_rate(
		rate_index);
}

void uarthub_core_set_trigger_uarthub_error_worker(int err_type, unsigned long err_ts)
{
	uarthub_assert_ctrl.err_type = err_type;
	uarthub_assert_ctrl.err_ts = err_ts;
	queue_work(uarthub_workqueue, &uarthub_assert_ctrl.trigger_assert_work);
}

static void trigger_uarthub_error_worker_handler(struct work_struct *work)
{
	struct assert_ctrl *queue = container_of(work, struct assert_ctrl, trigger_assert_work);
	int err_type = (int) queue->err_type;
	unsigned long err_ts = (unsigned long) queue->err_ts;
	unsigned long rem_nsec;
	int id = 0;
	int err_total = 0;
	int err_index = 0;
	struct timespec64 now;

	ktime_get_real_ts64(&now);
	tv_now_assert.tv_sec = now.tv_sec;
	tv_now_assert.tv_nsec = now.tv_nsec;

	if (g_last_err_type == err_type) {
		if ((((tv_now_assert.tv_sec == tv_end_assert.tv_sec) &&
				(tv_now_assert.tv_nsec > tv_end_assert.tv_nsec)) ||
				(tv_now_assert.tv_sec > tv_end_assert.tv_sec)) == false) {
#if ISR_CLEAR_ALL_IRQ
			uarthub_core_irq_clear_ctrl(-1);
#else
			uarthub_core_irq_clear_ctrl(err_type);
#endif
			uarthub_core_irq_mask_ctrl(0);
			tv_end_assert = tv_now_assert;
			tv_end_assert.tv_sec += 1;
			return;
		}
	} else
		g_last_err_type = err_type;

	tv_end_assert = tv_now_assert;
	tv_end_assert.tv_sec += 1;
	rem_nsec = do_div(err_ts, 1000000000);

	pr_info("[%s] err_type=[0x%x] err_time=[%5lu.%06lu]\n",
		__func__, err_type, err_ts, (rem_nsec/1000));
	err_total = 0;
	for (id = 0; id < irq_err_type_max; id++) {
		if (((err_type >> id) & 0x1) == 0x1)
			err_total++;
	}

	if (err_total > 0) {
		err_index = 0;
		for (id = 0; id < irq_err_type_max; id++) {
			if (((err_type >> id) & 0x1) == 0x1) {
				err_index++;
				if (g_core_irq_callback == NULL) {
					pr_info("[%s] %d-%d, err_id=[%d], reason=[%s], hub_irq_cb=[NULL]\n",
						__func__, err_total, err_index,
						id, UARTHUB_irq_err_type_str[id]);
				} else {
					pr_info("[%s] %d-%d, err_id=[%d], reason=[%s], hub_irq_cb=[%p]\n",
						__func__, err_total, err_index, id,
						UARTHUB_irq_err_type_str[id], g_core_irq_callback);
				}
			}
		}
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
#if ISR_CLEAR_ALL_IRQ
		uarthub_core_irq_clear_ctrl(-1);
#else
		uarthub_core_irq_clear_ctrl(err_type);
#endif
		uarthub_core_irq_mask_ctrl(0);
		return;
	}

	if (uarthub_core_is_bypass_mode() == 1) {
		pr_info("[%s] ignore irq error in bypass mode\n", __func__);
		uarthub_core_irq_mask_ctrl(1);
		uarthub_core_irq_clear_ctrl(-1);
		return;
	}

	if (uarthub_core_is_assert_state() == 1) {
		pr_info("[%s] ignore irq error if assert flow\n", __func__);
#if ISR_CLEAR_ALL_IRQ
		uarthub_core_irq_clear_ctrl(-1);
#else
		uarthub_core_irq_clear_ctrl(err_type);
#endif
		uarthub_core_irq_mask_ctrl(0);
		return;
	}

	uarthub_core_debug_info(__func__);

#if ISR_CLEAR_ALL_IRQ
	uarthub_core_irq_clear_ctrl(-1);
#else
	uarthub_core_irq_clear_ctrl(err_type);
#endif
	uarthub_core_irq_mask_ctrl(0);

	if (g_core_irq_callback)
		(*g_core_irq_callback)(err_type);
}

int uarthub_core_assert_state_ctrl(int assert_ctrl)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_assert_state_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	return g_plat_ic_core_ops->uarthub_plat_assert_state_ctrl(assert_ctrl);
}

int uarthub_core_reset_flow_control(void)
{
	int ret = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_reset_flow_control == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		return UARTHUB_ERR_HUB_CLK_DISABLE;
	}

	ret = g_plat_ic_core_ops->uarthub_plat_reset_flow_control();
	uarthub_core_crc_ctrl(1);
	return ret;
}

int uarthub_core_reset(void)
{
	int ret = 0;

	ret = uarthub_core_reset_to_ap_enable_only(0);

#if UARTHUB_INFO_LOG
	pr_info("[%s] ret=[%d]\n", __func__, ret);
#endif
	return ret;
}

int uarthub_core_reset_to_ap_enable_only(int ap_only)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_reset_to_ap_enable_only == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (mutex_lock_killable(&g_uarthub_reset_lock)) {
		pr_notice("[%s] mutex_lock_killable g_uarthub_reset_lock fail\n", __func__);
		return UARTHUB_ERR_MUTEX_LOCK_FAIL;
	}

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		mutex_unlock(&g_uarthub_reset_lock);
		return UARTHUB_ERR_HUB_CLK_DISABLE;
	}

	if (g_plat_ic_core_ops->uarthub_plat_reset_to_ap_enable_only(ap_only) != 0) {
		mutex_unlock(&g_uarthub_reset_lock);
		return -1;
	}

	mutex_unlock(&g_uarthub_reset_lock);

	return 0;
}

int uarthub_core_config_host_loopback(int dev_index, int tx_to_rx, int enable)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_set_host_loopback_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d], tx_to_rx=[%d], enable=[%d]\n",
		__func__, dev_index, tx_to_rx, enable);
#endif

	return g_plat_ic_core_ops->uarthub_plat_set_host_loopback_ctrl(
		dev_index, tx_to_rx, enable);
}

int uarthub_core_config_cmm_loopback(int tx_to_rx, int enable)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_set_cmm_loopback_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] tx_to_rx=[%d], enable=[%d]\n",
		__func__, tx_to_rx, enable);
#endif

	return g_plat_ic_core_ops->uarthub_plat_set_cmm_loopback_ctrl(tx_to_rx, enable);
}

int uarthub_core_is_apb_bus_clk_enable(void)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_is_ut_testing == 1)
		return 1;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_is_apb_bus_clk_enable == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	return g_plat_ic_core_ops->uarthub_plat_is_apb_bus_clk_enable();
}

int uarthub_core_is_uarthub_clk_enable(void)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_is_ut_testing == 1)
		return 1;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_is_uarthub_clk_enable == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	return g_plat_ic_core_ops->uarthub_plat_is_uarthub_clk_enable();
}

static void debug_info_worker_handler(struct work_struct *work)
{
	struct debug_info_ctrl *queue = container_of(work, struct debug_info_ctrl, debug_info_work);

	if (g_is_ut_testing == 1)
		mdelay(1);

	uarthub_core_debug_info(queue->tag);
}

static void debug_clk_info_work_worker_handler(struct work_struct *work)
{
	struct debug_info_ctrl *queue = container_of(work, struct debug_info_ctrl, debug_info_work);

	uarthub_core_debug_clk_info(queue->tag);
}

/*---------------------------------------------------------------------------*/

module_init(uarthub_core_init);
module_exit(uarthub_core_exit);

/*---------------------------------------------------------------------------*/

MODULE_AUTHOR("CTD/SE5/CS5/Johnny.Yao");
MODULE_DESCRIPTION("MTK UARTHUB Driver$1.01$");
MODULE_LICENSE("GPL");
