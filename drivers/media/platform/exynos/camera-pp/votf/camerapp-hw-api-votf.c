/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos CAMERA-PP VOTF driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "camerapp-hw-api-votf.h"

void camerapp_hw_votf_create_ring(void __iomem *base_addr, int ip, int module)
{
	if (module == C2SERV) {
		is_hw_set_reg(base_addr, &c2serv_regs[C2SERV_R_C2COM_LOCAL_IP], (unsigned int)ip);
		is_hw_set_reg(base_addr, &c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN], 0x1);
		is_hw_set_reg(base_addr, &c2serv_regs[C2SERV_R_C2COM_RING_ENABLE], 0x1);
	} else {
		is_hw_set_reg(base_addr, &c2agent_regs[C2AGENT_R_C2COM_LOCAL_IP], (unsigned int)ip);
		is_hw_set_reg(base_addr, &c2agent_regs[C2AGENT_R_C2COM_RING_CLK_EN], 0x1);
		is_hw_set_reg(base_addr, &c2agent_regs[C2AGENT_R_C2COM_RING_ENABLE], 0x1);
	}
}

void camerapp_hw_votf_destroy_ring(void __iomem *base_addr, int ip, int module)
{
	if (module == C2SERV) {
		is_hw_set_reg(base_addr, &c2serv_regs[C2SERV_R_C2COM_LOCAL_IP], (unsigned int)ip);
		is_hw_set_reg(base_addr, &c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN], 0x0);
		is_hw_set_reg(base_addr, &c2serv_regs[C2SERV_R_C2COM_RING_ENABLE], 0x0);
	} else {
		is_hw_set_reg(base_addr, &c2agent_regs[C2AGENT_R_C2COM_LOCAL_IP], (unsigned int)ip);
		is_hw_set_reg(base_addr, &c2agent_regs[C2AGENT_R_C2COM_RING_CLK_EN], 0x0);
		is_hw_set_reg(base_addr, &c2agent_regs[C2AGENT_R_C2COM_RING_ENABLE], 0x0);
	}
}

void camerapp_hw_votf_set_sel_reg(void __iomem *base_addr, u32 set, u32 mode)
{
	is_hw_set_reg(base_addr, &c2serv_regs[C2SERV_R_SELREGISTERMODE], mode);
	is_hw_set_reg(base_addr, &c2serv_regs[C2SERV_R_SELREGISTER], set);
}

void camerapp_hw_votf_reset(void __iomem *base_addr, int module)
{
	if (module == C2SERV)
		is_hw_set_reg(base_addr, &c2serv_regs[C2SERV_R_SW_RESET], 0x1);
	else
		is_hw_set_reg(base_addr, &c2serv_regs[C2AGENT_R_SW_RESET], 0x1);
}

void camerapp_hw_votf_sw_core_reset(void __iomem *base_addr, int module)
{
	if (module == C2SERV)
		is_hw_set_reg(base_addr, &c2serv_regs[C2SERV_R_SW_CORE_RESET], 0x1);
	else
		is_hw_set_reg(base_addr, &c2serv_regs[C2AGENT_R_SW_CORE_RESET], 0x1);
}

void camerapp_hw_votf_set_flush(void __iomem *votf_addr, u32 offset)
{
	writel(0x1, votf_addr + offset);
}

void camerapp_hw_votf_set_crop_start(void __iomem *votf_addr, u32 offset, bool start)
{
	writel(start, votf_addr + offset);
}

u32 camerapp_hw_votf_get_crop_start(void __iomem *votf_addr, u32 offset)
{
	return readl(votf_addr + offset);
}

void camerapp_hw_votf_set_crop_enable(void __iomem *votf_addr, u32 offset, bool enable)
{
	writel(enable, votf_addr + offset);
}

u32 camerapp_hw_votf_get_crop_enable(void __iomem *votf_addr, u32 offset)
{
	return readl(votf_addr + offset);
}

