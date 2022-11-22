#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <asm/io.h>

#include "sr2351_fm_ctrl.h"


struct sprd_2351_interface *fm_rf_ops = NULL;

struct shark_fm_info_t shark_fm_info = {
	1,	/* int_happen;*/
	1,	/*	seek_success;*/
	0,	/*   freq_set;*/
	0,	/*	freq_seek;*/
	0,	/*	freq_offset;*/
	0,	/*	rssi;*/
	0,	/*	rf_rssi;*/
	0,	/*	seek_cnt;*/
	0,	/*	inpwr_sts;*/
	0,	/*	fm_sts;*/
	0,	/*  agc_sts*/
	/*shark_fm_irq_handler,		(*irq_handler)(u32);*/
};

typedef struct {
  uint32_t ver;
  uint8_t  cap;
}rom_callback_func_t;

unsigned int get_shark_chip_id(void)
{
   unsigned int code_addr = 0;
   unsigned int header_phy_addr = 0;
   unsigned int header_addr = 0;
   unsigned int header_offset = 0;
   unsigned int chip_ver=0;
   
   code_addr =(volatile unsigned int) ioremap((unsigned int)0xFFFF0000,0x4000);
   header_phy_addr = (rom_callback_func_t *)(*((unsigned int*)(code_addr +0x20)));
   header_offset=header_phy_addr-0xffff0000;
   header_addr=header_offset+code_addr;
   chip_ver=*(unsigned int *)(header_addr);
	
   iounmap(code_addr);
   
   return chip_ver;
}

static void read_fm_reg(u32 reg_addr, u32 *reg_data)
{
	 *reg_data = sci_glb_read(reg_addr,-1UL);
}

static unsigned int write_fm_reg(u32 reg_addr, u32 val)
{
	sci_glb_write(reg_addr, val, -1UL);
	return 0;
}

