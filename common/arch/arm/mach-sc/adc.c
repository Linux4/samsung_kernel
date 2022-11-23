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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/irqflags.h>
#include <linux/delay.h>
#include <linux/hwspinlock.h>

#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/arch_lock.h>

static u32 io_base;		/* Mapped base address */

#define adc_write(val,reg) \
	do { \
		sci_adi_raw_write((u32)reg,val); \
} while (0)
static unsigned adc_read(unsigned addr)
{
	return sci_adi_read(addr);
}

/*FIXME:If we have not hwspinlock , we need use spinlock to do it*/
#define sci_adc_lock() 		do { \
		WARN_ON(IS_ERR_VALUE(hwspin_lock_timeout_irqsave(arch_get_hwlock(HWLOCK_ADC), -1, &flags)));} while(0)
#define sci_adc_unlock() 	do {hwspin_unlock_irqrestore(arch_get_hwlock(HWLOCK_ADC), &flags);} while(0)

#define ADC_CTL		(0x00)
#define ADC_SW_CH_CFG		(0x04)
#define ADC_FAST_HW_CHX_CFG(_X_)		((_X_) * 0x4 + 0x8)
#define ADC_SLOW_HW_CHX_CFG(_X_)		((_X_) * 0x4 + 0x28)
#define ADC_HW_CH_DELAY		(0x48)

#define ADC_DAT		(0x4c)
#define adc_get_data(_SAMPLE_BITS_)		(adc_read(io_base + ADC_DAT) & (_SAMPLE_BITS_))

#define ADC_IRQ_EN		(0x50)
#define adc_enable_irq(_X_)	do {adc_write(((_X_) & 0x1),io_base + ADC_IRQ_EN);} while(0)

#define ADC_IRQ_CLR		(0x54)
#define adc_clear_irq()		do {adc_write(0x1, io_base + ADC_IRQ_CLR);} while (0)

#define ADC_IRQ_STS		(0x58)
#define adc_mask_irqstatus()     adc_read(io_base + ADC_IRQ_STS)

#define ADC_IRQ_RAW		(0x5c)
#define adc_raw_irqstatus()     adc_read(io_base + ADC_IRQ_RAW)

#define ADC_DEBUG		(0x60)

/*ADC_CTL */
#define ADC_MAX_SAMPLE_NUM			(0x10)
#define BIT_SW_CH_RUN_NUM(_X_)		((((_X_) - 1) & 0xf ) << 4)
#define BIT_ADC_BIT_MODE(_X_)		(((_X_) & 0x1) << 2)	/*0: adc in 10bits mode, 1: adc in 12bits mode */
#define BIT_ADC_BIT_MODE_MASK		BIT_ADC_BIT_MODE(1)
#define BIT_SW_CH_ON                    ( BIT(1) ) /*WO*/
#define BIT_ADC_EN                      ( BIT(0) )
/*ADC_SW_CH_CFG && ADC_FAST(SLOW)_HW_CHX_CFG*/
#define BIT_CH_IN_MODE(_X_)		(((_X_) & 0x1) << 8)	/*0: resistance path, 1: capacitance path */
#define BIT_CH_SLOW(_X_)		(((_X_) & 0x1) << 6)	/*0: quick mode, 1: slow mode */
#define BIT_CH_SCALE(_X_)		(((_X_) & 0x1) << 5)	/*0: little scale, 1: big scale */
#define BIT_CH_ID(_X_)			((_X_) & 0x1f)
/*ADC_FAST(SLOW)_HW_CHX_CFG*/
#define BIT_CH_DLY_EN(_X_)		(((_X_) & 0x1) << 7)	/*0:disable, 1:enable */
/*ADC_HW_CH_DELAY*/
#define BIT_HW_CH_DELAY(_X_)		((_X_) & 0xff)	/*its unit is ADC clock */
#if defined(CONFIG_ARCH_SC8825)
#define BIT_ADC_EB                  ( BIT(5) )
#endif

static void sci_adc_enable(void)
{
#if defined(CONFIG_ARCH_SC8825)
	/* enable adc */
	sci_adi_set(ANA_REG_GLB_ANA_APB_CLK_EN,
		    BIT_ANA_ADC_EB | BIT_ANA_CLK_AUXADC_EN |
		    BIT_ANA_CLK_AUXAD_EN);
#elif defined(CONFIG_ARCH_SCX35)
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN,
		    BIT_ANA_ADC_EN);
	sci_adi_set(ANA_REG_GLB_ARM_CLK_EN,
		    BIT_CLK_AUXADC_EN | BIT_CLK_AUXAD_EN);
	sci_adi_set(ANA_REG_GLB_XTL_WAIT_CTRL,
		    BIT_XTL_EN);
	sci_glb_set(REG_AON_APB_SINDRV_CTRL, BIT_SINDRV_ENA);    //maybe need to modify it in later
