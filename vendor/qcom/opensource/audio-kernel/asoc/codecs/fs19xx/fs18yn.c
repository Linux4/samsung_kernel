/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2019-01-29 File created.
 */

#include "fsm_public.h"
#if defined(CONFIG_FSM_FS18YN)

#define FS18YN_STATUS         0xF000

#define FS18YN_REVID          0xF003

#define FS18YN_TEMPSEL        0xF008
#define FS18YN_AUDIOCTRL      0xF006

#define FS18YN_SYSCTRL        0xF009
#define FS18YN_PWDN           0x0009
#define FS18YN_I2CR           0x0109
#define FS18YN_AMPEN          0x0309

#define FS18YN_I2SCTRL        0xF004
#define FS18YN_I2SSR          0x3C04
#define FS18YN_I2SDOE         0x0B04
#define FS18YN_CHS12          0x1304

#define FS18YN_CHIPINI        0xF090

#define FS18YN_DACEQWL        0xF0A2
#define FS18YN_DACEQA         0xF0A6

#define FS18YN_DACCTRL        0xF0AE

#define FS18YN_DIGSTAT        0xF0BD
#define FS18YN_DACRUN         0x01BD

#define FS18YN_BSTCTRL        0xF0C0
#define FS18YN_SSEND          0x0FC0

#define FS18YN_PLLCTRL1       0xF0C1
#define FS18YN_PLLCTRL2       0xF0C2
#define FS18YN_PLLCTRL3       0xF0C3
#define FS18YN_PLLCTRL4       0xF0C4

#define FS18YN_PRESET_EQ_LEN  (0x001C)
#define FS18HS_PRESET_EQ_LEN  (0x0044)

#define FS18YN_FW_NAME        "fs18yn.fsm"

static const struct fsm_srate g_fs18yn_srate_tbl[] = {
	{   8000, 0x0 },
	{  16000, 0x3 },
	{  32000, 0x8 },
	{  44100, 0x7 },
	{  48000, 0x8 },
};

const static fsm_pll_config_t g_fs18yn_pll_tbl[] = {
	/* bclk,    0xA1,   0xA2,   0xA3 */
	{  256000, 0x01A0, 0x0180, 0x0001 }, //  8000*16*2
	{  512000, 0x01A0, 0x0180, 0x0002 }, // 16000*16*2 &  8000*32*2
	{ 1024000, 0x0260, 0x0120, 0x0003 }, //            & 16000*32*2
	{ 1024032, 0x0260, 0x0120, 0x0003 }, // 32000*16*2+32
	{ 1411200, 0x01A0, 0x0100, 0x0004 }, // 44100*16*2
	{ 1536000, 0x0260, 0x0100, 0x0004 }, // 48000*16*2
	{ 2048032, 0x0260, 0x0120, 0x0006 }, //            & 32000*32*2+32
	{ 2822400, 0x01A0, 0x0100, 0x0008 }, //            & 44100*32*2
	{ 3072000, 0x0260, 0x0100, 0x0008 }, //            & 48000*32*2
	{ 6144000, 0x0260, 0x0100, 0x0010 }, // 96000*32*2 & 48000*32*4
	{12288000, 0x0260, 0x0100, 0x0020 }, //            & 48000*32*8
};

