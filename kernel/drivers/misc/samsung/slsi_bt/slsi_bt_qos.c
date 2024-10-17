/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * S.LSI Bluetooth Service Control Driver                                     *
 *                                                                            *
 * This driver is tightly coupled with scsc maxwell driver and BT controller's*
 * data structure.                                                            *
 *                                                                            *
 ******************************************************************************/
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/delay.h>

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include <soc/samsung/exynos_pm_qos.h>

#include <scsc/scsc_mx.h>

#include "slsi_bt_log.h"
#include "slsi_bt_qos.h"

#define SLSI_BT_QOS_LOW_LEVEL                   (1)
#define SLSI_BT_QOS_MED_LEVEL                   (50)
#define SLSI_BT_QOS_HIGH_LEVEL                  (100)
#define SLSI_BT_QOS_DISABLE_TIMEOUT_MS          (500)

#define SLSI_BT_QOS_DISABLE                     SCSC_QOS_DISABLED

static uint32_t scsc_bt_qos_low_level = SLSI_BT_QOS_LOW_LEVEL;
static uint32_t scsc_bt_qos_medium_level = SLSI_BT_QOS_MED_LEVEL;
static uint32_t scsc_bt_qos_high_level = SLSI_BT_QOS_HIGH_LEVEL;
static uint32_t scsc_bt_qos_timeout = SLSI_BT_QOS_DISABLE_TIMEOUT_MS;

module_param(scsc_bt_qos_low_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_low_level,
	"Number of outstanding packets which triggers QoS low level");
module_param(scsc_bt_qos_medium_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_medium_level,
	"Number of outstanding packets which triggers QoS medium level");
module_param(scsc_bt_qos_high_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_high_level,
	"Number of outstanding packets which triggers QoS high level");
module_param(scsc_bt_qos_timeout, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_timeout,
	"Period of time (ms) which is used to disable QoS level ");

struct slsi_bt_qos {
	bool                            enabled;
	int                             lv;
	int                             expect;
	int                             tput;
	unsigned int                    rbytes;
	unsigned int                    last_msecs;

	struct work_struct              work;
	struct workqueue_struct	        *wq;
	wait_queue_head_t               update_req;
	struct mutex                    lock;
	atomic_t                        running;

	void                            *data;
};

static int get_scsc_qos_level(int tput)
{
	if (tput >= scsc_bt_qos_high_level && scsc_bt_qos_high_level > 0)
		return SCSC_QOS_MAX;
	else if (tput >= scsc_bt_qos_medium_level &&
		scsc_bt_qos_medium_level > 0)
		return SCSC_QOS_MED;
	else if (tput >= scsc_bt_qos_low_level && scsc_bt_qos_low_level > 0)
		return SCSC_QOS_MIN;
	return SCSC_QOS_DISABLED;
}

static int scsc_qos_update(int oqos, int nqos, void *data)
{
	struct scsc_service *service = (struct scsc_service *)data;

	/* To prevent frequent update, update only when the level is
	 * increased or disabled.
	 */
	if ((oqos < nqos) ||
	    (oqos != SLSI_BT_QOS_DISABLE && nqos == SLSI_BT_QOS_DISABLE)) {
		BT_DBG("set new qos from %d to %d\n", oqos, nqos);
		scsc_service_pm_qos_update_request(service, nqos);
		return nqos;
	}
	return oqos;
}

static void tput_algorithm_byte_per_ms(struct slsi_bt_qos *qos, int count)
{
	unsigned int now = jiffies_to_msecs(jiffies);
	unsigned int t_ms = now - qos->last_msecs;
	unsigned int Bps;
	const unsigned int t_base = 100;

	if (t_ms < t_base) {
		qos->rbytes += count;
		return;
	}

	qos->last_msecs = now;
	Bps = qos->rbytes * 1000 / t_ms;
	qos->tput = (Bps+9999) / 10000;

	//TR_DBG("tput - real: %u bytes/ %u ms\n", qos->rbytes, t_ms);
	TR_DBG("tput level:%u Bps:%d, bps:%d\n", qos->tput, Bps, Bps*8);

	qos->rbytes = 0;
}

static void qos_tput_update(struct slsi_bt_qos *qos, int count)
{
	if (count > 0) {
		tput_algorithm_byte_per_ms(qos, count);

		/* Don't disable qos by the algorithm */
		if (qos->lv != SCSC_QOS_DISABLED &&
		    qos->tput < scsc_bt_qos_low_level)
			qos->tput = scsc_bt_qos_low_level;
	} else {
		TR_DBG("reset tput values\n");
		qos->tput = 0;
	}
}

static void qos_work_func(struct work_struct *w)
{
	struct slsi_bt_qos *qos = container_of(w, struct slsi_bt_qos, work);
	unsigned int past;
	int ret = 1;

	mutex_lock(&qos->lock);
	TR_DBG("enter: expect qos level=%d\n", qos->expect);
	qos->lv = scsc_qos_update(qos->lv, qos->expect, qos->data);

	atomic_set(&qos->running, true);
	while(ret > 0) {
		past = qos->last_msecs;
		mutex_unlock(&qos->lock);
		ret = wait_event_timeout(qos->update_req,
					past != qos->last_msecs,
					msecs_to_jiffies(scsc_bt_qos_timeout));
		mutex_lock(&qos->lock);
		if (ret == 0) {
			qos_tput_update(qos, 0);
			qos->expect = SLSI_BT_QOS_DISABLE;
		}
		qos->lv = scsc_qos_update(qos->lv, qos->expect, qos->data);
	}
	atomic_set(&qos->running, false);
	mutex_unlock(&qos->lock);
	TR_DBG("end. qos level is %d\n", qos->lv);
}

