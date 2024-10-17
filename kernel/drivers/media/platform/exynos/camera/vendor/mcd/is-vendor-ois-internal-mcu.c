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
#include <linux/file.h>
#include <soc/samsung/exynos-pmu-if.h>

#include <exynos-is-sensor.h>
#include "is-device-sensor-peri.h"
#include "pablo-hw-api-common.h"
#include "is-hw-api-ois-mcu.h"
#include "is-vendor-ois-internal-mcu.h"
#include "is-vendor-ois.h"
#include "is-device-ois.h"
#include "is-sfr-ois-mcu-v1_1_1.h"
#ifdef CONFIG_AF_HOST_CONTROL
#include "is-device-af.h"
#endif
#include "is-vendor-private.h"
#include "is-sec-define.h"

static const struct v4l2_subdev_ops subdev_ops;
#if !defined(OIS_DUAL_CAL_DEFAULT_VALUE_TELE) && defined(CAMERA_2ND_OIS)
static struct mcu_efs_info efs_info;
#endif
u64 timestampboot;

static int is_vendor_ois_clk_get(struct ois_mcu_dev *mcu)
{
	mcu->clk = clk_get(mcu->dev, "user_mux");
	mcu->spi_clk = clk_get(mcu->dev, "ipclk_spi");
	if (!IS_ERR(mcu->clk) && !IS_ERR(mcu->spi_clk))
		return 0;
	else
		goto err;

err:
	if (PTR_ERR(mcu->clk) != -ENOENT) {
		dev_err(mcu->dev, "Failed to get 'user_mux' clock: %ld",
			PTR_ERR(mcu->clk));
		return PTR_ERR(mcu->clk);
	}
	dev_info(mcu->dev, "[@] 'user_mux' clock is not present\n");

	if (PTR_ERR(mcu->spi_clk) != -ENOENT) {
		dev_err(mcu->dev, "Failed to get 'spiclk' clock: %ld",
			PTR_ERR(mcu->spi_clk));
		return PTR_ERR(mcu->spi_clk);
	}
	dev_info(mcu->dev, "[@] 'spiclk' clock is not present\n");

	return -EIO;
}

static void is_vendor_ois_clk_put(struct ois_mcu_dev *mcu)
{
	if (!IS_ERR(mcu->clk))
		clk_put(mcu->clk);

	if (!IS_ERR(mcu->spi_clk))
		clk_put(mcu->spi_clk);
}

static int is_vendor_ois_clk_enable(struct ois_mcu_dev *mcu)
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

	if (IS_ERR(mcu->spi_clk)) {
		dev_info(mcu->dev, "[@] 'spi_clk' clock is not present\n");
		return -EIO;
	}

	/* set spi clock to 10Mhz */
	clk_set_rate(mcu->spi_clk, 19200000);
	ret = clk_prepare_enable(mcu->spi_clk);
	if (ret) {
		dev_err(mcu->dev, "%s: failed to enable clk (err %d)\n",
					__func__, ret);
		return ret;
	}

	return ret;
}

static void is_vendor_ois_clk_disable(struct ois_mcu_dev *mcu)
{
	if (!IS_ERR(mcu->clk))
		clk_disable_unprepare(mcu->clk);

	if (!IS_ERR(mcu->spi_clk))
		clk_disable_unprepare(mcu->spi_clk);
}

static int is_vendor_ois_runtime_resume(struct device *dev)
{
	struct ois_mcu_dev *mcu = dev_get_drvdata(dev);
	int ret = 0;

	info_mcu("%s E\n", __func__);

	ret = is_vendor_ois_clk_get(mcu);
	if (ret)
		return ret;

	ret = is_vendor_ois_clk_enable(mcu);

	__is_mcu_pmu_control(1);
	usleep_range(1000, 1100);

	__is_mcu_hw_enable(mcu->regs[OM_REG_SFR]);
	ret |= __is_mcu_hw_reset_peri(mcu->regs[OM_REG_PERI1], 0); /* Clear USI reset reg */
	usleep_range(2000, 2100);
	ret |= __is_mcu_hw_reset_peri(mcu->regs[OM_REG_PERI2], 0); /* Clear USI reset reg */
	usleep_range(2000, 2100);
	ret |= __is_mcu_hw_set_init_peri(mcu->regs[OM_REG_PERI_SETTING]); /* Gpio setting */
	ret |= __is_mcu_hw_set_clock_peri(mcu->regs[OM_REG_PERI1]); /* Set i2c clock to 1MH */

	clear_bit(OM_HW_SUSPENDED, &mcu->state);

	info_mcu("%s X\n", __func__);

	return ret;
}

static int is_vendor_ois_runtime_suspend(struct device *dev)
{
	struct ois_mcu_dev *mcu = dev_get_drvdata(dev);
	int ret = 0;

	info_mcu("%s E\n", __func__);

	ret = __is_mcu_hw_disable(mcu->regs[OM_REG_SFR]);
	ret |= __is_mcu_hw_set_clear_peri(mcu->regs[OM_REG_PERI_SETTING]); /* Gpio setting */
	usleep_range(2000, 2100); //TEMP_2020 Need to be checked
	ret |= __is_mcu_hw_reset_peri(mcu->regs[OM_REG_PERI1], 1); /* Clear USI reset reg */
	ret |= __is_mcu_hw_reset_peri(mcu->regs[OM_REG_PERI2], 1); /* Clear USI reset reg */
	ret |= __is_mcu_hw_clear_peri(mcu->regs[OM_REG_PERI1]);
	ret |= __is_mcu_hw_clear_peri(mcu->regs[OM_REG_PERI2]);

	is_vendor_ois_clk_disable(mcu);
	is_vendor_ois_clk_put(mcu);

	__is_mcu_pmu_control(0);
	/* wait for ois cpu power down, before csis block off (pm_runtime_put_sync). max 1ms. */
	usleep_range(1000, 1100);

	set_bit(OM_HW_SUSPENDED, &mcu->state);

	info_mcu("%s X\n", __func__);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int is_vendor_ois_resume(struct device *dev)
{
	/* TODO: */
	return 0;
}

static int is_vendor_ois_suspend(struct device *dev)
{
	struct ois_mcu_dev *mcu = dev_get_drvdata(dev);

	/* TODO: */
	if (!test_bit(OM_HW_SUSPENDED, &mcu->state))
		return -EBUSY;

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static irqreturn_t is_isr_is_vendor_ois(int irq, void *data)
{
	struct ois_mcu_dev *mcu;
	unsigned int state;

	mcu = (struct ois_mcu_dev *)data;
	state = is_mcu_hw_g_irq_state(mcu->regs[OM_REG_SFR], true);

	/* FIXME: temp log for testing */
	//info_mcu("IRQ: %d\n", state);
	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_WDT)) {
		/* TODO: WDR IRQ handling */
		dbg_ois("IRQ: MCU_IRQ_WDT");
	}

	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_WDT_RST)) {
		/* TODO: WDR RST handling */
		dbg_ois("IRQ: MCU_IRQ_WDT_RST");
	}

	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_LOCKUP_RST)) {
		/* TODO: LOCKUP RST handling */
		dbg_ois("IRQ: MCU_IRQ_LOCKUP_RST");
	}

	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_SYS_RST)) {
		/* TODO: SYS RST handling */
		dbg_ois("IRQ: MCU_IRQ_SYS_RST");
	}

	return IRQ_HANDLED;
}

/*
 * API functions
 */
int is_vendor_ois_power_ctrl(struct ois_mcu_dev *mcu, int on)
{
	int ret = 0;
#if defined(CONFIG_PM)
	int rpm_ret;
#endif
	BUG_ON(!mcu);

	info_mcu("%s E\n", __func__);

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
		ret = is_vendor_ois_runtime_resume(mcu->dev);
#endif
		clear_bit(OM_HW_SUSPENDED, &mcu->state);
		mcu->current_error_reg = 0;
		mcu->current_power_mode = OIS_POWER_MODE_NONE;
	} else {
		if (test_bit(OM_HW_SUSPENDED, &mcu->state)) {
			warning_mcu("already power off\n");
			goto p_err;
		}
#if defined(CONFIG_PM)
		info_mcu("%s: pm_runtime_put_sync start.\n", __func__);
		rpm_ret = pm_runtime_put_sync(mcu->dev);
		if (rpm_ret < 0)
			err_mcu("pm_runtime_put_sync() err: %d", rpm_ret);
		else
			info_mcu("%s: pm_runtime_put_sync end.\n", __func__);
#else
		ret = is_vendor_ois_runtime_suspend(mcu->dev);
#endif
		set_bit(OM_HW_SUSPENDED, &mcu->state);
		clear_bit(OM_HW_FW_LOADED, &mcu->state);
		clear_bit(OM_HW_RUN, &mcu->state);
		mcu->dev_ctrl_state = false;
	}

	info_mcu("%s: (%d) X\n", __func__, on);

p_err:
	return ret;
}

int is_vendor_ois_load_binary(struct ois_mcu_dev *mcu)
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