static int fs18yn_i2c_reset(fsm_dev_t *fsm_dev)
{
	uint16_t val;
	int ret;
	int i;

	if (!fsm_dev) {
		return -EINVAL;
	}

	for (i = 0; i < FSM_I2C_RETRY; i++) {
		fsm_reg_write(fsm_dev, REG(FS18YN_SYSCTRL), 0x0002); // reset nack
		fsm_delay_ms(10); // 10ms
		ret = fsm_reg_read(fsm_dev, REG(FS18YN_CHIPINI), &val);
		if ((val == 0x0003) || (val == 0x0300)) { // init finished
			break;
		}
	}
	if (i == FSM_I2C_RETRY) {
		pr_addr(err, "retry timeout");
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int fs18yn_get_srate_bits(fsm_dev_t *fsm_dev, uint32_t srate)
{
	int size;
	int idx;

	if (!fsm_dev) {
		return -EINVAL;
	}
	size = sizeof(g_fs18yn_srate_tbl)/ sizeof(struct fsm_srate);
	for (idx = 0; idx < size; idx++) {
		if (srate == g_fs18yn_srate_tbl[idx].srate)
			return g_fs18yn_srate_tbl[idx].bf_val;
	}
	pr_addr(err, "invalid srate:%d", srate);
	return -EINVAL;
}

static int fs18yn_write_preset_eq(fsm_dev_t *fsm_dev,
		int ram_id, uint16_t scene)
{
	uint8_t write_buf[sizeof(uint32_t)];
	preset_list_t *preset_list = NULL;
	dev_list_t *dev_list;
	int ret;
	int i;

	if ((fsm_dev->ram_scene[ram_id] != 0xFFFF)
			&& ((fsm_dev->ram_scene[ram_id] & scene) != 0)) {
		pr_addr(info, "RAM%d scene:%04X", ram_id,
				fsm_dev->ram_scene[ram_id]);
		return 0;
	}

	dev_list = fsm_dev->dev_list;
	if ((dev_list->eq_scenes & scene) == 0) {
		pr_addr(warning, "eq_scenes:%04X unmatched scene:%04X",
				dev_list->eq_scenes,scene);
		return 0;
	}
	for (i = 0; i < dev_list->len; i++) {
		if (dev_list->index[i].type != FSM_DSC_PRESET_EQ)
			continue;
		preset_list = (preset_list_t *)((uint8_t *)dev_list->index
				+ dev_list->index[i].offset);
		if (preset_list && preset_list->scene & scene)
			break;
	}
	// pr_addr(debug, "preset offset: %04x", index[i].offset);
	if (!preset_list || i >= dev_list->len) {
		pr_addr(err, "not found preset: %04X", scene);
		return -EINVAL;
	}

	if (preset_list->len != FS18YN_PRESET_EQ_LEN) {
		if ((HIGH8(fsm_dev->version) != FS18HS_DEV_ID) ||
				(preset_list->len != FS18HS_PRESET_EQ_LEN)) {
			pr_addr(err, "invalid size: %d", preset_list->len);
			return -EINVAL;
		}
	}

	ret = fsm_access_key(fsm_dev, 1);
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_DACEQA), 0x0000);
	for (i = 0; i < preset_list->len; i++) {
		convert_data_to_bytes(preset_list->data[i], write_buf);
		ret |= fsm_burst_write(fsm_dev, REG(FS18YN_DACEQWL),
				write_buf, sizeof(uint32_t));
	}
	ret |= fsm_access_key(fsm_dev, 0);
	if (!ret) {
		fsm_dev->ram_scene[ram_id] = preset_list->scene;
		pr_addr(info, "wroten ram_scene[%d]:%04X",
				ram_id, preset_list->scene);
	}

	FSM_FUNC_EXIT(ret);
	return ret;
}

static int fs18yn_switch_preset(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret;

	if (fsm_dev == NULL || fsm_dev->dev_list == NULL) {
		pr_err("Bad parameters");
		return -EINVAL;
	}

	ret = fsm_reg_write(fsm_dev, REG(FS18YN_PLLCTRL4), 0x000B);
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_SYSCTRL), 0x0000);
	fsm_delay_ms(20);
	ret |= fs18yn_write_preset_eq(fsm_dev, FSM_EQ_RAM0, cfg->next_scene);
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_SYSCTRL), 0x0001);
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_PLLCTRL4), 0x0000);
	if (!ret) {
		fsm_dev->cur_scene = cfg->next_scene;
		pr_addr(info, "switched scene:%04X", cfg->next_scene);
	} else {
		fsm_dev->cur_scene = FSM_SCENE_UNKNOW;
		pr_addr(err, "update scene[%04X] fail:%d", cfg->next_scene, ret);
	}

	FSM_FUNC_EXIT(ret);
	return ret;
}

static int fs18yn_reg_init(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret;

	if (fsm_dev == NULL || cfg == NULL) {
		return -EINVAL;
	}

	ret = fs18yn_i2c_reset(fsm_dev);
	fsm_dev->tcoef = 0x0188; /* Write init flag to 08h */
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_TEMPSEL), (fsm_dev->tcoef << 1));
	ret |= fsm_write_reg_tbl(fsm_dev, FSM_SCENE_COMMON);
	fsm_dev->cur_scene = FSM_SCENE_UNKNOW;
	fsm_dev->ram_scene[FSM_EQ_RAM0] = FSM_SCENE_UNKNOW;
	ret |= fs18yn_switch_preset(fsm_dev);
	ret |= fsm_write_reg_tbl(fsm_dev, cfg->next_scene);
	if (ret) {
		pr_addr(err, "init failed:%d", ret);
		fsm_dev->state.dev_inited = false;
	}

	return ret;
}

