/* SPDX-License-Identifier: GPL-2.0
 *
 * energy_model.h - energy model header file
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Chung-Kai Yang <Chung-kai.Yang@mediatek.com>
 */

#define MAX_NR_FREQ      32 /* Max supported number of frequency */
#define MAX_NR_WL_TYPE	5
#define MAX_PD_COUNT 3
#define VOLT_STEP    625
#define MAX_LEAKAGE_SIZE	36

#define EEM_LOG_BASE		0x00112400
#define EEM_LOG_SIZE		0x1800 /* 6k size 0x1800 */

#define csram_read(offs)	__raw_readl(csram_base + (offs))
#define CAPACITY_TBL_OFFSET 0xFA0
#define CAPACITY_TBL_SIZE 0x100
#define CAPACITY_ENTRY_SIZE 0x2

/* Frequency Scaling Step Settings */
#define FREQ_STEP		26000
#define CLUSTER_SIZE		0x120
#define CSRAM_TBL_OFFSET	0x10
#define CLUSTER_VOLT_SIZE	0x84
#define CSRAM_PWR_OFFSET (CSRAM_TBL_OFFSET + 0x90)

/* WL Setting(TCM) */
#define DEFAULT_TYPE	0
#define DEFAULT_NR_TYPE	1
#define WL_MEM_RES_IND	1
#define WL_TBL_BASE_PHYS 0x0C0BE000
#define WL_TBL_START_OFFSET 0x40
#define WL_OFFSET 0x318
#define WL_DSU_EM_START 0xCA0
#define WL_WEIGHTING_OFFSET 0x10
#define WL_DSU_EM_OFFSET 0x108
#define DSU_FREQ_OFFSET 0x370

#define PHY_CLK_OFF	0x11E0

extern struct mtk_em_perf_domain *mtk_em_pd_ptr_public;
extern struct mtk_em_perf_domain *mtk_em_pd_ptr_private;
extern struct mtk_dsu_em mtk_dsu_em;
extern struct mtk_mapping mtk_mapping;

struct para {
	unsigned int a: 12;
	unsigned int b: 20;
};

struct lkg_para {
	struct para a_b_para;
	unsigned int c;
};

struct leakage_data {
	void __iomem *base;
	int init;
};

struct mtk_em_perf_state {
	/* Performance state setting */
	unsigned int freq;
	unsigned int volt;
	unsigned int capacity;
	unsigned int dyn_pwr;
	unsigned int pwr_eff;
	struct lkg_para leakage_para;
	unsigned int dsu_freq;
};

struct mtk_weighting {
	unsigned int dsu_weighting : 16;
	unsigned int emi_weighting : 16;
};

struct mtk_em_perf_domain {
	struct mtk_em_perf_state *table;
	unsigned int nr_wl_tables;
	unsigned int cur_wl_table;
	unsigned int nr_perf_states;
	unsigned int cluster_num;
	unsigned long max_freq;
	unsigned long min_freq;
	struct mtk_weighting cur_weighting;
	struct mtk_weighting *mtk_weighting;
	struct cpumask *cpumask;
	struct mtk_em_perf_state **wl_table;
};

/* DSU Energy Model */
struct mtk_dsu_em {
	struct mtk_dsu_em_perf_state *dsu_table;
	unsigned int nr_dsu_type;
	unsigned int cur_dsu_type;
	unsigned int nr_perf_states;
	struct mtk_dsu_em_perf_state **dsu_wl_table;
};

struct mtk_dsu_em_perf_state {
	unsigned long dsu_frequency;
	unsigned long dsu_volt;
	unsigned long dynamic_power;
	unsigned int dsu_bandwidth;
	unsigned int emi_bandwidth;
};


struct mtk_relation {
	unsigned int cpu_type;
	unsigned int dsu_type;
};

struct mtk_mapping {
	unsigned int nr_cpu_type;
	unsigned int nr_dsu_type;
	unsigned int total_type;
	struct mtk_relation *cpu_to_dsu;
};


enum eemsn_det_id {
	EEMSN_DET_L = 0,
	EEMSN_DET_BL, /* for BL or B */
	EEMSN_DET_B, /* for B or DSU */
	EEMSN_DET_CCI,

	NR_EEMSN_DET,
};

struct eemsn_log_det {
	unsigned int temp;
	unsigned short freq_tbl[MAX_NR_FREQ];
	unsigned char volt_tbl_pmic[MAX_NR_FREQ];
	unsigned char volt_tbl_orig[MAX_NR_FREQ];
	unsigned char volt_tbl_init2[MAX_NR_FREQ];
	unsigned char num_freq_tbl;
	unsigned char lock;
	unsigned char features;
	int8_t volt_clamp;
	int8_t volt_offset;
	enum eemsn_det_id det_id;
};

struct eemsn_log {
	unsigned int eemsn_disable:8;
	unsigned int ctrl_aging_Enable:8;
	unsigned int sn_disable:8;
	unsigned int segCode:8;
	unsigned char init2_v_ready;
	unsigned char init_vboot_done;
	unsigned char lock;
	unsigned char eemsn_log_en;
	struct eemsn_log_det det_log[NR_EEMSN_DET];
};

static inline unsigned int mtk_get_nr_cap(unsigned int cluster)
{
	return mtk_em_pd_ptr_private[cluster].nr_perf_states;
}

unsigned int mtk_get_leakage(unsigned int cluster, unsigned int opp, unsigned int temperature);
unsigned int mtk_get_dsu_freq(void);
extern bool is_wl_support(void);
extern int mtk_update_wl_table(int cluster, int type);
extern __init int mtk_static_power_init(void);
