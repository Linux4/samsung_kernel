// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/interconnect.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6397/core.h>

#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif
#include "dvfsrc-helper.h"
#include "dvfsrc-common.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif

static struct mtk_dvfsrc *dvfsrc_drv;
static struct regmap *dvfsrc_regmap;

/* OPP */
#define MT_DVFSRC_OPP(_num_vcore, _num_ddr, _opp_table)	\
{	\
	.num_vcore_opp = _num_vcore,	\
	.num_dram_opp = _num_ddr,	\
	.opps = _opp_table,	\
	.num_opp = ARRAY_SIZE(_opp_table),	\
}

int (*dvfsrc_query_opp_info)(u32 id);
void register_dvfsrc_opp_handler(int (*handler)(u32 id))
{
	dvfsrc_query_opp_info = handler;
}

int mtk_dvfsrc_query_opp_info(u32 id)
{
	if (dvfsrc_query_opp_info != NULL)
		return dvfsrc_query_opp_info(id);

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_query_opp_info);

int mtk_dvfsrc_vcore_uv_table(u32 opp)
{
	u32 opp_idx;

	if (!dvfsrc_drv)
		return 0;

	if (opp >= dvfsrc_drv->opp_desc->num_vcore_opp)
		return 0;

	opp_idx = dvfsrc_drv->opp_desc->num_vcore_opp - opp - 1;

	return dvfsrc_drv->vopp_uv_tlb[opp_idx];
}
EXPORT_SYMBOL(mtk_dvfsrc_vcore_uv_table);

static void dvfsrc_setup_opp_table(struct mtk_dvfsrc *dvfsrc)
{
	int i;
	struct dvfsrc_opp *opp;
	struct arm_smccc_res ares;

	for (i = 0; i < dvfsrc->opp_desc->num_vcore_opp; i++) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_GET_VCORE_UV,
			i, 0, 0, 0, 0, 0,
			&ares);

		if (!ares.a0)
			dvfsrc->vopp_uv_tlb[i] = ares.a1;
	}

	for (i = 0; i < dvfsrc->opp_desc->num_vcore_opp; i++)
		dev_info(dvfsrc->dev, "dvfsrc vopp[%d] = %d\n",
			i, dvfsrc->vopp_uv_tlb[i]);

	for (i = 0; i < dvfsrc->opp_desc->num_opp; i++) {
		opp = &dvfsrc->opp_desc->opps[i];
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_GET_VCORE_UV,
			opp->vcore_opp, 0, 0, 0, 0, 0,
			&ares);

		if (!ares.a0)
			opp->vcore_uv = ares.a1;

		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_GET_DRAM_FREQ,
			opp->dram_opp, 0, 0, 0, 0, 0,
			&ares);
		if (!ares.a0)
			opp->dram_kbps = ares.a1;
	}
}

static int dvfsrc_query_sw_req_vcore_opp(struct mtk_dvfsrc *dvfsrc, int vcore_opp)
{
	int sw_req_opp;
	int sw_req = 0;
	int scp_req = 0;

	if (dvfsrc->force_opp_idx >= dvfsrc->opp_desc->num_opp) {
		mtk_dvfsrc_query_info(dvfsrc->dev->parent,
				      MTK_DVFSRC_CMD_VCORE_LEVEL_QUERY,
				      &sw_req);
		mtk_dvfsrc_query_info(dvfsrc->dev->parent,
				      MTK_DVFSRC_CMD_VSCP_LEVEL_QUERY,
				      &scp_req);
		sw_req_opp = (sw_req > scp_req) ? sw_req : scp_req;
		sw_req_opp = dvfsrc->opp_desc->num_vcore_opp - (sw_req_opp + 1);
		return sw_req_opp;
	}

	return vcore_opp;

}

int get_sw_req_vcore_opp(void)
{
	return mtk_dvfsrc_query_opp_info(MTK_DVFSRC_SW_REQ_VCORE_OPP);
}
EXPORT_SYMBOL(get_sw_req_vcore_opp);

static int dvfsrc_query_info(u32 id)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;
	const struct dvfsrc_opp *opp;
	int ret = 0;
	int level = 0;

	ret = mtk_dvfsrc_query_info(dvfsrc->dev->parent,
		MTK_DVFSRC_CMD_CURR_LEVEL_QUERY, &level);

	if (ret || level >= dvfsrc->opp_desc->num_opp)
		return 0;

	opp = &dvfsrc->opp_desc->opps[level];

	switch (id) {
	case MTK_DVFSRC_NUM_DVFS_OPP:
		ret = dvfsrc->opp_desc->num_opp;
		break;
	case MTK_DVFSRC_NUM_DRAM_OPP:
		ret = dvfsrc->opp_desc->num_dram_opp;
		break;
	case MTK_DVFSRC_NUM_VCORE_OPP:
		ret = dvfsrc->opp_desc->num_vcore_opp;
		break;
	case MTK_DVFSRC_CURR_DVFS_OPP:
		ret = dvfsrc->opp_desc->num_opp
				- (level + 1);
		break;
	case MTK_DVFSRC_CURR_DRAM_OPP:
		ret = dvfsrc->opp_desc->num_dram_opp
				- (opp->dram_opp + 1);
		break;
	case MTK_DVFSRC_CURR_VCORE_OPP:
		ret = dvfsrc->opp_desc->num_vcore_opp
				- (opp->vcore_opp + 1);
		break;
	case MTK_DVFSRC_CURR_DVFS_LEVEL:
		ret = level;
		break;
	case MTK_DVFSRC_CURR_DRAM_KHZ:
		ret = opp->dram_kbps;
		break;
	case MTK_DVFSRC_CURR_VCORE_UV:
		ret = opp->vcore_uv;
		break;
	case MTK_DVFSRC_SW_REQ_VCORE_OPP:
		ret = dvfsrc->opp_desc->num_vcore_opp - (opp->vcore_opp + 1);
		ret = dvfsrc_query_sw_req_vcore_opp(dvfsrc, ret);
		break;
	}

	return ret;
}

