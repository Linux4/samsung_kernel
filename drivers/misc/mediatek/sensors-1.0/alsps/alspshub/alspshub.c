// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[ALS/PS] " fmt

#include "alspshub.h"
#include <alsps.h>
#include <hwmsensor.h>
#include <SCP_sensorHub.h>
#include "SCP_power_monitor.h"
#include <linux/pm_wakeup.h>


#define ALSPSHUB_DEV_NAME     "alsps_hub_pl"

struct alspshub_ipi_data {
	struct work_struct init_done_work;
	atomic_t first_ready_after_boot;
	/*misc */
	atomic_t	als_suspend;
	atomic_t	ps_suspend;
	atomic_t	trace;
	atomic_t	scp_init_done;

	/*data */
	u16		als;
#ifdef CONFIG_HQ_PROJECT_O22
	/* hs14 code for SR-AL6528A-01-363 by xiongxiaoliang at 2022/09/05 start */
	u16		ps;
	/* hs14 code for SR-AL6528A-01-363 by xiongxiaoliang at 2022/09/05 end */
#else
	u8		ps;
#endif
	int		ps_cali;
	atomic_t	als_cali;
	atomic_t	ps_thd_val_high;
	atomic_t	ps_thd_val_low;
	/* hs03s code for SR-AL5625-01-143 by xiongxiaoliang at 2021/04/25 start */
	atomic_t	calibration_done;
	/* hs03s code for SR-AL5625-01-143 by xiongxiaoliang at 2021/04/25 end */
	ulong		enable;
	ulong		pending_intr;
	bool als_factory_enable;
	bool ps_factory_enable;
	bool als_android_enable;
	bool ps_android_enable;
	struct wakeup_source *ps_wake_lock;
};

static struct alspshub_ipi_data *obj_ipi_data;
static int ps_get_data(int *value, int *status);

static int alspshub_local_init(void);
static int alspshub_local_remove(void);
static int alspshub_init_flag = -1;
static struct alsps_init_info alspshub_init_info = {
	.name = "alsps_hub",
	.init = alspshub_local_init,
	.uninit = alspshub_local_remove,

};

static DEFINE_MUTEX(alspshub_mutex);
static DEFINE_SPINLOCK(calibration_lock);

enum {
	CMC_BIT_ALS = 1,
	CMC_BIT_PS = 2,
} CMC_BIT;
enum {
	CMC_TRC_ALS_DATA = 0x0001,
	CMC_TRC_PS_DATA = 0x0002,
	CMC_TRC_EINT = 0x0004,
	CMC_TRC_IOCTL = 0x0008,
	CMC_TRC_I2C = 0x0010,
	CMC_TRC_CVT_ALS = 0x0020,
	CMC_TRC_CVT_PS = 0x0040,
	CMC_TRC_DEBUG = 0x8000,
} CMC_TRC;
#ifdef CONFIG_HQ_PROJECT_O22
/* hs14 code for SR-AL6528A-01-363 by xiongxiaoliang at 2022/09/05 start */
long alspshub_read_ps(u16 *ps)
/* hs14 code for SR-AL6528A-01-363 by xiongxiaoliang at 2022/09/05 end */
#else
long alspshub_read_ps(u8 *ps)
#endif
{
	long res;
	struct alspshub_ipi_data *obj = obj_ipi_data;
	struct data_unit_t data_t;

	res = sensor_get_data_from_hub(ID_PROXIMITY, &data_t);
	if (res < 0) {
		*ps = -1;
		pr_err("sensor_get_data_from_hub fail, (ID: %d)\n",
			ID_PROXIMITY);
		return -1;
	}
	if (data_t.proximity_t.steps < obj->ps_cali)
		*ps = 0;
	else
		*ps = data_t.proximity_t.steps - obj->ps_cali;
	return 0;
}

long alspshub_read_als(u16 *als)
{
	long res = 0;
	struct data_unit_t data_t;

	res = sensor_get_data_from_hub(ID_LIGHT, &data_t);
	if (res < 0) {
		*als = -1;
		pr_err_ratelimited("sensor_get_data_from_hub fail, (ID: %d)\n",
			ID_LIGHT);
		return -1;
	}
	*als = data_t.light;

	return 0;
}

static ssize_t trace_show(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (!obj_ipi_data) {
		pr_err("obj_ipi_data is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t trace_store(struct device_driver *ddri,
				const char *buf, size_t count)
{
	int trace = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;
	int res = 0;
	int ret = 0;

	if (!obj) {
		pr_err("obj_ipi_data is null!!\n");
		return 0;
	}
	ret = sscanf(buf, "0x%x", &trace);
	if (ret != 1) {
		pr_err("invalid content: '%s', length = %zu\n", buf, count);
		return count;
	}
	atomic_set(&obj->trace, trace);
	res = sensor_set_cmd_to_hub(ID_PROXIMITY,
		CUST_ACTION_SET_TRACE, &trace);
	if (res < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_PROXIMITY, CUST_ACTION_SET_TRACE);
		return 0;
	}
	return count;
}

static ssize_t als_show(struct device_driver *ddri, char *buf)
{
	int res = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (!obj) {
		pr_err("obj_ipi_data is null!!\n");
		return 0;
	}
	res = alspshub_read_als(&obj->als);
	if (res)
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	else
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", obj->als);
}

static ssize_t ps_show(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (!obj) {
		pr_err("cm3623_obj is null!!\n");
		return 0;
	}
	res = alspshub_read_ps(&obj->ps);
	/* hs14 code for SR-AL6528A-01-363 by xiongxiaoliang at 2022/09/05 start */
	if (res)
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", (int)res);
	else
#ifdef CONFIG_HQ_PROJECT_O22
		return snprintf(buf, PAGE_SIZE, "%d\n", obj->ps);
#else
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", obj->ps);
#endif
	/* hs14 code for SR-AL6528A-01-363 by xiongxiaoliang at 2022/09/05 end */
}

static ssize_t reg_show(struct device_driver *ddri, char *buf)
{
	int res = 0;

	res = sensor_set_cmd_to_hub(ID_PROXIMITY, CUST_ACTION_SHOW_REG, buf);
	if (res < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_PROXIMITY, CUST_ACTION_SHOW_REG);
		return 0;
	}

	return res;
}