int is_vendor_ois_core_ctrl(struct ois_mcu_dev *mcu, int on)
{
	int ret = 0;

	BUG_ON(!mcu);

	info_mcu("%s E\n", __func__);

	if (on) {
		if (test_bit(OM_HW_RUN, &mcu->state)) {
			warning_mcu("already started\n");
			return ret;
		}
		__is_mcu_hw_s_irq_enable(mcu->regs[OM_REG_SFR], 0x0);
		set_bit(OM_HW_RUN, &mcu->state);
	} else {
		if (!test_bit(OM_HW_RUN, &mcu->state)) {
			warning_mcu("already stopped\n");
			return ret;
		}
		clear_bit(OM_HW_RUN, &mcu->state);
	}

	ret = __is_mcu_core_control(mcu->regs[OM_REG_SFR], on);

	info_mcu("%s: %d X\n", __func__, on);

	return ret;
}

int is_vendor_ois_dump(struct ois_mcu_dev *mcu, int type)
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
		__is_mcu_hw_peri1_dump(mcu->regs[OM_REG_PERI1]);
		break;
	case OM_REG_PERI2:
		__is_mcu_hw_peri2_dump(mcu->regs[OM_REG_PERI2]);
		break;
	default:
		err_mcu("undefined type (%d)\n", type);
	}

	return ret;
}

void is_vendor_ois_reset_mcu(struct ois_mcu_dev *mcu)
{
#ifdef USE_TELE2_OIS_AF_COMMON_INTERFACE
	/* write AF CTRL standby */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_CTRL_AF, MCU_AF_MODE_STANDBY);
	msleep(10);
#endif
	/* clear ois err reg */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_CHECKSUM, 0x0);
#ifdef USE_TELE2_OIS_AF_COMMON_INTERFACE
	/* write AF CTRL active */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_CTRL_AF, MCU_AF_MODE_ACTIVE);
#endif
}

#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
void is_vendor_ois_get_hw_param(struct cam_hw_param *hw_param, u16 i2c_error_reg)
{
	if (i2c_error_reg & MCU_REAR_OIS_ERR_REG)
		is_sec_get_hw_param(&hw_param, SENSOR_POSITION_REAR);
	else if (i2c_error_reg & MCU_REAR_2ND_OIS_ERR_REG)
		is_sec_get_hw_param(&hw_param, SENSOR_POSITION_REAR2);
	else if (i2c_error_reg & MCU_REAR_3RD_OIS_ERR_REG)
		is_sec_get_hw_param(&hw_param, SENSOR_POSITION_REAR4);
}
#endif

int is_vendor_ois_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
#ifdef USE_OIS_SLEEP_MODE
	u8 read_gyrocalcen = 0;
#endif
	u8 val = 0;
	u8 error_reg[2] = {0, };
	u8 gyro_orientation = 0;
	u8 wx_pole = 0;
	u8 wy_pole = 0;
#if defined(CAMERA_2ND_OIS)
	u8 tx_pole = 0;
	u8 ty_pole = 0;
#endif
#if defined(CAMERA_3RD_OIS)
	u8 t2x_pole = 0;
	u8 t2y_pole = 0;
#endif
	int retries = 600;
	int i = 0;
	int scale_factor = OIS_GYRO_SCALE_FACTOR;
	long gyro_data_x = 0, gyro_data_y = 0, gyro_data_z = 0, gyro_data_size = 0;
	u8 gyro_x = 0, gyro_x2 = 0;
	u8 gyro_y = 0, gyro_y2 = 0;
	u8 gyro_z = 0, gyro_z2 = 0;
#if defined(CAMERA_2ND_OIS)
	int tele_cmd_xcoef = 0;
	int tele_cmd_ycoef = 0;
#ifndef OIS_DUAL_CAL_DEFAULT_VALUE_TELE
	struct is_vendor_private *vendor_priv;
	u8 tele_xcoef[2];
	u8 tele_ycoef[2];
	long efs_size = 0;
#if !defined(OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE)
	int rom_id = 0;
	char *cal_buf;
	struct is_vendor_rom *rom_info = NULL;
	u8 eeprom_xcoef[2];
	u8 eeprom_ycoef[2];
#endif
#endif
#endif /* CAMERA_2ND_OIS */
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_ois *ois = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_ois_info *ois_pinfo = NULL;
	struct is_core *core = NULL;
#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
	struct cam_hw_param *hw_param = NULL;
	u16 i2c_error_reg = 0;
#endif

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err_mcu("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if (!is_mcu) {
		err_mcu("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_peri = is_mcu->sensor_peri;
	if (!sensor_peri) {
		err_mcu("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err_mcu("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	core = is_get_is_core();
	if (!core) {
		err_mcu("%s, core is null", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_ois_get_phone_version(&ois_pinfo);

	ois = is_mcu->ois;
	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->ois_mode = sensor_peri->mcu->ois->ois_mode;
	ois->coef = 0;
	ois->pre_coef = 255;
	ois->fadeupdown = false;
	ois->initial_centering_mode = false;
	ois->af_pos_wide = 0;
#if defined(CAMERA_2ND_OIS)
	ois->af_pos_tele = 0;
#endif
	ois->ois_power_mode = -1;
	ois_pinfo->reset_check = false;

	if (mcu->ois_hw_check) {
		if (module->position == SENSOR_POSITION_REAR)
			mcu->ois_wide_init = true;
#if defined(CAMERA_2ND_OIS)
		else if (module->position == SENSOR_POSITION_REAR2)
			mcu->ois_tele_init = true;
#endif
#if defined(CAMERA_3RD_OIS)
		else if (module->position == SENSOR_POSITION_REAR4)
			mcu->ois_tele2_init = true;
#endif

		info_mcu("%s %d sensor(%d) mcu is already initialized.\n", __func__, __LINE__, module->position);
		ois->ois_shift_available = true;
	}

	if (mcu->ois_hw_check && mcu->current_error_reg && (mcu->current_power_mode >= OIS_POWER_MODE_DUAL)) {
		val = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_STATUS);
		if (val == 0x01) { //idle status check, 0x01 == idle.
#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
			i2c_error_reg = mcu->current_error_reg;
			if (i2c_error_reg & MCU_I2C_ERR_VAL)
				is_vendor_ois_get_hw_param(hw_param, i2c_error_reg);
#endif
			is_vendor_ois_reset_mcu(mcu);
			is_vendor_ois_set_dev_ctrl(subdev, 1);
			info_mcu("%s Process dev ctrl again in case of error reg detected.", __func__);
			error_reg[0] = (u8)is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_GYRO_RESULT);
			error_reg[1] = (u8)is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_CHECKSUM);
			mcu->current_error_reg = (error_reg[1] << 8) | error_reg[0];
#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
			if (hw_param && (hw_param->i2c_ois_err_cnt > 0) && (mcu->current_error_reg == 0))
				hw_param->i2c_ois_err_cnt--;
#endif
			info_mcu("%s error reg value = 0x%02x/0x%02x", __func__, error_reg[0], error_reg[1]);
		} else {
			info_mcu("%s Do not process dev ctrl again. Mcu is not idle.", __func__);
		}
	}

	if (!mcu->ois_hw_check && test_bit(OM_HW_RUN, &mcu->state)) {
		do {
			val = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_STATUS);
			usleep_range(500, 510);
			if (--retries < 0) {
				err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
				break;
			}
		} while (val != 0x01);

		error_reg[0] = (u8)is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_GYRO_RESULT);
		error_reg[1] = (u8)is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_CHECKSUM);
		mcu->current_error_reg = (error_reg[1] << 8) | error_reg[0];
		info_mcu("%s error reg value = 0x%02x/0x%02x", __func__, error_reg[0], error_reg[1]);

#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
#if defined(CAMERA_3RD_OIS)
		if (mcu->current_power_mode >= OIS_POWER_MODE_TRIPLE)
#elif defined(CAMERA_2ND_OIS)
		if (mcu->current_power_mode >= OIS_POWER_MODE_DUAL)
#endif
		{
			i2c_error_reg = (error_reg[1] << 8) | error_reg[0];
			if (i2c_error_reg & MCU_I2C_ERR_VAL) {
				is_vendor_ois_get_hw_param(hw_param, i2c_error_reg);
				if (hw_param) {
					hw_param->i2c_ois_err_cnt++;
					if (i2c_error_reg & MCU_REAR_3RD_OIS_ERR_REG)
						hw_param->i2c_af_err_cnt++;
				}
			}
		}
#endif

		/* MCU err reg recovery code */
		if (core->mcu->need_reset_mcu && error_reg[1]) {
			is_vendor_ois_reset_mcu(mcu);
			core->mcu->need_reset_mcu = false;
			info("[%s] clear ois reset flag.", __func__);
		}

		if (val == 0x01) {
			/* loading gyro data */
			gyro_data_size = is_vendor_ois_get_efs_data(mcu, &gyro_data_x, &gyro_data_y, &gyro_data_z);
			info_mcu("Read Gyro offset data :  0x%04lx, 0x%04lx, 0x%04lx", gyro_data_x, gyro_data_y, gyro_data_z);
			gyro_data_x = gyro_data_x * scale_factor;
			gyro_data_y = gyro_data_y * scale_factor;
			gyro_data_z = gyro_data_z * scale_factor;
			gyro_data_x = gyro_data_x / 1000;
			gyro_data_y = gyro_data_y / 1000;
			gyro_data_z = gyro_data_z / 1000;
			if (gyro_data_size > 0) {
				gyro_x = gyro_data_x & 0xFF;
				gyro_x2 = (gyro_data_x >> 8) & 0xFF;
				gyro_y = gyro_data_y & 0xFF;
				gyro_y2 = (gyro_data_y >> 8) & 0xFF;
				gyro_z = gyro_data_z & 0xFF;
				gyro_z2 = (gyro_data_z >> 8) & 0xFF;
				is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_RAW_DEBUG_X1, gyro_x);
				is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_RAW_DEBUG_X2, gyro_x2);
				is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_RAW_DEBUG_Y1, gyro_y);
				is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_RAW_DEBUG_Y2, gyro_y2);
				is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_RAW_DEBUG_Z1, gyro_z);
				is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_RAW_DEBUG_Z2, gyro_z2);
				info_mcu("Write Gyro offset data :  0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x", gyro_x, gyro_x2, gyro_y, gyro_y2, gyro_z, gyro_z2);
			}
			/* write wide xgg ygg xcoef ycoef */
			if (ois_pinfo->wide_romdata.cal_mark[0] == 0xBB) {
				for (i = 0; i < 4; i++) {
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_REAR_XGG1 + i, ois_pinfo->wide_romdata.xgg[i]);
				}
				for (i = 0; i < 4; i++) {
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_REAR_YGG1 + i, ois_pinfo->wide_romdata.ygg[i]);
				}
				for (i = 0; i < 2; i++) {
#if defined(OIS_DUAL_CAL_DEFAULT_VALUE_WIDE)
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_XCOEF_M1_1 + i, OIS_DUAL_CAL_DEFAULT_VALUE_WIDE);
#else
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_XCOEF_M1_1 + i, ois_pinfo->wide_romdata.xcoef[i]);
#endif
				}
				for (i = 0; i < 2; i++) {
#if defined(OIS_DUAL_CAL_DEFAULT_VALUE_WIDE)
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_YCOEF_M1_1 + i, OIS_DUAL_CAL_DEFAULT_VALUE_WIDE);
#else
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_YCOEF_M1_1 + i, ois_pinfo->wide_romdata.ycoef[i]);
#endif
				}
			} else {
				info_mcu("%s Does not loading wide xgg/ygg data from eeprom.", __func__);
			}

