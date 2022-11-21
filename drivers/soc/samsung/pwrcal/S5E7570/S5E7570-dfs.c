#include "../pwrcal.h"
#include "../pwrcal-env.h"
#include "../pwrcal-clk.h"
#include "../pwrcal-pmu.h"
#include "../pwrcal-dfs.h"
#include "../pwrcal-rae.h"
#include "../pwrcal-asv.h"
#include "S5E7570-cmusfr.h"
#include "S5E7570-pmusfr.h"
#include "S5E7570-cmu.h"
#include "S5E7570-vclk-internal.h"

extern unsigned int dfscpucl0_rate_table[];
extern unsigned int dfsg3d_rate_table[];
extern unsigned int dfsmif_rate_table[];
extern unsigned int dfsint_rate_table[];
extern unsigned int dfsdisp_rate_table[];
extern unsigned int dfscam_rate_table[];

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
#ifdef CONFIG_SOC_EXYNOS7570_DUAL
	{840000,	0,	0},
	{840000 / 2,	0,	1},
	{840000 / 3,	0,	2},
	{840000 / 4,	0,	3},
#else
	{830000,	0,	0},
	{830000 / 2,	0,	1},
	{830000 / 3,	0,	2},
	{830000 / 4,	0,	3},
#endif
};

static struct dfs_table dfscpucl0_table = {
	.switches = dfscpucl0_switches,
	.num_of_switches = ARRAY_SIZE(dfscpucl0_switches),
	.switch_mux = CLK(CPUCL0_MUX_CLK_CPUCL0),
	.switch_use = 1,
	.switch_notuse = 0,
	.switch_src_div = CLK(MIF_DIV_CLKCMU_CPUCL0_SWITCH),
	.switch_src_gate = CLK(MIF_GATE_CLKCMU_CPUCL0_SWITCH),
	.switch_src_usermux = CLK(CPUCL0_MUX_CLKCMU_CPUCL0_SWITCH_USER),
};

struct pwrcal_clk_set dfscpucl0_en_list[] = {
	{CLK_NONE,	0,	-1},
};

static struct dfs_switch dfsg3d_switches[] = {
};

static struct dfs_table dfsg3d_table = {
	.switches = dfsg3d_switches,
	.num_of_switches = ARRAY_SIZE(dfsg3d_switches),
	.switch_use = 1,
	.switch_notuse = 0,
};

struct pwrcal_clk_set dfsg3d_en_list[] = {
	{CLK(G3D_MUX_CLKCMU_G3D_USER),	1,	0},
	{CLK(G3D_DIV_CLK_G3D_BUS),	3,	-1},
	{CLK(G3D_DIV_CLK_G3D_APB),	3,	-1},
	{CLK_NONE,	0,	-1},
};

static struct dfs_switch dfsmif_switches[] = {
};

extern void pwrcal_dmc_set_dvfs(unsigned long long target_mif_freq, unsigned int timing_set_idx);
extern void pwrcal_dmc_set_pre_dvfs(void);
extern void pwrcal_dmc_set_post_dvfs(unsigned long long target_freq);
extern void pwrcal_dmc_set_vtmon_on_swithing(void);
extern void pwrcal_dmc_set_refresh_method_pre_dvfs(unsigned long long current_rate, unsigned long long target_rate);
extern void pwrcal_dmc_set_refresh_method_post_dvfs(unsigned long long current_rate, unsigned long long target_rate);
extern void pwrcal_dmc_set_dsref_cycle(unsigned long long target_rate);

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
		status = pwrcal_getf(PAUSE, 16, 0x3);
		if (status == 0x0)
			break;

		if (timeout > CLK_WAIT_CNT)
			pr_err("PAUSE staus(0x%X) is not stable", status);

		cpu_relax();
	}

	return 0;
}

static int is_dll_on_status = 1;

static void dfsmif_trans_pre(unsigned int rate_from, unsigned int rate_to)
{
	static unsigned int paraset;
	unsigned long long from, to;

	is_dll_on_status = 1;

	from = (unsigned long long)rate_from * 1000;
	to = (unsigned long long)rate_to * 1000;

	pwrcal_dmc_set_refresh_method_pre_dvfs(from, to);
	pwrcal_clk_set_mif_pause_enable(1);

	/* VTMON disable before MIF DFS sequence*/
	pwrcal_dmc_set_pre_dvfs();

	paraset = pwrcal_readl(DREX_FREQ_CTRL1);
	paraset = (paraset + 1) % 2;
	pwrcal_dmc_set_dvfs(to, paraset);
}

static void dfsmif_trans_post(unsigned int rate_from, unsigned int rate_to)
{
	unsigned long long from, to;

	from = (unsigned long long)rate_from * 1000;
	to = (unsigned long long)rate_to * 1000;

	pwrcal_clk_wait_mif_pause();

	/* VTMON enable before MIF DFS sequence*/
	pwrcal_dmc_set_post_dvfs(to);

	pwrcal_dmc_set_refresh_method_post_dvfs(from, to);
	pwrcal_dmc_set_dsref_cycle(to);

	if (rate_to >= 416000)
		is_dll_on_status = 1;
	else
		is_dll_on_status = 0;
}

