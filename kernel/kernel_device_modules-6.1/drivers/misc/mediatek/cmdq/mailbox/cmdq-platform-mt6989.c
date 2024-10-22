// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <dt-bindings/gce/mt6989-gce.h>

#include "cmdq-util.h"
#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
#include <mtk-smmu-v3.h>
#endif

#define GCE_D_PA	0x1e980000
#define GCE_M_PA	0x1e990000
#define MMSID_SRT_NORMAL	31

#define MDP_THRD_MIN	20

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	if (gce_pa == GCE_D_PA) {
		switch (thread) {
		case 0 ... 9:
		case 22 ... 31:
			return "MM_DISP";
		case 16 ... 19:
			return "MM_MML";
		case 20 ... 21:
			return "MM_MDP";
		default:
			return "MM_GCE";
		}
	} else if (gce_pa == GCE_M_PA) {
		switch (thread) {
		case 0 ... 5:
		case 12:
		case 16 ... 19:
		case 22 ... 23:
		case 28 ... 29:
			return "MM_IMGSYS";
		case 10:
			return "MM_IMG_DIP";
		case 11:
		case 20:
			return "MM_IMG_FDVT";
		case 21:
			return "MM_CAM_DPE";
		case 26 ... 27:
		case 30 ... 31:
			return "MM_IMG_QOF";
		case 24:
			return "MM_CAM";
		default:
			return "MM_GCEM";
		}
	}

	return "MM_GCE";
}

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread)
{
	switch (event) {
	case CMDQ_EVENT_GPR_TIMER ... CMDQ_EVENT_GPR_TIMER + 32:
		return cmdq_thread_module_dispatch(gce_pa, thread);
	}

	if (gce_pa == GCE_D_PA) // GCE-D
		switch (event) {
		case CMDQ_EVENT_MMLSYS0_MDP_FG0_SOF
			... CMDQ_EVENT_MMLSYS0_MEM_ISOINTB_ENG_EVENT:
			return "MM_MDP";
		case CMDQ_EVENT_MMLSYS1_MDP_FG0_SOF
			... CMDQ_EVENT_MMLSYS1_MEM_ISOINTB_ENG_EVENT:
			return "MM_MML";
		case CMDQ_EVENT_OVL1_DISP_OVL0_2L_SOF
			... CMDQ_EVENT_DSI2_TE_I_DSI2_TE_I:
			return "MM_DISP";
		default:
			return "MM_GCE";
		}

	if (gce_pa == GCE_M_PA) // GCE-M
		switch (event) {
		case CMDQ_EVENT_VDEC1_VDEC1_EVENT_0
			... CMDQ_EVENT_VDEC1_VDEC1_EVENT_127:
		case CMDQ_EVENT_VDEC2_VDEC2_EVENT_0
			... CMDQ_EVENT_VDEC2_VDEC2_EVENT_31:
			return "MM_VDEC";
		case CMDQ_EVENT_VENC3_VENC_RESERVED
			... CMDQ_EVENT_VENC1_VENC_SOC_FRAME_DONE:
			return "MM_VENC";
		case CMDQ_EVENT_IMG_IMG_EVENT_0:
			return "MM_IMGSYS";
		case CMDQ_EVENT_IMG_TRAW0_CQ_THR_DONE_TRAW0_0
			... CMDQ_EVENT_IMG_TRAW0_DIP_RESERVED:
			return "MM_IMG_TRAW";
		case CMDQ_EVENT_IMG_TRAW1_CQ_THR_DONE_TRAW0_0
			... CMDQ_EVENT_IMG_TRAW1_DIP_DMA_ERR_EVENT:
			return "MM_IMG_LTRAW";
		case CMDQ_EVENT_IMG_ADL_TILE_DONE_EVENT
			... CMDQ_EVENT_IMG_ADLWR_TILE_DONE_EVENT:
			return "MM_IMG_ADL";
		case CMDQ_EVENT_IMG_DIP_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMG_DIP_DUMMY_2:
			return "MM_IMG_DIP";
		case CMDQ_EVENT_IMG_WPE_EIS_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_WPE_EIS_CQ_THR_DONE_P2_9:
		case CMDQ_EVENT_IMG_WPE0_DUMMY_0
			... CMDQ_EVENT_IMG_WPE0_DUMMY_2:
			return "MM_IMG_WPE"; //WPE_EIS
		case CMDQ_EVENT_IMG_PQDIP_A_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMG_PQA_DMA_ERR_EVENT:
			return "MM_IMG_PQDIP"; //PQDIP_A
		case CMDQ_EVENT_IMG_WPE_TNR_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_WPE_TNR_CQ_THR_DONE_P2_9:
		case CMDQ_EVENT_IMG_WPE1_DUMMY_0
			... CMDQ_EVENT_IMG_WPE1_DUMMY_2:
			return "MM_IMG_WPE"; //WPE_TNR
		case CMDQ_EVENT_IMG_PQDIP_B_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMG_PQB_DMA_ERR_EVENT:
			return "MM_IMG_PQDIP"; //PQDIP_B
		case CMDQ_EVENT_IMG_WPE_LITE_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_WPE_LITE_CQ_THR_DONE_P2_9:
		case CMDQ_EVENT_IMG_WPE2_DUMMY_0
			... CMDQ_EVENT_IMG_WPE2_DUMMY_2:
			return "MM_IMG_WPE"; //WPE_LITE
		case CMDQ_EVENT_IMG_XTRAW_RESERVED_0
			... CMDQ_EVENT_IMG_XTRAW_DMA_ERR_EVENT_RESERVED:
			return "MM_IMG_TRAW";
		case CMDQ_EVENT_IMG_IMGSYS_IPE_FDVT0_DONE:
			return "MM_IMG_FDVT";
		case CMDQ_EVENT_IMG_IMGSYS_IPE_ME_DONE:
		case CMDQ_EVENT_IMG_IMGSYS_IPE_MMG_DONE:
			return "MM_IMG_ME";
		case CMDQ_SYNC_TOKEN_DIP_POWER_CTRL
			... CMDQ_SYNC_TOKEN_TRAW_PWR_HAND_SHAKE:
			return "MM_IMG_QOF";
		case CMDQ_EVENT_IMG_IMG_EVENT_122
			... CMDQ_EVENT_IMG_IMG_EVENT_127:
			return "MM_IMGSYS";
		case CMDQ_EVENT_CAM_DPE_DVP_CMQ_EVENT
			... CMDQ_EVENT_CAM_DPE_DVS_CMQ_EVENT:
		case CMDQ_EVENT_CAM_DVGF_CMQ_EVENT:
			return "MM_CAM_DPE";
		case CMDQ_EVENT_CAM_CAM_SUBA_SW_PASS1_DONE
			... CMDQ_EVENT_CAM_PDA1_IRQO_EVENT_DONE_D1:
		case CMDQ_EVENT_CAM_CAM_SUBA_TG_INT1
			... CMDQ_EVENT_CAM_CAM_SUBC_RING_BUFFER_OVERFLOW_INT_IN:
		case CMDQ_EVENT_CAM_ADL_WR_FRAME_DONE
			... CMDQ_EVENT_CAM_ADL_RD_FRAME_DONE:
		case CMDQ_EVENT_CAM_FUS_LA_CMQ_EVENT_0
			... CMDQ_EVENT_CAM_SENINF_CAM17_FIFO_FULL:
		case CMDQ_SYNC_TOKEN_APUSYS_APU: //APUSYS_EDMA
			return "MM_CAM";
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_1
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_133:
		case CMDQ_SYNC_TOKEN_IMGSYS_WPE_EIS
			... CMDQ_SYNC_TOKEN_IPESYS_ME:
		case CMDQ_SYNC_TOKEN_IMGSYS_VSS_TRAW
			... CMDQ_SYNC_TOKEN_IMGSYS_VSS_DIP:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_134
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_221:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_222
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_250:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_251
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_300:
			return "MM_IMGSYS";
		default:
			return "MM_GCEM";
		}

	return "MM_GCE";
}

