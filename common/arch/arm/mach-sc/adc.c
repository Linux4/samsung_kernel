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
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/arch_lock.h>

static void __iomem *io_base;		/* Mapped base address */

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

struct sprd_adc_data{
	struct miscdevice misc_dev;
	struct mutex lock;
	unsigned short channel;
	unsigned char scale;
	unsigned int voltage_ratio;
};

static ssize_t sprd_adc_store(struct device *dev,
					struct device_attribute *dev_attr,
					const char *buf, size_t count);
static ssize_t sprd_adc_show(struct device *dev,
				       struct device_attribute *dev_attr, char *buf);

static struct device_attribute sprd_adc_attr[] = {
	__ATTR(adc_channel, S_IRUGO | S_IWUSR | S_IWGRP, sprd_adc_show, sprd_adc_store),
	__ATTR(adc_scale, S_IRUGO | S_IWUSR | S_IWGRP, sprd_adc_show, sprd_adc_store),
	__ATTR(adc_voltage_ratio, S_IRUGO, sprd_adc_show, NULL),
	__ATTR(adc_data_raw, S_IRUGO, sprd_adc_show, NULL),
};

#define SPRD_MODULE_NAME	"sprd-adc"

#define DIV_ROUND(n, d)		(((n) + ((d)/2)) / (d))
#define MEASURE_TIMES		( 15 )
#define ADC_DROP_CNT		( DIV_ROUND(MEASURE_TIMES, 5) )

static int average_int(int a[], int len)
{
	int i, sum = 0;
	for (i = 0; i < len; i++)
		sum += a[i];
	return DIV_ROUND(sum, len);
}

