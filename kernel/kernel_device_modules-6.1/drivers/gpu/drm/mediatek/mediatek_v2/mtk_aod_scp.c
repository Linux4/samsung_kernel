// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/rtc.h>
#include "mtk_drm_drv.h"
#include "scp.h"
#include "mtk_log.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_oddmr/mtk_disp_oddmr.h"

struct aod_scp_ipi_receive_info {
	unsigned int aod_id;
};

static int CFG_DISPLAY_WIDTH;      //(1080)
static int CFG_DISPLAY_HEIGHT;     //(2340)
static int CFG_DISPLAY_VREFRESH;
static unsigned int aod_mmsys_id;

#define ALIGN_TO(x, n)  (((x) + ((n) - 1)) & ~((n) - 1))
#define MTK_FB_ALIGNMENT 32
#define CFG_DISPLAY_ALIGN_WIDTH   ALIGN_TO(CFG_DISPLAY_WIDTH, MTK_FB_ALIGNMENT)

#define DRAM_ADDR_DIGITS_OFFSET		0x3080000
#define LK_DGT_SZ_OFFSET		0x1fa40

#define AOD_BR_DSI0_OFFSET     0xF000
#define AOD_BR_MIPITX0_OFFSET  0xE000
#define AOD_BR_DSC0_OFFSET     0xD000

enum CLOCL_DIGIT {
	SECONDS = BIT(0),
	SECOND_TENS = BIT(1),
	MINUTES = BIT(2),
	MINUTE_TENS = BIT(3),
	HOURS = BIT(4),
	HOUR_TENS = BIT(5),
};

struct disp_input_config {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int fmt;
	unsigned int addr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_pitch;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h;
	unsigned int aen;
	unsigned char alpha;
	unsigned char time_digit;
	unsigned char blend_mode;
};

struct disp_frame_config {
	unsigned int interval; //ms
	unsigned int ulps_wakeup_prd;
	unsigned int layer_info_addr[6];
	unsigned int digits_addr[10];
};

struct disp_module_backup_info {
	char *module_name;
	unsigned int base;
	unsigned int size;
	unsigned int offset;
};

int aod_scp_tz_minuteswest;
static struct aod_scp_ipi_receive_info aod_scp_msg;
static int aod_state;
static unsigned int aod_scp_pic;
static unsigned int aod_scp_dgt_oft;
static unsigned int aod_scp_ulps_wakeup_prd;
static uint64_t dram_preloader_res_mem;
static uint64_t dram_addr_digits;
static struct mtk_aod_scp_cb aod_scp_cb;

static struct disp_module_backup_info module_list[] = {
	{"dsi0", 0x0, 0x0, AOD_BR_DSI0_OFFSET},
	{"dsc0", 0x0, 0x0, AOD_BR_DSC0_OFFSET},
	{"mipitx0", 0x0, 0x0, AOD_BR_MIPITX0_OFFSET},
};

#define AOD_STAT_SET(stat) (aod_state |= (stat))
#define AOD_STAT_CLR(stat) (aod_state &= ~(stat))
#define AOD_STAT_MATCH(stat) ((aod_state & (stat)) == (stat))

enum AOD_SCP_STATE {
	AOD_STAT_ENABLE = BIT(0),
	AOD_STAT_CONFIGED = BIT(1),
	AOD_STAT_ACTIVE = BIT(2),
};

enum OVL_INPUT_FORMAT {
	OVL_INPUT_FORMAT_BGR565     = 0,
	OVL_INPUT_FORMAT_RGB888     = 1,
	OVL_INPUT_FORMAT_RGBA8888   = 2,
	OVL_INPUT_FORMAT_ARGB8888   = 3,
	OVL_INPUT_FORMAT_VYUY       = 4,
	OVL_INPUT_FORMAT_YVYU       = 5,
	OVL_INPUT_FORMAT_RGB565     = 6,
	OVL_INPUT_FORMAT_BGR888     = 7,
	OVL_INPUT_FORMAT_BGRA8888   = 8,
	OVL_INPUT_FORMAT_ABGR8888   = 9,
	OVL_INPUT_FORMAT_UYVY       = 10,
	OVL_INPUT_FORMAT_YUYV       = 11,
	OVL_INPUT_FORMAT_RGBA4444   = 12,
	OVL_INPUT_FORMAT_BGRA4444   = 13,
	OVL_INPUT_FORMAT_UNKNOWN    = 32,
};