#if defined(CAMERA_2ND_OIS)
			/* write tele xgg ygg xcoef ycoef */
			if (ois_pinfo->tele_tilt_romdata.cal_mark[0] == 0xBB) {
#ifdef OIS_DUAL_CAL_USE_REAR3_DATA
				tele_cmd_xcoef = OIS_CMD_XCOEF_M3_1;
				tele_cmd_ycoef = OIS_CMD_YCOEF_M3_1;
#else
				tele_cmd_xcoef = OIS_CMD_XCOEF_M2_1;
				tele_cmd_ycoef = OIS_CMD_YCOEF_M2_1;
#endif

#ifndef OIS_DUAL_CAL_DEFAULT_VALUE_TELE
				vendor_priv = core->vendor.private_data;
				efs_size = vendor_priv->tilt_cal_tele2_efs_size;
				if (efs_size) {
					efs_info.ois_hall_shift_x = *((s16 *)&vendor_priv->tilt_cal_tele2_efs_data[MCU_HALL_SHIFT_ADDR_X_M2]);
					efs_info.ois_hall_shift_y = *((s16 *)&vendor_priv->tilt_cal_tele2_efs_data[MCU_HALL_SHIFT_ADDR_Y_M2]);
					set_bit(IS_EFS_STATE_READ, &efs_info.efs_state);
				} else {
					clear_bit(IS_EFS_STATE_READ, &efs_info.efs_state);
				}
#endif
				for (i = 0; i < 4; i++) {
#if defined(CAMERA_2ND_OIS)
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_REAR2_XGG1 + i, ois_pinfo->tele_romdata.xgg[i]);
#endif
#if defined(CAMERA_3RD_OIS)
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_REAR3_XGG1 + i, ois_pinfo->tele2_romdata.xgg[i]);
#endif
				}
				for (i = 0; i < 4; i++) {
#if defined(CAMERA_2ND_OIS)
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_REAR2_YGG1 + i, ois_pinfo->tele_romdata.ygg[i]);
#endif
#if defined(CAMERA_3RD_OIS)
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_REAR3_YGG1 + i, ois_pinfo->tele2_romdata.ygg[i]);
#endif
				}
#ifdef OIS_DUAL_CAL_DEFAULT_VALUE_TELE
				is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_xcoef, OIS_DUAL_CAL_DEFAULT_VALUE_TELE);
				is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_xcoef + 1, OIS_DUAL_CAL_DEFAULT_VALUE_TELE);
				is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_ycoef, OIS_DUAL_CAL_DEFAULT_VALUE_TELE);
				is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_ycoef + 1, OIS_DUAL_CAL_DEFAULT_VALUE_TELE);

				info_mcu("%s tele use default coef value", __func__);
#else
				if (!test_bit(IS_EFS_STATE_READ, &efs_info.efs_state)) {
#if defined(OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE)
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_xcoef, OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE);
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_xcoef + 1, OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE);
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_ycoef, OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE);
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_ycoef + 1, OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE);

					info_mcu("%s tele use default eeprom coef value", __func__);
#else
					rom_id = is_vendor_get_rom_id_from_position(SENSOR_POSITION_REAR2);
					is_sec_get_rom_info(&rom_info, rom_id);
					cal_buf = rom_info->buf;

					if (rom_info->rom_dualcal_slave1_oisshift_x_addr != 0xFFFF
						&& rom_info->rom_dualcal_slave1_oisshift_x_addr < rom_info->rom_size) {
						eeprom_xcoef[0] = *((u8 *)&cal_buf[rom_info->rom_dualcal_slave1_oisshift_x_addr]);
						eeprom_xcoef[1] = *((u8 *)&cal_buf[rom_info->rom_dualcal_slave1_oisshift_x_addr + 1]);
					} else {
						eeprom_xcoef[0] = 0;
						eeprom_xcoef[1] = 0;
					}
					if (rom_info->rom_dualcal_slave1_oisshift_y_addr != 0xFFFF
						&& rom_info->rom_dualcal_slave1_oisshift_y_addr < rom_info->rom_size) {
						eeprom_ycoef[0] = *((u8 *)&cal_buf[rom_info->rom_dualcal_slave1_oisshift_y_addr]);
						eeprom_ycoef[1] = *((u8 *)&cal_buf[rom_info->rom_dualcal_slave1_oisshift_y_addr + 1]);
					} else {
						eeprom_ycoef[0] = 0;
						eeprom_ycoef[1] = 0;
					}

					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_xcoef, eeprom_xcoef[0]);
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_xcoef + 1, eeprom_xcoef[1]);
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_ycoef, eeprom_ycoef[0]);
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_ycoef + 1, eeprom_ycoef[1]);

					info_mcu("%s tele eeprom xcoef = %d/%d, ycoef = %d/%d", __func__, eeprom_xcoef[0], eeprom_xcoef[1],
						eeprom_ycoef[0], eeprom_ycoef[1]);
#endif
				} else {
#ifdef USE_OIS_SHIFT_FOR_12BIT
					efs_info.ois_hall_shift_x >>= 2;
					efs_info.ois_hall_shift_y >>= 2;
#endif
					tele_xcoef[0] = efs_info.ois_hall_shift_x & 0xFF;
					tele_xcoef[1] = (efs_info.ois_hall_shift_x >> 8) & 0xFF;
					tele_ycoef[0] = efs_info.ois_hall_shift_y & 0xFF;
					tele_ycoef[1] = (efs_info.ois_hall_shift_y >> 8) & 0xFF;

					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_xcoef, tele_xcoef[0]);
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_xcoef + 1, tele_xcoef[1]);
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_ycoef, tele_ycoef[0]);
					is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], tele_cmd_ycoef + 1, tele_ycoef[1]);

					info_mcu("%s tele efs xcoef = %d, ycoef = %d", __func__, efs_info.ois_hall_shift_x, efs_info.ois_hall_shift_y);
				}
#endif
			} else {
				info_mcu("%s Does not loading tele xgg/ygg data from eeprom.", __func__);
			}
#endif /* CAMERA_2ND_OIS */

			/* enable dual cal */
			is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_ENABLE_DUALCAL, 0x01);

			wx_pole = mcu->ois_gyro_direction[0];
			wy_pole = mcu->ois_gyro_direction[1];
			gyro_orientation = mcu->ois_gyro_direction[2];
#if defined(CAMERA_2ND_OIS)
			tx_pole = mcu->ois_gyro_direction[3];
			ty_pole = mcu->ois_gyro_direction[4];
#endif
#if defined(CAMERA_3RD_OIS)
			t2x_pole = mcu->ois_gyro_direction[5];
			t2y_pole = mcu->ois_gyro_direction[6];
#endif

#if defined(CAMERA_3RD_OIS)
			info_mcu("%s gyro direction list  %d,%d,%d,%d,%d,%d,%d\n", __func__, wx_pole, wy_pole, gyro_orientation,
				tx_pole, ty_pole, t2x_pole, t2y_pole);
#elif defined(CAMERA_2ND_OIS)
			info_mcu("%s gyro direction list  %d,%d,%d,%d,%d\n", __func__, wx_pole, wy_pole, gyro_orientation,
				tx_pole, ty_pole);
#else
			info_mcu("%s gyro direction list  %d,%d,%d\n", __func__, wx_pole, wy_pole, gyro_orientation);
