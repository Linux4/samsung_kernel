#ifndef __SPRD_BM_H__
#define __SPRD_BM_H__

#define SPRD_AHB_BM_NAME "sprd_ahb_busmonitor"
#define SPRD_AXI_BM_NAME "sprd_axi_busmonitor"

/*depending on the platform*/
enum sci_bm_index {
	AXI_BM0 = 0x0,
	AXI_BM1,
	AXI_BM2,
	AXI_BM3,
	AXI_BM4,
	AXI_BM5,
	AXI_BM6,
	AXI_BM7,
	AXI_BM8,
	AXI_BM9,
	AHB_BM0,
	AHB_BM1,
	AHB_BM2,
	BM_SIZE,
};

enum sci_bm_chn {
	CHN0 = 0x0,
	CHN1,
	CHN2,
	CHN3,
};

enum sci_bm_mode {
	R_MODE,
	W_MODE,
	RW_MODE,
	/*
	PER_MODE,
	RST_BUF_MODE,
	*/
};

struct sci_bm_cfg {
	u32 addr_min;
	u32 addr_max;
	/*only be effective for AHB busmonitor*/
	u64 data_min;
	u64 data_max;
	enum sci_bm_mode bm_mode;
};

int sci_bm_set_point(enum sci_bm_index bm_index, enum sci_bm_chn chn,
	const struct sci_bm_cfg *cfg, void(*call_back)(void));

void sci_bm_unset_point(enum sci_bm_index bm_index);

#endif