enum OVL_BLEND_MODE {
	BLD_MODE_NONE,
	BLD_MODE_PREMULTIPLIED,
	BLD_MODE_COVERAGE,
	BLD_MODE_SRC_ATOP,
	BLD_MODE_DST_ATOP,
	BLD_MODE_DST_OVER,
};

void mtk_module_backup(struct drm_crtc *crtc, unsigned int ulps_wakeup_prd)
{
	struct disp_frame_config *frame0;
	char *bkup_buf, *scp_sh_mem, *module_base;
	void __iomem *va = 0;
	int i, size;
	struct mtk_drm_private *priv;

	if (!AOD_STAT_MATCH(AOD_STAT_ENABLE) || !crtc)
		return;

	CFG_DISPLAY_WIDTH = crtc->state->mode.hdisplay;
	CFG_DISPLAY_HEIGHT = crtc->state->mode.vdisplay;
	CFG_DISPLAY_VREFRESH = drm_mode_vrefresh(&crtc->state->mode);

	if (crtc && crtc->dev && crtc->dev->dev_private) {
		priv = crtc->dev->dev_private;
		if (priv && priv->data)
			aod_mmsys_id = priv->data->mmsys_id;
	}

	scp_sh_mem = (char *)scp_get_reserve_mem_virt(SCP_AOD_MEM_ID);

	if (!scp_sh_mem) {
		DDPMSG("%s: Get shared memory fail\n", __func__);
		return;
	}

	frame0 = (void *)scp_sh_mem;
	frame0->ulps_wakeup_prd = ulps_wakeup_prd;
	aod_scp_ulps_wakeup_prd = ulps_wakeup_prd;
	DDPMSG("%s ulps_wakeup_prd %d %d\n",
		__func__, frame0->ulps_wakeup_prd, aod_scp_ulps_wakeup_prd);

	size = ARRAY_SIZE(module_list);

	for (i = 0; i < size; i++) {
		//DDPMSG("%s: %s base: 0x%x ,offset: 0x%x\n", __func__, module_list[i].module_name,
			//module_list[i].base, module_list[i].offset);

		if (!module_list[i].size)
			continue;

		va = ioremap(module_list[i].base, module_list[i].size);

		if (!va) {
			DDPMSG("%s ioremap error\n", __func__);
			return;
		}

		module_base = (char *)va;

		bkup_buf = scp_sh_mem + module_list[i].offset;
		memcpy(bkup_buf, module_base, module_list[i].size);

		iounmap(va);
	}

	AOD_STAT_SET(AOD_STAT_CONFIGED);
}

void mtk_module_backup_setup(struct device_node *node)
{
	uint16_t i, size = ARRAY_SIZE(module_list);
	char *name;
	struct of_phandle_args args;
	struct platform_device *module_pdev;
	struct resource *res = NULL;

	for (i = 0; i < size; i++) {
		name = module_list[i].module_name;

		if (!of_property_read_bool(node, name))
			continue;

		if (of_parse_phandle_with_fixed_args(node, name, 1, 0, &args)) {
			DDPMSG("%s: [%d] invalid property content\n", __func__, i);
			continue;
		}

		module_pdev = of_find_device_by_node(args.np);
		if (!module_pdev) {
			DDPMSG("%s: [%d] can't find module node\n", __func__, i);
			continue;
		}

		res = platform_get_resource(module_pdev, IORESOURCE_MEM, 0);

		module_list[i].base = (unsigned int)res->start;
		module_list[i].size = args.args[0];

		DDPMSG("%s: [%d] 0x%x, 0x%x\n", __func__,
			i, module_list[i].base, module_list[i].size);
	}
}

