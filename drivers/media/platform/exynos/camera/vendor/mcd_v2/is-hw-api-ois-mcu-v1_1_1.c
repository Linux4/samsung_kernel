// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <soc/samsung/exynos-pmu-if.h>

#include "is-sfr-ois-mcu-v1_1_1.h"
#include "is-hw-api-ois-mcu.h"
#include "pablo-binary.h"
#include "pablo-debug.h"

u32 is_mcu_get_reg(void __iomem *base, int cmd)
{
	u32 reg_value = 0;
	reg_value = is_hw_get_reg(base, &ois_mcu_regs[cmd]);
	dbg_ois("[GET_REG] reg:[%s][0x%04X], reg_value(R):[0x%08X]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, reg_value);
	return reg_value;
}

void is_mcu_set_reg(void __iomem *base, int cmd, u32 val)
{
	is_hw_set_reg(base, &ois_mcu_regs[cmd], val);
	dbg_ois("[SET_REG] reg:[%s][0x%04X], reg_value(W):[0x%08X]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, val);
}

u8 is_mcu_get_reg_u8(void __iomem *base, int cmd)
{
	u8 reg_value = 0;
	reg_value = is_hw_get_reg_u8(base, &ois_mcu_regs[cmd]);
	dbg_ois("[GET_REG] reg:[%s][0x%04X], reg_value(R):[0x%08X]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, reg_value);
	return reg_value;
}

void is_mcu_set_reg_u8(void __iomem *base, int cmd, u8 val)
{
	is_hw_set_reg_u8(base, &ois_mcu_regs[cmd], val);
	dbg_ois("[SET_REG] reg:[%s][0x%04X], reg_value(W):[0x%08X]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, val);
}

void is_mcu_hw_set_field(void __iomem *base, int cmd, int field, u32 val)
{
	is_hw_set_field(base, &ois_mcu_regs[cmd], &ois_mcu_fields[field], val);
	if (is_get_debug_param(IS_DEBUG_PARAM_SENSOR) == 7) {
		u32 reg_value;
		/* previous value reading */
		reg_value = readl(base + (ois_mcu_regs[cmd].sfr_offset));
		reg_value = is_hw_set_field_value(reg_value, &ois_mcu_fields[field], val);
		dbg_ois("[SET_FIELD] reg:[%s][0x%04X], field:[%s] reg_value(W):[0x%08X] val(W):[%d]\n",
			ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, ois_mcu_fields[field].field_name, reg_value, val);
	}
}

int __is_mcu_mcu_state(void __iomem *base, int *state)
{
	int ret = 0;
	int state_power = 0, state_qch = 0;

	ret = exynos_pmu_read(ois_mcu_regs[R_OIS_CPU_STATES].sfr_offset, &state_power);
	if (ret)
		err_mcu("%s Failed to read pmu (%d)", __func__, ret);

	state_power &= 0xFF;

	state_qch = is_mcu_get_reg(base, R_OIS_CSIS_QCH);
	state_qch &= 0x04;

	if ((state_power == PMU_POWER_STATE_POWER_UP) && state_qch)
		*state = true;
	else
		*state = false;

	dbg_ois("%s state_power (%X), state_qch (%X), state (%d)", __func__, state_power, state_qch, *state);
	return ret;
}

int __is_mcu_pmu_control(int on)
{
	int ret = 0;
	int val = 0;
	int retries = 15;
	if (on)
		ret = exynos_pmu_update(ois_mcu_regs[R_OIS_CPU_CONFIGURATION].sfr_offset,
			pmu_ois_mcu_masks[R_OIS_CPU_CONFIGURATION].sfr_offset, 0x1);
	else {
		ret = exynos_pmu_update(ois_mcu_regs[R_OIS_CPU_CONFIGURATION].sfr_offset,
			pmu_ois_mcu_masks[R_OIS_CPU_CONFIGURATION].sfr_offset, 0x0);
		
		while (retries-- > 0) {
			ret = exynos_pmu_read(ois_mcu_regs[R_OIS_CPU_STATES].sfr_offset, &val);
			if (ret < 0)
				err_mcu("Failed to read pmu (%d)", ret);
		
			if ((val & 0xFF) == PMU_POWER_STATE_POWER_DOWN)
				break;
		
			usleep_range(200, 210);
		}
	}
	if (ret)
		err_mcu("Failed to write pmu (%d)", ret);

	info_mcu("%s onoff = %d", __func__, on);
	return ret;
}

int __is_mcu_core_control(void __iomem *base, int on)
{
	u32 val;

	if (on)
		val = 0x1;
	else
		val = 0x0;

	is_mcu_hw_set_field(base, R_OIS_CM0P_BOOT, OIS_F_CM0P_BOOT_REQ, val);

	return 0;
}

int __is_mcu_qch_control(void __iomem *base, int on)
{
	u32 val;

	info_mcu("%s started", __func__);
	if (on)
		val = 0x1;
	else
		val = 0x0;

	is_mcu_hw_set_field(base, R_OIS_CSIS_QCH, OIS_F_CLK_REQ, 1);
	is_mcu_hw_set_field(base, R_OIS_CSIS_QCH, OIS_F_IGNORE_FORCE_PM_EN, on);

	return 0;
}

int __is_mcu_hw_enable(void __iomem *base)
{
	int ret = 0;

	info_mcu("%s started", __func__);

	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_IPCLKREQ_ON, 1);
	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_IPCLKREQ_ENABLE, 1);
	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_SLEEP_CTRL, 0);
	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_QACTIVE_DIRECT_CTRL, 1);
	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_FORCE_DBG_PWRUP, 1);
	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_DISABLE_IRQ, 0);

	is_mcu_hw_set_field(base, R_OIS_CM0P_REMAP_I2C_ADDR, OIS_F_CM0P_I2C_ADDR, 0x13900000);
	is_mcu_hw_set_field(base, R_OIS_CM0P_REMAP_SPI_ADDR, OIS_F_CM0P_SPI_ADDR, 0x13910000);

	info_mcu("%s end", __func__);

	return ret;
}

