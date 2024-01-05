/*
 *
 * Zinitix zt touchscreen driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */
#include "zinitix_ts.h"

/* define i2c sub functions*/
static inline s32 read_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0, count_recv = 0;

	if (info->plat_data->power_enabled == false) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return -EIO;
	}

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

	mutex_lock(&info->i2c_mutex);

retry:
	/* select register*/
	ret = i2c_master_send(client, (u8 *)&reg, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: send failed %d, retry %d\n", __func__, ret, count);
		sec_delay(1);

		if (++count < SEC_TS_I2C_RETRY_CNT)
			goto retry;

		info->plat_data->hw_param.comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}
	/* for setup tx transaction. */
	usleep_range(DELAY_FOR_TRANSCATION, DELAY_FOR_TRANSCATION);
	ret = i2c_master_recv(client, values, length);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: recv failed %d, retry:%d\n",
				__func__, ret, count_recv);

		if (++count < SEC_TS_I2C_RETRY_CNT)
			goto retry;

		info->plat_data->hw_param.comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	if (info->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		int i;

		pr_info("[sec_input] zinitix: %s: R:", __func__);
		for (i = 0; i < length; i++)
			pr_cont(" %02X", values[i]);
		pr_cont("\n");
	}

	mutex_unlock(&info->i2c_mutex);
	return length;

I2C_ERROR:
	if (info->work_state == PROBE) {
		input_err(true, &client->dev,
				"%s work state is PROBE.\n", __func__);
		return ret;
	}
	if (info->work_state == ESD_TIMER) {
		input_err(true, &client->dev,
				"%s reset work queue be working.\n", __func__);
		return -EIO;
	}
	info->work_state = NOTHING;
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	queue_work(esd_tmr_workqueue, &info->tmr_work);
#endif
	return ret;
}

static inline s32 write_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;
	u8 *pkt = NULL;

	if (info->plat_data->power_enabled == false) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return -EIO;
	}

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

	mutex_lock(&info->i2c_mutex);

	pkt = kzalloc(ZINITIX_REG_ADDR_LENGTH + length, GFP_KERNEL);
	if (!pkt) {
		mutex_unlock(&info->i2c_mutex);
		return -ENOMEM;
	}

	pkt[0] = (reg) & 0xff; /* reg addr */
	pkt[1] = (reg >> 8)&0xff;
	memcpy((u8 *)&pkt[2], values, length);

retry:
	ret = i2c_master_send(client, pkt, length + ZINITIX_REG_ADDR_LENGTH);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed %d, retry %d\n", __func__, ret, count);
		sec_delay(1);

		if (++count < SEC_TS_I2C_RETRY_CNT)
			goto retry;

		info->plat_data->hw_param.comm_err_count++;
		kfree(pkt);
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	if (info->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		int i;

		pr_info("[sec_input] zinitix: %s: W:", __func__);
		for (i = 0; i < length + ZINITIX_REG_ADDR_LENGTH; i++)
			pr_cont(" %02X", pkt[i]);
		pr_cont("\n");
	}

	kfree(pkt);
	mutex_unlock(&info->i2c_mutex);
	return length;

I2C_ERROR:
	if (info->work_state == PROBE) {
		input_err(true, &client->dev,
				"%s work state is PROBE.\n", __func__);
		return ret;
	}
	if (info->work_state == ESD_TIMER) {
		input_err(true, &client->dev,
				"%s reset work queue be working.\n", __func__);
		return -EIO;
	}
	info->work_state = NOTHING;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	queue_work(esd_tmr_workqueue, &info->tmr_work);
#endif

	return ret;

}

static inline s32 write_reg(struct i2c_client *client, u16 reg, u16 value)
{
	if (write_data(client, reg, (u8 *)&value, 2) < 0) {
		input_err(true, &client->dev, "%s is failed: reg: %04X, value: %04X\n",
					__func__, reg, value);
		return I2C_FAIL;
	}

	return I2C_SUCCESS;
}

static inline s32 write_cmd(struct i2c_client *client, u16 reg)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;

	if (info->plat_data->power_enabled == false) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return -EIO;
	}

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

	mutex_lock(&info->i2c_mutex);

retry:
	ret = i2c_master_send(client, (u8 *)&reg, ZINITIX_REG_ADDR_LENGTH);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed %d, retry %d\n", __func__, ret, count);
		sec_delay(1);

		if (++count < SEC_TS_I2C_RETRY_CNT)
			goto retry;

		info->plat_data->hw_param.comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	if (info->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD)
		pr_info("[sec_input] zinitix: %s: W: %04X", __func__, reg);

	mutex_unlock(&info->i2c_mutex);
	return I2C_SUCCESS;

I2C_ERROR:
	if (info->work_state == PROBE) {
		input_err(true, &client->dev,
				"%s work state is PROBE.\n", __func__);
		return ret;
	}
	if (info->work_state == ESD_TIMER) {
		input_err(true, &client->dev,
				"%s reset work queue be working.\n", __func__);
		return -EIO;
	}
	info->work_state = NOTHING;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	queue_work(esd_tmr_workqueue, &info->tmr_work);
#endif

	return ret;

}

static inline s32 read_raw_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;

	if (info->plat_data->power_enabled == false) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return -EIO;
	}

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

	mutex_lock(&info->i2c_mutex);

retry:
	/* select register */
	ret = i2c_master_send(client, (u8 *)&reg, ZINITIX_REG_ADDR_LENGTH);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: send failed %d, retry %d\n", __func__, ret, count);
		sec_delay(1);

		if (++count < SEC_TS_I2C_RETRY_CNT)
			goto retry;

		info->plat_data->hw_param.comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}

	/* for setup tx transaction. */
	usleep_range(200, 200);

	ret = i2c_master_recv(client, values, length);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: recv failed %d\n", __func__, ret);
		info->plat_data->hw_param.comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		goto I2C_ERROR;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	mutex_unlock(&info->i2c_mutex);

	if (info->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		int i;

		pr_info("[sec_input] zinitix: %s: W: %04X R:", __func__, reg);
		for (i = 0; i < length; i++)
			pr_cont(" %02X", values[i]);
		pr_cont("\n");
	}

	return length;

I2C_ERROR:
	if (info->work_state == PROBE) {
		input_err(true, &client->dev,
				"%s work state is PROBE.\n", __func__);
		return ret;
	}
	if (info->work_state == ESD_TIMER) {
		input_err(true, &client->dev,
				"%s reset work be working.\n", __func__);
		return -EIO;
	}
	info->work_state = NOTHING;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	queue_work(esd_tmr_workqueue, &info->tmr_work);
#endif

	return ret;
}

static inline s32 read_firmware_data(struct i2c_client *client,
		u16 addr, u8 *values, u16 length)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	s32 ret;

	if (info->plat_data->power_enabled == false) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return -EIO;
	}

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

	mutex_lock(&info->i2c_mutex);

	/* select register*/
	ret = i2c_master_send(client, (u8 *)&addr, ZINITIX_REG_ADDR_LENGTH);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: send failed %d\n", __func__, ret);
		info->plat_data->hw_param.comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		return ret;
	}

	/* for setup tx transaction. */
	sec_delay(1);

	ret = i2c_master_recv(client, values, length);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: recv failed %d\n", __func__, ret);
		info->plat_data->hw_param.comm_err_count++;
		mutex_unlock(&info->i2c_mutex);
		return ret;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	if (info->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		int i;

		pr_info("[sec_input] zinitix: %s: W: %04X R:", __func__, addr);
		for (i = 0; i < length; i++)
			pr_cont(" %02X", values[i]);
		pr_cont("\n");
	}

	mutex_unlock(&info->i2c_mutex);
	return length;
}

static void zt_set_optional_mode(struct zt_ts_info *info, int event, bool enable)
{
	mutex_lock(&info->power_init);
	mutex_lock(&info->set_reg_lock);
	if (enable)
		zinitix_bit_set(info->m_optional_mode.select_mode.flag, event);
	else
		zinitix_bit_clr(info->m_optional_mode.select_mode.flag, event);

	if (write_reg(info->client, ZT_OPTIONAL_SETTING, info->m_optional_mode.optional_mode) != I2C_SUCCESS)
		input_info(true, &info->client->dev, "%s, fail optional mode set\n", __func__);

	mutex_unlock(&info->set_reg_lock);
	mutex_unlock(&info->power_init);
}

static int ts_read_from_sponge(struct zt_ts_info *info, u16 offset, u8 *value, int len)
{
	int ret = 0;
	u8 pkt[66];

	pkt[0] = offset & 0xFF;
	pkt[1] = (offset >> 8) & 0xFF;

	pkt[2] = len & 0xFF;
	pkt[3] = (len >> 8) & 0xFF;

	mutex_lock(&info->power_init);
	mutex_lock(&info->sponge_mutex);
	if (write_data(info->client, ZT_SPONGE_READ_REG, (u8 *)&pkt, 4) < 0) {
		input_err(true, &info->client->dev, "%s: fail to write sponge command\n", __func__);
		ret = -EIO;
	}

	if (read_data(info->client, ZT_SPONGE_READ_REG, value, len) < 0) {
		input_err(true, &info->client->dev, "%s: fail to read sponge command\n", __func__);
		ret = -EIO;
	}
	mutex_unlock(&info->sponge_mutex);
	mutex_unlock(&info->power_init);

	return ret;
}

static int ts_write_to_sponge(struct zt_ts_info *info, u16 offset, u8 *value, int len)
{
	int ret = 0;
	u8 pkt[66];

	mutex_lock(&info->power_init);
	mutex_lock(&info->sponge_mutex);

	pkt[0] = offset & 0xFF;
	pkt[1] = (offset >> 8) & 0xFF;

	pkt[2] = len & 0xFF;
	pkt[3] = (len >> 8) & 0xFF;

	memcpy((u8 *)&pkt[4], value, len);

	if (write_data(info->client, ZT_SPONGE_WRITE_REG, (u8 *)&pkt, len + 4) < 0) {
		input_err(true, &info->client->dev, "%s: Failed to write offset\n", __func__);
		ret = -EIO;
	}

	if (write_cmd(info->client, ZT_SPONGE_SYNC_REG) != I2C_SUCCESS) {
		input_err(true, &info->client->dev, "%s: Failed to send notify\n", __func__);
		ret = -EIO;
	}
	mutex_unlock(&info->sponge_mutex);
	mutex_unlock(&info->power_init);

	return ret;
}

static void ts_set_utc_sponge(struct zt_ts_info *info)
{
	struct timespec64 current_time;
	int ret;
	u8 data[4] = {0, 0};

	ktime_get_real_ts64(&current_time);
	data[0] = (0xFF & (u8)((current_time.tv_sec) >> 0));
	data[1] = (0xFF & (u8)((current_time.tv_sec) >> 8));
	data[2] = (0xFF & (u8)((current_time.tv_sec) >> 16));
	data[3] = (0xFF & (u8)((current_time.tv_sec) >> 24));
	input_info(true, &info->client->dev, "Write UTC to Sponge = %X\n", (int)(current_time.tv_sec));

	ret = ts_write_to_sponge(info, ZT_SPONGE_UTC, (u8 *)&data, 4);
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: Failed to write sponge\n", __func__);
}

int get_fod_info(struct zt_ts_info *info)
{
	u8 data[6] = {0, };
	int ret, i;

	ret = ts_read_from_sponge(info, ZT_SPONGE_AOD_ACTIVE_INFO, data, 6);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: Failed to read rect\n", __func__);
		return ret;
	}

	for (i = 0; i < 3; i++)
		info->plat_data->aod_data.active_area[i] = (data[i * 2 + 1] & 0xFF) << 8 | (data[i * 2] & 0xFF);

	input_info(true, &info->client->dev, "%s: top:%d, edge:%d, bottom:%d\n",
			__func__, info->plat_data->aod_data.active_area[0], info->plat_data->aod_data.active_area[1],
			info->plat_data->aod_data.active_area[2]);

	return ret;
}

int get_aod_active_area(struct zt_ts_info *info)
{
	int ret;

	/* get fod information */
	ret = ts_read_from_sponge(info, ZT_SPONGE_FOD_INFO, info->fod_info_vi_trx, 3);
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: fail fod channel info.\n", __func__);

	info->fod_info_vi_data_len = info->fod_info_vi_trx[2];

	input_info(true, &info->client->dev, "%s: fod info %d,%d,%d\n", __func__,
			info->fod_info_vi_trx[0], info->fod_info_vi_trx[1], info->fod_info_vi_data_len);

	return ret;
}

void ts_check_custom_library(struct zt_ts_info *info)
{
	u8 data[10] = { 0 };
	int ret;

	ret = read_data(info->client, ZT_SPONGE_READ_INFO, &data[0], 10);
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: fail to read status reg\n", __func__);

	input_info(true, &info->client->dev,
			"%s: (%d) %c%c%c%c, || %02X, %02X, %02X, %02X, || %02X, %02X\n",
			__func__, ret, data[0], data[1], data[2], data[3], data[4],
			data[5], data[6], data[7], data[8], data[9]);

	ret = get_aod_active_area(info);
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: fail to get aod area\n", __func__);

	ret = get_fod_info(info);
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: fail to read fod info\n", __func__);

	data[0] = 0;
	ret = read_data(info->client, ZT_GET_FOD_WITH_FINGER_PACKET, data, 1);
	if (ret < 0) {
		input_err(true, &info->client->dev,
				"%s: fail to read fod_with_finger_packet, set as false\n", __func__);
		info->fod_with_finger_packet = false;
	} else {
		input_info(true, &info->client->dev, "%s: fod_with_finger_packet: %d\n", __func__, data[0]);
		info->fod_with_finger_packet = !!data[0];
	}
}

static void zt_set_lp_mode(struct zt_ts_info *info, int event, bool enable)
{
	int ret;

	mutex_lock(&info->set_lpmode_lock);

	if (enable)
		zinitix_bit_set(info->lpm_mode, event);
	else
		zinitix_bit_clr(info->lpm_mode, event);

	ret = ts_write_to_sponge(info, ZT_SPONGE_LP_FEATURE, &info->lpm_mode, 1);
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: fail to write sponge\n", __func__);

	mutex_unlock(&info->set_lpmode_lock);
}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
static irqreturn_t zt_touch_work(int irq, void *data);
static void clear_report_data(struct zt_ts_info *info);
static irqreturn_t secure_filter_interrupt(struct zt_ts_info *info)
{
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		if (atomic_cmpxchg(&info->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&info->input_dev->dev.kobj, NULL, "secure_touch");
		} else {
			input_info(true, &info->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&info->secure_pending_irqs));
		}

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */
static ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&info->secure_enabled));
}

static ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);
	int ret;
	unsigned long data;

	if (count > 2) {
		input_err(true, &info->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &data);
	if (ret != 0) {
		input_err(true, &info->client->dev, "%s: failed to read:%d\n",
				__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		/* Enable Secure World */
		if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
			input_err(true, &info->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		sec_delay(200);
		/* synchronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		mutex_lock(&info->work_lock);
		disable_irq(info->client->irq);

		/* zinitix timer stop, release all finger */
#if ESD_TIMER_INTERVAL
		write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, 0);
		esd_timer_stop(info);
#endif
		clear_report_data(info);

		if (pm_runtime_get_sync(info->client->adapter->dev.parent) < 0) {
			enable_irq(info->client->irq);
#if ESD_TIMER_INTERVAL
			esd_timer_start(CHECK_ESD_TIMER, info);
			write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
			input_err(true, &info->client->dev, "%s: failed to get pm_runtime\n", __func__);
			mutex_unlock(&info->work_lock);
			return -EIO;
		}

		reinit_completion(&info->secure_powerdown);
		reinit_completion(&info->secure_interrupt);

		sec_delay(10);

		atomic_set(&info->secure_enabled, 1);
		atomic_set(&info->secure_pending_irqs, 0);

		enable_irq(info->client->irq);

		input_info(true, &info->client->dev, "%s: secure touch enable\n", __func__);
		mutex_unlock(&info->work_lock);
	} else if (data == 0) {

		/* Disable Secure World */
		if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_DISABLED) {
			input_err(true, &info->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		pm_runtime_put_sync(info->client->adapter->dev.parent);
		atomic_set(&info->secure_enabled, 0);
		sysfs_notify(&info->input_dev->dev.kobj, NULL, "secure_touch");
		sec_delay(10);

		clear_report_data(info);
		zt_touch_work(info->client->irq, info);

		complete(&info->secure_powerdown);
		complete(&info->secure_interrupt);

		if (old_ta_status != g_ta_connected)
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_USB_DETECT_BIT, g_ta_connected);

#if ESD_TIMER_INTERVAL
		/* zinitix timer start */
		esd_timer_start(CHECK_ESD_TIMER, info);
		write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
		input_info(true, &info->client->dev, "%s: secure touch disable\n", __func__);

	} else {
		input_err(true, &info->client->dev, "%s: unsupported value, %ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

static ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_DISABLED) {
		input_err(true, &info->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&info->secure_pending_irqs, -1, 0) == -1) {
		input_err(true, &info->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&info->secure_pending_irqs, 1, 0) == 1)
		val = 1;

	input_err(true, &info->client->dev, "%s: pending irq is %d\n",
			__func__, atomic_read(&info->secure_pending_irqs));

	complete(&info->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		secure_touch_enable_show, secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, secure_touch_show, NULL);

static struct attribute *secure_attr[] = {
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};

static void secure_touch_init(struct zt_ts_info *info)
{
	input_info(true, &info->client->dev, "%s\n", __func__);

	init_completion(&info->secure_powerdown);
	init_completion(&info->secure_interrupt);
}

static void secure_touch_stop(struct zt_ts_info *info, bool stop)
{
	if (atomic_read(&info->secure_enabled)) {
		atomic_set(&info->secure_pending_irqs, -1);

		sysfs_notify(&info->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&info->secure_powerdown);

		input_info(true, &info->client->dev, "%s: %d\n", __func__, stop);
	}
}
#endif

#ifdef USE_MISC_DEVICE
static long ts_misc_fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int ts_misc_fops_open(struct inode *inode, struct file *filp);
static int ts_misc_fops_close(struct inode *inode, struct file *filp);

static const struct file_operations ts_misc_fops = {
	.owner = THIS_MODULE,
	.open = ts_misc_fops_open,
	.release = ts_misc_fops_close,
	//.unlocked_ioctl = ts_misc_fops_ioctl,
	.compat_ioctl = ts_misc_fops_ioctl,
};

static struct miscdevice touch_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "zinitix_touch_misc",
	.fops = &ts_misc_fops,
};

#define TOUCH_IOCTL_BASE	0xbc
#define TOUCH_IOCTL_GET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 0, int)
#define TOUCH_IOCTL_SET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 1, int)
#define TOUCH_IOCTL_GET_CHIP_REVISION		_IOW(TOUCH_IOCTL_BASE, 2, int)
#define TOUCH_IOCTL_GET_FW_VERSION			_IOW(TOUCH_IOCTL_BASE, 3, int)
#define TOUCH_IOCTL_GET_REG_DATA_VERSION	_IOW(TOUCH_IOCTL_BASE, 4, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_SIZE		_IOW(TOUCH_IOCTL_BASE, 5, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_DATA		_IOW(TOUCH_IOCTL_BASE, 6, int)
#define TOUCH_IOCTL_START_UPGRADE			_IOW(TOUCH_IOCTL_BASE, 7, int)
#define TOUCH_IOCTL_GET_X_NODE_NUM			_IOW(TOUCH_IOCTL_BASE, 8, int)
#define TOUCH_IOCTL_GET_Y_NODE_NUM			_IOW(TOUCH_IOCTL_BASE, 9, int)
#define TOUCH_IOCTL_GET_TOTAL_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 10, int)
#define TOUCH_IOCTL_SET_RAW_DATA_MODE		_IOW(TOUCH_IOCTL_BASE, 11, int)
#define TOUCH_IOCTL_GET_RAW_DATA			_IOW(TOUCH_IOCTL_BASE, 12, int)
#define TOUCH_IOCTL_GET_X_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 13, int)
#define TOUCH_IOCTL_GET_Y_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 14, int)
#define TOUCH_IOCTL_HW_CALIBRAION			_IOW(TOUCH_IOCTL_BASE, 15, int)
#define TOUCH_IOCTL_GET_REG					_IOW(TOUCH_IOCTL_BASE, 16, int)
#define TOUCH_IOCTL_SET_REG					_IOW(TOUCH_IOCTL_BASE, 17, int)
#define TOUCH_IOCTL_SEND_SAVE_STATUS		_IOW(TOUCH_IOCTL_BASE, 18, int)
#define TOUCH_IOCTL_DONOT_TOUCH_EVENT		_IOW(TOUCH_IOCTL_BASE, 19, int)
#endif

struct zt_ts_info *misc_info;

static void zt_ts_esd_timer_stop(struct zt_ts_info *info)
{
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(info->client, ZT_CLEAR_INT_STATUS_CMD);
#endif
}

static void zt_ts_esd_timer_start(struct zt_ts_info *info)
{
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void set_cover_type(struct zt_ts_info *info, bool enable)
{
	struct i2c_client *client = info->client;

	mutex_lock(&info->power_init);

	if (enable) {
		switch (info->cover_type) {
		case ZT_FLIP_WALLET:
			write_reg(client, ZT_COVER_CONTROL_REG, WALLET_COVER_CLOSE);
			break;
		case ZT_VIEW_COVER:
			write_reg(client, ZT_COVER_CONTROL_REG, VIEW_COVER_CLOSE);
			break;
		case ZT_CLEAR_FLIP_COVER:
			write_reg(client, ZT_COVER_CONTROL_REG, CLEAR_COVER_CLOSE);
			break;
		case ZT_NEON_COVER:
			write_reg(client, ZT_COVER_CONTROL_REG, LED_COVER_CLOSE);
			break;
		case ZT_CLEAR_SIDE_VIEW_COVER:
			write_reg(client, ZT_COVER_CONTROL_REG, CLEAR_SIDE_VIEW_COVER_CLOSE);
			break;
		case ZT_MINI_SVIEW_WALLET_COVER:
			write_reg(client, ZT_COVER_CONTROL_REG, MINI_SVIEW_WALLET_COVER_CLOSE);
			break;
		default:
			input_err(true, &info->client->dev, "%s: touch is not supported for %d cover\n",
					__func__, info->cover_type);
		}
	} else {
		write_reg(client, ZT_COVER_CONTROL_REG, COVER_OPEN);
	}

	mutex_unlock(&info->power_init);

	input_info(true, &info->client->dev, "%s: type %d enable %d\n", __func__, info->cover_type, enable);
}

static bool get_raw_data(struct zt_ts_info *info, u8 *buff, int skip_cnt)
{
	struct i2c_client *client = info->client;
	struct sec_ts_plat_data *plat_data = info->plat_data;
	u32 total_node = info->cap_info.total_node_num;
	u32 sz;
	int i, j = 0;

	disable_irq(info->irq);

	mutex_lock(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_info(true, &client->dev, "other process occupied.. (%d)\n",
				info->work_state);
		enable_irq(info->irq);
		mutex_unlock(&info->work_lock);
		return false;
	}

	info->work_state = RAW_DATA;

	for (i = 0; i < skip_cnt; i++) {
		while (gpio_get_value(plat_data->irq_gpio)) {
			sec_delay(1);
			if (++j > 3000) {
				input_err(true, &info->client->dev, "%s: (skip_cnt) wait int timeout\n", __func__);
				break;
			}
		}

		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
		sec_delay(1);
	}

	input_dbg(true, &info->client->dev, "%s read raw data\r\n", __func__);
	sz = total_node*2;

	j = 0;
	while (gpio_get_value(plat_data->irq_gpio)) {
		sec_delay(1);
		if (++j > 3000) {
			input_err(true, &info->client->dev, "%s: wait int timeout\n", __func__);
			break;
		}
	}

	if (read_raw_data(client, ZT_RAWDATA_REG, (char *)buff, sz) < 0) {
		input_info(true, &client->dev, "%s: error : read zinitix tc raw data\n", __func__);
		info->work_state = NOTHING;
		clear_report_data(info);
		enable_irq(info->irq);
		mutex_unlock(&info->work_lock);
		return false;
	}

	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	clear_report_data(info);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	mutex_unlock(&info->work_lock);

	return true;
}

static bool ts_get_raw_data(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	u32 total_node = info->cap_info.total_node_num;
	u32 sz;

	if (!mutex_trylock(&info->raw_data_lock)) {
		input_err(true, &client->dev, "%s: Failed to occupy mutex\n", __func__);
		return true;
	}

	sz = total_node * 2 + sizeof(struct point_info) * MAX_SUPPORTED_FINGER_NUM;

	if (read_raw_data(info->client, ZT_RAWDATA_REG,
				(char *)info->cur_data, sz) < 0) {
		input_err(true, &client->dev, "%s: Failed to read raw data\n", __func__);
		mutex_unlock(&info->raw_data_lock);
		return false;
	}

	info->update = 1;
	memcpy((u8 *)(&info->touch_info[0]),
			(u8 *)&info->cur_data[total_node],
			sizeof(struct point_info) * MAX_SUPPORTED_FINGER_NUM);
	mutex_unlock(&info->raw_data_lock);

	return true;
}

static void zt_ts_fod_event_report(struct zt_ts_info *info, struct point_info touch_info)
{
	if (!info->fod_enable)
		return;

	if ((touch_info.byte01.value_u8bit == 0)
			 || (touch_info.byte01.value_u8bit == 1)) {
		info->scrub_id = SPONGE_EVENT_TYPE_FOD_PRESS;

		info->scrub_x = ((touch_info.byte02.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0xF0) >> 4);
		info->scrub_y = ((touch_info.byte03.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0x0F));

		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
		input_sync(info->input_dev);
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, &info->client->dev, "%s: FOD %s PRESS: %d\n", __func__,
				touch_info.byte01.value_u8bit ? "NORMAL" : "LONG", info->scrub_id);
#else
		input_info(true, &info->client->dev, "%s: FOD %s PRESS: %d, %d, %d\n", __func__,
				touch_info.byte01.value_u8bit ? "NORMAL" : "LONG",
				info->scrub_id, info->scrub_x, info->scrub_y);
#endif
	} else if (touch_info.byte01.value_u8bit == 2) {
		info->scrub_id = SPONGE_EVENT_TYPE_FOD_RELEASE;

		info->scrub_x = ((touch_info.byte02.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0xF0) >> 4);
		info->scrub_y = ((touch_info.byte03.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0x0F));

		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
		input_sync(info->input_dev);
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, &info->client->dev, "%s: FOD RELEASE: %d\n", __func__, info->scrub_id);
#else
		input_info(true, &info->client->dev, "%s: FOD RELEASE: %d, %d, %d\n",
				__func__, info->scrub_id, info->scrub_x, info->scrub_y);
#endif
	} else if (touch_info.byte01.value_u8bit == 3) {
		info->scrub_id = SPONGE_EVENT_TYPE_FOD_OUT;

		info->scrub_x = ((touch_info.byte02.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0xF0) >> 4);
		info->scrub_y = ((touch_info.byte03.value_u8bit << 4) & 0xFF0)
			| ((touch_info.byte04.value_u8bit & 0x0F));

		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
		input_sync(info->input_dev);
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, &info->client->dev, "%s: FOD OUT: %d\n", __func__, info->scrub_id);
#else
		input_info(true, &info->client->dev, "%s: FOD OUT: %d, %d, %d\n",
				__func__, info->scrub_id, info->scrub_x, info->scrub_y);
#endif
	}
}

static bool ts_read_coord(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i;
	u16 status_data;
	u16 pocket_data;

	/* for  Debugging Tool */

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (ts_get_raw_data(info) == false)
			return false;

		if (info->touch_mode == TOUCH_SENTIVITY_MEASUREMENT_MODE) {
			for (i = 0; i < TOUCH_SENTIVITY_MEASUREMENT_COUNT; i++)
				info->sensitivity_data[i] = info->cur_data[i];
		}

		goto out;
	}

	if (info->plat_data->pocket_mode) {
		if (read_data(info->client, ZT_STATUS_REG, (u8 *)&status_data, 2) < 0)
			input_err(true, &client->dev, "%s: fail to read status reg\n", __func__);

		if (zinitix_bit_test(status_data, BIT_POCKET_MODE)) {
			if (read_data(info->client, ZT_POCKET_DETECT, (u8 *)&pocket_data, 2) < 0) {
				input_err(true, &client->dev, "%s: fail to read pocket reg\n", __func__);
			} else if (info->plat_data->input_dev_proximity) {
				input_info(true, &client->dev, "Pocket %s\n", pocket_data == 11 ? "IN" : "OUT");
				input_report_abs(info->plat_data->input_dev_proximity, ABS_MT_CUSTOM, pocket_data);
				input_sync(info->plat_data->input_dev_proximity);
			} else {
				input_err(true, &client->dev, "%s: no dev for proximity\n", __func__);
			}
		}
	}

	memset(info->touch_info, 0x0, sizeof(struct point_info) * MAX_SUPPORTED_FINGER_NUM);

	if (read_data(info->client, ZT_POINT_STATUS_REG,
				(u8 *)(&info->touch_info[0]), sizeof(struct point_info)) < 0) {
		input_err(true, &client->dev, "Failed to read point info\n");
		return false;
	}

	if (info->fod_enable && info->fod_with_finger_packet) {
		memset(&info->touch_fod_info, 0x0, sizeof(struct point_info));

		if (read_data(info->client, ZT_FOD_STATUS_REG,
			(u8 *)(&info->touch_fod_info), sizeof(struct point_info)) < 0) {
			input_err(true, &client->dev, "Failed to read Touch FOD info\n");
			return false;
		}

		memset(info->fod_touch_vi_data, 0x0, info->fod_info_vi_data_len);

		if (info->fod_info_vi_data_len > 0) {
			if (read_data(info->client, ZT_VI_STATUS_REG,
				info->fod_touch_vi_data, info->fod_info_vi_data_len) < 0) {
				input_err(true, &client->dev, "Failed to read Touch VI Data\n");
				return false;
			}
		}

		if (info->touch_fod_info.byte00.value.eid == GESTURE_EVENT
				&& info->touch_fod_info.byte00.value.tid == FINGERPRINT)
			zt_ts_fod_event_report(info, info->touch_fod_info);
	}

	if (info->touch_info[0].byte00.value.eid == COORDINATE_EVENT) {
		info->touched_finger_num = info->touch_info[0].byte07.value.left_event;

#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
		if (info->touched_finger_num > (info->cap_info.multi_fingers-1)) {
			input_err(true, &client->dev, "Invalid touched_finger_num(%d)\n",
					info->touched_finger_num);
			info->touched_finger_num = 0;
		}
#endif

		if (info->touched_finger_num > 0) {
			if (read_data(info->client, ZT_POINT_STATUS_REG1, (u8 *)(&info->touch_info[1]),
						(info->touched_finger_num)*sizeof(struct point_info)) < 0) {
				input_err(true, &client->dev, "Failed to read touched point info\n");
				return false;
			}
		}
	} else if (info->touch_info[0].byte00.value.eid == GESTURE_EVENT) {
		if (info->touch_info[0].byte00.value.tid == SWIPE_UP) {
			input_info(true, &client->dev, "%s: Spay Gesture\n", __func__);

			info->scrub_id = SPONGE_EVENT_TYPE_SPAY;
			info->scrub_x = 0;
			info->scrub_y = 0;

			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
			input_sync(info->input_dev);
		} else if (info->touch_info[0].byte00.value.tid == FINGERPRINT) {
				zt_ts_fod_event_report(info, info->touch_info[0]);
		} else if (info->touch_info[0].byte00.value.tid == SINGLE_TAP) {
			if (info->singletap_enable) {
				info->scrub_id = SPONGE_EVENT_TYPE_SINGLE_TAP;

				info->scrub_x = ((info->touch_info[0].byte02.value_u8bit << 4) & 0xFF0)
					| ((info->touch_info[0].byte04.value_u8bit & 0xF0) >> 4);
				info->scrub_y = ((info->touch_info[0].byte03.value_u8bit << 4) & 0xFF0)
					| ((info->touch_info[0].byte04.value_u8bit & 0x0F));

				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
				input_sync(info->input_dev);
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &client->dev, "%s: SINGLE TAP: %d\n", __func__, info->scrub_id);
#else
				input_info(true, &client->dev, "%s: SINGLE TAP: %d, %d, %d\n",
						__func__, info->scrub_id, info->scrub_x, info->scrub_y);
#endif
			}
		} else if (info->touch_info[0].byte00.value.tid == DOUBLE_TAP) {
			if (info->aot_enable && (info->touch_info[0].byte01.value_u8bit == 1)) {
				input_report_key(info->input_dev, KEY_WAKEUP, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_WAKEUP, 0);
				input_sync(info->input_dev);

				input_info(true, &client->dev, "%s: AOT Doubletab\n", __func__);
			} else if (info->aod_enable && (info->touch_info[0].byte01.value_u8bit == 0)) {
				info->scrub_id = SPONGE_EVENT_TYPE_AOD_DOUBLETAB;

				info->scrub_x = ((info->touch_info[0].byte02.value_u8bit << 4) & 0xFF0)
					| ((info->touch_info[0].byte04.value_u8bit & 0xF0) >> 4);
				info->scrub_y = ((info->touch_info[0].byte03.value_u8bit << 4) & 0xFF0)
					| ((info->touch_info[0].byte04.value_u8bit & 0x0F));

				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
				input_sync(info->input_dev);

#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &client->dev, "%s: AOD Doubletab: %d\n", __func__, info->scrub_id);
#else
				input_info(true, &client->dev, "%s: AOD Doubletab: %d, %d, %d\n",
						__func__, info->scrub_id, info->scrub_x, info->scrub_y);
#endif
			}
		}
	}
