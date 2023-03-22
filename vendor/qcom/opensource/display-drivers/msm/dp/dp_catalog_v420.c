// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */


#include "dp_catalog.h"
#include "dp_reg.h"
#include "dp_debug.h"
#include "dp_pll.h"
#include <linux/rational.h>
#include <drm/drm_fixed.h>

#if defined(CONFIG_SECDP)
#include "secdp.h"
#endif

#define dp_catalog_get_priv_v420(x) ({ \
	struct dp_catalog *catalog; \
	catalog = container_of(x, struct dp_catalog, x); \
	container_of(catalog->sub, \
		struct dp_catalog_private_v420, sub); \
})

#define dp_read(x) ({ \
	catalog->sub.read(catalog->dpc, io_data, x); \
})

#define dp_write(x, y) ({ \
	catalog->sub.write(catalog->dpc, io_data, x, y); \
})

#define MAX_VOLTAGE_LEVELS 4
#define MAX_PRE_EMP_LEVELS 4

#if !defined(CONFIG_SECDP)
static u8 const vm_pre_emphasis[MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	{0x00, 0x0E, 0x16, 0xFF},       /* pe0, 0 db */
	{0x00, 0x0E, 0x16, 0xFF},       /* pe1, 3.5 db */
	{0x00, 0x0E, 0xFF, 0xFF},       /* pe2, 6.0 db */
	{0xFF, 0xFF, 0xFF, 0xFF}        /* pe3, 9.5 db */
};

/* voltage swing, 0.2v and 1.0v are not support */
static u8 const vm_voltage_swing[MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	{0x07, 0x0F, 0x16, 0xFF}, /* sw0, 0.4v  */
	{0x11, 0x1E, 0x1F, 0xFF}, /* sw1, 0.6 v */
	{0x1A, 0x1F, 0xFF, 0xFF}, /* sw1, 0.8 v */
	{0xFF, 0xFF, 0xFF, 0xFF}  /* sw1, 1.2 v, optional */
};

static u8 const dp_pre_emp_hbr2_hbr3[MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	{0x00, 0x0C, 0x15, 0x1B}, /* pe0, 0 db */
	{0x02, 0x0E, 0x16, 0xFF}, /* pe1, 3.5 db */
	{0x02, 0x11, 0xFF, 0xFF}, /* pe2, 6.0 db */
	{0x04, 0xFF, 0xFF, 0xFF}  /* pe3, 9.5 db */
};

static u8 const dp_swing_hbr2_hbr3[MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	{0x02, 0x12, 0x16, 0x1A}, /* sw0, 0.4v  */
	{0x09, 0x19, 0x1F, 0xFF}, /* sw1, 0.6v */
	{0x10, 0x1F, 0xFF, 0xFF}, /* sw1, 0.8v */
	{0x1F, 0xFF, 0xFF, 0xFF}  /* sw1, 1.2v */
};

static u8 const dp_pre_emp_hbr_rbr[MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	{0x00, 0x0D, 0x14, 0x1A}, /* pe0, 0 db */
	{0x00, 0x0E, 0x15, 0xFF}, /* pe1, 3.5 db */
	{0x00, 0x0E, 0xFF, 0xFF}, /* pe2, 6.0 db */
	{0x03, 0xFF, 0xFF, 0xFF}  /* pe3, 9.5 db */
};

static u8 const dp_swing_hbr_rbr[MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	{0x08, 0x0F, 0x16, 0x1F}, /* sw0, 0.4v */
	{0x11, 0x1E, 0x1F, 0xFF}, /* sw1, 0.6v */
	{0x16, 0x1F, 0xFF, 0xFF}, /* sw1, 0.8v */
	{0x1F, 0xFF, 0xFF, 0xFF}  /* sw1, 1.2v */
};

static const u8 dp_pre_emp_hbr_rbr_v600[MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	{0x00, 0x0D, 0x14, 0x1A}, /* pe0, 0 db */
	{0x00, 0x0E, 0x15, 0xFF}, /* pe1, 3.5 db */
	{0x00, 0x0E, 0xFF, 0xFF}, /* pe2, 6.0 db */
	{0x02, 0xFF, 0xFF, 0xFF}  /* pe3, 9.5 db */
};
#else
/* actual DP PHY params are read at each dtsi */
#endif


