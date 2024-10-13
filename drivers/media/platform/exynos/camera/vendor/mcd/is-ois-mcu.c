// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <soc/samsung/exynos-pmu.h>

#include <exynos-is-sensor.h>
#include "is-device-sensor-peri.h"
#include "is-hw-api-common.h"
#include "is-hw-api-ois-mcu.h"
#include "is-ois-mcu.h"
#include "is-device-ois.h"
#include "is-sfr-ois-mcu-v1_1_1.h"
#ifdef CONFIG_AF_HOST_CONTROL
#include "is-device-af.h"
#endif

int debug_ois_mcu;
module_param(debug_ois_mcu, int, 0644);

static const struct v4l2_subdev_ops subdev_ops;
static bool ois_wide_init;
static bool ois_tele_init;
static bool ois_hw_check;
static bool ois_fadeupdown;
static u16 ois_center_x;
static u16 ois_center_y;
static struct mcu_default_data mcu_init;

static int ois_mcu_clk_get(struct ois_mcu_dev *mcu)
{
	mcu->clk = devm_clk_get(mcu->dev, "user_mux");
	if (!IS_ERR(mcu->clk))
		return 0;

	if (PTR_ERR(mcu->clk) != -ENOENT) {
		dev_err(mcu->dev, "Failed to get 'user_mux' clock: %ld",
			PTR_ERR(mcu->clk));
		return PTR_ERR(mcu->clk);
	}

	dev_info(mcu->dev, "[@] 'user_mux' clock is not present\n");

	return -EIO;
}

static void ois_mcu_clk_put(struct ois_mcu_dev *mcu)
{
	if (!IS_ERR(mcu->clk))
		clk_put(mcu->clk);
}

static int ois_mcu_clk_enable(struct ois_mcu_dev *mcu)
{
	int ret = 0;

	if (IS_ERR(mcu->clk)) {
		dev_info(mcu->dev, "[@] 'user_mux' clock is not present\n");
		return -EIO;
	}

	ret = clk_prepare_enable(mcu->clk);
	if (ret) {
		dev_err(mcu->dev, "%s: failed to enable clk (err %d)\n",
					__func__, ret);
		return ret;
	}

	return ret;
}

static void ois_mcu_clk_disable(struct ois_mcu_dev *mcu)
{
	if (!IS_ERR(mcu->clk))
		clk_disable_unprepare(mcu->clk);
}

static int ois_mcu_runtime_resume(struct device *dev)
{
	struct ois_mcu_dev *mcu = dev_get_drvdata(dev);
	int ret = 0;

	ret = ois_mcu_clk_get(mcu);
	if (ret)
		return ret;

	ret = ois_mcu_clk_enable(mcu);

	ret = __is_mcu_hw_enable(mcu->regs[OM_REG_CORE]);

	clear_bit(OM_HW_SUSPENDED, &mcu->state);

	return ret;
}

static int ois_mcu_runtime_suspend(struct device *dev)
{
	struct ois_mcu_dev *mcu = dev_get_drvdata(dev);
	int ret = 0;

	ret = __is_mcu_hw_disable(mcu->regs[OM_REG_CORE]);

	ois_mcu_clk_disable(mcu);
	ois_mcu_clk_put(mcu);

	set_bit(OM_HW_SUSPENDED, &mcu->state);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int ois_mcu_resume(struct device *dev)
{
	/* TODO: */
	return 0;
}

static int ois_mcu_suspend(struct device *dev)
{
	struct ois_mcu_dev *mcu = dev_get_drvdata(dev);

	/* TODO: */
	if (!test_bit(OM_HW_SUSPENDED, &mcu->state))
		return -EBUSY;

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static irqreturn_t is_isr_ois_mcu(int irq, void *data)
{
	struct ois_mcu_dev *mcu;
	unsigned int state;

	mcu = (struct ois_mcu_dev *)data;
	state = is_mcu_hw_g_irq_state(mcu->regs[OM_REG_CORE], true);

	/* FIXME: temp log for testing */
	info_mcu("IRQ: %d\n", state);
	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_WDT)) {
		/* TODO: WDR IRQ handling */
		dbg_mcu(1, "IRQ: MCU_IRQ_WDT");
	}

	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_WDT_RST)) {
		/* TODO: WDR RST handling */
		dbg_mcu(1, "IRQ: MCU_IRQ_WDT_RST");
	}

	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_LOCKUP_RST)) {
		/* TODO: LOCKUP RST handling */
		dbg_mcu(1, "IRQ: MCU_IRQ_LOCKUP_RST");
	}

	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_SYS_RST)) {
		/* TODO: SYS RST handling */
		dbg_mcu(1, "IRQ: MCU_IRQ_SYS_RST");
	}

	return IRQ_HANDLED;
}

/*
 * API functions
 */
int ois_mcu_power_ctrl(struct ois_mcu_dev *mcu, int on)
{
	int ret = 0;
#if defined(CONFIG_PM)
	int rpm_ret;
#endif
	BUG_ON(!mcu);

	if (on) {
		if (!test_bit(OM_HW_SUSPENDED, &mcu->state)) {
			warning_mcu("already power on\n");
			goto p_err;
		}
#if defined(CONFIG_PM)
		rpm_ret = pm_runtime_get_sync(mcu->dev);
		if (rpm_ret < 0)
			err_mcu("pm_runtime_get_sync() err: %d", rpm_ret);
#else
		ret = ois_mcu_runtime_resume(mcu->dev);
#endif
		clear_bit(OM_HW_SUSPENDED, &mcu->state);
	} else {
		if (test_bit(OM_HW_SUSPENDED, &mcu->state)) {
			warning_mcu("already power off\n");
			goto p_err;
		}
#if defined(CONFIG_PM)
		rpm_ret = pm_runtime_put_sync(mcu->dev);
		if (rpm_ret < 0)
			err_mcu("pm_runtime_put_sync() err: %d", rpm_ret);
#else
		ret = ois_mcu_runtime_suspend(mcu->dev);
#endif
		set_bit(OM_HW_SUSPENDED, &mcu->state);
		clear_bit(OM_HW_FW_LOADED, &mcu->state);
		clear_bit(OM_HW_RUN, &mcu->state);
	}

	info_mcu("%s: (%d) done\n", __func__, on);

p_err:
	return ret;
}