out:
	write_cmd(info->client, ZT_CLEAR_INT_STATUS_CMD);
	return true;
}

#if ESD_TIMER_INTERVAL
static void esd_timeout_handler(struct timer_list *t)
{
	struct zt_ts_info *info = from_timer(info, t, esd_timeout_tmr);

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
	struct i2c_client *client = info->client;

	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		esd_timer_stop(info);
		return;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	struct i2c_client *client = info->client;

	if (STUI_MODE_TOUCH_SEC & stui_get_mode()) {
		input_err(true, &client->dev,
				"%s TSP not accessible during TUI\n", __func__);
		return;
	}
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		esd_timer_stop(info);
		return;
	}
#endif
	info->p_esd_timeout_tmr = NULL;
	queue_work(esd_tmr_workqueue, &info->tmr_work);
}

static void esd_timer_start(u16 sec, struct zt_ts_info *info)
{
	if (info->sleep_mode) {
		input_info(true, &info->client->dev, "%s skip (sleep_mode)!\n", __func__);
		return;
	}

	mutex_lock(&info->lock);
	if (info->p_esd_timeout_tmr != NULL)
#if IS_ENABLED(CONFIG_SMP)
		del_singleshot_timer_sync(info->p_esd_timeout_tmr);
#else
		del_timer(info->p_esd_timeout_tmr);
#endif
	info->p_esd_timeout_tmr = NULL;

	timer_setup(&info->esd_timeout_tmr, esd_timeout_handler, 0);
	mod_timer(&info->esd_timeout_tmr, jiffies + (HZ * sec));
	info->p_esd_timeout_tmr = &info->esd_timeout_tmr;
	mutex_unlock(&info->lock);
}

static void esd_timer_stop(struct zt_ts_info *info)
{
	mutex_lock(&info->lock);
	if (info->p_esd_timeout_tmr)
#if IS_ENABLED(CONFIG_SMP)
		del_singleshot_timer_sync(info->p_esd_timeout_tmr);
#else
		del_timer(info->p_esd_timeout_tmr);
#endif

	info->p_esd_timeout_tmr = NULL;
	mutex_unlock(&info->lock);
}

static void esd_timer_init(struct zt_ts_info *info)
{
	mutex_lock(&info->lock);
	timer_setup(&info->esd_timeout_tmr, esd_timeout_handler, 0);
	info->p_esd_timeout_tmr = NULL;
	mutex_unlock(&info->lock);
}

static void ts_tmr_work(struct work_struct *work)
{
	struct zt_ts_info *info =
		container_of(work, struct zt_ts_info, tmr_work);
	struct i2c_client *client = info->client;

	input_info(true, &client->dev, "%s++\n", __func__);

	if (info->work_state != NOTHING) {
		input_info(true, &client->dev, "%s: Other process occupied (%d)\n",
				__func__, info->work_state);

		return;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &client->dev, "%s: ignored, because touch is in secure mode\n", __func__);
		return;
	}
#endif

	info->work_state = ESD_TIMER;

	disable_irq_nosync(info->irq);
	zt_power_control(info, false);
	zt_power_control(info, true);

	clear_report_data(info);
	if (mini_init_touch(info) == false)
		goto fail_time_out_init;

	info->work_state = NOTHING;
	enable_irq(info->irq);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "%s--\n", __func__);
#endif

	return;
fail_time_out_init:
	input_err(true, &client->dev, "%s: Failed to restart\n", __func__);
	esd_timer_start(CHECK_ESD_TIMER, info);
	info->work_state = NOTHING;
	enable_irq(info->irq);
}
#endif

static bool zt_power_sequence(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	u16 chip_code;
	u16 checksum;

	if (read_data(client, ZT_CHECKSUM_RESULT, (u8 *)&checksum, 2) < 0) {
		input_err(true, &client->dev, "%s: Failed to read checksum\n", __func__);
		goto fail_power_sequence;
	}

	if (checksum == CORRECT_CHECK_SUM)
		return true;

	if (write_reg(client, VCMD_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: Failed to send power sequence(vendor cmd enable)\n", __func__);
		goto fail_power_sequence;
	}
	usleep_range(10, 10);

	if (read_data(client, VCMD_ID, (u8 *)&chip_code, 2) < 0) {
		input_err(true, &client->dev, "%s: Failed to read chip code\n", __func__);
		goto fail_power_sequence;
	}

	input_info(true, &client->dev, "%s: chip code = 0x%x\n", __func__, chip_code);
	usleep_range(10, 10);

	if (write_cmd(client, VCMD_INTN_CLR) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: Failed to send power sequence(intn clear)\n", __func__);
		goto fail_power_sequence;
	}
	usleep_range(10, 10);

	if (write_reg(client, VCMD_NVM_INIT, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: Failed to send power sequence(nvm init)\n", __func__);
		goto fail_power_sequence;
	}
	sec_delay(2);

	if (write_reg(client, VCMD_NVM_PROG_START, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: Failed to send power sequence(program start)\n", __func__);
		goto fail_power_sequence;
	}

	sec_delay(FIRMWARE_ON_DELAY);	/* wait for checksum cal */

	if (read_data(client, ZT_CHECKSUM_RESULT, (u8 *)&checksum, 2) < 0)
		input_err(true, &client->dev, "%s: Failed to read checksum (retry)\n", __func__);

	if (checksum == CORRECT_CHECK_SUM)
		return true;
	else
		input_err(true, &client->dev, "%s: Failed to read checksum 0x%x\n", __func__, checksum);

fail_power_sequence:
	return false;
}

static bool zt_power_control(struct zt_ts_info *info, bool ctl)
{
	struct i2c_client *client = info->client;
	int ret = 0;

	input_info(true, &client->dev, "%s: %d\n", __func__, ctl);

	mutex_lock(&info->power_init);

	ret = info->plat_data->power(&client->dev, ctl);
	if (ret) {
		mutex_unlock(&info->power_init);
		return false;
	}

	sec_input_pinctrl_configure(&client->dev, ctl);

	if (ctl) {
		sec_delay(CHIP_ON_DELAY);
		zt_power_sequence(info);
	} else
		sec_delay(CHIP_OFF_DELAY);

	mutex_unlock(&info->power_init);
	return true;
}

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
int tsp_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	struct zt_ts_info *info = container_of(nb, struct zt_ts_info, vbus_nb);
	vbus_status_t vbus_type = *(vbus_status_t *)data;
	struct power_supply *psy_otg;
	union power_supply_propval val;
	int ret = 0;

	input_info(true, &info->client->dev, "%s cmd=%lu, vbus_type=%d\n", __func__, cmd, vbus_type);

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		input_info(true, &info->client->dev, "%s : attach\n", __func__);
		g_ta_connected = true;
		break;
	case STATUS_VBUS_LOW:
		input_info(true, &info->client->dev, "%s : detach\n", __func__);
		g_ta_connected = false;
		break;
	default:
		break;
	}

	psy_otg = power_supply_get_by_name("otg");
	if (psy_otg) {
		ret = psy_otg->desc->get_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			input_err(true, &info->client->dev, "%s: fail to set power_suppy ONLINE property(%d)\n",
					__func__, ret);
		} else {
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_OTG_MODE, val.intval);
			input_info(true, &info->client->dev, "VBUS %s\n", val.intval ? "OTG" : "CHARGER");
			if (val.intval)
				g_ta_connected = false;
		}
	} else {
		input_err(true, &info->client->dev, "%s: Fail to get psy battery\n", __func__);
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&info->secure_enabled)) {
		input_info(true, &info->client->dev,
			"%s: ignored, because secure mode, old:%d, TA:%d\n",
			__func__, old_ta_status, g_ta_connected);
		return 0;
	} else {
		old_ta_status = g_ta_connected;
	}
#endif
	zt_set_optional_mode(info, DEF_OPTIONAL_MODE_USB_DETECT_BIT, g_ta_connected);
	return 0;
}
#endif

static void zt_charger_status_cb(struct tsp_callbacks *cb, bool ta_status)
{
	struct zt_ts_info *info =
		container_of(cb, struct zt_ts_info, callbacks);
	if (!ta_status)
		g_ta_connected = false;
	else
		g_ta_connected = true;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&info->secure_enabled)) {
		input_info(true, &info->client->dev,
			"%s: ignored, because secure mode, old:%d, TA:%d\n",
			__func__, old_ta_status, g_ta_connected);
		return;
	} else {
		old_ta_status = g_ta_connected;
	}
#endif

	zt_set_optional_mode(info, DEF_OPTIONAL_MODE_USB_DETECT_BIT, g_ta_connected);
	input_info(true, &info->client->dev, "TA %s\n", ta_status ? "connected" : "disconnected");
}

static bool crc_check(struct zt_ts_info *info)
{
	u16 chip_check_sum = 0;

	if (read_data(info->client, ZT_CHECKSUM_RESULT,
				(u8 *)&chip_check_sum, 2) < 0) {
		input_err(true, &info->client->dev, "%s: read crc fail", __func__);
	}

	input_info(true, &info->client->dev, "%s: 0x%04X\n", __func__, chip_check_sum);

	if (chip_check_sum == CORRECT_CHECK_SUM)
		return true;
	else
		return false;
}

static bool ts_check_need_upgrade(struct zt_ts_info *info,
		u16 cur_version, u16 cur_minor_version, u16 cur_reg_version, u16 cur_hw_id)
{
	u16	new_version;
	u16	new_minor_version;
	u16	new_reg_version;
#if CHECK_HWID
	u16	new_hw_id;
#endif

	new_version = (u16) (info->fw_data[52] | (info->fw_data[53]<<8));
	new_minor_version = (u16) (info->fw_data[56] | (info->fw_data[57]<<8));
	new_reg_version = (u16) (info->fw_data[60] | (info->fw_data[61]<<8));

#if CHECK_HWID
	new_hw_id = (u16) (fw_data[0x7528] | (fw_data[0x7529]<<8));
	input_info(true, &info->client->dev, "cur HW_ID = 0x%x, new HW_ID = 0x%x\n",
			cur_hw_id, new_hw_id);
	if (cur_hw_id != new_hw_id)
		return false;
#endif

	input_info(true, &info->client->dev, "cur version = 0x%x, new version = 0x%x\n",
			cur_version, new_version);
	input_info(true, &info->client->dev, "cur minor version = 0x%x, new minor version = 0x%x\n",
			cur_minor_version, new_minor_version);
	input_info(true, &info->client->dev, "cur reg data version = 0x%x, new reg data version = 0x%x\n",
			cur_reg_version, new_reg_version);

	if (info->plat_data->bringup == 3) {
		input_info(true, &info->client->dev, "%s: bringup 3, update when version is different\n", __func__);
		if (cur_version == new_version
				&& cur_minor_version == new_minor_version
				&& cur_reg_version == new_reg_version)
			return false;
		else
			return true;
	} else if (info->plat_data->bringup == 2) {
		input_info(true, &info->client->dev, "%s: bringup 2, skip update\n", __func__);
		return false;
	}

	if (cur_version > 0xFF)
		return true;
	if (cur_version < new_version)
		return true;
	else if (cur_version > new_version)
		return false;
	if (cur_minor_version < new_minor_version)
		return true;
	else if (cur_minor_version > new_minor_version)
		return false;
	if (cur_reg_version < new_reg_version)
		return true;

	return false;
}

