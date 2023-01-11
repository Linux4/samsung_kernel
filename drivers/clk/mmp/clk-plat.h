#ifndef __MACH_MMP_CLK_PLAT_H
#define __MACH_MMP_CLK_PLAT_H

#ifdef CONFIG_PXA_DVFS
extern int setup_pxa1u88_dvfs_platinfo(void);
extern int setup_pxa1L88_dvfs_platinfo(void);
extern int ddr_get_dvc_level(int rate);
extern int setup_pxa1908_dvfs_platinfo(void);
extern int setup_pxa1936_dvfs_platinfo(void);
extern unsigned int get_helan3_max_freq(void);
#endif

/* supported DDR chip type */
enum ddr_type {
	DDR_400M = 0,
	DDR_533M,
	DDR_667M,
	DDR_800M,
	DDR_TYPE_MAX,
};

enum {
	CORE_0p8G = 832,
	CORE_1p0G = 1057,
	CORE_1p2G = 1183,
	CORE_1p25G = 1248,
	CORE_1p5G = 1526,
	CORE_1p8G = 1803,
	CORE_2p0G = 2000,
};

extern unsigned long max_freq;
extern enum ddr_type ddr_mode;
extern int is_1p5G_chip;

extern unsigned int mmp_clk_mix_get_opnum(struct clk *clk);
extern unsigned long mmp_clk_mix_get_oprate(struct clk *clk,
		unsigned int index);
extern unsigned int get_foundry(void);
extern unsigned int get_profile_pxa1L88(void);

#ifdef CONFIG_PM_DEVFREQ
extern void __init_comp_devfreq_table(struct clk *clk, unsigned int dev_id);
#endif

extern void register_mixclk_dcstatinfo(struct clk *clk);

extern int smc_get_fuse_info(long function_id, void *arg);

struct fuse_info {
	unsigned int arg0;
	unsigned int arg1;
	unsigned int arg2;
	unsigned int arg3;
	unsigned int arg4;
	unsigned int arg5;
	unsigned int arg6;
};

/*
 * fuse related information, shared via plat,
 * please extend if necessary.
 */
struct comm_fuse_info {
	unsigned int fab;
	unsigned int lvtdro;
	unsigned int svtdro;
	unsigned int profile;
	unsigned int iddq_1050;
	unsigned int iddq_1030;
	unsigned int skusetting;
};
extern int plat_fill_fuseinfo(struct comm_fuse_info *info);

struct parents_table {
	char *parent_name;
	struct clk *parent;
	u32 hw_sel_val;
};

extern void mmp_clk_parents_lookup(struct parents_table *parent_table,
	int parent_table_size);

#endif