int ois_mcu_load_binary(struct ois_mcu_dev *mcu)
{
	int ret = 0;
	long size = 0;

	BUG_ON(!mcu);

	if (test_bit(OM_HW_FW_LOADED, &mcu->state)) {
		warning_mcu("already fw was loaded\n");
		return ret;
	}

	size = __is_mcu_load_fw(mcu->regs[OM_REG_CORE], mcu->dev);

	if (size <= 0)
		return -EINVAL;

	set_bit(OM_HW_FW_LOADED, &mcu->state);

	return ret;
}

int ois_mcu_core_ctrl(struct ois_mcu_dev *mcu, int on)
{
	int ret;

	BUG_ON(!mcu);

	if (on) {
		if (test_bit(OM_HW_RUN, &mcu->state)) {
			warning_mcu("already started\n");
			return ret;
		}
		__is_mcu_hw_s_irq_enable(mcu->regs[OM_REG_CORE], 0xF);
		set_bit(OM_HW_RUN, &mcu->state);
	} else {
		if (!test_bit(OM_HW_RUN, &mcu->state)) {
			warning_mcu("already stopped\n");
			return ret;
		}
		clear_bit(OM_HW_RUN, &mcu->state);
	}

	ret = __is_mcu_core_control(mcu->regs[OM_REG_CORE], on);

	info_mcu("%s: %d\n", __func__, on);

	return ret;
}

int ois_mcu_dump(struct ois_mcu_dev *mcu, int type)
{
	int ret = 0;

	BUG_ON(!mcu);

	if (test_bit(OM_HW_SUSPENDED, &mcu->state))
		return 0;

	switch (type) {
	case OM_REG_CORE:
		__is_mcu_hw_cr_dump(mcu->regs[OM_REG_CORE]);
		__is_mcu_hw_sram_dump(mcu->regs[OM_REG_CORE],
			__is_mcu_get_sram_size());
		break;
	case OM_REG_PERI1:
		__is_mcu_hw_peri1_dump(mcu->regs[OM_REG_CORE]);
		break;
	case OM_REG_PERI2:
		__is_mcu_hw_peri2_dump(mcu->regs[OM_REG_CORE]);
		break;
	default:
		err_mcu("undefined type (%d)\n", type);
	}

	return ret;
}

int ois_mcu_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
#ifdef USE_OIS_SLEEP_MODE
	u8 read_gyrocalcen = 0;
#endif
	u8 val = 0;
	u8 gyro_orientation = 0;
	u8 wx_pole = 0;
	u8 wy_pole = 0;
#ifdef CAMERA_2ND_OIS
	u8 tx_pole = 0;
	u8 ty_pole = 0;
#endif
	int retries = 10;
	int i = 0;
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_ois *ois = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_ois_info *ois_pinfo = NULL;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_peri = is_mcu->sensor_peri;
	if (!sensor_peri) {
		err("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_sec_get_ois_pinfo(&ois_pinfo);

	ois = is_mcu->ois;
	ois->ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->coef = 0;
	ois->pre_coef = 255;
	ois->fadeupdown = false;
	ois->initial_centering_mode = false;
	ois->af_pos_wide = 0;
	ois->af_pos_tele = 0;
#ifdef CAMERA_2ND_OIS
	ois->ois_power_mode = -1;
#endif
	ois_pinfo->reset_check = false;

	if ((ois_wide_init == true && module->position == SENSOR_POSITION_REAR) ||
		(ois_tele_init == true && module->position == SENSOR_POSITION_REAR2)) {
		info("%s %d sensor(%d) is already initialized.\n", __func__, __LINE__, ois->device);
		ois_wide_init = ois_tele_init = true;
		ois->ois_shift_available = true;
	}

	if (!ois_hw_check) {
		do {
			val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
			msleep(3);
			if (--retries < 0) {
				err("Read status failed!!!!, data = 0x%04x\n", val);
				break;
			}
		} while (val != 0x01);

		if (val == 0x01) {
			/* write wide/tele xgg ygg */
			for (i = 0; i < 4; i++) {
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_XGG1 + i,
					ois_pinfo->wide_xgg[i]);
			}
			for (i = 0; i < 4; i++) {
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_YGG1 + i,
					ois_pinfo->wide_ygg[i]);
			}
			for (i = 0; i < 4; i++) {
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_XGG1 + i,
					ois_pinfo->tele_xgg[i]);
			}
			for (i = 0; i < 4; i++) {
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_YGG1 + i,
					ois_pinfo->tele_ygg[i]);
			}

			/* write wide/tele xcoef ycoef */
			for (i = 0; i < 2; i++) {
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_XCOEF_M1 + i,
					ois_pinfo->wide_xcoef[i]);
			}
			for (i = 0; i < 2; i++) {
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_YCOEF_M1 + i,
					ois_pinfo->wide_ycoef[i]);
			}
			for (i = 0; i < 2; i++) {
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_XCOEF_M2 + i,
					ois_pinfo->tele_xcoef[i]);
			}
			for (i = 0; i < 2; i++) {
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_YCOEF_M2 + i,
					ois_pinfo->tele_ycoef[i]);
			}

			/* enable dual cal */
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ENABLE_DUALCAL, 0x01);

			wx_pole = mcu_init.ois_gyro_list[0];
			wy_pole = mcu_init.ois_gyro_list[1];
			gyro_orientation = mcu_init.ois_gyro_list[2];