static void fm_register_set(void)
{  
	write_fm_reg(FM_REG_FM_CTRL	,		0x00001011);
	write_fm_reg(FM_REG_FM_EN	,		0x000032EE);
	write_fm_reg(FM_REG_CHAN	,		(899-1)*2);
	write_fm_reg(FM_REG_AGC_TBL_CLK,		0x00000000);
	write_fm_reg(FM_REG_SEEK_LOOP	,	1080 - 875);
	write_fm_reg(FM_REG_FMCTL_STI	,	0x0);
	write_fm_reg(FM_REG_BAND_LMT	,	(((1080 - 1) * 2)<<16) | ((875-1)*2));
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
	write_fm_reg(FM_REG_BAND_SPACE	,	0x00000001);
#else
	write_fm_reg(FM_REG_BAND_SPACE	,	0x00000002);
#endif
	write_fm_reg(FM_REG_ADC_INFCTRL,            0);
	write_fm_reg(FM_REG_SEEK_CH_TH	,	2000);
	write_fm_reg(FM_REG_AGC_TBL_RFRSSI0,	0x00020800);
	write_fm_reg(FM_REG_AGC_TBL_RFRSSI1,	(31 << 24) | (5 << 16) | (3000));
	write_fm_reg(FM_REG_AGC_TBL_WBRSSI	,	0x00F60C12);
	write_fm_reg(FM_REG_AGC_IDX_TH	,	0x00020407);
	write_fm_reg(FM_REG_AGC_RSSI_TH	,	0x00F600D8);
	write_fm_reg(FM_REG_SEEK_ADPS	,	0x0CD80000);
	write_fm_reg(FM_REG_STER_PWR	,	0x000001A6);
	write_fm_reg(FM_REG_MONO_PWR	,	0x019C0192);
	write_fm_reg(FM_REG_AGC_CTRL	,	0x09040120);
	write_fm_reg(FM_REG_AGC_ITV_TH	,	0x00FE00EE);
	write_fm_reg(FM_REG_AGC_ADPS0	,	0xFDF70009);
	write_fm_reg(FM_REG_AGC_ADPS1	,	0xFEF80003);
	write_fm_reg(FM_REG_PDP_TH	,		400);
	write_fm_reg(FM_REG_PDP_DEV	,		120);
	write_fm_reg(FM_REG_ADP_ST_CONF	,	0x200501BA);
	write_fm_reg(FM_REG_ADP_LPF_CONF	,	2);
	write_fm_reg(FM_REG_DEPHA_SCAL	,	5);
	write_fm_reg(FM_REG_HW_MUTE	,		0);
	write_fm_reg(FM_REG_SW_MUTE	,		0x01A601A1);
	write_fm_reg(FM_REG_UPD_CTRL	,	0x01013356);
	write_fm_reg(FM_REG_AUD_BLD0	,	0x01AD01A1);
	write_fm_reg(FM_REG_AUD_BLD1	,	0x81A901A5);
	write_fm_reg(FM_REG_AGC_HYS	,		0x00000202);
	write_fm_reg(FM_REG_RF_CFG_DLY	,	20000);
	write_fm_reg(FM_REG_RF_INIT_GAIN,	        0x0303);
	write_fm_reg(FM_REG_AGC_BITS_TBL0	,	0x0000);
	write_fm_reg(FM_REG_AGC_BITS_TBL1	,	0x0001);
	write_fm_reg(FM_REG_AGC_BITS_TBL2	,	0x0301);
	write_fm_reg(FM_REG_AGC_BITS_TBL3	,	0x0302);
	write_fm_reg(FM_REG_AGC_BITS_TBL4	,	0x0303);
	write_fm_reg(FM_REG_AGC_BITS_TBL5	,	0x0313);
	write_fm_reg(FM_REG_AGC_BITS_TBL6	,	0x0323);
	write_fm_reg(FM_REG_AGC_BITS_TBL7	,	0x0333);
	write_fm_reg(FM_REG_AGC_BITS_TBL8	,	0x0335);
	write_fm_reg(FM_REG_AGC_BITS_TBL9	,	0x0000);
	write_fm_reg(FM_REG_AGC_BITS_TBL10	,	0x0001);
	write_fm_reg(FM_REG_AGC_BITS_TBL11	,	0x0301);
	write_fm_reg(FM_REG_AGC_BITS_TBL12	,	0x0302);
	write_fm_reg(FM_REG_AGC_BITS_TBL13	,	0x0303);
	write_fm_reg(FM_REG_AGC_BITS_TBL14	,	0x0313);
	write_fm_reg(FM_REG_AGC_BITS_TBL15	,	0x0323);
	write_fm_reg(FM_REG_AGC_BITS_TBL16	,	0x0333);
	write_fm_reg(FM_REG_AGC_BITS_TBL17	,	0x0335);
	write_fm_reg(FM_REG_AGC_VAL_TBL0	,	7);
	write_fm_reg(FM_REG_AGC_VAL_TBL1	,	21);
	write_fm_reg(FM_REG_AGC_VAL_TBL2	,	29);
	write_fm_reg(FM_REG_AGC_VAL_TBL3	,	35);
	write_fm_reg(FM_REG_AGC_VAL_TBL4	,	41);
	write_fm_reg(FM_REG_AGC_VAL_TBL5	,	47);
	write_fm_reg(FM_REG_AGC_VAL_TBL6	,	53);
	write_fm_reg(FM_REG_AGC_VAL_TBL7	,	59);
	write_fm_reg(FM_REG_AGC_VAL_TBL8	,	71);
	write_fm_reg(FM_REG_AGC_BOND_TBL0	,	-120UL);
	write_fm_reg(FM_REG_AGC_BOND_TBL1	,	-89UL);
	write_fm_reg(FM_REG_AGC_BOND_TBL2	,	-83UL);
	write_fm_reg(FM_REG_AGC_BOND_TBL3	,	-77UL);
	write_fm_reg(FM_REG_AGC_BOND_TBL4	,	-41UL);
	write_fm_reg(FM_REG_AGC_BOND_TBL5	,	-35UL);
	write_fm_reg(FM_REG_AGC_BOND_TBL6	,	-29UL);
	write_fm_reg(FM_REG_AGC_BOND_TBL7	,	-21UL);
	write_fm_reg(FM_REG_AGC_BOND_TBL8	,	-13UL);
	write_fm_reg(FM_REG_AGC_BOND_TBL9	,	10);
	write_fm_reg(FM_REG_SPUR_FREQ0      ,       0x0000081e);
}