static int dfsmif_transition(unsigned int rate_from, unsigned int rate_to, struct dfs_table *table)
{
	int lv_from, lv_to;

	lv_from = dfs_get_lv(rate_from, table);
	if (lv_from >= table->num_of_lv)
		goto errorout;

	lv_to = dfs_get_lv(rate_to, table);
	if (lv_to >= table->num_of_lv)
		goto errorout;

	dfsmif_trans_pre(rate_from, rate_to);

	if (dfs_trans_div(lv_from, lv_to, table, TRANS_HIGH))
		goto errorout;

	if (dfs_trans_mux(lv_from, lv_to, table, TRANS_DIFF))
		goto errorout;

	if (dfs_trans_div(lv_from, lv_to, table, TRANS_LOW))
		goto errorout;

	dfsmif_trans_post(rate_from, rate_to);


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
	unsigned int drex_freq = 0;
	unsigned int temp = 0;

	for (m = 1; m < table->num_of_members; m++) {
		clk = table->members[m];
		if (is_pll(clk)) {
			rate = pwrcal_pll_get_rate(clk);
			do_div(rate, 1000);
			cur[m] = (unsigned int)rate;
		}
		if (is_mux(clk)) {
			if (clk->id == MIF_MUX_CLK_MIF_PHY_CLK) {
				temp = pwrcal_getf(clk->offset, 0, 0xFF0030);

				drex_freq = (unsigned int)(temp >> clk->shift);

				cur[m] = drex_freq;
			} else
				cur[m] = pwrcal_mux_get_src(clk);
		}
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

static struct dfs_table dfsmif_table = {
	.switches = dfsmif_switches,
	.num_of_switches = ARRAY_SIZE(dfsmif_switches),
	.switch_use = 1,
	.switch_notuse = 0,
	.private_trans = dfsmif_transition,
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
	{CLK_NONE,	0,	-1},
};

static struct dfs_switch dfscp_switches[] = {
};

static struct dfs_table dfscp_table = {
	.switches = dfscp_switches,
	.num_of_switches = ARRAY_SIZE(dfscp_switches),
};

struct pwrcal_clk_set dfscp_en_list[] = {
	{CLK_NONE,	0,	-1},
};

static int dfscpucl0_get_rate_table(unsigned long *table)
{
	return dfs_get_rate_table(&dfscpucl0_table, table);
}

static int dfscpucl0_idle_clock_down(unsigned int enable)
{
	return 0;
}

static struct vclk_dfs_ops dfscpucl0_dfsops = {
	.get_rate_table = dfscpucl0_get_rate_table,
	.cpu_idle_clock_down = dfscpucl0_idle_clock_down,
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

static struct vclk_dfs_ops dfsmif_dfsops = {
	.get_rate_table = dfsmif_get_rate_table,
	.is_dll_on = dfsmif_is_dll_on,
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

static int dfscp_get_rate_table(unsigned long *table)
{
	return dfs_get_rate_table(&dfscp_table, table);
}

static struct vclk_dfs_ops dfscp_dfsops = {
	.get_rate_table = dfscp_get_rate_table,
};

static DEFINE_SPINLOCK(dvfs_cpucl0_lock);
static DEFINE_SPINLOCK(dvfs_g3d_lock);
static DEFINE_SPINLOCK(dvfs_mif_lock);
static DEFINE_SPINLOCK(dvfs_int_lock);
static DEFINE_SPINLOCK(dvfs_disp_lock);
static DEFINE_SPINLOCK(dvfs_cam_lock);
static DEFINE_SPINLOCK(dvs_cp_lock);

DFS(dvfs_cpucl0) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 1,
	.vclk.vfreq	= 0,
	.vclk.name	= "dvfs_cpucl0",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvfs_cpucl0_lock,
	.en_clks	= dfscpucl0_en_list,
	.table		= &dfscpucl0_table,
	.dfsops		= &dfscpucl0_dfsops,
};
DFS(dvfs_g3d) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 0,
	.vclk.vfreq	= 350000,
	.vclk.name	= "dvfs_g3d",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvfs_g3d_lock,
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
	.en_clks	= dfsdisp_en_list,
	.table		= &dfsdisp_table,
	.dfsops		= &dfsdisp_dfsops,
};

DFS(dvfs_cam) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 0,
#ifdef CONFIG_SOC_EXYNOS7570_DUAL
	.vclk.vfreq	= 560000,
#else
	.vclk.vfreq	= 554000,
#endif
	.vclk.name	= "dvfs_cam",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvfs_cam_lock,
	.en_clks	= dfscam_en_list,
	.table		= &dfscam_table,
	.dfsops		= &dfscam_dfsops,
};

DFS(dvs_cp) = {
	.vclk.type	= vclk_group_dfs,
	.vclk.parent	= VCLK(pxmxdx_top),
	.vclk.ref_count	= 0,
	.vclk.vfreq	= 0,
	.vclk.name	= "dvs_cp",
	.vclk.ops	= &dfs_ops,
	.lock		= &dvs_cp_lock,
	.en_clks	= dfscp_en_list,
	.table		= &dfscp_table,
	.dfsops		= &dfscp_dfsops,
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
	dfs_set_clk_information(&vclk_dvfs_g3d);
	dfs_set_clk_information(&vclk_dvfs_mif);
	dfs_set_clk_information(&vclk_dvfs_int);
	dfs_set_clk_information(&vclk_dvfs_cam);
	dfs_set_clk_information(&vclk_dvfs_disp);
	dfs_set_clk_information(&vclk_dvs_cp);

	dfs_dram_init();
}