#endif

			is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_GYRO_POLA_X, wx_pole);
			is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_GYRO_POLA_Y, wy_pole);
			is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_GYRO_ORIENT, gyro_orientation);
#if defined(CAMERA_2ND_OIS)
			is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_GYRO_POLA_X_M2, tx_pole);
			is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_GYRO_POLA_Y_M2, ty_pole);
#endif
#if defined(CAMERA_3RD_OIS)
			is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_GYRO_POLA_X_M3, t2x_pole);
			is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_GYRO_POLA_Y_M3, t2y_pole);
#endif
			info_mcu("%s gyro init data applied.\n", __func__);

			mcu->ois_hw_check = true;

			if (module->position == SENSOR_POSITION_REAR)
				mcu->ois_wide_init = true;
#if defined(CAMERA_2ND_OIS)
			else if (module->position == SENSOR_POSITION_REAR2)
				mcu->ois_tele_init = true;
#endif
#if defined(CAMERA_3RD_OIS)
			else if (module->position == SENSOR_POSITION_REAR4) {
				mcu->ois_tele2_init = true;
				mcu->need_af_delay = true;
			}
#endif
		}
	}

	info_mcu("%s sensor(%d) X\n", __func__, module->position);

	return ret;
}

int is_vendor_ois_init_factory(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 val = 0;
	int retries = 600;
	u8 gyro_orientation = 0;
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_ois *ois = NULL;
	struct is_ois_info *ois_pinfo = NULL;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err_mcu("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if (!is_mcu) {
		err_mcu("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_ois_get_phone_version(&ois_pinfo);

	ois = is_mcu->ois;
	ois->ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->coef = 0;
	ois->pre_coef = 255;
	ois->fadeupdown = false;
	ois->initial_centering_mode = false;
	ois->af_pos_wide = 0;
#if defined(CAMERA_2ND_OIS)
	ois->af_pos_tele = 0;
#endif
	ois->ois_power_mode = -1;
	ois_pinfo->reset_check = false;

	if (test_bit(OM_HW_RUN, &mcu->state)) {
		do {
			val = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_STATUS);
			usleep_range(500, 510);
			if (--retries < 0) {
				err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
				break;
			}
		} while (val != 0x01);
	}

	/* OIS SEL (wide : 1 , tele : 2, w/t : 3, tele2 : 4, triple : 7) */
#if defined(CAMERA_3RD_OIS)
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x07);
#elif defined(CAMERA_2ND_OIS)
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x03);
#else
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x01);
#endif

	gyro_orientation = mcu->ois_gyro_direction[2];
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_GYRO_ORIENT, gyro_orientation);

	info_mcu("%s sensor(%d) X\n", __func__, ois->device);
	return ret;
}

#if defined(CAMERA_3RD_OIS)
void is_vendor_ois_init_rear2(struct is_core *core)
{
	u8 val = 0;
	int retries = 600;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	/* check ois status */
	do {
		val = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_STATUS);
		usleep_range(500, 510);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	/* set power mode (wide : 1 , tele : 2, w/t : 3, tele2 : 4, triple : 7) */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x04);

	info_mcu("%s : X\n", __func__);

	return;
}
#endif /* CAMERA_3RD_OIS */

int is_vendor_ois_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	struct is_core *core = NULL;
	int retries = 50;
	u8 val = 0;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if (!is_mcu) {
		err_mcu("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_peri = is_mcu->sensor_peri;
	if (!sensor_peri) {
		err_mcu("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err_mcu("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	core = is_get_is_core();
	if (!core) {
		err("core is null");
		ret = -EINVAL;
		return ret;
	}

	if (module->position  == SENSOR_POSITION_REAR)
		mcu->ois_wide_init = false;
#if defined(CAMERA_2ND_OIS)
	else if  (module->position  == SENSOR_POSITION_REAR2)
		mcu->ois_tele_init = false;
#endif
#if defined(CAMERA_3RD_OIS)
	else if  (module->position  == SENSOR_POSITION_REAR4)
		mcu->ois_tele2_init = false;
#endif
	if (mcu->ois_hw_check) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_START, 0x00);
		usleep_range(2000, 2100);
		do {
			val = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_STATUS);
			usleep_range(1000, 1100);
			if (--retries < 0) {
				err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
				break;
			}
		} while (val != 0x01);

		mcu->ois_fadeupdown = false;
		mcu->ois_hw_check = false;
		mcu->need_af_delay = false;
		mcu->is_mcu_active = false;
		info_mcu("%s ois stop. sensor = (%d)X\n", __func__, module->position);
	}

	if (core->mcu->need_reset_mcu) {
		core->mcu->need_reset_mcu = false;
		info("[%s] clear ois reset flag.", __func__);
	}

	info_mcu("%s sensor = (%d)X\n", __func__, module->position);

	return ret;
}

int is_vendor_ois_af_get_position(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	ctrl->value = ACTUATOR_STATUS_NO_BUSY;

	return 0;
}

int is_vendor_ois_af_valid_check(void)
{
	int i;
	struct is_sysfs_actuator *sysfs_actuator;

	sysfs_actuator = is_get_sysfs_actuator();

	if (sysfs_actuator->init_step > 0) {
		for (i = 0; i < sysfs_actuator->init_step; i++) {
			if (sysfs_actuator->init_positions[i] < 0) {
				warn("invalid position value, default setting to position");
				return 0;
			} else if (sysfs_actuator->init_delays[i] < 0) {
				warn("invalid delay value, default setting to delay");
				return 0;
			}
		}
	} else
		return 0;

	return sysfs_actuator->init_step;
}

int is_vendor_ois_af_write_position(struct ois_mcu_dev *mcu, u32 val)
{
	u8 val_high = 0, val_low = 0;

	dbg_ois("%s : E\n", __func__);

	val_high = (val & 0x0FFF) >> 4;
	val_low = (val & 0x000F) << 4;

#if defined(USE_TELE_OIS_AF_COMMON_INTERFACE)
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_POS1_REAR2_AF, val_high);
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_POS2_REAR2_AF, val_low);
#elif defined(USE_TELE2_OIS_AF_COMMON_INTERFACE)
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_POS1_REAR3_AF, val_high);
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_POS2_REAR3_AF, val_low);
#endif
	usleep_range(2000, 2100);

	dbg_ois("%s : [val : 0x%08x] X\n", __func__, val);

	return 0;
}

static int is_vendor_ois_af_init_position(struct ois_mcu_dev *mcu,
		struct is_actuator *actuator)
{
	int i;
	int ret = 0;
	int init_step = 0;
	struct is_sysfs_actuator *sysfs_actuator;

	sysfs_actuator = is_get_sysfs_actuator();
	init_step = is_vendor_ois_af_valid_check();

	if (init_step > 0) {
		for (i = 0; i < init_step; i++) {
			ret = is_vendor_ois_af_write_position(mcu, sysfs_actuator->init_positions[i]);
			if (ret < 0)
				goto p_err;

			mdelay(sysfs_actuator->init_delays[i]);
		}

		actuator->position = sysfs_actuator->init_positions[i];
	} else {
		/* wide, tele camera uses previous position at initial time */
		if (actuator->device == 1 || actuator->position == 0)
			actuator->position = MCU_ACT_DEFAULT_FIRST_POSITION;

		ret = is_vendor_ois_af_write_position(mcu, actuator->position);
		if (ret < 0)
			goto p_err;
	}

p_err:
	return ret;
}

int is_vendor_ois_af_init(struct v4l2_subdev *subdev, u32 val)
{
	struct is_actuator *actuator = NULL;

	WARN_ON(!subdev);

	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	WARN_ON(!actuator);

	actuator->position = val;

	dbg_ois("%s : X\n", __func__);

	return 0;
}

int is_vendor_ois_af_set_active(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct ois_mcu_dev *mcu = NULL;
	struct is_core *core;
	struct is_mcu *is_mcu = NULL;
	struct is_actuator *actuator = NULL;

	WARN_ON(!subdev);

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if (!is_mcu) {
		err_mcu("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	core = is_get_is_core();
	if (!core) {
		err("core is null");
		return -EINVAL;
	}

	mcu = core->mcu;
	actuator = is_mcu->actuator;

	info_mcu("%s : E\n", __func__);

	if (enable) {
		if (mcu->need_af_delay) {
			/* delay for mcu init <-> af ctrl */
			msleep(10);
			mcu->need_af_delay = false;
			info_mcu("%s : set af delay\n", __func__);
		}

		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_CTRL_AF, MCU_AF_MODE_ACTIVE);
		msleep(10);
		is_vendor_ois_af_init_position(mcu, actuator);
	} else {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_CTRL_AF, MCU_AF_MODE_STANDBY);
	}

	info_mcu("%s : enable = %d X\n", __func__, enable);

	return 0;
}

int is_vendor_ois_af_set_position(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	struct ois_mcu_dev *mcu = NULL;
	struct is_actuator *actuator = NULL;
	struct is_core *core;
	u32 position = 0;

	WARN_ON(!subdev);

	core = is_get_is_core();
	if (!core) {
		err("core is null");
		return -EINVAL;
	}

	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	WARN_ON(!actuator);

	mcu = core->mcu;
	position = ctrl->value;

	is_vendor_ois_af_write_position(mcu, position);

	actuator->position = position;

	dbg_ois("%s : [position : 0x%08x] X\n", __func__, position);

	return 0;
}