static ssize_t alslv_show(struct device_driver *ddri, char *buf)
{
	int res = 0;

	res = sensor_set_cmd_to_hub(ID_LIGHT, CUST_ACTION_SHOW_ALSLV, buf);
	if (res < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_LIGHT, CUST_ACTION_SHOW_ALSLV);
		return 0;
	}

	return res;
}

static ssize_t alsval_show(struct device_driver *ddri, char *buf)
{
	int res = 0;

	res = sensor_set_cmd_to_hub(ID_LIGHT, CUST_ACTION_SHOW_ALSVAL, buf);
	if (res < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_LIGHT, CUST_ACTION_SHOW_ALSVAL);
		return 0;
	}

	return res;
}

/* hs03s code for SR-AL5625-01-143 by xiongxiaoliang at 2021/04/25 start */
static int alshub_factory_enable_calibration(void);
static ssize_t test_cali_store(struct device_driver *ddri, const char *buf,
                   size_t tCount)
{
    int enable = 0, ret = 0;

    ret = kstrtoint(buf, 10, &enable);
    if (ret != 0) {
        pr_debug("kstrtoint fail\n");
        return 0;
    }
    if (enable == 1)
        alshub_factory_enable_calibration();
    return tCount;
}

static ssize_t cali_show(struct device_driver *ddri, char *buf)
{
    struct alspshub_ipi_data *obj = obj_ipi_data;

    return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->als_cali));
}

static ssize_t cali_status_show(struct device_driver *ddri, char *buf)
{
    struct alspshub_ipi_data *obj = obj_ipi_data;
    uint8_t status = 1;

    if (atomic_read(&obj->calibration_done)){
        status = 0;
    }

    atomic_set(&obj->calibration_done, 0);
    return snprintf(buf, PAGE_SIZE, "%d\n", status);
}
/* hs03s code for SR-AL5625-01-49 by xiongxiaoliang at 2021/05/06 start */
static ssize_t ps_rawdata_show(struct device_driver *ddri, char *buf)
{
    uint32_t ps_rawdata = 0;
    struct sensorInfo_t ps_devinfo;
    int err1 = -1, err2 = -1;

    memset(&ps_devinfo, 0, sizeof(ps_devinfo));
    err1 = sensor_set_cmd_to_hub(ID_PROXIMITY, CUST_ACTION_GET_RAW_DATA, &ps_rawdata);
    err2 = sensor_set_cmd_to_hub(ID_PROXIMITY, CUST_ACTION_GET_SENSOR_INFO, &ps_devinfo);

    return sprintf(buf, "%s:%u\n", ps_devinfo.name, ps_rawdata);
}

#ifdef CONFIG_HQ_PROJECT_O22
/*hs14 code for AL6528A-18 by xiongxiaoliang at 2022/09/06 start*/
static int ps_dynamic_set_threshold(int32_t thresh_high, int32_t thresh_low)
{
	int err = 0;
	int32_t cfg_data[2] = {0};

	if ((thresh_high < thresh_low) || (thresh_high <= 0) || (thresh_low <= 0)) {
		pr_err("PS set threshold fail! invalid value:[%d, %d]\n", thresh_high, thresh_low);
		return -1;
	}

	cfg_data[0] = thresh_high;
	cfg_data[1] = thresh_low;

	pr_err("%s cfg_data[0] = %ld, cfg_data[1] = %ld\n", __func__, cfg_data[0], cfg_data[1]);
	err = sensor_cfg_to_hub(ID_PROXIMITY, (uint8_t *)cfg_data, sizeof(cfg_data));
	if (err < 0) {
		pr_err("sensor_cfg_to_hub fail\n");
	}

	ps_cali_report(cfg_data);

	return err;
}

static int pshub_factory_get_raw_data(int32_t *data);
uint16_t g_pdata_no_cover = 0;
uint16_t g_pdata_2cm = 0;
uint16_t g_pdata_6cm = 0;
bool g_ps_cali_status = false;
#define SAVE_PDATA_0CM    0
#define SAVE_PDATA_2CM    1
#define SAVE_PDATA_6CM    2
/* hs14 code for AL6528A-35 by xiongxiaoliang at 2022/09/14 start */
#define INPUT_NUM   2
static ssize_t ps_mmi_cali_store(struct device_driver *ddri, const char *buf,
				size_t tCount)
{
	int enable = 0;
	int pdata_factory = 0;
	int ret = 0;

	int32_t pthresh_high = 0;
	int32_t pthresh_low = 0;
	g_ps_cali_status = false;

	if (sscanf(buf, "%d,%d", &enable, &pdata_factory) != INPUT_NUM) {
		pr_err("%s param error: ret = %d\n", __func__, ret);
		return tCount;
	}

	pr_err("%s enable = %d, pdata_factory = %d\n", __func__, enable, pdata_factory);

	if (enable == SAVE_PDATA_0CM) {
		g_pdata_no_cover = pdata_factory;
		g_ps_cali_status = true;
	} else if (enable == SAVE_PDATA_2CM) {
		g_pdata_2cm = pdata_factory;
		g_ps_cali_status = true;
	} else if (enable == SAVE_PDATA_6CM) {
		g_pdata_6cm = pdata_factory;
		g_ps_cali_status = true;
	} else {
		g_ps_cali_status = false;
		pr_err("%s please input enable 0/1/2 > ps_dynamic_cali\n", __func__);
	}

	if (enable == SAVE_PDATA_6CM) {
		pthresh_high = ((g_pdata_no_cover << 16) | g_pdata_2cm);
		pthresh_low = g_pdata_6cm;
		pr_err("%s [inf, 2cm, 6cm] = [%d, %d, %d]\n", __func__, g_pdata_no_cover, g_pdata_2cm, g_pdata_6cm);
		ret = ps_dynamic_set_threshold(pthresh_high, pthresh_low);
		if (ret < 0) {
			g_ps_cali_status = false;
		}
	}

	return tCount;
}