void shark_fm_seek_up(void)
{
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
    WRITE_REG(FM_REG_CHAN, shark_fm_info.freq_seek+1);
#else
	write_fm_reg(FM_REG_CHAN, shark_fm_info.freq_seek+2);
#endif
	sci_glb_set(FM_REG_FM_CTRL, BIT_19);
}

void shark_fm_seek_down(void)
{
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
     WRITE_REG(FM_REG_CHAN, shark_fm_info.freq_seek-1);
#else
	write_fm_reg(FM_REG_CHAN, shark_fm_info.freq_seek-2);
#endif
	sci_glb_clr(FM_REG_FM_CTRL, BIT_19);
}



irqreturn_t fm_interrupt(int irq, void *dev_id)
{
	SR2351_PRINT("interrupt bingo~~~~~~~~~~~~~\n");

	return IRQ_HANDLED;
}


void sr2351_fm_config_xtl(void)
{
	sci_glb_set(SHARK_APB_EB0_SET, BIT_20);
	sci_glb_set(SHARK_PMU_SLEEP_CTRL, BIT_8);
}


int sr2351_fm_en(void)
{
#if defined(CONFIG_ARCH_SCX30G)	

	sci_glb_clr(SHARK_PMU_APB_PD_AP_SYS_CFG,BIT_24);
#endif	
	sci_glb_set(FM_REG_FM_EN, BIT_31);

	return 0;
}


int sr2351_fm_dis(void)
{
#if defined(CONFIG_ARCH_SCX30G)	

	sci_glb_set(SHARK_PMU_APB_PD_AP_SYS_CFG,BIT_24);
#endif	
	sci_glb_clr(FM_REG_FM_EN, BIT_31);

	return 0;
}

int sr2351_fm_get_status(int *status)
{
	u32 reg_data = 0;
	read_fm_reg(FM_REG_FM_EN, &reg_data);
	*status = reg_data >> 31;
	return 0;
}

void sr2351_fm_enter_sleep(void)
{
	unsigned int chip_ver=0;

	#ifdef CONFIG_ARCH_SCX30G
	sci_glb_clr(SHARK_PMU_APB_MEM_PD_CFG0,BIT_5|BIT_4|BIT_3|BIT_2|BIT_1|BIT_0);
	#endif

	if(fm_rf_ops != NULL)
	{
		#if defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ARCH_SCX30G)
		/*Disable the RSSI AGC*/
		sci_glb_clr(FM_REG_FM_EN, BIT_2 | BIT_3);
		udelay(5);

		/*Switch the mspi clock*/
		#if defined(CONFIG_ARCH_SCX30G)
		printk("2351 fm enter sleep switch clock\n");
		sci_glb_clr(SHARK_MSPI_CLK_SWITCH, BIT_0);
		#elif defined(CONFIG_ARCH_SCX15)
		sci_glb_set(SHARK_MSPI_CLK_SWITCH, BIT_0 | BIT_1);
		#endif

		/*Enable the RSSI AGC*/
		sci_glb_set(FM_REG_FM_EN, BIT_2 | BIT_3);

		#elif defined(CONFIG_ARCH_SCX35)

		chip_ver =  get_shark_chip_id();
		 //TROUT_PRINT("chip_ver in trout_fm_enter_sleep :  %d\r\n", chip_ver);
		/*
		* BD version is the latest version for shark chip .
		* In this version, hardware fix the FM sleep bug about the Sensitivity.
		* So we need modify the sleep action based on the shark chip version.
		* The chip id of BD version is 1 ,and the one of the old version is 2,
		* such as BA/BB/BC 
		*/
		if(chip_ver != 0)
    		{
	    		if(SHARK_CHIP_BD == chip_ver)
	    		{
		  	   /*Disable the RSSI AGC*/
			   sci_glb_clr(FM_REG_FM_EN, BIT_2 | BIT_3);
			   udelay(5);

			    /*Switch the mspi clock*/
			   sci_glb_set(SHARK_MSPI_CLK_SWITCH, BIT_0 | BIT_1);

			    /*Enable the RSSI AGC*/
		  	   sci_glb_set(FM_REG_FM_EN, BIT_2 | BIT_3);
	    		}else
	    		{
	    		   sci_glb_clr(FM_REG_FM_EN,BIT_2|BIT_3);
	    		}
    		}

		#endif

		fm_rf_ops->write_reg(FM_SR2351_RX_GAIN, 0x0313);
	}
}

