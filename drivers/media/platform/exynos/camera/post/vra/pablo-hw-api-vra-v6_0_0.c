/*
 * Samsung EXYNOS CAMERA PostProcessing VRA driver
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-hw-api-vra.h"
#include "pablo-hw-reg-vra-v6_0_0.h"

static const struct vra_variant vra_variant[] = {
	{
		.limit_input = {
			.min_w		= 320,
			.min_h		= 240,
			.max_w		= 640,
			.max_h		= 480,
		},
		.version		= IP_VERSION,
	},
};

const struct vra_variant *camerapp_hw_vra_get_size_constraints(void __iomem *base_addr)
{
	const struct vra_variant *constraints;
	/* u32 version = camerapp_hw_vra_get_ver(base_addr); */

	vra_dbg("[VRA]\n");
	constraints = vra_variant;

	return constraints;
}

u32 camerapp_hw_vra_get_ver(void __iomem *base_addr)
{
	vra_dbg("[VRA]\n");
	/* No reg for get version */
	return IP_VERSION;
}

void camerapp_hw_vra_start(void __iomem *base_addr)
{
	vra_dbg("[VRA]\n");
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_GO], 1);
}

void camerapp_hw_vra_stop(void __iomem *base_addr)
{
	u32 reset_count = 0;

	vra_dbg("[VRA]\n");
	/* Disable interrupts */
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_IRQ_CFG], 0);
	/* Disable IP */
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_IP_CFG], 0);
	/* Wait for inactive status */
	do {
		reset_count++;
		if (reset_count > VRA_SW_RESET_COUNT) {
			vra_info("[VRA][err] fail to wait for inactive status (%d). return!\n",
				is_hw_get_field(base_addr, &vra_regs[VRA_R_IP_CFG], &vra_fields[VRA_F_FW_BUSY]));
			return;
		}
	} while (is_hw_get_field(base_addr, &vra_regs[VRA_R_IP_CFG], &vra_fields[VRA_F_FW_BUSY]) != 0);

	/* Clean interrupts */
	camerapp_hw_vra_get_intr_status_and_clear(base_addr);
}

void camerapp_hw_vra_interrupt_disable(void __iomem *base_addr)
{
	vra_dbg("[VRA]\n");
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_IRQ_CFG], 0);
}

void camerapp_hw_vra_sw_init(void __iomem *base_addr) {
#ifdef USE_VELOCE
	/* SYSMMU_D1_M2M_S2 */
	void __iomem *reg;
	vra_dbg("[VRA] Disable SYSMMU_D1_M2M_S2\n");
	reg = ioremap(0x12DF0000, 0x4);
	writel(0x0, reg);
	iounmap(reg);
#endif

	vra_dbg("[VRA] ioremaped base_addr:[0x%08X]\n", base_addr);

	/* SW reset */
	camerapp_hw_vra_sw_reset(base_addr);

	/* Enable IP */
	is_hw_set_field(base_addr, &vra_regs[VRA_R_IP_CFG], &vra_fields[VRA_F_IP_ENABLE], 1);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_PREFETCH_CFG], PREFETCH_CFG);

	/* Swapped */
	is_hw_set_field(base_addr, &vra_regs[VRA_R_AXIM_SWAP_CFG], &vra_fields[VRA_F_WR_DATA_SWAP], 0);
	is_hw_set_field(base_addr, &vra_regs[VRA_R_AXIM_SWAP_CFG], &vra_fields[VRA_F_RD_DATA_SWAP], 0);
	/* Outstanding */
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_AXIM_OUTSTANDING_CFG], 0x0);

	/* Set interrupt type and enable interruptss */
	is_hw_set_field(base_addr, &vra_regs[VRA_R_IRQ_CFG], &vra_fields[VRA_F_LEVEL_PULSE_N_SEL], INTERRUPT_TYPE);
	is_hw_set_field(base_addr, &vra_regs[VRA_R_IRQ_CFG], &vra_fields[VRA_F_IRQ], VRA_INT_EN);

