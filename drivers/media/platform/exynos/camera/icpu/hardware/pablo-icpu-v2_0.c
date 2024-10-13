// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/regmap.h>

#include "pablo-icpu.h"
#include "pablo-icpu-hw.h"
#include "pablo-debug.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-HW-V2_0]",
};

struct icpu_logger *get_icpu_hw_log(void)
{
	return &_log;
}

static struct io_ops {
	void (*write)(u32 addr, u32 val);
	u32 (*read)(u32 addr);
} __custom_io_ops;

#define ICPU_RESET_TIMEOUT	10000	/* 10ms */

/* HW CONFIGS */
/* PMU */
#define ICPU_CPU_CONFIGURATION_REG_OFFSET 0x0
#define ICPU_CPU_STATUS_REG_OFFSET 0x4

/* MCUCTL_PERI */
#define ICPU_CPU_REMAPS0_NS_REG_OFFSET 0x0
#define ICPU_CPU_REMAPS0_S_REG_OFFSET 0x4
#define ICPU_CPU_REMAPD0_NS_REG_OFFSET 0x8
#define ICPU_CPU_REMAPD0_S_REG_OFFSET 0xC
#define ICPU_CPU_WFI_STATUS 0xC4

/* SYSCTRL */
#define ICPU_SYSCTRL_DBG_REG0 0x0C
#define ICPU_SYSCTRL_DBG_REG1 0x10
#define ICPU_SYSCTRL_DBG_REG2 0x14
#define ICPU_SYSCTRL_DBG_REG3 0x18




/* Enables or releases the nCORERESET
 * 0 = Enables(Hold, Assert) reset
 * 1 = Releases reset
 */
static int __set_icpu_reset(struct regmap *reg_reset,
		unsigned int bit, unsigned int on)
{
	int ret;
	unsigned int mask;
	unsigned int val;

	if (!reg_reset) {
		ICPU_ERR("No sysreg remap");
		return 0;
	}

	mask = BIT(bit);
	val = on ? BIT(bit) : 0;

	ret = regmap_update_bits(reg_reset, ICPU_CPU_CONFIGURATION_REG_OFFSET, mask, val);
	if (ret) {
		ICPU_ERR("Failed to icpu sysreg %s (reset bit=%d)", on ? "Release" : "Enable", bit);
	} else if (!IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ)) {
		ret = regmap_read_poll_timeout(reg_reset, ICPU_CPU_STATUS_REG_OFFSET,
			val, (val & BIT(bit)) >> bit == on,
			0, ICPU_RESET_TIMEOUT);
	}

	return ret;
}

static inline void __io_write(u32 addr, u32 val)
{
	void __iomem *reg;

	ICPU_INFO("addr: %x, val: %x", addr, val);

	if (__custom_io_ops.write) {
		__custom_io_ops.write(addr, val);
	} else {
		reg = ioremap(addr, 0x4);
		writel(val, reg);
		iounmap(reg);
	}
}

static inline u32 __io_read(u32 addr)
{
	void __iomem *reg;
	u32 val;

	if (__custom_io_ops.read) {
		val = __custom_io_ops.read(addr);
	} else {
		reg = ioremap(addr, 0x4);
		val = readl(reg);
		iounmap(reg);
	}
	ICPU_INFO("addr: %x, val: %x", addr, val);

	return val;
}

static int __wait_io_read(u32 addr, u32 mask, u32 val, u32 timeout_ms)
{
	const unsigned long duration = 150;
	u32 retry = timeout_ms * 1000 / duration / 2;

	while (((__io_read(addr) & mask) != val) && --retry)
		fsleep(duration);

	return retry ? 0 : -ETIMEDOUT;
}

#if (IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ))
static int __set_hw_cfg_for_veloce(void)
{
	/* ICPU: s2mpu*/
	__io_write(0x1E870054, 0xff);
	/* reg force on veloce : TPZC_PROT0SET - Secure for SysMMU secure s1/s2, Non-secure for others */
	/* __io_write(0x1E810204, 0xFFFF033F); */
	/* reg force on veloce : TPZC_PROT1SET - CA53->DRAM Non-secure, CA53->SFR Secure, C_STORE->DRAM Non-secure */
	/* __io_write(0x1E810214, 0x15F); */
	return 0;
}
#endif

