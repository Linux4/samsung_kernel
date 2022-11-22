/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 * Author: steve.zhan <steve.zhan@spreadtrum.com>
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#if !defined(CONFIG_ARCH_SCX30G) && !defined(CONFIG_ARCH_SCX35L)

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/delay.h>

#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/irqs.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/arch_lock.h>
#include <mach/efuse.h>

#define CONFIG_EFUSE_TEST

#if defined(CONFIG_ARCH_SCX15)
static int idx = 0;
#define EFUSE_CONTROLLER_NUM	2

#undef SPRD_EFUSE_BASE
#define SPRD_EFUSE_BASE			(SPRD_UIDEFUSE_BASE + idx * 0x100)
#endif


/* registers definitions for controller CTL_EFUSE */
#define REG_EFUSE_DATA_RD               (SPRD_EFUSE_BASE + 0x0000)
#define REG_EFUSE_DATA_WR               (SPRD_EFUSE_BASE + 0x0004)
#define REG_EFUSE_BLOCK_INDEX           (SPRD_EFUSE_BASE + 0x0008)
#define REG_EFUSE_MODE_CTRL             (SPRD_EFUSE_BASE + 0x000c)
#define REG_EFUSE_PGM_PARA              (SPRD_EFUSE_BASE + 0x0010)
#define REG_EFUSE_STATUS                (SPRD_EFUSE_BASE + 0x0014)
#define REG_EFUSE_BLK_FLAGS             (SPRD_EFUSE_BASE + 0x0018)
#define REG_EFUSE_BLK_CLR               (SPRD_EFUSE_BASE + 0x001c)
#define REG_EFUSE_MAGIC_NUMBER          (SPRD_EFUSE_BASE + 0x0020)
#ifdef CONFIG_SC_X15
#define REG_EFUSE_STROBE_LOW_WIDTH		(SPRD_EFUSE_BASE + 0x0024)
#endif

#define ANA_PROT_KEY		(0xc686)

/* bits definitions for register REG_EFUSE_BLOCK_INDEX */
#define BITS_READ_INDEX(_X_)		( (_X_) << 0 &  (D_EFUSE_MAX_BLK_ID << 00) )
#define BITS_PGM_INDEX(_X_)		( (_X_) << 16 & (D_EFUSE_MAX_BLK_ID << 16) )
#define BITS_BIST_INDEX(_X_)		( (_X_) << 26 & (D_EFUSE_MAX_BLK_ID << 26) )
#define BITS_BIST_SIZE(_X_)		( (_X_) << 29 & (D_EFUSE_MAX_BLK_ID << 29) )

#define SHIFT_READ_INDEX                ( 0 )
#define MASK_READ_INDEX                 ( BIT(0)|BIT(1)|BIT(2) )

#define SHIFT_PGM_INDEX                 ( 16 )
#define MASK_PGM_INDEX                  ( BIT(16)|BIT(17)|BIT(18) )

/* bits definitions for register REG_EFUSE_MODE_CTRL */
#define BIT_PG_START                    ( BIT(0) )
#define BIT_RD_START                    ( BIT(1) )
#define BIT_STANDBY_START               ( BIT(2) )
#define BIT_BIST_START		        ( BIT(31) )

/* bits definitions for register REG_EFUSE_PGM_PARA */
#define BITS_TPGM_TIME_CNT(_x_)         ( (_x_) << 0 & (0x1FF) )
#define BIT_BIST_SW_EN		        ( BIT(27) )
#define BIT_CLK_EFS_EN                  ( BIT(28) )
#define BIT_EFUSE_VDD_ON                ( BIT(29) )
#define BIT_PCLK_DIV_EN                 ( BIT(30) )
#define BIT_PGM_EN                      ( BIT(31) )

