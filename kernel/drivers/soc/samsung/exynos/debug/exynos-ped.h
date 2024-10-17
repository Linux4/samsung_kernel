/* Group 0 */
#define INTC0_IEN_SET				(0x0288)
#define INTC0_IEN_CLR				(0x028C)
#define INTC0_IPEND				(0x0290)
#define INTC0_IPRIO_IPEND			(0x0294)
#define INTC0_IPRIORIY(x)			(0x0298 + 4*(x))
#define INTC0_IPRIO_SECURE_PEND			(0x02A8)
#define INTC0_IPRIO_NONSECURE_PEND		(0x02AC)
/* Group 1 */
#define INTC1_IEN_SET				(0x02B8)
#define INTC1_IEN_CLR				(0x02BC)
#define INTC1_IPEND				(0x02C0)
#define INTC1_IPRIO_IPEND			(0x02C4)
#define INTC1_IPRIORITY(x)			(0x02C8 + 4*(x))
#define INTC1_IPRIO_SECURE_PEND			(0x02D8)
#define INTC1_IPRIO_NONSECURE_PEND		(0x02DC)

#define INT_CLR					(0x0400)
#define FAULT_INJECT				(0x0404)
#define END_OF_REGS				(0x0404)

#define MAX_GROUP_ID				(2)
#define MAX_PORT_ID				(32)

struct ped_platdata {
	int num_nodegroup;
	struct lh_nodegroup *lh_group;
	bool probed;
};

struct ped_dev {
	struct device *dev;
	struct ped_platdata *pdata;
	u32 irq;
	spinlock_t ctrl_lock;
	bool suspend_state;
};

struct lh_nodegroup {
	char name[16];
	u64 phy_regs;
	int num_group0;
	int num_group1;
	u32 group0_mask;
	u32 group1_mask;
	u32 group0_sicd_mask;
	u32 group1_sicd_mask;
	char pd_name[16];
	bool pd_support;
	bool pd_status;
	void __iomem *regs;
} __packed;