static int mtk_dvfsrc_opp_setting_v4(struct mtk_dvfsrc *dvfsrc)
{
	const struct dvfsrc_config *config;
	int i;
	u32 max_opp, info;

	config = dvfsrc->dvd->config;
	max_opp = config->query_opp_count(dvfsrc);
	dev_info(dvfsrc->dev, "max_opp = %d\n", max_opp);
	if (max_opp == 0)
		return -EINVAL;

	dvfsrc->opp_desc = &dvfsrc->dvd->opps_desc[0];
	dvfsrc->opp_desc->num_opp = max_opp + 1;
	info = config->query_opp_gear_info(dvfsrc, 0);
	dev_info(dvfsrc->dev, "info = %x, index = %d\n", info, 0);
	dvfsrc->opp_desc->num_vcore_opp = (info & 0x7) + 1;
	dvfsrc->opp_desc->num_emi_opp = ((info >> 4) & 0xF) + 1;
	dvfsrc->opp_desc->num_dram_opp = ((info >> 12) & 0xF) + 1;
	dev_info(dvfsrc->dev, "num_vcore_opp = %d\n", dvfsrc->opp_desc->num_vcore_opp);
	dev_info(dvfsrc->dev, "num_emi_opp = %d\n", dvfsrc->opp_desc->num_emi_opp);
	dev_info(dvfsrc->dev, "num_dram_opp = %d\n", dvfsrc->opp_desc->num_dram_opp);

	dvfsrc->opp_desc->opps = devm_kzalloc(dvfsrc->dev,
				dvfsrc->opp_desc->num_opp * sizeof(struct dvfsrc_opp_desc),
				GFP_KERNEL);

	for (i = 0; i < dvfsrc->opp_desc->num_opp; i++) {
		info = config->query_opp_gear_info(dvfsrc, max_opp - i);
		dvfsrc->opp_desc->opps[i].vcore_opp = info & 0x7;
		dvfsrc->opp_desc->opps[i].emi_opp = (info >> 4) & 0xF;
		dvfsrc->opp_desc->opps[i].dram_opp = (info >> 12) & 0xF;
	}

	dvfsrc->vopp_uv_tlb = devm_kzalloc(dvfsrc->dev,
				dvfsrc->opp_desc->num_vcore_opp * sizeof(u32),
				GFP_KERNEL);
	if (!dvfsrc->vopp_uv_tlb)
		return -ENOMEM;

	dvfsrc_setup_opp_table(dvfsrc);
	dvfsrc->force_opp_idx = 0xFF;

	return 0;
}

static int mtk_dvfsrc_opp_setting(struct mtk_dvfsrc *dvfsrc)
{
	struct arm_smccc_res ares;

	if (dvfsrc->dvd->num_opp_desc == 0)
		return mtk_dvfsrc_opp_setting_v4(dvfsrc);

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_GET_OPP_TYPE,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		dvfsrc->opp_type = ares.a1;
	else {
		dev_info(dvfsrc->dev, "get opp type fails\n");
		return ares.a0;
	}

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_VCOREFS_GET_FW_TYPE,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		dvfsrc->fw_type = ares.a1;
	else {
		dev_info(dvfsrc->dev, "get fw type fails\n");
		return ares.a0;
	}

	if (dvfsrc->opp_type > dvfsrc->dvd->num_opp_desc)
		return -EINVAL;

	dvfsrc->opp_desc = &dvfsrc->dvd->opps_desc[dvfsrc->opp_type];

	dvfsrc->vopp_uv_tlb = devm_kzalloc(dvfsrc->dev,
				dvfsrc->opp_desc->num_vcore_opp * sizeof(u32),
				GFP_KERNEL);
	if (!dvfsrc->vopp_uv_tlb)
		return -ENOMEM;

	dvfsrc_setup_opp_table(dvfsrc);
	dvfsrc->force_opp_idx = 0xFF;

	return 0;
}

/* OPP END */
/* DEBUG */
int (*query_debug_info_handle)(u32 id);
void register_dvfsrc_debug_handler(int (*handler)(u32 id))
{
	query_debug_info_handle = handler;
}
EXPORT_SYMBOL(register_dvfsrc_debug_handler);