static bool check_upgrade_method(struct zt_ts_info *info, const u8 *firmware_data, u16 chip_code)
{
	char bindata[0x80];
	u16 buff16[64];
	u16 flash_addr;
	int nsectorsize = 8;
	u32 chk_info_checksum;

	nvm_binary_info fw_info;
	nvm_binary_info ic_info;
	int i;

	struct i2c_client *client = info->client;

	if (write_reg(client, VCMD_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: check upgrade error (vcmd)\n", __func__);
		return false;
	}

	sec_delay(1);

	if (write_cmd(client, VCMD_INTN_CLR) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: check upgrade error (intn clear)\n", __func__);
		return false;
	}

	if (write_reg(client, VCMD_NVM_INIT, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: check upgrade error (nvm init)\n", __func__);
		return false;
	}

	sec_delay(5);
	memset(bindata, 0xff, sizeof(bindata));

	if (write_reg(client, VCMD_UPGRADE_INIT_FLASH, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: check upgrade error (init flash)\n", __func__);
		return false;
	}

	for (flash_addr = 0; flash_addr < 0x80; flash_addr += nsectorsize) {
		if (read_firmware_data(client, VCMD_UPGRADE_READ_FLASH,
					(u8 *)&bindata[flash_addr], TC_SECTOR_SZ) < 0) {
			input_err(true, &client->dev, "%s: failed to read firmware\n", __func__);
			return false;
		}
	}

	memcpy(ic_info.buff32, bindata, 0x80);	//copy data
	memcpy(buff16, bindata, 0x80);			//copy data

	//BIG ENDIAN ==> LITTLE ENDIAN
	info->ic_info_size = (((u32)ic_info.val.info_size[1] << 16 & 0x00FF0000) | ((u32)ic_info.val.info_size[2] << 8 & 0x0000FF00) | ((u32)ic_info.val.info_size[3] & 0x000000FF));
	info->ic_core_size = (((u32)ic_info.val.core_size[1] << 16 & 0x00FF0000) | ((u32)ic_info.val.core_size[2] << 8 & 0x0000FF00) | ((u32)ic_info.val.core_size[3] & 0x000000FF));
	info->ic_cust_size = (((u32)ic_info.val.custom_size[1] << 16 & 0x00FF0000) | ((u32)ic_info.val.custom_size[2] << 8 & 0x0000FF00) | ((u32)ic_info.val.custom_size[3] & 0x000000FF));
	info->ic_regi_size = (((u32)ic_info.val.register_size[1] << 16 & 0x00FF0000) | ((u32)ic_info.val.register_size[2] << 8 & 0x0000FF00) | ((u32)ic_info.val.register_size[3] & 0x000000FF));

	memcpy(fw_info.buff32, firmware_data, 0x80);

	info->fw_info_size = (((u32)fw_info.val.info_size[1] << 16 & 0x00FF0000) | ((u32)fw_info.val.info_size[2] << 8 & 0x0000FF00) | ((u32)fw_info.val.info_size[3] & 0x000000FF));
	info->fw_core_size = (((u32)fw_info.val.core_size[1] << 16 & 0x00FF0000) | ((u32)fw_info.val.core_size[2] << 8 & 0x0000FF00) | ((u32)fw_info.val.core_size[3] & 0x000000FF));
	info->fw_cust_size = (((u32)fw_info.val.custom_size[1] << 16 & 0x00FF0000) | ((u32)fw_info.val.custom_size[2] << 8 & 0x0000FF00) | ((u32)fw_info.val.custom_size[3] & 0x000000FF));
	info->fw_regi_size = (((u32)fw_info.val.register_size[1] << 16 & 0x00FF0000) | ((u32)fw_info.val.register_size[2] << 8 & 0x0000FF00) | ((u32)fw_info.val.register_size[3] & 0x000000FF));

	/*
	 * [DESC] Determining the execution conditions of [FULL D/L] or [Partial D/L]
	 */
	info->kind_of_download_method = NEED_FULL_DL;

	if ((info->ic_core_size == info->fw_core_size)
			&& (info->ic_cust_size == info->fw_cust_size)
			&& (ic_info.val.core_checksum == fw_info.val.core_checksum)
			&& (ic_info.val.custom_checksum == fw_info.val.custom_checksum)
			&& ((info->ic_regi_size != info->fw_regi_size)
				|| (ic_info.val.register_checksum != fw_info.val.register_checksum)))
		info->kind_of_download_method = NEED_PARTIAL_DL_REG;

	if ((info->ic_core_size == info->fw_core_size)
			&& (ic_info.val.core_checksum == fw_info.val.core_checksum)
			&& ((info->ic_cust_size != info->fw_cust_size)
				|| (ic_info.val.custom_checksum != fw_info.val.custom_checksum)))
		info->kind_of_download_method = NEED_PARTIAL_DL_CUSTOM;


	if (info->ic_info_size == 0 || info->ic_core_size == 0 ||
		info->ic_cust_size == 0 || info->ic_regi_size == 0 ||
		info->fw_info_size == 0 || info->fw_core_size == 0 ||
		info->fw_cust_size == 0 || info->fw_regi_size == 0 ||
		info->ic_info_size == 0xFFFFFFFF || info->ic_core_size == 0xFFFFFFFF ||
		info->ic_cust_size == 0xFFFFFFFF || info->ic_regi_size == 0xFFFFFFFF)
		info->kind_of_download_method = NEED_FULL_DL;

	if (info->kind_of_download_method != NEED_FULL_DL) {
		chk_info_checksum = 0;

		//info checksum.
		buff16[0x20 / 2] = 0;
		buff16[0x22 / 2] = 0;

		//Info checksum
		for (i = 0; i < 0x80 / 2; i++)
			chk_info_checksum += buff16[i];

		if (chk_info_checksum != ic_info.val.info_checksum)
			info->kind_of_download_method = NEED_FULL_DL;
	}

	return true;
}

static bool upgrade_fw_full_download(struct zt_ts_info *info, const u8 *firmware_data, u16 chip_code, u32 total_size)
{
	struct i2c_client *client = info->client;
	u32 flash_addr;
	int i;
	int nmemsz;
	int nrdsectorsize = 8;
#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650)
	unsigned short int erase_info[2];
#endif
	// change erase/program time
	u32	icNvmDelayRegister = 0x001E002C;
	u32	icNvmDelayTime = 0x00004FC2;
	u8 cData[8];

	nmemsz = info->fw_info_size + info->fw_core_size + info->fw_cust_size + info->fw_regi_size;
	if (nmemsz % nrdsectorsize > 0)
		nmemsz = (nmemsz / nrdsectorsize) * nrdsectorsize + nrdsectorsize;

	//[DEBUG]
	input_info(true, &client->dev, "%s: [UPGRADE] ENTER Firmware Upgrade(FULL)\n", __func__);

	if (write_reg(client, VCMD_NVM_INIT, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (nvm init)\n", __func__);
		return false;
	}
	sec_delay(5);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
	//====================================================
	// change erase/program time

	// set default OSC Freq - 44Mhz
	if (write_reg(client, VCMD_OSC_FREQ_SEL, 128) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: fail to write VCMD_OSC_FREQ_SEL\n", __func__);
		return false;
	}
	sec_delay(5);

	// address : Talpgm - 4.5msec
	cData[0] = (icNvmDelayRegister) & 0xFF;
	cData[1] = (icNvmDelayRegister >> 8) & 0xFF;
	cData[2] = (icNvmDelayRegister >> 16) & 0xFF;
	cData[3] = (icNvmDelayRegister >> 24) & 0xFF;
	// data
	cData[4] = (icNvmDelayTime) & 0xFF;
	cData[5] = (icNvmDelayTime >> 8) & 0xFF;
	cData[6] = (icNvmDelayTime >> 16) & 0xFF;
	cData[7] = (icNvmDelayTime >> 24) & 0xFF;

	if (write_data(client, VCMD_REG_WRITE, (u8 *)&cData[0], 8) < 0) {
		input_err(true, &client->dev, "%s: fail to change erase/program time\n", __func__);
		return false;
	}
	sec_delay(5);
#endif

	input_info(true, &client->dev, "%s: init flash\n", __func__);

	if (write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to write nvm vpp on\n", __func__);
		return false;
	}
	sec_delay(10);

	if (write_cmd(client, VCMD_UPGRADE_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
		return false;
	}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650)
	input_info(true, &client->dev, "%s: [UPGRADE] Erase start\n", __func__);	//[DEBUG]
	// Mass Erase
	//====================================================
	if (write_reg(client, VCMD_UPGRADE_WRITE_MODE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to enter write mode\n", __func__);
		return false;
	}
	sec_delay(1);

	for (i = 0; i < 3; i++) {
		erase_info[0] = 0x0001;
		erase_info[1] = i; //Section num.
		write_data(client, VCMD_UPGRADE_BLOCK_ERASE, (u8 *)&erase_info[0], 4);
		sec_delay(50);
	}

	for (i = 95; i < 126; i++) {
		erase_info[0] = 0x0000;
		erase_info[1] = i; //Page num.
		write_data(client, VCMD_UPGRADE_BLOCK_ERASE, (u8 *)&erase_info[0], 4);
		sec_delay(50);
	}

	sec_delay(100);
	if (write_cmd(client, VCMD_UPGRADE_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
		return false;
	}

	input_info(true, &client->dev, "%s: [UPGRADE] Erase End.\n", __func__);	//[DEBUG]
	// Mass Erase End
	//====================================================
#else
	if (write_reg(client, VCMD_UPGRADE_WRITE_MODE, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to enter write mode\n", __func__);
		return false;
	}
#endif
	input_info(true, &client->dev, "%s: [UPGRADE] UPGRADE START.\n", __func__);	//[DEBUG]
	sec_delay(1);

	for (flash_addr = 0; flash_addr < nmemsz; ) {
		for (i = 0; i < info->tsp_page_size/TC_SECTOR_SZ; i++) {
			if (write_data(client,
						VCMD_UPGRADE_WRITE_FLASH,
						(u8 *)&firmware_data[flash_addr], TC_SECTOR_SZ) < 0) {
				input_err(true, &client->dev, "%s: error: write zinitix tc firmware\n", __func__);
				return false;
			}
			flash_addr += TC_SECTOR_SZ;
			usleep_range(100, 100);
		}

		usleep_range(info->fuzing_udelay, info->fuzing_udelay); /*for fuzing delay*/
	}

	input_err(true, &client->dev, "%s: [UPGRADE] UPGRADE END. VERIFY START.\n", __func__);	//[DEBUG]

	//VERIFY
	zt_power_control(info, false);

	if (zt_power_control(info, true) == true) {
		sec_delay(10);
		input_info(true, &client->dev, "%s: upgrade finished\n", __func__);
		return true;
	}

	input_info(true, &client->dev, "%s: [UPGRADE] VERIFY FAIL.\n", __func__);	//[DEBUG]

	return false;

}

static bool upgrade_fw_partial_download(struct zt_ts_info *info, const u8 *firmware_data, u16 chip_code, u32 total_size)
{
	struct i2c_client *client = info->client;

	int i;
	int nrdsectorsize;
	int por_off_delay, por_on_delay;
	int idx, nmemsz;
	u32 erase_start_page_num;
#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650)
	unsigned short int erase_info[2];
#endif
	int nsectorsize = 8;

	nrdsectorsize = 8;
	por_off_delay = USB_POR_OFF_DELAY;
	por_on_delay = USB_POR_ON_DELAY;

	nmemsz = info->fw_info_size + info->fw_core_size + info->fw_cust_size + info->fw_regi_size;
	if (nmemsz % nrdsectorsize > 0)
		nmemsz = (nmemsz / nrdsectorsize) * nrdsectorsize + nrdsectorsize;

	erase_start_page_num = (info->fw_info_size + info->fw_core_size) / info->tsp_page_size;

	if (write_reg(client, VCMD_NVM_INIT, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (nvm init)\n", __func__);
		return false;
	}

	sec_delay(5);
	input_err(true, &client->dev, "%s: init flash\n", __func__);

	if (write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to write nvm vpp on\n", __func__);
		return false;
	}
	sec_delay(10);

	if (write_cmd(client, VCMD_UPGRADE_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
		return false;
	}

	/* Erase Area */
#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650)
	if (write_reg(client, VCMD_UPGRADE_WRITE_MODE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to enter write mode\n", __func__);
		return false;

	}
	sec_delay(1);
	erase_info[0] = 0x0000;
	erase_info[1] = 0x0000;	/* Page num. */
	write_data(client, VCMD_UPGRADE_BLOCK_ERASE, (u8 *)&erase_info[0], 4);

	for (i = erase_start_page_num; i < nmemsz/info->tsp_page_size; i++) {
		sec_delay(50);
		erase_info[0] = 0x0000;
		erase_info[1] = i;	/* Page num.*/
		write_data(client, VCMD_UPGRADE_BLOCK_ERASE, (u8 *)&erase_info[0], 4);
	}
	sec_delay(50);

	if (write_cmd(client, VCMD_UPGRADE_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
		return false;
	}
	sec_delay(1);

	if (write_reg(client, VCMD_UPGRADE_START_PAGE, 0x00) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to start addr. (erase end)\n", __func__);
		return false;
	}
	sec_delay(5);
#else
	if (write_reg(client, VCMD_UPGRADE_WRITE_MODE, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to enter write mode\n", __func__);
		return false;

	}
	sec_delay(1);
#endif

	for (i = 0; i < info->fw_info_size; ) {
		for (idx = 0; idx < info->tsp_page_size / nsectorsize; idx++) {
			if (write_data(client, VCMD_UPGRADE_WRITE_FLASH, (char *)&firmware_data[i], nsectorsize) != 0) {
				input_err(true, &client->dev, "%s: failed to write flash\n", __func__);
				return false;
			}
			i += nsectorsize;
			usleep_range(100, 100);
		}

		usleep_range(info->fuzing_udelay, info->fuzing_udelay); /*for fuzing delay*/
	}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZT7650M)
	if (write_reg(client, VCMD_UPGRADE_START_PAGE, erase_start_page_num) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to start addr. (erase end)\n", __func__);
		return false;

	}
	sec_delay(5);
#endif

	for (i = info->tsp_page_size * erase_start_page_num; i < nmemsz; ) {
		for (idx = 0; idx < info->tsp_page_size  / nsectorsize; idx++) {
			//npagesize = 1024 // nsectorsize : 8
			if (write_data(client, VCMD_UPGRADE_WRITE_FLASH, (char *)&firmware_data[i], nsectorsize) != 0) {
				input_err(true, &client->dev, "%s: failed to write flash\n", __func__);
				return false;
			}
			i += nsectorsize;
			usleep_range(100, 100);
		}

		usleep_range(info->fuzing_udelay, info->fuzing_udelay); /*for fuzing delay*/
	}

	input_info(true, &client->dev, "%s: [UPGRADE] PARTIAL UPGRADE END. VERIFY START.\n", __func__);

	//VERIFY
	zt_power_control(info, false);

	if (zt_power_control(info, true) == true) {
		sec_delay(10);
		input_info(true, &client->dev, "%s: upgrade finished\n", __func__);
		return true;
	}

	input_info(true, &client->dev, "%s: [UPGRADE] VERIFY FAIL.\n", __func__);

	return false;
}

static u8 ts_upgrade_firmware(struct zt_ts_info *info,
		const u8 *firmware_data)
{
	struct i2c_client *client = info->client;
	u32 flash_addr;
	int retry_cnt = 0;
	u16 chip_code;
	bool ret = false;
	u32 size = 0;

retry_upgrade:
	zt_power_control(info, false);
	ret = info->plat_data->power(&client->dev, true);
	sec_input_pinctrl_configure(&client->dev, true);
	sec_delay(CHIP_ON_DELAY);

	if (write_reg(client, VCMD_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (vendor cmd enable)\n", __func__);
		goto fail_upgrade;
	}
	sec_delay(10);

	if (write_cmd(client, VCMD_UPGRADE_MODE) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: upgrade mode error\n", __func__);
		goto fail_upgrade;
	}
	sec_delay(5);

	if (write_reg(client, VCMD_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (vendor cmd enable)\n", __func__);
		goto fail_upgrade;
	}
	sec_delay(1);

	if (write_cmd(client, VCMD_INTN_CLR) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: vcmd int clr error\n", __func__);
		goto fail_upgrade;
	}

	if (read_data(client, VCMD_ID, (u8 *)&chip_code, 2) < 0) {
		input_err(true, &client->dev, "%s: failed to read chip code\n", __func__);
		goto fail_upgrade;
	}

	sec_delay(5);

	input_info(true, &client->dev, "chip code = 0x%x\n", chip_code);

	flash_addr = ((firmware_data[0x59]<<16) | (firmware_data[0x5A]<<8) | firmware_data[0x5B]); //Info
	flash_addr += ((firmware_data[0x5D]<<16) | (firmware_data[0x5E]<<8) | firmware_data[0x5F]); //Core
	flash_addr += ((firmware_data[0x61]<<16) | (firmware_data[0x62]<<8) | firmware_data[0x63]); //Custom
	flash_addr += ((firmware_data[0x65]<<16) | (firmware_data[0x66]<<8) | firmware_data[0x67]); //Register

	info->cap_info.ic_fw_size = ((firmware_data[0x69]<<16) | (firmware_data[0x6A]<<8) | firmware_data[0x6B]); //total size

	input_info(true, &client->dev, "f/w ic_fw_size = %d\n", info->cap_info.ic_fw_size);

	if (flash_addr != 0)
		size = flash_addr;
	else
		size = info->cap_info.ic_fw_size;

	input_info(true, &client->dev, "f/w size = 0x%x Page_sz = %d\n", size, info->tsp_page_size);
	usleep_range(10, 10);

	/////////////////////////////////////////////////////////////////////////////////////////
	ret = check_upgrade_method(info, firmware_data, chip_code);
	if (ret == false) {
		input_err(true, &client->dev, "%s: check upgrade method error\n", __func__);
		goto fail_upgrade;
	}
	/////////////////////////////////////////////////////////////////////////////////////////

	if (info->kind_of_download_method == NEED_FULL_DL)
		ret = upgrade_fw_full_download(info, firmware_data, chip_code, size);
	else
		ret = upgrade_fw_partial_download(info, firmware_data, chip_code, size);

	if (ret == true)
		return true;

fail_upgrade:
	zt_power_control(info, false);

	if (++retry_cnt < INIT_RETRY_CNT) {
		input_err(true, &client->dev, "upgrade failed: so retry... (%d)\n", retry_cnt);
		goto retry_upgrade;
	}

	input_info(true, &client->dev, "%s: Failed to upgrade\n", __func__);

	return false;
}

static bool ts_hw_calibration(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	u16	chip_eeprom_info;
	int time_out = 0;

	input_info(true, &client->dev, "%s start\n", __func__);

	if (write_reg(client, ZT_TOUCH_MODE, 0x07) != I2C_SUCCESS)
		return false;

	sec_delay(10);
	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	sec_delay(10);
	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	sec_delay(50);
	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	sec_delay(10);

	if (write_cmd(client, ZT_CALIBRATE_CMD) != I2C_SUCCESS)
		return false;

	if (write_cmd(client, ZT_CLEAR_INT_STATUS_CMD) != I2C_SUCCESS)
		return false;

	sec_delay(10);
	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);

	/* wait for h/w calibration*/
	do {
		sec_delay(200);
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);

		if (read_data(client, ZT_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0)
			return false;

		input_dbg(true, &client->dev, "touch eeprom info = 0x%04X\r\n",
				chip_eeprom_info);

		if (!zinitix_bit_test(chip_eeprom_info, 0))
			break;

		if (time_out == 4) {
			write_cmd(client, ZT_CALIBRATE_CMD);
			sec_delay(10);
			write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
			input_err(true, &client->dev, "%s: h/w calibration retry timeout.\n", __func__);
		}

		if (time_out++ > 10) {
			input_err(true, &client->dev, "%s: h/w calibration timeout.\n", __func__);
			write_reg(client, ZT_TOUCH_MODE, TOUCH_POINT_MODE);
			sec_delay(50);
			write_cmd(client, ZT_SWRESET_CMD);
			return false;
		}
	} while (1);

	write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0001);
	usleep_range(100, 100);
	if (write_cmd(client, ZT_SAVE_CALIBRATION_CMD) != I2C_SUCCESS)
		return false;

	sec_delay(1100);
	write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0000);

	if (write_reg(client, ZT_TOUCH_MODE, TOUCH_POINT_MODE) != I2C_SUCCESS)
		return false;

	sec_delay(50);
	if (write_cmd(client, ZT_SWRESET_CMD) != I2C_SUCCESS)
		return false;

	return true;
}

static int ic_version_check(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);
	int ret;
	u8 data[8] = {0};

	/* get chip information */
	ret = read_data(client, ZT_VENDOR_ID, (u8 *)&cap->vendor_id, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail vendor id\n", __func__);
		goto error;
	}

	ret = read_data(client, ZT_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail fw_minor_version\n", __func__);
		goto error;
	}

	ret = read_data(client, ZT_CHIP_REVISION, data, 8);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail chip_revision\n", __func__);
		goto error;
	}

	cap->ic_revision = data[0] | (data[1] << 8);
	cap->fw_version = data[2] | (data[3] << 8);
	cap->reg_data_version = data[4] | (data[5] << 8);
	cap->hw_id = data[6] | (data[7] << 8);

error:
	return ret;
}

static int fw_update_work(struct zt_ts_info *info, bool force_update)
{
	struct sec_ts_plat_data *plat_data = info->plat_data;
	struct capa_info *cap = &(info->cap_info);
	int ret;
	bool need_update = false;
	const struct firmware *tsp_fw = NULL;
	char fw_path[SEC_TS_MAX_FW_PATH];
	u16 chip_eeprom_info;
#ifdef TCLM_CONCEPT
	int restore_cal = 0;
#endif
	if (plat_data->bringup == 1) {
		input_info(true, &info->client->dev, "%s: bringup 1 skip update\n", __func__);
		return 0;
	}

	snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", plat_data->firmware_name);
	input_info(true, &info->client->dev,
			"%s: start\n", __func__);

	ret = request_firmware(&tsp_fw, fw_path, &(info->client->dev));
	if (ret < 0) {
		input_info(true, &info->client->dev,
				"%s: Firmware image %s not available\n", __func__, fw_path);
		ret = 0;
		goto fw_request_fail;
	} else {
		info->fw_data = (unsigned char *)tsp_fw->data;
	}

	need_update = ts_check_need_upgrade(info, cap->fw_version,
			cap->fw_minor_version, cap->reg_data_version, cap->hw_id);
	if (!need_update) {
		if (!crc_check(info))
			need_update = true;
	}

	if (need_update == true || force_update == true) {
		ret = ts_upgrade_firmware(info, info->fw_data);
		if (!ret) {
			input_err(true, &info->client->dev, "%s: failed fw update\n", __func__);
			goto fw_request_fail;
		}

#ifdef TCLM_CONCEPT
		ret = zt_tclm_data_read(&info->client->dev, SEC_TCLM_NVM_ALL_DATA);
		if (ret < 0)
			input_info(true, &info->client->dev, "%s: sec_tclm_get_nvm_all error\n", __func__);

		input_info(true, &info->client->dev, "%s: tune_fix_ver [%04X] afe_base [%04X]\n",
				__func__, info->tdata->nvdata.tune_fix_ver, info->tdata->afe_base);

		if (((info->tdata->nvdata.tune_fix_ver == 0xffff) ||
				(info->tdata->afe_base > info->tdata->nvdata.tune_fix_ver))
				&& (info->tdata->tclm_level > TCLM_LEVEL_CLEAR_NV)) {
			/* tune version up case */
			sec_tclm_root_of_cal(info->tdata, CALPOSITION_TUNEUP);
			restore_cal = 1;
		} else if (info->tdata->tclm_level == TCLM_LEVEL_CLEAR_NV) {
			/* firmup case */
			sec_tclm_root_of_cal(info->tdata, CALPOSITION_FIRMUP);
			restore_cal = 1;
		}

		if (restore_cal == 1) {
			input_err(true, &info->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
			ret = sec_execute_tclm_package(info->tdata, 0);
			if (ret < 0) {
				input_err(true, &info->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);
				goto fw_request_fail;
			}
		}

		sec_tclm_root_of_cal(info->tdata, CALPOSITION_NONE);
#endif

		ret = ic_version_check(info);
		if (ret < 0)
			input_err(true, &info->client->dev, "%s: failed ic version check\n", __func__);
	}

	if (read_data(info->client, ZT_EEPROM_INFO,
				(u8 *)&chip_eeprom_info, 2) < 0) {
		ret = -1;
		goto fw_request_fail;
	}

#ifndef TCLM_CONCEPT
	if (zinitix_bit_test(chip_eeprom_info, 0)) { /* hw calibration bit*/
		if (ts_hw_calibration(info) == false) {
			ret = -1;
			goto fw_request_fail;
		}
	}
#endif

fw_request_fail:
	if (tsp_fw)
		release_firmware(tsp_fw);
	return ret;
}

static bool init_touch(struct zt_ts_info *info)
{
	struct sec_ts_plat_data *plat_data = info->plat_data;
#if ESD_TIMER_INTERVAL
	u16 reg_val = 0;
#endif
	u8 data[6] = {0};

	/* get x,y data */
	read_data(info->client, ZT_TOTAL_NUMBER_OF_Y, data, 4);
	info->cap_info.x_node_num = data[2] | (data[3] << 8);
	info->cap_info.y_node_num = data[0] | (data[1] << 8);

	info->cap_info.MaxX = plat_data->max_x;
	info->cap_info.MaxY = plat_data->max_y;

	info->cap_info.total_node_num = info->cap_info.x_node_num * info->cap_info.y_node_num;
	info->cap_info.multi_fingers = MAX_SUPPORTED_FINGER_NUM;

	input_info(true, &info->client->dev, "node x %d, y %d  resolution x %d, y %d\n",
			info->cap_info.x_node_num, info->cap_info.y_node_num,
			info->cap_info.MaxX, info->cap_info.MaxY);

#if ESD_TIMER_INTERVAL
	if (write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL,
				SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_init;

	read_data(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, (u8 *)&reg_val, 2);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &info->client->dev, "Esd timer register = %d\n", reg_val);
#endif
#endif
	if (!mini_init_touch(info))
		goto fail_init;

	return true;
fail_init:
	return false;
}

static int zt_set_fod_rect(struct zt_ts_info *info)
{
	int i, ret;
	u8 data[8];
	u32 sum = 0;

	for (i = 0; i < 4; i++) {
		data[i * 2] = info->fod_rect[i] & 0xFF;
		data[i * 2 + 1] = (info->fod_rect[i] >> 8) & 0xFF;
		sum += info->fod_rect[i];
	}

	if (!sum) /* no data */
		return 0;

	input_info(true, &info->client->dev, "%s: %u,%u,%u,%u\n",
			__func__, info->fod_rect[0], info->fod_rect[1],
			info->fod_rect[2], info->fod_rect[3]);

	ret = ts_write_to_sponge(info, ZT_SPONGE_FOD_RECT, data, sizeof(data));
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);

	return ret;
}

static int zt_set_aod_rect(struct zt_ts_info *info)
{
	u8 data[8] = {0};
	int i;
	int ret;

	for (i = 0; i < 4; i++) {
		data[i * 2] = info->plat_data->aod_data.rect_data[i] & 0xFF;
		data[i * 2 + 1] = (info->plat_data->aod_data.rect_data[i] >> 8) & 0xFF;
	}

	ret = ts_write_to_sponge(info, ZT_SPONGE_TOUCHBOX_W_OFFSET, data, sizeof(data));
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: fail set custom lib\n", __func__);

	return ret;
}

static bool mini_init_touch(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i;

	if (write_cmd(client, ZT_SWRESET_CMD) != I2C_SUCCESS) {
		input_info(true, &client->dev, "%s: Failed to write reset command\n", __func__);
		goto fail_mini_init;
	}

	if (write_reg(client, ZT_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		goto fail_mini_init;

	/* cover_set */
	if (write_reg(client, ZT_COVER_CONTROL_REG, COVER_OPEN) != I2C_SUCCESS)
		goto fail_mini_init;

	if (info->flip_enable)
		set_cover_type(info, info->flip_enable);

	if (write_reg(client, ZT_OPTIONAL_SETTING, info->m_optional_mode.optional_mode) != I2C_SUCCESS)
		goto fail_mini_init;

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
		usleep_range(10, 10);
	}

#if ESD_TIMER_INTERVAL
	if (write_reg(client, ZT_PERIODICAL_INTERRUPT_INTERVAL,
				SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_mini_init;

	esd_timer_start(CHECK_ESD_TIMER, info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "%s: Started esd timer\n", __func__);
#endif
#endif

	ts_write_to_sponge(info, ZT_SPONGE_LP_FEATURE, &info->lpm_mode, 2);
	zt_set_fod_rect(info);

	if (info->sleep_mode) {
#if ESD_TIMER_INTERVAL
		esd_timer_stop(info);
#endif
		write_cmd(info->client, ZT_SLEEP_CMD);
		input_info(true, &info->client->dev, "%s, sleep mode\n", __func__);

		zt_set_aod_rect(info);
	} else {
		zt_set_grip_type(info, ONLY_EDGE_HANDLER);
	}

	input_info(true, &client->dev, "%s: Successfully mini initialized\r\n", __func__);
	return true;

fail_mini_init:
	input_err(true, &client->dev, "%s: Failed to initialize mini init\n", __func__);
	return false;
}

static void clear_report_data(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i;
	u8 reported = 0;
	char location[7] = "";

	if (info->plat_data->prox_power_off) {
		input_report_key(info->input_dev, KEY_INT_CANCEL, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_INT_CANCEL, 0);
		input_sync(info->input_dev);
	}

	info->plat_data->prox_power_off = 0;

	if (info->plat_data->input_dev_proximity) {
		input_report_abs(info->plat_data->input_dev_proximity, ABS_MT_CUSTOM, 0xff);
		input_sync(info->plat_data->input_dev_proximity);
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		if (info->cur_coord[i].touch_status > SEC_TS_COORDINATE_ACTION_NONE) {
			input_mt_slot(info->input_dev, i);
#if IS_ENABLED(CONFIG_SEC_FACTORY)
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, 0);
#endif
			input_report_abs(info->input_dev, ABS_MT_CUSTOM, 0);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
			input_report_key(info->input_dev, BTN_PALM, false);
			input_report_key(info->input_dev, BTN_TOUCH, false);

			reported = true;
			if (!m_ts_debug_mode && TSP_NORMAL_EVENT_MSG) {
				location_detect(info, location, info->cur_coord[i].x, info->cur_coord[i].y);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &client->dev, "[RA] tID:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d p:%d\n",
						i, location,
						info->cur_coord[i].x - info->pressed_x[i],
						info->cur_coord[i].y - info->pressed_y[i],
						info->move_count[i], info->finger_cnt1,
						info->cur_coord[i].x,
						info->cur_coord[i].y,
						info->cur_coord[i].palm_count);
#else
				input_info(true, &client->dev, "[RA] tID:%02d loc:%s dd:%d,%d mc:%d tc:%d p:%d\n",
						i, location,
						info->cur_coord[i].x - info->pressed_x[i],
						info->cur_coord[i].y - info->pressed_y[i],
						info->move_count[i], info->finger_cnt1,
						info->cur_coord[i].palm_count);
#endif
			}
		}
		memset(&info->old_coord[i], 0, sizeof(struct ts_coordinate));
		memset(&info->cur_coord[i], 0, sizeof(struct ts_coordinate));
		info->move_count[i] = 0;
		info->cur_coord[i].palm_count = 0;
	}

	info->plat_data->palm_flag = 0;
	info->glove_touch = 0;

	if (reported)
		input_sync(info->input_dev);

	info->finger_cnt1 = 0;
	info->plat_data->hw_param.check_multi = 0;
}

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
void trustedui_mode_on(void)
{
	input_info(true, &tui_tsp_info->client->dev, "%s, release all finger..", __func__);
	clear_report_data(tui_tsp_info);
	input_info(true, &tui_tsp_info->client->dev, "%s : esd timer disable", __func__);
	zt_ts_esd_timer_stop(tui_tsp_info);

}
EXPORT_SYMBOL(trustedui_mode_on);

void trustedui_mode_off(void)
{
	input_info(true, &tui_tsp_info->client->dev, "%s : esd timer enable", __func__);
	zt_ts_esd_timer_start(tui_tsp_info);

}
EXPORT_SYMBOL(trustedui_mode_off);
#endif

void location_detect(struct zt_ts_info *info, char *loc, int x, int y)
{
	memset(loc, 0x00, 7);
	strncpy(loc, "xy:", 3);
	if (x < info->plat_data->area_edge)
		strncat(loc, "E.", 2);
	else if (x < (info->plat_data->max_x - info->plat_data->area_edge))
		strncat(loc, "C.", 2);
	else
		strncat(loc, "e.", 2);
	if (y < info->plat_data->area_indicator)
		strncat(loc, "S", 1);
	else if (y < (info->plat_data->max_y - info->plat_data->area_navigation))
		strncat(loc, "C", 1);
	else
		strncat(loc, "N", 1);
}

static irqreturn_t zt_touch_work(int irq, void *data)
{
	struct zt_ts_info *info = (struct zt_ts_info *)data;
	struct i2c_client *client = info->client;
	int i;
	u8 reported = false;
	u8 tid = 0;
	u8 ttype, tstatus;
	u16 x, y, z, maxX, maxY, sen_max;
	u16 st;
	u16 prox_data = 0;
	u8 info_major_w = 0;
	u8 info_minor_w = 0;
	char location[7] = "";
	int ret;
	char pos[5];
	char cur = 0;
	char old = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (IRQ_HANDLED == secure_filter_interrupt(info)) {
		wait_for_completion_interruptible_timeout(&info->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &client->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif

	if (info->sleep_mode) {
		pm_wakeup_event(info->input_dev->dev.parent, 500);

		/* waiting for blsp block resuming, if not occurs i2c error */
		ret = wait_for_completion_interruptible_timeout(&info->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret == 0) {
			input_err(true, &info->client->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return IRQ_HANDLED;
		} else if (ret < 0) {
			input_err(true, &info->client->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n",
					__func__, ret);
			return IRQ_HANDLED;
		}
	}

	if (gpio_get_value(info->plat_data->irq_gpio)) {
		input_err(true, &client->dev, "%s: Invalid interrupt\n", __func__);

		return IRQ_HANDLED;
	}

	if (!mutex_trylock(&info->work_lock)) {
		input_err(true, &client->dev, "%s: Failed to occupy work lock\n", __func__);
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
		clear_report_data(info);
		return IRQ_HANDLED;
	}
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#endif

	if (info->work_state != NOTHING) {
		input_err(true, &client->dev, "%s: Other process occupied\n", __func__);
		usleep_range(DELAY_FOR_SIGNAL_DELAY, DELAY_FOR_SIGNAL_DELAY);

		if (!gpio_get_value(info->plat_data->irq_gpio)) {
			write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
			clear_report_data(info);
			usleep_range(DELAY_FOR_SIGNAL_DELAY, DELAY_FOR_SIGNAL_DELAY);
		}

		goto out;
	}

	if (ts_read_coord(info) == false) { /* maybe desirable reset */
		input_err(true, &client->dev, "%s: Failed to read info coord\n", __func__);
		goto out;
	}

	info->work_state = NORMAL;
	reported = false;

	if (info->touch_info[0].byte00.value.eid == CUSTOM_EVENT ||
			info->touch_info[0].byte00.value.eid == GESTURE_EVENT)
		goto out;

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		info->old_coord[i] = info->cur_coord[i];
		memset(&info->cur_coord[i], 0, sizeof(struct ts_coordinate));
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		ttype = (info->touch_info[i].byte06.value.touch_type23 << 2) | (info->touch_info[i].byte07.value.touch_type01);
		tstatus = info->touch_info[i].byte00.value.touch_status;

		if (tstatus == SEC_TS_COORDINATE_ACTION_NONE && ttype != TOUCH_PROXIMITY)
			continue;

		tid = info->touch_info[i].byte00.value.tid;

		info->cur_coord[tid].id = tid;
		info->cur_coord[tid].touch_status = tstatus;
		info->cur_coord[tid].x = (info->touch_info[i].byte01.value.x_coord_h << 4) | (info->touch_info[i].byte03.value.x_coord_l);
		info->cur_coord[tid].y = (info->touch_info[i].byte02.value.y_coord_h << 4) | (info->touch_info[i].byte03.value.y_coord_l);
		info->cur_coord[tid].z = info->touch_info[i].byte06.value.z_value;
		info->cur_coord[tid].ttype = ttype;
		info->cur_coord[tid].major = info->touch_info[i].byte04.value_u8bit;
		info->cur_coord[tid].minor = info->touch_info[i].byte05.value_u8bit;
		info->cur_coord[tid].noise = info->touch_info[i].byte08.value_u8bit;
		info->cur_coord[tid].max_sense = info->touch_info[i].byte09.value_u8bit;

		if (!info->cur_coord[tid].palm && (info->cur_coord[tid].ttype == TOUCH_PALM))
			info->cur_coord[tid].palm_count++;

		info->cur_coord[tid].palm = (info->cur_coord[tid].ttype == TOUCH_PALM);
		if (info->cur_coord[tid].palm)
			info->plat_data->palm_flag |= (1 << tid);
		else
			info->plat_data->palm_flag &= ~(1 << tid);

		if (info->cur_coord[tid].z <= 0)
			info->cur_coord[tid].z = 1;
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		if ((info->cur_coord[i].ttype == TOUCH_PROXIMITY)
				&& (info->plat_data->support_ear_detect)) {
			if (read_data(info->client, ZT_PROXIMITY_DETECT, (u8 *)&prox_data, 2) < 0)
				input_err(true, &client->dev, "%s: fail to read proximity detect reg\n", __func__);

			info->hover_event = prox_data;

			input_info(true, &client->dev, "%s: PROX(%d)\n", __func__, prox_data);
			input_report_abs(info->plat_data->input_dev_proximity, ABS_MT_CUSTOM, prox_data);
			input_sync(info->plat_data->input_dev_proximity);
			break;
		}
	}

	info->noise_flag = -1;
	info->flip_cover_flag = 0;

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		if (info->cur_coord[i].touch_status == SEC_TS_COORDINATE_ACTION_NONE)
			continue;

		if ((info->noise_flag == -1) && (info->old_coord[i].noise != info->cur_coord[i].noise)) {
			info->noise_flag = info->cur_coord[i].noise;
			input_info(true, &client->dev, "NOISE MODE %s [%d]\n",
				info->noise_flag > 0 ? "ON":"OFF", info->noise_flag);
		}

		if (info->flip_cover_flag == 0) {
			if (info->old_coord[i].ttype != TOUCH_FLIP_COVER &&
					info->cur_coord[i].ttype == TOUCH_FLIP_COVER) {
				info->flip_cover_flag = 1;
				input_info(true, &client->dev, "%s: FLIP COVER MODE ON\n", __func__);
			}

			if (info->old_coord[i].ttype == TOUCH_FLIP_COVER &&
					info->cur_coord[i].ttype != TOUCH_FLIP_COVER) {
				info->flip_cover_flag = 1;
				input_info(true, &client->dev, "%s: FLIP COVER MODE OFF\n", __func__);
			}
		}

		if (info->old_coord[i].ttype != info->cur_coord[i].ttype) {
			if (info->cur_coord[i].touch_status == SEC_TS_COORDINATE_ACTION_PRESS)
				snprintf(pos, 5, "P");
			else if (info->cur_coord[i].touch_status == SEC_TS_COORDINATE_ACTION_MOVE)
				snprintf(pos, 5, "M");
			else
				snprintf(pos, 5, "R");

			if (info->cur_coord[i].ttype == TOUCH_PALM)
				cur = 'P';
			else if (info->cur_coord[i].ttype == TOUCH_GLOVE)
				cur = 'G';
			else
				cur = 'N';

			if (info->old_coord[i].ttype == TOUCH_PALM)
				old = 'P';
			else if (info->old_coord[i].ttype == TOUCH_GLOVE)
				old = 'G';
			else
				old = 'N';

			if (cur != old)
				input_info(true, &client->dev, "tID:%d ttype(%c->%c) : %s\n", i, old, cur, pos);
		}

		if ((info->cur_coord[i].touch_status == SEC_TS_COORDINATE_ACTION_PRESS ||
				info->cur_coord[i].touch_status == SEC_TS_COORDINATE_ACTION_MOVE)) {
			x = info->cur_coord[i].x;
			y = info->cur_coord[i].y;
			z = info->cur_coord[i].z;
			info_major_w = info->cur_coord[i].major;
			info_minor_w = info->cur_coord[i].minor;
			sen_max = info->cur_coord[i].max_sense;

			maxX = info->cap_info.MaxX;
			maxY = info->cap_info.MaxY;

			if (x > maxX || y > maxY) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_err(true, &client->dev,
						"Invalid coord %d : x=%d, y=%d\n", i, x, y);
#endif
				continue;
			}

			st = sen_max & 0x0F;
			if (st < 1)
				st = 1;

			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);

			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, (u32)info_major_w);

#if IS_ENABLED(CONFIG_SEC_FACTORY)
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, (u32)st);
#endif
			input_report_abs(info->input_dev, ABS_MT_WIDTH_MAJOR, (u32)info_major_w);
			input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, info_minor_w);

			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
			input_report_key(info->input_dev, BTN_PALM, info->plat_data->palm_flag);

			input_report_key(info->input_dev, BTN_TOUCH, 1);

			if ((info->cur_coord[i].touch_status == SEC_TS_COORDINATE_ACTION_PRESS) &&
					(info->cur_coord[i].touch_status != info->old_coord[i].touch_status)) {
				info->pressed_x[i] = x; /*for getting coordinates of pressed point*/
				info->pressed_y[i] = y;
				info->finger_cnt1++;

				if ((info->finger_cnt1 > 4) && (info->plat_data->hw_param.check_multi == 0)) {
					info->plat_data->hw_param.check_multi = 1;
					info->plat_data->hw_param.multi_count++;
					input_info(true, &client->dev, "data : pn=%d mc=%d\n",
							info->finger_cnt1, info->plat_data->hw_param.multi_count);
				}

				location_detect(info, location, x, y);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &client->dev, "[P] tID:%d,%d x:%d y:%d z:%d(st:%d) max:%d major:%d minor:%d loc:%s tc:%d touch_type:%x noise:%x\n",
						i, (info->input_dev->mt->trkid - 1) & TRKID_MAX, x, y, z, st, sen_max, info_major_w,
						info_minor_w, location, info->finger_cnt1, info->cur_coord[i].ttype, info->cur_coord[i].noise);
#else
				input_info(true, &client->dev, "[P] tID:%d,%d z:%d(st:%d) max:%d major:%d minor:%d loc:%s tc:%d touch_type:%x noise:%x\n",
						i, (info->input_dev->mt->trkid - 1) & TRKID_MAX, z, st, sen_max, info_major_w,
						info_minor_w, location, info->finger_cnt1, info->cur_coord[i].ttype, info->cur_coord[i].noise);
#endif
			} else if (info->cur_coord[i].touch_status == SEC_TS_COORDINATE_ACTION_MOVE) {
				info->move_count[i]++;
			}
		} else if (info->cur_coord[i].touch_status == SEC_TS_COORDINATE_ACTION_RELEASE) {
			info->plat_data->palm_flag &= ~(1 << i);
			input_report_key(info->input_dev, BTN_PALM, info->plat_data->palm_flag);
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

			location_detect(info, location, info->cur_coord[i].x, info->cur_coord[i].y);

			if (info->finger_cnt1 > 0)
				info->finger_cnt1--;

			if (info->finger_cnt1 == 0) {
				input_report_key(info->input_dev, BTN_TOUCH, 0);
				info->plat_data->hw_param.check_multi = 0;
				info->plat_data->palm_flag = 0;
			}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &client->dev,
					"[R] tID:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d p:%d\n",
					i, location,
					info->cur_coord[i].x - info->pressed_x[i],
					info->cur_coord[i].y - info->pressed_y[i],
					info->move_count[i], info->finger_cnt1,
					info->cur_coord[i].x,
					info->cur_coord[i].y,
					info->cur_coord[i].palm_count);
#else
			input_info(true, &client->dev,
					"[R] tID:%02d loc:%s dd:%d,%d mc:%d tc:%d p:%d\n",
					i, location,
					info->cur_coord[i].x - info->pressed_x[i],
					info->cur_coord[i].y - info->pressed_y[i],
					info->move_count[i], info->finger_cnt1,
					info->cur_coord[i].palm_count);
#endif

			info->move_count[i] = 0;
			memset(&info->cur_coord[i], 0, sizeof(struct ts_coordinate));
		}
	}

	input_sync(info->input_dev);

out:
	if (info->work_state == NORMAL) {
#if ESD_TIMER_INTERVAL
		esd_timer_start(CHECK_ESD_TIMER, info);
#endif
		info->work_state = NOTHING;
	}

	mutex_unlock(&info->work_lock);

	return IRQ_HANDLED;
}

static int  zt_ts_open(struct device *dev)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);
	int ret = 0;

	if (info == NULL)
		return 0;

	if (!info->info_work_done) {
		input_err(true, &info->client->dev, "%s not finished info work\n", __func__);
		return 0;
	}

	input_info(true, &info->client->dev, "%s, %d \n", __func__, __LINE__);

	atomic_set(&info->plat_data->enabled, 1);

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
	sec_delay(100);
	if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
		tui_force_close(1);
		sec_delay(100);
		if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
			trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
			trustedui_set_mode(TRUSTEDUI_MODE_OFF);
		}
	}
#endif // CONFIG_TRUSTONIC_TRUSTED_UI
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(info, 0);
#endif

	if (info->sleep_mode) {
		mutex_lock(&info->work_lock);
		info->work_state = SLEEP_MODE_OUT;
		info->sleep_mode = 0;
		input_info(true, &info->client->dev, "%s, wake up\n", __func__);

		write_cmd(info->client, ZT_WAKEUP_CMD);
		write_reg(info->client, ZT_OPTIONAL_SETTING, info->m_optional_mode.optional_mode);
		info->work_state = NOTHING;
		mutex_unlock(&info->work_lock);

#if ESD_TIMER_INTERVAL
		esd_timer_start(CHECK_ESD_TIMER, info);
#endif
		if (device_may_wakeup(info->sec.fac_dev))
			disable_irq_wake(info->irq);
	} else {
		mutex_lock(&info->work_lock);
		if (info->work_state != RESUME
				&& info->work_state != EALRY_SUSPEND) {
			input_info(true, &info->client->dev, "invalid work proceedure (%d)\r\n",
					info->work_state);
			mutex_unlock(&info->work_lock);
			return 0;
		}

		ret = zt_power_control(info, true);
		if (ret == false) {
			zt_power_control(info, false);
			zt_power_control(info, true);
		}

		crc_check(info);

		if (mini_init_touch(info) == false)
			goto fail_late_resume;
		enable_irq(info->irq);
		info->work_state = NOTHING;

		if (g_ta_connected)
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_USB_DETECT_BIT, g_ta_connected);

		mutex_unlock(&info->work_lock);
		input_dbg(true, &info->client->dev, "%s--\n", __func__);
		return 0;

fail_late_resume:
		input_info(true, &info->client->dev, "%s: failed to late resume\n", __func__);
		enable_irq(info->irq);
		info->work_state = NOTHING;
		mutex_unlock(&info->work_lock);
	}

	cancel_delayed_work(&info->work_print_info);
	info->plat_data->print_info_cnt_open = 0;
	info->plat_data->print_info_cnt_release = 0;
	if (!shutdown_is_on_going_tsp)
		schedule_work(&info->work_print_info.work);

	return 0;
}

static int zt_ts_close(struct device *dev)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);
	int i;
	u8 prev_work_state;

	if (info == NULL)
		return 0;

	if (!info->info_work_done) {
		input_err(true, &info->client->dev, "%s not finished info work\n", __func__);
		return 0;
	}

	input_info(true, &info->client->dev,
			"%s, spay:%d aod:%d aot:%d singletap:%d prox:%d pocket:%d ed:%d\n",
			__func__, info->spay_enable, info->aod_enable,
			info->aot_enable, info->singletap_enable, info->plat_data->prox_power_off,
			info->plat_data->pocket_mode, info->plat_data->ed_enable);

	atomic_set(&info->plat_data->enabled, 0);

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
	sec_delay(100);
	if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
		tui_force_close(1);
		sec_delay(100);
		if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
			trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
			trustedui_set_mode(TRUSTEDUI_MODE_OFF);
		}
	}
#endif // CONFIG_TRUSTONIC_TRUSTED_UI
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(info, 1);
#endif

#ifdef TCLM_CONCEPT
	sec_tclm_debug_info(info->tdata);
#endif

#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
#endif

	if (info->touch_mode == TOUCH_AGING_MODE) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