#ifdef CAMERA_2ND_OIS
			tx_pole = mcu_init.ois_gyro_list[3];
			ty_pole = mcu_init.ois_gyro_list[4];
#endif
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_POLA_X, wx_pole);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_POLA_Y, wy_pole);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_ORIENT, gyro_orientation);
#ifdef CAMERA_2ND_OIS
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_POLA_X_M2, tx_pole);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_POLA_Y_M2, ty_pole);
#endif
			info("%s gyro init data applied\n", __func__);
		}

		ois_hw_check = true;
	}

	if (module->position == SENSOR_POSITION_REAR2) {
		ois_tele_init = true;
	} else if (module->position == SENSOR_POSITION_REAR) {
		ois_wide_init = true;
	}

	info("%s\n", __func__);
	return ret;
}

int ois_mcu_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct ois_mcu_dev *mcu = NULL;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}


	if (ois_hw_check) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x00);
		usleep_range(2000, 2100);
	}

	ois_fadeupdown = false;
	ois_hw_check = false;

	dbg_ois("%s\n", __func__);

	return ret;
}

int ois_mcu_set_ggfadeupdown(struct v4l2_subdev *subdev, int up, int down)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;
	u8 status = 0;
	int retries = 100;
	u8 data[2];
	u8 write_data[4] = {0,};
#ifdef USE_OIS_SLEEP_MODE
	u8 read_sensorStart = 0;
#endif

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	dbg_ois("%s up:%d down:%d\n", __func__, up, down);

#ifdef CAMERA_2ND_OIS
	if (ois->ois_power_mode < OIS_POWER_MODE_SINGLE) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x03);
	}
#else
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x03);
#endif

	/* Wide af position value */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_AF, 0x00);

	/* Tele af position value */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_AF, 0x00);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CACTRL_WRITE, 0x01);

	/* angle compensation 1.5->1.25
	 * before addr:0x0000, data:0x01
	 * write 0x3F558106
	 * write 0x3F558106
	 */
	write_data[0] = 0x06;
	write_data[1] = 0x81;
	write_data[2] = 0x55;
	write_data[3] = 0x3F;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ANGLE_COMP1, write_data[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ANGLE_COMP2, write_data[1]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ANGLE_COMP3, write_data[2]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ANGLE_COMP4, write_data[3]);

	write_data[0] = 0x06;
	write_data[1] = 0x81;
	write_data[2] = 0x55;
	write_data[3] = 0x3F;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ANGLE_COMP5, write_data[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ANGLE_COMP6, write_data[1]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ANGLE_COMP7, write_data[2]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ANGLE_COMP8, write_data[3]);

#ifdef USE_OIS_SLEEP_MODE
	/* if camera is already started, skip VDIS setting */
	read_sensorStart = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_LOG_Y);
	if (read_sensorStart == 0x03) {
		info("%s camera is already running.\n", __func__);
		return ret;
	}
#endif
	/* set fadeup */
	data[0] = up & 0xFF;
	data[1] = (up >> 8) & 0xFF;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FADE_UP1, data[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FADE_UP2, data[1]);

	/* set fadedown */
	data[0] = down & 0xFF;
	data[1] = (down >> 8) & 0xFF;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FADE_DOWN1, data[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FADE_DOWN2, data[1]);

	/* wait idle status
	 * 100msec delay is needed between "ois_power_on" and "ois_mode_s6".
	 */
	do {
		status = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		if (status == 0x01 || status == 0x13)
			break;
		if (--retries < 0) {
			err("%s : read register fail!. status: 0x%x\n", __func__, status);
			ret = -1;
			break;
		}
		usleep_range(1000, 1100);
	} while (status != 0x01);

	dbg_ois("%s retryCount = %d , status = 0x%x\n", __func__, 100 - retries, status);

	return ret;
}

int ois_mcu_set_mode(struct v4l2_subdev *subdev, int mode)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	if (ois->fadeupdown == false) {
		if (ois_fadeupdown == false) {
			ois_fadeupdown = true;
			ois_mcu_set_ggfadeupdown(subdev, 1000, 1000);
		}
		ois->fadeupdown = true;
	}

	if (mode == ois->pre_ois_mode) {
		return ret;
	}

	ois->pre_ois_mode = mode;
	info("%s: ois_mode value(%d)\n", __func__, mode);

	switch(mode) {
		case OPTICAL_STABILIZATION_MODE_STILL:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x00);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_VIDEO:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x01);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_CENTERING:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x05);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_STILL_ZOOM:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x13);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_VDIS:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x14);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_SINE_X:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_1, 0x01);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_2, 0x01);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_3, 0x2D);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x03);
			msleep(20);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_SINE_Y:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_1, 0x02);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_2, 0x01);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_3, 0x2D);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x03);
			msleep(20);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		default:
			dbg_ois("%s: ois_mode value(%d)\n", __func__, mode);
			break;
	}

	return ret;
}

int ois_mcu_shift_compensation(struct v4l2_subdev *subdev, int position, int resolution)
{
	int ret = 0;
	struct is_ois *ois;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	int position_changed;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_peri = is_mcu->sensor_peri;
	if (!sensor_peri) {
		err("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	position_changed = position / 2;

	if (module->position == SENSOR_POSITION_REAR && ois->af_pos_wide != position_changed) {
		/* Wide af position value */
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_AF, (u8)position_changed);
		ois->af_pos_wide = position_changed;
	} else if (module->position == SENSOR_POSITION_REAR2 && ois->af_pos_tele != position_changed) {
		/* Tele af position value */
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_AF, (u8)position_changed);
		ois->af_pos_tele = position_changed;
	}

p_err:
	return ret;
}