void sr2351_fm_exit_sleep(void)
{
    unsigned int chip_ver=0;

	if(fm_rf_ops != NULL)
	{
		#if defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ARCH_SCX30G)
		/*Disable the RSSI AGC*/
		sci_glb_clr(FM_REG_FM_EN, BIT_2 | BIT_3);
		udelay(5);

		/*Switch the mspi clock*/
		#if defined(CONFIG_ARCH_SCX30G)
		printk("2351 fm exit sleep switch clock\n");
		sci_glb_set(SHARK_MSPI_CLK_SWITCH, BIT_0);
		#elif defined(CONFIG_ARCH_SCX15)
		sci_glb_clr(SHARK_MSPI_CLK_SWITCH, BIT_0 | BIT_1);
		#endif

		/*Enable the RSSI AGC*/
		sci_glb_set(FM_REG_FM_EN, BIT_2 | BIT_3);

		#elif defined(CONFIG_ARCH_SCX35)

		chip_ver =  get_shark_chip_id();
		//TROUT_PRINT("chip_ver in trout_fm_exit_sleep :  %d\r\n", chip_ver);
		/*
		* BD version is the latest version for shark chip .
		* In this version, hardware fix the FM sleep bug about the Sensitivity.
		* So we need modify the sleep action based on the shark chip version.
		* The chip id of BD version is 1 ,and the one of the old version is 2,
		* such as BA/BB/BC 
		*/
		if(chip_ver != 0)
		{
			if(SHARK_CHIP_BD == chip_ver)
			{
			  /*Disable the RSSI AGC*/
			  sci_glb_clr(FM_REG_FM_EN, BIT_2 | BIT_3);
			  udelay(5);

			  /*Switch the mspi clock*/
			  sci_glb_clr(SHARK_MSPI_CLK_SWITCH, BIT_0 | BIT_1);

			  /*Enable the RSSI AGC*/
			  sci_glb_set(FM_REG_FM_EN, BIT_2 | BIT_3);
			}else
			{
			  sci_glb_set(FM_REG_FM_EN,BIT_2|BIT_3);
			}
		}	

		#endif
	}

	#ifdef CONFIG_ARCH_SCX30G
	sci_glb_set(SHARK_PMU_APB_MEM_PD_CFG0,BIT_5|BIT_3|BIT_1);
	#endif
}

void sr2351_fm_mute(void)
{
	sci_glb_set(FM_REG_HW_MUTE, BIT_0);
}

void sr2351_fm_unmute(void)
{
	sci_glb_clr(FM_REG_HW_MUTE, BIT_0);
}



void shark_fm_int_en(void)
{
	shark_fm_info.int_happen = 1;
	write_fm_reg(FM_REG_INT_EN, 0x01);/*enable fm int*/

	enable_irq(INT_NUM_FM_test);
}