static int fs18yn_config_pll(fsm_dev_t *fsm_dev, bool on)
{
	fsm_config_t *cfg = fsm_get_config();
	const struct fsm_pll_config *pll_cfg;
	int tbl_size;
	int idx;
	int ret;

	if (!fsm_dev || !cfg) {
		return -EINVAL;
	}
	// config pll need disable pll firstly
	if (!on) { // disable pll
		ret = fsm_reg_write(fsm_dev, REG(FS18YN_PLLCTRL4), 0x0000);
		return ret;
	}

	pll_cfg = g_fs18yn_pll_tbl;
	tbl_size = ARRAY_SIZE(g_fs18yn_pll_tbl);
	for (idx = 0; idx < tbl_size; idx++) {
		if (pll_cfg[idx].bclk == cfg->i2s_bclk)
			break;
	}
	if (idx >= tbl_size) {
		pr_addr(err, "invalid bclk/rate:%d,%d", cfg->i2s_bclk, cfg->i2s_srate);
		return -EINVAL;
	}
	pr_addr(debug, "bclk[%d]: %d", idx, cfg->i2s_bclk);
	ret = fsm_access_key(fsm_dev, 1);

	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_PLLCTRL1), pll_cfg[idx].c1);
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_PLLCTRL2), pll_cfg[idx].c2);
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_PLLCTRL3), pll_cfg[idx].c3);
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_PLLCTRL4), 0x000B);

	ret |= fsm_access_key(fsm_dev, 0);

	FSM_FUNC_EXIT(ret);
	return ret;
}

static int fs18yn_config_i2s(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	uint16_t i2sctrl;
	int i2ssr;
	int ret;

	if (!fsm_dev || !cfg) {
		return -EINVAL;
	}
	i2ssr = fs18yn_get_srate_bits(fsm_dev, cfg->i2s_srate);
	if (i2ssr < 0) {
		pr_addr(err, "unsupport srate:%d", cfg->i2s_srate);
		return -EINVAL;
	}
	ret = fsm_reg_read(fsm_dev, REG(FS18YN_I2SCTRL), &i2sctrl);
	set_bf_val(&i2sctrl, FS18YN_I2SSR, i2ssr);
	//set_bf_val(&i2sctrl, FS18YN_I2SDOE, 1);
	pr_addr(debug, "srate:%d, val:%04X", cfg->i2s_srate, i2sctrl);
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_I2SCTRL), i2sctrl);
	ret |= fsm_swap_channel(fsm_dev, cfg->next_angle);

	return ret;
}

static int fs18yn_start_up(fsm_dev_t *fsm_dev)
{
	int ret;

	if (!fsm_dev) {
		return -EINVAL;
	}
	ret = fs18yn_config_i2s(fsm_dev);
	ret |= fs18yn_config_pll(fsm_dev, true);
	ret |= fsm_config_vol(fsm_dev);
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_SYSCTRL), 0x0008);
	fsm_delay_ms(10);
	fsm_dev->errcode = ret;

	return ret;
}

static int fs18yn_shut_down(fsm_dev_t *fsm_dev)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	ret = fsm_reg_write(fsm_dev, REG(FS18YN_SYSCTRL), 0x0001);
	fsm_delay_ms(25);
	ret |= fsm_reg_write(fsm_dev, REG(FS18YN_PLLCTRL4), 0x0000);

	return ret;
}

void fs18yn_ops(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!fsm_dev || !cfg) {
		return;
	}

	fsm_set_fw_name(FS18YN_FW_NAME);
	fsm_dev->dev_ops.reg_init = fs18yn_reg_init;
	fsm_dev->dev_ops.switch_preset = fs18yn_switch_preset;
	fsm_dev->dev_ops.start_up = fs18yn_start_up;
	fsm_dev->dev_ops.shut_down = fs18yn_shut_down;
	cfg->store_otp = false;
}
#endif