static ssize_t ps_cali_status_show(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", g_ps_cali_status);
}
/* hs14 code for AL6528A-35 by xiongxiaoliang at 2022/09/14 end */
#endif

static DRIVER_ATTR_RO(als);
static DRIVER_ATTR_RO(ps);
static DRIVER_ATTR_RO(alslv);
static DRIVER_ATTR_RO(alsval);
static DRIVER_ATTR_RW(trace);
static DRIVER_ATTR_RO(reg);
static DRIVER_ATTR_WO(test_cali);
static DRIVER_ATTR_RO(cali);
static DRIVER_ATTR_RO(cali_status);
static DRIVER_ATTR_RO(ps_rawdata);
#ifdef CONFIG_HQ_PROJECT_O22
static DRIVER_ATTR_WO(ps_mmi_cali);
static DRIVER_ATTR_RO(ps_cali_status);
#endif
static struct driver_attribute *alspshub_attr_list[] = {
	&driver_attr_als,
	&driver_attr_ps,
	&driver_attr_trace,	/*trace log */
	&driver_attr_alslv,
	&driver_attr_alsval,
	&driver_attr_reg,
	&driver_attr_test_cali,
	&driver_attr_cali,
	&driver_attr_cali_status,
	&driver_attr_ps_rawdata,
#ifdef CONFIG_HQ_PROJECT_O22
	&driver_attr_ps_mmi_cali,
	&driver_attr_ps_cali_status,
#endif

};
/* hs03s code for SR-AL5625-01-49 by xiongxiaoliang at 2021/05/06 end */
/* hs03s code for SR-AL5625-01-143 by xiongxiaoliang at 2021/04/25 end */
/*hs14 code for AL6528A-18 by xiongxiaoliang at 2022/09/06 end*/

static int alspshub_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(ARRAY_SIZE(alspshub_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, alspshub_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				alspshub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int alspshub_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(ARRAY_SIZE(alspshub_attr_list));

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, alspshub_attr_list[idx]);

	return err;
}

static void alspshub_init_done_work(struct work_struct *work)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;
	int err = 0;
#ifndef MTK_OLD_FACTORY_CALIBRATION
	int32_t cfg_data[2] = {0};
#endif

	if (atomic_read(&obj->scp_init_done) == 0) {
		pr_err("wait for nvram to set calibration\n");
		return;
	}
	if (atomic_xchg(&obj->first_ready_after_boot, 1) == 0)
		return;
#ifdef MTK_OLD_FACTORY_CALIBRATION
	err = sensor_set_cmd_to_hub(ID_PROXIMITY,
		CUST_ACTION_SET_CALI, &obj->ps_cali);
	if (err < 0)
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_PROXIMITY, CUST_ACTION_SET_CALI);
#else
	spin_lock(&calibration_lock);
	cfg_data[0] = atomic_read(&obj->ps_thd_val_high);
	cfg_data[1] = atomic_read(&obj->ps_thd_val_low);
	spin_unlock(&calibration_lock);
	err = sensor_cfg_to_hub(ID_PROXIMITY,
		(uint8_t *)cfg_data, sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub ps fail\n");

	spin_lock(&calibration_lock);
	cfg_data[0] = atomic_read(&obj->als_cali);
	spin_unlock(&calibration_lock);
	err = sensor_cfg_to_hub(ID_LIGHT,
		(uint8_t *)cfg_data, sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub als fail\n");
#endif
}
static int ps_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (!obj)
		return 0;

	if (event->flush_action == FLUSH_ACTION)
		err = ps_flush_report();
	else if (event->flush_action == DATA_ACTION &&
			READ_ONCE(obj->ps_android_enable) == true) {
		__pm_wakeup_event(obj->ps_wake_lock, msecs_to_jiffies(100));
		err = ps_data_report_t(event->proximity_t.oneshot,
			SENSOR_STATUS_ACCURACY_HIGH,
			(int64_t)event->time_stamp);
	} else if (event->flush_action == CALI_ACTION) {
		spin_lock(&calibration_lock);
		atomic_set(&obj->ps_thd_val_high, event->data[0]);
		atomic_set(&obj->ps_thd_val_low, event->data[1]);
		spin_unlock(&calibration_lock);
		err = ps_cali_report(event->data);
	}
	return err;
}
static int als_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (!obj)
		return 0;

	if (event->flush_action == FLUSH_ACTION)
		err = als_flush_report();
	else if ((event->flush_action == DATA_ACTION) &&
			READ_ONCE(obj->als_android_enable) == true)
		err = als_data_report_t(event->light,
				SENSOR_STATUS_ACCURACY_MEDIUM,
				(int64_t)event->time_stamp);
	else if (event->flush_action == CALI_ACTION) {
		spin_lock(&calibration_lock);
		atomic_set(&obj->als_cali, event->data[0]);
		/* hs03s code for SR-AL5625-01-143 by xiongxiaoliang at 2021/04/25 start */
		atomic_set(&obj->calibration_done, 1);
		/* hs03s code for SR-AL5625-01-143 by xiongxiaoliang at 2021/04/25 end */
		spin_unlock(&calibration_lock);
		err = als_cali_report(event->data);
	}
	return err;
}

static int rgbw_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		err = rgbw_flush_report();
	else if (event->flush_action == DATA_ACTION)
		err = rgbw_data_report_t(event->data,
			(int64_t)event->time_stamp);
	return err;
}