int mtk_dvfsrc_query_debug_info(u32 id)
{
	if (query_debug_info_handle != NULL)
		return query_debug_info_handle(id);

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_query_debug_info);

/* Ceiling OPP */
int (*set_ceiling_opp_handle)(u8 user, u8 freq_opp);
void register_dvfsrc_ceiling_opp_handler(int (*handler)(u8 user, u8 freq_opp))
{
	set_ceiling_opp_handle = handler;
}

int mtk_dvfsrc_set_ceiling_freq(u8 user, u8 freq_opp)
{
	if (set_ceiling_opp_handle != NULL)
		return set_ceiling_opp_handle(user, freq_opp);

	return 0;
}

static int dvfsrc_query_debug_info(u32 id)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;
	const struct dvfsrc_config *config;
	int ret;

	config = dvfsrc_drv->dvd->config;
	ret = config->query_request(dvfsrc, id);

	return ret;
}

static DEFINE_MUTEX(ddr_ceil_mutex);
int mtk_dvfsrc_ceiling_opp(u8 user, u8 ddr_opp)
{
	int i;
	u8 max_freq_opp = 0, max_freq_gear;
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;
	const struct dvfsrc_config *config;

	if (user >= CEILING_ITEM_MAX){
		pr_info("ceiling wrong type user = %d\n", user);
		return -EINVAL;
	}

	if (ddr_opp >= dvfsrc->opp_desc->num_dram_opp)
		ddr_opp = 0;

	mutex_lock(&ddr_ceil_mutex);
	dvfsrc->ceil_ddr_opp[user] = ddr_opp;
	for (i = 0; i < CEILING_ITEM_MAX; i++)
		max_freq_opp = max_t(u8, dvfsrc->ceil_ddr_opp[user], max_freq_opp);

	max_freq_gear = dvfsrc->opp_desc->num_dram_opp - max_freq_opp - 1;
	config = dvfsrc_drv->dvd->config;
	if (max_freq_opp == 0)
		config->set_ddr_ceiling(dvfsrc, 0xFF);
	else
		config->set_ddr_ceiling(dvfsrc, max_freq_gear);

	mutex_unlock(&ddr_ceil_mutex);

	return 0;
}

#define DVFSRC_DEBUG_DUMP 0
#define DVFSRC_DEBUG_AEE 1
#define DVFSRC_DEBUG_VCORE_CHK 2

#define DVFSRC_AEE_LEVEL_ERROR 0
#define DVFSRC_AEE_FORCE_ERROR 1
#define DVFSRC_AEE_VCORE_CHK_ERROR 2
#define DVFSRC_AEE_TIMEOUT_ERROR 3

static char *dvfsrc_dump_info(struct mtk_dvfsrc *dvfsrc,
	char *p, u32 size)
{
	int vcore_uv = 0;
	char *buff_end = p + size;

	if (dvfsrc->vcore_power)
		vcore_uv = regulator_get_voltage(dvfsrc->vcore_power);

	p += snprintf(p, buff_end - p, "%-10s: %-8u uv\n",
			"Vcore", vcore_uv);
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	p += snprintf(p, buff_end - p, "%-10s: %-8u Mbps\n",
			"DDR", mtk_dramc_get_data_rate());
#endif
	p += snprintf(p, buff_end - p, "%-15s: %d\n",
			"FORCE_OPP_IDX",
			dvfsrc->force_opp_idx);
	p += snprintf(p, buff_end - p, "%-15s: %d\n",
			"CURR_DVFS_OPP",
			mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DVFS_OPP));
	p += snprintf(p, buff_end - p, "%-15s: %d\n",
			"CURR_VCORE_OPP",
			mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP));
	p += snprintf(p, buff_end - p, "%-15s: %d\n",
			"CURR_DRAM_OPP",
			mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP));
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static int dvfsrc_aee_trigger(struct mtk_dvfsrc *dvfsrc, u32 aee_type)
{
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	switch (aee_type) {
	case DVFSRC_AEE_LEVEL_ERROR:
		aee_kernel_warning("DVFSRC", "\nCRDISPATCH_KEY:DVFSRC\nLEVEL Change fail");
	break;
	case DVFSRC_AEE_FORCE_ERROR:
		aee_kernel_warning("DVFSRC", "\nCRDISPATCH_KEY:DVFSRC\nForce opp fail");
	break;
	case DVFSRC_AEE_VCORE_CHK_ERROR:
		aee_kernel_warning("DVFSRC", "\nCRDISPATCH_KEY:DVFSRC\nvcore check fail");
	break;
	case DVFSRC_AEE_TIMEOUT_ERROR:
		aee_kernel_warning("DVFSRC", "\nCRDISPATCH_KEY:DVFSRC\ntimeout fail");
	break;
	default:
		dev_info(dvfsrc->dev, "unknown aee type\n");
	break;
	}
#endif
	return NOTIFY_DONE;
}

