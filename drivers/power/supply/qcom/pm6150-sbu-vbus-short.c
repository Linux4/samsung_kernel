/*
 * pm6150-sbu-vbus-short.c - Detects SBUx to Vbus short & SBUx to GND short
 *
 * Copyright (C) 2019, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 */


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/qcom-geni-se.h>
#include <linux/sec_class.h>
#include <linux/iio/consumer.h>
#include <linux/pmic-voter.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/log2.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>

#include "smb5-reg.h"
#include "smb5-lib.h"
#include "pm6150-sbu-vbus-short.h"


#define PM6150_SBU_SHORT_DRIVER_NAME 	"pm6150-sbu-short"


/**
* s2mu005_rgb_led - structure to hold all service led related driver data 
* @pdev - holds platform data device
**/
struct pm6150_sbu_short_data{
	int sbu1_short_gpio;
	int sbu2_short_gpio;
	
	struct smb_charger *chg;
	struct device *sbu_dev;
};	

static struct pm6150_sbu_short_data *g_sbu_data;


/**
* pm6150_detect_sbu_vbus_short - Detects and return the status of SBU-VBUS  
*			short or open
**/
static int pm6150_detect_sbu_vbus_short(struct smb_charger *chg)
{
	int val;
	int ret = SBUx_VBUS_OPEN;
	/*
	1. Read VADC Channel 0x8 – This is VBUS / 16. Assume 5V. This will result in a read of 0.3125V. Store in VUSB_V.
a. smblib_get_iio_channel(chg, "usb_in_voltage", &chg->iio.usbin_v_chan);
b. iio_read_channel_processed(chg->iio.usbin_v_chan, &val->intval);
Note: VBUS ADC channel read may no longer be needed since the decision can be made based on whether the SBU ADC reads a saturated value or not. Still, if it is felt that a VBUS ADC value may be helpful, the above step can still be included.
2. Enable 100k pull down on SBU1 by enabling the corresponding GPIO.
3. Enable 10 μA current source on the SBU1 line by setting register 0x156A = 0x04.
4. Read VADC Channel 0x99 – This is SBU1 / 3.
a. smblib_get_iio_channel(chg, "sbux_res", &chg->iio.sbux_chan);
b. iio_read_channel_processed(chg->iio.sbux_chan , &val->intval);
5. Scale both the values appropriately.
6. If SBU1 voltage is close to 1V, then there is no short between SBU1 and VBUS. Continue with rest of the charger detection flow.
7. If SBU1 reads close to 3.8V, short exists between VBUS and SBU1. Restrict charging to 5V only.
8. Repeat steps above for SBU2.
9. If (VBUS_V – SBUx) < 100mV (this threshold can be defined in a variable and customized), then short exists. Restrict 5V charging.
	*/
	
	/* step 1 - May require later - TO DO */
	//iio_read_channel_processed(chg->iio.usbin_v_chan, &val);
	
	/* step 2. Enable 100k pull down on SBU1 by enabling the corresponding 
	GPIO. */
	gpio_direction_output(g_sbu_data->sbu1_short_gpio, 1);
	
	/* step 3. Enable 10 μA current source on the SBU1 line by setting 
	register 0x156A = 0x04. */
	ret = smblib_write(chg, TYPE_C_SBU_CFG_REG, SEL_SBU1_ISRC_VAL);
	if(unlikely(ret < 0)) {
		pr_err("[SBUx] Couldn't select SBU1 rc=%d\n", ret);
		goto ERR_SBU1_SHORT;
	}
	
	/* step 4. Read VADC Channel 0x99 – This is SBU1 / 3. */ 
	ret = iio_read_channel_processed(chg->iio.sbux_chan, &val);
	if(unlikely(ret < 0)) {
		pr_err("[SBUx]Couldn't read SBU1 rc=%d\n", ret);
		goto ERR_SBU1_SHORT;
	}
	
	/* step 5. Scale both the values appropriately.  - Not required*/
	
	/* step 6. If SBU1 voltage is close to 1V, then there is no 
	short between SBU1 and VBUS. Continue with rest of the charger 
	detection flow. */	
	/* 7. If SBU1 reads close to 3.8V, short exists between VBUS and SBU1. 
	Restrict charging to 5V only. */
	
	pr_info("[SBUx] SBU1 to VBUS Voltage : %x\n", val);
	if(val > 3600000 && val < 4000000) {
		pr_info("[SBUx] SBU1 to VBUS Short Detected\n");
		ret = SBU1_VBUS_SHORT;
		goto EXIT_SBU1_SHORT;
	}	
	
	/* Disable SBU1 */
	gpio_direction_output(g_sbu_data->sbu1_short_gpio, 0);
	if(unlikely(smblib_write(chg, TYPE_C_SBU_CFG_REG, 0) < 0)) {
		pr_err("[SBUx]Couldn't disbale SBU1 rc=%d\n");
	}	
	
	/* 8. Repeat steps above for SBU2. */
	gpio_direction_output(g_sbu_data->sbu2_short_gpio, 1);
	ret = smblib_write(chg, TYPE_C_SBU_CFG_REG, SEL_SBU2_ISRC_VAL);
	if(unlikely(ret < 0)) {
		pr_err("[SBUx]Couldn't select SBU2 rc=%d\n", ret);
		goto ERR_SBU2_SHORT;
	}
	ret = iio_read_channel_processed(chg->iio.sbux_chan, &val);
	if(unlikely(ret < 0)) {
		pr_err("[SBUx]Couldn't read SBU1 rc=%d\n", ret);
		goto ERR_SBU2_SHORT;
	}
	
	pr_info("[SBUx] SBU2 to VBUS Voltage : %x\n", val);
	if(val > 3600000 && val < 4000000) {
		pr_info("[SBUx] SBU2 to VBUS Short Detected\n");
		ret = SBU2_VBUS_SHORT;
		goto EXIT_SBU2_SHORT;
	}
	
	pr_info("[SBUx] SBUx to VBUS is OPEN\n");
	ret = SBUx_VBUS_OPEN;
	goto EXIT_SBU2_SHORT;
	
	/* Step 9 not required */
	/* 9. If (VBUS_V – SBUx) < 100mV (this threshold can be defined in a 
	variable and customized), then short exists. Restrict 5V charging. */


ERR_SBU1_SHORT:
	ret = SBUx_SHORT_UNDEF;
EXIT_SBU1_SHORT:
	/* Disable SBU1 GPIO */
	gpio_direction_output(g_sbu_data->sbu1_short_gpio, 0);
	goto EXIT_SBUx;
ERR_SBU2_SHORT:
	ret = SBUx_SHORT_UNDEF;
EXIT_SBU2_SHORT:
	/* Disable SBU2 GPIO */
	gpio_direction_output(g_sbu_data->sbu2_short_gpio, 0);
EXIT_SBUx:
	if(unlikely(smblib_write(chg, TYPE_C_SBU_CFG_REG, 0) < 0)) {
		pr_err("[SBUx]Couldn't disbale SBU2 rc=%d\n");
	}
	return ret;
}