static int __set_base_addr(void __iomem *base_addr, u32 dst_addr)
{

	if (!base_addr)
		return -EINVAL;

	if (dst_addr == 0)
		return -EINVAL;

	/* source
	   [31-1] : upper 31bit of source address. normally 0.
	   [0] : remap enable
	 */
	writel(1, base_addr + ICPU_CPU_REMAPS0_NS_REG_OFFSET);
	writel(1, base_addr + ICPU_CPU_REMAPS0_S_REG_OFFSET);

	writel(dst_addr, base_addr + ICPU_CPU_REMAPD0_NS_REG_OFFSET);
	writel(dst_addr, base_addr + ICPU_CPU_REMAPD0_S_REG_OFFSET);

	return 0;
}

static int __wait_wfi_state_timeout(void __iomem *base_addr, u32 timeout_ms)
{
	int ret = 0;
	u32 wait_time = 0;

	do {
		if (readl(base_addr + ICPU_CPU_WFI_STATUS) == 1) {
			ICPU_INFO("wait ICPU_CPU_WFI_STATUS for %d ms", wait_time);
			break;
		}

		usleep_range(10000, 11000);
		wait_time += 10;
	} while (wait_time <= timeout_ms);

	if (wait_time > timeout_ms) {
		ICPU_ERR("wait ICPU_CPU_WFI_STATUS timeout!!");
		ret = -ETIMEDOUT;
	}

	return ret;
}

static void __set_reg_sequence(u32 num_step, struct icpu_io_sequence *seq)
{
	int ret;
	int i;

	ICPU_INFO("E\n");

	for (i = 0; i < num_step; i++) {
		if (seq[i].type == 0) {
			__io_write(seq[i].addr, seq[i].val);
		} else if (seq[i].type == 1) {
			ret = __wait_io_read(seq[i].addr, seq[i].mask, seq[i].val, seq[i].timeout);
			ICPU_ERR_IF(ret, "step[%d] Wait io read timeout!!, read val(0x%x)", i,
					__io_read(seq[i].addr));
		} else if (seq[i].type == 0xDEAD) {
			is_debug_s2d(true, "ICPU powerdown fail! panic requested!");
		} else {
			ICPU_ERR("step[%d] unknown type(%d)", i, seq[i].type);
		}
	}

	ICPU_INFO("E\n");
}

static void __panic_handler(void __iomem *sysctrl_base)
{
	ICPU_INFO("Print ICPU Debug regs: 0x%x, 0x%x, 0x%x, 0x%x",
			ioread32(sysctrl_base + ICPU_SYSCTRL_DBG_REG0),
			ioread32(sysctrl_base + ICPU_SYSCTRL_DBG_REG1),
			ioread32(sysctrl_base + ICPU_SYSCTRL_DBG_REG2),
			ioread32(sysctrl_base + ICPU_SYSCTRL_DBG_REG3));
}

static void __set_debug_reg(void __iomem *sysctrl_base, u32 index, u32 val)
{
	ICPU_INFO("debug reg index(%d), val(0x%x)", index, val);

	switch (index) {
	case 0:
		iowrite32(val, sysctrl_base + ICPU_SYSCTRL_DBG_REG0);
		break;
	case 1:
		iowrite32(val, sysctrl_base + ICPU_SYSCTRL_DBG_REG1);
		break;
	case 2:
		iowrite32(val, sysctrl_base + ICPU_SYSCTRL_DBG_REG2);
		break;
	case 3:
		iowrite32(val, sysctrl_base + ICPU_SYSCTRL_DBG_REG3);
		break;
	default:
		ICPU_ERR("invalid debug register index(%d)", index);
		break;
	}
}

void icpu_hw_init(struct icpu_hw *hw)
{
	memset(hw, 0x0, sizeof(struct icpu_hw));

	hw->set_base_addr = __set_base_addr;
#if (IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ))
	hw->hw_misc_prepare = __set_hw_cfg_for_veloce;
#endif
	hw->reset = __set_icpu_reset;
	hw->wait_wfi_state_timeout = __wait_wfi_state_timeout;
	hw->set_reg_sequence = __set_reg_sequence;
	hw->panic_handler = __panic_handler;
	hw->set_debug_reg = __set_debug_reg;

	__custom_io_ops.read = 0;
	__custom_io_ops.write = 0;
}
KUNIT_EXPORT_SYMBOL(icpu_hw_init);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
void icpu_hw_config_io_ops(void *write, void *read)
{
	__custom_io_ops.write = write;
	__custom_io_ops.read = read;
}
KUNIT_EXPORT_SYMBOL(icpu_hw_config_io_ops);

void icpu_hw_reset_io_ops(void)
{
	__custom_io_ops.read = 0;
	__custom_io_ops.write = 0;
}
KUNIT_EXPORT_SYMBOL(icpu_hw_reset_io_ops);
#endif