int ois_mcu_self_test(struct is_core *core)
{
	u8 val = 0;
	u8 reg_val = 0, x = 0, y = 0;
	u16 x_gyro_log = 0, y_gyro_log = 0;
	int retries = 30;
	struct ois_mcu_dev *mcu = NULL;

	info("%s : E\n", __func__);

	mcu = core->mcu;

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL, 0x08);

	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL);
		 msleep(20);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_RESULT);

	/* Gyro selfTest result */
	reg_val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_VAL_X);
	x = reg_val;
	reg_val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_LOG_X);
	x_gyro_log = (reg_val << 8) | x;

	reg_val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_VAL_Y);
	y = reg_val;
	reg_val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_LOG_Y);
	y_gyro_log = (reg_val << 8) | y;

	info("%s(GSTLOG0=%d, GSTLOG1=%d)\n", __func__, x_gyro_log, y_gyro_log);

	info("%s(%d) : X\n", __func__, val);
	return (int)val;
}

bool ois_mcu_sine_wavecheck(struct is_core *core,
	int threshold, int *sinx, int *siny, int *result)
{
	u8 buf = 0, val = 0;
	int retries = 10;
	int sinx_count = 0, siny_count = 0;
	u8 u8_sinx_count[2] = {0, }, u8_siny_count[2] = {0, };
	u8 u8_sinx[2] = {0, }, u8_siny[2] = {0, };
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info("%s autotest started", __func__);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_THRESH_ERR_LEV, (u8)threshold); /* error threshold level. */

	return true;
	
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x01); /* OIS SEL (wide : 1 , tele : 2, both : 3 ). */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ERR_VAL_CNT, 0x00); /* count value for error judgement level. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FREQ_LEV, 0x05); /* frequency level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_AMPLI_LEV, 0x34); /* amplitude level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_DUM_PULSE, 0x03); /* dummy pulse setting. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VYVLE_LEV, 0x02); /* vyvle level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START_WAVE_CHECK, 0x01); /* start sine wave check operation */

	retries = 10;
	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START_WAVE_CHECK);
		msleep(100);
		if (--retries < 0) {
			err("sine wave operation fail.\n");
			break;
		}
	} while (val);

	buf = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_AUTO_TEST_RESULT);

	*result = (int)buf;


	u8_sinx_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_COUNT1);
	u8_sinx_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_COUNT2);
	sinx_count = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count > 0x7FFF) {
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);
	}
	u8_siny_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_COUNT1);
	u8_siny_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_COUNT2);
	siny_count = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count > 0x7FFF) {
		siny_count = -((siny_count ^ 0xFFFF) + 1);
	}
	u8_sinx[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_DIFF1);
	u8_sinx[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_DIFF2);
	*sinx = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx > 0x7FFF) {
		*sinx = -((*sinx ^ 0xFFFF) + 1);
	}
	u8_siny[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_DIFF1);
	u8_siny[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_DIFF2);
	*siny = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny > 0x7FFF) {
		*siny = -((*siny ^ 0xFFFF) + 1);
	}

	info("threshold = %d, sinx = %d, siny = %d, sinx_count = %d, syny_count = %d\n",
		threshold, *sinx, *siny, sinx_count, siny_count);

	if (buf == 0x0) {
		return true;
	} else {
		return false;
	}
}

bool ois_mcu_auto_test(struct is_core *core,
					int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y)
{
	int result = 0;
	bool value = false;
	struct ois_mcu_dev *mcu = NULL;

#ifdef CONFIG_AF_HOST_CONTROL
	is_af_move_lens(core);
	msleep(100);
#endif

	info("%s autotest started", __func__);

	mcu = core->mcu;

	value = ois_mcu_sine_wavecheck(core, threshold, sin_x, sin_y, &result);
	if (*sin_x == -1 && *sin_y == -1) {
		err("OIS device is not prepared.");
		*x_result = false;
		*y_result = false;

		return false;
	}

	if (value == true) {
		*x_result = true;
		*y_result = true;

		return true;
	} else {
		dbg_ois("OIS autotest is failed value = 0x%x\n", result);
		if ((result & 0x03) == 0x01) {
			*x_result = false;
			*y_result = true;
		} else if ((result & 0x03) == 0x02) {
			*x_result = true;
			*y_result = false;
		} else {
			*x_result = false;
			*y_result = false;
		}

		return false;
	}
}