static int alshub_factory_enable_sensor(bool enable_disable,
				int64_t sample_periods_ms)
{
	int err = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (enable_disable == true)
		WRITE_ONCE(obj->als_factory_enable, true);
	else
		WRITE_ONCE(obj->als_factory_enable, false);

	if (enable_disable == true) {
		err = sensor_set_delay_to_hub(ID_LIGHT, sample_periods_ms);
		if (err) {
			pr_err("sensor_set_delay_to_hub failed!\n");
			return -1;
		}
	}
	err = sensor_enable_to_hub(ID_LIGHT, enable_disable);
	if (err) {
		pr_err("sensor_enable_to_hub failed!\n");
		return -1;
	}
	mutex_lock(&alspshub_mutex);
	if (enable_disable)
		set_bit(CMC_BIT_ALS, &obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &obj->enable);
	mutex_unlock(&alspshub_mutex);
	return 0;
}
static int alshub_factory_get_data(int32_t *data)
{
	int err = 0;
	struct data_unit_t data_t;

	err = sensor_get_data_from_hub(ID_LIGHT, &data_t);
	if (err < 0)
		return -1;
	*data = data_t.light;
	return 0;
}
static int alshub_factory_get_raw_data(int32_t *data)
{
	return alshub_factory_get_data(data);
}
static int alshub_factory_enable_calibration(void)
{
	return sensor_calibration_to_hub(ID_LIGHT);
}
static int alshub_factory_clear_cali(void)
{
	return 0;
}
static int alshub_factory_set_cali(int32_t offset)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;
	int err = 0;
	int32_t cfg_data;

	cfg_data = offset;
	err = sensor_cfg_to_hub(ID_LIGHT,
		(uint8_t *)&cfg_data, sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub fail\n");
	atomic_set(&obj->als_cali, offset);
	als_cali_report(&cfg_data);

	return err;

}
static int alshub_factory_get_cali(int32_t *offset)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;

	*offset = atomic_read(&obj->als_cali);
	return 0;
}
static int pshub_factory_enable_sensor(bool enable_disable,
			int64_t sample_periods_ms)
{
	int err = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (enable_disable == true) {
		err = sensor_set_delay_to_hub(ID_PROXIMITY, sample_periods_ms);
		if (err) {
			pr_err("sensor_set_delay_to_hub failed!\n");
			return -1;
		}
	}
	err = sensor_enable_to_hub(ID_PROXIMITY, enable_disable);
	if (err) {
		pr_err("sensor_enable_to_hub failed!\n");
		return -1;
	}
	mutex_lock(&alspshub_mutex);
	if (enable_disable)
		set_bit(CMC_BIT_PS, &obj->enable);
	else
		clear_bit(CMC_BIT_PS, &obj->enable);
	mutex_unlock(&alspshub_mutex);
	return 0;
}
static int pshub_factory_get_data(int32_t *data)
{
	int err = 0, status = 0;

	err = ps_get_data(data, &status);
	if (err < 0)
		return -1;
	return 0;
}
static int pshub_factory_get_raw_data(int32_t *data)
{
	int err = 0;
	struct data_unit_t data_t;

	err = sensor_get_data_from_hub(ID_PROXIMITY, &data_t);
	if (err < 0)
		return -1;
	*data = data_t.proximity_t.steps;
	return 0;
}
static int pshub_factory_enable_calibration(void)
{
	return sensor_calibration_to_hub(ID_PROXIMITY);
}
static int pshub_factory_clear_cali(void)
{
#ifdef MTK_OLD_FACTORY_CALIBRATION
	int err = 0;
#endif
	struct alspshub_ipi_data *obj = obj_ipi_data;

	obj->ps_cali = 0;
#ifdef MTK_OLD_FACTORY_CALIBRATION
	err = sensor_set_cmd_to_hub(ID_PROXIMITY,
			CUST_ACTION_RESET_CALI, &obj->ps_cali);
	if (err < 0) {
		pr_err("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
			ID_PROXIMITY, CUST_ACTION_RESET_CALI);
		return -1;
	}
#endif
	return 0;
}
static int pshub_factory_set_cali(int32_t offset)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;

	obj->ps_cali = offset;
	return 0;
}
static int pshub_factory_get_cali(int32_t *offset)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;

	*offset = obj->ps_cali;
	return 0;
}
static int pshub_factory_set_threshold(int32_t threshold[2])
{
	int err = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;
#ifndef MTK_OLD_FACTORY_CALIBRATION
	int32_t cfg_data[2] = {0};
#endif
	if (threshold[0] < threshold[1] || threshold[0] <= 0 ||
		threshold[1] <= 0) {
		pr_err("PS set threshold fail! invalid value:[%d, %d]\n",
			threshold[0], threshold[1]);
		return -1;
	}

	spin_lock(&calibration_lock);
	atomic_set(&obj->ps_thd_val_high, (threshold[0] + obj->ps_cali));
	atomic_set(&obj->ps_thd_val_low, (threshold[1] + obj->ps_cali));
	spin_unlock(&calibration_lock);
#ifdef MTK_OLD_FACTORY_CALIBRATION
	err = sensor_set_cmd_to_hub(ID_PROXIMITY,
		CUST_ACTION_SET_PS_THRESHOLD, threshold);
	if (err < 0)
		pr_err("sensor_set_cmd_to_hub fail, (ID:%d),(action:%d)\n",
			ID_PROXIMITY, CUST_ACTION_SET_PS_THRESHOLD);
#else
	spin_lock(&calibration_lock);
	cfg_data[0] = atomic_read(&obj->ps_thd_val_high);
	cfg_data[1] = atomic_read(&obj->ps_thd_val_low);
	spin_unlock(&calibration_lock);
	err = sensor_cfg_to_hub(ID_PROXIMITY,
		(uint8_t *)cfg_data, sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub fail\n");

	ps_cali_report(cfg_data);
#endif
	return err;
}

static int pshub_factory_get_threshold(int32_t threshold[2])
{
	struct alspshub_ipi_data *obj = obj_ipi_data;

	spin_lock(&calibration_lock);
	threshold[0] = atomic_read(&obj->ps_thd_val_high) - obj->ps_cali;
	threshold[1] = atomic_read(&obj->ps_thd_val_low) - obj->ps_cali;
	spin_unlock(&calibration_lock);
	return 0;
}

static struct alsps_factory_fops alspshub_factory_fops = {
	.als_enable_sensor = alshub_factory_enable_sensor,
	.als_get_data = alshub_factory_get_data,
	.als_get_raw_data = alshub_factory_get_raw_data,
	.als_enable_calibration = alshub_factory_enable_calibration,
	.als_clear_cali = alshub_factory_clear_cali,
	.als_set_cali = alshub_factory_set_cali,
	.als_get_cali = alshub_factory_get_cali,

	.ps_enable_sensor = pshub_factory_enable_sensor,
	.ps_get_data = pshub_factory_get_data,
	.ps_get_raw_data = pshub_factory_get_raw_data,
	.ps_enable_calibration = pshub_factory_enable_calibration,
	.ps_clear_cali = pshub_factory_clear_cali,
	.ps_set_cali = pshub_factory_set_cali,
	.ps_get_cali = pshub_factory_get_cali,
	.ps_set_threshold = pshub_factory_set_threshold,
	.ps_get_threshold = pshub_factory_get_threshold,
};

static struct alsps_factory_public alspshub_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &alspshub_factory_fops,
};
static int als_open_report_data(int open)
{
	return 0;
}