int __is_mcu_hw_set_clock_peri(void __iomem *base)
{
	int ret = 0;

	is_mcu_set_reg(base, R_OIS_PERI_FS1, 0x01f0ff00);
	is_mcu_set_reg(base, R_OIS_PERI_FS2, 0x7f0003e0);
	is_mcu_set_reg(base, R_OIS_PERI_FS3, 0x001a0000);

	return ret;
}

int __is_mcu_hw_set_init_peri(void __iomem *base)
{
	int ret = 0;
	u32 recover_val = 0;
	u32 src = 0;

	/* GPP1 (4~5 bits are for OIS I2C) */
	src = is_mcu_get_reg(base, R_OIS_PERI_CON_CTRL);
	recover_val = src & 0xFF00FFFF;
	recover_val = src | 0x00440000;
	is_mcu_set_reg(base, R_OIS_PERI_CON_CTRL, recover_val);

	src = is_mcu_get_reg(base, R_OIS_PERI_PUD_CTRL);
	recover_val = src & 0xFF00FFFF;
	recover_val = src | 0x00550000;
	is_mcu_set_reg(base, R_OIS_PERI_PUD_CTRL, recover_val);

	/* GPP2 (0~3 bits are for OIS SPI) */
	src = is_mcu_get_reg(base, R_OIS_PERI2_CON_CTRL);
	recover_val = src & 0xFFFF0000;
	recover_val = src | 0x00004444;
	is_mcu_set_reg(base, R_OIS_PERI2_CON_CTRL, recover_val);

	src = is_mcu_get_reg(base, R_OIS_PERI2_PUD_CTRL);
	recover_val = src & 0xFFFF0000;
	recover_val = src | 0x00000000;
	is_mcu_set_reg(base, R_OIS_PERI2_PUD_CTRL, recover_val);

	return ret;
}