/* bits definitions for register REG_EFUSE_STATUS */
#define BIT_PGM_BUSY                    ( BIT(0) )
#define BIT_READ_BUSY                   ( BIT(1) )
#define BIT_STANDBY_BUSY                ( BIT(2) )
#define BIT_BIST_FAIL                   ( BIT(4) )
#define BIT_BIST_BUSY                   ( BIT(5) )

/* definitions for register REG_EFUSE_MAGIC_NUMBER */
#define MAGIC_NUMBER		(0x8810)

#define D_EFUSE_MIN_BLK_ID			(0)
#define D_EFUSE_MAX_BLK_ID  			(7)

#define D_EFUSE_VERIFY_BLK_ID(_X_)		do {    \
    BUG_ON((_X_) > D_EFUSE_MAX_BLK_ID);   \
}while(0)

//adie laser fuse control
#define AFUSE_DLY_PROT_KEY		(0xa2)

#define CAL_DATA_BLK            7
#define BASE_ADC_P0		711  //3.6V
#define BASE_ADC_P1		830  //4.2V
#define VOL_P0			3600
#define VOL_P1			4200
#define ADC_DATA_OFFSET		128

static DEFINE_MUTEX(ddie_fuse_lock);
static DEFINE_MUTEX(adie_fuse_lock);

#ifdef CONFIG_EFUSE_TEST
#define efuse_dbg    printk
static int efuse_auto_test_en = 0;
#endif

#define efuse_reg_read(r)               (__raw_readl((const volatile void*)(r)))
#define efuse_reg_write(v, r)           (__raw_writel((v), (volatile void*)(r)))

static void efuse_dump_register(u32 en)
{
#if defined(CONFIG_ARCH_SCX35)
	efuse_dbg("----------------- efuse dump start --------------------------------\n");

	efuse_dbg("Efuse base viral addr = 0x%08x\n", SPRD_EFUSE_BASE);
	efuse_dbg("REG_EFUSE_DATA_RD = 0x%08x\n", efuse_reg_read(REG_EFUSE_DATA_RD));
	efuse_dbg("REG_EFUSE_DATA_WR = 0x%08x\n", efuse_reg_read(REG_EFUSE_DATA_WR));
	efuse_dbg("REG_EFUSE_BLOCK_INDEX = 0x%08x\n", efuse_reg_read(REG_EFUSE_BLOCK_INDEX));
	efuse_dbg("REG_EFUSE_MODE_CTRL = 0x%08x\n", efuse_reg_read(REG_EFUSE_MODE_CTRL));
	efuse_dbg("REG_EFUSE_PGM_PARA = 0x%08x\n", efuse_reg_read(REG_EFUSE_PGM_PARA));
	efuse_dbg("REG_EFUSE_STATUS = 0x%08x\n", efuse_reg_read(REG_EFUSE_STATUS));
	efuse_dbg("REG_EFUSE_BLK_FLAGS = 0x%08x\n", efuse_reg_read(REG_EFUSE_BLK_FLAGS));
	efuse_dbg("REG_EFUSE_BLK_CLR = 0x%08x\n", efuse_reg_read(REG_EFUSE_BLK_CLR));
	efuse_dbg("REG_EFUSE_MAGIC_NUMBER = 0x%08x\n", efuse_reg_read(REG_EFUSE_MAGIC_NUMBER));

	efuse_dbg("REG_AON_APB_APB_EB0 = 0x%08x\n", sci_glb_raw_read(REG_AON_APB_APB_EB0));
	efuse_dbg("REG_AON_APB_PWR_CTRL = 0x%08x\n", sci_glb_raw_read(REG_AON_APB_PWR_CTRL));

	efuse_dbg("----------------- efuse dump end --------------------------------\n");
#endif
}

static __inline void __ddie_fuse_wait_status_clean(u32 bits)
{
	unsigned long timeout;

	/* wait for maximum of 300 msec */
	timeout = jiffies + msecs_to_jiffies(300);
	while (efuse_reg_read(REG_EFUSE_STATUS) & bits) {
		if (time_after(jiffies, timeout)) {
			WARN_ON(1);
			break;
		}
		cpu_relax();
	}
}