struct dp_catalog_private_v420 {
	struct device *dev;
	struct dp_catalog_sub sub;
	struct dp_catalog_io *io;
	struct dp_catalog *dpc;

#if defined(CONFIG_SECDP_DBG)
	char preshoot[DP_HW_PRESHOOT_MAX];
#endif
};

#if defined(CONFIG_SECDP_DBG)
struct dp_catalog_private_v420 *g_catalog_priv;

int secdp_catalog_preshoot_show(char *buf)
{
	struct dp_catalog_private_v420 *catalog_priv = g_catalog_priv;
	int  rc = 0;

	rc += scnprintf(buf + rc, PAGE_SIZE - rc,
			"%02x %02x\n",
			catalog_priv->preshoot[DP_HW_PRESHOOT_0],
			catalog_priv->preshoot[DP_HW_PRESHOOT_1]);

	return rc;
}

void secdp_catalog_preshoot_store(char *buf)
{
	struct dp_catalog_private_v420 *catalog_priv = g_catalog_priv;
	char *tok;
	u32  value;
	int  i, rc = 0;

	for (i = 0; i < DP_HW_PRESHOOT_MAX; i++) {
		tok = strsep(&buf, ",");
		if (!tok)
			continue;

		rc = kstrtouint(tok, 16, &value);
		if (rc) {
			DP_ERR("error: %s rc:%d\n", tok, rc);
			goto end;
		}

		catalog_priv->preshoot[i] = value;
	}
end:
	return;
}

static void _secdp_catalog_preshoot_init(struct dp_catalog_private_v420 *catalog)
{
	int i;

	for (i = 0; i < DP_HW_PRESHOOT_MAX; i++)
		catalog->preshoot[i] = 0xff;
}

static void _secdp_catalog_preshoot_adjust(
			struct dp_catalog_private_v420 *catalog)
{
	struct dp_io_data *io_data;
	int i;

	for (i = 0; i < DP_HW_PRESHOOT_MAX; i++) {
		if (catalog->preshoot[i] != 0xff) {
			if (i == DP_HW_PRESHOOT_0)
				io_data = catalog->io->dp_ln_tx0;
			else if (i == DP_HW_PRESHOOT_1)
				io_data = catalog->io->dp_ln_tx1;
			else
				DP_ERR("cannot be here\n");

			catalog->preshoot[i] |= BIT(5);

			/*
			 * USB3_DP_PHY_DP_QSERDES_TX0_PRE_EMPH
			 * USB3_DP_PHY_DP_QSERDES_TX1_PRE_EMPH
			 */
			dp_write(0x108, catalog->preshoot[i]);

			DP_INFO("%s 0x%02x write done!\n",
					secdp_preshoot_to_string(i),
					catalog->preshoot[i]);
		}
	}
}
#endif/*CONFIG_SECDP_DBG*/

static void dp_catalog_aux_setup_v420(struct dp_catalog_aux *aux,
		struct dp_aux_cfg *cfg)
{
	struct dp_catalog_private_v420 *catalog;
	struct dp_io_data *io_data;
	int i = 0;
	u32 phy_version;
	if (!aux || !cfg) {
		DP_ERR("invalid input\n");
		return;
	}

	DP_ENTER("\n");

	catalog = dp_catalog_get_priv_v420(aux);

	io_data = catalog->io->dp_phy;
	dp_write(DP_PHY_PD_CTL, 0x67);
	wmb(); /* make sure PD programming happened */

	phy_version = dp_catalog_get_dp_phy_version(catalog->dpc);
	if (phy_version >= 0x60000000) {
		/* Turn on BIAS current for PHY/PLL */
		io_data = catalog->io->dp_pll;
		dp_write(QSERDES_COM_BIAS_EN_CLKBUFLR_EN_V600, 0x17);
		wmb(); /* make sure BIAS programming happened */
	} else {
		/* Turn on BIAS current for PHY/PLL */
		io_data = catalog->io->dp_pll;
		dp_write(QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x17);
		wmb(); /* make sure BIAS programming happened */
	}