int __is_mcu_hw_set_clear_peri(void __iomem *base)
{
	int ret = 0;
	u32 recover_val = 0;
	u32 src = 0;

	src = is_mcu_get_reg(base, R_OIS_PERI_CON_CTRL);
	recover_val = src & 0xFF00FFFF;
	is_mcu_set_reg(base, R_OIS_PERI_CON_CTRL, recover_val);

	src = is_mcu_get_reg(base, R_OIS_PERI_PUD_CTRL);
	recover_val = src & 0xFF00FFFF;
	is_mcu_set_reg(base, R_OIS_PERI_PUD_CTRL, recover_val);

	src = is_mcu_get_reg(base, R_OIS_PERI2_CON_CTRL);
	recover_val = src & 0xFFFF0000;
	is_mcu_set_reg(base, R_OIS_PERI2_CON_CTRL, recover_val);

	src = is_mcu_get_reg(base, R_OIS_PERI2_PUD_CTRL);
	recover_val = src & 0xFFFF0000;
#ifdef SET_OIS_SPI_NONE_AFTER_POWER_OFF
	recover_val = src | 0x00000000;
#endif
	is_mcu_set_reg(base, R_OIS_PERI2_PUD_CTRL, recover_val);

#ifdef SET_OIS_SPI_NONE_AFTER_POWER_OFF
	src = is_mcu_get_reg(base, R_OIS_PERI2_PUDPDN_CTRL);
	recover_val = src & 0xFFFF0000;
	recover_val = src | 0x00000000;
	is_mcu_set_reg(base, R_OIS_PERI2_PUDPDN_CTRL, recover_val);
#endif

	return ret;
}

int __is_mcu_hw_reset_peri(void __iomem *base, int onoff)
{
	int ret = 0;
	u8 val = 0;

	if (onoff)
		val = 0x1;
	else
		val = 0x0;

	is_mcu_set_reg_u8(base, R_OIS_PERI_USI_CON, val);

	return ret;
}

int __is_mcu_hw_clear_peri(void __iomem *base)
{
	int ret = 0;

	is_mcu_set_reg_u8(base, R_OIS_PERI_USI_CON_CLEAR, 0x05);

	return ret;
}

int __is_mcu_hw_disable(void __iomem *base)
{
	int ret = 0;

	info_mcu("%s started", __func__);

	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_DISABLE_IRQ, 1);
	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_IPCLKREQ_ON, 0);
	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_IPCLKREQ_ENABLE, 1);
	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_SLEEP_CTRL, 0);
	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_QACTIVE_DIRECT_CTRL, 1);
	is_mcu_hw_set_field(base, R_OIS_CM0P_CTRL0, OIS_F_CM0P_FORCE_DBG_PWRUP, 0);

	is_mcu_hw_set_field(base, R_OIS_CM0P_REMAP_I2C_ADDR, OIS_F_CM0P_I2C_ADDR, 0x0);
	is_mcu_hw_set_field(base, R_OIS_CM0P_REMAP_SPI_ADDR, OIS_F_CM0P_SPI_ADDR, 0x0);

	usleep_range(1000, 1100);

	info_mcu("%s end", __func__);

	return ret;
}

long  __is_mcu_load_fw(void __iomem *base, struct device *dev)
{
	long ret = 0;
	struct is_binary mcu_bin;
	u8 ver[9];
	BUG_ON(!base);

	info_mcu("%s started", __func__);

	setup_binary_loader(&mcu_bin, 3, -EAGAIN, NULL, NULL);
	ret = request_binary(&mcu_bin, IS_MCU_PATH, IS_MCU_FW_NAME, dev);
	if (ret) {
		err_mcu("request_firmware was failed(%ld)\n", ret);
		ret = 0;
		goto request_err;
	}

	memcpy((void *)(base), (void *)mcu_bin.data, mcu_bin.size);
	info_mcu("Request FW was done (%s%s, %ld)\n",
		IS_MCU_PATH, IS_MCU_FW_NAME, mcu_bin.size);

	ret = mcu_bin.size;

	/* Module hw version */
	ver[0] = *((u8 *)mcu_bin.data + OIS_CMD_BASE + MCU_BIN_VERSION_OFFSET + 3);
	ver[1] = *((u8 *)mcu_bin.data + OIS_CMD_BASE + MCU_BIN_VERSION_OFFSET + 2);
	ver[2] = *((u8 *)mcu_bin.data + OIS_CMD_BASE + MCU_BIN_VERSION_OFFSET + 1);
	ver[3] = *((u8 *)mcu_bin.data + OIS_CMD_BASE + MCU_BIN_VERSION_OFFSET);

	/* Vdrinfo version */
	memcpy(&ver[4], mcu_bin.data + OIS_CMD_BASE + MCU_HW_VERSION_OFFSET, 4);

request_err:
	release_binary(&mcu_bin);

	info_mcu("%s end (Firmware version = %s)", __func__, ver);

	return ret;
}