#ifdef CAMERA_2ND_OIS
bool ois_mcu_sine_wavecheck_rear2(struct is_core *core,
					int threshold, int *sinx, int *siny, int *result,
					int *sinx_2nd, int *siny_2nd)
{
	u8 buf = 0, val = 0;
	int retries = 10;
	int sinx_count = 0, siny_count = 0;
	int sinx_count_2nd = 0, siny_count_2nd = 0;
	u8 u8_sinx_count[2] = {0, }, u8_siny_count[2] = {0, };
	u8 u8_sinx[2] = {0, }, u8_siny[2] = {0, };
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;
	
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x03); /* OIS SEL (wide : 1 , tele : 2, both : 3 ). */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_THRESH_ERR_LEV, (u8)threshold); /* error threshold level. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_THRESH_ERR_LEV_M2, (u8)threshold); /* error threshold level. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ERR_VAL_CNT, 0x00); /* count value for error judgement level. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FREQ_LEV, 0x05); /* frequency level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_AMPLI_LEV, 0x2A); /* amplitude level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_DUM_PULSE, 0x03); /* dummy pulse setting. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VYVLE_LEV, 0x02); /* vyvle level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START_WAVE_CHECK, 0x01); /* start sine wave check operation */

	retries = 10;
	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START_WAVE_CHECK);
		msleep(100);
		if (--retries < 0) {
			err("sine wave operation fail.\n");
			break;
		}
	} while (val);

	buf = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_AUTO_TEST_RESULT);

	*result = (int)buf;

	u8_sinx_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_COUNT1);
	u8_sinx_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_COUNT2);
	sinx_count = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count > 0x7FFF) {
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);
	}
	u8_siny_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_COUNT1);
	u8_siny_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_COUNT2);
	siny_count = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count > 0x7FFF) {
		siny_count = -((siny_count ^ 0xFFFF) + 1);
	}
	u8_sinx[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_DIFF1);
	u8_sinx[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_DIFF2);
	*sinx = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx > 0x7FFF) {
		*sinx = -((*sinx ^ 0xFFFF) + 1);
	}
	u8_siny[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_DIFF1);
	u8_siny[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_DIFF2);
	*siny = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny > 0x7FFF) {
		*siny = -((*siny ^ 0xFFFF) + 1);
	}

	u8_sinx_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_COUNT1);
	u8_sinx_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_COUNT2);
	sinx_count_2nd = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count_2nd > 0x7FFF) {
		sinx_count_2nd = -((sinx_count_2nd ^ 0xFFFF) + 1);
	}
	u8_siny_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_COUNT1);
	u8_siny_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_COUNT2);
	siny_count_2nd = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count_2nd > 0x7FFF) {
		siny_count_2nd = -((siny_count_2nd ^ 0xFFFF) + 1);
	}
	u8_sinx[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_DIFF1);
	u8_sinx[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_DIFF2);
	*sinx_2nd = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx_2nd > 0x7FFF) {
		*sinx_2nd = -((*sinx_2nd ^ 0xFFFF) + 1);
	}
	u8_siny[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_DIFF1);
	u8_siny[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_DIFF2);
	*siny_2nd = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny_2nd > 0x7FFF) {
		*siny_2nd = -((*siny_2nd ^ 0xFFFF) + 1);
	}

	info("threshold = %d, sinx = %d, siny = %d, sinx_count = %d, syny_count = %d\n",
		threshold, *sinx, *siny, sinx_count, siny_count);

	info("threshold = %d, sinx_2nd = %d, siny_2nd = %d, sinx_count_2nd = %d, syny_count_2nd = %d\n",
		threshold, *sinx_2nd, *siny_2nd, sinx_count_2nd, siny_count_2nd);

	if (buf == 0x0) {
		return true;
	} else {
		return false;
	}
}

bool ois_mcu_auto_test_rear2(struct is_core *core,
					int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
					bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd)
{
	int result = 0;
	bool value = false;

#ifdef CONFIG_AF_HOST_CONTROL
#ifdef CAMERA_REAR2_AF
	is_af_move_lens_rear2(core);
	msleep(100);
#endif
	is_af_move_lens(core);
	msleep(100);
#endif

	value = ois_mcu_sine_wavecheck_rear2(core, threshold, sin_x, sin_y, &result,
				sin_x_2nd, sin_y_2nd);

	if (*sin_x == -1 && *sin_y == -1) {
		err("OIS device is not prepared.");
		*x_result = false;
		*y_result = false;

		return false;
	}

	if (*sin_x_2nd == -1 && *sin_y_2nd == -1) {
		err("OIS 2 device is not prepared.");
		*x_result_2nd = false;
		*y_result_2nd = false;

		return false;
	}

	if (value == true) {
		*x_result = true;
		*y_result = true;
		*x_result_2nd = true;
		*y_result_2nd = true;

		return true;
	} else {
		err("OIS autotest_2nd is failed result (0x0051) = 0x%x\n", result);
		if ((result & 0x03) == 0x00) {
			*x_result = true;
			*y_result = true;
		} else if ((result & 0x03) == 0x01) {
			*x_result = false;
			*y_result = true;
		} else if ((result & 0x03) == 0x02) {
			*x_result = true;
			*y_result = false;
		} else {
			*x_result = false;
			*y_result = false;
		}

		if ((result & 0x30) == 0x00) {
			*x_result_2nd = true;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x10) {
			*x_result_2nd = false;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x20) {
			*x_result_2nd = true;
			*y_result_2nd = false;
		} else {
			*x_result_2nd = false;
			*y_result_2nd = false;
		}

		return false;
	}
}

int ois_mcu_set_power_mode(struct v4l2_subdev *subdev)
{
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;
	struct is_core *core;
	struct is_dual_info *dual_info = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	mcu = (struct ois_mcu_dev*)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		return -EINVAL;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		return -EINVAL;
	}

	sensor_peri = is_mcu->sensor_peri;
	if (!sensor_peri) {
		err("%s, sensor_peri is NULL", __func__);
		return -EINVAL;
	}

	module = sensor_peri->module;
	if (!module) {
		err("%s, module is NULL", __func__);
		return -EINVAL;
	}

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core is null");
		return -EINVAL;
	}

	ois = is_mcu->ois;
	if(!ois) {
		err("%s, ois subdev is NULL", __func__);
		return -EINVAL;
	}

	dual_info = &core->dual_info;

	/* OIS SEL (wide : 1 , tele : 2, both : 3 ). */
	if ((dual_info->mode != IS_DUAL_MODE_NOTHING)
		|| (dual_info->mode == IS_DUAL_MODE_NOTHING &&
			module->position == SENSOR_POSITION_REAR2)) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x03);
		ois->ois_power_mode = OIS_POWER_MODE_DUAL;
	} else {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x01);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE;
	}

	info("%s ois power setting is %d\n", __func__, ois->ois_power_mode);

	return 0;
}
#endif