static int compare_val(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

static void dump_adc_data(uint32_t* p_adc_val, int len)
{
	int i = 0;

	printk("dump adc data: ");
	for (i = 0; i < len; i++) {
		printk("%d ", p_adc_val[i]);
	}
	printk("\n");
}

/*
* Get the raw data of adc channel
*/
int sci_get_adc_data(unsigned int channel, unsigned int scale)
{
	uint32_t adc_val[MEASURE_TIMES] = {0}, adc_res = 0;
	struct adc_sample_data adc_sample = {
		.channel_id = channel,
		.channel_type = 0,	/* sw */
		.hw_channel_delay = 0,
		.scale = scale, /* 1: big scale, 0: small scale */
		.pbuf = &adc_val[0],
		.sample_num = MEASURE_TIMES,
		.sample_bits = 1, /* 12 bit*/
		.sample_speed = 0,	/* quick mode */
		.signal_mode = 0,	/* resistance path */
	};

	if (0 != sci_adc_get_values(&adc_sample)) {
		pr_err("sprd_adc error occured in function %s\n", __func__);
		sci_adc_dump_register();
		//BUG_ON();
		return 0;
	}

	dump_adc_data(adc_val, ARRAY_SIZE(adc_val));

	sort(adc_val, ARRAY_SIZE(adc_val), sizeof(uint32_t), compare_val, 0);

	adc_res = average_int(&adc_val[ADC_DROP_CNT], MEASURE_TIMES - ADC_DROP_CNT * 2);

	return (int)adc_res;
}
EXPORT_SYMBOL(sci_get_adc_data);

/*
* The value of adc device node is set to kernel space
*
* Example:
* $ adb root && adb shell
* $ echo 5 > /sys/class/misc/adc_channel  # set current adc channel to 5 (vbat channel)
* $ echo 1 > /sys/class/misc/adc_scale  #set adc to big scale
*/
static ssize_t sprd_adc_store(struct device *dev,
					struct device_attribute *dev_attr,
					const char *buf, size_t count)
{
	struct sprd_adc_data *adc_data = dev_get_drvdata(dev);
	//const ptrdiff_t offset = dev_attr - sprd_adc_attr;
	//unsigned long tmp = simple_strtoul(buf, NULL, 10);
	unsigned int len = 0, tmp_data = 0;

	if ((NULL == adc_data) || (NULL == buf))
		return -EIO;

	len = sscanf(buf, "%d", &tmp_data);

	if (0 == len)
		return -EIO;

	pr_info("%s line: %d, attr name(%s), tmp_data %d, count %d, len %d\n", __func__, __LINE__,
		dev_attr->attr.name, tmp_data, count, len);

	mutex_lock(&adc_data->lock);

	if (0 == strcmp(dev_attr->attr.name, "adc_channel")) {
		WARN_ON(tmp_data > BIT_CH_ID(-1));

		adc_data->channel = (unsigned short)tmp_data;
	} else if (0 == strcmp(dev_attr->attr.name, "adc_scale")) {
		adc_data->scale = (unsigned char)tmp_data;
	}

	mutex_unlock(&adc_data->lock);

	return count;
}

/*
* The value of adc device node is showed to user space
*
* Example:
* $ adb root && adb shell
* $ cat /sys/class/misc/adc_channel  # show current adc channel number
* $ cat /sys/class/misc/adc_scale  #show adc scale value
* $ cat /sys/class/misc/adc_data_raw  #show adc raw data
*/
static ssize_t sprd_adc_show(struct device *dev,
				       struct device_attribute *dev_attr, char *buf)
{
	struct sprd_adc_data *adc_data = dev_get_drvdata(dev);
	int len = 0, adc_value = 0;

	if ((NULL == adc_data) || (NULL == buf))
		return -EIO;

	pr_info("%s line: %d, attr name(%s), channel(%d), scale(%d)\n", __func__, __LINE__,
		dev_attr->attr.name, adc_data->channel, adc_data->scale);

	mutex_lock(&adc_data->lock);

	if (0 == strcmp(dev_attr->attr.name, "adc_channel")) {
		len += sprintf(buf, "%d\n", adc_data->channel);
	} else if (0 == strcmp(dev_attr->attr.name, "adc_scale")) {
		len += sprintf(buf, "%d\n", adc_data->scale);
	} else if (0 == strcmp(dev_attr->attr.name, "adc_voltage_ratio")) {
		unsigned int div_num = 0, div_den = 0;

		sci_adc_get_vol_ratio(adc_data->channel, adc_data->scale, &div_num, &div_den);
		adc_data->voltage_ratio = (div_num << 16) | (div_den & 0xFFFF);
		len += sprintf(buf, "%d\n", adc_data->voltage_ratio);
	} else if (0 == strcmp(dev_attr->attr.name, "adc_data_raw")) {
		adc_value = sci_get_adc_data(adc_data->channel, adc_data->scale);
		if (adc_value < 0)
			adc_value = 0;
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d\n", adc_value);

		pr_info("value: %d, len: %d\n", adc_value, len);
	}

	mutex_unlock(&adc_data->lock);

	return len;
}

static int sprd_device_delete_attributes(struct device *dev,
	struct device_attribute *dev_attrs, int len)
{
	int i = 0;

	for (i = 0; i < len; i++) {
		device_remove_file(dev, &dev_attrs[i]);
	}

	return 0;
}

static int sprd_device_create_attributes(struct device *dev,
	struct device_attribute *dev_attrs, int len)
{
	int i = 0, rc = 0;

	for (i = 0; i < len; i++) {
		rc = device_create_file(dev, &dev_attrs[i]);
		if (rc)
			break;
	}

	if (rc) {
		for (; i >= 0 ; --i)
			device_remove_file(dev, &dev_attrs[i]);
	}

	return rc;
}

static int __init sprd_adc_dev_register(void)
{
	static struct platform_device sprdadc_device = {
		.name = SPRD_MODULE_NAME,
		.id = -1,
	};

	return platform_device_register(&sprdadc_device);
}

static int sprd_adc_probe(struct platform_device *pdev)
{
	struct sprd_adc_data *adc_data = NULL;
	int rc = 0;

	pr_info("%s()->Line: %d\n", __func__, __LINE__);

	adc_data = devm_kzalloc(&pdev->dev, sizeof(struct sprd_adc_data), GFP_KERNEL);
	if (!adc_data) {
		pr_err("%s failed to kzalloc sprd_adc_data!\n", __func__);
		return -ENOMEM;
	}

	adc_data->channel = 5;
	adc_data->misc_dev.minor = MISC_DYNAMIC_MINOR;
	adc_data->misc_dev.name = SPRD_MODULE_NAME; //pdev->dev.driver->name
	adc_data->misc_dev.fops = NULL;
	adc_data->misc_dev.parent = NULL;

	dev_set_drvdata(&pdev->dev, adc_data);

	mutex_init(&adc_data->lock);

	rc = misc_register(&adc_data->misc_dev);
	if (rc) {
		pr_err("%s failed to register misc device.\n", __func__);
		return rc;
	}

	rc = sprd_device_create_attributes(adc_data->misc_dev.this_device, sprd_adc_attr, ARRAY_SIZE(sprd_adc_attr));
	if (rc) {
		pr_err("%s failed to create device attributes.\n", __func__);
		return rc;
	}

	return rc;
}

static int sprd_adc_remove(struct platform_device *pdev)
{
	struct sprd_adc_data *adc_data = platform_get_drvdata(pdev);
	int rc = 0;

	sprd_device_delete_attributes(adc_data->misc_dev.this_device, sprd_adc_attr, ARRAY_SIZE(sprd_adc_attr));

	rc = misc_deregister(&adc_data->misc_dev);
	if (rc) {
		pr_err("%s failed to unregister misc device.\n", __func__);
	}

	kfree(adc_data);

	return rc;
}


#ifdef CONFIG_OF
static struct of_device_id sprd_adc_of_match[] = {
	{ .compatible = "sprd,sprd-adc", },
	{ }
};
#endif

static struct platform_driver sprd_adc_driver = {
	.probe = sprd_adc_probe,
	.remove = sprd_adc_remove,
	.driver = {
		   .name = SPRD_MODULE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(sprd_adc_of_match),
	},
};

static int __init sprd_adc_driver_init(void)
{
	return platform_driver_register(&sprd_adc_driver);
}

static void __exit sprd_adc_driver_exit(void)
{
	platform_driver_unregister(&sprd_adc_driver);
}

module_init(sprd_adc_driver_init);
module_exit(sprd_adc_driver_exit);


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
#ifdef CONFIG_OF
	int ret;
	struct device_node *adc_node;
	struct resource res;

	adc_node = of_find_node_by_name(NULL, "adc");
	if (!adc_node) {
		pr_warn("Can't get the ADC node!\n");
		return;
	}
	pr_info(" find the SPRD ADC node!\n");

	ret = of_address_to_resource(adc_node, 0, &res);
	if (ret < 0) {
		pr_warn("Can't get the adc reg base!\n");
		return;
	}
	io_base = res.start;
	pr_info(" ADC reg base is %p!\n", io_base);

#else
	io_base = adc_base;
	sprd_adc_dev_register();
#endif
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

#if defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2723S)
#define RATIO(_n_, _d_) (_n_ << 16 | _d_)
static int sci_adc_ratio(int channel, int scale, int mux)
{
	switch (channel) {
	case ADC_CHANNEL_0:
		return RATIO(1, 1);
	case ADC_CHANNEL_1:
	case ADC_CHANNEL_2:
	case ADC_CHANNEL_3:
		return (scale ? RATIO(400, 1025) : RATIO(1, 1));
	case ADC_CHANNEL_VBAT:		//Vbat
	case ADC_CHANNEL_ISENSE:
		return RATIO(7, 29);
	case ADC_CHANNEL_VCHGSEN:
		return RATIO(77, 1024);
	case ADC_CHANNEL_DCDCCORE:  //dcdc core/arm/mem/gen/rf/con/wpa
		mux = mux >> 13;
		switch (mux) {
		case 1: //dcdcarm
		case 2: //dcdccore
			return (scale ? RATIO(36, 55) : RATIO(9, 11));
		case 3: //dcdcmem
		case 5: //dcdcrf
			return (scale ? RATIO(12, 25) : RATIO(3, 5));
		case 4: //dcdcgen
			return (scale ? RATIO(3, 10) : RATIO(3, 8));
		case 6: //dcdccon
			return (scale ? RATIO(9, 20) : RATIO(9, 16));
		case 7: //dcdwpa
			return (scale ? RATIO(12, 55) : RATIO(3, 11));
		default:
			return RATIO(1, 1);
		}
	case 0xE:	//LP dcxo
		return (scale ? RATIO(4, 5) : RATIO(1, 1));
	case 0x14:	//headmic
		return RATIO(1, 3);

	case ADC_CHANNEL_LDO0:		//dcdc supply LDO, vdd18/vddcamd/vddcamio/vddrf0/vddgen1/vddgen0
		return RATIO(1, 2);
	case ADC_CHANNEL_VBATBK:	//VbatBK
	case ADC_CHANNEL_LDO1:		//VbatD Domain LDO, vdd25/vddcama/vddsim2/vddsim1/vddsim0
	case ADC_CHANNEL_LDO2:		//VbatA Domain LDO,  vddwifipa/vddcammot/vddemmccore/vdddcxo/vddsdcore/vdd28
	case ADC_CHANNEL_WHTLED:	//kpled/vibr
	case ADC_CHANNEL_WHTLED_VFB://vddsdio/vddusb/vddfgu
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

unsigned int sci_adc_get_ratio(unsigned int channel_id, int scale, int mux)
{
	unsigned int ratio = (unsigned int)sci_adc_ratio(channel_id, scale, mux);

	return ratio;
}

#elif (defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ADIE_SC2711))
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
	case ADC_CHANNEL_VCHGSEN:
		return RATIO(77, 1024);
	case ADC_CHANNEL_DCDCCORE:		//dcdccore
	case ADC_CHANNEL_DCDCARM:		//dcdcarm
		return (scale ? RATIO(4, 5) : RATIO(1, 1));
	case ADC_CHANNEL_DCDCMEM:		//dcdcmem
		return (scale ? RATIO(3, 5) : RATIO(4, 5));
	case ADC_CHANNEL_DCDCLDO:	//dcdcgen
		return RATIO(4, 9);
	case 0x14 /* ADC_CHANNEL_HEADMIC */:	//DCDCHEADMIC
		return RATIO(1, 3);
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

