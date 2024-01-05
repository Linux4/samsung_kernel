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
#include "trout_fm_ctrl.h"
#include "trout_rf_shark.h"
#include <linux/sprd_2351.h>



struct sprd_2351_interface *fm_rf_ops = NULL;


void shark_fm_seek_up(void)
{
	u32 reg_data;
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
        WRITE_REG(FM_REG_CHAN, shark_fm_info.freq_seek+1);
#else
	WRITE_REG(FM_REG_CHAN, shark_fm_info.freq_seek+2);
#endif
	READ_REG(FM_REG_FM_CTRL, &reg_data);
	reg_data |= BIT_19;
	WRITE_REG(FM_REG_FM_CTRL, reg_data);
}

void shark_fm_seek_down(void)
{
	u32 reg_data;
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
        WRITE_REG(FM_REG_CHAN, shark_fm_info.freq_seek-1);
#else
	WRITE_REG(FM_REG_CHAN, shark_fm_info.freq_seek-2);
#endif
	READ_REG(FM_REG_FM_CTRL, &reg_data);
	reg_data &= ~BIT_19;
	WRITE_REG(FM_REG_FM_CTRL, reg_data);
}



irqreturn_t fm_interrupt(int irq, void *dev_id)
{
	TROUT_PRINT("interrupt bingo~~~~~~~~~~~~~\n");

	return IRQ_HANDLED;
}


void trout_fm_config_xtl(void)
{
	u32 reg_data;
	
    //READ_REG(SHARK_AON_CLK_FM_CFG, &reg_data);
	//reg_data |= 1;
	//WRITE_REG(SHARK_AON_CLK_FM_CFG, reg_data);
		
	READ_REG(SHARK_APB_EB0_SET, &reg_data);
	reg_data |= 1<<20;
	WRITE_REG(SHARK_APB_EB0_SET, reg_data);
	READ_REG(SHARK_PMU_SLEEP_CTRL, &reg_data);
	reg_data |= 1<<8;
	WRITE_REG(SHARK_PMU_SLEEP_CTRL, reg_data);
}


int trout_fm_en(void)
{
	u32 reg_data;
	READ_REG(FM_REG_FM_EN, &reg_data);
	reg_data |= BIT_31;
	WRITE_REG(FM_REG_FM_EN, reg_data);

	return 0;
}


int trout_fm_dis(void)
{
	u32 reg_data;
	READ_REG(FM_REG_FM_EN, &reg_data);
	reg_data &= ~BIT_31;
	WRITE_REG(FM_REG_FM_EN, reg_data);

	return 0;
}

int trout_fm_get_status(int *status)
{
	u32 reg_data = 0;
	READ_REG(FM_REG_FM_EN, &reg_data);
	*status = reg_data >> 31;
	return 0;
}

void trout_fm_enter_sleep(void)
{
	u32 reg_data;

      if(fm_rf_ops != NULL) 
      	{
	    READ_REG(FM_REG_FM_EN, &reg_data);
	    reg_data &= ~(BIT_2 | BIT_3);
	    WRITE_REG(FM_REG_FM_EN, reg_data);
    
           fm_rf_ops->write_reg(0x404, 0x0313);
      	}   
}

void trout_fm_exit_sleep(void)
{
	u32 reg_data;
	
	if(fm_rf_ops != NULL) 
      	{
	    READ_REG(FM_REG_FM_EN, &reg_data);
	    reg_data |= (BIT_2 | BIT_3);
	    WRITE_REG(FM_REG_FM_EN, reg_data);
	}
}

void trout_fm_mute(void)
{
	u32 reg_data;

	READ_REG(FM_REG_HW_MUTE, &reg_data);
	reg_data |= BIT_0;
	WRITE_REG(FM_REG_HW_MUTE, reg_data);
}

void trout_fm_unmute(void)
{
	u32 reg_data;

	READ_REG(FM_REG_HW_MUTE, &reg_data);
	reg_data &= ~BIT_0;
	WRITE_REG(FM_REG_HW_MUTE, reg_data);
}



void shark_fm_int_en()
{
	shark_fm_info.int_happen = 1;
	WRITE_REG(FM_REG_INT_EN, 0x01);/*enable fm int*/

	enable_irq(INT_NUM_FM_test);
}

void shark_fm_int_dis()
{
	shark_fm_info.int_happen = 1;
	WRITE_REG(FM_REG_INT_EN, 0x0);       /*disable fm int*/

}

void shark_fm_int_clr(void)
{
	WRITE_REG(FM_REG_INT_CLR, 0x1);
}