#if ESD_TIMER_INTERVAL
		write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
		input_info(true, &info->client->dev, "%s, set touch mode\n", __func__);
	}

	if (((info->spay_enable || info->aod_enable || info->aot_enable || info->singletap_enable))
			|| info->plat_data->pocket_mode || info->plat_data->ed_enable
			|| info->fod_enable || info->fod_lp_mode) {
		mutex_lock(&info->work_lock);
		prev_work_state = info->work_state;
		info->work_state = SLEEP_MODE_IN;
		input_info(true, &info->client->dev, "%s, sleep mode\n", __func__);

#if ESD_TIMER_INTERVAL
		esd_timer_stop(info);
#endif
		ts_set_utc_sponge(info);

		if (info->plat_data->prox_power_off && info->aot_enable)
			zinitix_bit_clr(info->lpm_mode, ZT_SPONGE_MODE_DOUBLETAP_WAKEUP);

		input_info(true, &info->client->dev,
				"%s: write lpm_mode 0x%02x (spay:%d, aod:%d, singletap:%d, aot:%d, fod:%d, fod_lp:%d)\n",
				__func__, info->lpm_mode,
				(info->lpm_mode & (1 << ZT_SPONGE_MODE_SPAY)) ? 1 : 0,
				(info->lpm_mode & (1 << ZT_SPONGE_MODE_AOD)) ? 1 : 0,
				(info->lpm_mode & (1 << ZT_SPONGE_MODE_SINGLETAP)) ? 1 : 0,
				(info->lpm_mode & (1 << ZT_SPONGE_MODE_DOUBLETAP_WAKEUP)) ? 1 : 0,
				info->fod_enable, info->fod_lp_mode);

		ts_write_to_sponge(info, ZT_SPONGE_LP_FEATURE, &info->lpm_mode, 2);
		zt_set_fod_rect(info);

		write_cmd(info->client, ZT_SLEEP_CMD);
		info->sleep_mode = 1;

		if (info->aot_enable)
			zinitix_bit_set(info->lpm_mode, ZT_SPONGE_MODE_DOUBLETAP_WAKEUP);

		/* clear garbage data */
		for (i = 0; i < 2; i++) {
			sec_delay(10);
			write_cmd(info->client, ZT_CLEAR_INT_STATUS_CMD);
		}
		clear_report_data(info);

		info->work_state = prev_work_state;
		if (device_may_wakeup(info->sec.fac_dev))
			enable_irq_wake(info->irq);
	} else {
		disable_irq(info->irq);
		mutex_lock(&info->work_lock);
		if (info->work_state != NOTHING) {
			input_info(true, &info->client->dev, "invalid work proceedure (%d)\r\n",
					info->work_state);
			mutex_unlock(&info->work_lock);
			enable_irq(info->irq);
			return 0;
		}
		info->work_state = EALRY_SUSPEND;

		clear_report_data(info);

#if ESD_TIMER_INTERVAL
		/*write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, 0);*/
		esd_timer_stop(info);
#endif

		zt_power_control(info, false);
	}

	cancel_delayed_work(&info->work_print_info);
	zt_print_info(info);

	input_info(true, &info->client->dev, "%s --\n", __func__);
	mutex_unlock(&info->work_lock);
	return 0;
}

static int ts_set_touchmode(struct zt_ts_info *info, u16 value)
{
	int i, ret = 1;
	int retry_cnt = 0;
	struct capa_info *cap = &(info->cap_info);

	disable_irq(info->irq);

	mutex_lock(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_info(true, &info->client->dev, "other process occupied.. (%d)\n",
				info->work_state);
		enable_irq(info->irq);
		mutex_unlock(&info->work_lock);
		return -1;
	}

retry_ts_set_touchmode:
	//wakeup cmd
	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);

	info->work_state = SET_MODE;

	if (value == TOUCH_SEC_MODE)
		info->touch_mode = TOUCH_POINT_MODE;
	else
		info->touch_mode = value;

	input_info(true, &info->client->dev, "[zinitix_touch] tsp_set_testmode %d\r\n", info->touch_mode);

	if (!((info->touch_mode == TOUCH_POINT_MODE) ||
				(info->touch_mode == TOUCH_SENTIVITY_MEASUREMENT_MODE))) {
		if (write_reg(info->client, ZT_DELAY_RAW_FOR_HOST,
					RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			input_info(true, &info->client->dev, "%s: Fail to set ZT_DELAY_RAW_FOR_HOST.\r\n", __func__);
	}

	if (write_reg(info->client, ZT_TOUCH_MODE,
				info->touch_mode) != I2C_SUCCESS)
		input_info(true, &info->client->dev, "[zinitix_touch] TEST Mode : "
				"Fail to set ZINITX_TOUCH_MODE %d.\r\n", info->touch_mode);

	input_info(true, &info->client->dev, "%s: tsp_set_testmode. write regiter end\n", __func__);

	ret = read_data(info->client, ZT_TOUCH_MODE, (u8 *)&cap->current_touch_mode, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail touch mode read\n", __func__);
		goto out;
	}

	if (cap->current_touch_mode != info->touch_mode) {
		if (retry_cnt < 1) {
			retry_cnt++;
			goto retry_ts_set_touchmode;
		}
		input_info(true, &info->client->dev, "%s: fail to set touch_mode %d (current_touch_mode %d).\n",
				__func__, info->touch_mode, cap->current_touch_mode);
		ret = -1;
		goto out;
	}

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		sec_delay(20);
		write_cmd(info->client, ZT_CLEAR_INT_STATUS_CMD);
	}

	clear_report_data(info);

	input_info(true, &info->client->dev, "%s: tsp_set_testmode. garbage data end\n", __func__);

out:
	info->work_state = NOTHING;
	enable_irq(info->irq);
	mutex_unlock(&info->work_lock);
	return ret;
}

static int ts_upgrade_sequence(struct zt_ts_info *info, const u8 *firmware_data, int restore_cal)
{
	int ret = 0;
	disable_irq(info->irq);
	mutex_lock(&info->work_lock);
	info->work_state = UPGRADE;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#endif
	clear_report_data(info);

	input_info(true, &info->client->dev, "%s: start upgrade firmware\n", __func__);
	if (ts_upgrade_firmware(info, firmware_data) == false) {
		ret = -1;
		goto out;
	}

	if (ic_version_check(info) < 0)
		input_err(true, &info->client->dev, "%s: failed ic version check\n", __func__);

#ifdef TCLM_CONCEPT
	if (restore_cal == 1) {
		input_err(true, &info->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
		ret = sec_execute_tclm_package(info->tdata, 0);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);
			ret = -EIO;
			goto out;
		}
	}
#else
	if (ts_hw_calibration(info) == false) {
		ret = -1;
		goto out;
	}
#endif

	if (mini_init_touch(info) == false) {
		ret = -1;
		goto out;

	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &info->client->dev, "%s: Started esd timer\n", __func__);
#endif
#endif
out:
	enable_irq(info->irq);
	misc_info->work_state = NOTHING;
	mutex_unlock(&info->work_lock);
	return ret;
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct sec_ts_plat_data *plat_data = info->plat_data;
	struct i2c_client *client = info->client;
	char fw_path[SEC_TS_MAX_FW_PATH+1];
	char result[16] = {0};
	const struct firmware *tsp_fw = NULL;
	unsigned char *fw_data = NULL;
	int restore_cal = 0;
	int ret;
	u8 update_type = 0;

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		update_type = TSP_TYPE_BUILTIN_FW;
		break;
	case UMS:
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		update_type = TSP_TYPE_EXTERNAL_FW_SIGNED;
#else
		update_type = TSP_TYPE_EXTERNAL_FW;
#endif
		break;
	case SPU:
		update_type = TSP_TYPE_SPU_FW_SIGNED;
		break;
	default:
		goto fw_update_out;
	}

	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (update_type == TSP_TYPE_BUILTIN_FW) {
		/* firmware update builtin binary */
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", plat_data->firmware_name);

		ret = request_firmware(&tsp_fw, fw_path, &(client->dev));
		if (ret) {
			input_info(true, &client->dev,
					"%s: Firmware image %s not available\n", __func__,
					fw_path);

			goto fw_update_out;
		} else {
			fw_data = (unsigned char *)tsp_fw->data;
		}

#ifdef TCLM_CONCEPT
		sec_tclm_root_of_cal(info->tdata, CALPOSITION_TESTMODE);
		restore_cal = 1;
#endif
		ret = ts_upgrade_sequence(info, (u8 *)fw_data, restore_cal);
		if (ret < 0)
			goto fw_update_out;

		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		/* firmware update ums or spu */
		if (update_type == TSP_TYPE_EXTERNAL_FW)
			snprintf(fw_path, SEC_TS_MAX_FW_PATH, TSP_EXTERNAL_FW);
		else if (update_type == TSP_TYPE_EXTERNAL_FW_SIGNED)
			snprintf(fw_path, SEC_TS_MAX_FW_PATH, TSP_EXTERNAL_FW_SIGNED);
		else if (update_type == TSP_TYPE_SPU_FW_SIGNED)
			snprintf(fw_path, SEC_TS_MAX_FW_PATH, TSP_SPU_FW_SIGNED);
		else
			goto fw_update_out;

		ret = request_firmware(&tsp_fw, fw_path, &(client->dev));
		if (ret) {
			input_err(true, &client->dev,
				"%s: Firmware image %s not available\n", __func__, fw_path);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto fw_update_out;
		}

#ifdef SPU_FW_SIGNED
		info->fw_data = (unsigned char *)tsp_fw->data;
		if (!(ts_check_need_upgrade(info, info->cap_info.fw_version, info->cap_info.fw_minor_version,
						info->cap_info.reg_data_version, info->cap_info.hw_id)) &&
				(update_type == TSP_TYPE_SPU_FW_SIGNED)) {
			input_info(true, &client->dev, "%s: skip ffu update\n", __func__);
			goto fw_update_out;
		}

		if (update_type == TSP_TYPE_EXTERNAL_FW_SIGNED || update_type == TSP_TYPE_SPU_FW_SIGNED) {
			int ori_size;
			int spu_ret;

			ori_size = tsp_fw->size - SPU_METADATA_SIZE(TSP);

			spu_ret = spu_firmware_signature_verify("TSP", tsp_fw->data, tsp_fw->size);
			if (ori_size != spu_ret) {
				input_err(true, &client->dev, "%s: signature verify failed, ori:%d, fsize:%ld\n",
						__func__, ori_size, tsp_fw->size);

				goto fw_update_out;
			}
		}
#endif

#ifdef TCLM_CONCEPT
		sec_tclm_root_of_cal(info->tdata, CALPOSITION_TESTMODE);
		restore_cal = 1;
#endif
		ret = ts_upgrade_sequence(info, tsp_fw->data, restore_cal);
		if (ret < 0)
			goto fw_update_out;

		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

fw_update_out:
#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(info->tdata, CALPOSITION_NONE);
#endif

	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(result, sizeof(result), "OK");
	else
		snprintf(result, sizeof(result), "NG");

	sec_cmd_set_cmd_result(sec, result, strnlen(result, sizeof(result)));

	if (tsp_fw)
		release_firmware(tsp_fw);
	return;
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct sec_ts_plat_data *plat_data = info->plat_data;
	struct i2c_client *client = info->client;
	const struct firmware *tsp_fw = NULL;
	unsigned char *fw_data = NULL;
	char fw_path[SEC_TS_MAX_FW_PATH];
	char buff[16] = { 0 };
	u16 fw_version, fw_minor_version, reg_version, hw_id, ic_revision;
	u32 version;
	int ret;

	snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", plat_data->firmware_name);

	ret = request_firmware(&tsp_fw, fw_path, &(client->dev));
	if (ret) {
		input_info(true, &client->dev,
				"%s: Firmware image %s not available\n", __func__,
				fw_path);
		goto fw_request_fail;
	} else {
		fw_data = (unsigned char *)tsp_fw->data;
	}
	sec_cmd_set_default_result(sec);

	/* To Do */
	/* modify m_firmware_data */
	hw_id = (u16)(fw_data[48] | (fw_data[49] << 8));
	fw_version = (u16)(fw_data[52] | (fw_data[53] << 8));
	fw_minor_version = (u16)(fw_data[56] | (fw_data[57] << 8));
	reg_version = (u16)(fw_data[60] | (fw_data[61] << 8));

	ic_revision = (u16)(fw_data[68] | (fw_data[69] << 8));
	version = (u32)((u32)(ic_revision & 0xff) << 24)
		| ((fw_minor_version & 0xff) << 16)
		| ((hw_id & 0xff) << 8) | (reg_version & 0xff);

	snprintf(buff, sizeof(buff), "ZI%08X", version);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

fw_request_fail:
	if (tsp_fw)
		release_firmware(tsp_fw);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };
	char model[16] = { 0 };
	u16 fw_version, fw_minor_version, reg_version, hw_id, vendor_id, ic_revision;
	u32 version, length;
	int ret;

	sec_cmd_set_default_result(sec);

	zt_ts_esd_timer_stop(info);

	mutex_lock(&info->work_lock);

	ret = ic_version_check(info);
	mutex_unlock(&info->work_lock);
	if (ret < 0) {
		input_info(true, &client->dev, "%s: version check error\n", __func__);
		return;
	}

	zt_ts_esd_timer_start(info);

	fw_version = info->cap_info.fw_version;
	fw_minor_version = info->cap_info.fw_minor_version;
	reg_version = info->cap_info.reg_data_version;
	hw_id = info->cap_info.hw_id;
	ic_revision =  info->cap_info.ic_revision;

	vendor_id = ntohs(info->cap_info.vendor_id);
	version = (u32)((u32)(ic_revision & 0xff) << 24)
		| ((fw_minor_version & 0xff) << 16)
		| ((hw_id & 0xff) << 8) | (reg_version & 0xff);

	length = sizeof(vendor_id);
	snprintf(buff, length + 1, "%s", (u8 *)&vendor_id);
	snprintf(buff + length, sizeof(buff) - length, "%08X", version);
	snprintf(model, length + 1, "%s", (u8 *)&vendor_id);
	snprintf(model + length, sizeof(model) - length, "%04X", version >> 16);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };
	u16 checksum;

	sec_cmd_set_default_result(sec);

	read_data(client, ZT_CHECKSUM, (u8 *)&checksum, 2);

	snprintf(buff, sizeof(buff), "0x%X", checksum);
	input_info(true, &client->dev, "%s %d %x\n", __func__, checksum, checksum);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[20] = { 0 };

	sec_cmd_set_default_result(sec);

	read_data(client, ZT_THRESHOLD, (u8 *)&info->cap_info.threshold, 2);

	snprintf(buff, sizeof(buff), "%d", info->cap_info.threshold);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_module_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = {0};

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff),  "%s", tostring(NA));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", ZT_VENDOR_NAME);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct zt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	const char *name_buff;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (pdata->chip_name)
		name_buff = pdata->chip_name;
	else
		name_buff = ZT_CHIP_NAME;

	snprintf(buff, sizeof(buff), "%s", name_buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	mutex_lock(&info->work_lock);

	read_data(client, ZT_TOTAL_NUMBER_OF_X, (u8 *)&info->cap_info.x_node_num, 2);

	mutex_unlock(&info->work_lock);

	snprintf(buff, sizeof(buff), "%u", info->cap_info.x_node_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	mutex_lock(&info->work_lock);

	read_data(client, ZT_TOTAL_NUMBER_OF_Y, (u8 *)&info->cap_info.y_node_num, 2);

	mutex_unlock(&info->work_lock);

	snprintf(buff, sizeof(buff), "%u", info->cap_info.y_node_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->debug_flag = sec->cmd_param[0];

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s: \"%s(%d)\"\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static bool get_channel_test_result(struct zt_ts_info *info, int skip_cnt)
{
	struct i2c_client *client = info->client;
	struct sec_ts_plat_data *plat_data = info->plat_data;
	int i;
	int retry = 150;

	disable_irq(info->irq);

	mutex_lock(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_info(true, &client->dev, "other process occupied.. (%d)\n",
				info->work_state);
		enable_irq(info->irq);
		mutex_unlock(&info->work_lock);
		return false;
	}

	info->work_state = RAW_DATA;

	for (i = 0; i < skip_cnt; i++) {
		while (gpio_get_value(plat_data->irq_gpio)) {
			sec_delay(7);
			if (--retry < 0)
				break;
		}
		retry = 150;
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
		sec_delay(1);
	}

	retry = 100;
	input_info(true, &client->dev, "%s: channel_test_result read\n", __func__);

	while (gpio_get_value(plat_data->irq_gpio)) {
		sec_delay(30);
		if (--retry < 0)
			break;
		else
			input_info(true, &client->dev, "%s: retry:%d\n", __func__, retry);
	}

	read_data(info->client, REG_CHANNEL_TEST_RESULT, (u8 *)info->raw_data->channel_test_data, 10);

	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	clear_report_data(info);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	mutex_unlock(&info->work_lock);

	return true;
}

static void run_test_open_short(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;

	zt_ts_esd_timer_stop(info);
	ts_set_touchmode(info, TOUCH_CHANNEL_TEST_MODE);
	get_channel_test_result(info, 2);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	input_info(true, &client->dev, "channel_test_result : %04X\n", raw_data->channel_test_data[0]);
	input_info(true, &client->dev, "RX Channel : %08X\n",
			raw_data->channel_test_data[1] | ((raw_data->channel_test_data[2] << 16) & 0xffff0000));
	input_info(true, &client->dev, "TX Channel : %08X\n",
			raw_data->channel_test_data[3] | ((raw_data->channel_test_data[4] << 16) & 0xffff0000));

	if (raw_data->channel_test_data[0] == TEST_SHORT) {
		info->plat_data->hw_param.ito_test[3] |= 0x0F;
	} else if (raw_data->channel_test_data[0] == TEST_CHANNEL_OPEN ||
							raw_data->channel_test_data[0] == TEST_PATTERN_OPEN) {
		if (raw_data->channel_test_data[3] | ((raw_data->channel_test_data[4] << 16) & 0xffff0000))
			info->plat_data->hw_param.ito_test[3] |= 0x10;

		if (raw_data->channel_test_data[1] | ((raw_data->channel_test_data[2] << 16) & 0xffff0000))
			info->plat_data->hw_param.ito_test[3] |= 0x20;
	}

	zt_ts_esd_timer_start(info);
}

static void check_trx_channel_test(struct zt_ts_info *info, char *buf)
{
	struct tsp_raw_data *raw_data = info->raw_data;
	u8 temp[10];
	int ii;
	u32 test_result;

	memset(temp, 0x00, sizeof(temp));

	test_result = raw_data->channel_test_data[3] | ((raw_data->channel_test_data[4] << 16) & 0xffff0000);
	for (ii = 0; ii < info->cap_info.x_node_num; ii++) {
		if (test_result & (1 << ii)) {
			memset(temp, 0x00, 10);
			snprintf(temp, sizeof(temp), "T%d, ", ii);
			strlcat(buf, temp, SEC_CMD_STR_LEN);
		}
	}

	test_result = raw_data->channel_test_data[1] | ((raw_data->channel_test_data[2] << 16) & 0xffff0000);
	for (ii = 0; ii < info->cap_info.y_node_num; ii++) {
		if (test_result & (1 << ii)) {
			memset(temp, 0x00, 10);
			snprintf(temp, sizeof(temp), "R%d, ", ii);
			strlcat(buf, temp, SEC_CMD_STR_LEN);
		}
	}
}

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 temp[10];
	char test[32];

	sec_cmd_set_default_result(sec);

	if (info->plat_data->power_enabled == false) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (sec->cmd_param[1])
		snprintf(test, sizeof(test), "TEST=%d,%d", sec->cmd_param[0], sec->cmd_param[1]);
	else
		snprintf(test, sizeof(test), "TEST=%d", sec->cmd_param[0]);

	/*
	 * run_test_open_short() need to be fix for separate by test item(open, short, pattern open)
	 */
	if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 1)
		run_test_open_short(info);

	if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 1) {
		/* 1,1 : open  */
		if (raw_data->channel_test_data[0] != TEST_CHANNEL_OPEN)
			goto OK;

		memset(temp, 0x00, sizeof(temp));
		snprintf(temp, sizeof(temp), "OPEN: ");
		strlcat(buff, temp, SEC_CMD_STR_LEN);

		check_trx_channel_test(info, buff);
		input_info(true, &client->dev, "%s\n", buff);
	} else if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 2) {
		/* 1,2 : short  */
		if (raw_data->channel_test_data[0] != TEST_SHORT)
			goto OK;

		memset(temp, 0x00, sizeof(temp));
		snprintf(temp, sizeof(temp), "SHORT: ");
		strlcat(buff, temp, SEC_CMD_STR_LEN);

		check_trx_channel_test(info, buff);
		input_info(true, &client->dev, "%s\n", buff);
	} else if (sec->cmd_param[0] == 2) {
		/* 2 : micro open(pattern open)  */
		if (raw_data->channel_test_data[0] != TEST_PATTERN_OPEN)
			goto OK;

		memset(temp, 0x00, sizeof(temp));
		snprintf(temp, sizeof(temp), "CRACK: ");
		strlcat(buff, temp, SEC_CMD_STR_LEN);

		check_trx_channel_test(info, buff);
		input_info(true, &client->dev, "%s\n", buff);
	} else if (sec->cmd_param[0] == 3) {
		/* 3 : bridge short  */
		snprintf(buff, sizeof(buff), "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

		sec_cmd_send_event_to_user(sec, test, "RESULT=FAIL");

		input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));
		return;
	} else {
		/* 0 or else : old command */
		if (raw_data->channel_test_data[0] == TEST_PASS)
			goto OK;
	}

	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	sec_cmd_send_event_to_user(sec, test, "RESULT=FAIL");

	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));
	return;


OK:
	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_send_event_to_user(sec, test, "RESULT=PASS");

	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));
	return;

}

static void touch_aging_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (sec->cmd_param[0] == 1) {
		zt_ts_esd_timer_stop(info);
		ret = ts_set_touchmode(info, TOUCH_AGING_MODE);
	} else {
		ret = ts_set_touchmode(info, TOUCH_POINT_MODE);
		zt_ts_esd_timer_start(info);
	}
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_cnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 min, max;
	s32 i, j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(info, TOUCH_RAW_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->cnd_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	input_info(true, &client->dev, "%s: CND start\n", __func__);

	min = 0xFFFF;
	max = 0x0000;

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			if (raw_data->cnd_data[i * info->cap_info.x_node_num + j] < min &&
					raw_data->cnd_data[i * info->cap_info.x_node_num + j] != 0)
				min = raw_data->cnd_data[i * info->cap_info.x_node_num + j];

			if (raw_data->cnd_data[i * info->cap_info.x_node_num + j] > max)
				max = raw_data->cnd_data[i * info->cap_info.x_node_num + j];
		}
	}
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CND");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	zt_display_rawdata(info, raw_data, TOUCH_RAW_MODE, 0);
out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CND");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
	return;
}

static void run_cnd_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[16] = { 0 };
	char *all_cmdbuff;
	s32 i, j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(info, TOUCH_RAW_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->cnd_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	all_cmdbuff = kzalloc(info->cap_info.x_node_num * info->cap_info.y_node_num * 6, GFP_KERNEL);
	if (!all_cmdbuff) {
		input_info(true, &info->client->dev, "%s: alloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			sprintf(buff, "%u,", raw_data->cnd_data[i * info->cap_info.x_node_num + j]);
			strcat(all_cmdbuff, buff);
		}
	}

	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, sizeof(all_cmdbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(all_cmdbuff);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	zt_ts_esd_timer_start(info);
}

static void run_dnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 min, max;
	s32 i, j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(info, TOUCH_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->dnd_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	min = 0xFFFF;
	max = 0x0000;

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			if (raw_data->dnd_data[i * info->cap_info.x_node_num + j] < min &&
					raw_data->dnd_data[i * info->cap_info.x_node_num + j] != 0)
				min = raw_data->dnd_data[i * info->cap_info.x_node_num + j];

			if (raw_data->dnd_data[i * info->cap_info.x_node_num + j] > max)
				max = raw_data->dnd_data[i * info->cap_info.x_node_num + j];
		}
	}
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DND");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	zt_display_rawdata(info, raw_data, TOUCH_DND_MODE, 0);
out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DND");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
}

static void run_dnd_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[16] = { 0 };
	char *all_cmdbuff;
	s32 i, j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(info, TOUCH_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->dnd_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	all_cmdbuff = kzalloc(info->cap_info.x_node_num * info->cap_info.y_node_num * 6, GFP_KERNEL);
	if (!all_cmdbuff) {
		input_info(true, &info->client->dev, "%s: alloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			sprintf(buff, "%u,", raw_data->dnd_data[i * info->cap_info.x_node_num + j]);
			strcat(all_cmdbuff, buff);
		}
	}

	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, sizeof(all_cmdbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(all_cmdbuff);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	zt_ts_esd_timer_start(info);
}

static void run_dnd_v_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_1[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->vgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &client->dev, "%s: DND V Gap start\n", __func__);

	input_info(true, &client->dev, "%s : ++++++ DND SPEC +++++++++\n", __func__);
	for (i = 0; i < y_num - 1; i++) {
		for (j = 0; j < x_num; j++) {
			offset = (i * x_num) + j;

			cur_val = raw_data->dnd_data[offset];
			next_val = raw_data->dnd_data[offset + x_num];
			if (!next_val) {
				raw_data->vgap_data[offset] = next_val;
				continue;
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			raw_data->vgap_data[offset] = val;

			if (raw_data->vgap_data[i * x_num + j] > screen_max)
				screen_max = raw_data->vgap_data[i * x_num + j];
		}
	}

	input_info(true, &client->dev, "DND V Gap screen_max %d\n", screen_max);
	snprintf(buff, sizeof(buff), "%d", screen_max);
	snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "DND_V_GAP");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	zt_display_rawdata(info, raw_data, TOUCH_DND_MODE, 1);
}

static void run_dnd_h_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_1[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->hgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &client->dev, "%s: DND H Gap start\n", __func__);

	for (i = 0; i < y_num; i++) {
		for (j = 0; j < x_num - 1; j++) {
			offset = (i * x_num) + j;

			cur_val = raw_data->dnd_data[offset];
			if (!cur_val) {
				raw_data->hgap_data[offset] = cur_val;
				continue;
			}

			next_val = raw_data->dnd_data[offset + 1];
			if (!next_val) {
				raw_data->hgap_data[offset] = next_val;
				for (++j; j < x_num - 1; j++) {
					offset = (i * x_num) + j;

					next_val = raw_data->dnd_data[offset];
					if (!next_val) {
						raw_data->hgap_data[offset] = next_val;
						continue;
					}
					break;
				}
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			raw_data->hgap_data[offset] = val;

			if (raw_data->hgap_data[i * x_num + j] > screen_max)
				screen_max = raw_data->hgap_data[i * x_num + j];
		}
	}

	input_info(true, &client->dev, "DND H Gap screen_max %d\n", screen_max);
	snprintf(buff, sizeof(buff), "%d", screen_max);
	snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "DND_H_GAP");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	zt_display_rawdata(info, raw_data, TOUCH_DND_MODE, 2);
}

static void run_dnd_h_gap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num * info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num - 1; j++) {
			offset = (i * info->cap_info.x_node_num) + j;

			cur_val = raw_data->dnd_data[offset];
			if (!cur_val) {
				raw_data->hgap_data[offset] = cur_val;
				continue;
			}

			next_val = raw_data->dnd_data[offset + 1];
			if (!next_val) {
				raw_data->hgap_data[offset] = next_val;
				for (++j; j < info->cap_info.x_node_num - 1; j++) {
					offset = (i * info->cap_info.x_node_num) + j;

					next_val = raw_data->dnd_data[offset];
					if (!next_val) {
						raw_data->hgap_data[offset] = next_val;
						continue;
					}
					break;
				}
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			raw_data->hgap_data[offset] = val;

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->hgap_data[offset]);
			strncat(buff, temp, CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_dnd_v_gap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num * info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (i = 0; i < info->cap_info.y_node_num - 1; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			offset = (i * info->cap_info.x_node_num) + j;

			cur_val = raw_data->dnd_data[offset];
			next_val = raw_data->dnd_data[offset + info->cap_info.x_node_num];
			if (!next_val) {
				raw_data->vgap_data[offset] = next_val;
				continue;
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			raw_data->vgap_data[offset] = val;

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->vgap_data[offset]);
			strncat(buff, temp, CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_selfdnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	int total_node = info->cap_info.x_node_num + info->cap_info.y_node_num;
	char tx_buff[SEC_CMD_STR_LEN] = { 0 };
	char rx_buff[SEC_CMD_STR_LEN] = { 0 };
	u16 tx_min, tx_max, rx_min, rx_max;
	s32 j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(info, TOUCH_SELF_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->selfdnd_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	input_info(true, &client->dev, "%s: SELF DND start\n", __func__);

	tx_min = 0xFFFF;
	tx_max = 0x0000;
	rx_min = 0xFFFF;
	rx_max = 0x0000;

	for (j = 0; j < info->cap_info.x_node_num; j++) {
		if (raw_data->selfdnd_data[j] < rx_min && raw_data->selfdnd_data[j] != 0)
			rx_min = raw_data->selfdnd_data[j];

		if (raw_data->selfdnd_data[j] > rx_max)
			rx_max = raw_data->selfdnd_data[j];
	}

	for (j = info->cap_info.x_node_num; j < total_node; j++) {
		if (raw_data->selfdnd_data[j] < tx_min && raw_data->selfdnd_data[j] != 0)
			tx_min = raw_data->selfdnd_data[j];

		if (raw_data->selfdnd_data[j] > tx_max)
			tx_max = raw_data->selfdnd_data[j];
	}

	input_info(true, &client->dev, "%s: SELF DND Pass\n", __func__);

	snprintf(tx_buff, sizeof(tx_buff), "%d,%d", tx_min, tx_max);
	snprintf(rx_buff, sizeof(rx_buff), "%d,%d", rx_min, rx_max);
	sec_cmd_set_cmd_result(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)), "SELF_DND_RX");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(rx_buff, sizeof(rx_buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
			sec_cmd_set_cmd_result_all(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)), "SELF_DND_RX");
		}
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
}

static void run_charge_pump_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(info, TOUCH_CHARGE_PUMP_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->charge_pump_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	snprintf(buff, sizeof(buff), "%d,%d", raw_data->charge_pump_data[0], raw_data->charge_pump_data[0]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CHARGE_PUMP");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CHARGE_PUMP");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
}

static void run_selfdnd_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num + info->cap_info.y_node_num;
	s32 j;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ts_set_touchmode(info, TOUCH_SELF_DND_MODE);

	get_raw_data(info, (u8 *)raw_data->selfdnd_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto NG;

	for (j = 0; j < total_node; j++) {
		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->selfdnd_data[j]);
		strncat(buff, temp, CMD_RESULT_WORD_LEN);
		memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);

NG:
	if (sec->cmd_state != SEC_CMD_STATUS_OK) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	zt_ts_esd_timer_start(info);
}

static void run_self_saturation_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	int total_node = info->cap_info.x_node_num + info->cap_info.y_node_num;
	char tx_buff[SEC_CMD_STR_LEN] = { 0 };
	char rx_buff[SEC_CMD_STR_LEN] = { 0 };
	u16 tx_min, tx_max, rx_min, rx_max;
	s32 j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(info, DEF_RAW_SELF_SSR_DATA_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->ssr_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	input_info(true, &client->dev, "%s: SELF SATURATION start\n", __func__);

	tx_min = 0xFFFF;
	tx_max = 0x0000;
	rx_min = 0xFFFF;
	rx_max = 0x0000;

	for (j = 0; j < info->cap_info.x_node_num; j++) {
		if (raw_data->ssr_data[j] < rx_min && raw_data->ssr_data[j] != 0)
			rx_min = raw_data->ssr_data[j];

		if (raw_data->ssr_data[j] > rx_max)
			rx_max = raw_data->ssr_data[j];
	}

	for (j = info->cap_info.x_node_num; j < total_node; j++) {
		if (raw_data->ssr_data[j] < tx_min && raw_data->ssr_data[j] != 0)
			tx_min = raw_data->ssr_data[j];

		if (raw_data->ssr_data[j] > tx_max)
			tx_max = raw_data->ssr_data[j];
	}

	input_info(true, &client->dev, "%s: SELF SATURATION Pass\n", __func__);

	snprintf(tx_buff, sizeof(tx_buff), "%d,%d", tx_min, tx_max);
	snprintf(rx_buff, sizeof(rx_buff), "%d,%d", rx_min, rx_max);
	sec_cmd_set_cmd_result(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, tx_buff, strnlen(tx_buff, sizeof(tx_buff)), "SELF_SATURATION_TX");
		sec_cmd_set_cmd_result_all(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)), "SELF_SATURATION_RX");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(rx_buff, sizeof(rx_buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
			sec_cmd_set_cmd_result_all(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)), "SELF_SATURATION_TX");
			sec_cmd_set_cmd_result_all(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)), "SELF_SATURATION_RX");
		}
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
}

