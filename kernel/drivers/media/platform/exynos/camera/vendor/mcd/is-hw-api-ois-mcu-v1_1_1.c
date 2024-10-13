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
#include "is-vendor-ois.h"
#include "pablo-binary.h"
#include "is-vendor.h"
#include "pablo-debug.h"

u32 is_mcu_get_reg_u32(enum ois_mcu_base_reg_index base, int cmd)
{
	u32 reg_value = 0;
	struct is_core *core = is_get_is_core();
	struct ois_mcu_dev *mcu = core->mcu;
	void __iomem *reg = mcu->regs[base];

	reg_value = is_hw_get_reg(reg, &ois_mcu_regs[cmd]);
	dbg_ois("[GET_REG] reg:[%s][0x%04X], reg_value(R):[0x%08X]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, reg_value);
	return reg_value;
}

void is_mcu_set_reg_u32(enum ois_mcu_base_reg_index base, int cmd, u32 val)
{
	struct is_core *core = is_get_is_core();
	struct ois_mcu_dev *mcu = core->mcu;
	void __iomem *reg = mcu->regs[base];

	is_hw_set_reg(reg, &ois_mcu_regs[cmd], val);
	dbg_ois("[SET_REG] reg:[%s][0x%04X], reg_value(W):[0x%08X]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, val);
}

u8 is_mcu_get_reg_u8(enum ois_mcu_base_reg_index base, int cmd)
{
	u8 reg_value = 0;
	struct is_core *core = is_get_is_core();
	struct ois_mcu_dev *mcu = core->mcu;
	void __iomem *reg = mcu->regs[base];

	reg_value = is_hw_get_reg_u8(reg, &ois_mcu_regs[cmd]);
	dbg_ois("[GET_REG] reg:[%s][0x%04X], reg_value(R):[0x%08X]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, reg_value);
	return reg_value;
}

void is_mcu_set_reg_u8(enum ois_mcu_base_reg_index base, int cmd, u8 val)
{
	struct is_core *core = is_get_is_core();
	struct ois_mcu_dev *mcu = core->mcu;
	void __iomem *reg = mcu->regs[base];

	is_hw_set_reg_u8(reg, &ois_mcu_regs[cmd], val);
	dbg_ois("[SET_REG] reg:[%s][0x%04X], reg_value(W):[0x%08X]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, val);
}

void is_mcu_hw_set_field(enum ois_mcu_base_reg_index base, int cmd, int field, u32 val)
{
	struct is_core *core = is_get_is_core();
	struct ois_mcu_dev *mcu = core->mcu;
	void __iomem *reg = mcu->regs[base];

	is_hw_set_field(reg, &ois_mcu_regs[cmd], &ois_mcu_fields[field], val);
	dbg_ois("[SET_FIELD] reg:[%s][0x%04X], field:[%s] val(W):[%d]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, ois_mcu_fields[field].field_name, val);
}

int __is_mcu_pmu_control(int on)
{
	int ret = 0;

	if (on)
		ret = exynos_pmu_update(ois_mcu_regs[OIS_CPU_CONFIGURATION].sfr_offset,
			pmu_ois_mcu_masks[OIS_CPU_CONFIGURATION].sfr_offset, 0x1);
	else
		ret = exynos_pmu_update(ois_mcu_regs[OIS_CPU_CONFIGURATION].sfr_offset,
			pmu_ois_mcu_masks[OIS_CPU_CONFIGURATION].sfr_offset, 0x0);

	info_mcu("%s onoff = %d", __func__, on);
	return ret;
}

int __is_mcu_core_control(enum ois_mcu_base_reg_index base, int on)
{
	u32 val;

	if (on)
		val = 0x1;
	else
		val = 0x0;

	is_mcu_hw_set_field(base, OIS_CM0P_BOOT, F_OIS_CM0P_BOOT_REQ, val);

	return 0;
}

int __is_mcu_hw_enable(enum ois_mcu_base_reg_index base)
{
	int ret = 0;

	info_mcu("%s started", __func__);

	is_mcu_hw_set_field(base, OIS_CM0P_CTRL0, F_OIS_CM0P_IPCLKREQ_ON, 1);
	usleep_range(1000, 1100); //quadra_bringup
	is_mcu_hw_set_field(base, OIS_CM0P_CTRL0, F_OIS_CM0P_IPCLKREQ_ENABLE, 1);
	usleep_range(1000, 1100); //quadra_bringup

#ifdef CONFIG_CAMERA_AAX_V55X
	is_mcu_hw_set_field(base, OIS_CM0P_REMAP_I2C_ADDR, F_OIS_CM0P_I2C_ADDR, 0x13920000);
	is_mcu_hw_set_field(base, OIS_CM0P_REMAP_SPI_ADDR, F_OIS_CM0P_SPI_ADDR, 0x13910000);
#else
	is_mcu_hw_set_field(base, OIS_CM4_REMAP_SPARE_ADDR, F_OIS_CM0P_SPDMA_ADDR, 0x119B0000);
	is_mcu_hw_set_field(base, OIS_CM4_REMAP_I3C_ADDR, F_OIS_CM0P_I2C_ADDR, 0x119C0000);
	is_mcu_hw_set_field(base, OIS_CM0P_REMAP_I2C_ADDR, F_OIS_CM0P_I2C_ADDR, 0x119D0000);
	is_mcu_hw_set_field(base, OIS_CM0P_REMAP_SPI_ADDR, F_OIS_CM0P_SPI_ADDR, 0x119E0000);
#endif

	info_mcu("%s end", __func__);

	return ret;
}

int __is_mcu_hw_set_clock_peri(enum ois_mcu_base_reg_index base)
{
	int ret = 0;

	is_mcu_set_reg_u32(base, OIS_PERI_FS1, 0x01f0ff00);
	is_mcu_set_reg_u32(base, OIS_PERI_FS2, 0x7f0003e0);
	is_mcu_set_reg_u32(base, OIS_PERI_FS3, 0x001a0000);

	return ret;
}

int __is_mcu_hw_set_init_peri(enum ois_mcu_base_reg_index base)
{
	int ret = 0;
	u32 recover_val = 0;
	u32 src = 0;

#ifdef CONFIG_CAMERA_AAX_V55X
	/* GPP1 */
	src = is_mcu_get_reg_u32(base, OIS_PERI_CON_CTRL);
	recover_val = src & 0x0000FFFF;	/* 4~7 bit mask */
	recover_val |= 0x44440000; /* USI09 SPI */
	is_mcu_set_reg_u32(base, OIS_PERI_CON_CTRL, recover_val);

	src = is_mcu_get_reg_u32(base, OIS_PERI_PUD_CTRL);
	recover_val = src & 0x0000FFFF;
	recover_val |= 0x00000000;
	is_mcu_set_reg_u32(base, OIS_PERI_PUD_CTRL, recover_val);

	/* GPP2 */
	src = is_mcu_get_reg_u32(base, OIS_PERI2_CON_CTRL);
	recover_val = src & 0xFFFFFF00;	/* 0~1 bit mask */
	recover_val |= 0x00000044; /* USI10 I2C */
	is_mcu_set_reg_u32(base, OIS_PERI2_CON_CTRL, recover_val);

	src = is_mcu_get_reg_u32(base, OIS_PERI2_PUD_CTRL);
	recover_val = src & 0xFFFFFF00;
	recover_val |= 0x00000055;
	is_mcu_set_reg_u32(base, OIS_PERI2_PUD_CTRL, recover_val);
#else
	recover_val = 0x00001122;
	is_mcu_set_reg_u32(base, OIS_PERI_CON_CTRL, recover_val);
	src = is_mcu_get_reg_u32(base, OIS_PERI_PUD_CTRL);

	recover_val = src & 0xFFFFFF00;
	is_mcu_set_reg_u32(base, OIS_PERI_PUD_CTRL, recover_val);

	recover_val = 0x00002222;
	is_mcu_set_reg_u32(base, OIS_PERI2_CON_CTRL, recover_val);

	src = is_mcu_get_reg_u32(base, OIS_PERI2_PUD_CTRL);
	recover_val = src & 0xFFFF0000;
	is_mcu_set_reg_u32(base, OIS_PERI2_PUD_CTRL, recover_val);

	/* I3C */
	recover_val = 0x00000022;
	is_mcu_set_reg_u32(base, OIS_PERI_I3C_CON_CTRL, recover_val);

	src = is_mcu_get_reg_u32(base, OIS_PERI_I3C_PUD_CTRL);
	recover_val = src & 0xFFFFFF00;
	is_mcu_set_reg_u32(base, OIS_PERI_I3C_PUD_CTRL, recover_val);

	src = is_mcu_get_reg_u32(base, OIS_PERI_I3C_DRV_CTRL);
	recover_val = src & 0xFFFFFF00;
	recover_val |= 0x00000044;
	is_mcu_set_reg_u32(base, OIS_PERI_I3C_DRV_CTRL, recover_val);
#endif

	return ret;
}

int __is_mcu_hw_set_clear_peri(enum ois_mcu_base_reg_index base)
{
	int ret = 0;
	u32 recover_val = 0;
	u32 src = 0;

#ifdef CONFIG_CAMERA_AAX_V55X
	/* GPP1 */
	src = is_mcu_get_reg_u32(base, OIS_PERI_CON_CTRL);
	recover_val = src & 0x0000FFFF;	/* 4~7 bit mask */
	is_mcu_set_reg_u32(base, OIS_PERI_CON_CTRL, recover_val);

	src = is_mcu_get_reg_u32(base, OIS_PERI_PUD_CTRL);
	recover_val = src & 0x0000FFFF;
	is_mcu_set_reg_u32(base, OIS_PERI_PUD_CTRL, recover_val);

	/* GPP2 */
	src = is_mcu_get_reg_u32(base, OIS_PERI2_CON_CTRL);
	recover_val = src & 0xFFFFFF00;	/* 0~1 bit mask */
	is_mcu_set_reg_u32(base, OIS_PERI2_CON_CTRL, recover_val);

	src = is_mcu_get_reg_u32(base, OIS_PERI2_PUD_CTRL);
	recover_val = src & 0xFFFFFF00;

	/* Set ois i2c pins PD to resolve floating voltage issues from ois i2c pins */
	recover_val |= 0x00000011;

	is_mcu_set_reg_u32(base, OIS_PERI2_PUD_CTRL, recover_val);
#else
	recover_val = 0x00000000;
	is_mcu_set_reg_u32(base, OIS_PERI_CON_CTRL, recover_val);

	src = is_mcu_get_reg_u32(base, OIS_PERI_PUD_CTRL);

	recover_val = src & 0xFFFFFF00;
	is_mcu_set_reg_u32(base, OIS_PERI_PUD_CTRL, recover_val);

	recover_val = 0x00000000;
	is_mcu_set_reg_u32(base, OIS_PERI2_CON_CTRL, recover_val);

	/* spi value ==> MISO, CS : NP, MOSI, CLK : PD */
	src = is_mcu_get_reg_u32(base, OIS_PERI2_PUD_CTRL);
	recover_val = src & 0xFFFF0000;
	recover_val |= 0x00000011;
	is_mcu_set_reg_u32(base, OIS_PERI2_PUD_CTRL, recover_val);
#endif

	return ret;
}

int __is_mcu_hw_reset_peri(enum ois_mcu_base_reg_index base, int onoff)
{
	int ret = 0;
	u8 val = 0;

	if (onoff)
		val = 0x1;
	else
		val = 0x0;

	is_mcu_set_reg_u8(base, OIS_PERI_USI_CON, val);

	return ret;
}

int __is_mcu_hw_clear_peri(enum ois_mcu_base_reg_index base)
{
	int ret = 0;

	is_mcu_set_reg_u8(base, OIS_PERI_USI_CON_CLEAR, 0x05);

	return ret;
}

int __is_mcu_hw_disable(enum ois_mcu_base_reg_index base)
{
	int ret = 0;

	info_mcu("%s started", __func__);

	is_mcu_hw_set_field(base, OIS_CM0P_CTRL0, F_OIS_CM0P_IPCLKREQ_ON, 0);
	is_mcu_hw_set_field(base, OIS_CM0P_CTRL0, F_OIS_CM0P_IPCLKREQ_ENABLE, 0);

#ifdef CONFIG_CAMERA_AAX_V55X
	is_mcu_hw_set_field(base, OIS_CM0P_REMAP_I2C_ADDR, F_OIS_CM0P_I2C_ADDR, 0x0);
	is_mcu_hw_set_field(base, OIS_CM0P_REMAP_SPI_ADDR, F_OIS_CM0P_SPI_ADDR, 0x0);
#else
	is_mcu_hw_set_field(base, OIS_CM4_REMAP_SPARE_ADDR, F_OIS_CM0P_SPDMA_ADDR, 0);
	is_mcu_hw_set_field(base, OIS_CM4_REMAP_I3C_ADDR, F_OIS_CM0P_SPDMA_ADDR, 0);
	is_mcu_hw_set_field(base, OIS_CM0P_REMAP_I2C_ADDR, F_OIS_CM0P_I2C_ADDR, 0x0);
	is_mcu_hw_set_field(base, OIS_CM0P_REMAP_SPI_ADDR, F_OIS_CM0P_SPI_ADDR, 0x0);
#endif

	usleep_range(1000, 1100);

	info_mcu("%s end", __func__);

	return ret;
}

long  __is_mcu_load_fw(void __iomem *base, struct device *dev)
{
	long ret = 0;
	struct is_binary mcu_bin;

	BUG_ON(!base);

	info_mcu("%s started", __func__);

	setup_binary_loader(&mcu_bin, 3, -EAGAIN, NULL, NULL);
	ret = request_binary(&mcu_bin, IS_MCU_PATH, IS_MCU_FW_NAME, dev);
	if (ret) {
		err_mcu("request_firmware was failed(%ld)", ret);
		ret = 0;
		goto request_err;
	}

	memcpy((void *)(base), (void *)mcu_bin.data, mcu_bin.size);

	info_mcu("Request FW was done (%s%s, %ld)\n",
		IS_MCU_PATH, IS_MCU_FW_NAME, mcu_bin.size);

	ret = mcu_bin.size;

request_err:
	release_binary(&mcu_bin);

	info_mcu("%s %d end", __func__, __LINE__);

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

unsigned int is_mcu_hw_g_irq_state(enum ois_mcu_base_reg_index base, bool clear)
{
	u32 src;

	src = is_mcu_get_reg_u32(base, OIS_CM0P_IRQ_STATUS);
	if (clear)
		is_mcu_set_reg_u32(base, OIS_CM0P_IRQ_CLEAR, src);

	return src;

}

void __is_mcu_hw_s_irq_enable(enum ois_mcu_base_reg_index base, u32 intr_enable)
{
	is_mcu_set_reg_u32(base, OIS_CM0P_IRQ_ENABLE, (intr_enable & 0xF));
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

	is_hw_dump_regs(base, &ois_mcu_regs[OIS_CPU_REG_MAX + 1], OIS_CM0P_REG_MAX - OIS_CPU_REG_MAX);

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

