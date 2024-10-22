// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_disp_c3d.h"

#include "mtk_disp_pq_helper.h"
#define HW_ENGINE_NUM (2)

#define C3D_REG_3(v0, off0, v1, off1, v2, off2) \
	(((v2) << (off2)) |  ((v1) << (off1)) | ((v0) << (off0)))
#define C3D_REG_2(v0, off0, v1, off1) \
	(((v1) << (off1)) | ((v0) << (off0)))

enum C3D_IOCTL_CMD {
	SET_C3DLUT = 0,
	BYPASS_C3D,
};

enum C3D_CMDQ_TYPE {
	C3D_USERSPACE = 0,
	C3D_PREPARE,
};

static bool debug_flow_log;
#define C3DFLOW_LOG(fmt, arg...) do { \
	if (debug_flow_log) \
		pr_notice("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_api_log;
#define C3DAPI_LOG(fmt, arg...) do { \
	if (debug_api_log) \
		pr_notice("[API]%s:" fmt, __func__, ##arg); \
	} while (0)

static void mtk_disp_c3d_write_mask(void __iomem *address, u32 data, u32 mask)
{
	u32 value = data;

	if (mask != ~0) {
		value = readl(address);
		value &= ~mask;
		data &= mask;
		value |= data;
	}
	writel(value, address);
}

inline struct mtk_disp_c3d *comp_to_c3d(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_c3d, ddp_comp);
}

static void disp_c3d_lock_wake_lock(struct mtk_ddp_comp *comp, bool lock)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;

	if (lock) {
		if (!primary_data->c3d_wake_locked) {
			__pm_stay_awake(primary_data->c3d_wake_lock);
			primary_data->c3d_wake_locked = true;
		} else  {
			DDPPR_ERR("%s: try lock twice\n", __func__);
		}
	} else {
		if (primary_data->c3d_wake_locked) {
			__pm_relax(primary_data->c3d_wake_lock);
			primary_data->c3d_wake_locked = false;
		} else {
			DDPPR_ERR("%s: try unlock twice\n", __func__);
		}
	}
}

static int mtk_disp_c3d_create_gce_pkt(struct mtk_ddp_comp *comp, struct cmdq_pkt **pkt)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	if (!mtk_crtc) {
		DDPPR_ERR("%s:%d, invalid crtc\n",
				__func__, __LINE__);
		return -1;
	}

	if (*pkt != NULL)
		return 0;

	if (mtk_crtc->gce_obj.client[CLIENT_PQ])
		*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_PQ]);
	else
		*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);

	return 0;
}

#define C3D_SRAM_POLL_SLEEP_TIME_US	(10)
#define C3D_SRAM_MAX_POLL_TIME_US	(1000)

static inline bool disp_c3d_reg_poll(struct mtk_ddp_comp *comp,
	unsigned long addr, unsigned int value, unsigned int mask)
{
	bool return_value = false;
	unsigned int reg_value = 0;
	unsigned int polling_time = 0;

	do {
		reg_value = readl(comp->regs + addr);

		if ((reg_value & mask) == value) {
			return_value = true;
			break;
		}

		udelay(C3D_SRAM_POLL_SLEEP_TIME_US);
		polling_time += C3D_SRAM_POLL_SLEEP_TIME_US;
	} while (polling_time < C3D_SRAM_MAX_POLL_TIME_US);

	return return_value;
}

static inline bool disp_c3d_sram_write(struct mtk_ddp_comp *comp,
	unsigned int addr, unsigned int value)
{
	bool return_value = false;

	do {
		writel(addr, comp->regs + C3D_SRAM_RW_IF_0);
		writel(value, comp->regs + C3D_SRAM_RW_IF_1);

		return_value = true;
	} while (0);

	return return_value;
}

static inline bool disp_c3d_sram_read(struct mtk_ddp_comp *comp,
	unsigned int addr, unsigned int *value)
{
	bool return_value = false;

	do {
		writel(addr, comp->regs + C3D_SRAM_RW_IF_2);

		if (disp_c3d_reg_poll(comp, C3D_SRAM_STATUS,
				(0x1 << 17), (0x1 << 17)) != true)
			break;

		*value = readl(comp->regs + C3D_SRAM_RW_IF_3);

		return_value = true;
	} while (0);

	return return_value;
}

static bool c3d_is_clock_on(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d *comp_c3d_data = NULL;

	if (!comp->mtk_crtc->is_dual_pipe &&
			atomic_read(&c3d_data->c3d_is_clock_on) == 1)
		return true;

	if (comp->mtk_crtc->is_dual_pipe) {
		comp_c3d_data = comp_to_c3d(c3d_data->companion);
		if (atomic_read(&c3d_data->c3d_is_clock_on) == 1 &&
			comp_c3d_data && atomic_read(&comp_c3d_data->c3d_is_clock_on) == 1)
			return true;
	}

	return false;
}

static bool disp_c3d_write_sram(struct mtk_ddp_comp *comp, int cmd_type)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	struct cmdq_pkt *cmdq_handle = primary_data->sram_pkt;
	struct cmdq_client *client = NULL;
	struct drm_crtc *crtc = &mtk_crtc->base;
	bool async = false;

	if (!cmdq_handle) {
		DDPMSG("%s: cmdq handle is null.\n", __func__);
		return false;
	}

	if (c3d_is_clock_on(comp) == false)
		return false;

	if (mtk_crtc->gce_obj.client[CLIENT_PQ])
		client = mtk_crtc->gce_obj.client[CLIENT_PQ];
	else
		client = mtk_crtc->gce_obj.client[CLIENT_CFG];

	disp_c3d_lock_wake_lock(comp, true);
	async = mtk_drm_idlemgr_get_async_status(crtc);
	if (async == false) {
		cmdq_mbox_enable(client->chan);
		mtk_drm_clear_async_cb_list(crtc);
	}

	switch (cmd_type) {
	case C3D_USERSPACE:
		cmdq_pkt_refinalize(cmdq_handle);
		cmdq_pkt_flush(cmdq_handle);
		if (async == false)
			cmdq_mbox_disable(client->chan);
		break;

	case C3D_PREPARE:
		cmdq_pkt_refinalize(cmdq_handle);
		if (async == false) {
			cmdq_pkt_flush(cmdq_handle);
			cmdq_mbox_disable(client->chan);
		} else {
			int ret = 0;

			ret = mtk_drm_idle_async_flush_cust(crtc, comp->id,
						cmdq_handle, false, NULL);
			if (ret < 0) {
				cmdq_pkt_flush(cmdq_handle);
				DDPMSG("%s, failed of async flush, %d\n", __func__, ret);
			}
		}
		break;
	}

	disp_c3d_lock_wake_lock(comp, false);

	return true;
}