/**
* pm6150_detect_sbu_gnd_short - Detects and return the status of SBU-GND  
*			short or open
**/
static int pm6150_detect_sbu_gnd_short(struct smb_charger *chg)
{
	int val;
	int ret;
	/*
	Steps to detect short are as follows:
1. Ensure that there is no short detected between VBUS to SBU.
2. Enable 180 μA current source on SBU1. To do this, set register 0x156A = 0x8.
3. Read SBUx ADC channel through the API given above. Store result in SBU1_RES.
a. smblib_get_iio_channel(chg, "sbux_res", &chg->iio.sbux_chan);
b. iio_read_channel_processed(chg->iio.sbux_chan , &val->intval);
4. If SBU1_RES < 180 mV, then, a short with resistance less than 1k to GND exists on SBU1 line.
5. Disable 180 μA current source on SBU1. To do this, set register 0x156A = 0x0.
6. Enable 180 μA current source on SBU2. To do this, set register 0x156A = 0x2.
7. Read SBUx ADC channel through the API given above. Store result in SBU2_RES.
a. smblib_get_iio_channel(chg, "sbux_res", &chg->iio.sbux_chan);
b. iio_read_channel_processed(chg->iio.sbux_chan , &val->intval);
8. If SBU2_RES < 180 mV, then, a short with resistance less than 1k to GND exists on SBU2 line.
9. Disable current source. To do this, set register 0x156A = 0x0.
	*/
	
	
	/* step 1. Ensure that there is no short detected between VBUS to SBU. */
	
	/* step 2. Enable 180 μA current source on SBU1. To do this, 
	set register 0x156A = 0x8. */
	ret = smblib_write(chg, TYPE_C_SBU_CFG_REG, 0x8);
	if(unlikely(ret < 0)) {
		pr_err("[SBUx]Couldn't select SBU1 rc=%d\n", ret);
		return SBUx_SHORT_UNDEF;
	}
	
	/* step 3. Read SBUx ADC channel through the API given above. 
	Store result in SBU1_RES.
		a. smblib_get_iio_channel(chg, "sbux_res", &chg->iio.sbux_chan);
		b. iio_read_channel_processed(chg->iio.sbux_chan , &val->intval);
	*/
	ret = iio_read_channel_processed(chg->iio.sbux_chan , &val);
	if(unlikely(ret < 0)) {
		pr_err("[SBUx]Couldn't read SBU1 rc=%d\n", ret);
		return SBUx_SHORT_UNDEF;
	}
	
	pr_info("[SBUx] SBU1 to GND Voltage : %x\n", val);
	
	/* step 4. If SBU1_RES < 180 mV, then, a short with resistance 
	less than 1k to GND exists on SBU1 line. */
	if(val < 0x195000) { // TO DO : Need to check this value for 180 mV
		pr_info("[SBUx] SBU1 to GND Short Detected\n");
		ret = SBU1_GND_SHORT;
		goto SBUx_GND_EXIT;
	}
	
	/* step 5. Disable 180 μA current source on SBU1. To do this,
	set register 0x156A = 0x0. */
	ret = smblib_write(chg, TYPE_C_SBU_CFG_REG, 0x0);
	if(unlikely(ret < 0)) {
		pr_err("[SBUx]Couldn't select SBU1 rc=%d\n", ret);
		return SBUx_SHORT_UNDEF;
	}
	
	/* step 6. Enable 180 μA current source on SBU2. To do this,
	set register 0x156A = 0x2. */
	ret = smblib_write(chg, TYPE_C_SBU_CFG_REG, 0x2);
	if(unlikely(ret < 0)) {
		pr_err("[SBUx]Couldn't select SBU1 rc=%d\n", ret);
		return SBUx_SHORT_UNDEF;
	}
	
	/* step 7. Read SBUx ADC channel through the API given above.
	Store result in SBU2_RES. 
		a. smblib_get_iio_channel(chg, "sbux_res", &chg->iio.sbux_chan);
		b. iio_read_channel_processed(chg->iio.sbux_chan , &val->intval);
	*/
	ret = iio_read_channel_processed(chg->iio.sbux_chan , &val);
	if(unlikely(ret < 0)) {
		pr_err("[SBUx]Couldn't read SBU1 rc=%d\n", ret);
		return SBUx_SHORT_UNDEF;
	}
	
	pr_info("[SBUx] SBU2 to GND Voltage : %x\n", val);
	/* step 8. If SBU2_RES < 180 mV, then, a short with resistance
	less than 1k to GND exists on SBU2 line. */
	if(val < 0x195000) { // TO DO : Need to check this value for 180 mV
		pr_info("[SBUx] SBU2 to GND Short Detected\n");
		ret = SBU2_GND_SHORT;
		goto SBUx_GND_EXIT;
	}
	
#if 0 // move to the end
	/* step 9. Disable current source. To do this, 
	set register 0x156A = 0x0. */
	ret = smblib_write(chg, TYPE_C_SBU_CFG_REG, 0x0);
	if(unlikely(ret < 0)) {
		pr_err("[SBUx]Couldn't select SBU1 rc=%d\n", ret);
		return SBUx_SHORT_UNDEF;
	}
#endif
	
	ret = SBUx_VBUS_OPEN;
	
SBUx_GND_EXIT:
	/* step 9. Disable current source. To do this,
	set register 0x156A = 0x0. */
	smblib_write(chg, TYPE_C_SBU_CFG_REG, 0x0);
	return ret;
}