static int als_enable_nodata(int en)
{
	int res = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	pr_debug("obj_ipi_data als enable value = %d\n", en);

	if (en == true)
		WRITE_ONCE(obj->als_android_enable, true);
	else
		WRITE_ONCE(obj->als_android_enable, false);

	res = sensor_enable_to_hub(ID_LIGHT, en);
	if (res < 0) {
		pr_err("%s is failed!!\n", __func__);
		return -1;
	}

	mutex_lock(&alspshub_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &obj_ipi_data->enable);
	else
		clear_bit(CMC_BIT_ALS, &obj_ipi_data->enable);
	mutex_unlock(&alspshub_mutex);
	return 0;
}

static int als_set_delay(u64 ns)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	int err = 0;
	unsigned int delayms = 0;

	delayms = (unsigned int)ns / 1000 / 1000;
	err = sensor_set_delay_to_hub(ID_LIGHT, delayms);
	if (err) {
		pr_err("%s fail!\n", __func__);
		return err;
	}
	pr_debug("%s (%d)\n", __func__, delayms);
	return 0;
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int als_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	als_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_LIGHT, flag,
		samplingPeriodNs, maxBatchReportLatencyNs);
}

static int als_flush(void)
{
	return sensor_flush_to_hub(ID_LIGHT);
}

