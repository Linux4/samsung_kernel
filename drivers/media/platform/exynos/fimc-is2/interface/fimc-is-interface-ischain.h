/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_INTERFACE_ISCHAIN_H
#define FIMC_IS_INTERFACE_ISCHAIN_H
#include "../hardware/fimc-is-hw-control.h"

#define IRQ_NAME_LENGTH 	16

struct fimc_is_hardware;

enum taaisp_interrupt_map {
	INTR_3AAISP1 = 0,
	INTR_3AAISP2 = 1,
	INTR_3AAISP3 = 2,
	INTR_3AAISP_MAX
};

enum scaler_interrupt_map {
	INTR_SCALER_FRAME_END			= 0,	/* FRAME_END_INTR [0] */
	INTR_SCALER_FRAME_START			= 4,	/* FRAME_START_INTR [4] */
	INTR_SCALER_SHADOW_CONDITION		= 8,	/* ShadowCondition_INTR [8] */
	INTR_SCALER_SHADOW_TRIGGER		= 16,	/* ShadowTrigger _INTR [16] */
	INTR_SCALER_SHADOW_TRIGGER_BYHW		= 12,	/* ShadowTriggerByHW _INTR [12] */
	INTR_SCALER_CORE_END			= 20,	/* CORE _END_INTR [20] */
	INTR_SCALER_CORE_START			= 24,	/* CORE_ START_INTR [24] */
	INTR_SCALER_DMA_FIFO_FULL		= 28,	/* DMA_FIFO_FULL_INTR [28] */
	INTR_SCALER_OUTSTALL_BLOCKING		= 30,	/* OUTSTALLBLOCKING_INTR [30] */
	INTR_SCALER_STALL_BLOCKING		= 31,	/* STALLBLOCKING_INTR [31] */
	INTR_MAX_SCALER_MAP
};

enum mc_scaler_interrupt_map {
	INTR_MC_SCALER_FRAME_END		= 0,
	INTR_MC_SCALER_FRAME_START		= 1,
	INTR_MC_SCALER_WDMA_FINISH		= 3,
	INTR_MC_SCALER_CORE_FINISH		= 4,
	INTR_MC_SCALER_SHADOW_HW_TRIGGER	= 5,
	INTR_MC_SCALER_SHADOW_TRIGGER	= 6,
	INTR_MC_SCALER_INPUT_PROTOCOL_ERR	= 7,
	INTR_MC_SCALER_INPUT_HORIZONTAL_OVF	= 8,
	INTR_MC_SCALER_INPUT_HORIZONTAL_UNF	= 9,
	INTR_MC_SCALER_INPUT_VERTICAL_OVF	= 10,
	INTR_MC_SCALER_INPUT_VERTICAL_UNF	= 11,
	INTR_MC_SCALER_OUTSTALL		= 12,
	INTR_MC_SCALER_OVERFLOW		= 13
};

enum fd_interrupt_map_v111 {
	INTR_FD_END_PROCESSING			= 0,
	INTR_FD_AXI_ERROR			= 1,
	INTR_FD_BUFFER_OVERFLOW			= 2,
	INTR_FD_AXI_ENTER_IDLE			= 3,
	INTR_FD_AXI_EXIT_IDLE			= 4,
	INTR_FD_SHADOW_HWTRCOND			= 5,
	INTR_FD_SHADOW_HWTRCOND_ASSERT		= 6,
	INTR_FD_SHADOW_TRIGGER			= 7,
	INTR_FD_RDMA_CLEAN_UP			= 8,
	INTR_FD_SW_RESET_COMPLETED		= 9,
	INTR_FD_START_PROCESSING		= 10,
	INTR_FD_VSYN				= 11,
	INTR_FD_MAX_MAP
};

enum fimc_is_interface_ischain_state {
	IS_CHAIN_IF_STATE_INIT,
	IS_CHAIN_IF_STATE_OPEN,
	IS_CHAIN_IF_STATE_REGISTERED,
	IS_CHAIN_IF_STATE_MAX
};

typedef int (*isp_handler)(u32 id, void *context);
typedef int (*subip_intr_handler)(void *data);

struct isp_intr_handler {
	u32 valid;
	u32 priority;
	u32 id;
	void *ctx;
	isp_handler handler;
	u32 chain_id;
};

struct fimc_is_interface_taaisp {
	int				id;
	unsigned long			state;
	void __iomem			*regs;
	void __iomem			*regs_b;	/* for isp */
	unsigned int			irq[INTR_3AAISP_MAX];
	char				irq_name[INTR_3AAISP_MAX][IRQ_NAME_LENGTH];

	/* for SDMA and memory log write */
	u32				kvaddr;
	u32				dvaddr;
	void				*fw_cookie;
	struct vb2_alloc_ctx		*alloc_ctx;

	struct isp_intr_handler		handler[INTR_3AAISP_MAX];
};

struct fimc_is_interface_subip {
	int				id;
	unsigned long			state;
	void __iomem			*regs;
	unsigned int			irq;
	char				irq_name[IRQ_NAME_LENGTH];

	subip_intr_handler		handler;
	struct fimc_is_hw_ip		*hw_ip;
};

/**
 * struct fimc_is_interface_ischain - Sub IPs in ischain interrupt interface structure
 * @state: is chain interface state
 */
struct fimc_is_interface_ischain {
	unsigned long				state;
	struct fimc_is_interface_taaisp		taaisp[HW_SLOT_3AAISP_MAX];
	struct fimc_is_interface_subip		subip[HW_SLOT_SUBIP_MAX];
};

int fimc_is_interface_ischain_probe(struct fimc_is_interface_ischain *this,
	struct fimc_is_hardware *hardware,
	struct fimc_is_resourcemgr *resourcemgr,
	struct platform_device *pdev,
	ulong core_regs);
int fimc_is_interface_isp_probe(struct fimc_is_interface_ischain *itfc,
	int intr_slot,
	int hw_id,
	struct platform_device *pdev,
	ulong core_regs);
int fimc_is_interface_scaler_probe(struct fimc_is_interface_ischain *itfc,
	int intr_slot,
	int hw_id,
	struct platform_device *pdev,
	ulong core_regs);
int fimc_is_interface_fd_probe(struct fimc_is_interface_ischain *itfc,
	int intr_slot,
	int hw_id,
	struct platform_device *pdev,
	ulong core_regs);
#endif