#endif
}

void sci_adc_dump_register()
{
	unsigned _base = (unsigned)(io_base);
	unsigned _end = _base + 0x64;

	printk("sci_adc_dump_register begin\n");
	for (; _base < _end; _base += 4) {
		printk("_base = 0x%x, value = 0x%x\n", _base, adc_read(_base));
	}
	printk("sci_adc_dump_register end\n");
}

EXPORT_SYMBOL(sci_adc_dump_register);

void sci_adc_init(void __iomem * adc_base)
{
	io_base = (u32) adc_base;
	adc_enable_irq(0);
	adc_clear_irq();
	sci_adc_enable();
}

EXPORT_SYMBOL(sci_adc_init);

/*
*	Notes: for hw channel,  config its hardware configuration register and using sw channel to read it.
*/
static int sci_adc_config(struct adc_sample_data *adc)
{
	unsigned addr = 0;
	unsigned val = 0;
	int ret = 0;

	BUG_ON(!adc);
	BUG_ON(adc->channel_id > ADC_MAX);

	val = BIT_CH_IN_MODE(adc->signal_mode);
	val |= BIT_CH_SLOW(adc->sample_speed);
	val |= BIT_CH_SCALE(adc->scale);
	val |= BIT_CH_ID(adc->channel_id);
	val |= BIT_CH_DLY_EN(adc->hw_channel_delay ? 1 : 0);

	adc_write(val, io_base + ADC_SW_CH_CFG);

	if (adc->channel_type > 0) {	/*hardware */
		adc_write(BIT_HW_CH_DELAY(adc->hw_channel_delay),
			  io_base + ADC_HW_CH_DELAY);

		if (adc->channel_type == 1) {	/*slow */
			addr = io_base + ADC_SLOW_HW_CHX_CFG(adc->channel_id);
		} else {
			addr = io_base + ADC_FAST_HW_CHX_CFG(adc->channel_id);
		}
		adc_write(val, addr);
	}

	return ret;
}

#if defined(CONFIG_ARCH_SCX15)
#define RATIO(_n_, _d_) (_n_ << 16 | _d_)
static int sci_adc_ratio(int channel, int scale, int mux)
{
	switch (channel) {
	case ADC_CHANNEL_0:
	case ADC_CHANNEL_1:
	case ADC_CHANNEL_2:
	case ADC_CHANNEL_3:
		return (scale ? RATIO(400, 1025) : RATIO(1, 1));
	case ADC_CHANNEL_VBAT:		//vbat
	case ADC_CHANNEL_ISENSE:
		return RATIO(7, 29);
	case ADC_CHANNEL_DCDCCORE:		//dcdccore
	case ADC_CHANNEL_DCDCARM:		//dcdcarm
		return (scale ? RATIO(4, 5) : RATIO(1, 1));
	case ADC_CHANNEL_DCDCMEM:		//dcdcmem
		return (scale ? RATIO(3, 5) : RATIO(4, 5));
	case ADC_CHANNEL_DCDCLDO:	//dcdcgen
		return RATIO(4, 9);
	case 0x14 /* ADC_CHANNEL_HEADMIC */:	//DCDCHEADMIC
		return (scale ? RATIO(1, 3) : RATIO(1, 1));
	case ADC_CHANNEL_LDO0:		//DCDC Supply LDO, VDD18/CAMIO/CAMD/EMMCIO
		return RATIO(1, 2);
	case ADC_CHANNEL_VBATBK:	//DCDCVBATBK
	case ADC_CHANNEL_LDO1:		//VBATD Domain LDO, VDD25/SD/USB/SIM0/SIM1/SIM2
	case ADC_CHANNEL_LDO2:		//VBATA Domain LDO,  VDD28/CAMA/CAMMOT/EMMCCORE/CON/DCXO/RF0
	case ADC_CHANNEL_USBDP:		//DP from terminal
	case ADC_CHANNEL_USBDM:		//DM from terminal
		return RATIO(1, 3);

	default:
		return RATIO(1, 1);
	}
	return RATIO(1, 1);
}