#ifdef CONFIG_HQ_PROJECT_HS03S
/* hs03s code for DEVAL5625-928 by xiongxiaoliang at 2021/06/02 start */
static int als_set_cali(uint8_t *data, uint8_t count)
{
    int32_t *buf = (int32_t *)data;
    struct alspshub_ipi_data *obj = obj_ipi_data;

    char *command_line = saved_command_line;
    pr_info("command_line = %s\n", command_line);

    spin_lock(&calibration_lock);
    atomic_set(&obj->als_cali, buf[0]);
    if(atomic_read(&obj->als_cali) == 0){
        atomic_set(&obj->als_cali, 1000);
    }
    pr_info("%d\n", atomic_read(&obj->als_cali));
    spin_unlock(&calibration_lock);

    if (NULL != strstr(command_line, "hx83112a_hdplus1600_dsi_vdo_ls_boe_9mask_55nm")){
        pr_info("lcd_first");
        data[2] = 1;
    }
    else if (NULL != strstr(command_line, "nl9911c_hdplus1600_dsi_vdo_truly_truly")){
        pr_info("lcd_second");
        data[2] = 2;
    }
    else if (NULL != strstr(command_line, "hx83102d_hdplus1600_dsi_vdo_jz_inx")){
        pr_info("lcd_third");
        data[2] = 3;
    }
    else if (NULL != strstr(command_line, "ili7806s_hdplus1600_dsi_vdo_txd_boe_9mask")){
        pr_info("lcd_forth");
        data[2] = 4;
    }
    else if (NULL != strstr(command_line, "jd9365t_hdplus1600_dsi_vdo_hlt_boe_6mask")){
        pr_info("lcd_fifth");
        data[2] = 5;
    }
    else if (NULL != strstr(command_line, "jd9365t_hdplus1600_dsi_vdo_hy_mdt")){
        pr_info("lcd_sixth");
        data[2] = 6;
    }
    else{
        pr_info("can't find lcd!");
        data[2] = 0;
    }

    return sensor_cfg_to_hub(ID_LIGHT, data, 3);
}
/* hs03s code for DEVAL5625-928 by xiongxiaoliang at 2021/06/02 end */
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
/* hs03s code for DEVAL5625-928 by xiongxiaoliang at 2021/06/02 start */
/* hs04 code for DEAL6398A-1878 by duxinqi at 2022/10/20 start */
static int als_set_cali(uint8_t *data, uint8_t count)
{
    int32_t *buf = (int32_t *)data;
    struct alspshub_ipi_data *obj = obj_ipi_data;

    char *command_line = saved_command_line;
    pr_info("command_line = %s\n", command_line);

    spin_lock(&calibration_lock);
    atomic_set(&obj->als_cali, buf[0]);
    if(atomic_read(&obj->als_cali) == 0){
        atomic_set(&obj->als_cali, 1000);
    }
    pr_info("%d\n", atomic_read(&obj->als_cali));
    spin_unlock(&calibration_lock);

    if (NULL != strstr(command_line, "lcd_jd9365t_txd_ctc_mipi_hdp_video")){
        pr_info("Main supply lcd");
        data[2] = 1;
    }
    else if (NULL != strstr(command_line, "lcd_gc7202_ls_hsd_mipi_hdp_video")){
        pr_info("Two supply lcd");
        data[2] = 2;
    }
    else if (NULL != strstr(command_line, "lcd_jd9365t_txd_boe_mipi_hdp_video")){
        pr_info("Three supply lcd");
        data[2] = 3;
    }
    else if (NULL != strstr(command_line, "lcd_nl9911c_hlt_hkc_mipi_hdp_video")){
        pr_info("Four supply lcd");
        data[2] = 4;
    }
    else if (NULL != strstr(command_line, "lcd_nl9911c_hr_hr_mipi_hdp_video")){
        pr_info("Five supply lcd");
        data[2] = 5;
    }
    else{
        pr_info("can't find covers!");
        data[2] = 0;
    }

    return sensor_cfg_to_hub(ID_LIGHT, data, 3);
}
/* hs04 code for DEAL6398A-1878 by duxinqi at 2022/10/20 end */
/* hs03s code for DEVAL5625-928 by xiongxiaoliang at 2021/06/02 end */
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
/*TabA7 Lite code for OT8-3912|SR-AX3565-01-853|SR-AX3565-01-870 by Hujincan at 20210531 start*/
static int als_set_cali(uint8_t *data, uint8_t count)
{
    int32_t *buf = (int32_t *)data;
    struct alspshub_ipi_data *obj = obj_ipi_data;
    /*if 0<(data[2] & 0xDF)<32 lcd is first,second,third...*/
    /*if (data[2] & 0x20) == 1 lcd is white,else lcd is black*/
    char *command_line = saved_command_line;
    pr_info("command_line = %s\n", command_line);

    /* TabA7 Lite code for OT8-5323 by duxinqi at 20220218 start */
    if (NULL != strstr(command_line, "hx83102e_hlt_hsd_fhdplus2408")){
        pr_info("lcd_first");
        data[2] = 1;
    }
    else if (NULL != strstr(command_line, "nt36523_liansi_hsd_incell_vdo")){
        pr_info("lcd_second");
        data[2] = 2;
    }
    else if (NULL != strstr(command_line, "ili9881t_liansi_inx_incell_vdo")){
        pr_info("lcd_third");
        data[2] = 3;
    }
    else if (NULL != strstr(command_line, "nt36523_hlt_mdt_incell_vdo")){
        pr_info("lcd_fourth");
        data[2] = 4;
    }
    else if (NULL != strstr(command_line, "ft8201ab_dt_qunchuang_inx_vdo_fhdplus2408")){
        pr_info("lcd_fifth");
        data[2] = 5;
    }
    else if (NULL != strstr(command_line, "nt36523bh_qunchuang_inx_incell_vdo")){
        pr_info("lcd_sixth");
        data[2] = 6;
    }
    else if (NULL != strstr(command_line, "hx83102e_liansi_mdt_incell_vdo")){
        pr_info("lcd_seventh");
        data[2] = 7;
    }
    else if (NULL != strstr(command_line, "hx83102e_liansi_huarui_incell_vdo")){
        pr_info("lcd_eighth");
        data[2] = 8;
    }
    else if (NULL != strstr(command_line, "hx83102e_gx_hsd_incell_vdo")){
        pr_info("lcd_eleventh");
        data[2] = 11;
    }
    else if (NULL != strstr(command_line, "nt36523b_txd_mdt_incell_vdo")){
        pr_info("lcd_twelfth");
        data[2] = 12;
    }
    else if (NULL != strstr(command_line, "hx83102e_hy_mdt_incell_vdo")){
        pr_info("lcd_thirteenth");
        data[2] = 13;
    }
    else{
        pr_info("can't find lcd!");
        data[2] = 0;
    }
    /* TabA7 Lite code for OT8-5323 by duxinqi at 20220218 end */

    if (NULL != strstr(command_line, "swid:0x20")){
        pr_info("lcd_black");
        data[2] += 0;
    }
    else if (NULL != strstr(command_line, "swid:0x21")){
        pr_info("lcd_white");
        data[2] += 32;
    }
    else if (NULL != strstr(command_line, "swid:0x30")){
        pr_info("lcd_black");
        data[2] += 0;
    }
    else if (NULL != strstr(command_line, "swid:0x31")){
        pr_info("lcd_white");
        data[2] += 32;
    }
    spin_lock(&calibration_lock);
    atomic_set(&obj->als_cali, buf[0]);
    spin_unlock(&calibration_lock);
    return sensor_cfg_to_hub(ID_LIGHT, data, 3);
}
/*TabA7 Lite code for OT8-3912|SR-AX3565-01-853|SR-AX3565-01-870 by Hujincan at 20210531 end*/
#endif

#ifdef CONFIG_HQ_PROJECT_O22
/* hs03s code for DEVAL5625-928 by xiongxiaoliang at 2021/06/02 start */
/* hs14 code for SR-AL6528A-01-437|AL6528A-190 by houxin at 2022/09/28 start */
struct lcd_id_info lcd_info[] = {
    {LCD_FIRST, "hwid:0x13"},
    {LCD_SECOND, "hwid:0x23"},
    {LCD_THIRD, "hwid:0x33"},
    {LCD_FOURTH, "hwid:0x43"},
};
static void lcd_info_judge(uint8_t *data)
{
    int i = 0;
    char *command_line = saved_command_line;
    data[2] = LCD_NONE;

    pr_info("command_line = %s\n", command_line);
    for (i = 0; i < sizeof(lcd_info) / sizeof(lcd_info[0]); i++) {
        if (NULL != strstr(command_line, lcd_info[i].lcd_strdata)) {
            pr_info("find lcd : %d!\n", lcd_info[i].hwid);
            data[2] = lcd_info[i].hwid;
            break;
        }
    }

    if (data[2] == LCD_NONE) {
        pr_info("can't find lcd!\n");
    }
}