static void disp_c3d_config_sram(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	struct cmdq_pkt *handle = NULL;
	unsigned int *cfg;
	struct cmdq_reuse *reuse;
	unsigned int reuse_buf_size;
	unsigned int sram_offset = 0;
	unsigned int write_value = 0;

	cfg = primary_data->c3d_sram_cfg;
	reuse_buf_size = DISP_C3D_SRAM_SIZE_17BIN + 1;
	if (c3d_data->data->bin_num == 9) {
		cfg = primary_data->c3d_sram_cfg_9bin;
		reuse_buf_size = DISP_C3D_SRAM_SIZE_9BIN + 1;
	} else if (c3d_data->data->bin_num != 17)
		DDPMSG("%s: %d bin Not support!", __func__, c3d_data->data->bin_num);

	reuse = c3d_data->reuse_c3d;
	handle = primary_data->sram_pkt;

	C3DFLOW_LOG("handle: %d\n", ((handle == NULL) ? 1 : 0));
	if (handle == NULL)
		return;

	(handle)->no_pool = true;

	// Write 3D LUT to SRAM
	if (!c3d_data->pkt_reused) {
		cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_RW_IF_0,
			c3d_data->data->c3d_sram_start_addr, ~0, &reuse[0]);
		reuse[0].val = c3d_data->data->c3d_sram_start_addr;
		for (sram_offset = c3d_data->data->c3d_sram_start_addr;
			sram_offset <= c3d_data->data->c3d_sram_end_addr;
				sram_offset += 4) {
			write_value = cfg[sram_offset/4];

			// use cmdq reuse to save time
			cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_RW_IF_1,
				write_value, ~0, &reuse[sram_offset/4 + 1]);

			c3d_data->pkt_reused = true;
		}
	} else {
		for (sram_offset = c3d_data->data->c3d_sram_start_addr;
			sram_offset <= c3d_data->data->c3d_sram_end_addr;
				sram_offset += 4) {
			reuse[sram_offset/4 + 1].val = cfg[sram_offset/4];
			cmdq_pkt_reuse_value(handle, &reuse[sram_offset/4 + 1]);
		}
	}
}

void disp_c3d_flip_sram(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	const char *caller)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	u32 sram_apb = 0, sram_int = 0, sram_cfg;
	unsigned int read_value = 0;

	read_value = readl(comp->regs + C3D_SRAM_CFG);
	sram_apb = (read_value >> 5) & 0x1;
	sram_int = (read_value >> 6) & 0x1;

	if ((sram_apb == sram_int) && (sram_int == 0)) {
		pr_notice("%s: sram_apb == sram_int, skip flip!", __func__);
		return;
	}

	if (atomic_cmpxchg(&c3d_data->c3d_force_sram_apb, 0, 1) == 0) {
		sram_apb = 0;
		sram_int = 1;
	} else if (atomic_cmpxchg(&c3d_data->c3d_force_sram_apb, 1, 0) == 1) {
		sram_apb = 1;
		sram_int = 0;
	} else {
		DDPINFO("[SRAM] Error when get hist_apb in %s", caller);
	}
	sram_cfg = (sram_int << 6) | (sram_apb << 5) | (1 << 4);
	C3DFLOW_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x in %s",
		sram_apb, sram_int, sram_cfg, caller);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + C3D_SRAM_CFG, sram_cfg, (0x7 << 4));
}

static int mtk_drm_ioctl_c3d_get_bin_num_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

	int *c3d_bin_num = (int *)data;
	*c3d_bin_num = c3d_data->data->bin_num;

	return ret;
}

int mtk_drm_ioctl_c3d_get_bin_num(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_C3D, 0);

	return mtk_drm_ioctl_c3d_get_bin_num_impl(comp, data);
}

static int disp_c3d_wait_irq(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	int ret = 0;

	if (atomic_read(&primary_data->c3d_get_irq) == 0 ||
			atomic_read(&primary_data->c3d_eventctl) == 0) {
		C3DFLOW_LOG("wait_event_interruptible +++\n");
		ret = wait_event_interruptible(primary_data->c3d_get_irq_wq,
				(atomic_read(&primary_data->c3d_get_irq) == 1) &&
				(atomic_read(&primary_data->c3d_eventctl) == 1));

		if (ret >= 0) {
			C3DFLOW_LOG("wait_event_interruptible ---\n");
			C3DFLOW_LOG("get_irq = 1, wake up, ret = %d\n", ret);
		} else {
			DDPINFO("%s: interrupted unexpected\n", __func__);
		}
	} else {
		C3DFLOW_LOG("get_irq = 0");
	}

	atomic_set(&primary_data->c3d_get_irq, 0);

	return ret;
}

