#include "../pwrcal.h"
#include "../pwrcal-env.h"
#include "../pwrcal-clk.h"
#include "../pwrcal-pmu.h"
#include "../pwrcal-dfs.h"
#include "../pwrcal-rae.h"
#include "../pwrcal-asv.h"
#include "S5E7880-cmusfr.h"
#include "S5E7880-pmusfr.h"
#include "S5E7880-cmu.h"
#include "S5E7880-vclk-internal.h"

#ifdef PWRCAL_TARGET_LINUX
#include <soc/samsung/ect_parser.h>
#else
#include <mach/ect_parser.h>
#endif

extern int set_lit_volt;
extern int set_int_volt;
extern int set_mif_volt;
extern int set_g3d_volt;
extern int set_disp_volt;
extern int set_cam_volt;

static int common_get_margin_param(unsigned int target_type)
{
		int add_volt = 0;

		switch (target_type) {
		case dvfs_cpucl0:
			add_volt = set_lit_volt;
			break;
		case dvfs_cpucl1:
			add_volt = set_lit_volt;
			break;
		case dvfs_g3d:
			add_volt = set_g3d_volt;
			break;
		case dvfs_mif:
			add_volt = set_mif_volt;
			break;
		case dvfs_int:
			add_volt = set_int_volt;
			break;
		case dvfs_disp:
			add_volt = set_disp_volt;
			break;
		case dvfs_cam:
			add_volt = set_cam_volt;
			break;
		default:
			return add_volt;
		}
		return add_volt;
}

static struct dfs_switch dfscpucl0_switches[] = {
	{ 800000,	0,	0	},
	{ 400000,	0,	1	},
	{ 266000,	0,	2	},
	{ 200000,	0,	3	},
};

static struct dfs_table dfscpucl0_table = {

	.switches = dfscpucl0_switches,
	.num_of_switches = ARRAY_SIZE(dfscpucl0_switches),
	.switch_mux = CLK(CPUCL0_MUX_CLK_CPUCL0),
	.switch_use = 1,
	.switch_notuse = 0,
	.switch_src_div = CLK(CCORE_DIV_CLKCMU_CPUCL0_SWITCH),
	.switch_src_gate = CLK(CCORE_GATE_CLKCMU_CPUCL0_SWITCH),
	.switch_src_usermux = CLK(CPUCL0_MUX_CLKCMU_CPUCL0_SWITCH_USER),
};


struct pwrcal_clk_set dfscpucl0_en_list[] = {
	{CLK_NONE,	0,	-1},
};



static struct dfs_switch dfscpucl1_switches[] = {
	{ 800000,	0,	0	},
	{ 400000,	0,	1	},
	{ 266000,	0,	2	},
	{ 200000,	0,	3	},
};

static struct dfs_table dfscpucl1_table = {

	.switches = dfscpucl1_switches,
	.num_of_switches = ARRAY_SIZE(dfscpucl1_switches),
	.switch_mux = CLK(CPUCL1_MUX_CLK_CPUCL1),
	.switch_use = 1,
	.switch_notuse = 0,
	.switch_src_div = CLK(CCORE_DIV_CLKCMU_CPUCL1_SWITCH),
	.switch_src_gate = CLK(CCORE_GATE_CLKCMU_CPUCL1_SWITCH),
	.switch_src_usermux = CLK(CPUCL1_MUX_CLKCMU_CPUCL1_SWITCH_USER),
};

struct pwrcal_clk_set dfscpucl1_en_list[] = {
	{CLK_NONE,	0,	-1},
};



static struct dfs_switch dfsg3d_switches[] = {
	{ 800000,	0,	0	},
	{ 400000,	0,	1	},
	{ 266000,	0,	2	},
	{ 200000,	0,	3	},
};

static struct dfs_table dfsg3d_table = {

	.switches = dfsg3d_switches,
	.num_of_switches = ARRAY_SIZE(dfsg3d_switches),
	.switch_mux = CLK(G3D_MUX_CLK_G3D),
	.switch_use = 1,
	.switch_notuse = 0,
	.switch_src_div = CLK(CCORE_DIV_CLKCMU_G3D_SWITCH),
	.switch_src_gate = CLK(CCORE_GATE_CLKCMU_G3D_SWITCH),
	.switch_src_usermux = CLK(G3D_MUX_CLKCMU_G3D_SWITCH_USER),
};