	io_data = catalog->io->dp_phy;
	/* DP AUX CFG register programming */
	for (i = 0; i < PHY_AUX_CFG_MAX; i++) {
		DP_DEBUG("%s: offset=0x%08x, value=0x%08x\n",
			dp_phy_aux_config_type_to_string(i),
			cfg[i].offset, cfg[i].lut[cfg[i].current_index]);
		dp_write(cfg[i].offset, cfg[i].lut[cfg[i].current_index]);
	}
	wmb(); /* make sure DP AUX CFG programming happened */

	dp_write(DP_PHY_AUX_INTERRUPT_MASK_V420, 0x1F);
}

static void dp_catalog_aux_clear_hw_int_v420(struct dp_catalog_aux *aux)
{
	struct dp_catalog_private_v420 *catalog;
	struct dp_io_data *io_data;
	u32 data = 0;
	u32 phy_version;
	if (!aux) {
		DP_ERR("invalid input\n");
		return;
	}

	catalog = dp_catalog_get_priv_v420(aux);
	phy_version = dp_catalog_get_dp_phy_version(catalog->dpc);
	io_data = catalog->io->dp_phy;
	if (phy_version >= 0x60000000)
		data = dp_read(DP_PHY_AUX_INTERRUPT_STATUS_V600);
	else
		data = dp_read(DP_PHY_AUX_INTERRUPT_STATUS_V420);

	dp_write(DP_PHY_AUX_INTERRUPT_CLEAR_V420, 0x1f);
	wmb(); /* make sure 0x1f is written before next write */

	dp_write(DP_PHY_AUX_INTERRUPT_CLEAR_V420, 0x9f);
	wmb(); /* make sure 0x9f is written before next write */

	dp_write(DP_PHY_AUX_INTERRUPT_CLEAR_V420, 0);
	wmb(); /* make sure register is cleared */
}

static void dp_catalog_panel_config_msa_v420(struct dp_catalog_panel *panel,
					u32 rate, u32 stream_rate_khz)
{
	u32 mvid, nvid, mvid_off = 0, nvid_off = 0;
	u32 const nvid_fixed = 0x8000;
	struct dp_catalog *dp_catalog;
	struct dp_catalog_private_v420 *catalog;
	struct dp_io_data *io_data;
	unsigned long num, den;
	u32 const input_scale = 10;
	u64 f1, f2;

	if (!panel || !rate) {
		DP_ERR("invalid input\n");
		return;
	}

	if (panel->stream_id >= DP_STREAM_MAX) {
		DP_ERR("invalid stream id:%d\n", panel->stream_id);
		return;
	}

	dp_catalog = container_of(panel, struct dp_catalog, panel);
	catalog = container_of(dp_catalog->sub, struct dp_catalog_private_v420, sub);

	/*
	 * MND calculator requires the target clock to be less than half the input clock. To meet
	 * this requirement, the input clock is scaled here and then the resulting M value is
	 * scaled by the same factor to offset the pre-scale.
	 */
	rational_best_approximation(rate * input_scale, stream_rate_khz,
			(unsigned long)(1 << 16) - 1,
			(unsigned long)(1 << 16) - 1, &den, &num);

	mvid = (num & 0xFFFF);
	nvid = (den & 0xFFFF);
	mvid *= input_scale;

	if (nvid < nvid_fixed) {
		f1 = drm_fixp_from_fraction(nvid_fixed, nvid);
		f2 = drm_fixp_from_fraction(mvid, 1);
		f1 = drm_fixp_mul(f1, f2);
		mvid = drm_fixp2int(f1);
		nvid = nvid_fixed;
	}

	io_data = catalog->io->dp_link;

	if (panel->stream_id == DP_STREAM_1) {
		mvid_off = DP1_SOFTWARE_MVID - DP_SOFTWARE_MVID;
		nvid_off = DP1_SOFTWARE_NVID - DP_SOFTWARE_NVID;
	}

	DP_DEBUG("pclk=%ld, lclk=%ld, mvid=0x%x, nvid=0x%x\n", stream_rate_khz, rate, mvid, nvid);
	dp_write(DP_SOFTWARE_MVID + mvid_off, mvid);
	dp_write(DP_SOFTWARE_NVID + nvid_off, nvid);
}