static __inline void __ddie_fuse_global_init(void)
{
#if defined(CONFIG_ARCH_SC8825)
	sci_glb_set(REG_GLB_GEN0, BIT_EFUSE_EB);
#elif defined(CONFIG_ARCH_SCX35)
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_EFUSE_EB);

	sci_glb_set(REG_AON_APB_APB_RST0, BIT_EFUSE_SOFT_RST);
	udelay(5);
	sci_glb_clr(REG_AON_APB_APB_RST0, BIT_EFUSE_SOFT_RST);

	sci_glb_set(REG_AON_APB_PWR_CTRL, BIT_EFUSE0_PWR_ON);
	sci_glb_set(REG_AON_APB_PWR_CTRL, BIT_EFUSE1_PWR_ON);
#endif
	efuse_reg_write(efuse_reg_read(REG_EFUSE_PGM_PARA) | BIT_EFUSE_VDD_ON | BIT_CLK_EFS_EN,
		     REG_EFUSE_PGM_PARA);
}

static __inline void __ddie_fuse_global_close(void)
{
	efuse_reg_write(efuse_reg_read(REG_EFUSE_PGM_PARA) & ~(BIT_EFUSE_VDD_ON | BIT_CLK_EFS_EN),
		     REG_EFUSE_PGM_PARA);

#if defined(CONFIG_ARCH_SC8825)
	sci_glb_clr(REG_GLB_GEN0, BIT_EFUSE_EB);
#elif defined(CONFIG_ARCH_SCX35)
	sci_glb_clr(REG_AON_APB_APB_EB0,BIT_EFUSE_EB);
	sci_glb_clr(REG_AON_APB_PWR_CTRL, BIT_EFUSE0_PWR_ON);
	sci_glb_clr(REG_AON_APB_PWR_CTRL, BIT_EFUSE1_PWR_ON);
#endif
}

static __inline int __ddie_fuse_read(u32 blk)
{
	int val = 0;
	unsigned long flags;

#if defined(CONFIG_ARCH_SCX15)
	idx = blk / 8;
	blk %= 8;

	BUG_ON(idx >= EFUSE_CONTROLLER_NUM);
#endif

	D_EFUSE_VERIFY_BLK_ID(blk);

	__arch_default_lock(HWLOCK_EFUSE, &flags);

	mutex_lock(&ddie_fuse_lock);
	__ddie_fuse_global_init();
	val = BITS_READ_INDEX(blk) | BITS_PGM_INDEX(blk);
	efuse_reg_write(val, REG_EFUSE_BLOCK_INDEX);
#ifdef CONFIG_DEBUG_FS
	efuse_dump_register(1);
#endif
	efuse_reg_write(efuse_reg_read(REG_EFUSE_MODE_CTRL) | BIT_RD_START,
		     REG_EFUSE_MODE_CTRL);
	__ddie_fuse_wait_status_clean(BIT_READ_BUSY);
	val = efuse_reg_read(REG_EFUSE_DATA_RD);
	__ddie_fuse_global_close();
	mutex_unlock(&ddie_fuse_lock);

	__arch_default_unlock(HWLOCK_EFUSE, &flags);

#if defined(CONFIG_ARCH_SCX15)
	pr_info("%s()->Line:%d; efuse idx=%d, blk=%d, data=0x%08x\n", __func__, __LINE__, idx, blk, val);
#else
	pr_info("%s()->Line:%d; efuse blk=%d, data=0x%08x\n", __func__, __LINE__, blk, val);
#endif

	return val;
}