long is_vendor_ois_actuator_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct v4l2_control *ctrl;

	ctrl = (struct v4l2_control *)arg;
	switch (cmd) {
	case SENSOR_IOCTL_ACT_S_CTRL:
		ret = is_vendor_ois_af_set_position(subdev, ctrl);
		if (ret) {
			err("mcu actuator_s_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_ACT_G_CTRL:
		ret = is_vendor_ois_af_get_position(subdev, ctrl);
		if (ret) {
			err("mcu actuator_g_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	default:
		err("Unknown command(%#x)", cmd);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return (long)ret;
}

#if defined(CAMERA_3RD_OIS) && defined(USE_TELE2_OIS_AF_COMMON_INTERFACE)
int is_vendor_ois_set_sleep_mode_folded_zoom(void)
{
	struct ois_mcu_dev *mcu = NULL;
	struct is_core *core;
	u8 state = 0;

	core = is_get_is_core();
	if (!core) {
		err("core is null");
		return -EINVAL;
	}

	mcu = core->mcu;

	state = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_CTRL_AF);
	state = state & MCU_AF_MODE_STANDBY;
	if (state == MCU_AF_MODE_ACTIVE) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_CTRL_AF, MCU_AF_MODE_STANDBY);
		usleep_range(5000, 5010);
	}

	info_mcu("%s : set sleep mode folded zoom. state = %d\n", __func__, state);

	return 0;
}
#endif

#if defined(CAMERA_2ND_OIS)
int is_vendor_ois_set_power_mode(struct v4l2_subdev *subdev, int forceMode)
{
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;
	bool camera_running;
	bool camera_running2;
#if defined(CAMERA_3RD_OIS)
	bool camera_running4;
#endif

	mcu = (struct ois_mcu_dev*)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		return -EINVAL;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if (!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		return -EINVAL;
	}

	ois = is_mcu->ois;
	if (!ois) {
		err("%s, ois subdev is NULL", __func__);
		return -EINVAL;
	}

	info_mcu("%s : E\n", __func__);

	camera_running = is_vendor_check_camera_running(SENSOR_POSITION_REAR);
	camera_running2 = is_vendor_check_camera_running(SENSOR_POSITION_REAR2);
#if defined(CAMERA_3RD_OIS)
	camera_running4 = is_vendor_check_camera_running(SENSOR_POSITION_REAR4);
#endif

	/* OIS SEL (wide : 1 , tele : 2, w/t : 3, tele2 : 4, triple : 7) */
#if defined(CAMERA_3RD_OIS)
	if (forceMode == OIS_USE_UW_ONLY) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x00);
		ois->ois_power_mode = OIS_POWER_MODE_NONE;
	} else if (camera_running && !camera_running2 && !camera_running4) { //TEMP_OLYMPUS ==> need to be changed based on camera scenario
#ifdef USE_TELE2_OIS_AF_COMMON_INTERFACE
		is_vendor_ois_set_sleep_mode_folded_zoom();
#endif
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x01);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_WIDE;
		usleep_range(5000, 5010);
	} else if (!camera_running && camera_running2 && !camera_running4) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x02);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_TELE;
	} else if (camera_running && camera_running2 && !camera_running4) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x03);
		ois->ois_power_mode = OIS_POWER_MODE_DUAL;
	} else if (!camera_running && !camera_running2 && camera_running4) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x04);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_TELE2;
	} else {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x07);
		ois->ois_power_mode = OIS_POWER_MODE_TRIPLE;
	}
#else
	if (forceMode == OIS_USE_UW_ONLY) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x00);
		ois->ois_power_mode = OIS_POWER_MODE_NONE;
	} else if (forceMode == OIS_USE_UW_WIDE) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x01);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_WIDE;
	} else if (camera_running && !camera_running2) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x01);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_WIDE;
	} else if (!camera_running && camera_running2) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x02);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_TELE;
	} else {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_OIS_SEL, 0x03);
		ois->ois_power_mode = OIS_POWER_MODE_DUAL;
	}
#endif
	mcu->current_power_mode = ois->ois_power_mode;
	info_mcu("%s ois power setting is %d X\n", __func__, ois->ois_power_mode);

	return 0;
}
#endif

void is_vendor_ois_device_ctrl(struct ois_mcu_dev *mcu, u8 value)
{
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_DEVCTRL, value);
}

int is_vendor_ois_set_dev_ctrl(struct v4l2_subdev *subdev, int forceMode)
{
	struct ois_mcu_dev *mcu = NULL;
	u8 val = 0;
	int retry = 200;

	mcu = (struct ois_mcu_dev*)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		return -EINVAL;
	}

	info_mcu("%s : E\n", __func__);

#if defined(CONFIG_SEC_FACTORY) //Factory timing issue.
	retry = 600;
#endif

	if (!(mcu->ois_wide_init
#if defined(CAMERA_2ND_OIS)
		|| mcu->ois_tele_init
#endif
#if defined(CAMERA_3RD_OIS)
		|| mcu->ois_tele2_init
#endif
	) || forceMode) {
		if (mcu->dev_ctrl_state == false || forceMode) {
			if (forceMode)
				is_vendor_ois_device_ctrl(mcu, 0x02);
			else
				is_vendor_ois_device_ctrl(mcu, 0x01);
			do {
				usleep_range(500, 510);
				val = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_DEVCTRL);
				if (--retry < 0) {
					err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
					break;
				}
			} while (val != 0x00);

			if (val == 0x00) {
				mcu->dev_ctrl_state = true;
				info_mcu("%s dev ctrl done.", __func__);
			}
		}
	}

	info_mcu("%s : X\n", __func__);

	return 0;
}

int is_vendor_ois_bypass_read(struct ois_mcu_dev *mcu, u16 id, u16 reg, u8 reg_size, u8 *buf, u8 data_size)
{
	u8 mode = 0;
	u8 rcvdata = 0;
	u8 dev_id[2] = {0, };
	u8 reg_add[2] = {0, };
	int retries = 1000;
	int i = 0;

	info_mcu("%s E\n", __func__);

	/* device id */
	dev_id[0] = id & 0xFF;
	dev_id[1] = (id >> 8) & 0xFF;
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DEVICE_ID1, dev_id[0]);
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DEVICE_ID2, dev_id[1]);

	/* register address */
	reg_add[0] = reg & 0xFF;
	reg_add[1] = (reg >> 8) & 0xFF;
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_REG_ADD1, reg_add[0]);
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_REG_ADD2, reg_add[1]);

	/* reg size */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_REG_SIZE, reg_size);

	/* data size */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DATA_SIZE, data_size);

	/* run bypass mode */
	mode = 0x02;
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_CTRL, mode);

	do {
		rcvdata = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_CTRL);
		usleep_range(1000, 1100);
		if (--retries < 0) {
			err_mcu("%s read status failed!!!!, data = 0x%04x\n", __func__, rcvdata);
			break;
		}
	} while (rcvdata != 0x00);

	/* get data */
	for (i = 0; i < data_size; i++) {
		rcvdata = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DATA_TRANSFER + i);
		*(buf + i) = rcvdata & 0xFF;
	}

	info_mcu("%s X\n", __func__);

	return 0;
}

int is_vendor_ois_bypass_write(struct ois_mcu_dev *mcu, u16 id, u16 reg, u8 reg_size, u8 *buf, u8 data_size)
{
	u8 mode = 0;
	u8 rcvdata = 0;
	u8 dev_id[2] = {0, };
	u8 reg_add[2] = {0, };
	int retries = 1000;
	int i = 0;

	info_mcu("%s E\n", __func__);

	/* device id */
	dev_id[0] = id & 0xFF;
	dev_id[1] = (id >> 8) & 0xFF;
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DEVICE_ID1, dev_id[0]);
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DEVICE_ID2, dev_id[1]);

	/* register address */
	reg_add[0] = reg& 0xFF;
	reg_add[1] = (reg >> 8) & 0xFF;
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_REG_ADD1, reg_add[0]);
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_REG_ADD2, reg_add[1]);

	/* reg size */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_REG_SIZE, reg_size);

	/* data size */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DATA_SIZE, data_size);

	/* send data */
	for (i = 0; i < data_size; i++) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DATA_TRANSFER + i, *(buf + i) & 0xFF);
	}

	/* run bypass mode */
	mode = 0x02;
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_CTRL, mode);

	do {
		rcvdata = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_CTRL);
		usleep_range(1000, 1100);
		if (--retries < 0) {
			err_mcu("%s read status failed!!!!, data = 0x%04x\n", __func__, rcvdata);
			break;
		}
		i++;
	} while (rcvdata != 0x00);

	info_mcu("%s X\n", __func__);

	return 0;
}