struct lh_nodegroup nodegroup_s5e9945_evt1[] = {
	{"ALIVE", 0x12BA0000, 26, 0, 0, 0, 0, 0, "no_pd", 0, 0, NULL},
	{"AUD", 0x12190000, 9, 0, 0, 0, 0, 0, "pd_aud", 1, 0, NULL},
	{"BRP", 0x1B8E0000, 4, 0, 0, 0, 0, 0, "pd_brp", 1, 0, NULL},
	{"CHUBVTS", 0x13E90000, 11, 0, 0, 0, 0, 0, "no_pd", 0, 0, NULL},
	{"CMGP", 0x140B0000, 2, 0, 0, 0, 0, 0, "no_pd", 0, 0, NULL},
	{"CPUCL0", 0x1DC70000, 29, 0, 0, 0, 0x1fffffff, 0, "no_pd", 0, 0, NULL},
	{"CSIS_ALIVE", 0x1A0E0000, 2, 0, 0, 0, 0, 0, "pd_aoccsis", 1, 0, NULL},
	{"CSIS", 0x1A1E0000, 8, 0, 0, 0, 0, 0, "pd_csis", 1, 0, NULL},
	{"CSTAT", 0x1A830000, 6, 0, 0, 0, 0, 0, "pd_cstat", 1, 0, NULL},
	{"DLFE", 0x27050000, 4, 0, 0, 0, 0, 0, "pd_dlfe", 1, 0, NULL},
	{"DLNE", 0x280A0000, 3, 0, 0, 0, 0, 0, "pd_dlne", 1, 0, NULL},
	{"DNC", 0x210F0000, 30, 0, 0, 0, 0, 0, "pd_dnc", 1, 0, NULL},
	{"DPUB", 0x190F0000, 24, 0, 0, 0, 0, 0, "pd_dpub", 1, 0, NULL},
	{"DPUF0", 0x198F0000, 7, 0, 0, 0, 0, 0, "pd_dpuf0", 1, 0, NULL},
	{"DPUF1", 0x19AF0000, 4, 0, 0, 0, 0, 0, "pd_dpuf1", 1, 0, NULL},
	{"DSP", 0x214C0000, 7, 0, 0, 0, 0, 0, "pd_dsp", 1, 0, NULL},
	//{"G3D", 0x22090000, 9, 0, 0, 0, "pd_g3dcore", 1, 0, NULL},
	{"GNPU0", 0x215F0000, 24, 0, 0, 0, 0, 0, "pd_gnpu0", 1, 0, NULL},
	{"GNPU1", 0x216F0000, 24, 0, 0, 0, 0, 0, "pd_gnpu1", 1, 0, NULL},
	//{"GNSS_INT", 0x161A0000, 2, 0, 0, 0, 0, 0, "??", NULL, NULL},
	//{"HSI0", 0x17850000, 3, 0, 0, 0, 0, 0, "pd_hsi0", 1, 0, NULL},
	{"HSI1", 0x180A0000, 6, 0, 0, 0, 0, 0, "pd_hsi1", 1, 0, NULL},
	{"ICPU", 0x1E890000, 8, 0, 0, 0, 0, 0, "pd_icpu", 1, 0, NULL},
	{"LME", 0x288F0000, 4, 0, 0, 0, 0, 0, "pd_lme", 1, 0, NULL},
	{"M2M", 0x27940000, 10, 0, 0, 0, 0, 0, "pd_m2m", 1, 0, NULL},
	{"MCFP", 0x1F8E0000, 22, 0, 0, 0, 0, 0, "pd_mcfp", 1, 0, NULL},
	{"MCSC", 0x1C8B0000, 15, 0, 0, 0, 0, 0, "pd_mcsc", 1, 0, NULL},
	{"MFC", 0x1E100000, 12, 0, 0, 0, 0, 0, "pd_mfc", 1, 0, NULL},
	{"MFD", 0x1F150000, 9, 0, 0, 0, 0, 0, "pd_mfd", 1, 0, NULL},
	{"MIF0", 0x23090000, 3, 0, 0, 0, 0x7, 0, "no_pd", 0, 0, NULL},
	{"MIF1", 0x24090000, 3, 0, 0x2, 0, 0x7, 0, "no_pd", 0, 0, NULL},	/* Must not use INT_EN_SET[1] */
	{"MIF2", 0x25090000, 3, 0, 0x2, 0, 0x7, 0, "no_pd", 0, 0, NULL},	/* Must not use INT_EN_SET[1] */
	{"MIF3", 0x26090000, 3, 0, 0x2, 0, 0x7, 0, "no_pd", 0, 0, NULL},	/* Must not use INT_EN_SET[1] */
	{"NOCL0", 0x22BF0000, 32, 25, 0, 0, 0xffffffff, 0x1ffffff, "no_pd", 0, 0, NULL},
	{"NOCL1A", 0x23840000, 32, 3, 0, 0, 0, 0, "no_pd", 0, 0, NULL},
	{"NOCL1B", 0x24850000, 32, 5, 0, 0, 0, 0, "no_pd", 0, 0, NULL},
	{"NOCL2A", 0x25860000, 32, 3, 0, 0, 0, 0, "pd_nocl2a", 1, 0, NULL},
	//{"CPALV_BUS", 0x2E720000, 1, 0, 0, 0, 0, 0, "??", NULL, NULL},
	// {"CPCPU_BUS", 0x2E1B0000, 4, 0, 0, 0, 0, 0, "??", NULL, NULL},
	{"NPUMEM_0", 0x21950000, 32, 32, 0, 0, 0, 0,"pd_npumem", 1, 0, NULL},
	{"NPUMEM_1", 0x21960000, 29, 0, 0, 0, 0, 0, "pd_npumem", 1, 0, NULL},
	{"PERIC0", 0x10960000, 1, 0, 0, 0, 0, 0, "no_pd", 0, 0, NULL},
	{"PERIC1", 0x11140000, 1, 0, 0, 0, 0, 0, "no_pd", 0, 0, NULL},
	{"PERIC2", 0x11840000, 2, 0, 0, 0, 0, 0, "no_pd", 0, 0, NULL},
	{"PERIS", 0x100E0000, 4, 0, 0, 0, 0, 0,"no_pd", 0, 0, NULL},
	{"RGBP", 0x1B0A0000, 6, 0, 0, 0, 0, 0, "pd_rgbp", 1, 0, NULL},
	{"SDMA", 0x212C0000, 23, 0, 0, 0, 0, 0,"pd_sdma", 1, 0, NULL},
	{"SNPU0", 0x217F0000, 18, 0, 0, 0, 0, 0, "pd_snpu0", 1, 0, NULL},
	{"SNPU1", 0x218F0000, 18, 0, 0, 0, 0, 0,"pd_snpu1", 1, 0, NULL},
	{"SSP", 0x16880000, 8, 0, 0, 0, 0, 0,"no_pd", 0, 0, NULL},
	{"UFD", 0x14980000, 4, 0, 0, 0, 0, 0,"pd_ufd", 1, 0, NULL},
	{"UFS", 0x17090000, 2, 0, 0, 0, 0, 0, "no_pd", 0, 0, NULL},
	{"UNPU", 0x15050000, 8, 0, 0, 0, 0, 0,"pd_unpu", 1, 0, NULL},
	{"YUVP", 0x1C0B0000, 4, 0, 0, 0, 0, 0,"pd_yuvp", 1, 0, NULL},
};