static void disp_c3d_update_sram(struct mtk_ddp_comp *comp,
	 bool check_sram)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	//unsigned long flags;
	unsigned int read_value = 0;
	int sram_apb = 0, sram_int = 0;
	char comp_name[64] = {0};

	mtk_ddp_comp_get_name(comp, comp_name, sizeof(comp_name));
	if (check_sram) {
		read_value = readl(comp->regs + C3D_SRAM_CFG);
		sram_apb = (read_value >> 5) & 0x1;
		sram_int = (read_value >> 6) & 0x1;
		C3DFLOW_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x in (SOF) comp:%s\n",
			sram_apb, sram_int, read_value, comp_name);
		// after suspend/resume, set FORCE_SRAM_APB = 0, FORCE_SRAM_INT = 0;
		// so need to config C3D_SRAM_CFG on ping-pong mode correctly.
		if ((sram_apb == sram_int) && (sram_int == 0)) {
			mtk_disp_c3d_write_mask(comp->regs + C3D_SRAM_CFG,
				(0 << 6)|(1 << 5)|(1 << 4), (0x7 << 4));
			C3DFLOW_LOG("%s: C3D_SRAM_CFG(0x%08x)\n", __func__,
				readl(comp->regs + C3D_SRAM_CFG));
		}
		if (sram_int != atomic_read(&c3d_data->c3d_force_sram_apb)) {
			pr_notice("c3d: SRAM config %d != %d config", sram_int,
				atomic_read(&c3d_data->c3d_force_sram_apb));

			if ((comp->mtk_crtc->is_dual_pipe) && (!c3d_data->is_right_pipe)) {
				pr_notice("%s: set update_sram_ignore=true", __func__);
				primary_data->update_sram_ignore = true;

				return;
			}
		}


		if ((comp->mtk_crtc->is_dual_pipe) && c3d_data->is_right_pipe
				&& primary_data->set_lut_flag && primary_data->update_sram_ignore) {
			primary_data->update_sram_ignore = false;
			primary_data->skip_update_sram = true;

			pr_notice("%s: set update_sram_ignore=false", __func__);

			return;
		}
	}

	disp_c3d_config_sram(comp);
}


static int disp_c3d_cfg_write_3dlut_to_reg(struct mtk_ddp_comp *comp,
	const struct DISP_C3D_LUT *c3d_lut)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	int c3dBinNum;
	int i;

	c3dBinNum = c3d_data->data->bin_num;

	C3DFLOW_LOG("c3dBinNum: %d", c3dBinNum);
	if (c3dBinNum == 17) {
		if (!c3d_data->is_right_pipe) {
			if (copy_from_user(&primary_data->c3d_reg_17bin,
				C3D_U32_PTR(c3d_lut->lut3d),
				sizeof(primary_data->c3d_reg_17bin)) == 0) {
				C3DFLOW_LOG("%x, %x, %x",
					primary_data->c3d_reg_17bin.lut3d_reg[14735],
					primary_data->c3d_reg_17bin.lut3d_reg[14737],
					primary_data->c3d_reg_17bin.lut3d_reg[14738]);
				mutex_lock(&primary_data->c3d_lut_lock);
				for (i = 0; i < C3D_3DLUT_SIZE_17BIN; i += 3) {
					primary_data->c3d_sram_cfg[i / 3] =
						primary_data->c3d_reg_17bin.lut3d_reg[i] |
						(primary_data->c3d_reg_17bin.lut3d_reg[i+1] << 10) |
						(primary_data->c3d_reg_17bin.lut3d_reg[i+2] << 20);
				}
				C3DFLOW_LOG("%x\n", primary_data->c3d_sram_cfg[4912]);

				mutex_unlock(&primary_data->c3d_lut_lock);
			}
		}
		mutex_lock(&primary_data->c3d_lut_lock);
		disp_c3d_update_sram(comp, true);
		mutex_unlock(&primary_data->c3d_lut_lock);
	} else if (c3dBinNum == 9) {
		if (!c3d_data->is_right_pipe) {
			if (copy_from_user(&primary_data->c3d_reg_9bin,
						C3D_U32_PTR(c3d_lut->lut3d),
						sizeof(primary_data->c3d_reg_9bin)) == 0) {
				mutex_lock(&primary_data->c3d_lut_lock);
				for (i = 0; i < C3D_3DLUT_SIZE_9BIN; i += 3) {
					primary_data->c3d_sram_cfg_9bin[i / 3] =
						primary_data->c3d_reg_9bin.lut3d_reg[i] |
						(primary_data->c3d_reg_9bin.lut3d_reg[i+1] << 10) |
						(primary_data->c3d_reg_9bin.lut3d_reg[i+2] << 20);
				}
				mutex_unlock(&primary_data->c3d_lut_lock);
			}
		}
		mutex_lock(&primary_data->c3d_lut_lock);
		disp_c3d_update_sram(comp, true);
		mutex_unlock(&primary_data->c3d_lut_lock);
	} else {
		pr_notice("%s, c3d bin num: %d not support", __func__, c3dBinNum);
	}

	return 0;
}

static int disp_c3d_write_3dlut_to_reg(struct mtk_ddp_comp *comp,
	const struct DISP_C3D_LUT *c3d_lut)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	int c3dBinNum;
	int i;

	c3dBinNum = c3d_data->data->bin_num;

	C3DFLOW_LOG("c3dBinNum: %d", c3dBinNum);
	if (c3dBinNum == 17) {
		if (copy_from_user(&primary_data->c3d_reg_17bin,
				C3D_U32_PTR(c3d_lut->lut3d),
				sizeof(primary_data->c3d_reg_17bin)) == 0) {
			C3DFLOW_LOG("%x, %x, %x", primary_data->c3d_reg_17bin.lut3d_reg[14735],
				primary_data->c3d_reg_17bin.lut3d_reg[14737],
				primary_data->c3d_reg_17bin.lut3d_reg[14738]);
			mutex_lock(&primary_data->c3d_lut_lock);
			for (i = 0; i < C3D_3DLUT_SIZE_17BIN; i += 3) {
				primary_data->c3d_sram_cfg[i / 3] =
					primary_data->c3d_reg_17bin.lut3d_reg[i] |
					(primary_data->c3d_reg_17bin.lut3d_reg[i + 1] << 10) |
					(primary_data->c3d_reg_17bin.lut3d_reg[i + 2] << 20);
			}
			C3DFLOW_LOG("%x\n", primary_data->c3d_sram_cfg[4912]);

			disp_c3d_update_sram(comp, true);
			mutex_unlock(&primary_data->c3d_lut_lock);
		}
	} else if (c3dBinNum == 9) {
		if (copy_from_user(&primary_data->c3d_reg_9bin,
					C3D_U32_PTR(c3d_lut->lut3d),
					sizeof(primary_data->c3d_reg_9bin)) == 0) {
			mutex_lock(&primary_data->c3d_lut_lock);
			for (i = 0; i < C3D_3DLUT_SIZE_9BIN; i += 3) {
				primary_data->c3d_sram_cfg_9bin[i / 3] =
					primary_data->c3d_reg_9bin.lut3d_reg[i] |
					(primary_data->c3d_reg_9bin.lut3d_reg[i + 1] << 10) |
					(primary_data->c3d_reg_9bin.lut3d_reg[i + 2] << 20);
			}

			disp_c3d_update_sram(comp, true);
			mutex_unlock(&primary_data->c3d_lut_lock);
		}
	} else {
		pr_notice("%s, c3d bin num: %d not support", __func__, c3dBinNum);
	}

	return 0;
}