struct pwrcal_clk_set dfsg3d_en_list[] = {
//	{CLK(G3D_PLL),	100000,	-1},
	{CLK_NONE,	0,	-1},
};

extern void pwrcal_dmc_set_dvfs(unsigned long long target_mif_freq, unsigned int timing_set_idx);
extern void pwrcal_dmc_set_pre_dvfs(void);
extern void pwrcal_dmc_set_post_dvfs(unsigned long long target_freq);
//extern void pwrcal_dmc_set_vtmon_on_swithing(void);
extern void pwrcal_dmc_set_refresh_method_pre_dvfs(unsigned long long current_rate, unsigned long long target_rate);
extern void pwrcal_dmc_set_refresh_method_post_dvfs(unsigned long long current_rate, unsigned long long target_rate);
//extern void pwrcal_dmc_set_dsref_cycle(unsigned long long target_rate);
extern unsigned int pwrcal_dmc_get_pchannel_state(int channel);

static int pwrcal_clk_set_mif_pause_enable(int enable)
{
	pwrcal_writel(PAUSE, (enable<<0)); /* CMU Pause enable */
	return 0;
}

static int pwrcal_clk_wait_mif_pause(void)
{
	int timeout;
	unsigned int status;

	for (timeout = 0;; timeout++) {
		status = pwrcal_dmc_get_pchannel_state(0);
		if (status == 0x0)
			break;

		if (timeout > CLK_WAIT_CNT)
			pr_err("PAUSE staus(0x%X) is not stable", status);

		cpu_relax();
	}

	for (timeout = 0;; timeout++) {
		status = pwrcal_dmc_get_pchannel_state(1);
		if (status == 0x0)
			break;

		if (timeout > CLK_WAIT_CNT)
			pr_err("PAUSE staus(0x%X) is not stable", status);

		cpu_relax();
	}

	return 0;
}

//static int is_dll_on_status = 1;

static void dfsmif_trans_pre(unsigned int rate_from, unsigned int rate_to)
{
	//unsigned long long from, to;

	//is_dll_on_status = 1;

	//from = (unsigned long long)rate_from * 1000;
	//to = (unsigned long long)rate_to * 1000;

	//pwrcal_dmc_set_refresh_method_pre_dvfs(from, to);
	//pwrcal_clk_set_mif_pause_enable(1);

	/* VTMON disable before MIF DFS sequence*/
	pwrcal_dmc_set_pre_dvfs();
}

static void dfsmif_trans_post(unsigned int rate_from, unsigned int rate_to)
{
	//unsigned long long from, to;
	unsigned long long to;

	//from = (unsigned long long)rate_from * 1000;
	to = (unsigned long long)rate_to * 1000;

	/* VTMON enable before MIF DFS sequence*/
	pwrcal_dmc_set_post_dvfs(to);

	//pwrcal_dmc_set_refresh_method_post_dvfs(from, to);
	//pwrcal_dmc_set_dsref_cycle(to);

//	if (rate_to >= 416000)
//		is_dll_on_status = 1;
//	else
//		is_dll_on_status = 0;
}

static void dfsmif_switch_pre(unsigned int rate_from, unsigned int rate_to)
{
	static unsigned int paraset;
	unsigned long long rate;

	paraset = (paraset + 1) % 2;
	rate = (unsigned long long)rate_to * 1000;
	pwrcal_dmc_set_dvfs(rate, paraset);
	pwrcal_clk_set_mif_pause_enable(1);
}

static void dfsmif_switch_post(unsigned int rate_from, unsigned int rate_to)
{
	pwrcal_clk_wait_mif_pause();
	pwrcal_clk_set_mif_pause_enable(0);
}

static inline unsigned int dfsmif_set_rate_switch(unsigned int rate_switch, struct dfs_table *table)
{
	int switch_index;

	switch_index = 0;

	pwrcal_div_set_ratio(table->switch_src_div, table->switches[switch_index].div_value + 1);
	pwrcal_mux_set_src(table->switch_src_mux, table->switches[switch_index].mux_value);
	return table->switches[switch_index].switch_rate;
}