static int als_set_cali(uint8_t *data, uint8_t count)
{
    int32_t *buf = (int32_t *)data;
    struct alspshub_ipi_data *obj = obj_ipi_data;

    spin_lock(&calibration_lock);
    atomic_set(&obj->als_cali, buf[0]);
    if(atomic_read(&obj->als_cali) == 0){
        atomic_set(&obj->als_cali, 1000);
    }
    lcd_info_judge(data);
    pr_info("%d\n", atomic_read(&obj->als_cali));
    spin_unlock(&calibration_lock);
    return sensor_cfg_to_hub(ID_LIGHT, data, 3);
}
/* hs14 code for SR-AL6528A-01-437|AL6528A-190 by houxin at 2022/09/28 end */
/* hs03s code for DEVAL5625-928 by xiongxiaoliang at 2021/06/02 end */
#endif

#ifdef CONFIG_HQ_PROJECT_A06
/* hs03s code for DEVAL5625-928 by xiongxiaoliang at 2021/06/02 start */
/* hs14 code for SR-AL6528A-01-437|AL6528A-190 by houxin at 2022/09/28 start */
struct lcd_id_info lcd_info[] = {
    {LCD_FIRST, "hwid:0x13"},
    {LCD_SECOND, "hwid:0x23"},
    {LCD_THIRD, "hwid:0x33"},
    {LCD_FOURTH, "hwid:0x43"},
};
static void lcd_info_judge(uint8_t *data)
{
    int i = 0;
    char *command_line = saved_command_line;
    data[2] = LCD_NONE;

    pr_info("command_line = %s\n", command_line);
    for (i = 0; i < sizeof(lcd_info) / sizeof(lcd_info[0]); i++) {
        if (NULL != strstr(command_line, lcd_info[i].lcd_strdata)) {
            pr_info("find lcd : %d!\n", lcd_info[i].hwid);
            data[2] = lcd_info[i].hwid;
            break;
        }
    }

    if (data[2] == LCD_NONE) {
        pr_info("can't find lcd!\n");
    }
}

static int als_set_cali(uint8_t *data, uint8_t count)
{
    int32_t *buf = (int32_t *)data;
    struct alspshub_ipi_data *obj = obj_ipi_data;

    spin_lock(&calibration_lock);
    atomic_set(&obj->als_cali, buf[0]);
    if(atomic_read(&obj->als_cali) == 0){
        atomic_set(&obj->als_cali, 1000);
    }
    lcd_info_judge(data);
    pr_info("%d\n", atomic_read(&obj->als_cali));
    spin_unlock(&calibration_lock);
    return sensor_cfg_to_hub(ID_LIGHT, data, 3);
}
/* hs14 code for SR-AL6528A-01-437|AL6528A-190 by houxin at 2022/09/28 end */
/* hs03s code for DEVAL5625-928 by xiongxiaoliang at 2021/06/02 end */
#endif

static int rgbw_enable(int en)
{
	int res = 0;

	res = sensor_enable_to_hub(ID_RGBW, en);
	if (res < 0) {
		pr_err("%s is failed!!\n", __func__);
		return -1;
	}
	return 0;
}

static int rgbw_batch(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_RGBW,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int rgbw_flush(void)
{
	return sensor_flush_to_hub(ID_RGBW);
}

static int als_get_data(int *value, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_LIGHT, &data);
	if (err) {
		pr_err("sensor_get_data_from_hub fail!\n");
	} else {
		time_stamp = data.time_stamp;
		*value = data.light;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	if (atomic_read(&obj_ipi_data->trace) & CMC_TRC_PS_DATA)
		pr_debug("value = %d\n", *value);
	return 0;
}

static int ps_open_report_data(int open)
{
	return 0;
}

static int ps_enable_nodata(int en)
{
	int res = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	pr_debug("obj_ipi_data als enable value = %d\n", en);
	if (en == true)
		WRITE_ONCE(obj->ps_android_enable, true);
	else
		WRITE_ONCE(obj->ps_android_enable, false);

	res = sensor_enable_to_hub(ID_PROXIMITY, en);
	if (res < 0) {
		pr_err("als_enable_nodata is failed!!\n");
		return -1;
	}

	mutex_lock(&alspshub_mutex);
	if (en)
		set_bit(CMC_BIT_PS, &obj_ipi_data->enable);
	else
		clear_bit(CMC_BIT_PS, &obj_ipi_data->enable);
	mutex_unlock(&alspshub_mutex);


	return 0;

}

static int ps_set_delay(u64 ns)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	int err = 0;
	unsigned int delayms = 0;

	delayms = (unsigned int)ns / 1000 / 1000;
	err = sensor_set_delay_to_hub(ID_PROXIMITY, delayms);
	if (err < 0) {
		pr_err("%s fail!\n", __func__);
		return err;
	}

	pr_debug("%s (%d)\n", __func__, delayms);
	return 0;
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int ps_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ps_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_PROXIMITY, flag,
		samplingPeriodNs, maxBatchReportLatencyNs);
}

static int ps_flush(void)
{
	return sensor_flush_to_hub(ID_PROXIMITY);
}

static int ps_get_data(int *value, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_PROXIMITY, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!\n");
		*value = -1;
		err = -1;
	} else {
		time_stamp = data.time_stamp;
		*value = data.proximity_t.oneshot;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	if (atomic_read(&obj_ipi_data->trace) & CMC_TRC_PS_DATA)
		pr_debug("value = %d\n", *value);

	return err;
}

static int ps_set_cali(uint8_t *data, uint8_t count)
{
	int32_t *buf = (int32_t *)data;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	spin_lock(&calibration_lock);
	atomic_set(&obj->ps_thd_val_high, buf[0]);
	atomic_set(&obj->ps_thd_val_low, buf[1]);
	spin_unlock(&calibration_lock);
	return sensor_cfg_to_hub(ID_PROXIMITY, data, count);
}

static int scp_ready_event(uint8_t event, void *ptr)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;

	switch (event) {
	case SENSOR_POWER_UP:
	    atomic_set(&obj->scp_init_done, 1);
		schedule_work(&obj->init_done_work);
		break;
	case SENSOR_POWER_DOWN:
	    atomic_set(&obj->scp_init_done, 0);
		break;
	}
	return 0;
}