int enable_shark_fm(void)
{
	u32 reg_data = 0;
	READ_REG(SHARK_APB_EB0, &reg_data);
	if (!(reg_data & BIT_1)) {
		reg_data |= BIT_1;
		WRITE_REG(SHARK_APB_EB0, reg_data);  /*enable fm*/
		READ_REG(SHARK_APB_RST0, &reg_data);/*return FALSE;*/

		reg_data |= BIT_1;
		WRITE_REG(SHARK_APB_RST0, reg_data); /*reset fm*/
		msleep(20);
		reg_data &= (~BIT_1);
		WRITE_REG(SHARK_APB_RST0, reg_data); /*reset fm*/
	}

	return 0;
}

int shark_write_regs(u32 reg_addr, u32 reg_cnt, u32 *reg_data)
{
	u32 i = 0;

	for (i = 0; i < reg_cnt; i++) {
		if (WRITE_REG(reg_addr+i*4, reg_data[i]) == -1)
			return -1;
	}
	return 0;
}

int shark_write_reg_cfg(struct shark_reg_cfg_t *reg_cfg, u32 cfg_cnt)
{
	u32 i;
	for (i = 0; i < cfg_cnt; i++) {
		if (WRITE_REG(reg_cfg[i].reg_addr, reg_cfg[i].reg_data) == -1)
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
	if (shark_write_reg_cfg(fm_reg_init_des, \
		ARRAY_SIZE(fm_reg_init_des)) == -1) {
		/*return FALSE;*/
	}
	if (WRITE_REG(FM_REG_AGC_TBL_CLK, 0x00000001) == -1)
		return -1;
	while (1) {
		msleep(20);
		READ_REG(FM_REG_AGC_TBL_CLK, &reg_data);
		if (reg_data>>17)
			break;
	}
	if (shark_write_regs(FM_REG_AGC_DB_TBL_BEGIN, FM_REG_AGC_DB_TBL_CNT, \
		fm_reg_agc_db_tbl) == -1)
		return -1;
	msleep(100);
	if (WRITE_REG(FM_REG_AGC_TBL_CLK, 0x00000000) == -1)
		;/*return FALSE;*/
	return 0;
}

int shark_fm_cfg_rf_reg(void)
{
	fm_rf_ops->write_reg(0x471, 0x0FFF);
        fm_rf_ops->write_reg(0x477, 0xFFFF);
        fm_rf_ops->write_reg(0x478, 0x001F);
        fm_rf_ops->write_reg(0x478, 0x009F);
        fm_rf_ops->write_reg(0x06F, 0x0201);
        fm_rf_ops->write_reg(0x431, 0x8022);

        fm_rf_ops->write_reg(0x06F, 0x0601);
	fm_rf_ops->write_reg(0x404, 0x0335);
	fm_rf_ops->write_reg(0x400, 0x0011);
	fm_rf_ops->write_reg(0x402, 0x07A6);
	fm_rf_ops->write_reg(0x404, 0x0335);

	msleep(20);

	return 0;
}

int trout_fm_init(void)
{
	u32 reg_data;

	sprd_get_rf2351_ops(&fm_rf_ops);
	fm_rf_ops->mspi_enable();

	trout_fm_config_xtl();
	/*added by xuede to route FMIQD0 to IIS0DI and FMIQD1 to IISD0DO*/
	READ_REG(PINMAP_FOR_FMIQ, &reg_data);
	reg_data |= 3<<16;
	WRITE_REG(PINMAP_FOR_FMIQ, reg_data);
	READ_REG(PINMAP_FOR_IIS0DO, &reg_data);
	reg_data |= 2<<4;
	WRITE_REG(PINMAP_FOR_IIS0DO, reg_data);
	READ_REG(PINMAP_FOR_IIS0DI, &reg_data);
	reg_data |= 2<<4;
	WRITE_REG(PINMAP_FOR_IIS0DI, reg_data);

	READ_REG(PINMAP_FOR_IIS0DO, &reg_data);
	reg_data |= 2;
	WRITE_REG(PINMAP_FOR_IIS0DO, reg_data);

	READ_REG(PINMAP_FOR_IIS0DI, &reg_data);
	reg_data |= 2;
	WRITE_REG(PINMAP_FOR_IIS0DI, reg_data);
	/* added end*/

	if (enable_shark_fm() != 0)
		return -1;
	msleep(20);

	shark_fm_int_en();

	if (shark_fm_reg_cfg() == -1)
		return -1;
	if (shark_fm_cfg_rf_reg() == -1)
		return -1;
	WRITE_REG(FM_REG_RF_CTL, ((0x404<<16)|(0x0402)));
	WRITE_REG(FM_REG_RF_CTL1, ((0x0466<<16)|(0x0043)));

        



	shark_fm_info.freq_seek = 860*2;
	/*result = request_irq(INT_NUM_FM_test,\
	  fm_interrupt, IRQF_DISABLED, "Trout_FM", NULL);*/
	return 0;
}

void __trout_fm_get_status(void)
{
	READ_REG(FM_REG_SEEK_CNT, &shark_fm_info.seek_cnt);
	READ_REG(FM_REG_CHAN_FREQ_STS, &shark_fm_info.freq_seek);
	READ_REG(FM_REG_FREQ_OFF_STS, &shark_fm_info.freq_offset);
	READ_REG(FM_REG_RF_RSSI_STS, &shark_fm_info.rf_rssi);
	READ_REG(FM_REG_RSSI_STS, &shark_fm_info.rssi);
	READ_REG(FM_REG_INPWR_STS, &shark_fm_info.inpwr_sts);
	READ_REG(FM_REG_WBRSSI_STS, &shark_fm_info.fm_sts);
	READ_REG(FM_REG_AGC_TBL_STS, &shark_fm_info.agc_sts);

}

void __trout_fm_show_status(void)
{
	TROUT_PRINT("FM_REG_SEEK_CNT     : %08x (%d)\r\n", \
		shark_fm_info.seek_cnt, shark_fm_info.seek_cnt);
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
        TROUT_PRINT("FM_REG_CHAN_FREQ_STS: %08x (%d)\r\n", \
		shark_fm_info.freq_seek, shark_fm_info.freq_seek*5 + 10);
#else		
	TROUT_PRINT("FM_REG_CHAN_FREQ_STS: %08x (%d)\r\n", \
		shark_fm_info.freq_seek, ((shark_fm_info.freq_seek >> 1) + 1));
#endif		
	TROUT_PRINT("FM_REG_FREQ_OFF_STS : %08x (%d)\r\n", \
		shark_fm_info.freq_offset, shark_fm_info.freq_offset);
	TROUT_PRINT("FM_REG_RF_RSSI_STS  : %08x\r\n", \
		shark_fm_info.rf_rssi);
	TROUT_PRINT("FM_REG_INPWR_STS    : %08x\r\n", \
		shark_fm_info.inpwr_sts);
	TROUT_PRINT("FM_REG_WBRSSI_STS   : %08x\r\n", \
		shark_fm_info.fm_sts);
	TROUT_PRINT("FM_REG_RSSI_STS     : %08x\r\n", \
		shark_fm_info.rssi);
	TROUT_PRINT("FM_REG_AGC_TBL_STS  : %08x\r\n", \
		shark_fm_info.agc_sts);

	TROUT_PRINT("fm work:%d, fm iq:%04x\r\n", \
		(shark_fm_info.fm_sts >> 20) & 1, \
		(shark_fm_info.fm_sts >> 16) & 0xf);
	TROUT_PRINT("pilot detect:%d, stero:%d, seek fail:%d, rssi:%08x\r\n", \
		(shark_fm_info.rssi >> 18) & 0x3, \
		(shark_fm_info.rssi >> 17) & 0x1, \
		(shark_fm_info.rssi >> 16) & 0x1, shark_fm_info.rssi & 0xff);
}

int trout_fm_deinit(void)
{
	/*free_irq(INT_NUM_FM_test, fm_interrupt);*/
    u32 reg_data;
	 
    fm_rf_ops->write_reg(0x400, 0x0000);
    fm_rf_ops->write_reg(0x6F, 0x201);
	
    READ_REG(SHARK_PMU_SLEEP_CTRL, &reg_data);
    reg_data &= (~BIT_8);
    WRITE_REG(SHARK_PMU_SLEEP_CTRL, reg_data);

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
	TROUT_PRINT("start_msecond: %u\n", start_msecond);
	do {
		msleep(20);

		READ_REG(FM_REG_INT_STS, &seek_succes);

		is_timeout = time_after(jiffies,
					jiffies_comp +
					msecs_to_jiffies(time_out));

		if (is_timeout && (seek_succes==0)) {
			TROUT_PRINT("FM search timeout.");
			//trout_fm_dis();

		        ret = Trout_SEEK_TIMEOUT;                       
		}

		if (seek_succes) {
			TROUT_PRINT("FM found frequency.");
			break;
		}
	} while (seek_succes == 0 && is_timeout == 0);

	end_msecond = jiffies_to_msecs(jiffies);
	TROUT_PRINT("end_msecond: %u\n", end_msecond);
	TROUT_PRINT("used time: %ums\n", end_msecond - start_msecond);

	__trout_fm_get_status();
	__trout_fm_show_status();

	TROUT_PRINT("\n");
	return ret;
}

int trout_fm_set_tune(u16 freq)
{
	u32 reg_data;
	trout_fm_dis();
	shark_fm_int_dis();
	shark_fm_int_clr();
	/*FM_ADC_Tuning_Value_overwrite(chan);*/
	WRITE_REG(FM_REG_FMCTL_STI, FM_CTL_STI_MODE_TUNE);	/* tune mode*/
#ifdef CONFIG_FM_SEEK_STEP_50KHZ

	WRITE_REG(FM_REG_CHAN, ((freq-10)/5) & 0xffff);

#else

	WRITE_REG(FM_REG_CHAN, ((freq-1)*2) & 0xffff);
#endif

	shark_fm_int_en();
	trout_fm_en();

	READ_REG(FM_REG_CHAN, &reg_data);
	TROUT_PRINT("chan reg_data is %d\n", reg_data);

	shark_fm_wait_int(3000);

	/*dump_fm_regs();*/

	shark_fm_int_clr();
	return 0;
}

int trout_fm_seek(u16 frequency, u8 seek_dir, u32 time_out, u16 *freq_found)
{
	int ret = 0;

	TROUT_PRINT("FM seek, freq(%i) dir(%i) timeout(%i).",
		    frequency, seek_dir, time_out);

	if (frequency < MIN_FM_FREQ || frequency > MAX_FM_FREQ) {
		TROUT_PRINT("out of freq range: %i", frequency);
		frequency = MIN_FM_FREQ;
	}
#ifdef CONFIG_FM_SEEK_STEP_50KHZ

    shark_fm_info.freq_seek = ((frequency-10)/5);

#else

	shark_fm_info.freq_seek = ((frequency-1)*2);
#endif

	seek_dir ^= 0x1;
	trout_fm_dis();
	shark_fm_int_clr();

	if (seek_dir == 0)
		shark_fm_seek_up();
	 else
		shark_fm_seek_down();

	WRITE_REG(FM_REG_FMCTL_STI, FM_CTL_STI_MODE_SEEK);	/* seek mode*/

	shark_fm_int_en();
	trout_fm_en();
	ret = shark_fm_wait_int(time_out);
	if (ret == Trout_SEEK_TIMEOUT)
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
            TROUT_PRINT("seek failed!");
            TROUT_PRINT("freq=%d",shark_fm_info.freq_seek*5 + 10);
#else	 
            if (seek_dir == 0){
                if ( MIN_FM_FREQ == ((shark_fm_info.freq_seek >> 1) + 1)){
                            shark_fm_info.freq_seek  =  (MAX_FM_FREQ -1 ) * 2;
		   }
		   else{
			    shark_fm_info.freq_seek -= 2;
		   }
            }
	     else{
                if ( MAX_FM_FREQ == ((shark_fm_info.freq_seek >> 1) + 1)){
                            shark_fm_info.freq_seek  =  (MIN_FM_FREQ -1 ) * 2;
		   }
		   else{
			    shark_fm_info.freq_seek += 2;
		   }
	     }
            TROUT_PRINT("seek time out!");
            TROUT_PRINT("freq=%d",((shark_fm_info.freq_seek >> 1) + 1)); 

#endif			
        }
		
	/*dump_fm_regs();*/
#ifdef CONFIG_FM_SEEK_STEP_50KHZ

	*freq_found = shark_fm_info.freq_seek*5 + 10;
#else	
	*freq_found = (shark_fm_info.freq_seek >> 1) + 1;
#endif

	shark_fm_int_clr();

	return ret;
}

int trout_fm_get_frequency(u16 *freq)
{
#ifdef CONFIG_FM_SEEK_STEP_50KHZ

        *freq = shark_fm_info.freq_seek*5 + 10;
#else
       *freq = ((shark_fm_info.freq_seek >> 1) + 1);
#endif

	return 0;
}

int trout_fm_stop_seek(void)
{
	u32 reg_value = 0x0;

	TROUT_PRINT("FM stop seek.");

	/* clear seek enable bit of seek register. */
	READ_REG(FM_REG_FMCTL_STI, &reg_value);
	if (reg_value & FM_CTL_STI_MODE_SEEK) {
		reg_value |= FM_CTL_STI_MODE_NORMAL;
		WRITE_REG(FM_REG_FMCTL_STI, reg_value);
	}

	return 0;
}

int trout_fm_rf_spi_write(u32 addr, u32 data)
{
	addr &= 0x0000ffff;
	WRITE_REG(0x1000 | addr, data);

	return 0;
}

int trout_fm_rf_spi_read(u32 addr, u32 *data)
{
	addr &= 0x0000ffff;
	READ_REG(0x1000 | addr, data);

	return 0;
}