//Need confirm.
static __inline int __adie_fuse_getdata(void)
{
#if defined(CONFIG_ARCH_SCX15)
	return 0;
#else
	int val = 0;
	unsigned long timeout;
	unsigned long flags;

	__arch_default_lock(HWLOCK_EFUSE, &flags);

	mutex_lock(&adie_fuse_lock);
#if defined(CONFIG_ARCH_SC8825)
	/* wait for maximum of 100 msec */
	sci_adi_write_fast(ANA_REG_GLB_AFUSE_CTRL, BIT_AFUSE_RD_REQ, 1);
	udelay(1);
	timeout = jiffies + msecs_to_jiffies(300);
	while(BIT_AFUSE_RD_REQ & sci_adi_read(ANA_REG_GLB_AFUSE_CTRL)) {
		if(time_after(jiffies, timeout))
		{
			WARN_ON(1);
			break;
		}
		cpu_relax();
	}
	val = sci_adi_read(ANA_REG_GLB_AFUSE_OUT_LOW);
	val |= (sci_adi_read(ANA_REG_GLB_AFUSE_OUT_HIGH)) << 16;
#elif defined(CONFIG_ARCH_SCX35)
	/* wait for maximum of 100 msec */
	sci_adi_write_fast(ANA_REG_GLB_AFUSE_CTRL, BIT_AFUSE_READ_REQ, 1);
	udelay(1);
	timeout = jiffies + msecs_to_jiffies(300);
	while(BIT_AFUSE_READ_REQ & sci_adi_read(ANA_REG_GLB_AFUSE_CTRL)) {
		if(time_after(jiffies, timeout)){
			WARN_ON(1);
			break;
		}
		cpu_relax();
	}
	val = sci_adi_read(ANA_REG_GLB_AFUSE_OUT0);
	val |= (sci_adi_read(ANA_REG_GLB_AFUSE_OUT1)) << 16;
#endif

	mutex_unlock(&adie_fuse_lock);

	__arch_default_unlock(HWLOCK_EFUSE, &flags);

	return val;
#endif /* CONFIG_ARCH_SCX15 */
}

int sci_efuse_get(u32 blk)
{
	pr_debug("sci_efuse_get, blk = %d\n", blk);

#if defined(CONFIG_ARCH_SCX15)
	return __ddie_fuse_read(blk);
#else
    if(CAL_DATA_BLK != blk)
		return 0;

	return __adie_fuse_getdata();
#endif
}
EXPORT_SYMBOL(sci_efuse_get);


int sci_efuse_calibration_get(unsigned int *p_cal_data)
{
	int data;
	unsigned short adc_temp;

	data = sci_efuse_get(CAL_DATA_BLK);

	data &= ~(1 << 31);

	printk("sci_efuse_calibration data:0x%x\n", data);
	if ((!data) || (p_cal_data == NULL)) {
		return 0;
	}
	//adc 3.6V
	adc_temp = ((data >> 8) & 0x00FF) + BASE_ADC_P0 - ADC_DATA_OFFSET;
	p_cal_data[1] = (VOL_P0) | ((adc_temp << 2) << 16);

	//adc 4.2V
	adc_temp = (data & 0x00FF) + BASE_ADC_P1 - ADC_DATA_OFFSET;
	p_cal_data[0] = (VOL_P1) | ((adc_temp << 2) << 16);

	return 1;
}
EXPORT_SYMBOL(sci_efuse_calibration_get);

#define CCCV_CAL_DATA_BLK		9
int sci_efuse_cccv_cal_get(unsigned int *p_cal_data)
{
	int data;

	data = sci_efuse_get(CCCV_CAL_DATA_BLK);

	data &= ~(1 << 31);

	printk("sci_efuse_cccv_cal_get data:0x%x\n", data);
	if ((!data) || (p_cal_data == NULL)) {
		return 0;
	}

      *p_cal_data = (data >> 24) & 0x003F;

	return 1;
}