void ois_mcu_enable(struct is_core *core)
{
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info("%s : E\n", __func__);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x00);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);

	info("%s : X\n", __func__);
}

bool ois_mcu_offset_test(struct is_core *core, long *raw_data_x, long *raw_data_y)
{
	int i = 0;
	u8 val = 0, x = 0, y = 0;
	int x_sum = 0, y_sum = 0, sum = 0;
	int retries = 0, avg_count = 30;
	bool result = false;
	int scale_factor = OIS_GYRO_SCALE_FACTOR_LSM6DSO;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info("%s : E\n", __func__);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL, 0x01);

	retries = avg_count;
	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL);
		 msleep(20);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	/* Gyro result check */
	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_RESULT);

	if ((val & 0x80) == 0x0) {
		info("[%s] Gyro result check success. Result is OK.", __func__);
		result = true;
	} else {
		info("[%s] Gyro result check fail. Result is NG.", __func__);
		result = false;
	}
 
	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X1);
		x = val;
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X2);
		x_sum = (val << 8) | x;
		if (x_sum > 0x7FFF) {
			x_sum = -((x_sum ^ 0xFFFF) + 1);
		}
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / scale_factor / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y1);
		y = val;
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y2);
		y_sum = (val << 8) | y;
		if (y_sum > 0x7FFF) {
			y_sum = -((y_sum ^ 0xFFFF) + 1);
		}
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / scale_factor / 10;

	//is_mcu_fw_version(core);
	info("%s : X raw_x = %ld, raw_y = %ld\n", __func__, *raw_data_x, *raw_data_y);

	return result;
}

void ois_mcu_get_offset_data(struct is_core *core, long *raw_data_x, long *raw_data_y)
{
	int i = 0;
	u8 val = 0, x = 0, y = 0;
	int x_sum = 0, y_sum = 0, sum = 0;
	int retries = 0, avg_count = 40;
	int scale_factor = OIS_GYRO_SCALE_FACTOR_LSM6DSO;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info("%s : E\n", __func__);

	/* check ois status */
	retries = avg_count;
	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		msleep(50);//msleep(20);
		if (--retries < 0) {
			err("Read status failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val != 0x01);

	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X1);
		x = val;
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X2);
		x_sum = (val << 8) | x;
		if (x_sum > 0x7FFF) {
			x_sum = -((x_sum ^ 0xFFFF) + 1);
		}
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / scale_factor / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y1);
		y = val;
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y2);
		y_sum = (val << 8) | y;
		if (y_sum > 0x7FFF) {
			y_sum = -((y_sum ^ 0xFFFF) + 1);
		}
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / scale_factor / 10;

	//is_mcu_fw_version(core);
	info("%s : X raw_x = %ld, raw_y = %ld\n", __func__, *raw_data_x, *raw_data_y);
	return;
}

void ois_mcu_gyro_sleep(struct is_core *core)
{
	u8 val = 0;
	int retries = 20;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x00);

	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);

		if (val == 0x01 || val == 0x13)
			break;

		msleep(1);
	} while (--retries > 0);

	if (retries <= 0) {
		err("Read register failed!!!!, data = 0x%04x\n", val);
	}

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_SLEEP, 0x03);
	msleep(1);

	return;
}

void ois_mcu_exif_data(struct is_core *core)
{
	u8 error_reg[2], status_reg;
	u16 error_sum;
	struct ois_mcu_dev *mcu = NULL;
	 struct is_ois_exif *ois_exif_data = NULL;

	mcu = core->mcu;

	is_ois_get_exif_data(&ois_exif_data);

	error_reg[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_RESULT);
	error_reg[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CHECKSUM);

	error_sum = (error_reg[1] << 8) | error_reg[0];

	status_reg = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);

	ois_exif_data->error_data = error_sum;
	ois_exif_data->status_data = status_reg;

	return;
}

u8 ois_mcu_read_status(struct is_core *core)
{
	u8 status = 0;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	status = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_READ_STATUS);

	return status;
}

u8 ois_mcu_read_cal_checksum(struct is_core *core)
{
	u8 status = 0;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	status = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CHECKSUM);

	return status;
}

int ois_mcu_set_coef(struct v4l2_subdev *subdev, u8 coef)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	if (ois->pre_coef == coef)
		return ret;

	dbg_ois("%s %d\n", __func__, coef);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SET_COEF, coef);

	ois->pre_coef = coef;

	return ret;
}

int ois_mcu_shift(struct v4l2_subdev *subdev)
{
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;
	u8 data[2];
	int ret = 0;

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	data[0] = (ois_center_x & 0xFF);
	data[1] = (ois_center_x & 0xFF00) >> 8;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CENTER_X1, data[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CENTER_X2, data[1]);

	data[0] = (ois_center_y & 0xFF);
	data[1] = (ois_center_y & 0xFF00) >> 8;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CENTER_Y1, data[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CENTER_Y2, data[1]);

	return ret;
}

int ois_mcu_set_centering(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x05);

	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_CENTERING;

	return ret;
}

u8 ois_mcu_read_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 mode = OPTICAL_STABILIZATION_MODE_OFF;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	mode = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE);

	switch(mode) {
		case 0x00:
			mode = OPTICAL_STABILIZATION_MODE_STILL;
			break;
		case 0x01:
			mode = OPTICAL_STABILIZATION_MODE_VIDEO;
			break;
		case 0x05:
			mode = OPTICAL_STABILIZATION_MODE_CENTERING;
			break;
		case 0x13:
			mode = OPTICAL_STABILIZATION_MODE_STILL_ZOOM;
			break;
		case 0x14:
			mode = OPTICAL_STABILIZATION_MODE_VDIS;
			break;
		default:
			dbg_ois("%s: ois_mode value(%d)\n", __func__, mode);
			break;
	}

	return mode;
}