void mtk_prepare_config_map(void)
{
	char *scp_sh_mem;
	struct disp_frame_config *frame0;
	struct disp_input_config *input[6];
	int i;
	unsigned int aod_scp_pic_sz = 0;
	unsigned int dst_h = 0, dst_w = 0;

	if (!AOD_STAT_MATCH(AOD_STAT_CONFIGED | AOD_STAT_ACTIVE))
		DDPMSG("[AOD] %s: invalid state:0x%x\n", __func__, aod_state);

	DDPMSG("%s: aod:%d scp rsv mem 0x%llx(0x%llx), size 0x%llx\n", __func__, SCP_AOD_MEM_ID,
		scp_get_reserve_mem_virt(SCP_AOD_MEM_ID), scp_get_reserve_mem_phys(SCP_AOD_MEM_ID),
		scp_get_reserve_mem_size(SCP_AOD_MEM_ID));

	scp_sh_mem = (char *)scp_get_reserve_mem_virt(SCP_AOD_MEM_ID);

	/* prepare frame config map */
	frame0 = (void *)scp_sh_mem;
	scp_sh_mem += sizeof(struct disp_frame_config);
	input[0] = (void *)scp_sh_mem;
	scp_sh_mem += sizeof(struct disp_input_config);
	input[1] = (void *)scp_sh_mem;
	scp_sh_mem += sizeof(struct disp_input_config);
	input[2] = (void *)scp_sh_mem;
	scp_sh_mem += sizeof(struct disp_input_config);
	input[3] = (void *)scp_sh_mem;
	scp_sh_mem += sizeof(struct disp_input_config);
	input[4] = (void *)scp_sh_mem;

	dst_w = 180;
	dst_h = 180;

	memset(input[0], 0, sizeof(struct disp_input_config));
	input[0]->layer	= 0;
	input[0]->layer_en	= 1;
	input[0]->fmt		= OVL_INPUT_FORMAT_BGRA8888;
	input[0]->addr		= dram_preloader_res_mem;
	input[0]->src_x		= 0;
	input[0]->src_y		= 0;
	input[0]->src_pitch = CFG_DISPLAY_ALIGN_WIDTH*4;
	input[0]->dst_x		= 0;
	input[0]->dst_y		= 0;
	input[0]->dst_w		= CFG_DISPLAY_WIDTH;
	input[0]->dst_h		= CFG_DISPLAY_HEIGHT;
	input[0]->aen		= 1;
	input[0]->alpha		= 0xff;
	input[0]->time_digit = 0;
	input[0]->blend_mode = BLD_MODE_PREMULTIPLIED;

	memcpy(input[1], input[0], sizeof(struct disp_input_config));
	input[1]->layer	= 1;
	input[1]->layer_en	= 1;
	input[1]->addr	= 0x0;
	input[1]->src_pitch = dst_w * 4;
	input[1]->dst_x		= 120 * (2 * (input[1]->layer - 1) + 1);
	input[1]->dst_y		= 400;
	input[1]->dst_w		= dst_w;
	input[1]->dst_h		= dst_h;
	input[1]->time_digit = MINUTE_TENS;

	memcpy(input[2], input[1], sizeof(struct disp_input_config));
	input[2]->layer	= 2;
	input[2]->layer_en	= 1;
	input[2]->dst_x		= 120 * (2 * (input[2]->layer - 1) + 1);
	input[2]->time_digit = MINUTES;

	memcpy(input[3], input[1], sizeof(struct disp_input_config));
	input[3]->layer	= 3;
	input[3]->layer_en	= 1;
	input[3]->dst_x		= 120 * (2 * (input[3]->layer - 1) + 1);
	input[3]->time_digit  = SECOND_TENS;

	memcpy(input[4], input[1], sizeof(struct disp_input_config));
	input[4]->layer	= 4;
	input[4]->layer_en	= 1;
	input[4]->dst_x		= 120 * (2 * (input[4]->layer - 1) + 1);
	input[4]->time_digit = SECONDS;

	frame0->interval = 1000;
	frame0->ulps_wakeup_prd = aod_scp_ulps_wakeup_prd;

	/* PA for SCP. */
	frame0->layer_info_addr[0] = scp_get_reserve_mem_phys(SCP_AOD_MEM_ID) +
					sizeof(struct disp_frame_config);
	frame0->layer_info_addr[1] = frame0->layer_info_addr[0] + sizeof(struct disp_input_config);
	frame0->layer_info_addr[2] = frame0->layer_info_addr[1] + sizeof(struct disp_input_config);
	frame0->layer_info_addr[3] = frame0->layer_info_addr[2] + sizeof(struct disp_input_config);
	frame0->layer_info_addr[4] = frame0->layer_info_addr[3] + sizeof(struct disp_input_config);
	frame0->layer_info_addr[5] = 0x0;

	if (aod_scp_pic)
		aod_scp_pic_sz = LK_DGT_SZ_OFFSET;

	for (i = 0; i < 10; i++)
		frame0->digits_addr[i] = dram_addr_digits + aod_scp_pic_sz * i;
}