static int dvfsrc_get_vcore_voltage(struct mtk_dvfsrc *dvfsrc)
{
	int ret;
	unsigned int regval = 0;

	if (!dvfsrc_regmap)
		return -EINVAL;

	ret = regmap_read(dvfsrc_regmap, dvfsrc->vcore_vsel_reg, &regval);
	if (ret != 0) {
		dev_info(dvfsrc->dev,
			"Failed to get vcore Buck vsel reg %x: %d\n",
			dvfsrc->vcore_vsel_reg, ret);
		return -EINVAL;
	}
	ret = (regval >> dvfsrc->vcore_vsel_shift) & dvfsrc->vcore_vsel_mask;
	ret = dvfsrc->vcore_range_min_uV + (dvfsrc->vcore_range_step * ret);

	return ret;
}

static int dvfsrc_vcore_check(struct mtk_dvfsrc *dvfsrc, u32 vcore_level)
{
	int vcore_uv = 0;
	int predict_uv;

	if (!dvfsrc->vchk_enable || !dvfsrc_regmap)
		return NOTIFY_DONE;

	if (vcore_level > dvfsrc->opp_desc->num_vcore_opp) {
		dev_info(dvfsrc->dev, "VCORE OPP ERROR = %d\n",
			vcore_level);
		return NOTIFY_BAD;
	}

	predict_uv = dvfsrc->vopp_uv_tlb[vcore_level];
	vcore_uv = dvfsrc_get_vcore_voltage(dvfsrc);

	if (vcore_uv < predict_uv) {
		dev_info(dvfsrc->dev, "VCORE CHECK FAIL= %d %d, %d\n",
			vcore_level, vcore_uv, predict_uv);
		return NOTIFY_BAD;
	}

	return NOTIFY_DONE;
}

static void dvfsrc_dump(struct mtk_dvfsrc *dvfsrc)
{
	char *p;
	ssize_t dump_size = DUMP_BUF_SIZE - 1;
	const struct dvfsrc_config *config;

	config = dvfsrc->dvd->config;
	mutex_lock(&dvfsrc->dump_lock);

	if (config->dump_spm_info && dvfsrc->spm_regs) {
		p = dvfsrc->dump_buf;
		config->dump_spm_info(dvfsrc, p, dump_size);
		pr_info("%s", dvfsrc->dump_buf);
	}

	if (config->dump_spm_timer_latch && dvfsrc->spm_regs && dvfsrc->dvd->spm_stamp_en) {
		p = dvfsrc->dump_buf;
		config->dump_spm_timer_latch(dvfsrc, p, dump_size);
		pr_info("%s", dvfsrc->dump_buf);
	}

	p = dvfsrc->dump_buf;
	config->dump_reg(dvfsrc, p, dump_size);
	pr_info("%s", dvfsrc->dump_buf);
	p = dvfsrc->dump_buf;
	config->dump_record(dvfsrc, p, dump_size);
	pr_info("%s", dvfsrc->dump_buf);

	mutex_unlock(&dvfsrc->dump_lock);
}

static int dvfsrc_debug_notifier_handler(struct notifier_block *b,
					 unsigned long l, void *v)
{
	int ret = NOTIFY_DONE;
	struct mtk_dvfsrc *dvfsrc;

	dvfsrc = container_of(b, struct mtk_dvfsrc, debug_notifier);

	switch (l) {
	case DVFSRC_DEBUG_DUMP:
		dvfsrc_dump(dvfsrc);
	break;
	case DVFSRC_DEBUG_AEE:
		ret = dvfsrc_aee_trigger(dvfsrc, *(u32 *) v);
	break;
	case DVFSRC_DEBUG_VCORE_CHK:
		ret = dvfsrc_vcore_check(dvfsrc, *(u32 *) v);
	break;
	default:
		dev_dbg(dvfsrc->dev, "unknown debug type\n");
	break;
	}

	return ret;
}

static void dvfsrc_debug_notifier_register(struct mtk_dvfsrc *dvfsrc)
{
	dvfsrc->debug_notifier.notifier_call = dvfsrc_debug_notifier_handler;
	register_dvfsrc_debug_notifier(&dvfsrc->debug_notifier);
}

static DEFINE_RATELIMIT_STATE(dvfsrc_ratelimit_force, 1 * HZ, 2);
static void dvfsrc_force_opp(struct mtk_dvfsrc *dvfsrc, u32 opp)
{
	if (dvfsrc->force_opp_idx != opp) {
		if (__ratelimit(&dvfsrc_ratelimit_force))
			pr_info("dvfsrc_force_opp\n");

		mtk_dvfsrc_send_request(dvfsrc->dev->parent,
			MTK_DVFSRC_CMD_FORCEOPP_REQUEST,
			opp);
	}
	dvfsrc->force_opp_idx = opp;
}

static void mtk_dvfsrc_get_perf_bw(struct mtk_dvfsrc *dvfsrc,
	struct device_node *np)
{
	int i;

	for (i = 0; i < dvfsrc->num_perf; i++) {
		dvfsrc->perfs_peak_bw[i] =
			dvfsrc_get_required_opp_peak_bw(np, i);
	}
}