void sci_adc_get_vol_ratio(unsigned int channel_id, int scale, unsigned int *div_numerators,
			   unsigned int *div_denominators)
{
	unsigned int ratio = sci_adc_ratio(channel_id, scale, 0);
	*div_numerators = ratio >> 16;
	*div_denominators = ratio << 16 >> 16;
}

#else
void sci_adc_get_vol_ratio(unsigned int channel_id, int scale, unsigned int *div_numerators,
			   unsigned int *div_denominators)
{
	unsigned int chip_id = 0;

	switch (channel_id) {

	case ADC_CHANNEL_0:
	case ADC_CHANNEL_1:
	case ADC_CHANNEL_2:
	case ADC_CHANNEL_3:
		if (scale) {
			*div_numerators = 16;
			*div_denominators = 41;
		} else {
			*div_numerators = 1;
			*div_denominators = 1;
		}
		return;
	case ADC_CHANNEL_PROG:	//channel 4
	case ADC_CHANNEL_VCHGBG:	//channel 7
	case ADC_CHANNEL_HEADMIC:	//18
		*div_numerators = 1;
		*div_denominators = 1;
		return;
	case ADC_CHANNEL_VBAT:	//channel 5
	case ADC_CHANNEL_ISENSE:	//channel 8
#if defined(CONFIG_ARCH_SCX35)
		*div_numerators = 7;
		*div_denominators = 29;
#else
		chip_id = sci_adi_read(CHIP_ID_LOW_REG);
#ifdef CHIP_ID_HIGH_REG
		chip_id |= (sci_adi_read(CHIP_ID_HIGH_REG) << 16);
#endif
		if (chip_id == 0x8820A001) {	//metalfix
			*div_numerators = 247;
			*div_denominators = 1024;
		} else {
			*div_numerators = 266;
			*div_denominators = 1000;
		}
#endif
		return;
	case ADC_CHANNEL_VCHGSEN:	//channel 6
		*div_numerators = 77;
		*div_denominators = 1024;
		return;
	case ADC_CHANNEL_TPYD:	//channel 9
	case ADC_CHANNEL_TPYU:	//channel 10
	case ADC_CHANNEL_TPXR:	//channel 11
	case ADC_CHANNEL_TPXL:	//channel 12
		if (scale) {	//larger
			*div_numerators = 2;
			*div_denominators = 5;
		} else {
			*div_numerators = 3;
			*div_denominators = 5;
		}
		return;
	case ADC_CHANNEL_DCDCCORE:	//channel 13
	case ADC_CHANNEL_DCDCARM:	//channel 14
		if (scale) {	//lager
			*div_numerators = 4;
			*div_denominators = 5;
		} else {
			*div_numerators = 1;
			*div_denominators = 1;
		}
		return;
	case ADC_CHANNEL_DCDCMEM:	//channel 15
		if (scale) {	//lager
			*div_numerators = 3;
			*div_denominators = 5;
		} else {
			*div_numerators = 4;
			*div_denominators = 5;
		}
		return;
        case ADC_CHANNEL_DCDCLDO:   //16
		*div_numerators = 4;
		*div_denominators = 9;
            return;

#if defined(CONFIG_ARCH_SCX35)
	case ADC_CHANNEL_VBATBK:
	case ADC_CHANNEL_LDO0:
	case ADC_CHANNEL_LDO2:
	case ADC_CHANNEL_USBDP:
	case ADC_CHANNEL_USBDM:
		*div_numerators = 1;
		*div_denominators = 3;
		return;
	case ADC_CHANNEL_LDO1:
		*div_numerators = 1;
		*div_denominators = 2;
		return;
	case ADC_CHANNEL_DCDCGPU:
		if (scale) {
			*div_numerators = 4;
			*div_denominators = 5;
		} else {
			*div_numerators = 1;
			*div_denominators = 1;
		}
		return;
	case ADC_CHANNEL_DCDCWRF:
		if (scale) {
			*div_numerators = 1;
			*div_denominators = 3;
		} else {
			*div_numerators = 4;
			*div_denominators = 5;
		}
		return;
#else
	case ADC_CHANNEL_VBATBK:    //channel 17
	case ADC_CHANNEL_LDO0:		//channel 19,20
	case ADC_CHANNEL_LDO1:
		*div_numerators = 1;
		*div_denominators = 3;
		return;
	case ADC_CHANNEL_LDO2:		//channel 21
		*div_numerators = 1;
		*div_denominators = 2;
		return;
#endif
	default:
		*div_numerators = 1;
		*div_denominators = 1;
		break;
	}
}
#endif