static void run_ssr_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[16] = { 0 };
	int total_node = info->cap_info.x_node_num + info->cap_info.y_node_num;
	char *all_cmdbuff;
	s32 j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(info, DEF_RAW_SELF_SSR_DATA_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->ssr_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	all_cmdbuff = kzalloc(total_node * 6, GFP_KERNEL);
	if (!all_cmdbuff) {
		input_info(true, &info->client->dev, "%s: alloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	for (j = 0; j < total_node; j++) {
		sprintf(buff, "%u,", raw_data->ssr_data[j]);
		strcat(all_cmdbuff, buff);
	}

	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, sizeof(all_cmdbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(all_cmdbuff);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	zt_ts_esd_timer_start(info);
}

static void run_selfdnd_h_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd[SEC_CMD_STR_LEN] = { 0 };
	int j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->self_hgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &client->dev, "%s: SELFDND H Gap start\n", __func__);

	for (j = 0; j < info->cap_info.x_node_num - 1; j++) {
		offset = j;
		cur_val = raw_data->selfdnd_data[offset];

		if (!cur_val) {
			raw_data->self_hgap_data[offset] = cur_val;
			continue;
		}

		next_val = raw_data->selfdnd_data[offset + 1];

		if (next_val > cur_val)
			val = 100 - ((cur_val * 100) / next_val);
		else
			val = 100 - ((next_val * 100) / cur_val);

		raw_data->self_hgap_data[offset] = val;

		if (raw_data->self_hgap_data[j] > screen_max)
			screen_max = raw_data->self_hgap_data[j];

	}

	input_info(true, &client->dev, "SELFDND H Gap screen_max %d\n", screen_max);
	snprintf(buff, sizeof(buff), "%d", screen_max);
	snprintf(buff_onecmd, sizeof(buff_onecmd), "%d,%d", 0, screen_max);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff_onecmd, strnlen(buff_onecmd, sizeof(buff_onecmd)), "SELF_DND_H_GAP");
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void run_selfdnd_h_gap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.y_node_num;
	int j, offset, val, cur_val, next_val;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (j = 0; j < info->cap_info.x_node_num - 1; j++) {
		offset = j;
		cur_val = raw_data->selfdnd_data[offset];

		if (!cur_val) {
			raw_data->self_hgap_data[offset] = cur_val;
			continue;
		}

		next_val = raw_data->selfdnd_data[offset + 1];

		if (next_val > cur_val)
			val = 100 - ((cur_val * 100) / next_val);
		else
			val = 100 - ((next_val * 100) / cur_val);

		raw_data->self_hgap_data[offset] = val;

		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->self_hgap_data[offset]);
		strncat(buff, temp, CMD_RESULT_WORD_LEN);
		memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_tsp_rawdata_read(void *device_data, u16 rawdata_mode, s16 *buff)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, ret;

	zt_ts_esd_timer_stop(info);
	ret = ts_set_touchmode(info, rawdata_mode);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)buff, 2);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	input_info(true, &info->client->dev, "touch rawdata %d start\n", rawdata_mode);

	for (i = 0; i < y_num; i++) {
		pr_info("%s [%5d] :", SECLOG, i);
		for (j = 0; j < x_num; j++)
			pr_cont("%06d ", buff[(i * x_num) + j]);

		pr_cont("\n");
	}
out:
	zt_ts_esd_timer_start(info);
}

static void run_interrupt_gpio_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int drive_value = 1;
	int irq_value = 0;
	int ret;

	sec_cmd_set_default_result(sec);

	if (info->plat_data->power_enabled == false) {
		input_err(true, &client->dev,
				"%s TSP power off\n", __func__);
		return;
	}

	disable_irq(info->irq);

	write_reg(client, 0x0A, 0x0A);

#if ESD_TIMER_INTERVAL
	zt_ts_esd_timer_stop(info);
	write_reg(client, ZT_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	usleep_range(1 * 1000, 1 * 1000);
#endif

	ret = ts_set_touchmode(info, TOUCH_NO_OPERATION_MODE);
	if (ret < 0)
		goto out;

	write_reg(info->client, 0x0A, 0x0A);
	write_cmd(client, ZT_DRIVE_INT_STATUS_CMD);
	usleep_range(20 * 1000, 21 * 1000);

	drive_value = gpio_get_value(info->plat_data->irq_gpio);
	input_info(true, &client->dev, "%s: ZT75XX_DRIVE_INT_STATUS_CMD: interrupt gpio: %d\n", __func__, drive_value);

	write_reg(info->client, 0x0A, 0x0A);
	write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	usleep_range(20 * 1000, 21 * 1000);

	irq_value = gpio_get_value(info->plat_data->irq_gpio);
	input_info(true, &client->dev, "%s: ZT75XX_CLEAR_INT_STATUS_CMD: interrupt gpio : %d\n", __func__, irq_value);

	write_cmd(info->client, 0x0B);
	ret = ts_set_touchmode(info, TOUCH_POINT_MODE);
	if (ret < 0)
		goto out;
out:
	if (drive_value == 0 && irq_value == 1) {
		snprintf(buff, sizeof(buff), "%s", "0");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		if (drive_value != 0)
			snprintf(buff, sizeof(buff), "%s", "1:HIGH");
		else if (irq_value != 1)
			snprintf(buff, sizeof(buff), "%s", "1:LOW");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "INT_GPIO");

	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));

	enable_irq(info->irq);

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

/*
## Mis Cal result ##
FD : spec out
F3,F4 : i2c faile
F2 : power off state
F1 : not support mis cal concept
F0 : initial value in function
00 : pass
 */

static void run_mis_cal_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct zt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset;
	char mis_cal_data = 0xF0;
	int ret = 0;
	s16 raw_data_buff[TSP_CMD_NODE_NUM];
	u16 chip_eeprom_info;
	int min = 0xFFFF, max = -0xFF;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	if (pdata->mis_cal_check == 0) {
		input_info(true, &info->client->dev, "%s: [ERROR] not support\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	if (info->work_state == SUSPEND) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		mis_cal_data = 0xF2;
		goto NG;
	}

	if (read_data(info->client, ZT_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0) {
		input_info(true, &info->client->dev, "%s: read eeprom_info i2c fail!\n", __func__);
		mis_cal_data = 0xF3;
		goto NG;
	}

	if (zinitix_bit_test(chip_eeprom_info, 0)) {
		input_info(true, &info->client->dev, "%s: eeprom cal 0, skip !\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	ret = ts_set_touchmode(info, TOUCH_REF_ABNORMAL_TEST_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ret = get_raw_data(info, (u8 *)raw_data->reference_data_abnormal, 2);
	if (!ret) {
		input_info(true, &info->client->dev, "%s:[ERROR] i2c fail!\n", __func__);
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	input_info(true, &info->client->dev, "%s start\n", __func__);

	ret = 1;
	for (i = 0; i < y_num; i++) {
		for (j = 0; j < x_num; j++) {
			offset = (i * x_num) + j;

			if (raw_data->reference_data_abnormal[offset] < min)
				min = raw_data->reference_data_abnormal[offset];

			if (raw_data->reference_data_abnormal[offset] > max)
				max = raw_data->reference_data_abnormal[offset];
		}
	}
	if (!ret)
		goto NG;

	mis_cal_data = 0x00;
	snprintf(buff, sizeof(buff), "%d,%d", min, max);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	zt_display_rawdata(info, raw_data, TOUCH_REF_ABNORMAL_TEST_MODE, 0);

	return;
NG:
	snprintf(buff, sizeof(buff), "%s_%d", "NG", mis_cal_data);

	if (mis_cal_data == 0xFD) {
		run_tsp_rawdata_read(device_data, 7, raw_data_buff);
		run_tsp_rawdata_read(device_data, TOUCH_REFERENCE_MODE, raw_data_buff);
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
}

static void run_mis_cal_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num * info->cap_info.y_node_num;
	int i, j, offset;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	sec_cmd_set_default_result(sec);

	ts_set_touchmode(info, TOUCH_POINT_MODE);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto NG;

	for (i = 0; i < info->cap_info.y_node_num ; i++) {
		for (j = 0; j < info->cap_info.x_node_num ; j++) {
			offset = (i * info->cap_info.x_node_num) + j;
			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->reference_data_abnormal[offset]);
			strncat(buff, temp, CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);

NG:
	if (sec->cmd_state != SEC_CMD_STATUS_OK) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
}

static void run_jitter_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tbuff[2] = { 0 };
	int ret = 0;
	int result = -1;
	short jitter_max, jitter_min;
	short min_of_min = 0;
	short max_of_min = 0;
	short min_of_max = 0;
	short max_of_max = 0;
	short min_of_avg = 0;
	short max_of_avg = 0;
	u8 test_step = 0;

	disable_irq(info->irq);
	zt_ts_esd_timer_stop(info);

	sec_cmd_set_default_result(sec);

	if (info->work_state == SUSPEND) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		goto OUT;
	}

	for (test_step = 0; test_step < 3; test_step++) {
		if (write_reg(client, ZT_JITTER_TEST_STEP, test_step) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed set jitter test\n", __func__);
			goto OUT;
		}

		if (write_reg(client, ZT_JITTER_SAMPLING_CNT, 0x0064) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed SAMPLING_CNT\n", __func__);
			goto OUT;
		}

		ret = ts_set_touchmode(info, TOUCH_JITTER_MODE);
		if (ret < 0) {
			ts_set_touchmode(info, TOUCH_POINT_MODE);
			goto OUT;
		}

		sec_delay(1500);

		ret = read_data(client, ZT_JITTER_RESULT_MAX, tbuff, 2);
		if (ret < 0) {
			ts_set_touchmode(info, TOUCH_POINT_MODE);
			input_err(true, &client->dev, "%s: fail read jitter data\n", __func__);
			goto OUT;
		}

		jitter_max = tbuff[1] << 8 | tbuff[0];

		ret = read_data(client, ZT_JITTER_RESULT_MIN, tbuff, 2);
		if (ret < 0) {
			ts_set_touchmode(info, TOUCH_POINT_MODE);
			input_err(true, &client->dev, "%s: fail read jitter data\n", __func__);
			goto OUT;
		}

		jitter_min = tbuff[1] << 8 | tbuff[0];

		if (test_step == 0) {
			max_of_max = jitter_max;
			min_of_max = jitter_min;
		} else if (test_step == 1) {
			max_of_min = jitter_max;
			min_of_min = jitter_min;
		} else {
			max_of_avg = jitter_max;
			min_of_avg = jitter_min;
		}
	}
	ts_set_touchmode(info, TOUCH_POINT_MODE);
	result = 0;

OUT:
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	if (result < 0)
		snprintf(buff, sizeof(buff), "NG");
	else
		snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d,%d",
				min_of_min, max_of_min, min_of_max,
				max_of_max, min_of_avg, max_of_avg);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		char buffer[SEC_CMD_STR_LEN] = { 0 };

		snprintf(buffer, sizeof(buffer), "%d,%d", min_of_min, max_of_min);
		sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_MIN");

		memset(buffer, 0x00, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%d,%d", min_of_max, max_of_max);
		sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_MAX");

		memset(buffer, 0x00, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%d,%d", min_of_avg, max_of_avg);
		sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_AVG");
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s\n", __func__, buff);
}


static void run_factory_miscalibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct zt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tbuff[2] = { 0 };
	char mis_cal_data = 0xF0;
	int ret = 0;
	u16 chip_eeprom_info;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	if (pdata->mis_cal_check == 0) {
		input_info(true, &info->client->dev, "%s: [ERROR] not support\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	if (info->work_state == SUSPEND) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		mis_cal_data = 0xF2;
		goto NG;
	}

	if (read_data(info->client, ZT_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0) {
		input_info(true, &info->client->dev, "%s: read eeprom_info i2c fail!\n", __func__);
		mis_cal_data = 0xF3;
		goto NG;
	}

	if (zinitix_bit_test(chip_eeprom_info, 0)) {
		input_info(true, &info->client->dev, "%s: eeprom cal 0, skip !\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	ret = ts_set_touchmode(info, TOUCH_REF_ABNORMAL_TEST_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}

	ret = write_cmd(client, ZT_MIS_CAL_SET);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed mis_cal_set\n", __func__);
		mis_cal_data = 0xF4;
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto NG;
	}

	sec_delay(600);

	ret = read_data(client, ZT_MIS_CAL_RESULT, tbuff, 2);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: fail fw_minor_version\n", __func__);
		mis_cal_data = 0xF4;
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto NG;
	}

	ts_set_touchmode(info, TOUCH_POINT_MODE);

	input_info(true, &info->client->dev, "%s start\n", __func__);

	if (tbuff[1] == 0x55) {
		snprintf(buff, sizeof(buff), "OK,0,%d", tbuff[0]);
	} else if (tbuff[1] == 0xAA) {
		snprintf(buff, sizeof(buff), "NG,0,%d", tbuff[0]);
	} else {
		input_err(true, &client->dev, "%s: failed tbuff[1]:0x%x, tbuff[0]:0x%x\n",
				__func__, tbuff[1], tbuff[0]);
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto NG;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	return;
NG:
	snprintf(buff, sizeof(buff), "%s_%d", "NG", mis_cal_data);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
}

#ifdef TCLM_CONCEPT
static void get_pat_information(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[50] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "C%02XT%04X.%4s%s%c%d%c%d%c%d",
			info->tdata->nvdata.cal_count, info->tdata->nvdata.tune_fix_ver, info->tdata->tclm_string[info->tdata->nvdata.cal_position].f_name,
			(info->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L " : " ",
			info->tdata->cal_pos_hist_last3[0], info->tdata->cal_pos_hist_last3[1],
			info->tdata->cal_pos_hist_last3[2], info->tdata->cal_pos_hist_last3[3],
			info->tdata->cal_pos_hist_last3[4], info->tdata->cal_pos_hist_last3[5]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

/* FACTORY TEST RESULT SAVING FUNCTION
 * bit 3 ~ 0 : OCTA Assy
 * bit 7 ~ 4 : OCTA module
 * param[0] : OCTA module(1) / OCTA Assy(2)
 * param[1] : TEST NONE(0) / TEST FAIL(1) / TEST PASS(2) : 2 bit
 */
static void get_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char cbuff[SEC_CMD_STR_LEN] = { 0 };
	u8 buff[2] = {0};

	sec_cmd_set_default_result(sec);

	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_FAC_RESULT, (u8 *)buff, 2);
	info->test_result.data[0] = buff[0];

	input_info(true, &info->client->dev, "%s : %X", __func__, info->test_result.data[0]);

	if (info->test_result.data[0] == 0xFF) {
		input_info(true, &info->client->dev, "%s: clear factory_result as zero\n", __func__);
		info->test_result.data[0] = 0;
	}

	snprintf(cbuff, sizeof(cbuff), "M:%s, M:%d, A:%s, A:%d",
			info->test_result.module_result == 0 ? "NONE" :
			info->test_result.module_result == 1 ? "FAIL" : "PASS",
			info->test_result.module_count,
			info->test_result.assy_result == 0 ? "NONE" :
			info->test_result.assy_result == 1 ? "FAIL" : "PASS",
			info->test_result.assy_count);

	sec_cmd_set_cmd_result(sec, cbuff, strnlen(cbuff, sizeof(cbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void set_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char cbuff[SEC_CMD_STR_LEN] = { 0 };
	u8 buff[2] = {0};

	sec_cmd_set_default_result(sec);

	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_FAC_RESULT, (u8 *)buff, 2);
	info->test_result.data[0] = buff[0];

	input_info(true, &info->client->dev, "%s : %X", __func__, info->test_result.data[0]);

	if (info->test_result.data[0] == 0xFF) {
		input_info(true, &info->client->dev, "%s: clear factory_result as zero\n", __func__);
		info->test_result.data[0] = 0;
	}

	if (sec->cmd_param[0] == TEST_OCTA_ASSAY) {
		info->test_result.assy_result = sec->cmd_param[1];
		if (info->test_result.assy_count < 3)
			info->test_result.assy_count++;

	} else if (sec->cmd_param[0] == TEST_OCTA_MODULE) {
		info->test_result.module_result = sec->cmd_param[1];
		if (info->test_result.module_count < 3)
			info->test_result.module_count++;
	}

	input_info(true, &info->client->dev, "%s: [0x%X] M:%s, M:%d, A:%s, A:%d\n",
			__func__, info->test_result.data[0],
			info->test_result.module_result == 0 ? "NONE" :
			info->test_result.module_result == 1 ? "FAIL" : "PASS",
			info->test_result.module_count,
			info->test_result.assy_result == 0 ? "NONE" :
			info->test_result.assy_result == 1 ? "FAIL" : "PASS",
			info->test_result.assy_count);

	set_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_FAC_RESULT, &info->test_result.data[0], 1);

	snprintf(cbuff, sizeof(cbuff), "OK");
	sec_cmd_set_cmd_result(sec, cbuff, strnlen(cbuff, sizeof(cbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void increase_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 count[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->plat_data->power_enabled == false) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);
	input_info(true, &info->client->dev, "%s: current disassemble count: %d\n", __func__, count[0]);

	if (count[0] == 0xFF)
		count[0] = 0;
	if (count[0] < 0xFE)
		count[0]++;

	set_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);

	sec_delay(5);

	memset(count, 0x00, 2);
	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);
	input_info(true, &info->client->dev, "%s: check disassemble count: %d\n", __func__, count[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

}

static void get_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 count[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->plat_data->power_enabled == false) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);
	if (count[0] == 0xFF) {
		count[0] = 0;
		count[1] = 0;
		set_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);
	}

	input_info(true, &info->client->dev, "%s: read disassemble count: %d\n", __func__, count[0]);
	snprintf(buff, sizeof(buff), "%d", count[0]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

int get_zt_tsp_nvm_data(struct zt_ts_info *info, u8 addr, u8 *values, u16 length)
{
	struct i2c_client *client = info->client;
	u16 buff_start;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	if (write_reg(client, ZT_POWER_STATE_FLAG, 1) != I2C_SUCCESS) {
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG, 1\n", __func__);
	}
	sec_delay(10);

	if (write_cmd(client, DEF_IUM_LOCK) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed ium lock\n", __func__);
		goto fail_ium_random_read;
	}
	sec_delay(40);

	buff_start = addr;	//custom setting address(0~62, 0,2,4,6)
	//length = 2;		// custom setting(max 64)
	if (length > TC_NVM_SECTOR_SZ)
		length = TC_NVM_SECTOR_SZ;
	if (length < 2) {
		length = 2;	//read 2byte
	}

	if (read_raw_data(client, buff_start + DEF_IUM_ADDR_OFFSET,
				values, length) < 0) {
		input_err(true, &client->dev, "Failed to read raw data %d\n", length);
		goto fail_ium_random_read;
	}

	if (write_cmd(client, DEF_IUM_UNLOCK) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed ium unlock\n", __func__);
		goto fail_ium_random_read;
	}

	if (write_reg(client, ZT_POWER_STATE_FLAG, 0) != I2C_SUCCESS) {
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG, 0\n", __func__);
	}
	sec_delay(10);

	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return 0;

fail_ium_random_read:

	zt_power_control(info, false);
	zt_power_control(info, true);

	mini_init_touch(info);

	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return -1;
}

int set_zt_tsp_nvm_data(struct zt_ts_info *info, u8 addr, u8 *values, u16 length)
{
	struct i2c_client *client = info->client;
	u8 buff[64];
	u16 buff_start;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	if (write_reg(client, ZT_POWER_STATE_FLAG, 1) != I2C_SUCCESS) {
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG, 1\n", __func__);
	}
	sec_delay(10);

	if (write_cmd(client, DEF_IUM_LOCK) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed ium lock\n", __func__);
		goto fail_ium_random_write;
	}
	sec_delay(40);

	buff_start = addr;	//custom setting address(0~62, 0,2,4,6)

	memcpy((u8 *)&buff[buff_start], values, length);

	/* data write start */
	if (length > TC_NVM_SECTOR_SZ)
		length = TC_NVM_SECTOR_SZ;
	if (length < 2) {
		length = 2;	//write 2byte
		buff[buff_start+1] = 0;
	}

	if (write_data(client, buff_start + DEF_IUM_ADDR_OFFSET,
				(u8 *)&buff[buff_start], length) < 0) {
		input_err(true, &client->dev, "%s: error : write zinitix tc firmware\n", __func__);
		goto fail_ium_random_write;
	}
	/* data write end */

	/* for save rom start */
	if (write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to write nvm wp disable\n", __func__);
		goto fail_ium_random_write;
	}
	sec_delay(10);

	if (write_cmd(client, DEF_IUM_SAVE_CMD) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed save ium\n", __func__);
		goto fail_ium_random_write;
	}
	sec_delay(30);

	if (write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: nvm wp enable\n", __func__);
		goto fail_ium_random_write;
	}
	sec_delay(10);
	/* for save rom end */

	if (write_cmd(client, DEF_IUM_UNLOCK) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed ium unlock\n", __func__);
		goto fail_ium_random_write;
	}

	if (write_reg(client, ZT_POWER_STATE_FLAG, 0) != I2C_SUCCESS) {
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG, 0\n", __func__);
	}
	sec_delay(10);

	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return 0;

fail_ium_random_write:

	if (write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: nvm wp enable\n", __func__);
	}
	sec_delay(10);

	zt_power_control(info, false);
	zt_power_control(info, true);

	mini_init_touch(info);

	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return -1;
}

int zt_tclm_data_read(struct device *dev, int address)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);
	int i, ret = 0;
	u8 nbuff[ZT_TS_NVM_OFFSET_LENGTH];

	switch (address) {
	case SEC_TCLM_NVM_OFFSET_IC_FIRMWARE_VER:
		ret = ic_version_check(info);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: fail to version check\n", __func__);
			return ret;
		}

		return ((info->cap_info.hw_id & 0xff) << 8) | (info->cap_info.reg_data_version & 0xff);

	case SEC_TCLM_NVM_ALL_DATA:
		/* Zinitx driver support index read/write so do not need read FAC_RESULT and DISASSEMBLE_COUNT here
		 * length minus the first 4 bytes
		 */
		ret = get_zt_tsp_nvm_data(info, 4, &nbuff[4], ZT_TS_NVM_OFFSET_LENGTH - 4);
		if (ret < 0)
			return ret;

		info->tdata->nvdata.cal_count = nbuff[ZT_TS_NVM_OFFSET_CAL_COUNT];
		info->tdata->nvdata.tune_fix_ver = (nbuff[ZT_TS_NVM_OFFSET_TUNE_VERSION] << 8) | nbuff[ZT_TS_NVM_OFFSET_TUNE_VERSION + 1];
		info->tdata->nvdata.cal_position = nbuff[ZT_TS_NVM_OFFSET_CAL_POSITION];
		info->tdata->nvdata.cal_pos_hist_cnt = nbuff[ZT_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT];
		info->tdata->nvdata.cal_pos_hist_lastp = nbuff[ZT_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP];
		for (i = ZT_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO; i < ZT_TS_NVM_OFFSET_LENGTH; i++)
			info->tdata->nvdata.cal_pos_hist_queue[i - ZT_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO] = nbuff[i];

		input_err(true, &info->client->dev, "%s: %d %X %x %d %d\n", __func__,
				info->tdata->nvdata.cal_count, info->tdata->nvdata.tune_fix_ver, info->tdata->nvdata.cal_position,
				info->tdata->nvdata.cal_pos_hist_cnt, info->tdata->nvdata.cal_pos_hist_lastp);

		return ret;
	default:
		return ret;
	}
}

int zt_tclm_data_write(struct device *dev, int address)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);
	int i, ret = 1;
	u8 nbuff[ZT_TS_NVM_OFFSET_LENGTH];

	memset(&nbuff[4], 0x00, ZT_TS_NVM_OFFSET_LENGTH - 4);

	nbuff[ZT_TS_NVM_OFFSET_CAL_COUNT] = info->tdata->nvdata.cal_count;
	nbuff[ZT_TS_NVM_OFFSET_TUNE_VERSION] = (u8)(info->tdata->nvdata.tune_fix_ver >> 8);
	nbuff[ZT_TS_NVM_OFFSET_TUNE_VERSION + 1] = (u8)(0xff & info->tdata->nvdata.tune_fix_ver);
	nbuff[ZT_TS_NVM_OFFSET_CAL_POSITION] = info->tdata->nvdata.cal_position;
	nbuff[ZT_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT] = info->tdata->nvdata.cal_pos_hist_cnt;
	nbuff[ZT_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP] = info->tdata->nvdata.cal_pos_hist_lastp;
	for (i = ZT_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO; i < ZT_TS_NVM_OFFSET_LENGTH; i++)
		nbuff[i] = info->tdata->nvdata.cal_pos_hist_queue[i - ZT_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO];

	ret = set_zt_tsp_nvm_data(info, 4, &nbuff[4], ZT_TS_NVM_OFFSET_LENGTH - 4);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: [ERROR] set_tsp_nvm_data ret:%d\n", __func__, ret);
	}

	return ret;
}
#endif

/*
 *	flag     1  :  set edge handler
 *		2  :  set (portrait, normal) edge zone data
 *		4  :  set (portrait, normal) dead zone data
 *		8  :  set landscape mode data
 *		16 :  mode clear
 *	data
 *		0xAA, FFF (y start), FFF (y end),  FF(direction)
 *		0xAB, FFFF (edge zone)
 *		0xAC, FF (up x), FF (down x), FFFF (y)
 *		0xAD, FF (mode), FFF (edge), FFF (dead zone x), FF (dead zone top y), FF (dead zone bottom y)
 *	case
 *		edge handler set :  0xAA....
 *		booting time :  0xAA...  + 0xAB...
 *		normal mode : 0xAC...  (+0xAB...)
 *		landscape mode : 0xAD...
 *		landscape -> normal (if same with old data) : 0xAD, 0
 *		landscape -> normal (etc) : 0xAC....  + 0xAD, 0
 */

static void set_grip_data_to_ic(struct zt_ts_info *ts, u8 flag)
{
	struct i2c_client *client = ts->client;

	input_info(true, &ts->client->dev, "%s: flag: %02X (clr,lan,nor,edg,han)\n", __func__, flag);

	mutex_lock(&ts->power_init);

	if (flag & G_SET_EDGE_HANDLER) {
		if (ts->plat_data->grip_data.edgehandler_direction == 0) {
			ts->plat_data->grip_data.edgehandler_start_y = 0x0;
			ts->plat_data->grip_data.edgehandler_end_y = 0x0;
		}

		if (write_reg(client, ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_START,
					ts->plat_data->grip_data.edgehandler_start_y) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set except start y error\n", __func__);
		}

		if (write_reg(client, ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_END,
					ts->plat_data->grip_data.edgehandler_end_y) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set except end y error\n", __func__);
		}

		if (write_reg(client, ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_SEL,
					(ts->plat_data->grip_data.edgehandler_direction) & 0x0003) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set except direct error\n", __func__);
		}

		input_info(true, &ts->client->dev, "%s: 0x%02X %02X, 0x%02X %02X, 0x%02X %02X\n", __func__,
				ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_SEL, ts->plat_data->grip_data.edgehandler_direction,
				ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_START, ts->plat_data->grip_data.edgehandler_start_y,
				ZT_EDGE_REJECT_PORT_EDGE_EXCEPT_END, ts->plat_data->grip_data.edgehandler_end_y);
	}

	if (flag & G_SET_EDGE_ZONE) {
		if (write_reg(client, ZT_EDGE_GRIP_PORT_SIDE_WIDTH,
				ts->plat_data->grip_data.edge_range) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: set grip side width error\n", __func__);
		}

		input_info(true, &ts->client->dev, "%s: 0x%02X %02X\n", __func__,
				ZT_EDGE_GRIP_PORT_SIDE_WIDTH, ts->plat_data->grip_data.edge_range);
	}

	if (flag & G_SET_NORMAL_MODE) {
		if (write_reg(client, ZT_EDGE_REJECT_PORT_SIDE_UP_WIDTH,
				ts->plat_data->grip_data.deadzone_up_x) != I2C_SUCCESS)
			input_err(true, &client->dev, "%s: set dead zone up x error\n", __func__);

		if (write_reg(client, ZT_EDGE_REJECT_PORT_SIDE_DOWN_WIDTH,
				ts->plat_data->grip_data.deadzone_dn_x) != I2C_SUCCESS)
			input_err(true, &client->dev, "%s: set dead zone down x error\n", __func__);

		if (write_reg(client, ZT_EDGE_REJECT_PORT_SIDE_UP_DOWN_DIV,
				ts->plat_data->grip_data.deadzone_y) != I2C_SUCCESS)
			input_err(true, &client->dev,
				"%s: set dead zone up/down div location error\n", __func__);

		input_info(true, &ts->client->dev, "%s: 0x%02X %02X, 0x%02X %02X, 0x%02X %02X\n", __func__,
				ZT_EDGE_REJECT_PORT_SIDE_UP_WIDTH, ts->plat_data->grip_data.deadzone_up_x,
				ZT_EDGE_REJECT_PORT_SIDE_DOWN_WIDTH, ts->plat_data->grip_data.deadzone_dn_x,
				ZT_EDGE_REJECT_PORT_SIDE_UP_DOWN_DIV, ts->plat_data->grip_data.deadzone_y);
	}

	if (flag & G_SET_LANDSCAPE_MODE) {
		if (write_reg(client, ZT_EDGE_LANDSCAPE_MODE,
				ts->plat_data->grip_data.landscape_mode & 0x1) != I2C_SUCCESS)
			input_err(true, &client->dev, "%s: set landscape mode error\n", __func__);

		if (write_reg(client, ZT_EDGE_GRIP_LAND_SIDE_WIDTH,
				ts->plat_data->grip_data.landscape_edge) != I2C_SUCCESS)
			input_err(true, &client->dev, "%s: set landscape side edge error\n", __func__);

		if (write_reg(client, ZT_EDGE_REJECT_LAND_SIDE_WIDTH,
				ts->plat_data->grip_data.landscape_deadzone) != I2C_SUCCESS)
			input_err(true, &client->dev, "%s: set landscape side deadzone error\n", __func__);

		if (write_reg(client, ZT_EDGE_REJECT_LAND_TOP_BOT_WIDTH,
				(((ts->plat_data->grip_data.landscape_top_deadzone << 8) & 0xFF00) |
				(ts->plat_data->grip_data.landscape_bottom_deadzone & 0x00FF))) != I2C_SUCCESS)
			input_err(true, &client->dev, "%s: set landscape top bot deazone error\n", __func__);


		if (write_reg(client, ZT_EDGE_GRIP_LAND_TOP_BOT_WIDTH,
				(((ts->plat_data->grip_data.landscape_top_gripzone << 8) & 0xFF00) |
				(ts->plat_data->grip_data.landscape_bottom_gripzone & 0x00FF))) != I2C_SUCCESS)
			input_err(true, &client->dev, "%s: set landscape top bot gripzone error\n", __func__);

		input_info(true, &ts->client->dev,
				"%s: 0x%02X %02X, 0x%02X %02X, 0x%02X %02X, 0x%02X %02X, 0x%02X %02X\n", __func__,
				ZT_EDGE_LANDSCAPE_MODE, ts->plat_data->grip_data.landscape_mode & 0x1,
				ZT_EDGE_GRIP_LAND_SIDE_WIDTH, ts->plat_data->grip_data.landscape_edge,
				ZT_EDGE_REJECT_LAND_SIDE_WIDTH, ts->plat_data->grip_data.landscape_deadzone,
				ZT_EDGE_REJECT_LAND_TOP_BOT_WIDTH,
				((ts->plat_data->grip_data.landscape_top_deadzone << 8) & 0xFF00) |
				(ts->plat_data->grip_data.landscape_bottom_deadzone & 0x00FF),
				ZT_EDGE_GRIP_LAND_TOP_BOT_WIDTH,
				((ts->plat_data->grip_data.landscape_top_gripzone << 8) & 0xFF00) |
				(ts->plat_data->grip_data.landscape_bottom_gripzone & 0x00FF));
	}

	if (flag & G_CLR_LANDSCAPE_MODE) {
		if (write_reg(client, ZT_EDGE_LANDSCAPE_MODE,
				ts->plat_data->grip_data.landscape_mode) != I2C_SUCCESS)
			input_err(true, &client->dev, "%s: clr landscape mode error\n", __func__);


		input_info(true, &ts->client->dev, "%s: 0x%02X %02X\n", __func__,
				ZT_EDGE_LANDSCAPE_MODE, ts->plat_data->grip_data.landscape_mode);
	}
	mutex_unlock(&ts->power_init);
}

/*
 *	index  0 :  set edge handler
 *		1 :  portrait (normal) mode
 *		2 :  landscape mode
 *
 *	data
 *		0, X (direction), X (y start), X (y end)
 *		direction : 0 (off), 1 (left), 2 (right)
 *			ex) echo set_grip_data,0,2,600,900 > cmd
 *
 *		1, X (edge zone), X (dead zone up x), X (dead zone down x), X (dead zone y)
 *			ex) echo set_grip_data,1,200,10,50,1500 > cmd
 *
 *		2, 1 (landscape mode), X (edge zone), X (dead zone x), X (dead zone top y), X (dead zone bottom y), X (edge zone top y), X (edge zone bottom y)
 *			ex) echo set_grip_data,2,1,200,100,120,0 > cmd
 *
 *		2, 0 (portrait mode)
 *			ex) echo set_grip_data,2,0  > cmd
 */

static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *ts = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode = G_NONE;

	sec_cmd_set_default_result(sec);

	memset(buff, 0, sizeof(buff));

	if (sec->cmd_param[0] == 0) {	// edge handler
		if (sec->cmd_param[1] == 0) {	// clear
			ts->plat_data->grip_data.edgehandler_direction = 0;
		} else if (sec->cmd_param[1] < 3) {
			ts->plat_data->grip_data.edgehandler_direction = sec->cmd_param[1];
			ts->plat_data->grip_data.edgehandler_start_y = sec->cmd_param[2];
			ts->plat_data->grip_data.edgehandler_end_y = sec->cmd_param[3];
		} else {
			input_err(true, &ts->client->dev, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}

		mode = mode | G_SET_EDGE_HANDLER;
		set_grip_data_to_ic(ts, mode);
	} else if (sec->cmd_param[0] == 1) {	// normal mode
		if (ts->plat_data->grip_data.edge_range != sec->cmd_param[1])
			mode = mode | G_SET_EDGE_ZONE;

		ts->plat_data->grip_data.edge_range = sec->cmd_param[1];
		ts->plat_data->grip_data.deadzone_up_x = sec->cmd_param[2];
		ts->plat_data->grip_data.deadzone_dn_x = sec->cmd_param[3];
		ts->plat_data->grip_data.deadzone_y = sec->cmd_param[4];
		mode = mode | G_SET_NORMAL_MODE;

		if (ts->plat_data->grip_data.landscape_mode == 1) {
			ts->plat_data->grip_data.landscape_mode = 0;
			mode = mode | G_CLR_LANDSCAPE_MODE;
		}
		set_grip_data_to_ic(ts, mode);
	} else if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {	// normal mode
			ts->plat_data->grip_data.landscape_mode = 0;
			mode = mode | G_CLR_LANDSCAPE_MODE;
		} else if (sec->cmd_param[1] == 1) {
			ts->plat_data->grip_data.landscape_mode = 1;
			ts->plat_data->grip_data.landscape_edge = sec->cmd_param[2];
			ts->plat_data->grip_data.landscape_deadzone	= sec->cmd_param[3];
			ts->plat_data->grip_data.landscape_top_deadzone = sec->cmd_param[4];
			ts->plat_data->grip_data.landscape_bottom_deadzone = sec->cmd_param[5];
			ts->plat_data->grip_data.landscape_top_gripzone = sec->cmd_param[6];
			ts->plat_data->grip_data.landscape_bottom_gripzone = sec->cmd_param[7];
			mode = mode | G_SET_LANDSCAPE_MODE;
		} else {
			input_err(true, &ts->client->dev, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}
		set_grip_data_to_ic(ts, mode);
	} else {
		input_err(true, &ts->client->dev, "%s: cmd0 is abnormal, %d", __func__, sec->cmd_param[0]);
		goto err_grip_data;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

err_grip_data:

	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

void zt_set_grip_type(struct zt_ts_info *ts, u8 set_type)
{
	u8 mode = G_NONE;

	input_info(true, &ts->client->dev, "%s: re-init grip(%d), edh:%d, edg:%d, lan:%d\n", __func__,
			set_type, ts->plat_data->grip_data.edgehandler_direction,
			ts->plat_data->grip_data.edge_range, ts->plat_data->grip_data.landscape_mode);

	/* edge handler */
	if (ts->plat_data->grip_data.edgehandler_direction != 0)
		mode |= G_SET_EDGE_HANDLER;

	if (set_type == GRIP_ALL_DATA) {
		/* edge */
		if (ts->plat_data->grip_data.edge_range != 60)
			mode |= G_SET_EDGE_ZONE;

		/* dead zone */
		if (ts->plat_data->grip_data.landscape_mode == 1)	/* default 0 mode, 32 */
			mode |= G_SET_LANDSCAPE_MODE;
		else
			mode |= G_SET_NORMAL_MODE;
	}

	if (mode)
		set_grip_data_to_ic(ts, mode);
}

static void set_touchable_area(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	input_info(true, &info->client->dev,
			"%s: set 16:9 mode %s\n", __func__, sec->cmd_param[0] ? "enable" : "disable");

	zt_set_optional_mode(info, DEF_OPTIONAL_MODE_TOUCHABLE_AREA, sec->cmd_param[0]);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int arg = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%u", (unsigned int) arg);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] > 1) {
			info->flip_enable = true;
			info->cover_type = sec->cmd_param[1];
#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
			sec_delay(100);
			if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
				tui_force_close(1);
				sec_delay(100);
				if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
					trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
					trustedui_set_mode(TRUSTEDUI_MODE_OFF);
				}
			}
#endif // CONFIG_TRUSTONIC_TRUSTED_UI
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
			stui_cancel_session();
#endif
		} else {
			info->flip_enable = false;
		}

		set_cover_type(info, info->flip_enable);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void clear_reference_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	zt_ts_esd_timer_stop(info);

	write_reg(client, ZT_EEPROM_INFO, 0xffff);

	write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0001);
	usleep_range(100, 100);
	if (write_cmd(client, ZT_SAVE_STATUS_CMD) != I2C_SUCCESS)
		return;

	sec_delay(500);
	write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0000);
	usleep_range(100, 100);

	zt_ts_esd_timer_start(info);
	input_info(true, &client->dev, "%s: TSP clear calibration bit\n", __func__);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