void camerapp_hw_votf_set_recover_enable(void __iomem *votf_addr, u32 offset, u32 cfg)
{
	writel(cfg, votf_addr + offset);
}

void camerapp_hw_votf_set_enable(void __iomem *votf_addr, u32 offset, bool enable)
{
	writel(enable, votf_addr + offset);
}

u32 camerapp_hw_votf_get_enable(void __iomem *votf_addr, u32 offset)
{
	return readl(votf_addr + offset);
}

void camerapp_hw_votf_set_limit(void __iomem *votf_addr, u32 offset, u32 limit)
{
	writel(limit, votf_addr + offset);
}

void camerapp_hw_votf_set_dest(void __iomem *votf_addr, u32 offset, u32 dest)
{
	writel(dest, votf_addr + offset);
}

void camerapp_hw_votf_set_token_size(void __iomem *votf_addr, u32 offset, u32 token_size)
{
	writel(token_size, votf_addr + offset);
}

void camerapp_hw_votf_set_first_token_size(void __iomem *votf_addr, u32 offset, u32 token_size)
{
	writel(token_size, votf_addr + offset);
}

void camerapp_hw_votf_set_frame_size(void __iomem *votf_addr, u32 offset, u32 frame_size)
{
	writel(frame_size, votf_addr + offset);
}

void camerapp_hw_votf_set_start(void __iomem *votf_addr, u32 offset, u32 start)
{
	writel(start, votf_addr + offset);
}

void camerapp_hw_votf_set_finish(void __iomem *votf_addr, u32 offset, u32 finish)
{
	writel(finish, votf_addr + offset);
}

void camerapp_hw_votf_set_threshold(void __iomem *votf_addr, u32 offset, u32 value)
{
	writel(value, votf_addr + offset);
}

u32 camerapp_hw_votf_get_threshold(void __iomem *votf_addr, u32 offset)
{
	return readl(votf_addr + offset);
}

void camerapp_hw_votf_set_read_bytes(void __iomem *votf_addr, u32 offset, u32 bytes)
{
	writel(bytes, votf_addr + offset);
}

u32 camerapp_hw_votf_get_fullness(void __iomem *votf_addr, u32 offset)
{
	return readl(votf_addr + offset);
}

u32 camerapp_hw_votf_get_busy(void __iomem *votf_addr, u32 offset)
{
	return readl(votf_addr + offset);
}

void camerapp_hw_votf_set_irq_enable(void __iomem *votf_addr, u32 offset, u32 irq)
{
	writel(irq, votf_addr + offset);
}

void camerapp_hw_votf_set_irq_status(void __iomem *votf_addr, u32 offset, u32 irq)
{
	writel(irq, votf_addr + offset);
}

void camerapp_hw_votf_set_irq(void __iomem *votf_addr, u32 offset, u32 irq)
{
	writel(irq, votf_addr + offset);
}

void camerapp_hw_votf_set_irq_clear(void __iomem *votf_addr, u32 offset, u32 irq)
{
	writel(irq, votf_addr + offset);
}

bool camerapp_check_votf_ring(void __iomem *base_addr, int module)
{
	u32 ring_enable = 0x0;
	u32 clk_enable = 0x0;

	if (module == C2SERV) {
		ring_enable = is_hw_get_reg(base_addr, &c2serv_regs[C2SERV_R_C2COM_RING_CLK_EN]);
		clk_enable = is_hw_get_reg(base_addr, &c2serv_regs[C2SERV_R_C2COM_RING_ENABLE]);
	} else {
		ring_enable = is_hw_get_reg(base_addr, &c2agent_regs[C2AGENT_R_C2COM_RING_CLK_EN]);
		clk_enable = is_hw_get_reg(base_addr, &c2agent_regs[C2AGENT_R_C2COM_RING_ENABLE]);
	}

	if (ring_enable && clk_enable)
		return true;
	return false;
}