int sci_adc_get_values(struct adc_sample_data *adc)
{
	unsigned long flags;
	int cnt = 12;
	unsigned addr = 0;
	unsigned val = 0;
	int ret = 0;
	int num = 0;
	int sample_bits_msk = 0;
	int *pbuf = 0;

	if (!adc || adc->channel_id > ADC_MAX)
		return -EINVAL;

	pbuf = adc->pbuf;
	if (!pbuf)
		return -EINVAL;

	num = adc->sample_num;
	if (num > ADC_MAX_SAMPLE_NUM)
		return -EINVAL;

	sci_adc_lock();

	sci_adc_config(adc);	//configs adc sample.

	addr = io_base + ADC_CTL;
	val = adc_read(addr);
	val &= ~(BIT_ADC_EN | BIT_SW_CH_ON | BIT_ADC_BIT_MODE_MASK);
	adc_write(val, addr);

	adc_clear_irq();

	val = BIT_SW_CH_RUN_NUM(num);
	val |= BIT_ADC_EN;
	val |= BIT_ADC_BIT_MODE(adc->sample_bits);
	val |= BIT_SW_CH_ON;

	adc_write(val, addr);

	while ((!adc_raw_irqstatus()) && cnt--) {
		udelay(50);
	}

	if (!cnt) {
		ret = -1;
		WARN_ON(1);
		goto Exit;
	}

	if (adc->sample_bits)
		sample_bits_msk = ((1 << 12) - 1);	//12
	else
		sample_bits_msk = ((1 << 10) - 1);	//10
	while (num--)
		*pbuf++ = adc_get_data(sample_bits_msk);

Exit:
	val = adc_read(addr);
	val &= ~BIT_ADC_EN;
	adc_write(val, addr);

	sci_adc_unlock();

	return ret;
}

EXPORT_SYMBOL_GPL(sci_adc_get_values);

static int __average(int a[], int N)
{
#define __DIV_ROUND(n, d)		(((n) + ((d)/2)) / (d))
	int i, sum = 0;

	for (i = 0; i < N; i++)
		sum += a[i];
	return __DIV_ROUND(sum, N);
}

static int sci_adc_set_current(u8 enable, int isen)
{
	if(enable) {
		/* BITS_AUXAD_CURRENT_IBS = (isen * 100 / 125 -1) */
		isen = (isen * 100 / 125 -1);
		if(isen > BITS_AUXAD_CURRENT_IBS(-1))
			isen = BITS_AUXAD_CURRENT_IBS(-1);
		sci_adi_write(ANA_REG_GLB_AUXAD_CTL, (BIT_AUXAD_CURRENTSEN_EN | BITS_AUXAD_CURRENT_IBS(isen)),
			BIT_AUXAD_CURRENTSEN_EN | BITS_AUXAD_CURRENT_IBS(-1));
	} else {
		sci_adi_clr(ANA_REG_GLB_AUXAD_CTL, BIT_AUXAD_CURRENTSEN_EN);
	}

	return 0;
}

/*
 * sci_adc_get_value_by_isen - read adc value by current sense
 * @channel: adc software channel id;
 * @scale: adc sample scale, 0:little scale, 1:big scale;
 * @current: adc current isense(uA),  1.25uA/step, max 40uA;
 *
 * returns: adc value
 */
int sci_adc_get_value_by_isen(unsigned int channel, int scale, int isen)
{
#define ADC_MESURE_NUMBER	15

	struct adc_sample_data adc;
	int results[ADC_MESURE_NUMBER + 1] = {0};
	int ret = 0, i = 0;

	/* Fixme: only external adc channel used */
	BUG_ON(channel > 3);

	WARN_ON(isen > 40);

	adc.channel_id = channel;
	adc.channel_type = 0;
	adc.hw_channel_delay = 0;
	adc.pbuf = &results[0];
	adc.sample_bits = 1;
	adc.sample_num = ADC_MESURE_NUMBER;
	adc.sample_speed = 0;
	adc.scale = scale;
	adc.signal_mode = 0;

	sci_adc_set_current(1, isen);
	if(0 == sci_adc_get_values(&adc)) {
		ret = __average(&results[ADC_MESURE_NUMBER/5],
			(ADC_MESURE_NUMBER - ADC_MESURE_NUMBER * 2 /5));
	}
	sci_adc_set_current(0, 0);

	for(i = 0; i < ARRAY_SIZE(results); i++) {
		printk("%d\t", results[i]);
	}
	printk("\n%s() adc[%d] value: %d\n", __func__, channel, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(sci_adc_get_value_by_isen);