int mtk_drm_ioctl_c3d_get_irq_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;

	C3DAPI_LOG("line: %d\n", __LINE__);
	ret = disp_c3d_wait_irq(comp);

	return ret;
}

int mtk_drm_ioctl_c3d_get_irq(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_C3D, 0);

	return mtk_drm_ioctl_c3d_get_irq_impl(comp, data);
}

int mtk_c3d_cfg_eventctl(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	int ret = 0;
	int *enabled = (int *)data;

	C3DFLOW_LOG("%d\n", *enabled);
	atomic_set(&primary_data->c3d_eventctl, *enabled);

	if (atomic_read(&primary_data->c3d_eventctl) == 1)
		wake_up_interruptible(&primary_data->c3d_get_irq_wq);

	return ret;
}

int mtk_drm_ioctl_c3d_eventctl_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	int ret = 0;
	int *enabled = (int *)data;

	C3DFLOW_LOG("%d\n", *enabled);

	atomic_set(&primary_data->c3d_eventctl, *enabled);
	C3DFLOW_LOG("%d\n", atomic_read(&primary_data->c3d_eventctl));

	if (*enabled)
		mtk_crtc_check_trigger(comp->mtk_crtc, true, true);

	return ret;
}

int mtk_drm_ioctl_c3d_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_C3D, 0);

	return mtk_drm_ioctl_c3d_eventctl_impl(comp, data);
}

int mtk_drm_ioctl_c3d_set_lut_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	int ret = 0;

	primary_data->skip_update_sram = false;

	C3DAPI_LOG("line: %d\n", __LINE__);

	memcpy(&primary_data->c3d_ioc_data, (struct DISP_C3D_LUT *)data,
				sizeof(struct DISP_C3D_LUT));
	// 1. kick idle
	if (!mtk_crtc) {
		DDPMSG("%s:%d, invalid mtk_crtc:0x%p\n",
				__func__, __LINE__, mtk_crtc);
		return -1;
	}

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!(mtk_crtc->enabled)) {
		DDPMSG("%s:%d, slepted\n", __func__, __LINE__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	// 2. lock for protect crtc & power
	mutex_lock(&primary_data->c3d_power_lock);
	if ((atomic_read(&c3d_data->c3d_is_clock_on) == 1)
			&& (mtk_crtc->enabled)) {
		primary_data->set_lut_flag = true;

		disp_c3d_write_3dlut_to_reg(comp, &primary_data->c3d_ioc_data);
		if (mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_c3d1 = c3d_data->companion;
			struct mtk_disp_c3d *companion_data = comp_to_c3d(comp_c3d1);

			if ((atomic_read(&companion_data->c3d_is_clock_on) == 1)
					&& (mtk_crtc->enabled))
				disp_c3d_write_3dlut_to_reg(comp_c3d1, &primary_data->c3d_ioc_data);
			else {
				DDPINFO("%s(line: %d): skip write_3dlut(mtk_crtc:%d)\n",
						__func__, __LINE__, mtk_crtc->enabled ? 1 : 0);

				primary_data->set_lut_flag = false;

				mutex_unlock(&primary_data->c3d_power_lock);

				return -1;
			}
		}
		primary_data->set_lut_flag = false;
	} else {
		DDPINFO("%s(line: %d): skip write_3dlut(mtk_crtc:%d)\n",
				__func__, __LINE__, mtk_crtc->enabled ? 1 : 0);

		mutex_unlock(&primary_data->c3d_power_lock);

		return -1;
	}
	disp_c3d_write_sram(comp, C3D_USERSPACE);

	mutex_unlock(&primary_data->c3d_power_lock);

	if (primary_data->skip_update_sram) {
		pr_notice("%s, skip_update_sram %d return\n", __func__,
				primary_data->skip_update_sram);
		return -EFAULT;
	}

	ret = mtk_crtc_user_cmd(crtc, comp, SET_C3DLUT, data);

	mtk_crtc_check_trigger(comp->mtk_crtc, true, false);

	return ret;
}

int mtk_drm_ioctl_c3d_set_lut(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_C3D, 0);

	return mtk_drm_ioctl_c3d_set_lut_impl(comp, data);
}

int mtk_drm_ioctl_bypass_c3d_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	int ret = 0;

	C3DAPI_LOG("line: %d\n", __LINE__);

	ret = mtk_crtc_user_cmd(crtc, comp, BYPASS_C3D, data);

	mtk_crtc_check_trigger(mtk_crtc, true, false);
	return ret;
}

int mtk_drm_ioctl_bypass_c3d(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_C3D, 0);

	return mtk_drm_ioctl_bypass_c3d_impl(comp, data);
}

static void disp_c3d_on_start_of_frame(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;

	if (!comp)
		return;
	atomic_set(&primary_data->c3d_get_irq, 1);

	if (atomic_read(&primary_data->c3d_eventctl) == 1)
		wake_up_interruptible(&primary_data->c3d_get_irq_wq);
}

static void disp_c3d_on_end_of_frame(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;

	if (!comp)
		return;
	atomic_set(&primary_data->c3d_get_irq, 0);
}