#define FGU_CAL_DATA_BLK0		8
#define FGU_CAL_DATA_BLK1		9
int sci_efuse_fgu_cal_get(unsigned int *p_cal_data)
{
	int data;

	data = sci_efuse_get(FGU_CAL_DATA_BLK0);

	data &= ~(1 << 31);

	printk("sci_efuse_fgu_cal_get 1 data:0x%x\n", data);
	if ((!data) || (p_cal_data == NULL)) {
		return 0;
	}
      p_cal_data[0] = (((data >> 16) & 0x1FF) + 6808) - 4096 -256;       //4.0V or 4.2V
      p_cal_data[1] = p_cal_data[0] -390 - ((data >> 25) & 0x1FF) +16 ;   //3.6V
      p_cal_data[2] = (((data) & 0x1FF) + 8192) -256 ;
      data = sci_efuse_get(FGU_CAL_DATA_BLK1);
      printk("sci_efuse_fgu_cal_get 2 data:0x%x\n", data);
      p_cal_data[3] = (((data) & 0x1FF) + 8192) - 256;
      return 1;
}


/*
 * sci_efuse_get_cal - read adc data saved in efuse
 * @pdata: adc data pointer
 * pdata[0] -> 4.2v
 * pdata[1] -> 3.6v
 * pdata[2] -> 0.4v
 * @num: the length of adc data
 *
 * retruns 0 if success, else
 * returns negative number.
 */
#define DELTA2ADC(_delta_, _Ideal_) 		( ((_delta_) + (_Ideal_) - 128) << 2 )
#define ADC_VOL(_delta_, _Ideal_, vol)	((DELTA2ADC(_delta_, _Ideal_) << 16) | (vol))

int sci_efuse_get_cal(unsigned int * pdata, int num)
{
	int i;
	u32 efuse_data;
	u8* delta = (u8*) &efuse_data;
	const u16 ideal[3][2] = {
		{4200, 830},	/* FIXME: efuse only support 10bits */
		{3600, 711},
		{400,   79},
	};

	efuse_data = (u32) sci_efuse_get(CAL_DATA_BLK);

	pr_info("%s efuse data: 0x%08x\n", __func__, efuse_data);

	if (!(efuse_data & BIT(31)) || (!pdata)) {
		return -1;
	}

	WARN_ON(!((delta[0] > delta[1]) && (delta[1] > delta[2])));

	pr_info("adc_cal: ");
	for(i = 0; i < num; i++) {
		pdata[i] = (unsigned int) ADC_VOL(delta[i], ideal[i][1], ideal[i][0]);
		pr_info("0x%08x ", pdata[i]);
	}
	pr_info("\n");

	return 0;
}
EXPORT_SYMBOL_GPL(sci_efuse_get_cal);

/*
 * sci_efuse_get_apt_cal - read ap data saved in efuse
 * @pdata: apt data pointer for dcdcwpa (unit: uV)
 * pdata[0] -> 3v
 * pdata[1] -> 0.8v
 * @num: the length of apt data
 *
 * retruns 0 if success, else
 * returns negative number.
 */
#define APT_CAL_DATA_BLK		( 10 )
#define OFFSET2VOL(p, uv)		( ((p) + 128) * (uv) / 256)

int sci_efuse_get_apt_cal(unsigned int * pdata, int num)
{
	int i = 0;
	u32 efuse_data = 0;
	u8* offset = (u8*) &efuse_data;
	const unsigned int vol_ref[2] = {
		3000000, //3v
		800000, //0.8v
	};

	/* Fixme: dcdcwpa only support two points(3.6v / 0.8v) voltage calibration */
	BUG_ON(num > 2);

	efuse_data = (u32) sci_efuse_get(APT_CAL_DATA_BLK);

	//pr_info("%s efuse data: 0x%08x\n", __func__, efuse_data);

	if (!(efuse_data & BIT(31)) || (!pdata)) {
		return -1;
	}

	printk("%s: ", __func__);
	for(i = 0; i < num; i++) {
		pdata[i] = (unsigned int) OFFSET2VOL(offset[i], vol_ref[i]);
		printk("(%uuV : %u), ", vol_ref[i], pdata[i]);
	}
	printk("\n");

	return 0;
}
EXPORT_SYMBOL_GPL(sci_efuse_get_apt_cal);