#if 0
/**
* pm6150_detect_sbu_cc_short - Detects and return the status of SBU-CC 
*			short or open
**/
static int pm6150_detect_sbu_cc_short(struct smb_charger *chg)
{
	return SBUx_VBUS_OPEN;
}
#endif

/**
* pm6150_detect_cc_vbus_short - Detects and return the status of VBUS-CC 
*			short or open
**/
static int pm6150_detect_cc_vbus_short(struct smb_charger *chg)
{
	return \
		(smblib_get_prop_ufp_mode(chg) == POWER_SUPPLY_TYPEC_NON_COMPLIANT) ? \
		CCx_VBUS_SHORT : SBUx_VBUS_OPEN;
}


/**
* pm6150_detect_sbu_short - Detects and return the status of SBU-GND short or 
*			SBU-VBUS short or open. This will be called before and from AFC.
**/
int pm6150_detect_sbu_short(void)
{
	int isShort; 
	
	isShort = pm6150_detect_sbu_vbus_short(g_sbu_data->chg);
	if(unlikely(isShort != SBUx_VBUS_OPEN))
		return isShort;
		
	isShort = pm6150_detect_sbu_gnd_short(g_sbu_data->chg);
	if(unlikely(isShort != SBUx_VBUS_OPEN))
		return isShort;
	
#if 0
	isShort = pm6150_detect_sbu_cc_short(g_sbu_data->chg);
	if(unlikely(isShort != SBUx_VBUS_OPEN))
		return isShort;
#endif	
	
	isShort = pm6150_detect_cc_vbus_short(g_sbu_data->chg);
	if(unlikely(isShort != SBUx_VBUS_OPEN))
		return isShort;
	
	return SBUx_VBUS_OPEN;
}
EXPORT_SYMBOL(pm6150_detect_sbu_short);