int mtk_dvfsrc_set_vcore_avs(int enable)
{
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;
	const struct dvfsrc_config *config;

	if (!dvfsrc_drv)
		return 0;

	config = dvfsrc_drv->dvd->config;

	switch (dvfsrc->dvd->version) {
	case 0x6989:
	case 0x6897:
	case 0x6878:
		config->set_vcore_avs(dvfsrc, enable, 3);
	break;
	default:
	break;
	}

	if (!enable)
		udelay(2000);

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_set_vcore_avs);

static int mtk_dvfsrc_debug_setting(struct mtk_dvfsrc *dvfsrc)
{
	struct device_node *np = dvfsrc->dev->of_node;

	dvfsrc->num_perf = of_count_phandle_with_args(np,
		   "required-opps", NULL);

	if (dvfsrc->num_perf > 0) {
		dvfsrc->perfs_peak_bw = devm_kzalloc(dvfsrc->dev,
			 dvfsrc->num_perf * sizeof(u32),
			GFP_KERNEL);

		if (!dvfsrc->perfs_peak_bw)
			return -ENOMEM;

		mtk_dvfsrc_get_perf_bw(dvfsrc, np);
	} else {
		dvfsrc->num_perf = 0;
	}

	dvfsrc->vcore_power =
		devm_regulator_get_optional(dvfsrc->dev, "vcore");
	if (IS_ERR(dvfsrc->vcore_power)) {
		dev_info(dvfsrc->dev, "get vcore failed = %ld\n",
			PTR_ERR(dvfsrc->vcore_power));
		return PTR_ERR(dvfsrc->vcore_power);
	}

	dvfsrc->dvfsrc_vcore_power =
		devm_regulator_get_optional(dvfsrc->dev, "rc-vcore");
	if (IS_ERR(dvfsrc->dvfsrc_vcore_power)) {
		dev_info(dvfsrc->dev, "get dvfsrc_vcore failed = %ld\n",
			PTR_ERR(dvfsrc->dvfsrc_vcore_power));
		return PTR_ERR(dvfsrc->dvfsrc_vcore_power);
	}

	dvfsrc->bw_path = devm_of_icc_get(dvfsrc->dev, "icc-bw");
	if (IS_ERR(dvfsrc->bw_path)) {
		dev_info(dvfsrc->dev, "get icc-bw failed = %ld\n",
			PTR_ERR(dvfsrc->bw_path));
		return PTR_ERR(dvfsrc->bw_path);
	}

	dvfsrc->hrt_path = devm_of_icc_get(dvfsrc->dev, "icc-hrt-bw");
	if (IS_ERR(dvfsrc->hrt_path)) {
		dev_info(dvfsrc->dev, "get icc-hrt_bw failed = %ld\n",
			PTR_ERR(dvfsrc->hrt_path));
		return PTR_ERR(dvfsrc->hrt_path);
	}

	dvfsrc->perf_path = devm_of_icc_get(dvfsrc->dev, "icc-perf-bw");
	if (IS_ERR(dvfsrc->perf_path)) {
		dev_info(dvfsrc->dev, "get icc-perf_bw failed = %ld\n",
			PTR_ERR(dvfsrc->perf_path));
		return PTR_ERR(dvfsrc->perf_path);
	}

	dvfsrc->force_opp = dvfsrc_force_opp;
	dvfsrc->dump_info = dvfsrc_dump_info;

	return 0;
}

static int mtk_dvfsrc_mt6397_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);

	dvfsrc_regmap = mt6397->regmap;

	return 0;
}