void mtk_aod_scp_get_time(void)
{
	struct rtc_time tm;
	struct rtc_time tm_android;
	struct timespec64 tv = {0};
	struct timespec64 tv_android = {0};

	ktime_get_real_ts64(&tv);
	tv_android = tv;
	rtc_time64_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= (uint64_t)sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	DDPMSG("%s UTC %02d:%02d:%02d\n", __func__, tm.tm_hour, tm.tm_min, tm.tm_sec);
	DDPMSG("%s Android %02d:%02d:%02d\n",
		__func__, tm_android.tm_hour, tm_android.tm_min, tm_android.tm_sec);
	aod_scp_tz_minuteswest = sys_tz.tz_minuteswest;

}

static DEFINE_MUTEX(spm_sema_lock);
static void __iomem *spm_base;
#define OFST_M2_SCP 0x6A4
#define OFST_M6_AP  0x6B4
#define KEY_HOLE    BIT(1)

/* API: use SPM HW semaphore to protect reading MFG_TOP_CFG */
int mtk_aod_scp_set_semaphore(bool lock)
{
	int i = 0;
	bool key = false;
	void __iomem *SPM_SEMA_AP = NULL, *SPM_SEMA_SCP = NULL;

	if (!spm_base) {
		DDPPR_ERR("%s, invalid spm base\n", __func__);
		goto fail;
	}

	SPM_SEMA_AP = spm_base + OFST_M6_AP;
	SPM_SEMA_SCP = spm_base + OFST_M2_SCP;

	mutex_lock(&spm_sema_lock);

	key = ((readl(SPM_SEMA_AP) & KEY_HOLE) == KEY_HOLE);
	if (key == lock) {
		DDPINFO("%s, skip %s sema\n", __func__, lock ? "get" : "put");
		mutex_unlock(&spm_sema_lock);
		return 1;
	}

	if (lock) {
		do {
			/* 10ms timeout */
			if (unlikely(++i > 1000))
				goto fail;
			writel(KEY_HOLE, SPM_SEMA_AP);
			udelay(10);
		} while ((readl(SPM_SEMA_AP) & KEY_HOLE) != KEY_HOLE);
	} else {
		writel(KEY_HOLE, SPM_SEMA_AP);
		do {
			/* 10ms timeout */
			if (unlikely(++i > 1000))
				goto fail;
			udelay(10);
		} while (readl(SPM_SEMA_AP) & KEY_HOLE);
	}

	mutex_unlock(&spm_sema_lock);
	return 1;

fail:
	DDPPR_ERR("%s: %s sema:0x%lx/0x%lx fail(0x%x), retry:%d\n",
		__func__, lock ? "get" : "put", (unsigned long)SPM_SEMA_AP,
		(unsigned long)SPM_SEMA_SCP, readl(SPM_SEMA_AP), i);
	return 0;
}