int zt_tclm_execute_force_calibration(struct device *dev, int cal_mode)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);

	if (ts_hw_calibration(info) == false)
		return -1;

	return 0;
}

static void ts_enter_strength_mode(struct zt_ts_info *info, int testnum)
{
	struct i2c_client *client = info->client;
	u8 i;

	mutex_lock(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_info(true, &info->client->dev, "other process occupied.. (%d)\n", info->work_state);
		mutex_unlock(&info->work_lock);
		return;
	}

	info->touch_mode = testnum;

	write_cmd(client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_cmd(client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);

	if (testnum == TOUCH_DELTA_MODE) {
		input_info(true, &info->client->dev, "%s: shorten delay raw for host\n", __func__);
		if (write_reg(client, ZT_DELAY_RAW_FOR_HOST, RAWDATA_DELAY_FOR_HOST / 5) != I2C_SUCCESS) {
			input_info(true, &client->dev, "%s: Fail to delay_raw_for_host enter\n", __func__);
			mutex_unlock(&info->work_lock);
			return;
		}
	}

	if (write_reg(client, ZT_POWER_STATE_FLAG, 1) != I2C_SUCCESS) {
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG 1\n", __func__);
		mutex_unlock(&info->work_lock);
		return;
	}
	sec_delay(10);

	if (write_reg(client, ZT_TOUCH_MODE, testnum) != I2C_SUCCESS) {
		input_info(true, &client->dev, "%s: Fail to set ZINITX_TOUCH_MODE %d\n", __func__, testnum);
		mutex_unlock(&info->work_lock);
		return;
	}

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		usleep_range(20 * 1000, 20 * 1000);
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	}

	clear_report_data(info);

	input_info(true, &client->dev, "%s: Enter_strength_mode\n", __func__);

	mutex_unlock(&info->work_lock);
}

static void ts_exit_strength_mode(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 i;

	mutex_lock(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_info(true, &info->client->dev, "other process occupied.. (%d)\n", info->work_state);
		mutex_unlock(&info->work_lock);
		return;
	}

	if (write_reg(client, ZT_TOUCH_MODE, TOUCH_POINT_MODE) != I2C_SUCCESS) {
		input_info(true, &client->dev, "[zinitix_touch] TEST Mode : "
				"Fail to set ZINITX_TOUCH_MODE %d.\r\n", TOUCH_POINT_MODE);
		mutex_unlock(&info->work_lock);
		return;
	}
	if (write_reg(client, ZT_POWER_STATE_FLAG, 0) != I2C_SUCCESS) {
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG 0\n", __func__);
		mutex_unlock(&info->work_lock);
		return;
	}
	sec_delay(10);

	if (info->touch_mode == TOUCH_DELTA_MODE) {
		input_info(true, &info->client->dev, "%s: restore delay raw for host\n", __func__);
		if (write_reg(client, ZT_DELAY_RAW_FOR_HOST, RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS) {
			input_info(true, &client->dev, "%s: Fail to delay_raw_for_host exit\n", __func__);
			mutex_unlock(&info->work_lock);
			return;
		}
	}

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		usleep_range(20 * 1000, 20 * 1000);
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
	}

	clear_report_data(info);
	input_info(true, &client->dev, "%s\n", __func__);

	info->touch_mode = TOUCH_POINT_MODE;
	mutex_unlock(&info->work_lock);
}

static void ts_get_strength_data(struct zt_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i, j, n;
	u8 ref_max[2] = {0, 0};

	mutex_lock(&info->raw_data_lock);
	read_data(info->client, 0x0308, ref_max, 2);

	input_info(true, &client->dev, "reference max: %X %X\n", ref_max[0], ref_max[1]);

	n = 0;
	for (i = 0 ; i < info->cap_info.y_node_num; i++) {
		pr_info("%s %d |", SECLOG, i);
		for (j = 0 ; j < info->cap_info.x_node_num; j++, n++) {
			pr_cont(" %d", info->cur_data[n]);
		}
		pr_cont("\n");
	}
	mutex_unlock(&info->raw_data_lock);
}

static void run_cs_raw_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retry = 0;
	char *all_cmdbuff;
	s32 i, j;

	sec_cmd_set_default_result(sec);

	disable_irq(info->irq);

	ts_enter_strength_mode(info, TOUCH_RAW_MODE);

	while (gpio_get_value(info->plat_data->irq_gpio)) {
		sec_delay(30);

		retry++;

		input_info(true, &client->dev, "%s: retry:%d\n", __func__, retry);

		if (retry > 100) {
			enable_irq(info->irq);
			goto out;
		}
	}
	ts_get_raw_data(info);

	ts_exit_strength_mode(info);

	enable_irq(info->irq);

	ts_get_strength_data(info);

	all_cmdbuff = kzalloc(info->cap_info.x_node_num * info->cap_info.y_node_num * 6, GFP_KERNEL);
	if (!all_cmdbuff) {
		input_info(true, &info->client->dev, "%s: alloc failed\n", __func__);
		goto out;
	}

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			sprintf(buff, "%d,", info->cur_data[i * info->cap_info.x_node_num + j]);
			strcat(all_cmdbuff, buff);
		}
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, sizeof(all_cmdbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(all_cmdbuff);

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

}

static void run_cs_delta_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retry = 0;
	char *all_cmdbuff;
	s32 i, j;

	sec_cmd_set_default_result(sec);

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	ts_enter_strength_mode(info, TOUCH_DELTA_MODE);

	while (gpio_get_value(info->plat_data->irq_gpio)) {
		sec_delay(30);

		retry++;

		input_info(true, &client->dev, "%s: retry:%d\n", __func__, retry);

		if (retry > 100) {
			enable_irq(info->irq);
			zt_ts_esd_timer_start(info);
			goto out;
		}
	}
	ts_get_raw_data(info);

	ts_exit_strength_mode(info);

	enable_irq(info->irq);

	zt_ts_esd_timer_start(info);

	ts_get_strength_data(info);

	all_cmdbuff = kzalloc(info->cap_info.x_node_num * info->cap_info.y_node_num * 6, GFP_KERNEL);
	if (!all_cmdbuff) {
		input_info(true, &info->client->dev, "%s: alloc failed\n", __func__);
		goto out;
	}

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			sprintf(buff, "%d,", info->cur_data[i * info->cap_info.x_node_num + j]);
			strcat(all_cmdbuff, buff);
		}
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, sizeof(all_cmdbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(all_cmdbuff);

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_ref_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int i;
#ifdef TCLM_CONCEPT
	int ret;
#endif
	sec_cmd_set_default_result(sec);

	if (info->finger_cnt1 != 0) {
		input_info(true, &client->dev, "%s: return (finger cnt %d)\n", __func__, info->finger_cnt1);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	disable_irq(info->irq);

	zt_ts_esd_timer_stop(info);
	zt_power_control(info, false);
	zt_power_control(info, true);

	if (ts_hw_calibration(info) == true) {
#ifdef TCLM_CONCEPT
		/* devide tclm case */
		sec_tclm_case(info->tdata, sec->cmd_param[0]);

		input_info(true, &info->client->dev, "%s: param, %d, %c, %d\n", __func__,
				sec->cmd_param[0], sec->cmd_param[0], info->tdata->root_of_calibration);

		ret = sec_execute_tclm_package(info->tdata, 1);
		if (ret < 0) {
			input_err(true, &info->client->dev,
					"%s: sec_execute_tclm_package\n", __func__);
		}
		sec_tclm_root_of_cal(info->tdata, CALPOSITION_NONE);
#endif
		input_info(true, &client->dev, "%s: TSP calibration Pass\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec_cmd_set_cmd_result(sec, buff, (int)strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		input_info(true, &client->dev, "%s: TSP calibration Fail\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, (int)strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	zt_power_control(info, false);
	zt_power_control(info, true);
	mini_init_touch(info);

	for (i = 0; i < 5; i++) {
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
		usleep_range(10, 10);
	}

	clear_report_data(info);

	zt_ts_esd_timer_start(info);
	enable_irq(info->irq);
	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_amp_check_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int total_node = info->cap_info.y_node_num;
	u16 min, max;
	s32 i;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(info, TOUCH_AMP_CHECK_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	get_raw_data(info, (u8 *)raw_data->amp_check_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	min = 0xFFFF;
	max = 0x0000;

	for (i = 0; i < total_node; i++) {
		if (raw_data->amp_check_data[i] < min)
			min = raw_data->amp_check_data[i];

		if (raw_data->amp_check_data[i] > max)
			max = raw_data->amp_check_data[i];
	}

	input_info(true, &client->dev, "%s: amp check data min:%d, max:%d\n", __func__, min, max);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "AMP_CHECK");
	}

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EDGE_SELECT, !sec->cmd_param[0]);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->spay_enable = !!sec->cmd_param[0];

	zt_set_lp_mode(info, ZT_SPONGE_MODE_SPAY, info->spay_enable);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);
}

static void fod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s: fod_enable:%d, short_mode:%d, strict mode:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],	sec->cmd_param[2]);

	info->fod_enable = !!sec->cmd_param[0];
	info->fod_mode_set = (sec->cmd_param[1] & 0x01) | ((sec->cmd_param[2] & 0x01) << 1);

	zt_set_lp_mode(info, ZT_SPONGE_MODE_PRESS, info->fod_enable);
	ret = ts_write_to_sponge(info, ZT_SPONGE_FOD_PROPERTY, (u8 *)&info->fod_mode_set, 2);
	if (ret < 0)
		input_err(true, &info->client->dev, "%s: Failed to write sponge\n", __func__);

	input_info(true, &info->client->dev, "%s: %s, fast:%s, strict:%s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off",
			info->fod_mode_set & 1 ? "on" : "off",
			info->fod_mode_set & 2 ? "on" : "off",
			info->lpm_mode);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);
}

static void fod_lp_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);

	if (val) {
		info->fod_lp_mode = 1;
	} else {
		info->fod_lp_mode = 0;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);
}

static void singletap_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->singletap_enable = !!sec->cmd_param[0];

	zt_set_lp_mode(info, ZT_SPONGE_MODE_SINGLETAP, info->singletap_enable);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->aot_enable = !!sec->cmd_param[0];

	zt_set_lp_mode(info, ZT_SPONGE_MODE_DOUBLETAP_WAKEUP, info->aot_enable);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);
}

static void aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->aod_enable = !!sec->cmd_param[0];

	zt_set_lp_mode(info, ZT_SPONGE_MODE_AOD, info->aod_enable);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);
}

static void set_fod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int i;
	int ret;

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s: start x:%d, start y:%d, end x:%d, end y:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	for (i = 0; i < 4; i++)
		info->fod_rect[i] = sec->cmd_param[i] & 0xFFFF;

	ret = zt_set_fod_rect(info);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		goto error;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;

error:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int i;
	int ret;

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	for (i = 0; i < 4; i++)
		info->plat_data->aod_data.rect_data[i] = sec->cmd_param[i];

	ret = zt_set_aod_rect(info);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		goto error;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;

error:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void get_wet_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 temp;

	sec_cmd_set_default_result(sec);

	mutex_lock(&info->work_lock);
	read_data(client, ZT_DEBUG_REG, (u8 *)&temp, 2);
	mutex_unlock(&info->work_lock);

	input_info(true, &client->dev, "%s, %x\n", __func__, temp);

	if (zinitix_bit_test(temp, DEF_DEVICE_STATUS_WATER_MODE))
		temp = true;
	else
		temp = false;

	snprintf(buff, sizeof(buff), "%u", temp);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "WET_MODE");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_SENSITIVE_BIT, sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void pocket_mode_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->plat_data->pocket_mode = sec->cmd_param[0];

		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_POCKET_MODE, info->plat_data->pocket_mode);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);
}

static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		mutex_lock(&info->power_init);
		write_reg(client, ZT_SET_SIP_MODE, (u8)sec->cmd_param[0]);
		mutex_unlock(&info->power_init);
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_snr_non_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 status;
	int wait_time = 0;
	int retry_cnt = 0;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 1 || sec->cmd_param[0] > 1000) {
		input_err(true, &info->client->dev, "%s: strange value frame:%d\n",
				__func__, sec->cmd_param[0]);
		goto ERROR_INIT;
	}

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	if (write_reg(client, ZT_POWER_STATE_FLAG, 1) != I2C_SUCCESS)
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG, 1\n", __func__);

	sec_delay(20);

	/* set frame count */
	if (write_reg(client, REG_SNR_TEST_N_DATA_CNT, sec->cmd_param[0]) != I2C_SUCCESS)
		input_err(true, &client->dev, "%s: failed REG_SNR_TEST_N_DATA_CNT\n", __func__);
	sec_delay(10);

	/* start Non-touched Peak Noise */
	if (write_reg(info->client, REG_SNR_TEST_START_CMD, 1) != I2C_SUCCESS)
		input_info(true, &info->client->dev, "%s, fail REG_SNR_TEST_START_CMD (1) set\n", __func__);
	sec_delay(20);

	wait_time = (sec->cmd_param[0] * 1000) / 90 + 200;	/* frame count * 1000 / frame rate + 200 msec margin*/
	sec_delay(wait_time);

	retry_cnt = 50;
	while ((read_data(client, REG_SNR_TEST_RESULT_STATUS, (u8 *)&status, 2) >= 0) && (retry_cnt-- > 0)) {
		if (status == 1)
			break;
		sec_delay(20);
	}

	if (status == 0) {
		input_err(true, &info->client->dev, "%s: failed non-touched status:%d\n", __func__, status);
		goto ERROR;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	/* EXIT SNR mode */
	if (write_reg(info->client, REG_SNR_TEST_START_CMD, 0) != I2C_SUCCESS)
		input_info(true, &info->client->dev, "%s, fail REG_SNR_TEST_START_CMD (0) set\n", __func__);
	sec_delay(10);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);

	if (write_reg(client, ZT_POWER_STATE_FLAG, 0) != I2C_SUCCESS)
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG, 0\n", __func__);

	sec_delay(10);

	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return;

ERROR:
	/* EXIT SNR mode */
	if (write_reg(info->client, REG_SNR_TEST_START_CMD, 0) != I2C_SUCCESS)
		input_info(true, &info->client->dev, "%s, fail REG_SNR_TEST_START_CMD (0) set\n", __func__);
	sec_delay(10);

	if (write_reg(client, ZT_POWER_STATE_FLAG, 0) != I2C_SUCCESS)
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG, 0\n", __func__);

	sec_delay(10);

	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

ERROR_INIT:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_snr_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct zt_ts_snr_result_cmd snr_cmd_result;
	struct zt_ts_snr_result snr_result;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tbuff[SEC_CMD_STR_LEN] = { 0 };
	int wait_time = 0;
	int retry_cnt = 0;
	int i = 0;

	sec_cmd_set_default_result(sec);

	memset(&snr_result, 0, sizeof(struct zt_ts_snr_result));
	memset(&snr_cmd_result, 0, sizeof(struct zt_ts_snr_result_cmd));

	if (sec->cmd_param[0] < 1 || sec->cmd_param[0] > 1000) {
		input_err(true, &info->client->dev, "%s: strange value frame:%d\n",
				__func__, sec->cmd_param[0]);
		goto ERROR_INIT;
	}

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	if (write_reg(client, ZT_POWER_STATE_FLAG, 1) != I2C_SUCCESS)
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG, 1\n", __func__);

	sec_delay(20);

	/* set frame count */
	if (write_reg(client, REG_SNR_TEST_N_DATA_CNT, sec->cmd_param[0]) != I2C_SUCCESS)
		input_err(true, &client->dev, "%s: failed REG_SNR_TEST_N_DATA_CNT\n", __func__);
	sec_delay(10);

	/* start touched Peak Noise */
	if (write_reg(info->client, REG_SNR_TEST_START_CMD, 2) != I2C_SUCCESS)
		input_info(true, &info->client->dev, "%s, fail REG_SNR_TEST_START_CMD (2) set\n", __func__);
	sec_delay(20);

	wait_time = (sec->cmd_param[0] * 1000) / 90 + 200;	/* frame count * 1000 / frame rate + 200 msec margin*/
	sec_delay(wait_time);

	retry_cnt = 50;
	while ((read_data(client, REG_SNR_TEST_RESULT_STATUS, (u8 *)&snr_cmd_result, 6) >= 0) && (retry_cnt-- > 0)) {
		if (snr_cmd_result.status == 2)
			break;
		sec_delay(20);
	}

	if (snr_cmd_result.status == 0) {
		input_err(true, &info->client->dev, "%s: failed touched status:%d\n", __func__, snr_cmd_result.status);
		goto ERROR;
	} else {
		input_info(true, &info->client->dev, "%s: status:%d, point:%d, average:%d\n", __func__,
			snr_cmd_result.status, snr_cmd_result.point, snr_cmd_result.average);
	}

	if (read_data(info->client, REG_SNR_TEST_RESULT, (u8 *)&snr_result, sizeof(struct zt_ts_snr_result)) < 0) {
		input_err(true, &info->client->dev, "%s: read REG_SNR_TEST_RESULT", __func__);
		goto ERROR;
	}

	for (i = 0; i < 9; i++) {
		input_info(true, &info->client->dev, "%s: average:%d, snr1:%d, snr2:%d\n", __func__,
			snr_result.result[i].average, snr_result.result[i].snr1, snr_result.result[i].snr2);
		snprintf(tbuff, sizeof(tbuff), "%d,%d,%d,",
			snr_result.result[i].average,
			snr_result.result[i].snr1,
			snr_result.result[i].snr2);
		strlcat(buff, tbuff, sizeof(buff));
	}

	write_cmd(info->client, ZT_CLEAR_INT_STATUS_CMD);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	/* EXIT SNR mode */
	if (write_reg(info->client, REG_SNR_TEST_START_CMD, 0) != I2C_SUCCESS)
		input_info(true, &info->client->dev, "%s, fail REG_SNR_TEST_START_CMD (0) set\n", __func__);
	sec_delay(10);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);

	if (write_reg(client, ZT_POWER_STATE_FLAG, 0) != I2C_SUCCESS)
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG, 0\n", __func__);

	sec_delay(10);

	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	return;

ERROR:
	/* EXIT SNR mode */
	if (write_reg(info->client, REG_SNR_TEST_START_CMD, 0) != I2C_SUCCESS)
		input_info(true, &info->client->dev, "%s, fail REG_SNR_TEST_START_CMD (0) set\n", __func__);
	sec_delay(10);

	if (write_reg(client, ZT_POWER_STATE_FLAG, 0) != I2C_SUCCESS)
		input_info(true, &client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG, 0\n", __func__);

	sec_delay(10);

	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

ERROR_INIT:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

int zt_fix_active_mode(struct zt_ts_info *info, bool active)
{
	u16 temp_power_state;
	u8 ret_cnt = 0;

	input_info(true, &info->client->dev, "%s: %s\n", __func__, active ? "fix" : "release");

retry:
	write_reg(info->client, ZT_POWER_STATE_FLAG, active);

	temp_power_state = 0xFFFF;
	read_data(info->client, ZT_POWER_STATE_FLAG, (u8 *)&temp_power_state, 2);

	if (temp_power_state != active) {
		input_err(true, &info->client->dev, "%s: fail write 0x%x register = %d\n",
			__func__, ZT_POWER_STATE_FLAG, temp_power_state);
		if (ret_cnt < 3) {
			ret_cnt++;
			goto retry;
		} else {
			input_err(true, &info->client->dev, "%s: fail write 0x%x register\n",
				__func__, ZT_POWER_STATE_FLAG);
			return I2C_FAIL;
		}
	}

	sec_delay(5);

	return I2C_SUCCESS;
}

static void fix_active_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[16] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->fix_active_mode = sec->cmd_param[0];
		ret = zt_fix_active_mode(info, info->fix_active_mode);
		if (ret != I2C_SUCCESS) {
			input_err(true, &info->client->dev, "%s: failed, retval = %d\n", __func__, ret);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s cmd_param: %d\n", __func__, buff, sec->cmd_param[0]);
}

static void get_crc_check(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 chip_check_sum = 0;

	sec_cmd_set_default_result(sec);

	if (read_data(info->client, ZT_CHECKSUM_RESULT,
				(u8 *)&chip_check_sum, 2) < 0) {
		input_err(true, &info->client->dev, "%s: read crc fail", __func__);
		goto err_get_crc_check;
	}

	input_info(true, &info->client->dev, "%s: 0x%04X\n", __func__, chip_check_sum);

	if (chip_check_sum != CORRECT_CHECK_SUM)
		goto err_get_crc_check;

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;

err_get_crc_check:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_test_vsync(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buf[64] = { 0 };
	u16 data = 0;

	sec_cmd_set_default_result(sec);

	if (write_reg(info->client, ZT_POWER_STATE_FLAG, 1) != I2C_SUCCESS) {
		input_err(true, &info->client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG 1\n", __func__);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	sec_delay(100);

	if (read_data(info->client, ZT_VSYNC_TEST_RESULT, (u8 *)&data, 2) < 0) {
		input_err(true, &info->client->dev, "%s: read crc fail", __func__);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	input_info(true, &info->client->dev, "%s: result %d\n", __func__, data);

	snprintf(buf, sizeof(buf), "%d", data);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	if (write_reg(info->client, ZT_POWER_STATE_FLAG, 0) != I2C_SUCCESS)
		input_err(true, &info->client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG 0\n", __func__);

	sec_delay(10);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "VSYNC");
}

static void read_osc_value(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u16 data[2] = {0};
	u32 osc_timer_val;

	sec_cmd_set_default_result(sec);

	if (write_reg(info->client, ZT_POWER_STATE_FLAG, 1) != I2C_SUCCESS) {
		input_err(true, &info->client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG 1\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	sec_delay(100);

	ret = read_data(info->client, ZT_OSC_TIMER_LSB, (u8 *)&data[0], 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,
			"%s: fail read proximity threshold\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	ret = read_data(info->client, ZT_OSC_TIMER_MSB, (u8 *)&data[1], 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,
			"%s: fail read proximity threshold\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	osc_timer_val = (data[1] << 16) | data[0];

	input_info(true, &info->client->dev,
					"%s: osc_timer_value %08X\n", __func__, osc_timer_val);

	snprintf(buff, sizeof(buff), "%u,%u", osc_timer_val, osc_timer_val);
	sec->cmd_state = SEC_CMD_STATUS_OK;

ERROR:
	if (write_reg(info->client, ZT_POWER_STATE_FLAG, 0) != I2C_SUCCESS)
		input_err(true, &info->client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG 0\n", __func__);

	sec_delay(10);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "OSC_DATA");
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (info->plat_data->power_enabled == false) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	run_interrupt_gpio_test(sec);
	run_dnd_read(sec);
	run_dnd_v_gap_read(sec);
	run_dnd_h_gap_read(sec);
	run_cnd_read(sec);
	run_selfdnd_read(sec);
	run_selfdnd_h_gap_read(sec);
	run_self_saturation_read(sec);
	run_charge_pump_read(sec);
	run_test_vsync(sec);
	read_osc_value(sec);
	run_amp_check_read(sec);

#ifdef TCLM_CONCEPT
	run_mis_cal_read(sec);
#endif

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &client->dev, "%s: %d%s\n", __func__, sec->item_count,
			sec->cmd_result_all);
}

static void factory_cmd_result_all_imagetest(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (info->plat_data->power_enabled == false) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	run_jitter_test(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &client->dev, "%s: %d%s\n", __func__, sec->item_count,
			sec->cmd_result_all);
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[3] = { 0 };
	u8 conn_check_val;
	int ret;

	sec_cmd_set_default_result(sec);

	ret = read_data(client, ZT_CONNECTION_CHECK_REG, (u8 *)&conn_check_val, 1);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail read TSP connection value\n", __func__);
		goto err_conn_check;
	}

	if (conn_check_val != 1)
		goto err_conn_check;

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;

err_conn_check:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 prox_x_data, prox_y_data;
	u16 threshold;
	int ret;

	sec_cmd_set_default_result(sec);

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);
	mutex_lock(&info->work_lock);

	ret = read_data(client, ZT_PROXIMITY_XDATA, (u8 *)&prox_x_data, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail read proximity x data\n", __func__);
		goto READ_FAIL;
	}

	ret = read_data(client, ZT_PROXIMITY_YDATA, (u8 *)&prox_y_data, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail read proximity y data\n", __func__);
		goto READ_FAIL;
	}

	ret = read_data(client, ZT_PROXIMITY_THRESHOLD, (u8 *)&threshold, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,
				"%s: fail read proximity threshold\n", __func__);
		goto READ_FAIL;
	}

	mutex_unlock(&info->work_lock);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	snprintf(buff, sizeof(buff), "SUM_X:%d SUM_Y:%d THD_X:%d THD_Y:%d",
			prox_x_data, prox_y_data, threshold, threshold);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;

READ_FAIL:
	mutex_unlock(&info->work_lock);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1 && sec->cmd_param[0] != 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->plat_data->ed_enable = sec->cmd_param[0];

		if (info->plat_data->ed_enable == 3) {
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, true);
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, false);
		} else if (info->plat_data->ed_enable == 1) {
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, false);
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, true);
		} else {
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, false);
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, false);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);
}