int is_vendor_ois_check_cross_talk(struct v4l2_subdev *subdev, u16 *hall_data)
{
	int ret = 0;
	u8 val = 0;
	u16 x_target = 0;
	int retries = 600;
	u8 addr_size = 0x02;
	u8 data[2] = {0, };
	u8 hall_value[2] = {0, };
	int i = 0;
	struct ois_mcu_dev *mcu = NULL;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err_mcu("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	do {
		val = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_STATUS);
		usleep_range(500, 510);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	data[0] = 0x08;
	is_vendor_ois_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0002, addr_size, data, 0x01);
	data[0] = 0x01;
	is_vendor_ois_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0080, addr_size, data, 0x01);
	data[0] = 0x01;
	is_vendor_ois_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0000, addr_size, data, 0x01);

	data[0] = 0x20;
	data[1] = 0x03;
	is_vendor_ois_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0022, addr_size, data, 0x02);
	data[0] = 0x00;
	data[1] = 0x08;
	is_vendor_ois_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0024, addr_size, data, 0x02);

	x_target = 800;
	for (i = 0; i < 10; i++) {
		data[0] = x_target & 0xFF;
		data[1] = (x_target >> 8) & 0xFF;
		is_vendor_ois_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0022, addr_size, data, 0x02);
		msleep(45);

		is_vendor_ois_bypass_read(mcu, MCU_BYPASS_MODE_READ_ID, 0x0090, addr_size, hall_value, 0x02);
		*(hall_data + i) = (hall_value[1] << 8) | hall_value[0];
		info_mcu("%s hall_data[0] = 0x%02x, hall_value[1] = 0x%02x", __func__, hall_value[0], hall_value[1]);
		x_target += 300;
	}

	info_mcu("%s  X\n", __func__);

	return ret;
}

int is_vendor_ois_bypass_read_mode1(struct ois_mcu_dev *mcu, u8 id, u8 reg, u8 *buf, u8 data_size)
{
	u8 mode = 0;
	u8 rcvdata = 0;
	int retries = 1000;
	int i = 0;

	info_mcu("%s E\n", __func__);

	/* device id */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DEVICE_ID1, id);

	/* register address */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DEVICE_ID2, reg);

	/* data size */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_REG_ADD1, data_size);

	/* run bypass mode */
	mode = 0x01;
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_CTRL, mode);

	do {
		rcvdata = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_CTRL);
		usleep_range(1000, 1100);
		if (--retries < 0) {
			err_mcu("%s read status failed!!!!, data = 0x%04x\n", __func__, rcvdata);
			break;
		}
	} while (rcvdata != 0x00);

	/* get data */
	for (i = 0; i < data_size; i++) {
		rcvdata = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_REG_ADD2 + i);
		*(buf + i) = rcvdata & 0xFF;
	}

	info_mcu("%s X\n", __func__);

	return 0;
}

int is_vendor_ois_bypass_write_mode1(struct ois_mcu_dev *mcu, u8 id, u8 reg, u8 *buf, u8 data_size)
{
	u8 mode = 0;
	u8 rcvdata = 0;
	int retries = 1000;
	int i = 0;

	info_mcu("%s E\n", __func__);

	/* device id */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DEVICE_ID1, id);

	/* register address */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_DEVICE_ID2, reg);

	/* data size */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_REG_ADD1, data_size);

	/* send data */
	for (i = 0; i < data_size; i++) {
		is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_REG_ADD2 + i, *(buf + i) & 0xFF);
	}

	/* run bypass mode */
	mode = 0x01;
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_CTRL, mode);

	do {
		rcvdata = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_BYPASS_CTRL);
		usleep_range(1000, 1100);
		if (--retries < 0) {
			err_mcu("%s read status failed!!!!, data = 0x%04x\n", __func__, rcvdata);
			break;
		}
		i++;
	} while (rcvdata != 0x00);

	info_mcu("%s X\n", __func__);

	return 0;
}

int is_vendor_ois_check_hall_cal(struct v4l2_subdev *subdev, u16 *hall_cal_data)
{
	int ret = 0;
	u8 val = 0;
	int retries = 600;
	u8 rxbuf[32] = {0, };
	u8 txbuf[32] = {0, };
	u16 af_best_pos = 0;
	u16 temp = 0;
	int pre_pcal[2] = {0, };
	int pre_ncal[2] = {0, };
	int cur_pcal[2] = {0, };
	int cur_ncal[2] = {0, };
	struct ois_mcu_dev *mcu = NULL;
	struct is_core *core = NULL;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err_mcu("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	core = is_get_is_core();
	if (!core) {
		err("core is null");
		ret = -EINVAL;
		return ret;
	}

	do {
		val = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_STATUS);
		usleep_range(500, 510);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	/* Read stored calibration mark */
	is_vendor_ois_bypass_read_mode1(mcu, 0xE9, 0xE4, rxbuf, 0x01);
	info_mcu("read reg(0xE4) = 0x%02x", rxbuf[0]);

	if (rxbuf[0] != 0x01) {
		info_mcu("calibration data empty(0x%02x)", rxbuf[0]);
		return ret;
	}

	/* Read stored AF best position*/
	is_vendor_ois_bypass_read_mode1(mcu, 0xE9, 0xE5, rxbuf, 0x01);
	af_best_pos = (u16)rxbuf[0] << 4;
	info_mcu("read reg(0xE5) = 0x%04x", af_best_pos);


	/* Read stored PCAL and NCAL of X axis */
	is_vendor_ois_bypass_read_mode1(mcu, 0xE9, 0x04, rxbuf, 0x04);
	temp = ((u16)rxbuf[0] << 8) & 0x8000;
	temp |= ((u16)rxbuf[0] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[1] >> 7) & 0x0001;
	pre_pcal[0] = (int)temp;
	info_mcu("read reg(0x04) = 0x%04x", pre_pcal[0]);

	temp = 0x0;
	temp = ((u16)rxbuf[2] << 8) & 0x8000;
	temp |= ((u16)rxbuf[2] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[3] >> 7) & 0x0001;
	pre_ncal[0] = (int)temp;
	info_mcu("read reg(0x06) = 0x%04x", pre_ncal[0]);


	/* Read stored PCAL and NCAL for Y axis */
	memset(rxbuf, 0x0, sizeof(rxbuf));
	is_vendor_ois_bypass_read_mode1(mcu, 0x69, 0x04, rxbuf, 0x04);
	temp = ((u16)rxbuf[0] << 8) & 0x8000;
	temp |= ((u16)rxbuf[0] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[1] >> 7) & 0x0001;
	pre_pcal[1] = (int)temp;
	info_mcu("read reg(0x04) = 0x%04x", pre_pcal[1]);

	temp = 0x0;
	temp = ((u16)rxbuf[2] << 8) & 0x8000;
	temp |= ((u16)rxbuf[2] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[3] >> 7) & 0x0001;
	pre_ncal[1] = (int)temp;
	info_mcu("read reg(0x06) = 0x%04x", pre_ncal[1]);

	/* Move AF to best position which read from EEPROM */
#ifdef CONFIG_AF_HOST_CONTROL
	is_af_move_lens_pos(core, SENSOR_POSITION_REAR2, af_best_pos);
#endif
	msleep(50);

	/* Change setting  Mode for Hall cal */
	txbuf[0] = 0x3B;
	is_vendor_ois_bypass_write_mode1(mcu, 0xE8, 0xAE, txbuf, 0x01);
	is_vendor_ois_bypass_write_mode1(mcu, 0x68, 0xAE, txbuf, 0x01);
	info_mcu("write reg(0xAE) = 0x%02x", txbuf[0]);

	/* Start hall calibration for X axis */
	txbuf[0] = 0x01;
	is_vendor_ois_bypass_write_mode1(mcu, 0xE8, 0x02, txbuf, 0x01);
	msleep(150);

	/* Start hall calibration for Y axis */
	is_vendor_ois_bypass_write_mode1(mcu, 0x68, 0x02, txbuf, 0x01);
	msleep(150);

	/*Clear setting  Mode */
	txbuf[0] = 0x00;
	is_vendor_ois_bypass_write_mode1(mcu, 0xE8, 0xAE, txbuf, 0x01);
	is_vendor_ois_bypass_write_mode1(mcu, 0x68, 0xAE, txbuf, 0x01);
	info_mcu("write reg(0xAE) = 0x%02x", txbuf[0]);

	/*Read new PCAL and NCAL for X axis*/
	memset(rxbuf, 0x0, sizeof(rxbuf));
	is_vendor_ois_bypass_read_mode1(mcu, 0xE9, 0x04, rxbuf, 0x04);
	temp = ((u16)rxbuf[0] << 8) & 0x8000;
	temp |= ((u16)rxbuf[0] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[1] >> 7) & 0x0001;
	cur_pcal[0] = (int)temp;
	info_mcu("read reg(0x04) = 0x%04x", cur_pcal[0]);

	temp = 0x0;
	temp = ((u16)rxbuf[2] << 8) & 0x8000;
	temp |= ((u16)rxbuf[2] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[3] >> 7) & 0x0001;
	cur_ncal[0] = (int)temp;
	info_mcu("read reg(0x06) = 0x%04x", cur_ncal[0]);

	/*Read new PCAL and NCAL for Y axis*/
	memset(rxbuf, 0x0, sizeof(rxbuf));
	is_vendor_ois_bypass_read_mode1(mcu, 0x69, 0x04, rxbuf, 0x04);
	temp = ((u16)rxbuf[0] << 8) & 0x8000;
	temp |= ((u16)rxbuf[0] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[1] >> 7) & 0x0001;
	cur_pcal[1] = (int)temp;
	info_mcu("read reg(0x04) = 0x%04x", cur_pcal[1]);

	temp = 0x0;
	temp = ((u16)rxbuf[2] << 8) & 0x8000;
	temp |= ((u16)rxbuf[2] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[3] >> 7) & 0x0001;
	cur_ncal[1] = (int)temp;
	info_mcu("read reg(0x06) = 0x%04x", cur_ncal[1]);

	hall_cal_data[0] = pre_pcal[0];
	hall_cal_data[1] = pre_ncal[0];
	hall_cal_data[2] = pre_pcal[1];
	hall_cal_data[3] = pre_ncal[1];
	hall_cal_data[4] = cur_pcal[0];
	hall_cal_data[5] = cur_ncal[0];
	hall_cal_data[6] = cur_pcal[1];
	hall_cal_data[7] = cur_ncal[1];

	info_mcu("%s  X\n", __func__);

	return ret;
}

int is_vendor_ois_read_ext_clock(struct v4l2_subdev *subdev, u32 *clock)
{
	int ret = 0;
	u8 val = 0;
	int retries = 600;
	u8 addr_size = 0x02;
	u8 data[4] = {0, };

	struct ois_mcu_dev *mcu = NULL;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err_mcu("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	do {
		val = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_STATUS);
		usleep_range(500, 510);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	is_vendor_ois_bypass_read(mcu, MCU_BYPASS_MODE_READ_ID, 0x03F0, addr_size, data, 0x02);
	is_vendor_ois_bypass_read(mcu, MCU_BYPASS_MODE_READ_ID, 0x03F2, addr_size, &data[2], 0x02);
	*clock = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];

	info_mcu("%s  X\n", __func__);

	return ret;
}

