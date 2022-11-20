 #ifndef __SC8830_DMC_DFS_H
#define __SC8830_DMC_DFS_H
#define EMC_DLL_SWITCH_DISABLE_MODE		0x0
#define EMC_DLL_SWITCH_ENABLE_MODE		0x1
#define EMC_DLL_NOT_SWITCH_MODE			0x2
#define EMC_DLL_MODE_MASK			(0x3)
#define EMC_CLK_DIV_OFFSET			2

#define EMC_FREQ_SWITCH_STATUS_OFFSET		(4)
#define EMC_FREQ_SWITCH_COMPLETE			(1)
#define EMC_FREQ_SWITCH_STATUS_MASK			(0xf << EMC_FREQ_SWITCH_STATUS_OFFSET)

#define EMC_DDR_TYPE_LPDDR1			0
#define EMC_DDR_TYPE_LPDDR2			1
#define EMC_DDR_TYPE_LPDDR3			2
#define EMC_DDR_TYPE_DDR2			3
#define EMC_DDR_TYPE_DDR3			4
#define EMC_DDR_TYPE_OFFSET			8
#define EMC_DDR_TYPE_MASK			(0xf << EMC_DDR_TYPE_OFFSET)

#define EMC_CLK_FREQ_OFFSET			(12)
#define EMC_CLK_FREQ_MASK			(0xfff << EMC_CLK_FREQ_OFFSET)

#define EMC_FREQ_NORMAL_SCENE			0x0 //normal lcd power off
#define EMC_FREQ_MP4_SENE			0x1 //play mp4 mode
#define EMC_FREQ_NORMAL_SWITCH_SENE     0x02
#define EMC_FREQ_DEEP_SLEEP_SENE        0x03
#define EMC_FREQ_RESUME_SENE            0x04
#define EMC_FREQ_SENE_OFFSET			24
#define EMC_FREQ_SENE_MASK			(0xf << EMC_FREQ_SENE_OFFSET)

#define EMC_BSP_BPS_200_CLR			0x0
#define EMC_BSP_BPS_200_SET			0x1
#define EMC_BSP_BPS_200_NOT_CHANGE		0x2
#define EMC_BSP_BPS_200_OFFSET			28
#define EMC_BSP_BPS_200_MASK			(0xf << EMC_BSP_BPS_200_OFFSET)

#define EMC_PARAM_IDX_OFFSET		28
#define EMC_PARAM_IDX_MASK		(0XF << EMC_PARAM_IDX_OFFSET)
#define DMC_CHANGE_FREQ_WAIT_TIMEOUT		100
typedef struct
{
    u32 ddr_clk;
    //umctl reg
    u32 umctl2_rfshtmg;
    u32 umctl2_init0;
    u32 umctl2_init1;
    u32 umctl2_init2;
    u32 umctl2_init3;
    u32 umctl2_init4;
    u32 umctl2_init5;
    u32 umctl2_dramtmg0;
    u32 umctl2_dramtmg1;
    u32 umctl2_dramtmg2;
    u32 umctl2_dramtmg3;
    u32 umctl2_dramtmg4;
    u32 umctl2_dramtmg5;
    u32 umctl2_dramtmg6;
    u32 umctl2_dramtmg7;
    u32 umctl2_dramtmg8;
    u32 umctl2_dfitmg0;
    u32 umctl2_dfitmg1;
    //publ reg
    u32 publ_ptr0;
    u32 publ_ptr1;
    u32 publ_dtpr0;
    u32 publ_dtpr1;
    u32 publ_dtpr2;
    u32 publ_mr0;
    u32 publ_mr1;
    u32 publ_mr2;
    u32 publ_mr3;
    u32 publ_dx0gcr;
    u32 publ_dx1gcr;
    u32 publ_dx2gcr;
    u32 publ_dx3gcr;
    u32 publ_dx0dqstr;
    u32 publ_dx1dqstr;
    u32 publ_dx2dqstr;
    u32 publ_dx3dqstr;
}ddr_dfs_val_t;

typedef struct
{
	u32 ddr_clk;

	//umctl reg
	u32 umctl_rfshtmg;
	u32 umctl_dramtmg0;
	u32 umctl_dramtmg1;
	u32 umctl_dramtmg2;
	u32 umctl_dramtmg3;
	u32 umctl_dramtmg4;
	u32 umctl_dramtmg5;
	u32 umctl_dramtmg7;
	u32 umctl_dramtmg8;
	u32 umctl_dfitmg0;
	u32 umctl_init3;
	u32 umctl_zqctl0;
	u32 umctl_zqctl1;

	//publ reg
	u32 publ_mr2;
	u32 publ_dx0gtr;
	u32 publ_dx1gtr;
	u32 publ_dx2gtr;
	u32 publ_dx3gtr;
	u32 publ_pgcr3;
	u32 publ_acmdlr;
	u32 publ_dx0mdlr;
	u32 publ_dx1mdlr;
	u32 publ_dx2mdlr;
	u32 publ_dx3mdlr;
	u32 publ_aclcdlr;
	u32 publ_dx0lcdlr0;
	u32 publ_dx1lcdlr0;
	u32 publ_dx2lcdlr0;
	u32 publ_dx3lcdlr0;
	u32 publ_dx0lcdlr1;
	u32 publ_dx1lcdlr1;
	u32 publ_dx2lcdlr1;
	u32 publ_dx3lcdlr1;
	u32 publ_dx0lcdlr2;
	u32 publ_dx1lcdlr2;
	u32 publ_dx2lcdlr2;
	u32 publ_dx3lcdlr2;
	u32 publ_dsgcr;
	u32 publ_dtpr0;
	u32 publ_dtpr1;
	u32 publ_dtpr2;
	u32 publ_dtpr3;
	u32 resevered;
}ddr_dfs_v2_t;

typedef struct
{
	u32 ddr_clk;

	u32 publ_acmdlr;
	u32 publ_dx0mdlr;
	u32 publ_dx1mdlr;
	u32 publ_dx2mdlr;
	u32 publ_dx3mdlr;

	u32 publ_aclcdlr;
	u32 publ_dx0lcdlr0;
	u32 publ_dx1lcdlr0;
	u32 publ_dx2lcdlr0;
	u32 publ_dx3lcdlr0;
	u32 publ_dx0lcdlr1;
	u32 publ_dx1lcdlr1;
	u32 publ_dx2lcdlr1;
	u32 publ_dx3lcdlr1;
	u32 publ_dx0lcdlr2;
	u32 publ_dx1lcdlr2;
	u32 publ_dx2lcdlr2;
	u32 publ_dx3lcdlr2;
} publ_calc_t;

#define AP_DMC_DFS_MAX_REQ 	3
#define AP_UNVALID_REQ	0
struct dmc_req_array {
	/*if the req[n] is 0 the req[n] is the last request from AP*/
	u32 req[AP_DMC_DFS_MAX_REQ];
};
#endif //__SC8830_DMC_DFS_H