static int disp_c3d_set_1dlut(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, int lock)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	unsigned int *lut1d;
	int ret = 0;
	char comp_name[64] = {0};

	mtk_ddp_comp_get_name(comp, comp_name, sizeof(comp_name));

	if (lock)
		mutex_lock(&primary_data->c3d_global_lock);
	lut1d = &primary_data->c3d_lut1d[0];
	if (lut1d == NULL) {
		pr_notice("%s: table [%s] not initialized, use default config\n",
				__func__, comp_name);
		ret = -EFAULT;
	} else {
		C3DFLOW_LOG("%x, %x, %x, %x, %x", lut1d[0],
				lut1d[2], lut1d[3], lut1d[5], lut1d[6]);
		c3d_data->has_set_1dlut = true;

		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_000_001,
				C3D_REG_2(lut1d[1], 0, lut1d[0], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_002_003,
				C3D_REG_2(lut1d[3], 0, lut1d[2], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_004_005,
				C3D_REG_2(lut1d[5], 0, lut1d[4], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_006_007,
				C3D_REG_2(lut1d[7], 0, lut1d[6], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_008_009,
				C3D_REG_2(lut1d[9], 0, lut1d[8], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_010_011,
				C3D_REG_2(lut1d[11], 0, lut1d[10], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_012_013,
				C3D_REG_2(lut1d[13], 0, lut1d[12], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_014_015,
				C3D_REG_2(lut1d[15], 0, lut1d[14], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_016_017,
				C3D_REG_2(lut1d[17], 0, lut1d[16], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_018_019,
				C3D_REG_2(lut1d[19], 0, lut1d[18], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_020_021,
				C3D_REG_2(lut1d[21], 0, lut1d[20], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_022_023,
				C3D_REG_2(lut1d[23], 0, lut1d[22], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_024_025,
				C3D_REG_2(lut1d[25], 0, lut1d[24], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_026_027,
				C3D_REG_2(lut1d[27], 0, lut1d[26], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_028_029,
				C3D_REG_2(lut1d[29], 0, lut1d[28], 16), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_C1D_030_031,
				C3D_REG_2(lut1d[31], 0, lut1d[30], 16), ~0);
	}

	if (lock)
		mutex_unlock(&primary_data->c3d_global_lock);

	return ret;
}

static int disp_c3d_write_1dlut_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_C3D_LUT *c3d_lut)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	unsigned int *c3d_lut1d;
	int ret = 0;

	if (c3d_lut == NULL) {
		ret = -EFAULT;
	} else {
		pr_notice("%s", __func__);
		c3d_lut1d = (unsigned int *) &(c3d_lut->lut1d[0]);
		C3DFLOW_LOG("%x, %x, %x, %x, %x", c3d_lut1d[0],
				c3d_lut1d[2], c3d_lut1d[3], c3d_lut1d[5], c3d_lut1d[6]);

		mutex_lock(&primary_data->c3d_global_lock);
		if (!c3d_data->has_set_1dlut ||
				memcmp(&primary_data->c3d_lut1d[0], c3d_lut1d,
				sizeof(primary_data->c3d_lut1d))) {
			memcpy(&primary_data->c3d_lut1d[0], c3d_lut1d,
					sizeof(primary_data->c3d_lut1d));
			ret = disp_c3d_set_1dlut(comp, handle, 0);
		}
		mutex_unlock(&primary_data->c3d_global_lock);
	}

	return ret;
}

static int disp_c3d_write_lut_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_C3D_LUT *c3d_lut)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;

	if (atomic_read(&primary_data->c3d_force_relay) == 1) {
		// Set reply mode
		DDPINFO("c3d_force_relay\n");
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x3, 0x3);
	} else {
		// Disable reply mode
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x2, 0x3);
	}

	disp_c3d_write_1dlut_to_reg(comp, handle, c3d_lut);

	return 0;
}

static int disp_c3d_set_lut(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		void *data)
{
	int ret;
	struct mtk_disp_c3d *c3d = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d->primary_data;

	ret = disp_c3d_write_lut_to_reg(comp, handle, (struct DISP_C3D_LUT *)data);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_c3d1 = c3d->companion;

		ret = disp_c3d_write_lut_to_reg(comp_c3d1, handle, (struct DISP_C3D_LUT *)data);
		disp_c3d_flip_sram(comp, handle, __func__);
		disp_c3d_flip_sram(comp_c3d1, handle, __func__);
	} else {
		disp_c3d_flip_sram(comp, handle, __func__);
	}
	atomic_set(&primary_data->c3d_sram_hw_init, 1);

	return ret;
}

static void mtk_disp_c3d_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	char comp_name[64] = {0};

	if (!atomic_read(&primary_data->c3d_sram_hw_init) && !bypass) {
		DDPPR_ERR("%s, c3d sram invalid, skip unrelay setting!\n", __func__);
		return;
	}

	mtk_ddp_comp_get_name(comp, comp_name, sizeof(comp_name));
	pr_notice("%s, comp: %s, bypass: %d\n",
			__func__, comp_name, bypass);

	if (bypass == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_CFG, 0x1, 0x1);
		atomic_set(&primary_data->c3d_force_relay, 0x1);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_CFG, 0x0, 0x1);
		atomic_set(&primary_data->c3d_force_relay, 0x0);
	}
}

int mtk_c3d_cfg_bypass(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	unsigned int *value = data;
	struct mtk_disp_c3d *c3d = comp_to_c3d(comp);

	mtk_disp_c3d_bypass(comp, *value, handle);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_c3d1 = c3d->companion;

		mtk_disp_c3d_bypass(comp_c3d1, *value, handle);
	}
	return ret;
}

static int mtk_disp_c3d_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int cmd, void *data)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

	C3DFLOW_LOG("cmd: %d\n", cmd);
	switch (cmd) {
	case SET_C3DLUT:
	{
		if (disp_c3d_set_lut(comp, handle, data) < 0) {
			DDPPR_ERR("SET_PARAM: fail\n");
			return -EFAULT;
		}
	}
	break;
	case BYPASS_C3D:
	{
		struct mtk_ddp_comp *comp_c3d1 = c3d_data->companion;
		unsigned int *value = data;

		mtk_disp_c3d_bypass(comp, *value, handle);
		if (comp->mtk_crtc->is_dual_pipe && comp_c3d1)
			mtk_disp_c3d_bypass(comp_c3d1, *value, handle);
	}
	break;
	default:
		DDPPR_ERR("error cmd: %d\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static void mtk_disp_c3d_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!c3d_data->is_right_pipe) {
			c3d_data->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				c3d_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width +=
				c3d_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			c3d_data->tile_overhead.in_width =
				cfg->tile_overhead.left_in_width;
			c3d_data->tile_overhead.overhead =
				cfg->tile_overhead.left_overhead;
		} else if (c3d_data->is_right_pipe) {
			c3d_data->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				c3d_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				c3d_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			c3d_data->tile_overhead.in_width =
				cfg->tile_overhead.right_in_width;
			c3d_data->tile_overhead.overhead =
				cfg->tile_overhead.right_overhead;
		}
	}

}