long is_vendor_ois_open_fw(struct is_core *core)
{
	int ret = 0;
	struct is_binary mcu_bin;
	struct is_mcu *is_mcu = NULL;
	struct is_device_sensor *device = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_ois_info *ois_minfo = NULL;
	struct is_ois_info *ois_pinfo = NULL;

	info_mcu("%s started", __func__);

	mcu = core->mcu;

	device = &core->sensor[0];
	is_mcu = device->mcu;

	is_ois_get_module_version(&ois_minfo);
	is_ois_get_phone_version(&ois_pinfo);

	setup_binary_loader(&mcu_bin, 3, -EAGAIN, NULL, NULL);
	ret = request_binary(&mcu_bin, IS_MCU_PATH, IS_MCU_FW_NAME, mcu->dev);
	if (ret) {
		err_mcu("request_firmware was failed(%d)\n", ret);
		ret = 0;
		goto request_err;
	}

	memcpy(&is_mcu->vdrinfo_bin[0], mcu_bin.data + OIS_CMD_BASE + MCU_HW_VERSION_OFFSET, sizeof(is_mcu->vdrinfo_bin));
	is_mcu->hw_bin[0] = *((u8 *)mcu_bin.data + OIS_CMD_BASE + MCU_BIN_VERSION_OFFSET + 3);
	is_mcu->hw_bin[1] = *((u8 *)mcu_bin.data + OIS_CMD_BASE + MCU_BIN_VERSION_OFFSET + 2);
	is_mcu->hw_bin[2] = *((u8 *)mcu_bin.data + OIS_CMD_BASE + MCU_BIN_VERSION_OFFSET + 1);
	is_mcu->hw_bin[3] = *((u8 *)mcu_bin.data + OIS_CMD_BASE + MCU_BIN_VERSION_OFFSET);
	memcpy(ois_pinfo->header_ver, is_mcu->hw_bin, 4);
	memcpy(&ois_pinfo->header_ver[4], mcu_bin.data + OIS_CMD_BASE + MCU_HW_VERSION_OFFSET, 4);
	memcpy(ois_minfo->header_ver, ois_pinfo->header_ver, sizeof(ois_pinfo->header_ver));

	info_mcu("Request FW was done (%s%s, %ld)\n",
		IS_MCU_PATH, IS_MCU_FW_NAME, mcu_bin.size);

	ret = mcu_bin.size;

request_err:
	release_binary(&mcu_bin);

	info_mcu("%s %d end", __func__, __LINE__);

	return ret;
}

#ifdef USE_OIS_HALL_DATA_FOR_VDIS
int is_vendor_ois_get_hall_data(struct v4l2_subdev *subdev, struct is_ois_hall_data *halldata)
{
	int ret = 0;
	struct ois_mcu_dev *mcu = NULL;
	u8 val[4] = {0, };
	u64 timeStamp = 0;
	int val_sum = 0;
	int max_cnt = 192;
	int index = 0;
	int i = 0;
	int valid_cnt = 0;
	int valid_num = 0;
	u64 prev_timestampboot = timestampboot;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	/* SVDIS CTRL READ HALLDATA */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_SVDIS_CTRL, 0x02);
	usleep_range(150, 160);

	/* S/W interrupt to MCU */
	is_mcu_hw_set_field(mcu->regs[OM_REG_SFR], OIS_CM0P_IRQ, F_OIS_CM0P_IRQ_REQ, 0x01);
	usleep_range(200, 210);

	/* get current AP time stamp (read irq timing) */
	timestampboot = ktime_get_boottime_ns();

	val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TIME_STAMP1);
	val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TIME_STAMP2);
	val[2] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TIME_STAMP3);
	val[3] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TIME_STAMP4);
	timeStamp =  ((uint64_t)val[3] << 24) | ((uint64_t)val[2] << 16) | ((uint64_t)val[1] << 8) | (uint64_t)val[0];
	halldata->timeStamp = prev_timestampboot + (timeStamp * 1000);

	valid_num = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_VALID_NUMBER);
	halldata->validData = valid_num;

	valid_cnt = (int)valid_num * 8;
	if (valid_cnt > max_cnt) {
		valid_cnt = max_cnt;
	}

	/* Wide data */
	for (i = 0; i < valid_cnt; i += 8) {
		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_WIDE_X_ANG_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_WIDE_X_ANG_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->xAngleWide[index] = val_sum;

		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_WIDE_Y_ANG_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_WIDE_Y_ANG_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->yAngleWide[index] = val_sum;

		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_WIDE_X_ANGVEL_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_WIDE_X_ANGVEL_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->xAngVelWide[index] = val_sum;

		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_WIDE_Y_ANGVEL_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_WIDE_Y_ANGVEL_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->yAngVelWide[index] = val_sum;
		index++;

		if (index >= NUM_OF_HALLDATA_AT_ONCE)
			break;
	}

#if defined(CAMERA_2ND_OIS)
	/* Tele data */
	index = 0;

	for (i = 0; i < valid_cnt; i += 8) {
		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE_X_ANG_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE_X_ANG_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->xAngleTele[index] = val_sum;

		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE_Y_ANG_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE_Y_ANG_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->yAngleTele[index] = val_sum;

		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE_X_ANGVEL_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE_X_ANGVEL_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->xAngVelTele[index] = val_sum;

		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE_Y_ANGVEL_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE_Y_ANGVEL_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->yAngVelTele[index] = val_sum;
		index++;

		if (index >= NUM_OF_HALLDATA_AT_ONCE)
			break;
	}
#endif

#if defined(CAMERA_3RD_OIS)
	/* Tele2 data */
	index = 0;

	for (i = 0; i < valid_cnt; i += 8) {
		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE2_X_ANG_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE2_X_ANG_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->xAngleTele2[index] = val_sum;

		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE2_Y_ANG_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE2_Y_ANG_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->yAngleTele2[index] = val_sum;

		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE2_X_ANGVEL_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE2_X_ANGVEL_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->xAngVelTele2[index] = val_sum;

		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE2_Y_ANGVEL_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_TELE2_Y_ANGVEL_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->yAngVelTele2[index] = val_sum;
		index++;

		if (index >= NUM_OF_HALLDATA_AT_ONCE)
			break;
	}
#endif

	/* Z-axis data */
	index = 0;
	valid_cnt = valid_cnt / 4;  //= valid * 2

	for (i = 0; i < valid_cnt; i += 2) {
		val[0] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_Z_ANGVEL_0 + i);
		val[1] = is_mcu_get_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_HALL_Z_ANGVEL_0 + i + 1);
		val_sum = (val[1] << 8) | val[0];
		halldata->zAngVel[index] = val_sum;
		index++;

		if (index >= NUM_OF_HALLDATA_AT_ONCE)
			break;
	}

	/* delay between write irq & read irq */
	usleep_range(250, 260);

	/* SVDIS CTRL WRITE TIMESTAMP */
	is_mcu_set_reg_u8(mcu->regs[OM_REG_CORE], OIS_CMD_SVDIS_CTRL, 0x01);
	usleep_range(300, 310);

	/* S/W interrupt to MCU */
	is_mcu_hw_set_field(mcu->regs[OM_REG_SFR], OIS_CM0P_IRQ, F_OIS_CM0P_IRQ_REQ, 0x01);

	return ret;
}
#endif