unsigned int __is_mcu_get_sram_size(void)
{
	return 0xDFFF; /* fixed for v1.1.x */
}

unsigned int is_mcu_hw_g_irq_type(unsigned int state, enum mcu_event_type type)
{
	return state & (1 << type);
}

unsigned int is_mcu_hw_g_irq_state(void __iomem *base, bool clear)
{
	u32 src;

	src = is_mcu_get_reg(base, R_OIS_CM0P_IRQ_STATUS);
	if (clear)
		is_mcu_set_reg(base, R_OIS_CM0P_IRQ_CLEAR, src);

	return src;

}

void __is_mcu_hw_s_irq_enable(void __iomem *base, u32 intr_enable)
{
	is_mcu_set_reg(base, R_OIS_CM0P_IRQ_ENABLE, (intr_enable & 0xF));
}

int __is_mcu_hw_sram_dump(void __iomem *base, unsigned int range)
{
	unsigned int i;
	u8 reg_value = 0;
	char *sram_info;

	sram_info = __getname();
	if (unlikely(!sram_info))
		return -ENOMEM;

	info_mcu("SRAM DUMP ++++ start (v1.1.0)\n");
	info_mcu("Kernel virtual for sram: %08lx\n", (ulong)base);
	snprintf(sram_info, PATH_MAX, "%04X:", 0);
	for (i = 0; i <= range; i++) {
		reg_value = *(u8 *)(base + i);
		if ((i > 0) && !(i % 0x10)) {
			info_mcu("%s\n", sram_info);
			snprintf(sram_info, PATH_MAX,
				"%04x: %02X", i, reg_value);
		} else {
			snprintf(sram_info + strlen(sram_info),
				PATH_MAX, " %02X", reg_value);
		}
	}
	info_mcu("%s\n", sram_info);

	__putname(sram_info);
	info_mcu("Kernel virtual for sram: %08lx\n",
		(ulong)(base + i - 1));
	info_mcu("SRAM DUMP --- end (v1.1.0)\n");

	return 0;
}

int __is_mcu_hw_sfr_dump(void __iomem *base, unsigned int range)
{
	unsigned int i;
	u8 reg_value = 0;
	char *sram_info;

	sram_info = __getname();
	if (unlikely(!sram_info))
		return -ENOMEM;

	info_mcu("SFR DUMP ++++ start (v1.1.0)\n");
	info_mcu("Kernel virtual for : %08lx\n", (ulong)base);
	snprintf(sram_info, PATH_MAX, "%04X:", 0);
	for (i = 0; i <= range; i++) {
		reg_value = *(u8 *)(base + i);
		if ((i > 0) && !(i%0x10)) {
			info_mcu("%s\n", sram_info);
			snprintf(sram_info, PATH_MAX,
				"%04x: %02X", i, reg_value);
		} else {
			snprintf(sram_info + strlen(sram_info),
				PATH_MAX, " %02X", reg_value);
		}
	}
	info_mcu("%s\n", sram_info);

	__putname(sram_info);
	info_mcu("Kernel virtual for : %08lx\n",
		(ulong)(base + i - 1));
	info_mcu("SFR DUMP --- end (v1.1.0)\n");

	return 0;
}

int __is_mcu_hw_cr_dump(void __iomem *base)
{
	info_mcu("SFR DUMP ++++ start (v1.1.0)\n");

	is_hw_dump_regs(base, &ois_mcu_regs[R_OIS_PMU_REG_MAX + 1], R_OIS_CM0P_REG_MAX - R_OIS_PMU_REG_MAX);

	info_mcu("SFR DUMP --- end (v1.1.0)\n");

	return 0;
}

int __is_mcu_hw_peri1_dump(void __iomem *base)
{
	info_mcu("PERI1 SFR DUMP ++++ start (v1.1.0)\n");

	__is_mcu_hw_sfr_dump(base, 0xDFFF);

	info_mcu("PERI1 SFR DUMP --- end (v1.1.0)\n");
	return 0;
}

int __is_mcu_hw_peri2_dump(void __iomem *base)
{
	info_mcu("PERI2 SFR DUMP ++++ start (v1.1.0)\n");

	__is_mcu_hw_sfr_dump(base, 0xDFFF);

	info_mcu("PERI2 SFR DUMP --- end (v1.1.0)\n");
	return 0;
}