/*
 * below code is for test ,maybe used in the future.
 */
static __inline void __ddie_fuse_test_clear_blkerrflag(u32 blk)
{
	int offset = 0x8;
	if (efuse_reg_read(REG_EFUSE_BLK_FLAGS) & (1 << (blk + offset))) {
		pr_debug("efuse_auto_test_en, find block = %d, error\n", blk);
		efuse_reg_write(1 << (blk + offset), REG_EFUSE_BLK_CLR);
	}
}

static __inline void __ddie_fuse_test_clear_blkprotflag(u32 blk)
{
	int offset = 0x0;
	if (efuse_reg_read(REG_EFUSE_BLK_FLAGS) & (1 << (blk + offset)))
		efuse_reg_write(1 << (blk + offset), REG_EFUSE_BLK_CLR);
}

static __inline void __ddie_fuse_program_getkey(void)
{
	u32 key = MAGIC_NUMBER;
	int val = efuse_reg_read(REG_EFUSE_PGM_PARA);
	val |= BIT_PGM_EN;
	efuse_reg_write(val, REG_EFUSE_PGM_PARA);
	efuse_reg_write(key, REG_EFUSE_MAGIC_NUMBER);
}

static __inline void __ddie_fuse_program_putkey(void)
{
	u32 key = ~MAGIC_NUMBER;
	int val = efuse_reg_read(REG_EFUSE_PGM_PARA);
	val &= ~BIT_PGM_EN;
	efuse_reg_write(val, REG_EFUSE_PGM_PARA);
	efuse_reg_write(key, REG_EFUSE_MAGIC_NUMBER);
}

static void sci_ddie_fuse_program_vdd(u32 enable, u32 msleep_value)
{
	u32 val = 0;
	val = efuse_reg_read(REG_EFUSE_PGM_PARA);
#if defined(CONFIG_ARCH_SC8825)
	if (enable) {
		val |= (BIT_EFUSE_VDD_ON | BIT_CLK_EFS_EN);
		efuse_reg_write(val, REG_EFUSE_PGM_PARA);
		sci_adi_write_fast(ANA_REG_GLB_EFS_PROT, ANA_PROT_KEY, 1);
		sci_adi_write_fast(ANA_REG_GLB_EFS_CTRL, BIT_EFS_2P5V_PWR_ON,
				   1);
		if (msleep_value > 0)
			msleep(msleep_value);
	} else {
		val &= ~(BIT_EFUSE_VDD_ON | BIT_CLK_EFS_EN);
		efuse_reg_write(val, REG_EFUSE_PGM_PARA);
		sci_adi_write_fast(ANA_REG_GLB_EFS_CTRL,
				   (u16) ~ BIT_EFS_2P5V_PWR_ON, 1);
		sci_adi_write_fast(ANA_REG_GLB_EFS_PROT, (u16) ~ ANA_PROT_KEY,
				   1);
	}
#elif defined(CONFIG_ARCH_SCX35)
	if(enable)
	{
	} else {
	}
#endif
}