/*	for game mode
 *	byte[0]: Setting for the Game Mode with 240Hz scan rate
 *		- 0: Disable
 *		- 1: Enable
 *
 *	byte[1]: Vsycn mode
 *		- 0: Normal 60
 *		- 1: HS60
 *		- 2: HS120
 *		- 3: VSYNC 48
 *		- 4: VSYNC 96
*/
static void set_scan_rate(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tBuff[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1 ||
			sec->cmd_param[1] < 0 || sec->cmd_param[1] > 4) {
		input_err(true, &info->client->dev, "%s: not support param\n", __func__);
		goto NG;
	}

	tBuff[0] = sec->cmd_param[0];
	tBuff[1] = sec->cmd_param[1];

	mutex_lock(&info->power_init);
	if (write_reg(info->client, ZT_SET_SCANRATE_ENABLE, tBuff[0]) != I2C_SUCCESS) {
		input_err(true, &info->client->dev,
				"%s: failed to set scan mode enable\n", __func__);
		mutex_unlock(&info->power_init);
		goto NG;
	}

	if (write_reg(info->client, ZT_SET_SCANRATE, tBuff[1]) != I2C_SUCCESS) {
		input_err(true, &info->client->dev,
				"%s: failed to set scan rate\n", __func__);
		mutex_unlock(&info->power_init);
		goto NG;
	}
	mutex_unlock(&info->power_init);

	input_info(true, &info->client->dev,
					"%s: set scan rate %d %d\n", __func__, tBuff[0], tBuff[1]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_wirelesscharger_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &info->client->dev, "%s: not support param\n", __func__);
		goto NG;
	}

	mutex_lock(&info->power_init);
	if (write_reg(info->client, ZT_SET_WIRELESSCHARGER_MODE, (u8)sec->cmd_param[0]) != I2C_SUCCESS) {
		input_err(true, &info->client->dev,
				"%s: failed to set scan mode enable\n", __func__);
		mutex_unlock(&info->power_init);
		goto NG;
	}
	mutex_unlock(&info->power_init);

	input_info(true, &info->client->dev,
					"%s: set wireless charger mode %d\n", __func__, sec->cmd_param[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_note_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &info->client->dev, "%s: not support param\n", __func__);
		goto NG;
	}

	mutex_lock(&info->power_init);
	if (write_reg(info->client, ZT_SET_NOTE_MODE, (u8)sec->cmd_param[0]) != I2C_SUCCESS) {
		input_err(true, &info->client->dev,
				"%s: failed to set scan mode enable\n", __func__);
		mutex_unlock(&info->power_init);
		goto NG;
	}
	mutex_unlock(&info->power_init);

	input_info(true, &info->client->dev,
					"%s: set note mode %d\n", __func__, sec->cmd_param[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tBuff[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &info->client->dev, "%s: not support param\n", __func__);
		goto NG;
	}

	tBuff[0] = sec->cmd_param[0];

	mutex_lock(&info->power_init);
	if (write_reg(info->client, ZT_SET_GAME_MODE, tBuff[0]) != I2C_SUCCESS) {
		input_err(true, &info->client->dev,
				"%s: failed to set scan mode enable\n", __func__);
		mutex_unlock(&info->power_init);
		goto NG;
	}
	mutex_unlock(&info->power_init);

	input_info(true, &info->client->dev,
					"%s: set game mode %d\n", __func__, tBuff[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_info(true, &client->dev, "%s: scrub_id: %d, X:%d, Y:%d\n", __func__,
			info->scrub_id, info->scrub_x, info->scrub_y);

	snprintf(buff, sizeof(buff), "%d %d %d", info->scrub_id, info->scrub_x, info->scrub_y);

	info->scrub_id = 0;
	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	s16 i, value[TOUCH_SENTIVITY_MEASUREMENT_COUNT];
	char buff[SEC_CMD_STR_LEN] = { 0 };

	for (i = 0; i < TOUCH_SENTIVITY_MEASUREMENT_COUNT; i++)
		value[i] = info->sensitivity_data[i];

	input_info(true, &info->client->dev, "%s: sensitivity mode,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
			value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7], value[8]);

	snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d,%d,%d,%d,%d",
			value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7], value[8]);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	int ret;
	unsigned long value = 0;

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	if (info->plat_data->power_enabled == false) {
		input_err(true, &info->client->dev, "%s: power off in IC\n", __func__);
		return 0;
	}

	zt_ts_esd_timer_stop(info);
	input_err(true, &info->client->dev, "%s: enable:%ld\n", __func__, value);

	if (value == 1) {
		ts_set_touchmode(info, TOUCH_SENTIVITY_MEASUREMENT_MODE);
		input_info(true, &info->client->dev, "%s: enable end\n", __func__);
	} else {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		input_info(true, &info->client->dev, "%s: disable end\n", __func__);
	}

	zt_ts_esd_timer_start(info);
	input_info(true, &info->client->dev, "%s: done\n", __func__);
	return count;
}

static ssize_t fod_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	/* get fod information */
	if (!info->fod_info_vi_trx[0] || !info->fod_info_vi_trx[1] || !info->fod_info_vi_data_len) {
		ret = ts_read_from_sponge(info, ZT_SPONGE_FOD_INFO, info->fod_info_vi_trx, 3);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: fail fod channel info.\n", __func__);
			return ret;
		}
	}

	info->fod_info_vi_data_len = info->fod_info_vi_trx[2];

	input_info(true, &info->client->dev, "%s: %d,%d,%d\n", __func__,
			info->fod_info_vi_trx[0], info->fod_info_vi_trx[1], info->fod_info_vi_data_len);

	snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d",
			info->fod_info_vi_trx[0], info->fod_info_vi_trx[1], info->fod_info_vi_data_len,
			info->cap_info.x_node_num, info->cap_info.y_node_num);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t fod_pos_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	int i, ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	if (!info->fod_info_vi_data_len) {
		input_err(true, &info->client->dev, "%s: not read fod_info yet\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	if (!info->fod_with_finger_packet) {
		memset(info->fod_touch_vi_data, 0x00, info->fod_info_vi_data_len);
		ret = ts_read_from_sponge(info, ZT_SPONGE_FOD_POSITION,
						info->fod_touch_vi_data, info->fod_info_vi_data_len);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: fail fod data read error.\n", __func__);
			return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
		}
	}

	for (i = 0; i < info->fod_info_vi_data_len; i++) {
		snprintf(buff, 3, "%02X", info->fod_touch_vi_data[i]);
		strlcat(buf, buff, SEC_CMD_BUF_SIZE);
	}

	return strlen(buf);
}

static ssize_t aod_active_area(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: top:%d, edge:%d, bottom:%d\n",
			__func__, info->plat_data->aod_data.active_area[0],
			info->plat_data->aod_data.active_area[1],
			info->plat_data->aod_data.active_area[2]);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d,%d,%d",
		info->plat_data->aod_data.active_area[0], info->plat_data->aod_data.active_area[1],
		info->plat_data->aod_data.active_area[2]);
}

static ssize_t read_ito_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %02X%02X%02X%02X\n",
			__func__, info->plat_data->hw_param.ito_test[0],
			info->plat_data->hw_param.ito_test[1],
			info->plat_data->hw_param.ito_test[2],
			info->plat_data->hw_param.ito_test[3]);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%02X%02X%02X%02X",
			info->plat_data->hw_param.ito_test[0], info->plat_data->hw_param.ito_test[1],
			info->plat_data->hw_param.ito_test[2], info->plat_data->hw_param.ito_test[3]);
}

static ssize_t read_wet_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__, info->plat_data->hw_param.wet_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->plat_data->hw_param.wet_count);
}

static ssize_t clear_wet_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	info->plat_data->hw_param.wet_count = 0;

	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_multi_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->plat_data->hw_param.multi_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->plat_data->hw_param.multi_count);
}

static ssize_t clear_multi_count_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	info->plat_data->hw_param.multi_count = 0;

	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_comm_err_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->plat_data->hw_param.comm_err_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->plat_data->hw_param.comm_err_count);
}

static ssize_t clear_comm_err_count_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	info->plat_data->hw_param.comm_err_count = 0;

	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_module_id_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s\n", __func__);
#ifdef TCLM_CONCEPT
	return snprintf(buf, SEC_CMD_BUF_SIZE, "ZI%02X%02x%c%01X%04X\n",
			info->cap_info.reg_data_version, info->test_result.data[0],
			info->tdata->tclm_string[info->tdata->nvdata.cal_position].s_name,
			info->tdata->nvdata.cal_count & 0xF, info->tdata->nvdata.tune_fix_ver);
#else
	return snprintf(buf, SEC_CMD_BUF_SIZE, "ZI%02X\n",
			info->cap_info.reg_data_version);
#endif
}

/** for protos **/
static ssize_t protos_event_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->hover_event);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->hover_event != 3 ? 0 : 3);
}

static ssize_t protos_event_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	u8 data;
	int ret;

	ret = kstrtou8(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, &info->client->dev, "%s: %d\n", __func__, data);

	if (data != 0 && data != 1 && data != 3) {
		input_err(true, &info->client->dev, "%s: incorrect data\n", __func__);
		return -EINVAL;
	}

	info->plat_data->ed_enable = data;

	if (info->plat_data->ed_enable == 3) {
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, true);
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, false);
	} else if (info->plat_data->ed_enable == 1) {
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, false);
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, true);
	} else {
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, false);
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, false);
	}

	return count;
}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static u16 ts_get_touch_reg(u16 addr)
{
	int ret = 1;
	u16 reg_value;

	disable_irq(misc_info->irq);

	mutex_lock(&misc_info->work_lock);
	if (misc_info->work_state != NOTHING) {
		input_info(true, &misc_info->client->dev, "other process occupied.. (%d)\n",
				misc_info->work_state);
		enable_irq(misc_info->irq);
		mutex_unlock(&misc_info->work_lock);
		return -1;
	}
	misc_info->work_state = SET_MODE;

	write_reg(misc_info->client, 0x0A, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_reg(misc_info->client, 0x0A, 0x0A);

	ret = read_data(misc_info->client, addr, (u8 *)&reg_value, 2);
	if (ret < 0)
		input_err(true, &misc_info->client->dev, "%s: fail read touch reg\n", __func__);

	misc_info->work_state = NOTHING;
	enable_irq(misc_info->irq);
	mutex_unlock(&misc_info->work_lock);

	return reg_value;
}

static void ts_set_touch_reg(u16 addr, u16 value)
{
	disable_irq(misc_info->irq);

	mutex_lock(&misc_info->work_lock);
	if (misc_info->work_state != NOTHING) {
		input_info(true, &misc_info->client->dev, "other process occupied.. (%d)\n",
				misc_info->work_state);
		enable_irq(misc_info->irq);
		mutex_unlock(&misc_info->work_lock);
		return;
	}
	misc_info->work_state = SET_MODE;

	write_reg(misc_info->client, 0x0A, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_reg(misc_info->client, 0x0A, 0x0A);

	if (write_reg(misc_info->client, addr, value) != I2C_SUCCESS)
		input_err(true, &misc_info->client->dev, "%s: fail write touch reg\n", __func__);

	misc_info->work_state = NOTHING;
	enable_irq(misc_info->irq);
	mutex_unlock(&misc_info->work_lock);
}

static ssize_t read_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: 0x%x\n", __func__,
			info->store_reg_data);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "0x%x", info->store_reg_data);
}

static ssize_t store_read_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	u32 buff[2] = {0, }; //addr, size
	int ret;

	ret = sscanf(buf, "0x%x,0x%x", &buff[0], &buff[1]);
	if (ret != 2) {
		input_err(true, &info->client->dev,
				"%s: failed read params[0x%x]\n", __func__, ret);
		return -EINVAL;
	}

	if (buff[1] != 1 && buff[1] != 2) {
		input_err(true, &info->client->dev,
				"%s: incorrect byte length [0x%x]\n", __func__, buff[1]);
		return -EINVAL;
	}

	info->store_reg_data = ts_get_touch_reg((u16)buff[0]);

	if (buff[1] == 1)
		info->store_reg_data = info->store_reg_data & 0x00FF;

	input_info(true, &info->client->dev,
			"%s: read touch reg [addr:0x%x][data:0x%x]\n",
			__func__, buff[0], info->store_reg_data);

	return size;
}

static ssize_t store_write_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	int buff[3];
	int ret;

	ret = sscanf(buf, "0x%x,0x%x,0x%x", &buff[0], &buff[1], &buff[2]);
	if (ret != 3) {
		input_err(true, &info->client->dev,
				"%s: failed read params[0x%x]\n", __func__, ret);
		return -EINVAL;
	}

	if (buff[1] != 1 && buff[1] != 2) {
		input_err(true, &info->client->dev,
				"%s: incorrect byte length [0x%x]\n", __func__, buff[1]);
		return -EINVAL;
	}

	if (buff[1] == 1)
		buff[2] = buff[2] & 0x00FF;

	ts_set_touch_reg((u16)buff[0], (u16)buff[2]);
	input_info(true, &info->client->dev,
			"%s: write touch reg [addr:0x%x][byte:0x%x][data:0x%x]\n",
			__func__, buff[0], buff[1], buff[2]);

	return size;
}
#endif
static ssize_t get_lp_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	u8 sponge_data[10] = {0, };
	u16 current_index;
	u8 dump_format, dump_num;
	u16 dump_start, dump_end;
	u16 debug = 0;
	int i;

	if (info->plat_data->power_enabled == false) {
		input_err(true, &info->client->dev, "%s: Touch is stopped!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "TSP turned off");
	}

	if (info->work_state == ESD_TIMER) {
		input_err(true, &info->client->dev, "%s: Reset is ongoing!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "Reset is ongoing");
	}

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);
	mutex_lock(&info->work_lock);

	info->work_state = RAW_DATA;

	ts_read_from_sponge(info, ZT_SPONGE_DUMP_FORMAT, (u8 *)sponge_data, 4);

	read_data(info->client, ZT_REG_DEBUG19, (u8 *)&debug, 2);	
	input_info(true, &info->client->dev, "%s: debug[0x%04X]\n", __func__, debug);

	read_data(info->client, ZT_CHECKSUM_RESULT, (u8 *)&debug, 2);
	input_info(true, &info->client->dev, "%s: crc[0x%04X]\n", __func__, debug);

	dump_format = sponge_data[0];
	dump_num = sponge_data[1];
	dump_start = ZT_SPONGE_DUMP_START;
	dump_end = dump_start + (dump_format * (dump_num - 1));

	current_index = (sponge_data[3] & 0xFF) << 8 | (sponge_data[2] & 0xFF);
	if (current_index > dump_end || current_index < dump_start) {
		input_err(true, &info->client->dev,
				"Failed to Sponge LP log %d\n", current_index);
		snprintf(buf, SEC_CMD_BUF_SIZE,
				"NG, Failed to Sponge LP log, current_index=%d",
				current_index);
		goto out;
	}

	input_info(true, &info->client->dev, "%s: DEBUG format=%d, num=%d, start=%d, end=%d, current_index=%d\n",
				__func__, dump_format, dump_num, dump_start, dump_end, current_index);

	if (dump_num != 70 || dump_start != 244 || dump_end != 934) {
		input_err(true, &info->client->dev, "wrong data. skipped\n");
		goto out;
	}

	for (i = dump_num - 1 ; i >= 0 ; i--) {
		u16 data0, data1, data2, data3, data4;
		char buff[30] = {0, };
		u16 sponge_addr;

		if (current_index < (dump_format * i))
			sponge_addr = (dump_format * dump_num) + current_index - (dump_format * i);
		else
			sponge_addr = current_index - (dump_format * i);

		if (sponge_addr < dump_start)
			sponge_addr += (dump_format * dump_num);

		ts_read_from_sponge(info, sponge_addr, (u8 *)sponge_data, dump_format);

		data0 = (sponge_data[1] & 0xFF) << 8 | (sponge_data[0] & 0xFF);
		data1 = (sponge_data[3] & 0xFF) << 8 | (sponge_data[2] & 0xFF);
		data2 = (sponge_data[5] & 0xFF) << 8 | (sponge_data[4] & 0xFF);
		data3 = (sponge_data[7] & 0xFF) << 8 | (sponge_data[6] & 0xFF);
		data4 = (sponge_data[9] & 0xFF) << 8 | (sponge_data[8] & 0xFF);

		if (data0 || data1 || data2 || data3 || data4) {
			if (dump_format == 10) {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x%04x\n",
						sponge_addr, data0, data1, data2, data3, data4);
			} else {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x\n",
						sponge_addr, data0, data1, data2, data3);
			}
			strlcat(buf, buff, PAGE_SIZE);
		}
	}

out:
	info->work_state = NOTHING;
	mutex_unlock(&info->work_lock);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	return strlen(buf);
}

static DEVICE_ATTR(scrub_pos, S_IRUGO, scrub_position_show, NULL);
static DEVICE_ATTR(sensitivity_mode, S_IRUGO | S_IWUSR | S_IWGRP, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(ito_check, S_IRUGO | S_IWUSR | S_IWGRP, read_ito_check_show, clear_wet_mode_store);
static DEVICE_ATTR(wet_mode, S_IRUGO | S_IWUSR | S_IWGRP, read_wet_mode_show, clear_wet_mode_store);
static DEVICE_ATTR(comm_err_count, S_IRUGO | S_IWUSR | S_IWGRP, read_comm_err_count_show, clear_comm_err_count_store);
static DEVICE_ATTR(multi_count, S_IRUGO | S_IWUSR | S_IWGRP, read_multi_count_show, clear_multi_count_store);
static DEVICE_ATTR(module_id, S_IRUGO, read_module_id_show, NULL);
static DEVICE_ATTR(virtual_prox, S_IRUGO | S_IWUSR | S_IWGRP, protos_event_show, protos_event_store);
static DEVICE_ATTR(fod_info, S_IRUGO, fod_info_show, NULL);
static DEVICE_ATTR(fod_pos, S_IRUGO, fod_pos_show, NULL);
static DEVICE_ATTR(aod_active_area, 0444, aod_active_area, NULL);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static DEVICE_ATTR(read_reg_data, S_IRUGO | S_IWUSR | S_IWGRP, read_reg_show, store_read_reg);
static DEVICE_ATTR(write_reg_data, S_IWUSR | S_IWGRP, NULL, store_write_reg);
#endif
static DEVICE_ATTR(get_lp_dump, 0444, get_lp_dump, NULL);

static struct attribute *touchscreen_attributes[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_ito_check.attr,
	&dev_attr_wet_mode.attr,
	&dev_attr_multi_count.attr,
	&dev_attr_comm_err_count.attr,
	&dev_attr_module_id.attr,
	&dev_attr_virtual_prox.attr,
	&dev_attr_fod_info.attr,
	&dev_attr_fod_pos.attr,
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	&dev_attr_read_reg_data.attr,
	&dev_attr_write_reg_data.attr,
#endif
	&dev_attr_get_lp_dump.attr,
	&dev_attr_aod_active_area.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("get_module_vendor", get_module_vendor),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},

	/* vendor dependent command */
	{SEC_CMD("run_cnd_read", run_cnd_read),},
	{SEC_CMD("run_cnd_read_all", run_cnd_read_all),},
	{SEC_CMD("run_dnd_read", run_dnd_read),},
	{SEC_CMD("run_dnd_read_all", run_dnd_read_all),},
	{SEC_CMD("run_dnd_v_gap_read", run_dnd_v_gap_read),},
	{SEC_CMD("run_dnd_v_gap_read_all", run_dnd_v_gap_read_all),},
	{SEC_CMD("run_dnd_h_gap_read", run_dnd_h_gap_read),},
	{SEC_CMD("run_dnd_h_gap_read_all", run_dnd_h_gap_read_all),},
	{SEC_CMD("run_selfdnd_read", run_selfdnd_read),},
	{SEC_CMD("run_selfdnd_read_all", run_selfdnd_read_all),},
	{SEC_CMD("run_self_saturation_read", run_self_saturation_read),},	/* self_saturation_rx */
	{SEC_CMD("run_self_saturation_read_all", run_ssr_read_all),},
	{SEC_CMD("run_selfdnd_h_gap_read", run_selfdnd_h_gap_read),},
	{SEC_CMD("run_selfdnd_h_gap_read_all", run_selfdnd_h_gap_read_all),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("run_jitter_test", run_jitter_test),},
	{SEC_CMD("run_charge_pump_read", run_charge_pump_read),},
	{SEC_CMD("run_interrupt_gpio_test", run_interrupt_gpio_test),},
	{SEC_CMD("run_mis_cal_read", run_mis_cal_read),},
	{SEC_CMD("run_factory_miscalibration", run_factory_miscalibration),},
	{SEC_CMD("run_mis_cal_read_all", run_mis_cal_read_all),},
	{SEC_CMD_H("touch_aging_mode", touch_aging_mode),},
#ifdef TCLM_CONCEPT
	{SEC_CMD("get_pat_information", get_pat_information),},
	{SEC_CMD("get_tsp_test_result", get_tsp_test_result),},
	{SEC_CMD("set_tsp_test_result", set_tsp_test_result),},
	{SEC_CMD("increase_disassemble_count", increase_disassemble_count),},
	{SEC_CMD("get_disassemble_count", get_disassemble_count),},
#endif
	{SEC_CMD("run_cs_raw_read_all", run_cs_raw_read_all),},
	{SEC_CMD("run_cs_delta_read_all", run_cs_delta_read_all),},
	{SEC_CMD("run_force_calibration", run_ref_calibration),},
	{SEC_CMD("run_amp_check_read", run_amp_check_read),},
	{SEC_CMD("clear_reference_data", clear_reference_data),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD_H("set_touchable_area", set_touchable_area),},
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD("fod_enable", fod_enable),},
	{SEC_CMD_H("fod_lp_mode", fod_lp_mode),},
	{SEC_CMD("set_fod_rect", set_fod_rect),},
	{SEC_CMD_H("singletap_enable", singletap_enable),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("aod_enable", aod_enable),},
	{SEC_CMD("set_aod_rect", set_aod_rect),},
	{SEC_CMD("get_wet_mode", get_wet_mode),},
	{SEC_CMD_H("glove_mode", glove_mode),},
	{SEC_CMD_H("pocket_mode_enable", pocket_mode_enable),},
	{SEC_CMD("get_crc_check", get_crc_check),},
	{SEC_CMD("run_test_vsync", run_test_vsync),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("factory_cmd_result_all_imagetest", factory_cmd_result_all_imagetest),},
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD_H("set_scan_rate", set_scan_rate),},
	{SEC_CMD_H("set_wirelesscharger_mode", set_wirelesscharger_mode),},
	{SEC_CMD_H("set_note_mode", set_note_mode),},
	{SEC_CMD_H("set_game_mode", set_game_mode),},
	{SEC_CMD("read_osc_value", read_osc_value),},
	{SEC_CMD_H("set_sip_mode", set_sip_mode),},
	{SEC_CMD("run_snr_non_touched", run_snr_non_touched),},
	{SEC_CMD("run_snr_touched", run_snr_touched),},
	{SEC_CMD_H("fix_active_mode", fix_active_mode),},
	{SEC_CMD("debug", debug),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static int init_sec_factory(struct zt_ts_info *info)
{
	struct tsp_raw_data *raw_data;
	int ret;

	raw_data = kzalloc(sizeof(struct tsp_raw_data), GFP_KERNEL);
	if (unlikely(!raw_data)) {
		input_err(true, &info->client->dev, "%s: Failed to allocate memory\n",
				__func__);
		ret = -ENOMEM;

		goto err_alloc;
	}

	ret = sec_cmd_init(&info->sec, &info->client->dev, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP, &touchscreen_attr_group);
	if (ret < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
		goto err_init_cmd;
	}

	ret = sysfs_create_link(&info->sec.fac_dev->kobj,
			&info->input_dev->dev.kobj, "input");
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: Failed to create link\n", __func__);
		goto err_create_sysfs;
	}

	info->raw_data = raw_data;

	return ret;

err_create_sysfs:
err_init_cmd:
	kfree(raw_data);
err_alloc:

	return ret;
}

#ifdef USE_MISC_DEVICE
static int ts_misc_fops_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int ts_misc_fops_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static long ts_misc_fops_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct raw_ioctl raw_ioctl;
	u8 *u8Data;
	int ret = 0;
	size_t sz = 0;
	u16 mode;
	struct reg_ioctl reg_ioctl;
	u16 val;
	int nval = 0;
#if IS_ENABLED(CONFIG_COMPAT)
	void __user *argp = compat_ptr(arg);
#else
	void __user *argp = (void __user *)arg;
#endif

	if (misc_info == NULL) {
		pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
		return -1;
	}

	switch (cmd) {
	case TOUCH_IOCTL_GET_DEBUGMSG_STATE:
		ret = m_ts_debug_mode;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_SET_DEBUGMSG_STATE:
		if (copy_from_user(&nval, argp, sizeof(nval))) {
			input_err(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			return -1;
		}
		if (nval)
			input_info(true, &misc_info->client->dev, "[zinitix_touch] on debug mode (%d)\n", nval);
		else
			input_info(true, &misc_info->client->dev, "[zinitix_touch] off debug mode (%d)\n", nval);
		m_ts_debug_mode = nval;
		break;

	case TOUCH_IOCTL_GET_CHIP_REVISION:
		ret = misc_info->cap_info.ic_revision;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_FW_VERSION:
		ret = misc_info->cap_info.fw_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_REG_DATA_VERSION:
		ret = misc_info->cap_info.reg_data_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_SIZE:
		if (copy_from_user(&sz, argp, sizeof(size_t)))
			return -1;
		if (misc_info->cap_info.ic_fw_size != sz) {
			input_info(true, &misc_info->client->dev, "%s: firmware size error\n", __func__);
			return -1;
		}
		break;

	case TOUCH_IOCTL_GET_X_RESOLUTION:
		ret = misc_info->plat_data->max_x;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_RESOLUTION:
		ret = misc_info->plat_data->max_y;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_X_NODE_NUM:
		ret = misc_info->cap_info.x_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_NODE_NUM:
		ret = misc_info->cap_info.y_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_TOTAL_NODE_NUM:
		ret = misc_info->cap_info.total_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_HW_CALIBRAION:
		ret = -1;
		disable_irq(misc_info->irq);
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\r\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = HW_CALIBRAION;
		sec_delay(100);

		/* h/w calibration */
		if (ts_hw_calibration(misc_info) == true) {
			ret = 0;
#ifdef TCLM_CONCEPT
			sec_tclm_root_of_cal(misc_info->tdata, CALPOSITION_TESTMODE);
			sec_execute_tclm_package(misc_info->tdata, 1);
			sec_tclm_root_of_cal(misc_info->tdata, CALPOSITION_NONE);
#endif
		}

		mode = misc_info->touch_mode;
		if (write_reg(misc_info->client,
					ZT_TOUCH_MODE, mode) != I2C_SUCCESS) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: failed to set touch mode %d.\n",
					mode);
			goto fail_hw_cal;
		}

		if (write_cmd(misc_info->client,
					ZT_SWRESET_CMD) != I2C_SUCCESS)
			goto fail_hw_cal;

		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;
fail_hw_cal:
		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return -1;

	case TOUCH_IOCTL_SET_RAW_DATA_MODE:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		if (copy_from_user(&nval, argp, sizeof(nval))) {
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			misc_info->work_state = NOTHING;
			return -1;
		}
		ts_set_touchmode(misc_info, (u16)nval);

		return 0;

	case TOUCH_IOCTL_GET_REG:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]:other process occupied.. (%d)\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;

		if (copy_from_user(&reg_ioctl, argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			return -1;
		}

		if (read_data(misc_info->client,
					(u16)reg_ioctl.addr, (u8 *)&val, 2) < 0)
			ret = -1;

		nval = (int)val;

#if IS_ENABLED(CONFIG_COMPAT)
		if (copy_to_user(compat_ptr(reg_ioctl.val), (u8 *)&nval, 4)) {
#else
		if (copy_to_user((void __user *)(reg_ioctl.val), (u8 *)&nval, 4)) {
#endif
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_to_user\n", __func__);
			return -1;
		}

		input_info(true, &misc_info->client->dev, "%s read : reg addr = 0x%x, val = 0x%x\n", __func__,
				reg_ioctl.addr, nval);

		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_SET_REG:

		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (copy_from_user(&reg_ioctl,
					argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user(1)\n", __func__);
			return -1;
		}

#if IS_ENABLED(CONFIG_COMPAT)
		if (copy_from_user(&val, compat_ptr(reg_ioctl.val), sizeof(val))) {
#else
		if (copy_from_user(&val, (void __user *)(reg_ioctl.val), sizeof(val))) {
#endif
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user(2)\n", __func__);
			return -1;
		}

		if (write_reg(misc_info->client,
					(u16)reg_ioctl.addr, val) != I2C_SUCCESS)
			ret = -1;

		input_info(true, &misc_info->client->dev, "%s write : reg addr = 0x%x, val = 0x%x\r\n", __func__,
				reg_ioctl.addr, val);
		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_DONOT_TOUCH_EVENT:

		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\r\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (write_reg(misc_info->client,
					ZT_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			ret = -1;
		input_info(true, &misc_info->client->dev, "%s write : reg addr = 0x%x, val = 0x0\r\n", __func__,
				ZT_INT_ENABLE_FLAG);

		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_SEND_SAVE_STATUS:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev,
				"[zinitix_touch]: other process occupied.(%d)\n",
				misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = SET_MODE;
		ret = 0;
		write_reg(misc_info->client, VCMD_NVM_WRITE_ENABLE, 0x0001);
		if (write_cmd(misc_info->client,
					ZT_SAVE_STATUS_CMD) != I2C_SUCCESS)
			ret =  -1;

		sec_delay(1000);	/* for fusing eeprom */
		write_reg(misc_info->client, VCMD_NVM_WRITE_ENABLE, 0x0000);

		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_GET_RAW_DATA:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}

		if (misc_info->touch_mode == TOUCH_POINT_MODE)
			return -1;

		mutex_lock(&misc_info->raw_data_lock);
		if (misc_info->update == 0) {
			mutex_unlock(&misc_info->raw_data_lock);
			return -2;
		}

		if (copy_from_user(&raw_ioctl,
					argp, sizeof(struct raw_ioctl))) {
			mutex_unlock(&misc_info->raw_data_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			return -1;
		}

		misc_info->update = 0;

		u8Data = (u8 *)&misc_info->cur_data[0];
		if (raw_ioctl.sz > MAX_TRAW_DATA_SZ*2)
			raw_ioctl.sz = MAX_TRAW_DATA_SZ*2;
#if IS_ENABLED(CONFIG_COMPAT)
		if (copy_to_user(compat_ptr(raw_ioctl.buf), (u8 *)u8Data, raw_ioctl.sz)) {
#else
		if (copy_to_user((void __user *)(raw_ioctl.buf), (u8 *)u8Data, raw_ioctl.sz)) {
#endif
			mutex_unlock(&misc_info->raw_data_lock);
			return -1;
		}

		mutex_unlock(&misc_info->raw_data_lock);
		return 0;

	default:
		break;
	}
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_OF)
static int zt_ts_parse_dt(struct device_node *np,
		struct device *dev,
		struct zt_ts_platform_data *pdata)
{
	/* Optional parmeters(those values are not mandatory)
	 * do not return error value even if fail to get the value
	 */
	of_property_read_string(np, "zinitix,chip_name", &pdata->chip_name);

	pdata->mis_cal_check = of_property_read_bool(np, "zinitix,mis_cal_check");

	input_err(true, dev, "%s: MISCAL:%d\n",
			__func__, pdata->mis_cal_check);

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	of_property_read_u32(np, "zinitix,ss_touch_num", &pdata->ss_touch_num);
	input_info(true, dev, "%s: ss_touch_num:%d\n", __func__, pdata->ss_touch_num);
#endif
	return 0;
}
#endif

static void zt_display_rawdata(struct zt_ts_info *info, struct tsp_raw_data *raw_data, int type, int gap)
{
	int x_num = info->cap_info.x_node_num;
	int y_num = info->cap_info.y_node_num;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int tmp_rawdata;
	int i, j;

	pStr = kzalloc(6 * (x_num + 1), GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, 6 * (x_num + 1));
	snprintf(pTmp, sizeof(pTmp), "      Rx");
	strlcat(pStr, pTmp, 6 * (x_num + 1));

	for (i = 0; i < x_num; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d  ", i);
		strlcat(pStr, pTmp, 6 * (x_num + 1));
	}

	input_info(true, &info->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, 6 * (x_num + 1));
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, 6 * (x_num + 1));

	for (i = 0; i < x_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "-----");
		strlcat(pStr, pTmp, 6 * (x_num + 1));
	}

	input_info(true, &info->client->dev, "%s\n", pStr);

	for (i = 0; i < y_num; i++) {
		memset(pStr, 0x0, 6 * (x_num + 1));
		snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", i);
		strlcat(pStr, pTmp, 6 * (x_num + 1));

		for (j = 0; j < x_num; j++) {
			switch (type) {
			case TOUCH_REF_ABNORMAL_TEST_MODE:
				/* print mis_cal data (value - DEF_MIS_CAL_SPEC_MID) */
				tmp_rawdata = raw_data->reference_data_abnormal[(i * x_num) + j];
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);
				break;
			case TOUCH_DND_MODE:
				if (gap == 1) {
					/* print dnd v gap data */
					tmp_rawdata = raw_data->vgap_data[(i * x_num) + j];
					snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);
				} else if (gap == 2) {
					/* print dnd h gap data */
					tmp_rawdata = raw_data->hgap_data[(i * x_num) + j];
					snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);
				} else {
					/* print dnd data */
					tmp_rawdata = raw_data->dnd_data[(i * x_num) + j];
					snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);
				}
				break;
			case TOUCH_RAW_MODE:
				/* print cnd data */
				tmp_rawdata = raw_data->cnd_data[(i * x_num) + j];
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);
				break;
			case TOUCH_JITTER_MODE:
				/* print jitter data */
				tmp_rawdata = raw_data->jitter_data[(i * x_num) + j];
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);
				break;
			case TOUCH_REFERENCE_MODE:
				/* print reference data */
				tmp_rawdata = raw_data->reference_data[(i * x_num) + j];
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);
				break;
			case TOUCH_DELTA_MODE:
				/* print delta data */
				tmp_rawdata = raw_data->delta_data[(i * x_num) + j];
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);
				break;
			}
			strlcat(pStr, pTmp, 6 * (x_num + 1));
		}
		input_info(true, &info->client->dev, "%s\n", pStr);
	}

	kfree(pStr);
}

