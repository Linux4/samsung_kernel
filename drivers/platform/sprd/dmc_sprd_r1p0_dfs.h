#ifndef _DMC_SPRD_R1P0_DFS_H__
#define _DMC_SPRD_R1P0_DFS_H__

typedef struct __PIKE_DMC_REG_INFO {
	volatile unsigned int dmc_cfg0;						/*addr:0x0*/
	volatile unsigned int dmc_cfg1;						/*addr:0x4*/
	volatile unsigned int pad0[6];			
	volatile unsigned int ahbaxireg[35];				/*addr:0x20*/
	volatile unsigned int dmc_sts3;						/*addr:0xac*/
	volatile unsigned int pad1[20];			
	volatile unsigned int dmc_dcfg0;					/*addr:0x100*/
	volatile unsigned int dmc_dcfg1;					/*addr:0x104*/
	volatile unsigned int dmc_dcfg2;					/*addr:0x108*/
	volatile unsigned int dmc_dcfg3;					/*addr:0x10c*/
	volatile unsigned int dmc_dcfg4;					/*addr:0x110*/	
	volatile unsigned int pad2[3];				
	volatile unsigned int dmc_lpcfg0;					/*addr:0x120*/
	volatile unsigned int dmc_lpcfg1;					/*addr:0x124*/
	volatile unsigned int dmc_lpcfg2;					/*addr:0x128*/
	volatile unsigned int dmc_lpcfg3;					/*addr:0x12c*/
	volatile unsigned int pad3[4];				
	volatile unsigned int dmc_dtmg_f0[11];				/*addr:0x140*/
	volatile unsigned int pad4[1];
	volatile unsigned int dmc_dtmg_f1[11];				/*addr:0x170*/
	volatile unsigned int pad5[1];
	volatile unsigned int dmc_dtmg_f2[11];				/*addr:0x1a0*/
	volatile unsigned int pad6[1];
	volatile unsigned int dmc_dtmg_f3[11];				/*addr:0x1d0*/
	volatile unsigned int pad7[1];	
	volatile unsigned int dmc_bist[27];					/*addr:0x200*/
	volatile unsigned int pad8[37];
	volatile unsigned int dmc_cfg_dll_ac;				/*addr:0x300*/
	volatile unsigned int dmc_sts_dll_ac;				/*addr:0x304*/
	volatile unsigned int dmc_clkwr_dll_ac;				/*addr:0x308*/
	volatile unsigned int dmc_addr_out0_dll_ac;			/*addr:0x30c*/
	volatile unsigned int pad9[1];
	volatile unsigned int dmc_addr_out1_dll_ac;			/*addr:0x314*/
	volatile unsigned int pad10[1];
	volatile unsigned int dmc_cmd_out0_dll_ac;			/*addr:0x31c*/
	volatile unsigned int pad11[1];
	volatile unsigned int dmc_cmd_out1_dll_ac;			/*addr:0x324*/
	volatile unsigned int pad12[54];
	volatile unsigned int dmc_cfg_dll_ds0;				/*addr:0x400*/
	volatile unsigned int dmc_sts_dll_ds0;				/*addr:0x404*/
	volatile unsigned int dmc_clkwr_dll_ds0;			/*addr:0x408*/
	volatile unsigned int dmc_dqsin_pos_dll_ds0;		/*addr:0x40c*/
	volatile unsigned int dmc_dqsin_neg_dll_ds0;		/*addr:0x410*/
	volatile unsigned int dmc_dqsgate_pre_dll_ds0;		/*addr:0x414*/
	volatile unsigned int dmc_dqsgate_pst_dll_ds0;		/*addr:0x418*/
	volatile unsigned int dmc_date_out_dll_ds0;			/*addr:0x41c*/
	volatile unsigned int pad13[1];
	volatile unsigned int dmc_data_in_dll_ds0;			/*addr:0x424*/
	volatile unsigned int pad14[1];
	volatile unsigned int dmc_dmdqs_inout_dll_ds0;		/*addr:0x42c*/
	volatile unsigned int pad15[52];
	volatile unsigned int dmc_cfg_dll_ds1;				/*addr:0x500*/
	volatile unsigned int dmc_sts_dll_ds1;				/*addr:0x504*/
	volatile unsigned int dmc_clkwr_dll_ds1;			/*addr:0x508*/
	volatile unsigned int dmc_dqsin_pos_dll_ds1;		/*addr:0x50c*/
	volatile unsigned int dmc_dqsin_neg_dll_ds1;		/*addr:0x510*/
	volatile unsigned int dmc_dqsgate_pre_dll_ds1;		/*addr:0x514*/
	volatile unsigned int dmc_dqsgate_pst_dll_ds1;		/*addr:0x518*/
	volatile unsigned int dmc_date_out_dll_ds1;			/*addr:0x51c*/
	volatile unsigned int pad16[1];
	volatile unsigned int dmc_data_in_dll_ds1;			/*addr:0x524*/
	volatile unsigned int pad17[1];
	volatile unsigned int dmc_dmdqs_inout_dll_ds1;		/*addr:0x52c*/
	volatile unsigned int pad18[52];	
	volatile unsigned int dmc_cfg_dll_ds2;				/*addr:0x600*/
	volatile unsigned int dmc_sts_dll_ds2;				/*addr:0x604*/
	volatile unsigned int dmc_clkwr_dll_ds2;			/*addr:0x608*/
	volatile unsigned int dmc_dqsin_pos_dll_ds2;		/*addr:0x60c*/
	volatile unsigned int dmc_dqsin_neg_dll_ds2;		/*addr:0x610*/
	volatile unsigned int dmc_dqsgate_pre_dll_ds2;		/*addr:0x614*/
	volatile unsigned int dmc_dqsgate_pst_dll_ds2;		/*addr:0x618*/
	volatile unsigned int dmc_date_out_dll_ds2;			/*addr:0x61c*/
	volatile unsigned int pad19[1];
	volatile unsigned int dmc_data_in_dll_ds2;			/*addr:0x624*/
	volatile unsigned int pad20[1];
	volatile unsigned int dmc_dmdqs_inout_dll_ds2;		/*addr:0x62c*/
	volatile unsigned int pad21[52];
	volatile unsigned int dmc_cfg_dll_ds3;				/*addr:0x700*/
	volatile unsigned int dmc_sts_dll_ds3;				/*addr:0x704*/
	volatile unsigned int dmc_clkwr_dll_ds3;			/*addr:0x708*/
	volatile unsigned int dmc_dqsin_pos_dll_ds3;		/*addr:0x70c*/
	volatile unsigned int dmc_dqsin_neg_dll_ds3;		/*addr:0x710*/
	volatile unsigned int dmc_dqsgate_pre_dll_ds3;		/*addr:0x714*/
	volatile unsigned int dmc_dqsgate_pst_dll_ds3;		/*addr:0x718*/
	volatile unsigned int dmc_date_out_dll_ds3;			/*addr:0x71c*/
	volatile unsigned int pad22[1];
	volatile unsigned int dmc_data_in_dll_ds3;			/*addr:0x724*/
	volatile unsigned int pad23[1];
	volatile unsigned int dmc_dmdqs_inout_dll_ds3;		/*addr:0x72c*/	
}PIKE_DMC_REG_INFO, *PIKE_DMC_REG_INFO_PTR;


#endif