static void mtk_disp_c3d_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	c3d_data->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		c3d_data->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	c3d_data->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void mtk_disp_c3d_config(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	unsigned int width;
	unsigned int overhead_v;

	C3DFLOW_LOG("line: %d\n", __LINE__);

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support)
		width = c3d_data->tile_overhead.in_width;
	else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;
	}

	if (!c3d_data->set_partial_update)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_SIZE, (width << 16) | cfg->h, ~0);
	else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : c3d_data->tile_overhead_v.overhead_v;
		cmdq_pkt_write(handle, comp->cmdq_base,
		   comp->regs_pa + C3D_SIZE, (width << 16) | (c3d_data->roi_height + overhead_v * 2), ~0);
	}

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_R2Y_09, 0, 0x1);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_Y2R_09, 0, 0x1);
}


static void mtk_disp_c3d_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;

	C3DFLOW_LOG("line: %d\n", __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_EN, 0x1, ~0);

	if (atomic_read(&primary_data->c3d_force_relay) == 1 ||
			atomic_read(&primary_data->c3d_sram_hw_init) == 0) {
		// Set reply mode
		DDPINFO("c3d_force_relay\n");
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x3, 0x3);
	} else {
		// Disable reply mode
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x2, 0x3);
	}

	disp_c3d_set_1dlut(comp, handle, 0);
}

static void mtk_disp_c3d_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	C3DFLOW_LOG("line: %d\n", __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_EN, 0x0, ~0);
}

static void mtk_disp_c3d_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	struct mtk_disp_c3d *priv = dev_get_drvdata(comp->dev);
	unsigned long long time[2] = {0};
	// create cmdq_pkt

	time[0] = sched_clock();

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&c3d_data->c3d_is_clock_on, 1);

	if (atomic_read(&primary_data->c3d_sram_hw_init) == 0) {
		mtk_ddp_write_mask_cpu(comp, 0x1, C3D_CFG, 0x1);
		time[1] = sched_clock();
		C3DFLOW_LOG("compID:%d, timediff: %llu\n", comp->id, time[1] - time[0]);
		return;
	}
	/* Bypass shadow register and read shadow register */
	if (priv->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, C3D_BYPASS_SHADOW,
			C3D_SHADOW_CTL, C3D_BYPASS_SHADOW);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			C3D_SHADOW_CTL, C3D_BYPASS_SHADOW);

	mutex_lock(&primary_data->c3d_lut_lock);
	mtk_disp_c3d_write_mask(comp->regs + C3D_SRAM_CFG,
		(0 << 6)|(0 << 5)|(1 << 4), (0x7 << 4));
	disp_c3d_write_sram(comp, C3D_PREPARE);
	atomic_set(&c3d_data->c3d_force_sram_apb, 0);
	mutex_unlock(&primary_data->c3d_lut_lock);

	time[1] = sched_clock();
	C3DFLOW_LOG("compID:%d, timediff: %llu\n", comp->id, time[1] - time[0]);
}

static void mtk_disp_c3d_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	unsigned long flags;

	c3d_data->has_set_1dlut = false;

	C3DFLOW_LOG("line: %d\n", __LINE__);

	mutex_lock(&primary_data->c3d_power_lock);
	spin_lock_irqsave(&primary_data->c3d_clock_lock, flags);
	atomic_set(&c3d_data->c3d_is_clock_on, 0);
	spin_unlock_irqrestore(&primary_data->c3d_clock_lock, flags);
	mtk_ddp_comp_clk_unprepare(comp);
	mutex_unlock(&primary_data->c3d_power_lock);
}

static void mtk_c3d_primary_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d *companion_data = comp_to_c3d(c3d_data->companion);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	unsigned int c3d_lut1d_init[DISP_C3D_1DLUT_SIZE] = {
		0, 256, 512, 768, 1024, 1280, 1536, 1792,
		2048, 2304, 2560, 2816, 3072, 3328, 3584, 3840,
		4096, 4608, 5120, 5632, 6144, 6656, 7168, 7680,
		8192, 9216, 10240, 11264, 12288, 13312, 14336, 15360
	};

	if (c3d_data->is_right_pipe) {
		kfree(c3d_data->primary_data);
		c3d_data->primary_data = companion_data->primary_data;
		return;
	}
	memcpy(&primary_data->c3d_lut1d, &c3d_lut1d_init,
			sizeof(c3d_lut1d_init));
	init_waitqueue_head(&primary_data->c3d_get_irq_wq);
	spin_lock_init(&primary_data->c3d_clock_lock);
	mutex_init(&primary_data->c3d_global_lock);
	mutex_init(&primary_data->c3d_power_lock);
	mutex_init(&primary_data->c3d_lut_lock);
	// destroy used pkt and create new one
	mtk_disp_c3d_create_gce_pkt(comp, &primary_data->sram_pkt);
}

void mtk_disp_c3d_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	mtk_disp_c3d_config(comp, cfg, handle);
}