void shark_fm_int_dis(void)
{
	shark_fm_info.int_happen = 1;
	write_fm_reg(FM_REG_INT_EN, 0x0);       /*disable fm int*/

}

void shark_fm_int_clr(void)
{
	write_fm_reg(FM_REG_INT_CLR, 0x1);
}

int enable_shark_fm(void)
{
	u32 reg_data = 0;
	read_fm_reg(SHARK_APB_EB0, &reg_data);
	if (!(reg_data & BIT_1)) {
		  sci_glb_set(SHARK_APB_EB0, BIT_1);

		  /*reset fm*/
		  sci_glb_set(SHARK_APB_RST0, BIT_1);
		  msleep(20);
		  sci_glb_clr(SHARK_APB_RST0, BIT_1);
	}

	return 0;
}

int shark_write_regs(u32 reg_addr, u32 reg_cnt, u32 *reg_data)
{
	u32 i = 0;

	for (i = 0; i < reg_cnt; i++) {
		if (write_fm_reg(reg_addr+i*4, reg_data[i]) == -1)
			return -1;
	}
	return 0;
}

int shark_write_reg_cfg(struct shark_reg_cfg_t *reg_cfg, u32 cfg_cnt)
{
	u32 i;
	for (i = 0; i < cfg_cnt; i++) {
		if (write_fm_reg(reg_cfg[i].reg_addr, reg_cfg[i].reg_data) == -1)
			return -1;
	}
	return 0;
}

int shark_fm_reg_cfg(void)
{
	u32 reg_data;
	u32 fm_reg_agc_db_tbl[FM_REG_AGC_DB_TBL_CNT] = {
		15848,	12589,		10000,	7943,	6309,
		5011,	3981,		3162,	2511,	1995,
		1584,	1258,		1000,	794,	630,
		501,	398,		316,	251,	199,
		158,	125,		100,	79,		63,
		50,		39,			31,		25,		19,
		15,		12,			10,		7,		6,
		5,		3,			3,		2,		1,
		1
	};
#if 0	
	if (shark_write_reg_cfg(fm_reg_init_des, \
		ARRAY_SIZE(fm_reg_init_des)) == -1) {
		/*return FALSE;*/
	}
#else
    fm_register_set();
#endif	
	if (write_fm_reg(FM_REG_AGC_TBL_CLK, 0x00000001) == -1)
		return -1;
	while (1) {
		msleep(20);
		read_fm_reg(FM_REG_AGC_TBL_CLK, &reg_data);
		if (reg_data>>17)
			break;
	}
	if (shark_write_regs(FM_REG_AGC_DB_TBL_BEGIN, FM_REG_AGC_DB_TBL_CNT, \
		fm_reg_agc_db_tbl) == -1)
		return -1;
	msleep(100);
	if (write_fm_reg(FM_REG_AGC_TBL_CLK, 0x00000000) == -1)
		;/*return FALSE;*/
	return 0;
}

int shark_fm_cfg_rf_reg(void)
{
	fm_rf_ops->write_reg(FM_SR2351_DCOC_CAL_TIMER, 0x0FFF);
	fm_rf_ops->write_reg(FM_SR2351_RC_TUNER, 0xFFFF);
	fm_rf_ops->write_reg(FM_SR2351_RX_ADC_CLK, 0x001F);
	fm_rf_ops->write_reg(FM_SR2351_RX_ADC_CLK, 0x009F);
	fm_rf_ops->write_reg(FM_SR2351_ADC_CLK, 0x0201);
	fm_rf_ops->write_reg(FM_SR2351_OVERLOAD_DET, 0x8022);

	fm_rf_ops->write_reg(FM_SR2351_ADC_CLK, 0x0601);
	fm_rf_ops->write_reg(FM_SR2351_RX_GAIN, 0x0335);
	fm_rf_ops->write_reg(FM_SR2351_MODE, 0x0011);
	fm_rf_ops->write_reg(FM_SR2351_FREQ, 0x07A6);
	fm_rf_ops->write_reg(FM_SR2351_RX_GAIN, 0x0335);

	msleep(20);

	return 0;
}