static const struct of_device_id mtk_dvfsrc_mt6397_of_match[] = {
	{
		.compatible = "mediatek,mt6359p-dvfsrc-debug",
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_mt6397_driver = {
	.probe	= mtk_dvfsrc_mt6397_probe,
	.driver = {
		.name = "mtk-dvfsrc-vcore-debug",
		.of_match_table = of_match_ptr(mtk_dvfsrc_mt6397_of_match),
	},
};

static int mtk_dvfsrc_regmap_debug_setting(struct mtk_dvfsrc *dvfsrc)
{
	int ret;
	struct device_node *np = dvfsrc->dev->of_node;

	ret = of_property_read_u32(np, "vcore_vsel_reg", &dvfsrc->vcore_vsel_reg);
	if (ret)
		goto no_property;

	ret = of_property_read_u32(np, "vcore_vsel_mask", &dvfsrc->vcore_vsel_mask);
	if (ret)
		goto no_property;

	ret = of_property_read_u32(np, "vcore_vsel_shift", &dvfsrc->vcore_vsel_shift);
	if (ret)
		goto no_property;

	ret = of_property_read_u32(np, "vcore_range_min_uV", &dvfsrc->vcore_range_min_uV);
	if (ret)
		goto no_property;

	ret = of_property_read_u32(np, "vcore_range_step", &dvfsrc->vcore_range_step);
	if (ret)
		goto no_property;

	ret =  platform_driver_register(&mtk_dvfsrc_mt6397_driver);
	if (ret) {
		dev_info(dvfsrc->dev, "register regmap fail\n");
		goto no_property;
	}
	dvfsrc->vchk_enable = true;
	dev_info(dvfsrc->dev, "vcore checker is enabled\n");
	return 0;

no_property:
	dev_info(dvfsrc->dev, "vcore checker is disabled\n");
	return 0;
}
/* DEBUG END*/

static struct dvfsrc_opp dvfsrc_opp_mt6873_lp4[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{3, 6, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6873_desc[] = {
	MT_DVFSRC_OPP(4, 7, dvfsrc_opp_mt6873_lp4),
};

static const struct dvfsrc_debug_data mt6873_data = {
	.version = 0x6873,
	.config = &mt6873_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6873_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6873_desc),
};

static const struct dvfsrc_debug_data mt6853_data = {
	.version = 0x6853,
	.config = &mt6873_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6873_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6873_desc),
};

static const struct dvfsrc_debug_data mt6789_data = {
	.version = 0x6789,
	.config = &mt6873_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6873_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6873_desc),
};

static struct dvfsrc_opp dvfsrc_opp_mt6885_lp4[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{0, 3, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{3, 6, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6885_desc[] = {
	MT_DVFSRC_OPP(4, 7, dvfsrc_opp_mt6885_lp4),
};

static const struct dvfsrc_debug_data mt6885_data = {
	.version = 0x6885,
	.config = &mt6873_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6885_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6885_desc),
};

static struct dvfsrc_opp dvfsrc_opp_mt6893_lp4[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{0, 3, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{3, 6, 0, 0},
	{4, 6, 0, 0},
	{4, 7, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6893_desc[] = {
	MT_DVFSRC_OPP(5, 8, dvfsrc_opp_mt6893_lp4),
};

static const struct dvfsrc_debug_data mt6893_data = {
	.version = 0x6893,
	.config = &mt6893_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6893_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6893_desc),
};

static struct dvfsrc_opp dvfsrc_opp_mt6833_lp4[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{0, 3, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{1, 5, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{2, 6, 0, 0},
	{3, 6, 0, 0},
	{3, 7, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6833_desc[] = {
	MT_DVFSRC_OPP(4, 8, dvfsrc_opp_mt6833_lp4),
};

static const struct dvfsrc_debug_data mt6833_data = {
	.version = 0x6833,
	.config = &mt6873_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6833_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6833_desc),
};

static struct dvfsrc_opp dvfsrc_opp_mt6877_lp4[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{4, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{4, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{4, 2, 0, 0},
	{0, 3, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{4, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{4, 4, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{4, 5, 0, 0},
	{3, 6, 0, 0},
	{4, 6, 0, 0},
	{4, 7, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6877_desc[] = {
	MT_DVFSRC_OPP(5, 8, dvfsrc_opp_mt6877_lp4),
};

static const struct dvfsrc_debug_data mt6877_data = {
	.version = 0x6877,
	.config = &mt6877_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6877_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6877_desc),
};

static struct dvfsrc_opp dvfsrc_opp_mt6983[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{4, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{4, 1, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{4, 2, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{4, 3, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{4, 4, 0, 0},
	{3, 5, 0, 0},
	{4, 5, 0, 0},
	{3, 6, 0, 0},
	{4, 6, 0, 0},
	{4, 7, 0, 0},
	{4, 8, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6983_desc[] = {
	MT_DVFSRC_OPP(5, 9, dvfsrc_opp_mt6983),
};

static const struct dvfsrc_debug_data mt6983_data = {
	.version = 0x6983,
	.config = &mt6983_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6983_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6983_desc),
	.spm_stamp_en = true,
};

static const struct dvfsrc_debug_data mt6895_data = {
	.version = 0x6895,
	.config = &mt6983_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6983_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6983_desc),
};

static struct dvfsrc_opp dvfsrc_opp_mt6855[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{0, 3, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{0, 4, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{1, 5, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{2, 6, 0, 0},
	{3, 6, 0, 0},
	{3, 7, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6855_desc[] = {
	MT_DVFSRC_OPP(4, 8, dvfsrc_opp_mt6855),
};

static const struct dvfsrc_debug_data mt6855_data = {
	.version = 0x6855,
	.config = &mt6983_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6855_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6855_desc),
};

static struct dvfsrc_opp dvfsrc_opp_mt6879[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{4, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{4, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{4, 2, 0, 0},
	{0, 3, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{4, 3, 0, 0},
	{0, 4, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{4, 4, 0, 0},
	{1, 5, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{4, 5, 0, 0},
	{2, 6, 0, 0},
	{3, 6, 0, 0},
	{4, 6, 0, 0},
	{4, 7, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6879_desc[] = {
	MT_DVFSRC_OPP(5, 8, dvfsrc_opp_mt6879),
};

static const struct dvfsrc_debug_data mt6879_data = {
	.version = 0x6879,
	.config = &mt6983_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6879_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6879_desc),
};

static struct dvfsrc_opp dvfsrc_opp_mt6985[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{4, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{4, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{4, 2, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{4, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{4, 4, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{4, 5, 0, 0},
	{3, 6, 0, 0},
	{4, 6, 0, 0},
	{4, 7, 0, 0},
	{4, 8, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6985_desc[] = {
	MT_DVFSRC_OPP(5, 9, dvfsrc_opp_mt6985),
};

static const struct dvfsrc_debug_data mt6985_data = {
	.version = 0x6985,
	.config = &mt6983_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6985_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6985_desc),
	.spm_stamp_en = true,
};

static struct dvfsrc_opp dvfsrc_opp_mt6886[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{4, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{4, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{4, 2, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{4, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{4, 4, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{4, 5, 0, 0},
	{3, 6, 0, 0},
	{4, 6, 0, 0},
	{4, 7, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6886_desc[] = {
	MT_DVFSRC_OPP(5, 8, dvfsrc_opp_mt6886),
};

static const struct dvfsrc_debug_data mt6886_data = {
	.version = 0x6886,
	.config = &mt6983_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6886_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6886_desc),
};

static struct dvfsrc_opp dvfsrc_opp_mt6897_lp5x[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{4, 0, 0, 0},
	{5, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{4, 1, 0, 0},
	{5, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{4, 2, 0, 0},
	{5, 2, 0, 0},
	{0, 3, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{4, 3, 0, 0},
	{5, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{4, 4, 0, 0},
	{5, 4, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{4, 5, 0, 0},
	{5, 5, 0, 0},
	{3, 6, 0, 0},
	{4, 6, 0, 0},
	{5, 6, 0, 0},
	{4, 7, 0, 0},
	{5, 7, 0, 0},
	{4, 7, 0, 0},
	{5, 8, 0, 0},
	{4, 7, 0, 0},
	{5, 9, 0, 0},
};

static struct dvfsrc_opp dvfsrc_opp_mt6897_lp5[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{4, 0, 0, 0},
	{5, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{4, 1, 0, 0},
	{5, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{4, 2, 0, 0},
	{5, 2, 0, 0},
	{0, 3, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{4, 3, 0, 0},
	{5, 3, 0, 0},
	{1, 4, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{4, 4, 0, 0},
	{5, 4, 0, 0},
	{2, 5, 0, 0},
	{3, 5, 0, 0},
	{4, 5, 0, 0},
	{5, 5, 0, 0},
	{3, 6, 0, 0},
	{4, 6, 0, 0},
	{5, 6, 0, 0},
	{4, 7, 0, 0},
	{5, 7, 0, 0},
	{4, 8, 0, 0},
	{5, 8, 0, 0},
	{4, 9, 0, 0},
	{5, 9, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6897_desc[] = {
	MT_DVFSRC_OPP(6, 10, dvfsrc_opp_mt6897_lp5x),
	MT_DVFSRC_OPP(6, 10, dvfsrc_opp_mt6897_lp5),
};

static const struct dvfsrc_debug_data mt6897_data = {
	.version = 0x6897,
	.config = &mt6897_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6897_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6897_desc),
};

static struct dvfsrc_opp_desc dvfsrc_opp_common_desc[1];

static const struct dvfsrc_debug_data mt6989_data = {
	.version = 0x6989,
	.config = &mt6989_dvfsrc_config,
	.opps_desc = dvfsrc_opp_common_desc,
	.num_opp_desc = 0,
	.ceiling_support = true,
};

static struct dvfsrc_opp dvfsrc_opp_mt6878[] = {
	{0, 0, 0, 0},
	{1, 0, 0, 0},
	{2, 0, 0, 0},
	{3, 0, 0, 0},
	{0, 1, 0, 0},
	{1, 1, 0, 0},
	{2, 1, 0, 0},
	{3, 1, 0, 0},
	{0, 2, 0, 0},
	{1, 2, 0, 0},
	{2, 2, 0, 0},
	{3, 2, 0, 0},
	{1, 3, 0, 0},
	{2, 3, 0, 0},
	{3, 3, 0, 0},
	{2, 4, 0, 0},
	{3, 4, 0, 0},
	{3, 5, 0, 0},
	{3, 6, 0, 0},
};

static struct dvfsrc_opp_desc dvfsrc_opp_mt6878_desc[] = {
	MT_DVFSRC_OPP(4, 7, dvfsrc_opp_mt6878),
};

static const struct dvfsrc_debug_data mt6878_data = {
	.version = 0x6878,
	.config = &mt6897_dvfsrc_config,
	.opps_desc = dvfsrc_opp_mt6878_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6878_desc),
};


static const struct of_device_id dvfsrc_helper_of_match[] = {
	{
		.compatible = "mediatek,mt6789-dvfsrc",
		.data = &mt6789_data,
	}, {
		.compatible = "mediatek,mt6873-dvfsrc",
		.data = &mt6873_data,
	}, {
		.compatible = "mediatek,mt6853-dvfsrc",
		.data = &mt6853_data,
	}, {
		.compatible = "mediatek,mt6885-dvfsrc",
		.data = &mt6885_data,
	}, {
		.compatible = "mediatek,mt6893-dvfsrc",
		.data = &mt6893_data,
	}, {
		.compatible = "mediatek,mt6833-dvfsrc",
		.data = &mt6833_data,
	}, {
		.compatible = "mediatek,mt6877-dvfsrc",
		.data = &mt6877_data,
	}, {
		.compatible = "mediatek,mt6983-dvfsrc",
		.data = &mt6983_data,
	}, {
		.compatible = "mediatek,mt6895-dvfsrc",
		.data = &mt6895_data,
	}, {
		.compatible = "mediatek,mt6879-dvfsrc",
		.data = &mt6879_data,
	}, {
		.compatible = "mediatek,mt6855-dvfsrc",
		.data = &mt6855_data,
	}, {
		.compatible = "mediatek,mt6985-dvfsrc",
		.data = &mt6985_data,
	}, {
		.compatible = "mediatek,mt6886-dvfsrc",
		.data = &mt6886_data,
	}, {
		.compatible = "mediatek,mt6897-dvfsrc",
		.data = &mt6897_data,
	}, {
		.compatible = "mediatek,mt6989-dvfsrc",
		.data = &mt6989_data,
	}, {
		.compatible = "mediatek,mt6878-dvfsrc",
		.data = &mt6878_data,
	}, {
		/* sentinel */
	},
};

static int mtk_dvfsrc_helper_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct platform_device *parent_dev;
	struct resource *res;
	struct mtk_dvfsrc *dvfsrc;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	match = of_match_node(dvfsrc_helper_of_match, dev->parent->of_node);
	if (!match) {
		dev_info(dev, "invalid compatible string\n");
		return -ENODEV;
	}

	parent_dev = to_platform_device(dev->parent);
	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dvd = match->data;
	dvfsrc->dev = &pdev->dev;

	res = platform_get_resource_byname(parent_dev, IORESOURCE_MEM, "dvfsrc");
	if (!res) {
		dev_info(dev, "resource not found\n");
		return -ENODEV;
	}

	dvfsrc->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	res = platform_get_resource_byname(parent_dev,
			IORESOURCE_MEM, "spm");
	if (res) {
		dvfsrc->spm_regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(dvfsrc->spm_regs))
			dvfsrc->spm_regs = NULL;
	}

	ret = mtk_dvfsrc_opp_setting(dvfsrc);
	if (ret) {
		dev_info(dev, "dvfsrc opp setting fail\n");
		return ret;
	}

	ret = mtk_dvfsrc_debug_setting(dvfsrc);
	if (ret) {
		dev_info(dev, "dvfsrc debug setting fail\n");
		return ret;
	}
	mutex_init(&dvfsrc->dump_lock);
	dvfsrc_drv = dvfsrc;
	platform_set_drvdata(pdev, dvfsrc);
	mtk_dvfsrc_regmap_debug_setting(dvfsrc);
	register_dvfsrc_opp_handler(dvfsrc_query_info);
	dvfsrc_debug_notifier_register(dvfsrc);
	dvfsrc_register_sysfs(dev);
	register_dvfsrc_debug_handler(dvfsrc_query_debug_info);
	if (dvfsrc->dvd->ceiling_support) {
		dvfsrc->ceil_ddr_support = of_property_read_bool(np, "ceil-ddr-support");
		if (dvfsrc->ceil_ddr_support)
			register_dvfsrc_ceiling_opp_handler(mtk_dvfsrc_ceiling_opp);
	}

	return 0;
}

static int mtk_dvfsrc_helper_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dvfsrc *dvfsrc = platform_get_drvdata(pdev);

	unregister_dvfsrc_debug_notifier(&dvfsrc->debug_notifier);
	dvfsrc_unregister_sysfs(dev);
	platform_driver_unregister(&mtk_dvfsrc_mt6397_driver);
	dvfsrc_drv = NULL;
	return 0;
}

static const struct of_device_id mtk_dvfsrc_helper_of_match[] = {
	{
		.compatible = "mediatek,dvfsrc-helper",
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_helper_driver = {
	.probe	= mtk_dvfsrc_helper_probe,
	.remove	= mtk_dvfsrc_helper_remove,
	.driver = {
		.name = "mtk-dvfsrc-helper",
		.of_match_table = of_match_ptr(mtk_dvfsrc_helper_of_match),
	},
};

static int __init mtk_dvfsrc_helper_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_dvfsrc_helper_driver);
	if (ret)
		return ret;
#if IS_ENABLED(CONFIG_MTK_DVFSRC_MET)
	ret = mtk_dvfsrc_met_init();
	if (ret) {
		platform_driver_unregister(&mtk_dvfsrc_helper_driver);
		return ret;
	}
#endif
#if IS_ENABLED(CONFIG_MTK_DVFSRC_MB)
	ret = mtk_dvfsrc_mb_init();
	if (ret) {
		platform_driver_unregister(&mtk_dvfsrc_helper_driver);
		return ret;
	}
#endif
	return 0;
}
late_initcall_sync(mtk_dvfsrc_helper_init)

static void __exit mtk_dvfsrc_helper_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_helper_driver);
#if IS_ENABLED(CONFIG_MTK_DVFSRC_MET)
	mtk_dvfsrc_met_exit();
#endif
#if IS_ENABLED(CONFIG_MTK_DVFSRC_MB)
	mtk_dvfsrc_mb_exit();
#endif
}
module_exit(mtk_dvfsrc_helper_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC helper driver");