#ifdef USE_VELOCE
	vra_dbg("[VRA] set tzinfo\n");
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_TZINFO_SQID0], VRA_TZINFO);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_TZINFO_SQID1], VRA_TZINFO);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_TZINFO_SQID2], VRA_TZINFO);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_TZINFO_SQID3], VRA_TZINFO);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_TZINFO_SQID4], VRA_TZINFO);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_TZINFO_SQID5], VRA_TZINFO);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_TZINFO_SQID6], VRA_TZINFO);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_TZINFO_SQID7], VRA_TZINFO);
#endif
}

u32 camerapp_hw_vra_sw_reset(void __iomem *base_addr)
{
	vra_dbg("[VRA]\n");
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_SW_RESET_BIT], 1);
#ifdef USE_VELOCE
	camerapp_vra_sfr_dump(base_addr);
#endif
	return 0;
}

u32 camerapp_hw_vra_get_intr_status_and_clear(void __iomem *base_addr)
{
	u32 int_status = 0;
	vra_dbg("[VRA]\n");
	int_status = is_hw_get_reg(base_addr, &vra_regs[VRA_R_IRQ_MASKED]);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_IRQ], int_status);
	vra_dbg("int(0x%x)\n", int_status);
	return int_status;
}

u32 camerapp_hw_vra_get_int_frame_end(void)
{
	return VRA_INT_FRAME_END;
}

u32 camerapp_hw_vra_get_int_err(void)
{
	return VRA_INT_ERR;
}

void camerapp_hw_vra_update_param(void __iomem *base_addr, struct vra_dev *vra)
{
	struct vra_frame *d_frame, *s_frame;

	vra_dbg("[VRA]\n");
	s_frame = &vra->current_ctx->s_frame;
	d_frame = &vra->current_ctx->d_frame;
	/* Set base DMA addresses */
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_INPUT_BASE_ADDR], s_frame->addr.y);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_OUTPUT_BASE_ADDR], d_frame->addr.y);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_TEMPORARY_BASE_ADDR], vra->current_ctx->t_addr);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_CONSTANT_BASE_ADDR], vra->current_ctx->c_addr);
	is_hw_set_reg(base_addr, &vra_regs[VRA_R_INSTRUCTION_BASE_ADDR], vra->current_ctx->i_addr);
#ifdef USE_VELOCE
	camerapp_vra_sfr_dump(base_addr);
#endif
}

void camerapp_vra_sfr_dump(void __iomem *base_addr)
{
	u32 i;
	u32 reg_value;

	vra_dbg("[VRA] v6.0\n");
	for (i = 0; i < VRA_REG_CNT; i++) {
		reg_value = readl(base_addr + vra_regs[i].sfr_offset);
		pr_info("[@][SFR][DUMP] reg:[%s][0x%04X], value:[0x%08X]\n",
			vra_regs[i].reg_name, vra_regs[i].sfr_offset, reg_value);
	}
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct camerapp_kunit_hw_vra_func camerapp_kunit_hw_vra = {
	.camerapp_hw_vra_get_ver = camerapp_hw_vra_get_ver,
	.camerapp_vra_sfr_dump = camerapp_vra_sfr_dump,
	.camerapp_hw_vra_get_size_constraints = camerapp_hw_vra_get_size_constraints,
	.camerapp_hw_vra_start = camerapp_hw_vra_start,
	.camerapp_hw_vra_stop = camerapp_hw_vra_stop,
	.camerapp_hw_vra_sw_init = camerapp_hw_vra_sw_init,
	.camerapp_hw_vra_sw_reset = camerapp_hw_vra_sw_reset,
	.camerapp_hw_vra_get_intr_status_and_clear = camerapp_hw_vra_get_intr_status_and_clear,
	.camerapp_hw_vra_get_int_frame_end = camerapp_hw_vra_get_int_frame_end,
	.camerapp_hw_vra_update_param = camerapp_hw_vra_update_param,
	.camerapp_hw_vra_interrupt_disable = camerapp_hw_vra_interrupt_disable,
};

struct camerapp_kunit_hw_vra_func *camerapp_kunit_get_hw_vra_test(void) {
	return &camerapp_kunit_hw_vra;
}
KUNIT_EXPORT_SYMBOL(camerapp_kunit_get_hw_vra_test);
#endif