int mtk_c3d_cfg_set_lut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_c3d *c3d = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d->primary_data;
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	primary_data->skip_update_sram = false;
	C3DAPI_LOG("line: %d\n", __LINE__);

	// 1. kick idle
	if (!mtk_crtc)
		return -1;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!(mtk_crtc->enabled)) {
		DDPMSG("%s:%d, slepted\n", __func__, __LINE__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	// 2. lock for protect crtc & power
	mutex_lock(&primary_data->c3d_power_lock);
	if ((atomic_read(&c3d->c3d_is_clock_on) == 1)
			&& (mtk_crtc->enabled)) {
		primary_data->set_lut_flag = true;

		disp_c3d_cfg_write_3dlut_to_reg(comp, (struct DISP_C3D_LUT *)data);
		if ((comp->mtk_crtc) && (comp->mtk_crtc->is_dual_pipe)) {
			struct mtk_ddp_comp *comp_c3d1 = c3d->companion;
			struct mtk_disp_c3d *companion_data = comp_to_c3d(comp_c3d1);

			if ((atomic_read(&companion_data->c3d_is_clock_on) == 1)
					&& (mtk_crtc->enabled))
				disp_c3d_cfg_write_3dlut_to_reg(comp_c3d1,
					(struct DISP_C3D_LUT *)data);
			else {
				DDPINFO("%s(line: %d): skip write_3dlut(mtk_crtc:%d)\n",
						__func__, __LINE__, mtk_crtc->enabled ? 1 : 0);

				primary_data->set_lut_flag = false;

				mutex_unlock(&primary_data->c3d_power_lock);

				return -1;
			}
		}
		disp_c3d_write_sram(comp, C3D_USERSPACE);
		primary_data->set_lut_flag = false;
		if (primary_data->skip_update_sram) {
			pr_notice("%s, skip_update_sram %d return\n", __func__,
					primary_data->skip_update_sram);
			mutex_unlock(&primary_data->c3d_power_lock);
			return -EFAULT;
		}
		ret = disp_c3d_set_lut(comp, handle, data);
		if (ret < 0) {
			DDPPR_ERR("SET_PARAM: fail\n");
			mutex_unlock(&primary_data->c3d_power_lock);
			return -EFAULT;
		}
	} else {
		DDPINFO("%s(line: %d): skip write_3dlut(mtk_crtc:%d)\n",
				__func__, __LINE__, mtk_crtc->enabled ? 1 : 0);

		mutex_unlock(&primary_data->c3d_power_lock);

		return -1;
	}
	mutex_unlock(&primary_data->c3d_power_lock);

	return ret;
}

static int mtk_c3d_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
							enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_disp_c3d *data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = data->primary_data;

	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_c3d *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = mtk_pq_helper_fill_comp_pipe_info(comp, path_order,
							is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_c3d(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		mtk_c3d_primary_data_init(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			mtk_c3d_primary_data_init(data->companion);
	}
		break;
	case NOTIFY_CONNECTOR_SWITCH:
	{
		DDPMSG("%s, set sram_hw_init 0\n", __func__);
		atomic_set(&primary_data->c3d_sram_hw_init, 0);
	}
		break;
	default:
		break;
	}
	return 0;
}

static int mtk_c3d_pq_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;
	/* will only call left path */
	switch (cmd) {
	/* TYPE1 no user cmd */
	case PQ_C3D_EVENTCTL:
		/*TODO check trigger ??*/
		ret = mtk_c3d_cfg_eventctl(comp, handle, data, data_size);
		break;
	case PQ_C3D_BYPASS:
		ret = mtk_c3d_cfg_bypass(comp, handle, data, data_size);
		break;
	case PQ_C3D_SET_LUT:
		ret = mtk_c3d_cfg_set_lut(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_c3d_ioctl_transact(struct mtk_ddp_comp *comp,
		unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	switch (cmd) {
	case PQ_C3D_GET_BIN_NUM:
		ret = mtk_drm_ioctl_c3d_get_bin_num_impl(comp, data);
		break;
	case PQ_C3D_GET_IRQ:
		ret = mtk_drm_ioctl_c3d_get_irq_impl(comp, data);
		break;
	case PQ_C3D_EVENTCTL:
		ret = mtk_drm_ioctl_c3d_eventctl_impl(comp, data);
		break;
	case PQ_C3D_SET_LUT:
		ret = mtk_drm_ioctl_c3d_set_lut_impl(comp, data);
		break;
	case PQ_C3D_BYPASS:
		ret = mtk_drm_ioctl_bypass_c3d_impl(comp, data);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_c3d_set_partial_update(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle, struct mtk_rect partial_roi, bool enable)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	c3d_data->set_partial_update = enable;
	c3d_data->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : c3d_data->tile_overhead_v.overhead_v;

	DDPDBG("%s, %s overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v);

	if (c3d_data->set_partial_update) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + C3D_SIZE, c3d_data->roi_height + overhead_v * 2, 0xffff);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + C3D_SIZE, full_height, 0xffff);
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_c3d_funcs = {
	.config = mtk_disp_c3d_config,
	.first_cfg = mtk_disp_c3d_first_cfg,
	.start = mtk_disp_c3d_start,
	.stop = mtk_disp_c3d_stop,
	.bypass = mtk_disp_c3d_bypass,
	.user_cmd = mtk_disp_c3d_user_cmd,
	.prepare = mtk_disp_c3d_prepare,
	.unprepare = mtk_disp_c3d_unprepare,
	.config_overhead = mtk_disp_c3d_config_overhead,
	.config_overhead_v = mtk_disp_c3d_config_overhead_v,
	.io_cmd = mtk_c3d_io_cmd,
	.pq_frame_config = mtk_c3d_pq_frame_config,
	.mutex_eof_irq = disp_c3d_on_end_of_frame,
	.mutex_sof_irq = disp_c3d_on_start_of_frame,
	.pq_ioctl_transact = mtk_c3d_ioctl_transact,
	.partial_update = mtk_c3d_set_partial_update,
};

static int mtk_disp_c3d_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_c3d *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	pr_notice("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	pr_notice("%s-\n", __func__);
	return 0;
}

static void mtk_disp_c3d_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_c3d *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	pr_notice("%s+\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
	pr_notice("%s-\n", __func__);
}

static const struct component_ops mtk_disp_c3d_component_ops = {
	.bind	= mtk_disp_c3d_bind,
	.unbind = mtk_disp_c3d_unbind,
};

void mtk_c3d_dump(struct mtk_ddp_comp *comp)
{
	void __iomem  *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x4, 0x18, 0x8C);
	mtk_cust_dump_reg(baddr, 0x24, 0x30, 0x34, 0x38);
	mtk_cust_dump_reg(baddr, 0x3C, 0x40, 0x44, 0x48);
	mtk_cust_dump_reg(baddr, 0x4C, 0x50, 0x54, 0x58);
	mtk_cust_dump_reg(baddr, 0x5C, 0x60, 0x64, 0x68);
	mtk_cust_dump_reg(baddr, 0x6C, 0x70, 0x74, 0x78);
	mtk_cust_dump_reg(baddr, 0x7C, 0x80, 0x84, 0x88);
}

void mtk_c3d_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	void __iomem  *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp),
			&comp->regs_pa);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0x94; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && c3d_data->companion) {
		baddr = c3d_data->companion->regs;
		DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(c3d_data->companion),
				&c3d_data->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(c3d_data->companion));
		for (k = 0; k <= 0x94; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(c3d_data->companion));
	}
}