int mtk_aod_scp_set_semaphore_noirq(bool lock)
{
	int i = 0;
	bool key = false;
	void __iomem *SPM_SEMA_AP = NULL, *SPM_SEMA_SCP = NULL;

	if (!spm_base) {
		DDPPR_ERR("%s, invalid spm base\n", __func__);
		goto fail;
	}

	SPM_SEMA_AP = spm_base + OFST_M6_AP;
	SPM_SEMA_SCP = spm_base + OFST_M2_SCP;

	//mutex_lock(&spm_sema_lock);

	key = ((readl(SPM_SEMA_AP) & KEY_HOLE) == KEY_HOLE);
	if (key == lock) {
		DDPINFO("%s, skip %s sema\n", __func__, lock ? "get" : "put");
		//mutex_unlock(&spm_sema_lock);
		return 1;
	}

	if (lock) {
		do {
			/* 40ms timeout */
			if (unlikely(++i > 4000))
				goto fail;
			writel(KEY_HOLE, SPM_SEMA_AP);
			udelay(10);
		} while ((readl(SPM_SEMA_AP) & KEY_HOLE) != KEY_HOLE);
	} else {
		writel(KEY_HOLE, SPM_SEMA_AP);
		do {
			/* 10ms timeout */
			if (unlikely(++i > 1000))
				goto fail;
			udelay(10);
		} while (readl(SPM_SEMA_AP) & KEY_HOLE);
	}

	//mutex_unlock(&spm_sema_lock);

	return 1;
fail:
	DDPPR_ERR("%s: %s sema:0x%lx/0x%lx fail(0x%x), retry:%d\n",
		__func__, lock ? "get" : "put", (unsigned long)SPM_SEMA_AP,
		(unsigned long)SPM_SEMA_SCP, readl(SPM_SEMA_AP), i);
	return 0;
}

int mtk_aod_scp_ipi_send(int value)
{
	unsigned int retry_cnt = 0;
	int ret;

	DDPMSG("%s+\n", __func__);

	mtk_aod_scp_get_time();

	for (retry_cnt = 0; retry_cnt <= 10; retry_cnt++) {
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_SCP_AOD,
					0, &aod_scp_tz_minuteswest, 1, 0);

		if (ret == IPI_ACTION_DONE) {
			DDPMSG("%s ipi send msg done\n", __func__);
			break;
		}
	}

	if (ret != IPI_ACTION_DONE)
		DDPMSG("%s ipi send msg fail:%d\n", __func__, ret);

	return ret;
}

int mtk_aod_scp_doze_update(int doze)
{
	if (!AOD_STAT_MATCH(AOD_STAT_ENABLE))
		return 0;

	if (doze) {
		if (aod_mmsys_id == MMSYS_MT6989) {
			mtkfb_set_backlight_level_AOD(1800);
			AOD_STAT_SET(AOD_STAT_ACTIVE);
			mtk_prepare_config_map();
		} else {
			AOD_STAT_SET(AOD_STAT_ACTIVE);
			mtk_prepare_config_map();
		}
	} else
		AOD_STAT_CLR(AOD_STAT_ACTIVE);

	return 0;
}

static int mtk_aod_scp_recv_handler(unsigned int id, void *prdata, void *data, unsigned int len)
{
	DDPMSG("%s\n", __func__);

	return 0;
}

static int mtk_aod_scp_ipi_register(void)
{
	int ret;

	ret = mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_AOD,
			(void *)mtk_aod_scp_recv_handler, NULL, &aod_scp_msg);

	if (ret != IPI_ACTION_DONE)
		DDPMSG("%s resigter ipi fail: %d\n", __func__, ret);
	else
		DDPMSG("%s register ipi done\n", __func__);

	return ret;
}

