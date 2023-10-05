/* SPDX-License-Identifier: GPL */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_ICPU_HW_H
#define PABLO_ICPU_HW_H

struct icpu_mbox_tx_info {
	void __iomem *int_enable_reg;
	void __iomem *int_gen_reg;
	void __iomem *int_status_reg;
	void __iomem *data_reg;
	u32 data_max_len;
};

struct icpu_mbox_rx_info {
	void __iomem *int_status_reg;
	void __iomem *data_reg;
	u32 data_max_len;
	int irq;
};

/* only u32 member expected */
#define ICPU_IO_SEQUENCE_LEN (5)
struct icpu_io_sequence {
	u32 type; /* 0: write, 1: read */
	u32 addr;
	u32 mask;
	u32 val;
	u32 timeout; /* ms, 0: no timeout */
};

struct icpu_platform_data {
	void __iomem *mcuctl_reg_base;
	void __iomem *sysctrl_reg_base;
	void __iomem *cstore_reg_base;
	struct regmap *sysreg_reset;
	u32 sysreg_reset_bit;

	struct clk *clk;

	u32 num_chans;
	u32 num_tx_mbox;
	struct icpu_mbox_tx_info *tx_infos;
	u32 num_rx_mbox;
	struct icpu_mbox_rx_info *rx_infos;

	u32 num_force_powerdown_step;
	struct icpu_io_sequence *force_powerdown_seq;
};

struct icpu_hw {
	int (*set_base_addr)(void __iomem *, u32);
	int (*hw_misc_prepare)(void);
	int (*reset)(struct regmap *reg_reset,
			unsigned int bit, unsigned int on);
	int (*wait_wfi_state_timeout)(void __iomem *, u32);
	void (*force_powerdown)(u32, struct icpu_io_sequence *);
	void (*panic_handler)(void __iomem *);
	void (*set_debug_reg)(void __iomem *, u32, u32);
};

#define HW_OPS(f, ... ) \
	hw.f ? hw.f(__VA_ARGS__) : 0

void icpu_hw_init(struct icpu_hw *hw);

#endif