struct slsi_bt_qos *slsi_bt_qos_start(struct scsc_service *service)
{
	struct slsi_bt_qos *qos;
	int err = 0;

	if (service == NULL)
		return NULL;

	qos = kzalloc(sizeof(struct slsi_bt_qos), GFP_KERNEL);
	if (qos == NULL) {
		BT_ERR("Fail to allocate qos\n");
		err = -ENOMEM;
		goto error;
	}
	qos->data = (void *)service;

	qos->wq = create_singlethread_workqueue("bt_qos_wq");
	if (qos->wq == NULL) {
		err = -ENOMEM;
		goto error;
	}

	err = scsc_service_pm_qos_add_request(service, SCSC_QOS_DISABLED);
	if (err)
		goto error;

	mutex_init(&qos->lock);
	atomic_set(&qos->running, false);
	INIT_WORK(&qos->work, qos_work_func);
	init_waitqueue_head(&qos->update_req);
	qos->enabled = true;

	BT_INFO("success. qos levels: [%d, %d, %d], timeout: %d ms\n",
		scsc_bt_qos_low_level, scsc_bt_qos_medium_level,
		scsc_bt_qos_high_level, scsc_bt_qos_timeout);
	return qos;

error:
	BT_WARNING("failed to start qos: err=%d\n", err);
	if (qos != NULL) {
		if (qos->wq)
			destroy_workqueue(qos->wq);
		kfree(qos);
	}
	return NULL;
}

void slsi_bt_qos_stop(struct slsi_bt_qos *qos)
{
	if (qos == NULL || !qos->enabled)
		return;

	if (atomic_read(&qos->running)) {
		qos->expect = SCSC_QOS_DISABLED;
		qos->last_msecs = 0;
		wake_up(&qos->update_req);
		udelay(1000);
	}

	scsc_service_pm_qos_remove_request((struct scsc_service *)qos->data);

	qos->enabled = false;
	BT_DBG("request to stop qos thread!\n");
	flush_workqueue(qos->wq);
	destroy_workqueue(qos->wq);
	cancel_work_sync(&qos->work);
	mutex_destroy(&qos->lock);
	kfree(qos);
	BT_INFO("QoS deinit done!\n");
}

void slsi_bt_qos_update(struct slsi_bt_qos *qos, int count)
{
	if (qos == NULL)
		return;

	mutex_lock(&qos->lock);
	qos_tput_update(qos, count);
	qos->expect = get_scsc_qos_level(qos->tput);
	if (!atomic_read(&qos->running))
		queue_work(qos->wq, &qos->work);
	else
		wake_up(&qos->update_req);
	mutex_unlock(&qos->lock);
}

static void slsi_bt_qos_level_reset(struct platform_device *pdev)
{
	if (of_property_read_u32(pdev->dev.of_node, "samsung,qos_level_low",
			&scsc_bt_qos_low_level))
		scsc_bt_qos_low_level = SLSI_BT_QOS_LOW_LEVEL;
	if (of_property_read_u32(pdev->dev.of_node, "samsung,qos_level_medium",
			&scsc_bt_qos_medium_level))
		scsc_bt_qos_medium_level = SLSI_BT_QOS_MED_LEVEL;
	if (of_property_read_u32(pdev->dev.of_node, "samsung,qos_level_high",
			&scsc_bt_qos_high_level))
		scsc_bt_qos_high_level = SLSI_BT_QOS_HIGH_LEVEL;
	if (of_property_read_u32(pdev->dev.of_node, "samsung,qos_timeout",
			&scsc_bt_qos_timeout))
		scsc_bt_qos_timeout = SLSI_BT_QOS_DISABLE_TIMEOUT_MS;

	TR_INFO("Reset QoS level: low(%d) med(%d) high(%d)\n",
		scsc_bt_qos_low_level, scsc_bt_qos_medium_level,
		scsc_bt_qos_high_level);
}

static struct platform_device *qos_pdev = NULL;
static int slsi_bt_qos_reset_set_param_cb(const char *buffer,
					    const struct kernel_param *kp)
{
	if (qos_pdev) {
		slsi_bt_qos_level_reset(qos_pdev);
		return 0;
	}
	return -ENXIO;
}

static struct kernel_param_ops slsi_bt_qos_reset_ops = {
	.set = slsi_bt_qos_reset_set_param_cb,
	.get = NULL,
};

module_param_cb(scsc_bt_qos_reset_levels, &slsi_bt_qos_reset_ops, NULL,
		S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_reset_levels,
		 "Reset the QoS level values to device default value");

static int slsi_bt_qos_platform_probe(struct platform_device *pdev)
{
	qos_pdev = pdev;
	slsi_bt_qos_level_reset(pdev);
	return 0;
}

static const struct of_device_id slsi_bt_qos[] = {
	{ .compatible = "samsung,scsc_bt_qos" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, slsi_bt_qos);

static struct platform_driver platform_bt_qos_driver = {
	.probe  = slsi_bt_qos_platform_probe,
	.driver = {
		.name = "SCSC_BT_QOS",
		.owner = THIS_MODULE,
		.pm = NULL,
		.of_match_table = of_match_ptr(slsi_bt_qos),
	},
};

void slsi_bt_qos_service_init(void)
{
	int ret = platform_driver_register(&platform_bt_qos_driver);

	if (ret)
		BT_WARNING("platform_driver_register is failed\n");
}

void slsi_bt_qos_service_exit(void)
{
	qos_pdev = NULL;
	platform_driver_unregister(&platform_bt_qos_driver);
}