#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
/**
* pm6150_sbu_short_show - sysfs read interface to show Subx 
*      short or open status.
*      Accessible only on non-ship binaries.
**/
static ssize_t pm6150_sbu_short_show(struct device *dev, \
		struct device_attribute *attr, char *buf)
{
	switch(pm6150_detect_sbu_short())
	{
	case SBUx_VBUS_OPEN:
		return sprintf(buf, "SBUx - Open\n");
		
	case SBUx_VBUS_SHORT:
		return sprintf(buf, "SBUx to VBus short\n");
	case SBU1_VBUS_SHORT:
		return sprintf(buf, "SBU1 to VBus short\n");
	case SBU2_VBUS_SHORT:
		return sprintf(buf, "SBU2 to VBus short\n");

	case SBUx_GND_SHORT:
		return sprintf(buf, "SBUx to GND short\n");
	case SBU1_GND_SHORT:
		return sprintf(buf, "SBU1 to GND short\n");
	case SBU2_GND_SHORT:
		return sprintf(buf, "SBU2 to GND short\n");
	
	case CCx_VBUS_SHORT:
		return sprintf(buf, "CC to VBus short\n");	

	case SBUx_CCx_SHORT:
		return sprintf(buf, "SBUx to CCx short\n");
	case SBU1_CC1_SHORT:
		return sprintf(buf, "SBU1 to CC1 short\n");
	case SBU1_CC2_SHORT:
		return sprintf(buf, "SBU1 to CC2 short\n");
	case SBU2_CC1_SHORT:
		return sprintf(buf, "SBU2 to CC1 short\n");
	case SBU2_CC2_SHORT:
		return sprintf(buf, "SBU2 to CC2 short\n");

	case SBUx_SHORT_UNDEF:
	default:
		return sprintf(buf, "SBUx-CC-VBUS:status Unknown\n");
	}
}
static DEVICE_ATTR(sbux_short, S_IRUGO, pm6150_sbu_short_show, NULL);

static struct attribute *sbux_attributes[] = {
	&dev_attr_sbux_short.attr,
	NULL,
};

static struct attribute_group sbux_attr_group = {
	.attrs = sbux_attributes,
};


/**
* pm6150_sbu_short_sysfs_init - creates sysfs for sbu short
*			will be used only in development binaries.
**/
int pm6150_sbu_short_sysfs_init()
{
	int ret;
	
	g_sbu_data->sbu_dev = sec_device_create(0, NULL, "sbux_short");
	if (unlikely(IS_ERR(g_sbu_data->sbu_dev))) {
		pr_err("[SBUx]sysfs dir sbux_short create failed\n");
		return -ENODEV;
	}
	
	ret = sysfs_create_group(&g_sbu_data->sbu_dev->kobj, &sbux_attr_group);
	if (unlikely(ret)) {
		pr_err("[SBUx]sysfs sbux-short create failed\n");
	}
	
	return 0;
}
#endif // CONFIG_SAMSUNG_PRODUCT_SHIP


/**
* pm6150_sbu_short_probe - parses hw details from dts
**/
static int pm6150_sbu_short_parse_dt(struct smb_charger *chg, \
					struct pm6150_sbu_short_data *sbu_data)
{
	int ret;
	struct device_node *node = chg->dev->of_node;
	
	sbu_data->sbu1_short_gpio = of_get_named_gpio(node, "sbu1-short-gpio", 0);
	ret = gpio_request(sbu_data->sbu1_short_gpio, "sbu1-short-gpio");	
	if(unlikely(ret))
		return -ENODEV;
	
	sbu_data->sbu2_short_gpio = of_get_named_gpio(node, "sbu2-short-gpio", 0);
	ret = gpio_request(sbu_data->sbu2_short_gpio, "sbu2-short-gpio");	
	if(unlikely(ret))
		return -ENODEV;
	
	return 0;
}


/**
* pm6150_detect_sbu_short_init - Enumerates SBU Short detection resources.
**/
int pm6150_detect_sbu_short_init(struct smb_charger *chg)
{
	int ret;
	struct pm6150_sbu_short_data *sbu_data;

	sbu_data = devm_kzalloc(chg->dev, sizeof(*sbu_data), GFP_KERNEL);
	if (unlikely(!sbu_data))
		return -ENOMEM;
	
	g_sbu_data = sbu_data;
	
	sbu_data->chg = chg;
	
	ret =  pm6150_sbu_short_parse_dt(chg, sbu_data);
	if (unlikely(ret)) {
		pr_err("[SBUx][%s] pm6150_sbu_short_parse_dt failed\n", __func__);
	}
	
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	pm6150_sbu_short_sysfs_init();
#endif // CONFIG_SAMSUNG_PRODUCT_SHIP

	pr_info("[SBUx][%s] pm6150_sbu_short_parse_dt init successful\n", __func__);
	
	return 0;
}