int sr2351_fm_init(void)
{
	sprd_get_rf2351_ops(&fm_rf_ops);
	fm_rf_ops->mspi_enable();

	sr2351_fm_config_xtl();
	
	if (enable_shark_fm() != 0)
		return -1;
	msleep(20);

	shark_fm_int_en();

	if (shark_fm_reg_cfg() == -1)
		return -1;
	if (shark_fm_cfg_rf_reg() == -1)
		return -1;

	write_fm_reg(FM_REG_RF_CTL, ((0x404<<16)|(0x0402)));
	write_fm_reg(FM_REG_RF_CTL1, ((0x0466<<16)|(0x0043)));

	shark_fm_info.freq_seek = 860*2;
	/*result = request_irq(INT_NUM_FM_test,
	  fm_interrupt, IRQF_DISABLED, "sr2351_FM", NULL);*/
	return 0;
}

void __sr2351_fm_get_status(void)
{
	read_fm_reg(FM_REG_SEEK_CNT, &shark_fm_info.seek_cnt);
	read_fm_reg(FM_REG_CHAN_FREQ_STS, &shark_fm_info.freq_seek);
	read_fm_reg(FM_REG_FREQ_OFF_STS, &shark_fm_info.freq_offset);
	read_fm_reg(FM_REG_RF_RSSI_STS, &shark_fm_info.rf_rssi);
	read_fm_reg(FM_REG_RSSI_STS, &shark_fm_info.rssi);
	read_fm_reg(FM_REG_INPWR_STS, &shark_fm_info.inpwr_sts);
	read_fm_reg(FM_REG_WBRSSI_STS, &shark_fm_info.fm_sts);
	read_fm_reg(FM_REG_AGC_TBL_STS, &shark_fm_info.agc_sts);

}

void __sr2351_fm_show_status(void)
{
	SR2351_PRINT("FM_REG_SEEK_CNT     : %08x (%d)\r\n", \
	shark_fm_info.seek_cnt, shark_fm_info.seek_cnt);
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
	SR2351_PRINT("FM_REG_CHAN_FREQ_STS: %08x (%d)\r\n", \
	shark_fm_info.freq_seek, shark_fm_info.freq_seek*5 + 10);
#else	
	SR2351_PRINT("FM_REG_CHAN_FREQ_STS: %08x (%d)\r\n", \
	shark_fm_info.freq_seek, ((shark_fm_info.freq_seek >> 1) + 1));
#endif
	SR2351_PRINT("FM_REG_FREQ_OFF_STS : %08x (%d)\r\n", \
	shark_fm_info.freq_offset, shark_fm_info.freq_offset);
	SR2351_PRINT("FM_REG_RF_RSSI_STS  : %08x\r\n", \
	shark_fm_info.rf_rssi);
	SR2351_PRINT("FM_REG_INPWR_STS    : %08x\r\n", \
	shark_fm_info.inpwr_sts);
	SR2351_PRINT("FM_REG_WBRSSI_STS   : %08x\r\n", \
	shark_fm_info.fm_sts);
	SR2351_PRINT("FM_REG_RSSI_STS     : %08x\r\n", \
	shark_fm_info.rssi);
	SR2351_PRINT("FM_REG_AGC_TBL_STS  : %08x\r\n", \
	shark_fm_info.agc_sts);

	SR2351_PRINT("fm work:%d, fm iq:%04x\r\n", \
	(shark_fm_info.fm_sts >> 20) & 1, \
	(shark_fm_info.fm_sts >> 16) & 0xf);
	SR2351_PRINT("pilot detect:%d, stero:%d, seek fail:%d, rssi:%08x\r\n", \
	(shark_fm_info.rssi >> 18) & 0x3, \
	(shark_fm_info.rssi >> 17) & 0x1, \
	(shark_fm_info.rssi >> 16) & 0x1, shark_fm_info.rssi & 0xff);
}