u32 cmdq_util_hw_id(u32 pa)
{
	switch (pa) {
	case GCE_D_PA:
		return 0;
	case GCE_M_PA:
		return 1;
	default:
		cmdq_err("unknown addr:%x", pa);
	}

	return 0;
}

u32 cmdq_test_get_subsys_list(u32 **regs_out)
{
	static u32 regs[] = {
		0x1f003000,	/* mdp_rdma0 */
		0x1f000100,	/* mmsys_config */
		0x14001000,	/* dispsys */
		0x15101200,	/* imgsys */
		0x1000106c,	/* infra */
	};

	*regs_out = regs;
	return ARRAY_SIZE(regs);
}

void cmdq_test_set_ostd(void)
{
	void __iomem	*va_base;
	u32 val = 0x01014000;
	u32 pa_base;
	u32 preval, newval;

	/* 1. set mdp_smi_common outstanding to 1 : 0x1E80F10C = 0x01014000 */
	pa_base = 0x1E80F10C;
	va_base = ioremap(pa_base, 0x1000);
	preval = readl(va_base);
	writel(val, va_base);
	newval = readl(va_base);
	cmdq_msg("%s addr0x%#x: 0x%#x -> 0x%#x  ", __func__, pa_base, preval, newval);
}

const char *cmdq_util_hw_name(void *chan)
{
	u32 hw_id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));

	if (hw_id == 0)
		return "GCE-D";

	if (hw_id == 1)
		return "GCE-M";

	return "CMDQ";
}