static struct scp_power_monitor scp_ready_notifier = {
	.name = "alsps",
	.notifier_call = scp_ready_event,
};

static int alspshub_probe(struct platform_device *pdev)
{
	struct alspshub_ipi_data *obj;
	struct platform_driver *paddr =
			alspshub_init_info.platform_diver_addr;

	int err = 0;
	struct als_control_path als_ctl = { 0 };
	struct als_data_path als_data = { 0 };
	struct ps_control_path ps_ctl = { 0 };
	struct ps_data_path ps_data = { 0 };

	pr_debug("%s\n", __func__);
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(*obj));
	obj_ipi_data = obj;

	INIT_WORK(&obj->init_done_work, alspshub_init_done_work);

	platform_set_drvdata(pdev, obj);


	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->scp_init_done, 0);
	atomic_set(&obj->first_ready_after_boot, 0);

	obj->enable = 0;
	obj->pending_intr = 0;
	obj->ps_cali = 0;
	atomic_set(&obj->ps_thd_val_low, 21);
	atomic_set(&obj->ps_thd_val_high, 28);
	WRITE_ONCE(obj->als_factory_enable, false);
	WRITE_ONCE(obj->als_android_enable, false);
	WRITE_ONCE(obj->ps_factory_enable, false);
	WRITE_ONCE(obj->ps_android_enable, false);

	clear_bit(CMC_BIT_ALS, &obj->enable);
	clear_bit(CMC_BIT_PS, &obj->enable);
	scp_power_monitor_register(&scp_ready_notifier);
	err = scp_sensorHub_data_registration(ID_PROXIMITY, ps_recv_data);
	if (err < 0) {
		pr_err("scp_sensorHub_data_registration failed\n");
		goto exit_kfree;
	}
	err = scp_sensorHub_data_registration(ID_LIGHT, als_recv_data);
	if (err < 0) {
		pr_err("scp_sensorHub_data_registration failed\n");
		goto exit_kfree;
	}
	err = scp_sensorHub_data_registration(ID_RGBW, rgbw_recv_data);
	if (err < 0) {
		pr_err("scp_sensorHub_data_registration failed\n");
		goto exit_kfree;
	}
	err = alsps_factory_device_register(&alspshub_factory_device);
	if (err) {
		pr_err("alsps_factory_device_register register failed\n");
		goto exit_kfree;
	}
	pr_debug("alspshub_misc_device misc_register OK!\n");
	als_ctl.is_use_common_factory = false;
	ps_ctl.is_use_common_factory = false;
	err = alspshub_create_attr(&paddr->driver);
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay = als_set_delay;
	als_ctl.batch = als_batch;
	als_ctl.flush = als_flush;
	als_ctl.set_cali = als_set_cali;
	als_ctl.rgbw_enable = rgbw_enable;
	als_ctl.rgbw_batch = rgbw_batch;
	als_ctl.rgbw_flush = rgbw_flush;
	als_ctl.is_report_input_direct = false;

	als_ctl.is_support_batch = false;

	err = als_register_control_path(&als_ctl);
	if (err) {
		pr_err("register fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	err = als_register_data_path(&als_data);
	if (err) {
		pr_err("tregister fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.batch = ps_batch;
	ps_ctl.flush = ps_flush;
	ps_ctl.set_cali = ps_set_cali;
	ps_ctl.is_report_input_direct = false;

	ps_ctl.is_support_batch = false;

	err = ps_register_control_path(&ps_ctl);
	if (err) {
		pr_err("register fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	if (err) {
		pr_err("tregister fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	obj->ps_wake_lock = wakeup_source_register(NULL, "ps_wake_lock");
	if (!obj->ps_wake_lock) {
		pr_err("wakeup source init fail\n");
		err = -ENOMEM;
		goto exit_create_attr_failed;
	}

	alspshub_init_flag = 0;
	pr_debug("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
	alspshub_delete_attr(&(alspshub_init_info.platform_diver_addr->driver));
exit_kfree:
	kfree(obj);
	obj_ipi_data = NULL;
exit:
	pr_err("%s: err = %d\n", __func__, err);
	alspshub_init_flag = -1;
	return err;
}

static int alspshub_remove(struct platform_device *pdev)
{
	int err = 0;
	struct platform_driver *paddr =
			alspshub_init_info.platform_diver_addr;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (obj)
		wakeup_source_unregister(obj->ps_wake_lock);
	err = alspshub_delete_attr(&paddr->driver);
	if (err)
		pr_err("alspshub_delete_attr fail: %d\n", err);
	alsps_factory_device_deregister(&alspshub_factory_device);
	kfree(platform_get_drvdata(pdev));
	return 0;

}

static int alspshub_suspend(struct platform_device *pdev, pm_message_t msg)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int alspshub_resume(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}
static struct platform_device alspshub_device = {
	.name = ALSPSHUB_DEV_NAME,
	.id = -1,
};

static struct platform_driver alspshub_driver = {
	.probe = alspshub_probe,
	.remove = alspshub_remove,
	.suspend = alspshub_suspend,
	.resume = alspshub_resume,
	.driver = {
		.name = ALSPSHUB_DEV_NAME,
	},
};

static int alspshub_local_init(void)
{

	if (platform_driver_register(&alspshub_driver)) {
		pr_err("add driver error\n");
		return -1;
	}
	if (-1 == alspshub_init_flag)
		return -1;
	return 0;
}
static int alspshub_local_remove(void)
{

	platform_driver_unregister(&alspshub_driver);
	return 0;
}

static int __init alspshub_init(void)
{
	if (platform_device_register(&alspshub_device)) {
		pr_err("alsps platform device error\n");
		return -1;
	}
	alsps_driver_add(&alspshub_init_info);
	return 0;
}

static void __exit alspshub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(alspshub_init);
module_exit(alspshub_exit);
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
MODULE_DESCRIPTION("alspshub driver");
MODULE_LICENSE("GPL");