void sci_ddie_fuse_program(u32 blk, int data)
{
	int val = 0;
	unsigned long flags;

#if defined(CONFIG_ARCH_SCX15)
	idx = blk / 8;
	blk %= 8;

	BUG_ON(idx >= EFUSE_CONTROLLER_NUM);
#endif

	D_EFUSE_VERIFY_BLK_ID(blk);

	__arch_default_lock(HWLOCK_EFUSE, &flags);

	mutex_lock(&ddie_fuse_lock);
	__ddie_fuse_global_init();
	__ddie_fuse_program_getkey();
	sci_ddie_fuse_program_vdd(1, 1);

	if (efuse_auto_test_en) {
		val = efuse_reg_read(REG_EFUSE_PGM_PARA);
		val |= (1 << (blk + 16));
		efuse_reg_write(val, REG_EFUSE_PGM_PARA);
	}
	val = BITS_PGM_INDEX(blk);
	efuse_reg_write(val, REG_EFUSE_BLOCK_INDEX);
	efuse_reg_write(data, REG_EFUSE_DATA_WR);
#ifdef CONFIG_DEBUG_FS
	efuse_dump_register(1);
#endif
	efuse_reg_write(efuse_reg_read(REG_EFUSE_MODE_CTRL) | BIT_PG_START,
		     REG_EFUSE_MODE_CTRL);
	__ddie_fuse_wait_status_clean(BIT_PGM_BUSY);

	if (efuse_auto_test_en)
		__ddie_fuse_test_clear_blkerrflag(blk);

	sci_ddie_fuse_program_vdd(0, 0);
	__ddie_fuse_program_putkey();
	__ddie_fuse_global_close();

	mutex_unlock(&ddie_fuse_lock);

	__arch_default_unlock(HWLOCK_EFUSE, &flags);

}

void sci_ddie_fuse_set_cyclecnt(u32 cnt, u32 efuse_clk_div_en)
{
	int val = 0;

	if ((int)cnt > 0)
		cnt--;

	mutex_lock(&ddie_fuse_lock);
	__ddie_fuse_global_init();
	__ddie_fuse_program_getkey();
	val = BITS_TPGM_TIME_CNT(cnt);
	if (efuse_clk_div_en)
		val |= BIT_PCLK_DIV_EN;
	efuse_reg_write(val, REG_EFUSE_PGM_PARA);
	__ddie_fuse_program_putkey();

	__ddie_fuse_global_close();
	mutex_unlock(&ddie_fuse_lock);

}

void sci_ddie_fuse_bist(u32 start_blk, u32 size)
{
	int val = 0;
#if defined(CONFIG_ARCH_SCX15)
	idx = start_blk / 8;
	start_blk %= 8;

	BUG_ON(idx >= EFUSE_CONTROLLER_NUM);
#endif
	D_EFUSE_VERIFY_BLK_ID(start_blk);

	/*bist for ddie_fuse don't need key */
	mutex_lock(&ddie_fuse_lock);
	__ddie_fuse_global_init();
	sci_ddie_fuse_program_vdd(1, 1);

	val = BITS_BIST_INDEX(start_blk) | BITS_BIST_SIZE(size);
	efuse_reg_write(val, REG_EFUSE_BLOCK_INDEX);
	val = efuse_reg_read(REG_EFUSE_PGM_PARA);
	val |= BIT_BIST_SW_EN;
	efuse_reg_write(val, REG_EFUSE_PGM_PARA);

	efuse_reg_write(efuse_reg_read(REG_EFUSE_MODE_CTRL) | BIT_BIST_START,
		     REG_EFUSE_MODE_CTRL);
	__ddie_fuse_wait_status_clean(BIT_BIST_BUSY);
	if (efuse_reg_read(REG_EFUSE_STATUS) & BIT_BIST_FAIL)
		pr_debug("sci_efuse_bist error\n");

	sci_ddie_fuse_program_vdd(0, 0);
	__ddie_fuse_global_close();
	mutex_unlock(&ddie_fuse_lock);
}