bool cmdq_thread_ddr_module(const s32 thread)
{
	switch (thread) {
	case 0 ... 6:
	case 8 ... 9:
	case 15:
		return false;
	default:
		return true;
	}
}

bool cmdq_mbox_hw_trace_thread(void *chan)
{
	const phys_addr_t gce_pa = cmdq_mbox_get_base_pa(chan);
	const s32 idx = cmdq_mbox_chan_id(chan);

	if (gce_pa == GCE_D_PA)
		switch (idx) {
		case 16 ... 19: // MML
			cmdq_log("%s: pa:%pa idx:%d", __func__, &gce_pa, idx);
			return false;
		}

	return true;
}

void cmdq_error_irq_debug(void *chan)
{
	struct device *dev = cmdq_mbox_get_dev(chan);

	//dump smmu info to check gce va mode
	mtk_smmu_reg_dump(MM_SMMU, dev, MMSID_SRT_NORMAL);
	//dump gce req
	cmdq_mbox_dump_gce_req(chan);
}

bool cmdq_check_tf(struct device *dev,
	u32 sid, u32 tbu, u32 *axids)
{
	struct mtk_smmu_fault_param out_param;

	return mtk_smmu_tf_detect(MM_SMMU, dev,
		sid, tbu, axids, 1, &out_param);
}

uint cmdq_get_mdp_min_thread(void)
{
	return MDP_THRD_MIN;
}

struct cmdq_util_platform_fp platform_fp = {
	.thread_module_dispatch = cmdq_thread_module_dispatch,
	.event_module_dispatch = cmdq_event_module_dispatch,
	.util_hw_id = cmdq_util_hw_id,
	.test_get_subsys_list = cmdq_test_get_subsys_list,
	.test_set_ostd = cmdq_test_set_ostd,
	.util_hw_name = cmdq_util_hw_name,
	.thread_ddr_module = cmdq_thread_ddr_module,
	.hw_trace_thread = cmdq_mbox_hw_trace_thread,
	.dump_error_irq_debug = cmdq_error_irq_debug,
	.check_tf = cmdq_check_tf,
	.get_mdp_min_thread = cmdq_get_mdp_min_thread,
};

static int __init cmdq_platform_init(void)
{
	cmdq_util_set_fp(&platform_fp);
	return 0;
}
module_init(cmdq_platform_init);

MODULE_LICENSE("GPL");