#if 0
static void hwacg_mif(unsigned int enable)
{
	if (enable) {
		//Q-channel enable - MIF

		//disables manual control mode
		pwrcal_setf(CG_CTRL_MAN_ACLK_MIF0, 0, 0x7FFF, 0);
		pwrcal_setf(CG_CTRL_MAN_ACLK_MIF0_SECURE_DREX_TZ, 0, 0x1, 0);
		pwrcal_setf(CG_CTRL_MAN_PCLK_MIF0_SECURE_DREX_TZ, 0, 0x1, 0);
		pwrcal_setf(CG_CTRL_MAN_PCLK_MIF0, 0, 0x7F, 0);
		pwrcal_setf(CG_CTRL_MAN_SCLK_RCLK_DREX_MIF0, 0, 0x1, 0);
		//disables manual control mode
		pwrcal_setf(CG_CTRL_MAN_ACLK_MIF1, 0, 0x7FFF, 0);
		pwrcal_setf(CG_CTRL_MAN_ACLK_MIF1_SECURE_DREX_TZ, 0, 0x1, 0);
		pwrcal_setf(CG_CTRL_MAN_PCLK_MIF1_SECURE_DREX_TZ, 0, 0x1, 0);
		pwrcal_setf(CG_CTRL_MAN_PCLK_MIF1, 0, 0x7F, 0);
		pwrcal_setf(CG_CTRL_MAN_SCLK_RCLK_DREX_MIF1, 0, 0x1, 0);
		//CNT_EXPIRE_VALUE
		pwrcal_setf(QCH_CTRL_LH_P_MIF0, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_PMU_MIF0, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_SYSREG_MIF0, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_PPMU_DMC_CPU_MIF0, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_LH_DMC_CPU_MIF0, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_LH_DMC_BUSD_MIF0, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_CMU_MIF0, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(PCH_CTRL_DREX_PCH_1_MIF0, 0, 0xFFFFFF, 0x7FF);
		pwrcal_setf(PCH_CTRL_DREX_PCH_1_MIF0, 24, 0xF, 0x4); // Design guide corner case
		//CNT_EXPIRE_VALUE
		pwrcal_setf(QCH_CTRL_LH_P_MIF1, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_PMU_MIF1, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_SYSREG_MIF1, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_PPMU_DMC_CPU_MIF1, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_LH_DMC_CPU_MIF1, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_LH_DMC_BUSD_MIF1, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(QCH_CTRL_CMU_MIF1, 16, 0xFFFF, 0x7FF);
		pwrcal_setf(PCH_CTRL_DREX_PCH_1_MIF1, 0, 0xFFFFFF, 0x7FF);
		pwrcal_setf(PCH_CTRL_DREX_PCH_1_MIF1, 24, 0xF, 0x4);
		// enables HWACG
		pwrcal_setbit(QCH_CTRL_LH_P_MIF0, 0, 1);
		pwrcal_setbit(QCH_CTRL_PMU_MIF0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SYSREG_MIF0, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_DMC_CPU_MIF0, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_DMC_CPU_MIF0, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_DMC_BUSD_MIF0, 0, 1);
		pwrcal_setbit(QCH_CTRL_CMU_MIF0, 0, 1);
		pwrcal_setbit(QSTATE_CTRL_DREX_RCLK_MIF0, 0, 1);
		//P-channel enable
		pwrcal_setbit(PCH_CTRL_DREX_PCH_0_MIF0, 0, 1);
		// enables HWACG
		pwrcal_setbit(QCH_CTRL_LH_P_MIF1, 0, 1);
		pwrcal_setbit(QCH_CTRL_PMU_MIF1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SYSREG_MIF1, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_DMC_CPU_MIF1, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_DMC_CPU_MIF1, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_DMC_BUSD_MIF1, 0, 1);
		pwrcal_setbit(QCH_CTRL_CMU_MIF1, 0, 1);
		pwrcal_setbit(QSTATE_CTRL_DREX_RCLK_MIF1, 0, 1);
		//P-channel enable
		pwrcal_setbit(PCH_CTRL_DREX_PCH_0_MIF1, 0, 1);
	} else {
		// disables HWACG
		// 0
		pwrcal_setbit(QCH_CTRL_LH_P_MIF0, 0, 0);
		pwrcal_setbit(QCH_CTRL_PMU_MIF0, 0, 0);
		pwrcal_setbit(QCH_CTRL_SYSREG_MIF0, 0, 0);
		pwrcal_setbit(QCH_CTRL_PPMU_DMC_CPU_MIF0, 0, 0);
		pwrcal_setbit(QCH_CTRL_LH_DMC_CPU_MIF0, 0, 0);
		pwrcal_setbit(QCH_CTRL_LH_DMC_BUSD_MIF0, 0, 0);
		pwrcal_setbit(QCH_CTRL_CMU_MIF0, 0, 0);
		pwrcal_setbit(QSTATE_CTRL_DREX_RCLK_MIF0, 0, 0);
		pwrcal_setbit(PCH_CTRL_DREX_PCH_0_MIF0, 0, 0);
		// 1
		pwrcal_setbit(QCH_CTRL_LH_P_MIF1, 0, 0);
		pwrcal_setbit(QCH_CTRL_PMU_MIF1, 0, 0);
		pwrcal_setbit(QCH_CTRL_SYSREG_MIF1, 0, 0);
		pwrcal_setbit(QCH_CTRL_PPMU_DMC_CPU_MIF1, 0, 0);
		pwrcal_setbit(QCH_CTRL_LH_DMC_CPU_MIF1, 0, 0);
		pwrcal_setbit(QCH_CTRL_LH_DMC_BUSD_MIF1, 0, 0);
		pwrcal_setbit(QCH_CTRL_CMU_MIF1, 0, 0);
		pwrcal_setbit(QSTATE_CTRL_DREX_RCLK_MIF1, 0, 0);
		pwrcal_setbit(PCH_CTRL_DREX_PCH_0_MIF1, 0, 0);
		// Set CG_CTRL_VAL_* to proper value
		pwrcal_setf(CG_CTRL_VAL_ACLK_MIF0, 0, 0x7FFF, 0x7FFF);
		pwrcal_setf(CG_CTRL_VAL_ACLK_MIF0_SECURE_DREX_TZ, 0, 0x1, 0x1);
		pwrcal_setf(CG_CTRL_VAL_PCLK_MIF0_SECURE_DREX_TZ, 0, 0x1, 0x1);
		pwrcal_setf(CG_CTRL_VAL_PCLK_MIF0, 0, 0x7F, 0x7F);
		pwrcal_setf(CG_CTRL_VAL_SCLK_RCLK_DREX_MIF0, 0, 0x1, 0x1);
		// Set CG_CTRL_VAL_* to proper value
		pwrcal_setf(CG_CTRL_VAL_ACLK_MIF1, 0, 0x7FFF, 0x7FFF);
		pwrcal_setf(CG_CTRL_VAL_ACLK_MIF1_SECURE_DREX_TZ, 0, 0x1, 0x1);
		pwrcal_setf(CG_CTRL_VAL_PCLK_MIF1_SECURE_DREX_TZ, 0, 0x1, 0x1);
		pwrcal_setf(CG_CTRL_VAL_PCLK_MIF1, 0, 0x7F, 0x7F);
		pwrcal_setf(CG_CTRL_VAL_SCLK_RCLK_DREX_MIF1, 0, 0x1, 0x1);
		//enables manual control mode
		pwrcal_setf(CG_CTRL_MAN_ACLK_MIF0, 0, 0x7FFF, 0x7FFF);
		pwrcal_setf(CG_CTRL_MAN_ACLK_MIF0_SECURE_DREX_TZ, 0, 0x1, 0x1);
		pwrcal_setf(CG_CTRL_MAN_PCLK_MIF0_SECURE_DREX_TZ, 0, 0x1, 0x1);
		pwrcal_setf(CG_CTRL_MAN_PCLK_MIF0, 0, 0x7F, 0x7F);
		pwrcal_setf(CG_CTRL_MAN_SCLK_RCLK_DREX_MIF0, 0, 0x1, 0x1);
		//enables manual control mode
		pwrcal_setf(CG_CTRL_MAN_ACLK_MIF1, 0, 0x7FFF, 0x7FFF);
		pwrcal_setf(CG_CTRL_MAN_ACLK_MIF1_SECURE_DREX_TZ, 0, 0x1, 0x1);
		pwrcal_setf(CG_CTRL_MAN_PCLK_MIF1_SECURE_DREX_TZ, 0, 0x1, 0x1);
		pwrcal_setf(CG_CTRL_MAN_PCLK_MIF1, 0, 0x7F, 0x7F);
		pwrcal_setf(CG_CTRL_MAN_SCLK_RCLK_DREX_MIF1, 0, 0x1, 0x1);
	}
	return;
}
#endif

static int dfsmif_transition_switch(unsigned int rate_from, unsigned int rate_switch, struct dfs_table *table)
{
	int lv_from, lv_switch;
	unsigned int aclk_mif_pll, bus_pll_user, cur_switch_rate;

	if (rate_from == rate_switch)
		return 0;

	aclk_mif_pll = pwrcal_mux_get_src(table->switch_mux);

	if (aclk_mif_pll == 1) {
		bus_pll_user = pwrcal_mux_get_src(table->switch_src_usermux);

		if (bus_pll_user == 1) {
			cur_switch_rate = (unsigned int)(800000);

			if (rate_switch == cur_switch_rate)
				return 0;
			else
				goto errorout;
		}
	}

	lv_from = dfs_get_lv(rate_from, table);

	if (lv_from >= table->num_of_lv)
		goto errorout;

	dfsmif_set_rate_switch(rate_switch, table);

	lv_switch = dfs_get_lv(rate_switch, table) - 1;	/* switch is not regular level */

	dfsmif_trans_pre(rate_from, rate_switch);

	if (dfs_enable_switch(table))
		goto errorout;

	if (dfs_trans_div(lv_from, lv_switch, table, TRANS_HIGH)) /* switching div setting */
		goto errorout;

	if (dfs_trans_mux(lv_from, lv_switch, table, TRANS_DIFF)) /* switching mux setting */
		goto errorout;

	dfsmif_switch_pre(rate_from, rate_switch); /* timing parameter setting for switching frequency */

	if (dfs_use_switch(table))
		goto errorout;  /* Switching mux setting */

	dfsmif_switch_post(rate_from, rate_switch);  /* waiting for idle status of pause */

	if (dfs_trans_div(lv_from, lv_switch, table, TRANS_LOW))
		goto errorout;

	return 0;

errorout:
	return -1;
}

static int dfsmif_transition(unsigned int rate_switch, unsigned int rate_to, struct dfs_table *table)
{
	int lv_to, lv_switch;

	lv_to = dfs_get_lv(rate_to, table);

	if (lv_to >= table->num_of_lv)
		goto errorout;

	lv_switch = dfs_get_lv(rate_switch, table) - 1;	/* switch is not regular level */

	if (dfs_trans_pll(lv_switch, lv_to, table, TRANS_FORCE))
		goto errorout;

	if (dfs_trans_div(lv_switch, lv_to, table, TRANS_HIGH))
		goto errorout;
	if (dfs_trans_mux(lv_switch, lv_to, table, TRANS_DIFF))
		goto errorout;

	dfsmif_switch_pre(rate_switch, rate_to);
	if (dfs_not_use_switch(table))
		goto errorout;
	dfsmif_switch_post(rate_switch, rate_to);

	if (dfs_trans_div(lv_switch, lv_to, table, TRANS_LOW))
		goto errorout;

	dfsmif_trans_post(rate_switch, rate_to);

	return 0;

errorout:
	return -1;
}

static unsigned long dfs_mif_get_rate(struct dfs_table *table)
{
	int l, m;
	unsigned int cur[128] = {0, };
	unsigned long long rate;
	struct pwrcal_clk *clk;
	unsigned int aclk_mif_pll, bus_pll_user;

	aclk_mif_pll = pwrcal_mux_get_src(table->switch_mux);

	if (aclk_mif_pll == 1) {

		bus_pll_user = pwrcal_mux_get_src(table->switch_src_usermux);

		if (bus_pll_user == 1)
			return (unsigned long)(800000);
		else
			return (unsigned long)(26000);
	}

	for (m = 1; m < table->num_of_members; m++) {
		clk = table->members[m];
		if (is_pll(clk)) {
			rate = pwrcal_pll_get_rate(clk);
			do_div(rate, 1000);
			cur[m] = (unsigned int)rate;
		}
		if (is_mux(clk))
			cur[m] = pwrcal_mux_get_src(clk);
		if (is_div(clk))
			cur[m] = pwrcal_div_get_ratio(clk) - 1;
		if (is_gate(clk))
			cur[m] = pwrcal_gate_is_enabled(clk);
	}

	for (l = 0; l < table->num_of_lv; l++) {
		for (m = 1; m < table->num_of_members; m++)
			if (cur[m] != get_value(table, l, m))
				break;

		if (m == table->num_of_members)
			return get_value(table, l, 0);
	}

	for (m = 1; m < table->num_of_members; m++) {
		clk = table->members[m];
		pr_err("dfs_get_rate mid : %s : %d\n", clk->name, cur[m]);
	}

	return 0;
}

static struct dfs_switch dfsmif_switches[] = {
	{	1599000,	0,	0	},
	{	799500,	0,	1	},
};

static struct dfs_table dfsmif_table = {

	.switches = dfsmif_switches,
	.num_of_switches = ARRAY_SIZE(dfsmif_switches),
	.switch_mux = CLK(CCORE_MUX_ACLK_MIF_PLL),
	.switch_use = 1,
	.switch_notuse = 0,
	.switch_src_mux = CLK(CCORE_MUX_CLKCMU_MIF_SWITCH),
	.switch_src_div = CLK(CCORE_DIV_CLKCMU_MIF_SWITCH),
	.switch_src_gate = CLK(CCORE_GATE_CLKCMU_MIF_SWITCH),
	.switch_src_usermux = CLK(CCORE_MUX_BUS_PLL_MIF), // MUX_BUS_PLL_USER
	.private_trans = dfsmif_transition,
	.private_switch = dfsmif_transition_switch,
	.private_getrate = dfs_mif_get_rate,
};


struct pwrcal_clk_set dfsmif_en_list[] = {
	{	CLK_NONE,	0,	-1},
};


static struct dfs_switch dfsint_switches[] = {
};

static struct dfs_table dfsint_table = {

	.switches = dfsint_switches,
	.num_of_switches = ARRAY_SIZE(dfsint_switches),
	.switch_use = 1,
	.switch_notuse = 0,
};

struct pwrcal_clk_set dfsint_en_list[] = {
	{CLK_NONE,	0,	-1},
};

static struct dfs_switch dfsdisp_switches[] = {
};

static struct dfs_table dfsdisp_table = {

	.switches = dfsdisp_switches,
	.num_of_switches = ARRAY_SIZE(dfsdisp_switches),
	.switch_use = 1,
	.switch_notuse = 0,
};

struct pwrcal_clk_set dfsdisp_en_list[] = {
	{CLK_NONE,	0,	-1},
};

static struct dfs_switch dfscam_switches[] = {
};

static struct dfs_table dfscam_table = {

	.switches = dfscam_switches,
	.num_of_switches = ARRAY_SIZE(dfscam_switches),
	.switch_use = 1,
	.switch_notuse = 0,
};

struct pwrcal_clk_set dfscam_en_list[] = {
	{CLK(ISP_MUX_CLKCMU_ISP_VRA_USER),	1,	0},
	{CLK(ISP_MUX_CLKCMU_ISP_CAM_USER),	1,	0},
	{CLK(ISP_MUX_CLKCMU_ISP_ISP_USER),	1,	0},
	{CLK(ISP_MUX_CLK_ISP_VRA),	-1,	0},
	{CLK(ISP_MUX_CLK_ISP_CAM),	-1,	0},
	{CLK(ISP_MUX_CLK_ISP_ISP),	-1,	0},
	{CLK(ISP_MUX_CLK_ISP_ISPD),	1,	-1},
	{CLK(ISP_DIV_CLK_ISP_APB),	3,	-1},
	{CLK(ISP_DIV_CLK_ISP_CAM_HALF),	1,	-1},
	{CLK(CCORE_MUXGATE_CLKCMU_ISP_VRA),	1,	0},
	{CLK(CCORE_MUXGATE_CLKCMU_ISP_CAM),	1,	0},
	{CLK(CCORE_MUXGATE_CLKCMU_ISP_ISP),	1,	0},
	{CLK_NONE,	0,	-1},
};

static int dfscpucl0_init_smpl(void)
{
	return 0;
}

static int dfscpucl0_set_smpl(void)
{
	return 0;
}

static int dfscpucl0_get_smpl(void)
{
	return 0;
}

static int dfscpucl0_get_rate_table(unsigned long *table)
{
	return dfs_get_rate_table(&dfscpucl1_table, table);
}

static int dfscpucl0_idle_clock_down(unsigned int enable)
{
	return 0;
}
static struct vclk_dfs_ops dfscpucl0_dfsops = {
	.init_smpl = dfscpucl0_init_smpl,
	.set_smpl = dfscpucl0_set_smpl,
	.get_smpl = dfscpucl0_get_smpl,
	.get_rate_table = dfscpucl0_get_rate_table,
	.cpu_idle_clock_down = dfscpucl0_idle_clock_down,
	.get_margin_param = common_get_margin_param,
};

static int dfscpucl1_get_rate_table(unsigned long *table)
{
	return dfs_get_rate_table(&dfscpucl1_table, table);
}

static int dfscpucl1_idle_clock_down(unsigned int enable)
{
	return 0;
}

static struct vclk_dfs_ops dfscpucl1_dfsops = {
	.get_rate_table = dfscpucl1_get_rate_table,
	.cpu_idle_clock_down = dfscpucl1_idle_clock_down,
	.get_margin_param = common_get_margin_param,
};

static int dfsg3d_dvs(int on)
{
	return 0;
}

static int dfsg3d_get_rate_table(unsigned long *table)
{
	return dfs_get_rate_table(&dfsg3d_table, table);
}

static struct vclk_dfs_ops dfsg3d_dfsops = {
	.dvs = dfsg3d_dvs,
	.get_rate_table = dfsg3d_get_rate_table,
	.get_margin_param = common_get_margin_param,
};

static int dfsmif_get_rate_table(unsigned long *table)
{
	return dfs_get_rate_table(&dfsmif_table, table);
}

static int dfsmif_is_dll_on(void)
{
	return 1;
}

extern void pwrcal_dmc_set_clock_gating(int enable);
static int dfsmif_ctrl_clk_gate(unsigned int enable)
{
	pwrcal_dmc_set_clock_gating((int)enable);

	return 0;
}

static struct vclk_dfs_ops dfsmif_dfsops = {
	.get_rate_table = dfsmif_get_rate_table,
	.is_dll_on = dfsmif_is_dll_on,
	.ctrl_clk_gate = dfsmif_ctrl_clk_gate,
	.get_margin_param = common_get_margin_param,
};

static int dfsint_get_rate_table(unsigned long *table)
{
	return dfs_get_rate_table(&dfsint_table, table);
}

static struct vclk_dfs_ops dfsint_dfsops = {
	.get_rate_table = dfsint_get_rate_table,
	.get_margin_param = common_get_margin_param,
};

static int dfsdisp_get_rate_table(unsigned long *table)
{
	return dfs_get_rate_table(&dfsdisp_table, table);
}

static struct vclk_dfs_ops dfsdisp_dfsops = {
	.get_rate_table = dfsdisp_get_rate_table,
	.get_margin_param = common_get_margin_param,
};

static int dfscam_get_rate_table(unsigned long *table)
{
	return dfs_get_rate_table(&dfscam_table, table);
}

static struct vclk_dfs_ops dfscam_dfsops = {
	.get_rate_table = dfscam_get_rate_table,
	.get_margin_param = common_get_margin_param,
};


static DEFINE_SPINLOCK(dvfs_cpucl0_lock);
static DEFINE_SPINLOCK(dvfs_cpucl1_lock);
static DEFINE_SPINLOCK(dvfs_g3d_lock);
static DEFINE_SPINLOCK(dvfs_mif_lock);
static DEFINE_SPINLOCK(dvfs_int_lock);
static DEFINE_SPINLOCK(dvfs_disp_lock);
static DEFINE_SPINLOCK(dvfs_cam_lock);

DFS(dvfs_cpucl0) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 1,
	.vclk.vfreq	= 0,
	.vclk.name	= "dvfs_cpucl0",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvfs_cpucl0_lock,
//	.clks		= dfscpucl0_dfsclkgrp,
	.en_clks	= dfscpucl0_en_list,
	.table		= &dfscpucl0_table,
	.dfsops		= &dfscpucl0_dfsops,
};

DFS(dvfs_cpucl1) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 1,
	.vclk.vfreq	= 0,
	.vclk.name	= "dvfs_cpucl1",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvfs_cpucl1_lock,
//	.clks		= dfscpucl1_dfsclkgrp,
	.en_clks	= dfscpucl1_en_list,
	.table		= &dfscpucl1_table,
	.dfsops		= &dfscpucl1_dfsops,
};