bool ois_mcu_gyro_cal(struct is_core *core, long *x_value, long *y_value)
{
	u8 val = 0, x = 0, y = 0;
	int retries = 30;
	int scale_factor = OIS_GYRO_SCALE_FACTOR_LSM6DSO;
	int x_sum = 0, y_sum = 0;
	bool result = false;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info("%s : E\n", __func__);

	/* check ois status */
	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		 msleep(20);
		if (--retries < 0) {
			err("Read status failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val != 0x01);

	retries = 30;

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL, 0x01);

	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL);
		msleep(15);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	/* Gyro result check */
	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_RESULT);

	if ((val & 0x23) == 0x0) {
		/* Execute OIS DATA AREA WRITE start */
		info("[%s] Gyro result check success. Start Data write.", __func__);
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_WRITE_DATA, 0x01);
		msleep(170);
		retries = 20;
		do {
			val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_WRITE_DATA);
			msleep(10);
			if (--retries < 0) {
				err("Read register failed!!!!, data = 0x%04x\n", val);
				break;
			}
		} while (val);

		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_READ_WRITE_RESULT);

		if (val == 0xAA) {
			info("[%s] Written cal is OK. val = 0x%02x.", __func__, val);
			result = true;
		} else {
			info("[%s] Written cal is NG. val = 0x%02x.", __func__, val);
		}
	}

	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X1);
	x = val;
	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X2);
	x_sum = (val << 8) | x;
	if (x_sum > 0x7FFF) {
		x_sum = -((x_sum ^ 0xFFFF) + 1);
	}

	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y1);
	y = val;
	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y2);
	y_sum = (val << 8) | y;
	if (y_sum > 0x7FFF) {
		y_sum = -((y_sum ^ 0xFFFF) + 1);
	}

	*x_value = x_sum * 1000 / scale_factor;
	*y_value = y_sum * 1000 / scale_factor;

	info("%s X (x = %ld/y = %ld) : result = %d\n", __func__, *x_value, *y_value, result);

	return result;
}

static struct is_ois_ops ois_ops_mcu = {
	.ois_init = ois_mcu_init,
	.ois_deinit = ois_mcu_deinit,
	.ois_set_mode = ois_mcu_set_mode,
	.ois_shift_compensation = ois_mcu_shift_compensation,
	.ois_self_test = ois_mcu_self_test,
	.ois_auto_test = ois_mcu_auto_test,
#ifdef CAMERA_2ND_OIS
	.ois_auto_test_rear2 = ois_mcu_auto_test_rear2,
	.ois_set_power_mode = ois_mcu_set_power_mode,
#endif
	//.ois_check_fw = is_ois_check_fw_mcu, //TEMP_2020
	.ois_enable = ois_mcu_enable,
	.ois_offset_test = ois_mcu_offset_test,
	.ois_get_offset_data = ois_mcu_get_offset_data,
	.ois_gyro_sleep = ois_mcu_gyro_sleep,
	.ois_exif_data = ois_mcu_exif_data,
	.ois_read_status = ois_mcu_read_status,
	.ois_read_cal_checksum = ois_mcu_read_cal_checksum,
	.ois_set_coef = ois_mcu_set_coef,
	//.ois_read_fw_ver = is_mcu_read_fw_ver, //TEMP_2020
	.ois_center_shift = ois_mcu_shift,
	.ois_set_center = ois_mcu_set_centering,
	.ois_read_mode = ois_mcu_read_mode,
	.ois_calibration_test = ois_mcu_gyro_cal,
};