bool is_vendor_ois_check_fw(struct is_core *core)
{
	long ret = 0;
	struct is_vendor_private *vendor_priv;

	info_mcu("%s", __func__);

	ret = is_vendor_ois_open_fw(core);
	if (ret == 0) {
		err("mcu fw open failed");
		return false;
	}

	vendor_priv = core->vendor.private_data;
	vendor_priv->ois_ver_read = true;

	return true;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = is_vendor_ois_af_init,
	.ioctl = is_vendor_ois_actuator_ioctl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static int is_vendor_ois_probe(struct platform_device *pdev)
{
	struct is_core *core;
	struct ois_mcu_dev *mcu = NULL;
	struct resource *res;
	int ret = 0;
	struct device_node *dnode;
	struct is_mcu *is_mcu = NULL;
	struct is_device_sensor *device;
	struct v4l2_subdev *subdev_mcu = NULL;
	struct v4l2_subdev *subdev_ois = NULL;
	struct is_ois *ois = NULL;
	struct is_actuator *actuator = NULL;
	struct v4l2_subdev *subdev_actuator = NULL;
	const u32 *sensor_id_spec;
	const u32 *mcu_actuator_spec;
	u32 sensor_id_len;
	u32 sensor_id[IS_SENSOR_COUNT] = {0, };
	u32 mcu_actuator_list[IS_SENSOR_COUNT] = {0, };
	int i;
	u32 mcu_actuator_len;
	struct is_vendor_private *vendor_priv;
	bool support_photo_fastae = false;
	bool skip_video_fastae = false;
	bool off_during_uwonly_mode = false;
	struct is_ois_ops *ois_ops_mcu;
	const u32 *gyro_direction_spec;
	u32 gyro_direction_len;

	core = pablo_get_core_async();
	if (!core) {
		err("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

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

	mcu_actuator_spec = of_get_property(dnode, "mcu_ctrl_actuator", &mcu_actuator_len);
	if (mcu_actuator_spec) {
		mcu_actuator_len /= (unsigned int)sizeof(*mcu_actuator_spec);
		ret = of_property_read_u32_array(dnode, "mcu_ctrl_actuator",
		        mcu_actuator_list, mcu_actuator_len);
		if (ret)
		        info_mcu("mcu_ctrl_actuator read is fail(%d)", ret);
	}

	support_photo_fastae = of_property_read_bool(dnode, "mcu_support_photo_fastae");
	if (!support_photo_fastae) {
		info_mcu("support_photo_fastae not use");
	}

	skip_video_fastae = of_property_read_bool(dnode, "mcu_skip_video_fastae");
	if (!skip_video_fastae) {
		info_mcu("skip_video_fastae not use");
	}

	off_during_uwonly_mode = of_property_read_bool(dnode, "mcu_off_during_uwonly_mode");
	if (!off_during_uwonly_mode) {
		info_mcu("off_during_uwonly_mode not use");
	}

	mcu = devm_kzalloc(&pdev->dev, sizeof(struct ois_mcu_dev), GFP_KERNEL);
	if (!mcu)
		return -ENOMEM;

	gyro_direction_spec = of_get_property(dnode, "ois_gyro_direction", &gyro_direction_len);
	if (gyro_direction_spec) {
		gyro_direction_len /= (unsigned int)sizeof(*gyro_direction_spec);
		ret = of_property_read_u32_array(dnode, "ois_gyro_direction",
				mcu->ois_gyro_direction, gyro_direction_len);
		if (ret)
			probe_err("ois_gyro_direction read is fail(%d)", ret);
	}

	is_mcu = devm_kzalloc(&pdev->dev, sizeof(struct is_mcu) * sensor_id_len, GFP_KERNEL);
	if (!mcu) {
		err("fimc_is_mcu is NULL");
		ret = -ENOMEM;
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
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_ois = devm_kzalloc(&pdev->dev, sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_ois) {
		err("subdev_ois is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	actuator = devm_kzalloc(&pdev->dev, sizeof(struct is_actuator), GFP_KERNEL);
	if (!actuator) {
		err("actuator is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_actuator = devm_kzalloc(&pdev->dev, sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_actuator) {
		err("subdev_actuator is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	mcu->dev = &pdev->dev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_CORE] = devm_ioremap(mcu->dev, res->start, resource_size(res));
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

	mcu->regs[OM_REG_PERI1] = devm_ioremap(mcu->dev, res->start, resource_size(res));
	if (!mcu->regs[OM_REG_PERI1]) {
		dev_err(&pdev->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_PERI1] = res->start;
	mcu->regs_end[OM_REG_PERI1] = res->end;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_PERI2] = devm_ioremap(mcu->dev, res->start, resource_size(res));
	if (!mcu->regs[OM_REG_PERI2]) {
		dev_err(mcu->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_PERI2] = res->start;
	mcu->regs_end[OM_REG_PERI2] = res->end;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_PERI_SETTING] = devm_ioremap(mcu->dev, res->start, resource_size(res));
	if (!mcu->regs[OM_REG_PERI_SETTING]) {
		dev_err(&pdev->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_PERI_SETTING] = res->start;
	mcu->regs_end[OM_REG_PERI_SETTING] = res->end;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_SFR] = devm_ioremap(mcu->dev, res->start, resource_size(res));
	if (!mcu->regs[OM_REG_SFR]) {
		dev_err(mcu->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_SFR] = res->start;
	mcu->regs_end[OM_REG_SFR] = res->end;

	mcu->irq = platform_get_irq(pdev, 0);
	if (mcu->irq < 0) {
		dev_err(mcu->dev, "[@] failed to get IRQ resource: %d\n",
							mcu->irq);
		ret = mcu->irq;
		goto err_get_irq;
	}
	ret = devm_request_irq(mcu->dev, mcu->irq, is_isr_is_vendor_ois,
			0,
			dev_name(mcu->dev), mcu);
	if (ret) {
		dev_err(mcu->dev, "[@] failed to request IRQ(%d): %d\n",
							mcu->irq, ret);
		goto err_req_irq;
	}

	platform_set_drvdata(pdev, mcu);
	core->mcu = mcu;
	atomic_set(&mcu->shared_rsc_count, 0);
	mutex_init(&mcu->power_mutex);

	vendor_priv = core->vendor.private_data;
	vendor_priv->ois_ver_read = false;

	is_vendor_ois_get_ops(&ois_ops_mcu);

	for (i = 0; i < sensor_id_len; i++) {
		probe_info("%s sensor_id %d\n", __func__, sensor_id[i]);

		probe_info("%s mcu_actuator_list %d\n", __func__, mcu_actuator_list[i]);

		device = &core->sensor[sensor_id[i]];

		is_mcu[i].name = MCU_NAME_INTERNAL;
		is_mcu[i].subdev = &subdev_mcu[i];
		is_mcu[i].device = sensor_id[i];
		is_mcu[i].private_data = core;
		is_mcu[i].support_photo_fastae = support_photo_fastae;
		is_mcu[i].skip_video_fastae = skip_video_fastae;
		is_mcu[i].off_during_uwonly_mode = off_during_uwonly_mode;

		ois[i].subdev = &subdev_ois[i];
		ois[i].device = sensor_id[i];
		ois[i].ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
		ois[i].pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
		ois[i].ois_shift_available = false;
		ois[i].ixc_lock = NULL;
		ois[i].ois_ops = ois_ops_mcu;

#if defined(USE_TELE_OIS_AF_COMMON_INTERFACE) || defined(USE_TELE2_OIS_AF_COMMON_INTERFACE)
		if (mcu_actuator_list[i] == 1) {
			actuator->id = ACTUATOR_NAME_AK737X;
			actuator->subdev = subdev_actuator;
			actuator->device = sensor_id[i];
			actuator->position = 0;
			actuator->need_softlanding = 0;
			actuator->max_position = MCU_ACT_POS_MAX_SIZE;
			actuator->pos_size_bit = MCU_ACT_POS_SIZE_BIT;
			actuator->pos_direction = MCU_ACT_POS_DIRECTION;

			is_mcu[i].subdev_actuator = subdev_actuator;
			is_mcu[i].actuator = actuator;

			device->subdev_actuator[sensor_id[i]] = subdev_actuator;
			device->actuator[sensor_id[i]] = actuator;

			v4l2_subdev_init(subdev_actuator, &subdev_ops);
			v4l2_set_subdevdata(subdev_actuator, actuator);
			v4l2_set_subdev_hostdata(subdev_actuator, device);
		}
#endif

		is_mcu[i].mcu_ctrl_actuator = mcu_actuator_list[i];
		is_mcu[i].subdev_ois = &subdev_ois[i];
		is_mcu[i].ois = &ois[i];

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
	devm_iounmap(mcu->dev, mcu->regs[OM_REG_PERI_SETTING]);
	devm_iounmap(mcu->dev, mcu->regs[OM_REG_SFR]);
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

	if (actuator)
		kfree(actuator);

	if (subdev_actuator)
		kfree(subdev_actuator);

	return ret;
}

static const struct dev_pm_ops is_vendor_ois_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(is_vendor_ois_suspend, is_vendor_ois_resume)
	SET_RUNTIME_PM_OPS(is_vendor_ois_runtime_suspend, is_vendor_ois_runtime_resume,
			   NULL)
};

static const struct of_device_id sensor_is_vendor_ois_match[] = {
	{
		.compatible = "samsung,sensor-ois-mcu",
	},
	{},
};

struct platform_driver sensor_ois_mcu_platform_driver = {
	.probe = is_vendor_ois_probe,
	.driver = {
		.name   = "Sensor-OIS-MCU",
		.owner  = THIS_MODULE,
		.pm	= &is_vendor_ois_pm_ops,
		.of_match_table = sensor_is_vendor_ois_match,
	}
};

#ifndef MODULE
static int __init sensor_is_vendor_ois_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_ois_mcu_platform_driver,
							is_vendor_ois_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_ois_mcu_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_is_vendor_ois_init);
#endif

MODULE_DESCRIPTION("Exynos Pablo OIS-Internal MCU driver");
MODULE_AUTHOR("Younghwan Joo <yhwan.joo@samsung.com>");
MODULE_LICENSE("GPL v2");