DFS(dvfs_g3d) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 0,
	.vclk.vfreq	= 350000,
	.vclk.name	= "dvfs_g3d",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvfs_g3d_lock,
//	.clks		= dfsg3d_dfsclkgrp,
	.en_clks	= dfsg3d_en_list,
	.table		= &dfsg3d_table,
	.dfsops		= &dfsg3d_dfsops,
};

DFS(dvfs_mif) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 1,
	.vclk.vfreq	= 0,
	.vclk.name	= "dvfs_mif",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvfs_mif_lock,
//	.clks		= dfsmif_dfsclkgrp,
	.en_clks	= dfsmif_en_list,
	.table		= &dfsmif_table,
	.dfsops		= &dfsmif_dfsops,
};

DFS(dvfs_int) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 1,
	.vclk.vfreq	= 0,
	.vclk.name	= "dvfs_int",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvfs_int_lock,
//	.clks		= dfsint_dfsclkgrp,
	.en_clks	= dfsint_en_list,
	.table		= &dfsint_table,
	.dfsops		= &dfsint_dfsops,
};

DFS(dvfs_disp) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 0,
	.vclk.vfreq	= 0,
	.vclk.name	= "dvfs_disp",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvfs_disp_lock,
//	.clks		= dfsdisp_dfsclkgrp,
	.en_clks	= dfsdisp_en_list,
	.table		= &dfsdisp_table,
	.dfsops		= &dfsdisp_dfsops,
};