static int __init ois_mcu_probe(struct platform_device *pdev)
{
	struct is_core *core;
	struct ois_mcu_dev *mcu;
	struct resource *res;
	int ret;
	struct device_node *dnode;
	struct is_mcu *is_mcu = NULL;
	struct is_device_sensor *device;
	struct v4l2_subdev *subdev_mcu = NULL;
	struct v4l2_subdev *subdev_ois = NULL;
	struct is_device_ois *ois_device = NULL;
	struct is_ois *ois = NULL;
	const u32 *ois_gyro_spec;
	const u32 *sensor_id_spec;
	u32 sensor_id_len;
	u32 sensor_id[IS_SENSOR_COUNT] = {0, };	
	int i;

	core = (struct is_core *)dev_get_drvdata(is_dev);

	dnode = pdev->dev.of_node;

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec) {
		err("sensor_id num read is fail(%d)", ret);
		goto p_err;
	}

	sensor_id_len /= (unsigned int)sizeof(*sensor_id_spec);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	ois_gyro_spec = of_get_property(dnode, "ois_gyro_list", &mcu_init.ois_gyro_list_len);
	if (ois_gyro_spec) {
		mcu_init.ois_gyro_list_len /= (unsigned int)sizeof(*ois_gyro_spec);
		ret = of_property_read_u32_array(dnode, "ois_gyro_list",
		        mcu_init.ois_gyro_list, mcu_init.ois_gyro_list_len);
		if (ret)
		        info("ois_gyro_list read is fail(%d)", ret);
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		if (!device) {
			err("sensor device is NULL");
			ret = -EPROBE_DEFER;
			goto p_err;
		}
	}

	mcu = devm_kzalloc(&pdev->dev, sizeof(struct ois_mcu_dev), GFP_KERNEL);
	if (!mcu)
		return -ENOMEM;

	is_mcu = devm_kzalloc(&pdev->dev, sizeof(struct is_mcu) * sensor_id_len, GFP_KERNEL);
	if (!mcu) {
		err("fimc_is_mcu is NULL");
		ret -ENOMEM;
		goto p_err;
	}

	subdev_mcu = devm_kzalloc(&pdev->dev, sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_mcu) {
		err("subdev_mcu is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	ois = devm_kzalloc(&pdev->dev, sizeof(struct is_ois) * sensor_id_len, GFP_KERNEL);
	if (!ois) {
		err("fimc_is_ois is NULL");
		ret -ENOMEM;
		goto p_err;
	}

	subdev_ois = devm_kzalloc(&pdev->dev, sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_ois) {
		err("subdev_ois is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	ois_device = devm_kzalloc(&pdev->dev, sizeof(struct is_device_ois), GFP_KERNEL);
	if (!ois_device) {
		err("fimc_is_device_ois is NULL");
		ret -ENOMEM;
		goto p_err;
	}

	mcu->dev = &pdev->dev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_CORE] = devm_ioremap_nocache(mcu->dev,
				res->start, resource_size(res));
	if (!mcu->regs[OM_REG_CORE]) {
		dev_err(&pdev->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_CORE] = res->start;
	mcu->regs_end[OM_REG_CORE] = res->end;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_PERI1] = devm_ioremap_nocache(mcu->dev,
				res->start, resource_size(res));
	if (!mcu->regs[OM_REG_PERI1]) {
		dev_err(&pdev->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_PERI1] = res->start;
	mcu->regs_end[OM_REG_PERI1] = res->end;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_PERI2] = devm_ioremap_nocache(mcu->dev,
				res->start, resource_size(res));
	if (!mcu->regs[OM_REG_PERI2]) {
		dev_err(mcu->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_PERI2] = res->start;
	mcu->regs_end[OM_REG_PERI2] = res->end;

	mcu->irq = platform_get_irq(pdev, 0);
	if (mcu->irq < 0) {
		dev_err(mcu->dev, "[@] failed to get IRQ resource: %d\n",
							mcu->irq);
		ret = mcu->irq;
		goto err_get_irq;
	}
	ret = devm_request_irq(mcu->dev, mcu->irq, is_isr_ois_mcu,
			0,
			dev_name(mcu->dev), mcu);
	if (ret) {
		dev_err(mcu->dev, "[@] failed to request IRQ(%d): %d\n",
							mcu->irq, ret);
		goto err_req_irq;
	}

	platform_set_drvdata(pdev, mcu);
	core->mcu = mcu;

	ois_device->ois_ops = &ois_ops_mcu;

	for (i = 0; i < sensor_id_len; i++) {
		probe_info("%s sensor_id %d\n", __func__, sensor_id[i]);

		is_mcu[i].name = MCU_NAME_INTERNAL;
		is_mcu[i].subdev = &subdev_mcu[i];
		is_mcu[i].device = sensor_id[i];
		is_mcu[i].private_data = core;

		ois[i].subdev = &subdev_ois[i];
		ois[i].device = sensor_id[i];
		ois[i].ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
		ois[i].pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
		ois[i].ois_shift_available = false;
		ois[i].i2c_lock = NULL;
		ois[i].ois_ops = &ois_ops_mcu;

		is_mcu[i].subdev_ois = &subdev_ois[i];
		is_mcu[i].ois = &ois[i];
		is_mcu[i].ois_device = ois_device;

		device = &core->sensor[sensor_id[i]];
		device->subdev_mcu = &subdev_mcu[i];
		device->mcu = &is_mcu[i];

		v4l2_set_subdevdata(&subdev_mcu[i], mcu);
		v4l2_set_subdev_hostdata(&subdev_mcu[i], &is_mcu[i]);

		probe_info("%s done\n", __func__);
	}

#if defined(CONFIG_PM)
	pm_runtime_enable(mcu->dev);
	set_bit(OM_HW_SUSPENDED, &mcu->state);
#endif
	set_bit(OM_HW_NONE, &mcu->state);

	probe_info("[@] %s device probe success\n", dev_name(mcu->dev));

	return 0;

err_req_irq:
err_get_irq:
	devm_iounmap(mcu->dev, mcu->regs[OM_REG_CORE]);
	devm_iounmap(mcu->dev, mcu->regs[OM_REG_PERI1]);
	devm_iounmap(mcu->dev, mcu->regs[OM_REG_PERI2]);
err_ioremap:
	devm_release_mem_region(mcu->dev, res->start, resource_size(res));
p_err:
	if (mcu)
		kfree(mcu);

	if (is_mcu)
		kfree(is_mcu);

	if (subdev_mcu)
		kfree(subdev_mcu);

	if (ois)
		kfree(ois);

	if (subdev_ois)
		kfree(subdev_ois);

	if (ois_device)
		kfree(ois_device);

	return ret;
}

static const struct dev_pm_ops ois_mcu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ois_mcu_suspend, ois_mcu_resume)
	SET_RUNTIME_PM_OPS(ois_mcu_runtime_suspend, ois_mcu_runtime_resume,
			   NULL)
};

static const struct of_device_id sensor_ois_mcu_match[] = {
	{
		.compatible = "samsung,sensor-ois-mcu",
	},
	{},
};

static struct platform_driver sensor_ois_mcu_platform_driver = {
	.driver = {
		.name   = "Sensor-OIS-MCU",
		.owner  = THIS_MODULE,
		.pm	= &ois_mcu_pm_ops,
		.of_match_table = sensor_ois_mcu_match,
	}
};

static int __init sensor_ois_mcu_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_ois_mcu_platform_driver,
							ois_mcu_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_ois_mcu_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_ois_mcu_init);

MODULE_DESCRIPTION("Exynos Pablo OIS-MCU driver");
MODULE_AUTHOR("Younghwan Joo <yhwan.joo@samsung.com>");
MODULE_LICENSE("GPL v2");