static int mtk_aod_scp_probe(struct platform_device *pdev)
{
	struct device_node *aod_scp_node = NULL;
	struct device_node *smp_node = NULL;
	struct device_node *preloader_mem = NULL;
	struct platform_device *spm_pdev = NULL;
	struct resource *res = NULL;
	struct reserved_mem *rmem = NULL;
	unsigned int ret = 0;

	DDPMSG("%s+\n", __func__);

	aod_state = 0;

	aod_scp_node = of_find_node_by_name(NULL, "AOD-SCP-ON");
	if (aod_scp_node) {
		of_node_put(aod_scp_node);

		mtk_aod_scp_ipi_register();

		aod_scp_cb.send_ipi = mtk_aod_scp_doze_update;
		aod_scp_cb.module_backup = mtk_module_backup;
		mtk_aod_scp_ipi_init(&aod_scp_cb);

		smp_node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
		if (smp_node) {
			spm_pdev = of_find_device_by_node(smp_node);
			of_node_put(smp_node);
			if (!spm_pdev) {
				DDPPR_ERR("%s: invalid spm device\n", __func__);
				return 0;
			}

			res = platform_get_resource(spm_pdev, IORESOURCE_MEM, 0);

			spm_base = devm_ioremap(&spm_pdev->dev, res->start, resource_size(res));
			if (unlikely(!spm_base)) {
				DDPPR_ERR("%s: fail to ioremap SPM: 0x%llx", __func__, res->start);
				return 0;
			}
		}

		mtk_module_backup_setup(pdev->dev.of_node);

		AOD_STAT_SET(AOD_STAT_ENABLE);

		mtk_aod_scp_set_semaphore_noirq(1);

		// for 0~9 digit buffer
		preloader_mem = of_find_compatible_node(NULL, NULL, "mediatek,me_aod-scp_buf");
		ret = of_property_read_u32(pdev->dev.of_node, "aod_scp_pic", &aod_scp_pic);
		if (ret) {
			aod_scp_pic = 0;
			DDPMSG("%s can't find aod_scp_pic in FDT\n", __func__);
		} else
			DDPMSG("%s find aod_scp_pic %d in FDT\n", __func__, aod_scp_pic);

		ret = of_property_read_u32(pdev->dev.of_node, "aod_scp_dgt_oft", &aod_scp_dgt_oft);
		if (ret) {
			aod_scp_dgt_oft = DRAM_ADDR_DIGITS_OFFSET;
			DDPMSG("%s can't find aod_scp_dgt_oft in FDT\n", __func__);
		} else
			DDPMSG("%s find aod_scp_dgt_oft 0x%08x in FDT\n",
				__func__, aod_scp_dgt_oft);

		if (preloader_mem) {
			rmem = of_reserved_mem_lookup(preloader_mem);
			DDPMSG("rmem base : %pa, size : %pa\n", &rmem->base, &rmem->size);
			dram_preloader_res_mem = rmem->base;
			dram_addr_digits = rmem->base + DRAM_ADDR_DIGITS_OFFSET;
			DDPMSG("rmem base pa : %llx\n", dram_addr_digits);
		} else
			DDPMSG("%s can't find me_aod-scp_buf reserved memory", __func__);

		DDPMSG("%s: w/ mtk_aod_scp_ipi_register\n", __func__);
	}	else
		DDPMSG("%s: w/o mtk_aod_scp_ipi_register\n", __func__);

	DDPMSG("%s-\n", __func__);

	return 0;
}

static int aod_scp_suspend(struct device *dev)
{
	//DDPMSG("%s+\n", __func__);
	return 0;
}

static int aod_scp_resume(struct device *dev)
{
	//DDPMSG("%s+\n", __func__);
	return 0;
}

static int aod_scp_suspend_noirq(struct device *dev)
{
	if (AOD_STAT_MATCH(AOD_STAT_ACTIVE)) {
		mtk_aod_scp_ipi_send(0);
		mtk_oddmr_scp_status(1);
	}

	mtk_aod_scp_set_semaphore_noirq(0);
	return 0;
}

static int aod_scp_resume_noirq(struct device *dev)
{
	if (mtk_aod_scp_set_semaphore_noirq(1) == 0)
		DDPAEE("[AOD]:failed to get semaphore\n");
	else
		mtk_oddmr_scp_status(0);

	return 0;
}

static const struct dev_pm_ops aod_scp_pm_ops = {
	.suspend = aod_scp_suspend,
	.resume = aod_scp_resume,
	.suspend_noirq = aod_scp_suspend_noirq,
	.resume_noirq = aod_scp_resume_noirq,
};


static const struct of_device_id mtk_aod_scp_of_match[] = {
	{.compatible = "mediatek,aod_scp",},
	{},
};

struct platform_driver mtk_aod_scp_driver = {
	.probe = mtk_aod_scp_probe,
	.driver = {
				.name = "mtk-aod-scp",
				.of_match_table = mtk_aod_scp_of_match,
				.pm = &aod_scp_pm_ops,
			},
};

static int __init mtk_aod_scp_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_aod_scp_driver);
	if (ret < 0)
		DDPPR_ERR("Failed to register aod driver: %d\n", ret);

	return 0;
}

static void __exit mtk_aod_scp_exit(void)
{
}

module_init(mtk_aod_scp_init);
module_exit(mtk_aod_scp_exit);

MODULE_AUTHOR("Ahsin Chen <ahsin.chen@mediatek.com>");
MODULE_DESCRIPTION("Mediatek AOD-SCP");
MODULE_LICENSE("GPL v2");