DFS(dvfs_cam) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 0,
	.vclk.vfreq	= 400000,
	.vclk.name	= "dvfs_cam",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvfs_cam_lock,
//	.clks		= dfscam_dfsclkgrp,
	.en_clks	= dfscam_en_list,
	.table		= &dfscam_table,
	.dfsops		= &dfscam_dfsops,
};

void dfs_set_clk_information(struct pwrcal_vclk_dfs *dfs)
{
	int i, j;
	void *dvfs_block;
	struct ect_dvfs_domain *dvfs_domain;
	struct dfs_table *dvfs_table;

	dvfs_block = ect_get_block("DVFS");
	if (dvfs_block == NULL)
		return;

	dvfs_domain = ect_dvfs_get_domain(dvfs_block, dfs->vclk.name);
	if (dvfs_domain == NULL)
		return;

	dvfs_table = dfs->table;
	dvfs_table->num_of_lv = dvfs_domain->num_of_level;
	dvfs_table->num_of_members = dvfs_domain->num_of_clock + 1;
	dvfs_table->max_freq = dvfs_domain->max_frequency;
	dvfs_table->min_freq = dvfs_domain->min_frequency;

	dvfs_table->members = kzalloc(sizeof(struct pwrcal_clk *) * (dvfs_domain->num_of_clock + 1), GFP_KERNEL);
	if (dvfs_table->members == NULL)
		return;

	dvfs_table->members[0] = REPRESENT_RATE;
	for (i = 0; i < dvfs_domain->num_of_clock; ++i) {
		dvfs_table->members[i + 1] = clk_find(dvfs_domain->list_clock[i]);
		if (dvfs_table->members[i] == NULL)
			return;
	}

	dvfs_table->rate_table = kzalloc(sizeof(unsigned int) * (dvfs_domain->num_of_clock + 1) * dvfs_domain->num_of_level, GFP_KERNEL);
	if (dvfs_table->rate_table == NULL)
		return;

	for (i = 0; i < dvfs_domain->num_of_level; ++i) {

		dvfs_table->rate_table[i * (dvfs_domain->num_of_clock + 1)] = dvfs_domain->list_level[i].level;
		for (j = 0; j <= dvfs_domain->num_of_clock; ++j) {
			dvfs_table->rate_table[i * (dvfs_domain->num_of_clock + 1) + j + 1] =
				dvfs_domain->list_dvfs_value[i * dvfs_domain->num_of_clock + j];
		}
	}

}
void dfs_init(void)
{
	dfs_set_clk_information(&vclk_dvfs_cpucl0);
	dfs_set_clk_information(&vclk_dvfs_cpucl1);
	dfs_set_clk_information(&vclk_dvfs_g3d);
	dfs_set_clk_information(&vclk_dvfs_mif);
	dfs_set_clk_information(&vclk_dvfs_int);
	dfs_set_clk_information(&vclk_dvfs_cam);
	dfs_set_clk_information(&vclk_dvfs_disp);

	dfs_dram_init();
}