void sci_adie_fuse_set_readdly(u32 read_delay)
{
#if defined(CONFIG_ARCH_SCX15)
	return;
#else
	u32 v = 0;
	mutex_lock(&adie_fuse_lock);
#if defined(CONFIG_ARCH_SC8825)
	v = BITS_AFUSE_RD_DLY_PROT(AFUSE_DLY_PROT_KEY);	/*get lock */
	v |= BITS_AFUSE_RD_DLY(read_delay);
	sci_adi_write_fast(ANA_REG_GLB_AFUSE_CTRL, v, 0);
	v = ~(BITS_AFUSE_RD_DLY_PROT(AFUSE_DLY_PROT_KEY));	/*release lock */
	sci_adi_write_fast(ANA_REG_GLB_AFUSE_CTRL, v, 1);
#elif defined(CONFIG_ARCH_SCX35)
	//v = BITS_AFUSE_READ_DLY_PROT(AFUSE_DLY_PROT_KEY);	/*get lock */
	v |= BITS_AFUSE_READ_DLY(read_delay);
	sci_adi_write_fast(ANA_REG_GLB_AFUSE_CTRL, v, 0);
	//v = ~(BITS_AFUSE_READ_DLY_PROT(AFUSE_DLY_PROT_KEY));	/*release lock */
	sci_adi_write_fast(ANA_REG_GLB_AFUSE_CTRL, v, 1);

#endif
	mutex_unlock(&adie_fuse_lock);
#endif /* CONFIG_ARCH_SCX15 */
}

#ifdef CONFIG_EFUSE_TEST
EXPORT_SYMBOL(sci_ddie_fuse_program);
EXPORT_SYMBOL(sci_ddie_fuse_set_cyclecnt);
EXPORT_SYMBOL(sci_ddie_fuse_bist);
EXPORT_SYMBOL(sci_adie_fuse_set_readdly);
#endif

#ifdef CONFIG_DEBUG_FS
struct sci_fuse {
	const char *name;
	int blk_id;
	int is_adie_fuse;
};
static struct sci_fuse sci_fuse_array[] = {
	{"adie-fuse0", 0, 1},
	{"ddie-fuse0", 0, 0},
	{"ddie-fuse1", 1, 0},
	{"ddie-fuse2", 2, 0},
	{"ddie-fuse3", 3, 0},
	{"ddie-fuse4", 4, 0},
	{"ddie-fuse5", 5, 0},
	{"ddie-fuse6", 6, 0},
	{"ddie-fuse7", 7, 0},
#if defined(CONFIG_ARCH_SCX15)
	{"ddie-fuse8", 8, 0},
	{"ddie-fuse9", 9, 0},
	{"ddie-fuse10", 10, 0},
	{"ddie-fuse11", 11, 0},
	{"ddie-fuse12", 12, 0},
	{"ddie-fuse13", 13, 0},
	{"ddie-fuse14", 14, 0},
	{"ddie-fuse15", 15, 0},
#endif
};

#define IIS_TO_AP	(0)
#define IIS_TO_CP0	(1)
#define IIS_TO_CP1	(2)
#define IIS_TO_CP2	(3)

static int fuse_debug_set(void *data, u64 val)
{
	struct sci_fuse *p = data;
	if (!(p->is_adie_fuse))
		sci_ddie_fuse_program(p->blk_id, (int)val);

	return 0;
}

static int fuse_debug_get(void *data, u64 * val)
{
	struct sci_fuse *p = data;

	if (p->is_adie_fuse)
		*val = __adie_fuse_getdata();
	else
		*val = __ddie_fuse_read(p->blk_id);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(efuse_enable_fops, fuse_debug_get,
			fuse_debug_set, "%llu\n");

static struct dentry *debugfs_base;
static int __init fuse_debug_add(struct sci_fuse *fuse)
{
	if (!debugfs_create_file(fuse->name, S_IRUGO | S_IWUSR, debugfs_base,
				 fuse, &efuse_enable_fops))
		return -ENOMEM;

	return 0;
}

int __init fuse_debug_init(void)
{
	int i;
	debugfs_base = debugfs_create_dir("efuse", NULL);
	if (!debugfs_base)
		return -ENOMEM;
	for (i = 0; i < ARRAY_SIZE(sci_fuse_array); ++i) {
		fuse_debug_add(&sci_fuse_array[i]);
	}
	return 0;
}

late_initcall(fuse_debug_init);

#endif
#endif