int sr2351_fm_deinit(void)
{
    fm_rf_ops->write_reg(FM_SR2351_MODE, 0x0000);
    fm_rf_ops->write_reg(FM_SR2351_ADC_CLK, 0x201);

	sci_glb_clr(SHARK_PMU_SLEEP_CTRL, BIT_8);

    fm_rf_ops->mspi_disable();
    sprd_put_rf2351_ops(&fm_rf_ops);

    return 0;
}

int shark_fm_wait_int(int time_out)
{
       int ret = 0;
	ulong jiffies_comp = 0;
	u8 is_timeout = 1;
	u32 seek_succes = 0;
	unsigned int start_msecond = 0;
	unsigned int end_msecond = 0;

	jiffies_comp = jiffies;
	start_msecond = jiffies_to_msecs(jiffies);
	SR2351_PRINT("start_msecond: %u\n", start_msecond);
	do {
		msleep(20);
		read_fm_reg(FM_REG_INT_STS, &seek_succes);

		is_timeout = time_after(jiffies,
					jiffies_comp +
					msecs_to_jiffies(time_out));

		if (is_timeout && (seek_succes==0)) 
                {
			SR2351_PRINT("FM search timeout.");
			sr2351_fm_dis();
		        ret = SR2351_SEEK_TIMEOUT;                       
		}

		if (seek_succes) 
                {
			SR2351_PRINT("FM found frequency.");
			break;
		}
	} while (seek_succes == 0 && is_timeout == 0);

	end_msecond = jiffies_to_msecs(jiffies);
	SR2351_PRINT("end_msecond: %u\n", end_msecond);
	SR2351_PRINT("used time: %ums\n", end_msecond - start_msecond);

	__sr2351_fm_get_status();
	__sr2351_fm_show_status();

	SR2351_PRINT("\n");
	return ret;
}

int sr2351_fm_set_tune(u32 freq)
{
	u32 reg_data;
	sr2351_fm_dis();
	shark_fm_int_dis();
	shark_fm_int_clr();
	/*FM_ADC_Tuning_Value_overwrite(chan);*/
	write_fm_reg(FM_REG_FMCTL_STI, FM_CTL_STI_MODE_TUNE);	/* tune mode*/
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
	write_fm_reg(FM_REG_CHAN, ((freq-10)/5) & 0xffff);
#else
	write_fm_reg(FM_REG_CHAN, ((freq-1)*2) & 0xffff);
#endif
	shark_fm_int_en();
	sr2351_fm_en();

	read_fm_reg(FM_REG_CHAN, &reg_data);
	SR2351_PRINT("chan reg_data is %d\n", reg_data);

	shark_fm_wait_int(3000);

	/*dump_fm_regs();*/

	shark_fm_int_clr();
	return 0;
}