/* print raw data at booting time */
static void zt_display_rawdata_boot(struct zt_ts_info *info, struct tsp_raw_data *raw_data, int *min, int *max, bool is_mis_cal)
{
	int x_num = info->cap_info.x_node_num;
	int y_num = info->cap_info.y_node_num;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int tmp_rawdata;
	int i, j;

	input_raw_info(true, &info->client->dev, "%s: %s\n", __func__,
			is_mis_cal ? "mis_cal " : "dnd ");

	pStr = kzalloc(6 * (x_num + 1), GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, 6 * (x_num + 1));
	snprintf(pTmp, sizeof(pTmp), "      Rx");
	strlcat(pStr, pTmp, 6 * (x_num + 1));

	for (i = 0; i < x_num; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d  ", i);
		strlcat(pStr, pTmp, 6 * (x_num + 1));
	}

	input_raw_info(true, &info->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, 6 * (x_num + 1));
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, 6 * (x_num + 1));

	for (i = 0; i < x_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "-----");
		strlcat(pStr, pTmp, 6 * (x_num + 1));
	}

	input_raw_info(true, &info->client->dev, "%s\n", pStr);

	for (i = 0; i < y_num; i++) {
		memset(pStr, 0x0, 6 * (x_num + 1));
		snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", i);
		strlcat(pStr, pTmp, 6 * (x_num + 1));

		for (j = 0; j < x_num; j++) {

			if (is_mis_cal) {
				/* print mis_cal data (value - DEF_MIS_CAL_SPEC_MID) */
				tmp_rawdata = raw_data->reference_data_abnormal[(i * x_num) + j];
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);

				if (tmp_rawdata < *min)
					*min = tmp_rawdata;

				if (tmp_rawdata > *max)
					*max = tmp_rawdata;

			} else {
				/* print dnd data */
				tmp_rawdata = raw_data->dnd_data[(i * x_num) + j];
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);

				if (tmp_rawdata < *min && tmp_rawdata != 0)
					*min = tmp_rawdata;

				if (tmp_rawdata > *max)
					*max = tmp_rawdata;
			}
			strlcat(pStr, pTmp, 6 * (x_num + 1));
		}
		input_raw_info(true, &info->client->dev, "%s\n", pStr);
	}

	input_raw_info(true, &info->client->dev, "Max/Min %d,%d ##\n", *max, *min);

	kfree(pStr);
}

static void zt_run_dnd(struct zt_ts_info *info)
{
	struct tsp_raw_data *raw_data = info->raw_data;
	int min = 0xFFFF, max = -0xFF;
	int ret;

	zt_ts_esd_timer_stop(info);
	ret = ts_set_touchmode(info, TOUCH_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->dnd_data, 1);
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	zt_display_rawdata_boot(info, raw_data, &min, &max, false);

out:
	zt_ts_esd_timer_start(info);
}

static void zt_run_mis_cal(struct zt_ts_info *info)
{
	struct zt_ts_platform_data *pdata = info->pdata;
	struct tsp_raw_data *raw_data = info->raw_data;

	char mis_cal_data = 0xF0;
	int ret = 0;
	s16 raw_data_buff[TSP_CMD_NODE_NUM];
	u16 chip_eeprom_info;
	int min = 0xFFFF, max = -0xFF;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	if (pdata->mis_cal_check == 0) {
		input_info(true, &info->client->dev, "%s: [ERROR] not support\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	if (info->work_state == SUSPEND) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		mis_cal_data = 0xF2;
		goto NG;
	}

	if (read_data(info->client, ZT_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0) {
		input_info(true, &info->client->dev, "%s: read eeprom_info i2c fail!\n", __func__);
		mis_cal_data = 0xF3;
		goto NG;
	}

	if (zinitix_bit_test(chip_eeprom_info, 0)) {
		input_info(true, &info->client->dev, "%s: eeprom cal 0, skip !\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	ret = ts_set_touchmode(info, TOUCH_REF_ABNORMAL_TEST_MODE);
	if (ret < 0) {
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ret = get_raw_data(info, (u8 *)raw_data->reference_data_abnormal, 2);
	if (!ret) {
		input_info(true, &info->client->dev, "%s:[ERROR] i2c fail!\n", __func__);
		ts_set_touchmode(info, TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ts_set_touchmode(info, TOUCH_POINT_MODE);

	zt_display_rawdata_boot(info, raw_data, &min, &max, true);
	if ((min + DEF_MIS_CAL_SPEC_MID) < DEF_MIS_CAL_SPEC_MIN ||
			(max + DEF_MIS_CAL_SPEC_MID) > DEF_MIS_CAL_SPEC_MAX) {
		mis_cal_data = 0xFD;
		goto NG;
	}

	mis_cal_data = 0x00;
	input_info(true, &info->client->dev, "%s : mis_cal_data: %X\n", __func__, mis_cal_data);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return;
NG:
	input_err(true, &info->client->dev, "%s : mis_cal_data: %X\n", __func__, mis_cal_data);
	if (mis_cal_data == 0xFD) {
		run_tsp_rawdata_read(&info->sec, 7, raw_data_buff);
		run_tsp_rawdata_read(&info->sec, TOUCH_REFERENCE_MODE, raw_data_buff);
	}
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
}

static void zt_run_rawdata(struct zt_ts_info *info)
{
	info->tsp_dump_lock = 1;
	input_raw_data_clear();
	input_raw_info(true, &info->client->dev, "%s: start ##\n", __func__);
	zt_run_dnd(info);
	zt_run_mis_cal(info);
	input_raw_info(true, &info->client->dev, "%s: done ##\n", __func__);
	info->tsp_dump_lock = 0;
}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
#include "../../../sec_input/sec_tsp_dumpkey.h"
static struct delayed_work *p_ghost_check;

static void zt_check_rawdata(struct work_struct *work)
{
	struct zt_ts_info *info = container_of(work, struct zt_ts_info,
			ghost_check.work);

	if (info->tsp_dump_lock == 1) {
		input_info(true, &info->client->dev, "%s: ignored ## already checking..\n", __func__);
		return;
	}

	if (info->plat_data->power_enabled == false) {
		input_info(true, &info->client->dev, "%s: ignored ## IC is power off\n", __func__);
		return;
	}

	zt_run_rawdata(info);
}

static void dump_tsp_log(struct device *dev)
{
	pr_info("%s: %s %s: start\n", ZT_TS_DEVICE, SECLOG, __func__);

	if (p_ghost_check == NULL) {
		pr_err("%s: %s %s: ignored ## tsp probe fail!!\n", ZT_TS_DEVICE, SECLOG, __func__);
		return;
	}
	schedule_delayed_work(p_ghost_check, msecs_to_jiffies(100));
}
#endif

static void zt_read_info_work(struct work_struct *work)
{
	struct zt_ts_info *info = container_of(work, struct zt_ts_info,
			work_read_info.work);
#ifdef TCLM_CONCEPT
	u8 data[2] = {0};
	int ret;
#endif

	mutex_lock(&info->modechange);

#ifdef TCLM_CONCEPT
	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_FAC_RESULT, (u8 *)data, 2);
	info->test_result.data[0] = data[0];

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	ret = sec_tclm_check_cal_case(info->tdata);
	input_info(true, &info->client->dev, "%s: sec_tclm_check_cal_case result %d; test result %X\n",
			__func__, ret, info->test_result.data[0]);

	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
#endif
	run_test_open_short(info);

	input_log_fix();
	zt_run_rawdata(info);
	info->info_work_done = true;
	mutex_unlock(&info->modechange);
}

void zt_print_info(struct zt_ts_info *info)
{
	u32 fw_version = 0;

	if (!info)
		return;

	if (!info->client)
		return;

	info->plat_data->print_info_cnt_open++;

	if (info->plat_data->print_info_cnt_open > 0xfff0)
		info->plat_data->print_info_cnt_open = 0;

	if (info->finger_cnt1 == 0)
		info->plat_data->print_info_cnt_release++;

	fw_version =  (u32)((u32)(info->cap_info.ic_revision & 0xff) << 24)
		| ((info->cap_info.fw_minor_version & 0xff) << 16)
		| ((info->cap_info.hw_id & 0xff) << 8) | (info->cap_info.reg_data_version & 0xff);

	input_info(true, &info->client->dev,
			"tc:%d noise:%s(%d) cover:%d lp:(%x) fod:%d ED:%d // v:ZI%08X C%02XT%04X.%4s%s // #%d %d\n",
			info->finger_cnt1, info->noise_flag > 0 ? "ON":"OFF", info->noise_flag, info->flip_cover_flag,
			info->lpm_mode, info->fod_mode_set, info->plat_data->ed_enable,
			fw_version,
#ifdef TCLM_CONCEPT
			info->tdata->nvdata.cal_count, info->tdata->nvdata.tune_fix_ver,
			info->tdata->tclm_string[info->tdata->nvdata.cal_position].f_name,
			(info->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L" : " ",
#else
			0, 0, " ", " ",
#endif
			info->plat_data->print_info_cnt_open, info->plat_data->print_info_cnt_release);
}

static void touch_print_info_work(struct work_struct *work)
{
	struct zt_ts_info *info = container_of(work, struct zt_ts_info,
			work_print_info.work);

	zt_print_info(info);

	if (!shutdown_is_on_going_tsp)
		schedule_delayed_work(&info->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

static void zt_ts_set_input_prop(struct zt_ts_info *info, struct input_dev *dev, u8 propbit)
{
	static char zt_phys[64] = { 0 };

	snprintf(zt_phys, sizeof(zt_phys),
			"%s/input1", dev->name);
	dev->id.bustype = BUS_I2C;
	dev->phys = zt_phys;
	dev->dev.parent = &info->client->dev;

	set_bit(EV_SYN, dev->evbit);
	set_bit(EV_KEY, dev->evbit);
	set_bit(EV_ABS, dev->evbit);
	set_bit(BTN_TOUCH, dev->keybit);
	set_bit(propbit, dev->propbit);
	set_bit(KEY_INT_CANCEL, dev->keybit);
	set_bit(EV_LED, dev->evbit);
	set_bit(LED_MISC, dev->ledbit);
	set_bit(BTN_PALM, dev->keybit);

	set_bit(KEY_BLACK_UI_GESTURE, dev->keybit);
	set_bit(KEY_WAKEUP, dev->keybit);


	input_set_abs_params(dev, ABS_MT_POSITION_X,
			0, info->plat_data->max_x + ABS_PT_OFFSET,	0, 0);
	input_set_abs_params(dev, ABS_MT_POSITION_Y,
			0, info->plat_data->max_y + ABS_PT_OFFSET,	0, 0);
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	input_set_abs_params(dev, ABS_MT_PRESSURE,
			0, 3000, 0, 0);
#endif
	input_set_abs_params(dev, ABS_MT_TOUCH_MAJOR,
			0, 255, 0, 0);
	input_set_abs_params(dev, ABS_MT_WIDTH_MAJOR,
			0, 255, 0, 0);
	input_set_abs_params(dev, ABS_MT_TOUCH_MINOR,
			0, 255, 0, 0);

	set_bit(MT_TOOL_FINGER, dev->keybit);
	if (propbit == INPUT_PROP_POINTER)
		input_mt_init_slots(dev, MAX_SUPPORTED_FINGER_NUM, INPUT_MT_POINTER);
	else
		input_mt_init_slots(dev, MAX_SUPPORTED_FINGER_NUM, INPUT_MT_DIRECT);

	input_set_drvdata(dev, info);
}

static void zt_ts_set_input_prop_proximity(struct zt_ts_info *info, struct input_dev *dev)
{
	static char zt_phys[64] = { 0 };

	snprintf(zt_phys, sizeof(zt_phys), "%s/input1", dev->name);
	dev->phys = zt_phys;
	dev->id.bustype = BUS_I2C;
	dev->dev.parent = &info->client->dev;

	set_bit(EV_SYN, dev->evbit);
	set_bit(EV_SW, dev->evbit);

	set_bit(INPUT_PROP_DIRECT, dev->propbit);

	input_set_abs_params(dev, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);
	input_set_drvdata(dev, info);
}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
static struct zt_ts_info *stui_ts;
extern int stui_i2c_lock(struct i2c_adapter *adap);
extern int stui_i2c_unlock(struct i2c_adapter *adap);

static int zt_stui_tsp_enter(void)
{
	int ret = 0;

	if (!stui_ts)
		return -EINVAL;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(stui_ts);
	write_reg(stui_ts->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, 0);
#endif

	disable_irq(stui_ts->irq);
	clear_report_data(stui_ts);

	ret = stui_i2c_lock(stui_ts->client->adapter);
	if (ret) {
		pr_err("[STUI] stui_i2c_lock failed : %d\n", ret);
		enable_irq(stui_ts->client->irq);
		return -1;
	}

	return 0;
}

static int zt_stui_tsp_exit(void)
{
	int ret = 0;

	if (!stui_ts)
		return -EINVAL;

	ret = stui_i2c_unlock(stui_ts->client->adapter);
	if (ret)
		pr_err("[STUI] stui_i2c_unlock failed : %d\n", ret);

	enable_irq(stui_ts->irq);

#if ESD_TIMER_INTERVAL
	write_reg(stui_ts->client, ZT_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
	esd_timer_start(CHECK_ESD_TIMER, stui_ts);
#endif

	return ret;
}

static int zt_stui_tsp_type(void)
{
	return STUI_TSP_TYPE_ZINITIX;
}
#endif

static int zt_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
//	struct zt_ts_platform_data *pdata = client->dev.platform_data;
	struct zt_ts_platform_data *pdata = NULL;
	struct sec_ts_plat_data *plat_data = NULL;
#ifdef TCLM_CONCEPT
	struct sec_tclm_data *tdata = NULL;
#endif
	struct zt_ts_info *info;
	struct device_node *np = client->dev.of_node;
	int ret = 0;
	bool force_update = false;

	input_info(true, &client->dev, "%s: start probe\n", __func__);

	if (client->dev.of_node) {
		plat_data = devm_kzalloc(&client->dev,
				sizeof(struct sec_ts_plat_data), GFP_KERNEL);
		if (!plat_data) {
			input_err(true, &client->dev, "%s: Failed to alloc plat_data\n", __func__);
			return -ENOMEM;
		}

		client->dev.platform_data = plat_data;
		ret = sec_input_parse_dt(&client->dev);
		if (ret) {
			input_err(true, &client->dev, "%s: Failed to parse dt\n", __func__);
			goto err_no_platform_data;
		}

		pdata = devm_kzalloc(&client->dev,
				sizeof(struct zt_ts_platform_data), GFP_KERNEL);
		if (!pdata) {
			input_err(true, &client->dev, "%s: Failed to alloc pdata\n", __func__);
			goto error_allocate_pdata;
		}


		ret = zt_ts_parse_dt(np, &client->dev, pdata);
		if (ret) {
			input_err(true, &client->dev, "Error parsing dt %d\n", ret);
			goto err_platform_data;
		}

#ifdef TCLM_CONCEPT
		tdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_tclm_data), GFP_KERNEL);
		if (!tdata)
			goto error_allocate_tdata;

		sec_tclm_parse_dt(&client->dev, tdata);
#endif
	} else if (!pdata) {
		input_err(true, &client->dev, "%s: Not exist platform data\n", __func__);
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s: Not compatible i2c function\n", __func__);
		return -EIO;
	}

	info = kzalloc(sizeof(struct zt_ts_info), GFP_KERNEL);
	if (!info) {
		input_err(true, &client->dev, "%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, info);
	info->client = client;
	info->pdata = pdata;
	info->plat_data = plat_data;
	info->plat_data->dev = &info->client->dev;
	info->plat_data->power = sec_input_power;

	info->tsp_page_size = TSP_PAGE_SIZE;
	info->fuzing_udelay = FUZING_UDELAY;

	input_info(true, &client->dev, "%s: tsp_page_size %d, fuzing_udelay:%d, FW:%s, CN:%s\n",
				__func__, info->tsp_page_size, info->fuzing_udelay,
				plat_data->firmware_name, pdata->chip_name);

#ifdef TCLM_CONCEPT
	info->tdata = tdata;
	if (!info->tdata)
		goto error_null_data;

	sec_tclm_initialize(info->tdata);
	info->tdata->dev = &info->client->dev;
//	info->tdata->client = info->client;
	info->tdata->tclm_read = zt_tclm_data_read;
	info->tdata->tclm_write = zt_tclm_data_write;
	info->tdata->tclm_execute_force_calibration = zt_tclm_execute_force_calibration;
	info->tdata->tclm_parse_dt = sec_tclm_parse_dt;
#endif
	INIT_DELAYED_WORK(&info->work_read_info, zt_read_info_work);
	INIT_DELAYED_WORK(&info->work_print_info, touch_print_info_work);

	mutex_init(&info->modechange);
	mutex_init(&info->set_reg_lock);
	mutex_init(&info->set_lpmode_lock);
	mutex_init(&info->work_lock);
	mutex_init(&info->raw_data_lock);
	mutex_init(&info->i2c_mutex);
	mutex_init(&info->sponge_mutex);
	mutex_init(&info->power_init);
#if ESD_TIMER_INTERVAL
	mutex_init(&info->lock);
	INIT_WORK(&info->tmr_work, ts_tmr_work);
#endif

	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		input_err(true, &client->dev, "%s: Failed to allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_alloc;
	}

	if (info->plat_data->support_dex) {
		info->plat_data->input_dev_pad = input_allocate_device();
		if (!info->plat_data->input_dev_pad) {
			input_err(true, &client->dev, "%s: allocate device err!\n", __func__);
			ret = -ENOMEM;
			goto err_allocate_input_dev_pad;
		}
	}

	if (info->plat_data->support_ear_detect) {
		info->plat_data->input_dev_proximity = input_allocate_device();
		if (!info->plat_data->input_dev_proximity) {
			input_err(true, &client->dev, "%s: allocate input_dev_proximity err!\n", __func__);
			ret = -ENOMEM;
			goto err_allocate_input_dev_proximity;
		}

		info->plat_data->input_dev_proximity->name = "sec_touchproximity";
		zt_ts_set_input_prop_proximity(info, info->plat_data->input_dev_proximity);
	}

	info->plat_data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(info->plat_data->pinctrl)) {
		input_err(true, &client->dev, "%s: Failed to get pinctrl data\n", __func__);
		ret = PTR_ERR(info->plat_data->pinctrl);
		goto err_get_pinctrl;
	}

	info->work_state = PROBE;

	// power on
	if (zt_power_control(info, true) == false) {
		input_err(true, &info->client->dev,
				"%s: POWER_ON_SEQUENCE failed", __func__);
		force_update = true;
	}

	/* init touch mode */
	info->touch_mode = TOUCH_POINT_MODE;
	misc_info = info;

#if ESD_TIMER_INTERVAL
	esd_tmr_workqueue =
		create_singlethread_workqueue("esd_tmr_workqueue");

	if (!esd_tmr_workqueue) {
		input_err(true, &client->dev, "%s: Failed to create esd tmr work queue\n", __func__);
		ret = -EPERM;

		goto err_esd_sequence;
	}

	esd_timer_init(info);
#endif

	ret = ic_version_check(info);
	if (ret < 0) {
		input_err(true, &info->client->dev,
				"%s: fail version check", __func__);
		force_update = true;
	}

	ret = fw_update_work(info, force_update);
	if (ret < 0) {
		ret = -EPERM;
		input_err(true, &info->client->dev,
				"%s: fail update_work", __func__);
		goto err_fw_update;
	}

	info->input_dev->name = "sec_touchscreen";
	zt_ts_set_input_prop(info, info->input_dev, INPUT_PROP_DIRECT);
	ret = input_register_device(info->input_dev);
	if (ret) {
		input_info(true, &client->dev, "unable to register %s input device\r\n",
				info->input_dev->name);
		goto err_input_register_device;
	}

	if (info->plat_data->support_dex) {
		info->plat_data->input_dev_pad->name = "sec_touchpad";
		zt_ts_set_input_prop(info, info->plat_data->input_dev_pad, INPUT_PROP_POINTER);
	}

	if (info->plat_data->support_dex) {
		ret = input_register_device(info->plat_data->input_dev_pad);
		if (ret) {
			input_err(true, &client->dev, "%s: Unable to register %s input device\n",
					__func__, info->plat_data->input_dev_pad->name);
			goto err_input_pad_register_device;
		}
	}

	if (info->plat_data->support_ear_detect) {
		ret = input_register_device(info->plat_data->input_dev_proximity);
		if (ret) {
			input_err(true, &client->dev, "%s: Unable to register %s input device\n",
					__func__, info->plat_data->input_dev_proximity->name);
			goto err_input_proximity_register_device;
		}
	}

	info->plat_data->enable = zt_ts_open;
	info->plat_data->disable = zt_ts_close;

	if (init_touch(info) == false) {
		ret = -EPERM;
		goto err_init_touch;
	}

	info->work_state = NOTHING;

	init_completion(&info->plat_data->resume_done);
	complete_all(&info->plat_data->resume_done);

	/* configure irq */
	info->irq = gpio_to_irq(plat_data->irq_gpio);
	if (info->irq < 0) {
		input_info(true, &client->dev, "%s: failed to get gpio_to_irq\n", __func__);
		ret = -EINVAL;
		goto error_gpio_irq;
	}

	/* ret = request_threaded_irq(info->irq, ts_int_handler, zt_touch_work,*/
	ret = request_threaded_irq(info->irq, NULL, zt_touch_work,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT, ZT_TS_DEVICE, info);

	if (ret) {
		input_info(true, &client->dev, "unable to register irq.(%s)\r\n",
				info->input_dev->name);
		goto err_request_irq;
	}
	input_info(true, &client->dev, "%s\n", __func__);

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
	trustedui_set_tsp_irq(info->irq);
	input_info(true, &client->dev, "%s[%d] called!\n",
			__func__, info->irq);
#endif

#ifdef USE_MISC_DEVICE
	ret = misc_register(&touch_misc_device);
	if (ret) {
		input_err(true, &client->dev, "%s: Failed to register touch misc device\n", __func__);
		goto err_misc_register;
	}
#endif
	ret = init_sec_factory(info);
	if (ret) {
		input_err(true, &client->dev, "%s: Failed to init sec factory device\n", __func__);

		goto err_kthread_create_failed;
	}

	info->register_cb = info->pdata->register_cb;

	info->callbacks.inform_charger = zt_charger_status_cb;
	if (info->register_cb)
		info->register_cb(&info->callbacks);
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&info->vbus_nb, tsp_vbus_notification,
			VBUS_NOTIFY_DEV_CHARGER);
#endif
	device_init_wakeup(info->sec.fac_dev, true);

#if 0 //IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
	tui_tsp_info = info;
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	stui_ts = info;
	stui_tsp_init(zt_stui_tsp_enter, zt_stui_tsp_exit, zt_stui_tsp_type);
	input_info(true, &stui_ts->client->dev, "secure touch support\n");
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (sysfs_create_group(&info->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &info->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(info);
#endif
	ts_check_custom_library(info);

	schedule_delayed_work(&info->work_read_info, msecs_to_jiffies(5000));

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	sec_input_dumpkey_register(MULTI_DEV_NONE, dump_tsp_log, &info->client->dev);
	INIT_DELAYED_WORK(&info->ghost_check, zt_check_rawdata);
	p_ghost_check = &info->ghost_check;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (info->pdata->ss_touch_num > 0)
		sec_secure_touch_register(info, &info->client->dev, info->pdata->ss_touch_num, &info->input_dev->dev.kobj);
#endif

	atomic_set(&info->plat_data->enabled, 1);

	input_log_fix();
	return 0;

err_kthread_create_failed:
	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);
	kfree(info->raw_data);
#ifdef USE_MISC_DEVICE
	misc_deregister(&touch_misc_device);
err_misc_register:
#endif
	free_irq(info->irq, info);
err_request_irq:
error_gpio_irq:
err_init_touch:
	info->plat_data->enable = NULL;
	info->plat_data->disable = NULL;
	if (info->plat_data->support_ear_detect) {
		input_unregister_device(info->plat_data->input_dev_proximity);
		info->plat_data->input_dev_proximity = NULL;
	}
err_input_proximity_register_device:
	if (info->plat_data->support_dex) {
		input_unregister_device(info->plat_data->input_dev_pad);
		info->plat_data->input_dev_pad = NULL;
	}
err_input_pad_register_device:
	input_unregister_device(info->input_dev);
	info->input_dev = NULL;
err_input_register_device:
err_fw_update:
#if ESD_TIMER_INTERVAL
	del_timer(&(info->esd_timeout_tmr));
err_esd_sequence:
#endif
	zt_power_control(info, false);
err_get_pinctrl:
	if (info->plat_data->support_ear_detect) {
		if (info->plat_data->input_dev_proximity)
			input_free_device(info->plat_data->input_dev_proximity);
	}
err_allocate_input_dev_proximity:
	if (info->plat_data->support_dex) {
		if (info->plat_data->input_dev_pad)
			input_free_device(info->plat_data->input_dev_pad);
	}
err_allocate_input_dev_pad:
	if (info->input_dev)
		input_free_device(info->input_dev);
#ifdef TCLM_CONCEPT
error_null_data:
#endif
err_alloc:
	kfree(info);
error_allocate_tdata:
err_platform_data:
error_allocate_pdata:
err_no_platform_data:

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	p_ghost_check = NULL;
#endif
	input_info(true, &client->dev, "%s: Failed to probe\n", __func__);
	input_log_fix();
	return ret;
}

static int zt_ts_dev_remove(struct i2c_client *client)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);
	struct sec_ts_plat_data *plat_data = info->plat_data;

	disable_irq(info->irq);
	mutex_lock(&info->work_lock);

	info->work_state = REMOVE;

	cancel_delayed_work_sync(&info->work_read_info);
	cancel_delayed_work_sync(&info->work_print_info);

	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);
	kfree(info->raw_data);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	cancel_delayed_work_sync(&info->ghost_check);
	sec_input_dumpkey_unregister(MULTI_DEV_NONE);
	p_ghost_check = NULL;
#endif

#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	write_reg(info->client, ZT_PERIODICAL_INTERRUPT_INTERVAL, 0);
	esd_timer_stop(info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "%s: Stopped esd timer\n", __func__);
#endif
	destroy_workqueue(esd_tmr_workqueue);
#endif

	if (info->irq)
		free_irq(info->irq, info);
#ifdef USE_MISC_DEVICE
	misc_deregister(&touch_misc_device);
#endif

	if (gpio_is_valid(plat_data->irq_gpio) != 0)
		gpio_free(plat_data->irq_gpio);

	if (info->plat_data->support_dex) {
		input_mt_destroy_slots(info->plat_data->input_dev_pad);
		input_unregister_device(info->plat_data->input_dev_pad);
	}

	if (info->plat_data->support_ear_detect) {
		input_mt_destroy_slots(info->plat_data->input_dev_proximity);
		input_unregister_device(info->plat_data->input_dev_proximity);
	}

	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);
	mutex_unlock(&info->work_lock);
	kfree(info);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
void zt_ts_remove(struct i2c_client *client)
{
	zt_ts_dev_remove(client);
}
#else
int zt_ts_remove(struct i2c_client *client)
{
	zt_ts_dev_remove(client);
	return 0;
}
#endif

void zt_ts_shutdown(struct i2c_client *client)
{
	struct zt_ts_info *info = i2c_get_clientdata(client);

	input_info(true, &client->dev, "%s++\n", __func__);
	shutdown_is_on_going_tsp = true;
	disable_irq(info->irq);
	mutex_lock(&info->work_lock);
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	esd_timer_stop(info);
#endif
	mutex_unlock(&info->work_lock);
	zt_power_control(info, false);
	input_info(true, &client->dev, "%s--\n", __func__);
}

#if IS_ENABLED(CONFIG_PM)
static int zt_ts_pm_suspend(struct device *dev)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);

	reinit_completion(&info->plat_data->resume_done);

	return 0;
}

static int zt_ts_pm_resume(struct device *dev)
{
	struct zt_ts_info *info = dev_get_drvdata(dev);

	complete_all(&info->plat_data->resume_done);

	return 0;
}

static const struct dev_pm_ops zt_ts_dev_pm_ops = {
	.suspend = zt_ts_pm_suspend,
	.resume = zt_ts_pm_resume,
};
#endif

static struct i2c_device_id zt_idtable[] = {
	{ZT_TS_DEVICE, 0},
	{ }
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id zinitix_match_table[] = {
	{ .compatible = "zinitix,zt_ts_device",},
	{},
};
#endif

static struct i2c_driver zt_ts_driver = {
	.probe	= zt_ts_probe,
	.remove	= zt_ts_remove,
	.shutdown = zt_ts_shutdown,
	.id_table	= zt_idtable,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= ZT_TS_DEVICE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = zinitix_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &zt_ts_dev_pm_ops,
#endif
	},
};

static int __init zt_ts_init(void)
{
	pr_info("[sec_input]%s\n", __func__);
	return i2c_add_driver(&zt_ts_driver);
}

static void __exit zt_ts_exit(void)
{
	i2c_del_driver(&zt_ts_driver);
}

module_init(zt_ts_init);
module_exit(zt_ts_exit);

MODULE_DESCRIPTION("touch-screen device driver using i2c interface");
MODULE_LICENSE("GPL");
