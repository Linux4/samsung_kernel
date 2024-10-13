#ifndef __FVMAP_H__
#define __FVMAP_H__

enum margin_id {
	MARGIN_MIF,
	MARGIN_INT,
	MARGIN_CPUCL0,
	MARGIN_CPUCL1,
	MARGIN_G3D,
	MARGIN_CAM,
	MARGIN_DISP,
	MARGIN_AUD,
	MARGIN_CP,
	MAX_MARGIN_ID,
};

/* FV(Frequency Voltage MAP) */
struct fvmap_header {
	unsigned char dvfs_type;
	unsigned char num_of_lv;
	unsigned char num_of_members;
	unsigned char num_of_pll;
	unsigned char num_of_mux;
	unsigned char num_of_div;
	unsigned short gearratio;
	unsigned char init_lv;
	unsigned char num_of_gate;
	unsigned char reserved[2];
	unsigned short block_addr[3];
	unsigned short o_members;
	unsigned short o_ratevolt;
	unsigned short o_tables;
};

struct clocks {
	unsigned short addr[0];
};

struct pll_header {
	unsigned int addr;
	unsigned short o_lock;
	unsigned short level;
	unsigned int pms[0];
};

struct rate_volt {
	unsigned int rate;
	unsigned int volt;
};

struct rate_volt_header {
	struct rate_volt table[0];
};

struct dvfs_table {
	unsigned char val[0];
};

struct freq_volt {
	unsigned int rate;
	unsigned int volt;
};

#if defined(CONFIG_ACPM_DVFS) || defined(CONFIG_ACPM_DVFS_MODULE)
extern int fvmap_init(void __iomem *sram_base);
extern int fvmap_get_voltage_table(unsigned int id, unsigned int *table);
extern int fvmap_get_freq_volt_table(unsigned int id, void *freq_volt_table,
		unsigned int table_size);
#else
static inline int fvmap_init(void __iomem *sram_base)
{
	return 0;
}

static inline int fvmap_get_voltage_table(unsigned int id, unsigned int *table)
{
	return 0;
}
static inline int fvmap_get_freq_volt_table(unsigned int id, void *freq_volt_table,
		unsigned int table_size)
{
	return 0;
}
#endif
#endif