static void dp_catalog_ctrl_phy_lane_cfg_v420(struct dp_catalog_ctrl *ctrl,
		bool flipped, u8 ln_cnt)
{
	u32 info = 0x0;
	struct dp_catalog_private_v420 *catalog;
	struct dp_io_data *io_data;
	u8 orientation = BIT(!!flipped);

	if (!ctrl) {
		DP_ERR("invalid input\n");
		return;
	}

	catalog = dp_catalog_get_priv_v420(ctrl);
	io_data = catalog->io->dp_phy;

	info |= (ln_cnt & 0x0F);
	info |= ((orientation & 0x0F) << 4);
	DP_DEBUG("Shared Info = 0x%x\n", info);

	dp_write(DP_PHY_SPARE0_V420, info);
}

#if defined(CONFIG_SECDP)
static void secdp_catalog_ctrl_update_vx_px_v420(struct dp_catalog_ctrl *ctrl,
		u8 v_level, u8 p_level, bool high, u32 version,
		u8* value0, u8* value1)
{
	struct dp_catalog_private_v420 *catalog;
	struct dp_parser *parser;

	catalog = dp_catalog_get_priv_v420(ctrl);
	parser = catalog->dpc->parser;

	/*
	 * For DP controller versions >= 1.2.3
	 */
	if (version >= 0x10020003) {
		if (high) {
			*value0 = parser->dp_swing_hbr2_hbr3[v_level][p_level];
			*value1 = parser->dp_pre_emp_hbr2_hbr3[v_level][p_level];
		} else {
			*value0 = parser->dp_swing_hbr_rbr[v_level][p_level];
			*value1 = parser->dp_pre_emp_hbr_rbr[v_level][p_level];
		}
	} else {
		*value0 = parser->vm_voltage_swing[v_level][p_level];
		*value1 = parser->vm_pre_emphasis[v_level][p_level];
	}
}
#endif

static void dp_catalog_ctrl_update_vx_px_v420(struct dp_catalog_ctrl *ctrl,
		u8 v_level, u8 p_level, bool high)
{
	struct dp_catalog_private_v420 *catalog;
	struct dp_io_data *io_data;
	u8 value0, value1;
	u32 version;
	u32 phy_version;

	if (!ctrl || !((v_level < MAX_VOLTAGE_LEVELS)
		&& (p_level < MAX_PRE_EMP_LEVELS))) {
		DP_ERR("invalid input\n");
		return;
	}

	DP_DEBUG("hw: v=%d p=%d, high=%d\n", v_level, p_level, high);

	catalog = dp_catalog_get_priv_v420(ctrl);
	phy_version = dp_catalog_get_dp_phy_version(catalog->dpc);

	io_data = catalog->io->dp_ahb;
	version = dp_read(DP_HW_VERSION);
	DP_DEBUG("version: 0x%x\n", version);

#if !defined(CONFIG_SECDP)
	/*
	 * For DP controller versions >= 1.2.3
	 */
	if (version >= 0x10020003) {
		if (high) {
			value0 = dp_swing_hbr2_hbr3[v_level][p_level];
			value1 = dp_pre_emp_hbr2_hbr3[v_level][p_level];
		} else {
			value0 = dp_swing_hbr_rbr[v_level][p_level];
			if (phy_version >= 0x60000000)
				value1 = dp_pre_emp_hbr_rbr_v600[v_level][p_level];
			else
				value1 = dp_pre_emp_hbr_rbr[v_level][p_level];
		}
	} else {
		value0 = vm_voltage_swing[v_level][p_level];
		value1 = vm_pre_emphasis[v_level][p_level];
	}
#else
	secdp_catalog_ctrl_update_vx_px_v420(ctrl, v_level, p_level, high,
		version, &value0, &value1);

#if defined(SECDP_SELF_TEST)
	if (secdp_self_test_status(ST_VOLTAGE_TUN) >= 0) {
		u8 val = secdp_self_test_get_arg(ST_VOLTAGE_TUN)[v_level*4 + p_level];

		DP_INFO("[vx] value0: 0x%02x => 0x%02x\n", value0, val);
		value0 = val;
	}

	if (secdp_self_test_status(ST_PREEM_TUN) >= 0) {
		u8 val = secdp_self_test_get_arg(ST_PREEM_TUN)[v_level*4 + p_level];

		DP_INFO("[px] value0: 0x%02x => 0x%02x\n", value1, val);
		value1 = val;
	}
#endif
#endif/*CONFIG_SECDP*/

	/* program default setting first */
	io_data = catalog->io->dp_ln_tx0;
	dp_write(TXn_TX_DRV_LVL_V420, 0x2A);
	dp_write(TXn_TX_EMP_POST1_LVL, 0x20);

	io_data = catalog->io->dp_ln_tx1;
	dp_write(TXn_TX_DRV_LVL_V420, 0x2A);
	dp_write(TXn_TX_EMP_POST1_LVL, 0x20);

	/* Enable MUX to use Cursor values from these registers */
	value0 |= BIT(5);
	value1 |= BIT(5);

	/* Configure host and panel only if both values are allowed */
	if (value0 != 0xFF && value1 != 0xFF) {
		io_data = catalog->io->dp_ln_tx0;
		dp_write(TXn_TX_DRV_LVL_V420, value0);
		dp_write(TXn_TX_EMP_POST1_LVL, value1);

		io_data = catalog->io->dp_ln_tx1;
		dp_write(TXn_TX_DRV_LVL_V420, value0);
		dp_write(TXn_TX_EMP_POST1_LVL, value1);

		DP_DEBUG("hw: vx_value=0x%x px_value=0x%x\n",
			value0, value1);
	} else {
		DP_ERR("invalid vx (0x%x=0x%x), px (0x%x=0x%x\n",
			v_level, value0, p_level, value1);
	}

#if defined(CONFIG_SECDP_DBG)
	_secdp_catalog_preshoot_adjust(catalog);
#endif
}

