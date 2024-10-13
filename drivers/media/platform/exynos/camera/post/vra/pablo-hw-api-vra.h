/*
 * Samsung EXYNOS CAMERA PostProcessing VRA driver
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef CAMERAPP_HW_API_VRA_H
#define CAMERAPP_HW_API_VRA_H

#include "pablo-hw-api-common.h"
#include "pablo-vra.h"

void camerapp_hw_vra_start(void __iomem *base_addr);
void camerapp_hw_vra_stop(void __iomem *base_addr);
void camerapp_hw_vra_sw_init(void __iomem *base_addr);
u32 camerapp_hw_vra_sw_reset(void __iomem *base_addr);
void camerapp_hw_vra_update_param(void __iomem *base_addr, struct vra_dev *vra);
void camerapp_hw_vra_status_read(void __iomem *base_addr);
void camerapp_vra_sfr_dump(void __iomem *base_addr);
void camerapp_hw_vra_interrupt_disable(void __iomem *base_addr);
u32 camerapp_hw_vra_get_intr_status_and_clear(void __iomem *base_addr);
u32 camerapp_hw_vra_get_int_frame_end(void);
u32 camerapp_hw_vra_get_int_err(void);
u32 camerapp_hw_vra_get_ver(void __iomem *base_addr);
const struct vra_variant *camerapp_hw_vra_get_size_constraints(void __iomem *base_addr);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct camerapp_kunit_hw_vra_func {
	u32 (*camerapp_hw_vra_get_ver)(void __iomem *reg);
	void (*camerapp_vra_sfr_dump)(void __iomem *reg);
	const struct vra_variant *(*camerapp_hw_vra_get_size_constraints)(void __iomem *reg);
	void (*camerapp_hw_vra_start)(void __iomem *base_addr);
	void (*camerapp_hw_vra_stop)(void __iomem *base_addr);
	void (*camerapp_hw_vra_sw_init)(void __iomem *base_addr);
	u32 (*camerapp_hw_vra_sw_reset)(void __iomem *base_addr);
	u32 (*camerapp_hw_vra_get_intr_status_and_clear)(void __iomem *base_addr);
	u32 (*camerapp_hw_vra_get_int_frame_end)(void);
	void (*camerapp_hw_vra_update_param)(void __iomem *base_addr, struct vra_dev *vra);
	void (*camerapp_hw_vra_interrupt_disable)(void __iomem *reg);
};
struct camerapp_kunit_hw_vra_func *camerapp_kunit_get_hw_vra_test(void);
#endif
#endif