int sr2351_fm_seek(u32 frequency, u32 seek_dir, u32 time_out, u32 *freq_found)
{
	int ret = 0;

	SR2351_PRINT("FM seek, freq(%i) dir(%i) timeout(%i).",frequency, seek_dir, time_out);

	if (frequency < MIN_FM_FREQ || frequency > MAX_FM_FREQ)
        {
		SR2351_PRINT("out of freq range: %i", frequency);
		frequency = MIN_FM_FREQ;
	}

#ifdef CONFIG_FM_SEEK_STEP_50KHZ
	shark_fm_info.freq_seek = ((frequency-10)/5);
#else
	shark_fm_info.freq_seek = ((frequency-1)*2);
#endif

	seek_dir ^= 0x1;
	sr2351_fm_dis();
	shark_fm_int_clr();

	if (seek_dir == 0)
		shark_fm_seek_up();
	 else
		shark_fm_seek_down();

	/*Handel False station,change the spuer value when stark seek*/
	write_fm_reg(FM_REG_SPUR_RM_CTL, FM_SPUR_RM_CTL_SET_VALUE);
	write_fm_reg(FM_REG_SPUR_DC_RM_CTL, FM_SPUR_DC_RM_CTL_SET_VALUE);
	write_fm_reg(FM_REG_FMCTL_STI, FM_CTL_STI_MODE_SEEK);	/* seek mode*/

	shark_fm_int_en();
	sr2351_fm_en();
	ret = shark_fm_wait_int(time_out);
	if (ret == SR2351_SEEK_TIMEOUT)
       {
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
            if (seek_dir == 0){
                if ( MIN_FM_FREQ == (shark_fm_info.freq_seek*5 + 10)){
                           shark_fm_info.freq_seek  =  (MAX_FM_FREQ -10 ) /5;
		   		}
			   else{
				    shark_fm_info.freq_seek -= 1;
			   }
            }
		     else{
	             if ( MAX_FM_FREQ == shark_fm_info.freq_seek*5 + 10){
	                        shark_fm_info.freq_seek  =  (MIN_FM_FREQ -10 ) /5;
			   }
			   else{
				    shark_fm_info.freq_seek += 1;
			   }
		     }
            SR2351_PRINT("seek failed!");
            SR2351_PRINT("freq=%d",shark_fm_info.freq_seek*5 + 10);

#else
            if (seek_dir == 0)
            {
                if ( MIN_FM_FREQ == ((shark_fm_info.freq_seek >> 1) + 1))
                {
                    shark_fm_info.freq_seek  =  (MAX_FM_FREQ -1 ) * 2;
		}
		else
                {
	            shark_fm_info.freq_seek -= 2;
		}
            }
	    else
            {
                if ( MAX_FM_FREQ == ((shark_fm_info.freq_seek >> 1) + 1))
                {
                    shark_fm_info.freq_seek  =  (MIN_FM_FREQ -1 ) * 2;
		}
		else
                {
	            shark_fm_info.freq_seek += 2;
	        }
	     }
            SR2351_PRINT("seek time out!");
            SR2351_PRINT("freq=%d",((shark_fm_info.freq_seek >> 1) + 1));
#endif
        }
		
	/*dump_fm_regs();*/
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
	*freq_found = shark_fm_info.freq_seek*5 + 10;
#else
	*freq_found = (shark_fm_info.freq_seek >> 1) + 1;
#endif

	shark_fm_int_clr();

	/*Handel False station spuer,restore the default value when stop seek*/
	write_fm_reg(FM_REG_SPUR_RM_CTL, FM_SPUR_RM_CTL_DEFAULT_VALUE);
	write_fm_reg(FM_REG_SPUR_DC_RM_CTL, FM_SPUR_DC_RM_CTL_DEFAULT_VALUE);

	return ret;
}

int sr2351_fm_get_frequency(u32 *freq)
{
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
	*freq = shark_fm_info.freq_seek*5 + 10;
#else
	*freq = ((shark_fm_info.freq_seek >> 1) + 1);
#endif
	return 0;
}

int sr2351_fm_stop_seek(void)
{
	u32 reg_value = 0x0;
    
	SR2351_PRINT("FM stop seek.");

	/* clear seek enable bit of seek register. */
	read_fm_reg(FM_REG_FMCTL_STI, &reg_value);
	if (reg_value & FM_CTL_STI_MODE_SEEK)
        {
		reg_value |= FM_CTL_STI_MODE_NORMAL;
		write_fm_reg(FM_REG_FMCTL_STI, reg_value);
	}

	return 0;
}

int sr2351_fm_get_rssi(u32 *rssi)
{
  u32 reg_data;

  read_fm_reg(FM_REG_INPWR_STS, &reg_data);
  *rssi = reg_data;
  SR2351_PRINT("rssi value in sr2351_fm_get_rssi() is %08x\r\n",*rssi);
  
  return 0;
}