static int mtk_disp_c3d_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_c3d *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret = -1;

	pr_notice("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		goto error_dev_init;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		DDPPR_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_C3D);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_c3d_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_c3d_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	pr_notice("%s-\n", __func__);

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int mtk_disp_c3d_remove(struct platform_device *pdev)
{
	struct mtk_disp_c3d *priv = dev_get_drvdata(&pdev->dev);

	pr_notice("%s+\n", __func__);
	component_del(&pdev->dev, &mtk_disp_c3d_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	pr_notice("%s-\n", __func__);
	return 0;
}

static const struct mtk_disp_c3d_data mt6983_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 17,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6895_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 17,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6879_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 9,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 2912,
};

static const struct mtk_disp_c3d_data mt6985_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 17,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6897_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 17,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6886_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 17,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6989_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 17,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6878_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.bin_num = 9,
	.c3d_sram_start_addr = 0,
	.c3d_sram_end_addr = 2912,
};

static const struct of_device_id mtk_disp_c3d_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6983-disp-c3d",
	  .data = &mt6983_c3d_driver_data},
	{ .compatible = "mediatek,mt6895-disp-c3d",
	  .data = &mt6895_c3d_driver_data},
	{ .compatible = "mediatek,mt6879-disp-c3d",
	  .data = &mt6879_c3d_driver_data},
	{ .compatible = "mediatek,mt6985-disp-c3d",
	  .data = &mt6985_c3d_driver_data},
	{ .compatible = "mediatek,mt6886-disp-c3d",
	  .data = &mt6886_c3d_driver_data},
	{ .compatible = "mediatek,mt6897-disp-c3d",
	  .data = &mt6897_c3d_driver_data},
	{ .compatible = "mediatek,mt6989-disp-c3d",
	  .data = &mt6989_c3d_driver_data},
	{ .compatible = "mediatek,mt6878-disp-c3d",
	  .data = &mt6878_c3d_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_c3d_driver_dt_match);

struct platform_driver mtk_disp_c3d_driver = {
	.probe = mtk_disp_c3d_probe,
	.remove = mtk_disp_c3d_remove,
	.driver = {
			.name = "mediatek-disp-c3d",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_c3d_driver_dt_match,
		},
};

void mtk_disp_c3d_debug(struct drm_crtc *crtc, const char *opt)
{
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_C3D, 0);
	struct mtk_disp_c3d *c3d_data;
	struct cmdq_reuse *reuse;
	u32 sram_offset;

	pr_notice("[C3D debug]: %s\n", opt);
	if (strncmp(opt, "flow_log:", 9) == 0) {
		debug_flow_log = strncmp(opt + 9, "1", 1) == 0;
		pr_notice("[C3D debug] debug_flow_log = %d\n", debug_flow_log);
	} else if (strncmp(opt, "api_log:", 8) == 0) {
		debug_api_log = strncmp(opt + 8, "1", 1) == 0;
		pr_notice("[C3D debug] debug_api_log = %d\n", debug_api_log);
	} else if (strncmp(opt, "debugdump:", 10) == 0) {
		pr_notice("[C3D debug] debug_flow_log = %d\n", debug_flow_log);
		pr_notice("[C3D debug] debug_api_log = %d\n", debug_api_log);
	} else if (strncmp(opt, "dumpsram", 8) == 0) {
		if (!comp) {
			pr_notice("[C3D debug] null pointer!\n");
			return;
		}
		c3d_data = comp_to_c3d(comp);
		reuse = c3d_data->reuse_c3d;
		for (sram_offset = c3d_data->data->c3d_sram_start_addr;
			sram_offset + 4 <= c3d_data->data->c3d_sram_end_addr && sram_offset <= 100;
			sram_offset += 4) {
			DDPMSG("[debug] c3d_sram 0x%x: 0x%x, 0x%x, 0x%x, 0x%x\n",
					sram_offset, reuse[sram_offset + 1].val, reuse[sram_offset + 2].val,
					reuse[sram_offset + 3].val, reuse[sram_offset + 4].val);
		}
	}
}

void disp_c3d_set_bypass(struct drm_crtc *crtc, int bypass)
{
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_C3D, 0);
	int ret;

	ret = mtk_crtc_user_cmd(crtc, comp, BYPASS_C3D, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}

unsigned int disp_c3d_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_c3d *c3d_data;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_C3D, 0);
	c3d_data = comp_to_c3d(comp);

	return atomic_read(&c3d_data->primary_data->c3d_force_relay);
}