static void dp_catalog_ctrl_lane_pnswap_v420(struct dp_catalog_ctrl *ctrl,
						u8 ln_pnswap)
{
	struct dp_catalog_private_v420 *catalog;
	struct dp_io_data *io_data;
	u32 cfg0, cfg1;

	catalog = dp_catalog_get_priv_v420(ctrl);

	cfg0 = 0x0a;
	cfg1 = 0x0a;

	cfg0 |= ((ln_pnswap >> 0) & 0x1) << 0;
	cfg0 |= ((ln_pnswap >> 1) & 0x1) << 2;
	cfg1 |= ((ln_pnswap >> 2) & 0x1) << 0;
	cfg1 |= ((ln_pnswap >> 3) & 0x1) << 2;

	io_data = catalog->io->dp_ln_tx0;
	dp_write(TXn_TX_POL_INV_V420, cfg0);

	io_data = catalog->io->dp_ln_tx1;
	dp_write(TXn_TX_POL_INV_V420, cfg1);
}

static void dp_catalog_put_v420(struct dp_catalog *catalog)
{
	struct dp_catalog_private_v420 *catalog_priv;

	if (!catalog)
		return;

	catalog_priv = container_of(catalog->sub,
			struct dp_catalog_private_v420, sub);
	devm_kfree(catalog_priv->dev, catalog_priv);
}

struct dp_catalog_sub *dp_catalog_get_v420(struct device *dev,
		struct dp_catalog *catalog, struct dp_catalog_io *io)
{
	struct dp_catalog_private_v420 *catalog_priv;

	if (!dev || !catalog) {
		DP_ERR("invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	DP_ENTER("\n");

	catalog_priv = devm_kzalloc(dev, sizeof(*catalog_priv), GFP_KERNEL);
	if (!catalog_priv)
		return ERR_PTR(-ENOMEM);

	catalog_priv->dev = dev;
	catalog_priv->io = io;
	catalog_priv->dpc = catalog;

	catalog_priv->sub.put      = dp_catalog_put_v420;

#if defined(CONFIG_SECDP_DBG)
	_secdp_catalog_preshoot_init(catalog_priv);
	g_catalog_priv = catalog_priv;
#endif

	catalog->aux.setup         = dp_catalog_aux_setup_v420;
	catalog->aux.clear_hw_interrupts = dp_catalog_aux_clear_hw_int_v420;
	catalog->panel.config_msa  = dp_catalog_panel_config_msa_v420;
	catalog->ctrl.phy_lane_cfg = dp_catalog_ctrl_phy_lane_cfg_v420;
	catalog->ctrl.update_vx_px = dp_catalog_ctrl_update_vx_px_v420;
	catalog->ctrl.lane_pnswap = dp_catalog_ctrl_lane_pnswap_v420;

	return &catalog_priv->sub;
}
