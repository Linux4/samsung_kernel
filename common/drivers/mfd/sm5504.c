/* drivers/mfd/SM5504.c
 * SM5504 Multifunction Device / MUIC Driver
 *
 * Copyright (C) 2013
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/rtdefs.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/mfd/sm5504.h>
#include <linux/usb_notify.h>
//#ifdef CONFIG_MUIC_SUPPORT_FACTORY
#include <linux/platform_data/mv_usb.h>
#ifdef CONFIG_MUIC_FACTORY_EVENT
#include <linux/switch.h>
#endif
//#endif
#if defined(CONFIG_MUIC_SUPPORT_RUSTPROOF) 
#include <linux/battery/sec_charger.h>
extern int sec_vf_adc_check(void);
extern int sec_vf_adc_current_check(void);
#endif

#ifdef CONFIG_USB_INTERRUPT_BY_MUIC
extern void dwc_udc_startup(void);
extern void dwc_udc_shutdown(void);
#endif


#define SM5504_DEVICE_NAME "sm5504"
#define ALIAS_NAME SM5504_DEVICE_NAME
#define SM5504_DRV_VER "2.1.0SPRD"
#define SM5504_IRQF_MODE IRQF_TRIGGER_LOW

#define SM5504_REG_CHIP_ID          0x01
#define SM5504_REG_CONTROL          0x02
#define SM5504_REG_INT_FLAG1        0x03
#define SM5504_REG_INT_FLAG2        0x04
#define SM5504_REG_INTERRUPT_MASK1  0x05
#define SM5504_REG_INTERRUPT_MASK2  0x06
#define SM5504_REG_ADC              0x07
#define SM5504_REG_DEVICE1          0x0A
#define SM5504_REG_DEVICE2          0x0B
#define SM5504_REG_MANUAL_SW1       0x13
#define SM5504_REG_MANUAL_SW2       0x14
#define SM5504_REG_RESET            0x1B
#define SM5504_REG_CHG_TYPE         0x24

#define DCD_T_RETRY 2

#define SM5504_DEVICE1_OTG  0x01
#define SM5504_DEVICE1_SDP  0x04
#define SM5504_DEVICE1_UART 0x08
#define SM5504_DEVICE1_CDPORT   0x20
#define SM5504_DEVICE1_DCPORT   0x40

//#ifdef CONFIG_MUIC_SUPPORT_FACTORY
#ifdef CONFIG_MUIC_FACTORY_EVENT
bool is_factory_start = false;
extern int switch_dev_register(struct switch_dev *sdev);
extern void switch_set_state(struct switch_dev *sdev, int state);
#endif
extern struct class *sec_class;
//#endif
int epmic_event_handler(int level);

#if defined(CONFIG_MUIC_SUPPORT_RUSTPROOF)
bool is_close_uart_path = false;
bool enable_muic_uart = false;
#endif

#if defined(CONFIG_TOUCHSCREEN_IST30XXA) || defined(CONFIG_TOUCHSCREEN_IST30XXB) || defined(CONFIG_TOUCHSCREEN_BAFFINE_LITE_VE_IST3038)
int flag =0;
extern void ist30xx_set_ta_mode(bool charging);
#endif


static irqreturn_t sm5504_irq_handler(int irq, void *data);

struct device_desc {
	char *name;
	uint32_t reg_val;
	int cable_type;
};

static const struct device_desc device_to_cable_type_mapping[] = {
	{
	 .name = "OTG",
	 .reg_val = SM5504_DEVICE1_OTG,
	 .cable_type = MUIC_SM5504_CABLE_TYPE_OTG,
	 },
	{
	 .name = "USB SDP",
	 .reg_val = SM5504_DEVICE1_SDP,
	 .cable_type = MUIC_SM5504_CABLE_TYPE_USB,
	 },
	{
	 .name = "UART",
	 .reg_val = SM5504_DEVICE1_UART,
	 .cable_type = MUIC_SM5504_CABLE_TYPE_UART,
	 },
	{
	 .name = "USB CDP",
	 .reg_val = SM5504_DEVICE1_CDPORT,
	 .cable_type = MUIC_SM5504_CABLE_TYPE_CDP,
	 },
	{
	 .name = "USB DCP",
	 .reg_val = SM5504_DEVICE1_DCPORT,
	 .cable_type = MUIC_SM5504_CABLE_TYPE_REGULAR_TA,
	 },
};

struct id_desc {
	char *name;
	int cable_type_with_vbus;
	int cable_type_without_vbus;
};

static const struct id_desc id_to_cable_type_mapping[] = {
	{
	 /* 00000, 0 */
	 .name = "OTG",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_OTG_WITH_VBUS,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_OTG,
	 },
	{			/* 00001, 1 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 00010, 2 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 00011, 3 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 00100, 4 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 00101, 5 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 00110, 6 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 00111, 7 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 01000, 8 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 01001, 9 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 01010, 10 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 01011, 11 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 01100, 12 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 01101, 13 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 01110, 14 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 01111, 15 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 10000, 16 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 10001, 17 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 10010, 18 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 10011, 19 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 10100, 20 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 10101, 21 */
	 .name = "ADC0x15 Charger/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_0x15,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 10110, 22 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 10111, 23 */
	 .name = "Type 1 Charger/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_TYPE1_CHARGER,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 11000, 24 */
	 .name = "FM BOOT OFF USB/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_JIG_USB_OFF,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 11001, 25 */
	 .name = "FM BOOT ON USB/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_JIG_USB_ON,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 11010, 26 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 11011, 27 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 11100, 28 */
	 .name = "JIG UART BOOT OFF",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF,
	 },
	{			/* 11101, 29 */
	 .name = "JIG UART BOOT ON",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_JIG_UART_ON_WITH_VBUS,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_JIG_UART_ON,
	 },
	{			/* 11110, 30 */
	 .name = "AT&T TA/Unknown",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_UNKNOWN,
	 },
	{			/* 11111, 31 */
	 .name = "AT&T TA/No cable",
	 .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
	 .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_NONE,
	 },
	{           /* 100000, 32 */  // MHL 0x20
	  .name = "AT&T TA/No cable",
        .cable_type_with_vbus = MUIC_SM5504_CABLE_TYPE_ATT_TA,
        .cable_type_without_vbus = MUIC_SM5504_CABLE_TYPE_NONE,
        },
};

enum {
	VBUS_SHIFT = 0,
	ACCESSORY_SHIFT,
	OCP_SHIFT,
	OVP_SHIFT,
	OTP_SHIFT,
	ADC_CHG_SHIFT,
	CABLE_CHG_SHIFT,
	OTG_SHIFT,
	DCDT_SHIFT,
	USB_SHIFT,
	UART_SHIFT,
	JIG_SHIFT,
	L_USB_SHIFT,
};

struct sm5504_status {
	int cable_type;
	int id_adc;
	uint8_t irq_flags[2];
	uint8_t device_reg[2];
	/* Processed useful status
	 * Compare previous and current regs
	 * to get this information */
	union {
		struct {
			uint32_t vbus_status:1;
			uint32_t accessory_status:1;
			uint32_t ocp_status:1;
			uint32_t ovp_status:1;
			uint32_t otp_status:1;
			uint32_t adc_chg_status:1;
			uint32_t cable_chg_status:1;
			uint32_t otg_status:1;
			uint32_t dcdt_status:1;
			uint32_t usb_connect:1;
			uint32_t uart_connect:1;
			uint32_t jig_connect:1;
			uint32_t l_usb_connect:1;
		};
		uint32_t status;
	};
};

typedef struct sm5504_chip {
	struct i2c_client *iic;
	struct mutex io_lock;
	struct sm5504_platform_data *pdata;
	struct device *dev;
//#ifdef CONFIG_MUIC_SUPPORT_FACTORY
	struct device *switch_dev;
//#endif

#ifdef CONFIG_USB_INTERRUPT_BY_MUIC
	bool			is_usb_ready;
#endif

	struct workqueue_struct *wq;
	/*struct delayed_work irq_work;*/
	struct delayed_work init_work;
#ifdef CONFIG_USB_INTERRUPT_BY_MUIC
	struct delayed_work usb_work;
#endif
	struct wake_lock muic_wake_lock;
	struct sm5504_status prev_status;
	struct sm5504_status curr_status;
	int dcdt_retry_count;
	int irq;
	int adc_reg_addr;
} sm5504_chip_t;

//#ifdef CONFIG_MUIC_SUPPORT_FACTORY
static struct sm5504_status *current_status;
#ifdef CONFIG_MUIC_FACTORY_EVENT
static struct switch_dev switch_dock = {
        .name = "dock",
};
#endif
//#endif

static int32_t sm5504_read(struct sm5504_chip *chip, uint8_t reg,
			   uint8_t nbytes, uint8_t *buff)
{
	int ret;
	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_read_i2c_block_data(chip->iic, reg, nbytes, buff);
	mutex_unlock(&chip->io_lock);
	return ret;
}

static int32_t sm5504_reg_read(struct sm5504_chip *chip, int reg)
{
	int ret;
	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_read_byte_data(chip->iic, reg);
	mutex_unlock(&chip->io_lock);
	return ret;
}

int32_t sm5504_reg_write(struct sm5504_chip *chip, int reg, unsigned char data)
{
	int ret;
	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_write_byte_data(chip->iic, reg, data);
	mutex_unlock(&chip->io_lock);
	return ret;
}

static int32_t sm5504_assign_bits(struct sm5504_chip *chip, int reg,
				  unsigned char mask, unsigned char data)
{
	struct i2c_client *iic;
	int ret;
	iic = chip->iic;
	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_read_byte_data(iic, reg);
	if (ret < 0)
		goto out;
	ret &= ~mask;
	ret |= data;
	ret = i2c_smbus_write_byte_data(iic, reg, ret);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
}

int32_t sm5504_set_bits(struct sm5504_chip *chip, int reg, unsigned char mask)
{
	return sm5504_assign_bits(chip, reg, mask, mask);
}

int32_t sm5504_clr_bits(struct sm5504_chip *chip, int reg, unsigned char mask)
{
	return sm5504_assign_bits(chip, reg, mask, 0);
}

static inline int sm5504_update_regs(sm5504_chip_t *chip)
{
	int ret;
	ret = sm5504_read(chip, SM5504_REG_INT_FLAG1,
			  2, chip->curr_status.irq_flags);
	if (ret < 0)
	{
		RTERR("%s line %d : return %d\n", __FUNCTION__,__LINE__,ret);
		return ret;
	}
	ret = sm5504_read(chip, SM5504_REG_DEVICE1,
			  2, chip->curr_status.device_reg);
	if (ret < 0)
	{
                RTERR("%s line %d : return %d\n", __FUNCTION__,__LINE__,ret);
		return ret;
	}
	ret = sm5504_reg_read(chip, chip->adc_reg_addr);
	if (ret < 0)
	{
		RTERR("%s line %d : return %d\n", __FUNCTION__,__LINE__,ret);
		return ret;
	}
	chip->curr_status.id_adc = ret;
	/* invalid id value */
	if (ret >= ARRAY_SIZE(id_to_cable_type_mapping))
        return -EINVAL;
	return 0;
}

static int sm5504_get_cable_type_by_device_reg(sm5504_chip_t *chip)
{
	uint32_t device_reg = chip->curr_status.device_reg[1];
	int i;
	RTINFO("Device = 0x%x, 0x%x\n",
	       (int)chip->curr_status.device_reg[0],
	       (int)chip->curr_status.device_reg[1]);
	device_reg <<= 8;
	device_reg |= chip->curr_status.device_reg[0];
	for (i = 0; i < ARRAY_SIZE(device_to_cable_type_mapping); i++) {
		if (device_to_cable_type_mapping[i].reg_val == device_reg) {
			if (chip->curr_status.dcdt_status == 1) {
                /* USB ID ==> open, i.e., ADC reading = 0x1f */
				return id_to_cable_type_mapping[0x1f].cable_type_with_vbus;
			}
			return device_to_cable_type_mapping[i].cable_type;
		}
	}
	/* not found */
	return -EINVAL;
}

static int sm5504_get_cable_type_by_id(sm5504_chip_t *chip)
{
	int id_val = chip->curr_status.id_adc;
	int cable_type;
	int ret;

	RTINFO("ID value = 0x%x\n", id_val);
	if (chip->curr_status.vbus_status)
		cable_type = id_to_cable_type_mapping[id_val].cable_type_with_vbus;
	else
		cable_type = id_to_cable_type_mapping[id_val].cable_type_without_vbus;

    	if ( (cable_type == MUIC_SM5504_CABLE_TYPE_TYPE1_CHARGER )
        	&& ( ( chip->curr_status.irq_flags[1] & 0x01 ) == 0x01 ) ) {

        ret = sm5504_reg_read(chip, SM5504_REG_CHG_TYPE);
        if (ret >= 0) {
            	switch(ret){
            		case 0x01 : // DCP
                		cable_type = MUIC_SM5504_CABLE_TYPE_REGULAR_TA;
                		break;
            		case 0x02 : // CDP
                		cable_type = MUIC_SM5504_CABLE_TYPE_CDP;
                		break;
            		case 0x04 : // SDP
                		cable_type = MUIC_SM5504_CABLE_TYPE_L_USB;
                		break;
            		case 0x08 : // Timeout SDP
                		cable_type = MUIC_SM5504_CABLE_TYPE_L_USB;
                		break;
            		}	
        	}	
    	}

	/* To remove OTG accessory, VBUS would still exist*/
	if ((chip->prev_status.cable_type ==
		MUIC_SM5504_CABLE_TYPE_OTG) && id_val == 0x1f)
		cable_type = MUIC_SM5504_CABLE_TYPE_NONE;

	return cable_type;
}

static int sm5504_get_cable_type(sm5504_chip_t *chip)
{
	int ret;
	/* Check dangerous case first and it can make system stop charging */
	if (chip->curr_status.ovp_status | chip->curr_status.ocp_status)
        return MUIC_SM5504_CABLE_TYPE_UNKNOWN;
	ret = sm5504_get_cable_type_by_device_reg(chip);
	if (ret >= 0)
		return ret;
	else
		return sm5504_get_cable_type_by_id(chip);
}

static void sm5504_preprocess_status(sm5504_chip_t *chip)
{
	chip->curr_status.ocp_status =
	    (chip->curr_status.irq_flags[1] & 0x60) ? 1 : 0;
#if defined(CONFIG_MACH_GRANDNEOVE3G)
	chip->curr_status.ovp_status =
	    ( (  (chip->curr_status.irq_flags[1] & 0x10) && ( (chip->curr_status.irq_flags[1] & 0x80) == 0x00 )  ) ||
	    (chip->curr_status.irq_flags[0] & 0x10)) ? 1 : 0;
#else
	chip->curr_status.ovp_status =
	    ((chip->curr_status.irq_flags[1] & 0x10) ||
	    (chip->curr_status.irq_flags[0] & 0x10)) ? 1 : 0;
#endif
	chip->curr_status.otp_status =
	    ((chip->curr_status.irq_flags[1] & 0x08) ||
        (chip->curr_status.irq_flags[0] & 0x80))  ? 1 : 0;
	chip->curr_status.dcdt_status =
	    (chip->curr_status.irq_flags[0] & 0x08) ? 1 : 0;
	chip->curr_status.vbus_status =
	    (chip->curr_status.irq_flags[1] & 0x02) ? 0 : 1;
	chip->curr_status.cable_type = sm5504_get_cable_type(chip);
	chip->curr_status.adc_chg_status =
	    (chip->prev_status.id_adc != chip->curr_status.id_adc) ? 1 : 0;
	chip->curr_status.otg_status =
	    (chip->curr_status.cable_type ==
	     MUIC_SM5504_CABLE_TYPE_OTG) ? 1 : 0;
	chip->curr_status.accessory_status =
	    ((chip->curr_status.cable_type !=
	      MUIC_SM5504_CABLE_TYPE_NONE) &&
	     (chip->curr_status.cable_type !=
	      MUIC_SM5504_CABLE_TYPE_UNKNOWN)) ? 1 : 0;
	chip->curr_status.cable_chg_status =
	    (chip->curr_status.cable_type !=
	     chip->prev_status.cable_type) ? 1 : 0;
	chip->curr_status.usb_connect =
	    ((chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_USB) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_CDP) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_USB_OFF) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_L_USB) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_USB_ON)) ? 1 : 0;
	chip->curr_status.uart_connect =
	    ((chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_UART) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_UART_ON) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_UART_ON_WITH_VBUS)) ? 1 : 0;
	chip->curr_status.jig_connect =
	    ((chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_USB_OFF) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_USB_ON) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_UART_ON) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS) ||
	     (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_JIG_UART_ON_WITH_VBUS)) ? 1 : 0;
    chip->curr_status.l_usb_connect = (chip->curr_status.cable_type ==
	      MUIC_SM5504_CABLE_TYPE_L_USB) ? 1 : 0;
}

#define FLAG_HIGH           (0x01)
#define FLAG_LOW            (0x02)
#define FLAG_LOW_TO_HIGH    (0x04)
#define FLAG_HIGH_TO_LOW    (0x08)
#define FLAG_RISING         FLAG_LOW_TO_HIGH
#define FLAG_FALLING        FLAG_HIGH_TO_LOW
#define FLAG_CHANGED        (FLAG_LOW_TO_HIGH | FLAG_HIGH_TO_LOW)

static inline uint32_t state_check(unsigned int old_state,
				   unsigned int new_state,
				   unsigned int bit_mask)
{
	unsigned int ret = 0;
	old_state &= bit_mask;
	new_state &= bit_mask;
	if (new_state)
		ret |= FLAG_HIGH;
	else
		ret |= FLAG_LOW;
	if (old_state != new_state) {
		if (new_state)
			ret |= FLAG_LOW_TO_HIGH;
		else
			ret |= FLAG_HIGH_TO_LOW;
	}
	return ret;
}

struct sm5504_event_handler {
	char *name;
	uint32_t bit_mask;
	uint32_t type;
	void (*handler) (struct sm5504_chip *chip,
			 const struct sm5504_event_handler *handler,
			 unsigned int old_status, unsigned int new_status);

};

static void sm5504_ocp_handler(struct sm5504_chip *chip,
			       const struct sm5504_event_handler *handler,
			       unsigned int old_status, unsigned int new_status)
{
	RTERR("OCP event triggered\n");
	if (chip->pdata->ocp_callback)
		chip->pdata->ocp_callback();
}

static void sm5504_ovp_handler(struct sm5504_chip *chip,
			       const struct sm5504_event_handler *handler,
			       unsigned int old_status, unsigned int new_status)
{
	RTERR("OVP event triggered\n");
	if (chip->pdata->ovp_callback)
		chip->pdata->ovp_callback();
}

static void sm5504_otp_handler(struct sm5504_chip *chip,
			       const struct sm5504_event_handler *handler,
			       unsigned int old_status, unsigned int new_status)
{
	RTERR("OTP event triggered\n");
	if (chip->pdata->otp_callback)
		chip->pdata->otp_callback();
}

struct sm5504_event_handler urgent_event_handlers[] = {
	{
	 .name = "OCP",
	 .bit_mask = (1 << OCP_SHIFT),
	 .type = FLAG_RISING,
	 .handler = sm5504_ocp_handler,
	 },
	{
	 .name = "OVP",
	 .bit_mask = (1 << OVP_SHIFT),
	 .type = FLAG_RISING,
	 .handler = sm5504_ovp_handler,
	 },
	{
	 .name = "OTP",
	 .bit_mask = (1 << OTP_SHIFT),
	 .type = FLAG_RISING,
	 .handler = sm5504_otp_handler,
	 },
};

#if RTDBGINFO_LEVEL <= RTDBGLEVEL
static char *sm5504_cable_names[] = {

	"MUIC_SM5504_CABLE_TYPE_NONE",
	"MUIC_SM5504_CABLE_TYPE_UART",
	"MUIC_SM5504_CABLE_TYPE_USB",
	"MUIC_SM5504_CABLE_TYPE_OTG",
	/* TA Group */
	"MUIC_SM5504_CABLE_TYPE_REGULAR_TA",
	"MUIC_SM5504_CABLE_TYPE_ATT_TA",
	"MUIC_SM5504_CABLE_TYPE_0x15",
	"MUIC_SM5504_CABLE_TYPE_TYPE1_CHARGER",
	"MUIC_SM5504_CABLE_TYPE_0x1A",
	/* JIG Group */
	"MUIC_SM5504_CABLE_TYPE_JIG_USB_OFF",
	"MUIC_SM5504_CABLE_TYPE_JIG_USB_ON",
	"MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF",
	"MUIC_SM5504_CABLE_TYPE_JIG_UART_ON",
	/* JIG type with VBUS */
	"MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS",
	"MUIC_SM5504_CABLE_TYPE_JIG_UART_ON_WITH_VBUS",

	"MUIC_SM5504_CABLE_TYPE_CDP",
	"MUIC_SM5504_CABLE_TYPE_L_USB",
	"MUIC_SM5504_CABLE_TYPE_UNKNOWN",
	"MUIC_SM5504_CABLE_TYPE_INVALID",
};
#endif /*RTDBGINFO_LEVEL<=RTDBGLEVEL */

#if defined(CONFIG_BATTERY_SAMSUNG) || defined(CONFIG_RT_BATTERY)
#define cable_change_callback sec_charger_cb
extern void sec_charger_cb(u8 cable_type);
#endif

static void sm5504_cable_change_handler(struct sm5504_chip *chip,
					const struct sm5504_event_handler
					*handler, unsigned int old_status,
					unsigned int new_status)
{
	RTINFO("Cable change to %s\n",
	       sm5504_cable_names[chip->curr_status.cable_type]);

#if defined(CONFIG_MACH_GRANDNEOVE3G)
	if ( chip->curr_status.cable_type == MUIC_SM5504_CABLE_TYPE_REGULAR_TA ) { // TA attach
		// DM_COM open
		sm5504_reg_write(chip, SM5504_REG_MANUAL_SW1, 0x04);
	    sm5504_reg_write(chip, SM5504_REG_MANUAL_SW2, 0x01);
		sm5504_clr_bits(chip, SM5504_REG_CONTROL, (1 << 2) );
		RTINFO("DCP attach DM_COM open\n");
	} else if (chip->curr_status.cable_type == MUIC_SM5504_CABLE_TYPE_NONE ) { // TA detach
	    sm5504_set_bits(chip, SM5504_REG_CONTROL, (1 << 2) );
		sm5504_reg_write(chip, SM5504_REG_MANUAL_SW1, 0x00);
	    sm5504_reg_write(chip, SM5504_REG_MANUAL_SW2, 0x01);	
		RTINFO("DCP detach \n");
	}
#endif

#if defined(CONFIG_BATTERY_SAMSUNG) || defined(CONFIG_RT_BATTERY)
	if((chip->curr_status.cable_type == MUIC_SM5504_CABLE_TYPE_OTG) &&  (chip->curr_status.vbus_status== 1))
		cable_change_callback(MUIC_SM5504_CABLE_TYPE_OTG_WITH_VBUS);
	else
    cable_change_callback(chip->curr_status.cable_type);
#endif
	if(!chip->curr_status.ovp_status){
		if( chip->curr_status.cable_type ==MUIC_SM5504_CABLE_TYPE_L_USB)
		{
			if (chip->pdata->usb_callback)
				chip->pdata->usb_callback(1);
		}
		else if (chip->pdata->cable_chg_callback)
			chip->pdata->cable_chg_callback(chip->curr_status.cable_type);
	}
#if defined(CONFIG_TOUCHSCREEN_IST30XXA) || defined(CONFIG_TOUCHSCREEN_IST30XXB) || defined(CONFIG_TOUCHSCREEN_BAFFINE_LITE_VE_IST3038)
	if(strcmp(sm5504_cable_names[chip->curr_status.cable_type], "MUIC_SM5504_CABLE_TYPE_REGULAR_TA") == 0 || 
		strcmp(sm5504_cable_names[chip->curr_status.cable_type], "MUIC_SM5504_CABLE_TYPE_USB") == 0 )
	{
		flag = 1;
		ist30xx_set_ta_mode(flag);
	}
	if(strcmp(sm5504_cable_names[chip->curr_status.cable_type], "MUIC_SM5504_CABLE_TYPE_NONE") == 0 && flag == 1)
	{
		flag = 0;
		ist30xx_set_ta_mode(flag);
	}
	printk("[TSP] Flag: %d", flag);
#endif

}

static uint8_t get_usb_type(int cable_type)
{
	uint8_t ret;
	switch (cable_type) {
	case MUIC_SM5504_CABLE_TYPE_CDP:
		ret = 2;
		break;
	default:
		ret = 1;
		break;
	}
	return ret;
}

static void sm5504_otg_attach_handler(struct sm5504_chip *chip,
				      const struct sm5504_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	struct otg_notify *o_notify = get_otg_notify();
	RTINFO("OTG attached\n");
	sm5504_reg_write(chip, SM5504_REG_MANUAL_SW1, 0x24);
	/* Disable USBCHDEN and AutoConfig*/
	sm5504_clr_bits(chip, SM5504_REG_CONTROL, (1 << 2) | (1 << 6));
	if (chip->pdata->otg_callback)
		chip->pdata->otg_callback(1);
	send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
}

static void sm5504_otg_detach_handler(struct sm5504_chip *chip,
				      const struct sm5504_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	struct otg_notify *o_notify = get_otg_notify();
	RTINFO("OTG detached\n");
	sm5504_reg_write(chip, SM5504_REG_MANUAL_SW1, 0x00);
	/* Enable USBCHDEN and AutoConfig*/
	sm5504_set_bits(chip, SM5504_REG_CONTROL, (1 << 2) | (1 << 6));
	if (chip->pdata->otg_callback)
		chip->pdata->otg_callback(0);
	send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 0);
}

static void sm5504_usb_attach_handler(struct sm5504_chip *chip,
				      const struct sm5504_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	uint8_t usb_type;
	RTINFO("USB attached\n");
	usb_type = get_usb_type(chip->curr_status.cable_type);
#ifdef CONFIG_USB_INTERRUPT_BY_MUIC
	if (chip->is_usb_ready )
	{
		pr_info("usb: [%s] startup usb by muic\n", __func__);
		epmic_event_handler(1);
	}
	if (chip->pdata->usb_callback && chip->is_usb_ready)
#else
	if (chip->pdata->usb_callback)
#endif
		chip->pdata->usb_callback(usb_type);
}

static void sm5504_usb_detach_handler(struct sm5504_chip *chip,
				      const struct sm5504_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	RTINFO("USB detached\n");

	epmic_event_handler(0);

	if (chip->pdata->usb_callback)
		chip->pdata->usb_callback(0);
}

static void sm5504_uart_attach_handler(struct sm5504_chip *chip,
				       const struct sm5504_event_handler
				       *handler, unsigned int old_status,
				       unsigned int new_status)
{
#if defined(CONFIG_MUIC_SUPPORT_RUSTPROOF) && !defined(CONFIG_MUIC_SUPPORT_FACTORY) && !defined(CONFIG_MUIC_SUPPORT_RUSTPROOF_INBATT) 
    RTINFO("UART attached\n");

#if defined(CONFIG_MACH_YOUNG23GDTV)
    if (sec_vf_adc_current_check() && !enable_muic_uart)
#else
    if (sec_vf_adc_check() && !enable_muic_uart)
#endif		
	{
		sm5504_reg_write(chip, SM5504_REG_MANUAL_SW1, 0x00);
        	sm5504_clr_bits(chip, SM5504_REG_CONTROL, 1 << 2);
		is_close_uart_path = true;
        RTINFO("SM5504 disable UART PATH enable_muic_uart : %d \n", enable_muic_uart);
    } else {
		if (chip->pdata->uart_callback)
			chip->pdata->uart_callback(1);    
        RTINFO("SM5504 enable UART PATH  enable_muic_uart : %d \n", enable_muic_uart);
    }
#else
	RTINFO("UART attached\n");

	if (chip->pdata->uart_callback)
		chip->pdata->uart_callback(1);
#endif
}


static void sm5504_uart_detach_handler(struct sm5504_chip *chip,
				       const struct sm5504_event_handler
				       *handler, unsigned int old_status,
				       unsigned int new_status)
{
	RTINFO("UART detached\n");
#if defined(CONFIG_MUIC_SUPPORT_RUSTPROOF) && !defined(CONFIG_MUIC_SUPPORT_FACTORY) && !defined(CONFIG_MUIC_SUPPORT_RUSTPROOF_INBATT) 
	if(is_close_uart_path)
	{
	        sm5504_reg_write(chip, SM5504_REG_MANUAL_SW1, 0x00);
        	sm5504_set_bits(chip, SM5504_REG_CONTROL, 1 << 2);
		is_close_uart_path = false;
		RTINFO("Open UART PATH by detaching\n");
	} else {
		if (chip->pdata->uart_callback)
			chip->pdata->uart_callback(0);
	}
#else
	if (chip->pdata->uart_callback)
		chip->pdata->uart_callback(0);
#endif
}

#if defined(CONFIG_MUIC_SUPPORT_RUSTPROOF)
static ssize_t uart_sel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	RTINFO("SM5504 uart_sel_show\n");
	return sprintf(buf, "%d\n", is_close_uart_path);

}

static int __init mode_get_rustproof(char *str)
{
	if (strcmp(str, "9") == 0)
		enable_muic_uart = 1;
	else
		enable_muic_uart = 0;

	return 1;
}

__setup("muic_rustproof=", mode_get_rustproof);

static DEVICE_ATTR(uart_sel, S_IRUGO | S_IWUSR ,
				uart_sel_show, NULL);
#endif

static inline jig_type_t get_jig_type(int cable_type)
{
	jig_type_t type = JIG_USB_BOOT_ON;
	switch (cable_type) {
	case MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS:
	case MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF:
		type = JIG_UART_BOOT_OFF;
		break;
	case MUIC_SM5504_CABLE_TYPE_JIG_UART_ON_WITH_VBUS:
	case MUIC_SM5504_CABLE_TYPE_JIG_UART_ON:
		type = JIG_UART_BOOT_ON;
		break;
	case MUIC_SM5504_CABLE_TYPE_JIG_USB_OFF:
		type = JIG_USB_BOOT_OFF;
		break;
	case MUIC_SM5504_CABLE_TYPE_JIG_USB_ON:
		type = JIG_USB_BOOT_ON;
		break;
	}
	return type;
}

#ifdef CONFIG_MUIC_FACTORY_EVENT
void muic_factory_cb(int type)
{
        pr_info("%s:%s= MUIC dock type=%d\n", "sec-switch.c", __func__, type);
        switch_set_state(&switch_dock, type);
}

void muic_init_factory_cb(void)
{
        int ret;
        /* for CarDock, DeskDock */
        ret = switch_dev_register(&switch_dock);
        pr_info("MUIC ret=%d\n", ret);

        if (ret < 0)
                pr_err("Failed to register dock switch. %d\n", ret);
}
#endif

static void sm5504_jig_attach_handler(struct sm5504_chip *chip,
				      const struct sm5504_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	jig_type_t type;
	type = get_jig_type(chip->curr_status.cable_type);
        #ifdef CONFIG_MUIC_FACTORY_EVENT
        if(is_factory_start)
        {
                muic_factory_cb(1);
        }
        #endif
	RTINFO("JIG attached (type = %d)\n", (int)type);
	if (chip->pdata->jig_callback)
		chip->pdata->jig_callback(type, 1);
}

static void sm5504_jig_detach_handler(struct sm5504_chip *chip,
				      const struct sm5504_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	jig_type_t type;
	type = get_jig_type(chip->prev_status.cable_type);
	RTINFO("JIG detached (type = %d)\n", (int)type);
	if (chip->pdata->jig_callback)
		chip->pdata->jig_callback(type, 0);
}

static void sm5504_l_usb_attach_handler(struct sm5504_chip *chip,
				      const struct sm5504_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	RTINFO("200k special USB cable attached\n");
	/* Make switch connect to USB path */
	sm5504_reg_write(chip, SM5504_REG_MANUAL_SW1, 0x24);
	/* Change to manual-config */
	sm5504_clr_bits(chip, SM5504_REG_CONTROL, 1 << 2);
}

static void sm5504_l_usb_detach_handler(struct sm5504_chip *chip,
				      const struct sm5504_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	RTINFO("200k special USB cable detached\n");
	/* Make switch opened */
	sm5504_reg_write(chip, SM5504_REG_MANUAL_SW1, 0x00);
	/* Change to auto-config */
	sm5504_set_bits(chip, SM5504_REG_CONTROL, 1 << 2);
}


struct sm5504_event_handler normal_event_handlers[] = {
    {
        .name = "200k special USB attached",
        .bit_mask = (1 << L_USB_SHIFT),
        .type = FLAG_RISING,
        .handler = sm5504_l_usb_attach_handler,
    },
    {
        .name = "200k special USB detached",
        .bit_mask = (1 << L_USB_SHIFT),
        .type = FLAG_FALLING,
        .handler = sm5504_l_usb_detach_handler,
    },
	{
	 .name = "Cable changed",
	 .bit_mask = (1 << CABLE_CHG_SHIFT),
	 .type = FLAG_HIGH,
	 .handler = sm5504_cable_change_handler,
	 },
	{
	 .name = "OTG attached",
	 .bit_mask = (1 << OTG_SHIFT),
	 .type = FLAG_RISING,
	 .handler = sm5504_otg_attach_handler,
	 },
	{
	 .name = "OTG detached",
	 .bit_mask = (1 << OTG_SHIFT),
	 .type = FLAG_FALLING,
	 .handler = sm5504_otg_detach_handler,
	 },
	{
	 .name = "USB attached",
	 .bit_mask = (1 << USB_SHIFT),
	 .type = FLAG_RISING,
	 .handler = sm5504_usb_attach_handler,
	 },
	{
	 .name = "USB detached",
	 .bit_mask = (1 << USB_SHIFT),
	 .type = FLAG_FALLING,
	 .handler = sm5504_usb_detach_handler,
	 },
	{
	 .name = "UART attached",
	 .bit_mask = (1 << UART_SHIFT),
	 .type = FLAG_RISING,
	 .handler = sm5504_uart_attach_handler,
	 },
	{
	 .name = "UART detached",
	 .bit_mask = (1 << UART_SHIFT),
	 .type = FLAG_FALLING,
	 .handler = sm5504_uart_detach_handler,
	 },
	{
	 .name = "JIG attached",
	 .bit_mask = (1 << JIG_SHIFT),
	 .type = FLAG_RISING,
	 .handler = sm5504_jig_attach_handler,
	 },
	{
	 .name = "JIG detached",
	 .bit_mask = (1 << JIG_SHIFT),
	 .type = FLAG_FALLING,
	 .handler = sm5504_jig_detach_handler,
	 },
};

static void sm5504_process_urgent_evt(sm5504_chip_t *chip)
{
	unsigned int i;
	unsigned int type, sta_chk;
	uint32_t prev_status, curr_status;
	prev_status = chip->prev_status.status;
	curr_status = chip->curr_status.status;
	for (i = 0; i < ARRAY_SIZE(urgent_event_handlers); i++) {
		sta_chk = state_check(prev_status,
				      curr_status,
				      urgent_event_handlers[i].bit_mask);
		type = urgent_event_handlers[i].type;
		if (type & sta_chk) {

			if (urgent_event_handlers[i].handler)
				urgent_event_handlers[i].handler(chip,
						urgent_event_handlers + i,
						prev_status,
						curr_status);
		}
	}
}

#ifdef CONFIG_USB_INTERRUPT_BY_MUIC
static void sm5504_process_usb_evt(sm5504_chip_t *chip)
{
	unsigned int i;
	unsigned int type, sta_chk;
	uint32_t prev_status, curr_status;	
	
	prev_status = chip->prev_status.status;
	curr_status = chip->curr_status.status;

	for (i = 0; i < ARRAY_SIZE(normal_event_handlers); i++) {
		sta_chk = state_check(prev_status,
				      curr_status,
				      normal_event_handlers[i].bit_mask);
		type = normal_event_handlers[i].type;		
		
		if (type & sta_chk) {
			if (normal_event_handlers[i].handler &&
				(!strcmp(normal_event_handlers[i].name, "USB attached")))
				normal_event_handlers[i].handler(chip,
						normal_event_handlers + i,
						prev_status,
						curr_status);
		}
	}
}
#endif

static void sm5504_process_normal_evt(sm5504_chip_t *chip)
{
	unsigned int i;
	unsigned int type, sta_chk;
	uint32_t prev_status, curr_status;

	prev_status = chip->prev_status.status;
	curr_status = chip->curr_status.status;

	for (i = 0; i < ARRAY_SIZE(normal_event_handlers); i++) {
		sta_chk = state_check(prev_status,
				      curr_status,
				      normal_event_handlers[i].bit_mask);
		type = normal_event_handlers[i].type;

		if (type & sta_chk) {
			if (normal_event_handlers[i].handler)
				normal_event_handlers[i].handler(chip,
						normal_event_handlers + i,
						prev_status,
						curr_status);
		}
	}
}

static void sm5504_irq_work(sm5504_chip_t *chip)
{
#ifdef CONFIG_USB_NOTIFY_LAYER
	struct otg_notify *notify = get_otg_notify();
#endif
	int ret;

	chip->prev_status = chip->curr_status;
	ret = sm5504_update_regs(chip);
	if (ret < 0) {
		RTERR("Error : can't update(read) register status:%d\n", ret);
	        pr_info("%s line %d : INTF1 = 0x%x, INTF2 = 0x%x\n", __FUNCTION__,__LINE__,
         	(int)chip->curr_status.irq_flags[0],
         	(int)chip->curr_status.irq_flags[1]);
        	pr_info("%s line %d : DEV1 = 0x%x, DEV2 = 0x%x\n", __FUNCTION__,__LINE__,
         	(int)chip->curr_status.device_reg[0],
         	(int)chip->curr_status.device_reg[1]);
        	pr_info("%s line %d : ADC = 0x%x\n", __FUNCTION__,__LINE__,
         	(int)chip->curr_status.id_adc);
		ret = sm5504_update_regs(chip);
		if(ret < 0)
		{	
			 RTERR("Retry Error : can't update(read) register status:%d\n", ret);
			/* roll back status */
			chip->curr_status = chip->prev_status;
			return;
		}
	}
/* for printing out registers -- start */
	pr_info("%s : INTF1 = 0x%x, INTF2 = 0x%x\n", __FUNCTION__,
         (int)chip->curr_status.irq_flags[0],
         (int)chip->curr_status.irq_flags[1]);
	pr_info("%s : DEV1 = 0x%x, DEV2 = 0x%x\n", __FUNCTION__,
         (int)chip->curr_status.device_reg[0],
         (int)chip->curr_status.device_reg[1]);
	pr_info("%s : ADC = 0x%x\n", __FUNCTION__,
         (int)chip->curr_status.id_adc);
/* for printint out registers -- end*/

	if ( (chip->curr_status.id_adc == 0x17)
		&& ( (chip->curr_status.irq_flags[0] & 0x01) == 0x01 )
		&& ( (chip->curr_status.irq_flags[1] & 0x01) == 0x00 ) )
    	{ 
        	// BCD scan start
		sm5504_clr_bits(chip, SM5504_REG_CONTROL, 0x40);
		msleep_interruptible(1);
		sm5504_set_bits(chip, SM5504_REG_CONTROL, 0x40);        
		return;
	}
	sm5504_preprocess_status(chip);
	RTINFO("Status : cable type = %d,\n"
	       "vbus = %d, accessory = %d\n"
	       "ocp = %d, ovp = %d, otp = %d,\n"
	       "adc_chg = %d, cable_chg = %d\n"
	       "otg = %d, dcdt = %d, usb = %d,\n"
	       "uart = %d, jig = %d\n"
	       "200k usb cable = %d\n",
	       chip->curr_status.cable_type,
	       chip->curr_status.vbus_status,
	       chip->curr_status.accessory_status,
	       chip->curr_status.ocp_status,
	       chip->curr_status.ovp_status,
	       chip->curr_status.otp_status,
	       chip->curr_status.adc_chg_status,
	       chip->curr_status.cable_chg_status,
	       chip->curr_status.otg_status,
	       chip->curr_status.dcdt_status,
	       chip->curr_status.usb_connect,
	       chip->curr_status.uart_connect,
	       chip->curr_status.jig_connect,
	       chip->curr_status.l_usb_connect);

	sm5504_process_urgent_evt(chip);

	if((chip->curr_status.ovp_status != chip->prev_status.ovp_status)
		&& chip->curr_status.vbus_status && chip->curr_status.uart_connect
		&& chip->curr_status.jig_connect)
	{
		RTINFO("Recovery OVP for JIG UART with VBUS cable\n");
		chip->curr_status.cable_chg_status = 1;
	}

	if((chip->curr_status.vbus_status != chip->prev_status.vbus_status )
		&& chip->curr_status.vbus_status /*&& chip->pdata->vbus_callback*/)
		{
			RTINFO("SM5504 vbus callback set true \n");
#ifdef CONFIG_USB_NOTIFY_LAYER
			send_otg_notify(notify, NOTIFY_EVENT_VBUSPOWER, 1);
#endif
		//	chip->pdata->vbus_callback(1);
		}
	else if((chip->curr_status.vbus_status != chip->prev_status.vbus_status )
		&& !chip->curr_status.vbus_status /*&& chip->pdata->vbus_callback*/)
		{
			RTINFO("SM5504 vbus callback set false \n");
#ifdef CONFIG_USB_NOTIFY_LAYER
			send_otg_notify(notify, NOTIFY_EVENT_VBUSPOWER, 0);
#endif
		//	chip->pdata->vbus_callback(0);
		}
	else
		RTINFO("no vbus \n");

	sm5504_process_normal_evt(chip);
}

static irqreturn_t sm5504_irq_handler(int irq, void *data)
{
	struct sm5504_chip *chip = data;
	wake_lock_timeout(&(chip->muic_wake_lock), msecs_to_jiffies(500));
	RTINFO("SM5504 interrupt triggered!\n");
	msleep(300);
	sm5504_irq_work(chip);
	return IRQ_HANDLED;
}

static const struct i2c_device_id sm5504_id_table[] = {
	{"sm5504", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, sm5504_id_table);

static bool sm5504_reset_check(struct i2c_client *iic)
{
	int ret;
	ret = i2c_smbus_read_byte_data(iic, SM5504_REG_CHIP_ID);
	if (ret < 0) {
		RTERR("Error : can't read device ID from IC(%d)\n", ret);
		return false;
	}
	if ((ret & 0x07) != 0x01) {
	    RTERR("Error : vendor ID mismatch (0x%d)!\n", ret);
	    return false;
	}

    /* write default value instead of sending reset command*/
    /* REG[0x02] = 0xE5, REG[0x14] = 0x01*/
	i2c_smbus_write_byte_data(iic, SM5504_REG_CONTROL, 0xE5);
	i2c_smbus_write_byte_data(iic, SM5504_REG_MANUAL_SW2, 0x01);
	return true;
}


#define BATTERY_PSY_NAME "battery"
#define CHARGER_PSY_NAME "sec-charger"
#define POLL_DELAY  50

#ifdef CONFIG_USB_INTERRUPT_BY_MUIC
static void sm5504_muic_usb_detect(struct work_struct *work)
{
	struct sm5504_chip *chip =
		container_of(work, struct sm5504_chip, usb_work.work);

//	mutex_lock(&chip->io_lock);
	printk("usb: %s chip->prev_status = 0x%x, chip->curr_status=0x%x \r\n", __func__,
							chip->prev_status.status, chip->curr_status.status);
	chip->is_usb_ready = true;
	chip->prev_status.status = 0;
	sm5504_process_usb_evt(chip);
//	mutex_unlock(&chip->io_lock);
}
#endif

static void sm5504_init_work(struct work_struct *work)
{
	int ret;
	sm5504_chip_t *chip = container_of(to_delayed_work(work),
					   sm5504_chip_t, init_work);

#ifdef CHECK_PSY_READY
    /* make sure that Samsung battery and battery are ready to receive notification*/
    struct power_supply *psy_battery = NULL;
    struct power_supply *psy_charger = NULL;
    do {
        psy_battery = power_supply_get_by_name(BATTERY_PSY_NAME);
        psy_charger = power_supply_get_by_name(CHARGER_PSY_NAME);
        if (psy_battery == NULL || psy_charger == NULL) {
            dev_info(&chip->iic->dev, "SM5504 : Battery and charger are not ready\r\n");
            msleep(POLL_DELAY);
        }
    } while (psy_battery == NULL || psy_charger == NULL);
    dev_info(&chip->iic->dev, "SM5504 : Battery and charger are ready\r\n");
#endif
	/* Dummy read */
	sm5504_reg_read(chip, SM5504_REG_INT_FLAG1);
	sm5504_clr_bits(chip, SM5504_REG_CONTROL, 0x60);
	msleep_interruptible(1);
	sm5504_set_bits(chip, SM5504_REG_CONTROL, 0x60);
	/* enable interrupt */
	ret = sm5504_clr_bits(chip, SM5504_REG_CONTROL, 0x01);
	if (ret < 0) {
	    dev_err(&chip->iic->dev, "can't enable sm5504's INT (%d)\r\n",ret);
	}
	sm5504_irq_work(chip);
}

static void sm5504_init_regs(sm5504_chip_t *chip)
{
	/* initialize with MUIC_SM5504_CABLE_TYPE_INVALID
	 * to make 1st detection work always report cable type*/
	chip->curr_status.cable_type = MUIC_SM5504_CABLE_TYPE_INVALID;
	chip->curr_status.id_adc = 0x1f;
	chip->adc_reg_addr = SM5504_REG_ADC;

	/* Only mask Connect */
	sm5504_reg_write(chip, SM5504_REG_INTERRUPT_MASK1, 0x20);
	/* Only mask OCP_LATCH and POR */
	sm5504_reg_write(chip, SM5504_REG_INTERRUPT_MASK2, 0x24);
	sm5504_reg_write(chip, 0x20,0x06);
	queue_delayed_work(chip->wq, &chip->init_work, 0);
}
#ifdef CONFIG_MUIC_FACTORY_EVENT
static ssize_t sm5504_muic_show_apo_factory(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
        const char *mode;

        /* true: Factory mode, false: not Factory mode */
        if (is_factory_start)
                mode = "FACTORY_MODE";
        else
                mode = "NOT_FACTORY_MODE";

        return sprintf(buf, "%s\n", mode);
}

static ssize_t sm5504_muic_set_apo_factory(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count)
{
        const char *mode;

        pr_info("%s:%s buf:%s\n", SM5504_DEVICE_NAME, __func__, buf);

        /* "FACTORY_START": factory mode */
        if (!strncmp(buf, "FACTORY_START", 13)) {
                is_factory_start = true;
                mode = "FACTORY_MODE";
        } else {
                pr_warn("%s:%s Wrong command\n", SM5504_DEVICE_NAME, __func__);
                return count;
        }

        pr_info("%s:%s apo factory=%s\n", SM5504_DEVICE_NAME, __func__, mode);

        return count;
}

static DEVICE_ATTR(apo_factory, 0666,
                sm5504_muic_show_apo_factory,
                sm5504_muic_set_apo_factory);

#endif /* CONFIG_MUIC_FACTORY_EVENT */

//#ifdef CONFIG_MUIC_SUPPORT_FACTORY
static ssize_t adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u8 adc_value[] = "1c";
	u8 adc_value_2[] = "1d";	
	u8 adc_value_3[] = "18";
	u8 adc_value_4[] = "19";	
	u8 adc_fail = 0;

	if (current_status->cable_type == MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF) {
		RTINFO("adc_show JIG UART BOOT OFF\n");
		return sprintf(buf, "%s\n", adc_value);
	}
	else if (current_status->cable_type == MUIC_SM5504_CABLE_TYPE_JIG_UART_ON) {
		RTINFO("adc_show JIG UART BOOT ON\n");
		return sprintf(buf, "%s\n", adc_value_2);
	}
	else if (current_status->cable_type == MUIC_SM5504_CABLE_TYPE_JIG_USB_OFF) {
		RTINFO("adc_show JIG USB BOOT OFF\n");
		return sprintf(buf, "%s\n", adc_value_3);
	}
	else if (current_status->cable_type == MUIC_SM5504_CABLE_TYPE_JIG_USB_ON) {
		RTINFO("adc_show JIG USB BOOT ON\n");
		return sprintf(buf, "%s\n", adc_value_4);
	}
	else {
		RTINFO("adc_show no detect\n");
		return sprintf(buf, "%d\n", adc_fail);
	}
}
static ssize_t dev_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u8 dev_value[] = "JIG UART OFF";
	u8 dev_value_2[] = "JIG UART ON";	
	u8 dev_value_3[] = "JIG USB OFF";
	u8 dev_value_4[] = "JIG USB ON";	
	u8 dev_fail = 0;

	if (current_status->cable_type == MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF) {
		RTINFO("dev_show JIG UART BOOT OFF\n");
		return sprintf(buf, "%s\n", dev_value);
	}
	else if (current_status->cable_type == MUIC_SM5504_CABLE_TYPE_JIG_UART_ON) {
		RTINFO("dev_show JIG UART BOOT ON\n");
		return sprintf(buf, "%s\n", dev_value_2);
	}
	else if (current_status->cable_type == MUIC_SM5504_CABLE_TYPE_JIG_USB_OFF) {
		RTINFO("dev_show JIG USB BOOT OFF\n");
		return sprintf(buf, "%s\n", dev_value_3);
	}
	else if (current_status->cable_type == MUIC_SM5504_CABLE_TYPE_JIG_USB_ON) {
		RTINFO("dev_show JIG USB BOOT ON\n");
		return sprintf(buf, "%s\n", dev_value_4);
	}
	else {
		RTINFO("dev_show no detect\n");
		return sprintf(buf, "%d\n", dev_fail);
	}
}

static ssize_t usb_state_show_attrs(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	if (current_status->usb_connect)
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	else
		return sprintf(buf, "USB_STATE_NOTCONFIGURED\n");
}

static ssize_t usb_sel_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "PDA");
}

static DEVICE_ATTR(attached_dev, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH /*0665 */ ,
		   dev_show, NULL);
static DEVICE_ATTR(adc, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH /*0665 */ ,
		   adc_show, NULL);
static DEVICE_ATTR(usb_state, S_IRUGO, usb_state_show_attrs, NULL);
static DEVICE_ATTR(usb_sel, S_IRUGO, usb_sel_show_attrs, NULL);

static struct attribute *sm5504_muic_attributes[] = {
        &dev_attr_usb_state.attr,
        &dev_attr_usb_sel.attr,
        &dev_attr_adc.attr,
        &dev_attr_attached_dev.attr,
#ifdef CONFIG_MUIC_FACTORY_EVENT
        &dev_attr_apo_factory.attr,
#endif
#ifdef CONFIG_MUIC_SUPPORT_RUSTPROOF
        &dev_attr_uart_sel.attr,
#endif

        NULL
};
static const struct attribute_group sm5504_muic_group = {
        .attrs = sm5504_muic_attributes,
};
static int sec_get_usb_vbus(unsigned int *level)
{
	if (current_status->vbus_status) {
		RTINFO("set VBUS_HIGH\n");
		*level = VBUS_HIGH;
	} else {
		RTINFO("set VBUS_LOW\n");
		*level = VBUS_LOW;
	}
	return 0;
}
//#endif

static int sm5504_parse_dt(struct device *dev,
                           struct sm5504_platform_data *pdata)
{
    struct device_node *np = dev->of_node;
    enum of_gpio_flags irq_gpio_flags;

    pdata->irq_gpio = of_get_named_gpio_flags(np, "sm5504,irq-gpio",
                0, &irq_gpio_flags);
    return 0;
}


static int sm5504_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct sm5504_platform_data *pdata;
	struct sm5504_chip *chip;
//#ifdef CONFIG_MUIC_SUPPORT_FACTORY
	struct device *switch_dev;
//#endif
	int ret;
	RTINFO("SiliconMitus SM5504 driver %s probing...\n", SM5504_DRV_VER);
	if(client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory \n");
			ret = -ENOMEM;
			goto err_nomem;
		}
		ret = sm5504_parse_dt(&client->dev, pdata);
		if (ret < 0)
            goto err_parse_dt;
	} else
        pdata = client->dev.platform_data;

	ret = i2c_check_functionality(client->adapter,
				      I2C_FUNC_SMBUS_BYTE_DATA |
				      I2C_FUNC_SMBUS_I2C_BLOCK);
	if (ret < 0) {
		RTERR("Error : i2c functionality check failed\n");
		goto err_i2c_func;
	}
	if (!sm5504_reset_check(client)) {
		ret = -ENODEV;
		goto err_reset_sm5504_fail;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (chip == NULL) {
		ret = -ENOMEM;
		goto err_nomem;
	}
//#ifdef CONFIG_MUIC_SUPPORT_FACTORY
	current_status = &chip->curr_status;
//#endif
	wake_lock_init(&chip->muic_wake_lock, WAKE_LOCK_SUSPEND,
		       "sm5504_wakelock");
	ret = gpio_request(pdata->irq_gpio, "SM5504_EINT");
	RTWARN_IF(ret < 0,
		  "Warning : failed to request GPIO%d (retval = %d)\n",
		  pdata->irq_gpio, ret);
	ret = gpio_direction_input(pdata->irq_gpio);
	RTWARN_IF(ret < 0,
		  "Warning : failed to set GPIO%d as input pin(retval = %d)\n",
		  pdata->irq_gpio, ret);
	chip->iic = client;
	chip->dev = &client->dev;
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	chip->wq = create_workqueue("sm5504_workqueue");
	INIT_DELAYED_WORK(&chip->init_work, sm5504_init_work);
	mutex_init(&chip->io_lock);
	chip->irq = gpio_to_irq(pdata->irq_gpio);
	client->irq = chip->irq;
	RTINFO("Request IRQ %d(GPIO %d)...\n",
	       chip->irq, pdata->irq_gpio);
	ret = request_threaded_irq(chip->irq, NULL,
		sm5504_irq_handler, SM5504_IRQF_MODE |
		IRQF_NO_SUSPEND | IRQF_ONESHOT,
                   SM5504_DEVICE_NAME, chip);
	if (ret < 0) {
		RTERR
		    ("Error : failed to request irq %d (gpio=%d, retval=%d)\n",
		     chip->irq, pdata->irq_gpio, ret);
		goto err_request_irq_fail;
	}
//#ifdef CONFIG_MUIC_SUPPORT_FACTORY
	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");
	chip->switch_dev = switch_dev;
	ret = sysfs_create_group(&switch_dev->kobj, &sm5504_muic_group);
//#endif

	sm5504_init_regs(chip);
#ifdef CONFIG_MUIC_FACTORY_EVENT
	muic_init_factory_cb();
#endif

#ifdef CONFIG_USB_INTERRUPT_BY_MUIC
/* usb cable notification delay */
	chip->is_usb_ready = false;
	INIT_DELAYED_WORK(&chip->usb_work, sm5504_muic_usb_detect);
	schedule_delayed_work(&chip->usb_work, msecs_to_jiffies(13000));
#endif
	
	pr_info("SM5504 : SiliconMitus SM5504 MUIC driver %s initialize successfully\n", SM5504_DRV_VER);
	return 0;
err_request_irq_fail:
	mutex_destroy(&chip->io_lock);
	destroy_workqueue(chip->wq);
	gpio_free(pdata->irq_gpio);
	wake_lock_destroy(&chip->muic_wake_lock);
	kfree(chip);
err_nomem:
err_reset_sm5504_fail:
err_i2c_func:
err_parse_dt:
	return ret;
}

static int __exit sm5504_remove(struct i2c_client *client)
{
	struct sm5504_chip *chip;
	RTINFO("SM5504 driver removing...\n");
	chip = i2c_get_clientdata(client);
	if (chip) {
	   free_irq(chip->irq, chip);
		gpio_free(chip->pdata->irq_gpio);
		mutex_destroy(&chip->io_lock);
		if (chip->wq)
			destroy_workqueue(chip->wq);
		wake_lock_destroy(&chip->muic_wake_lock);
//#ifdef CONFIG_MUIC_SUPPORT_FACTORY
       	sysfs_remove_group(&chip->switch_dev->kobj, &sm5504_muic_group);
//#endif
		kfree(chip);
	}
	return 0;

}

static void sm5504_shutdown(struct i2c_client *iic)
{
	struct sm5504_chip *chip;
	chip = i2c_get_clientdata(iic);
	disable_irq(iic->irq);
	/*cancel_delayed_work_sync(&chip->irq_work);*/
	pr_info("SM5504 : Shutdown : set sm5504 regs to default setting...\n");
	i2c_smbus_write_byte_data(iic, SM5504_REG_CONTROL, 0xE5);
	i2c_smbus_write_byte_data(iic, SM5504_REG_MANUAL_SW2, 0x01);
}

#ifdef CONFIG_OF
static struct of_device_id sm5504_match_table[] = {
	{ .compatible = "SiliconMitus,sm5504",},
	{},
};
#else
#define sm5504_match_table NULL
#endif

#if defined(CONFIG_MUIC_SUPPORT_RUSTPROOF) && defined(CONFIG_MUIC_SUPPORT_RUSTPROOF_INBATT) && !defined(CONFIG_MUIC_SUPPORT_FACTORY)
static int sm5504_suspend(struct device *dev)
{

	struct sm5504_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);
	chip = i2c_get_clientdata(client);

	RTINFO("SM5504 curr_status.uart_connect : %d \n", chip->curr_status.uart_connect);

	if(chip->curr_status.uart_connect== 1)
	{
		sm5504_reg_write(chip, SM5504_REG_MANUAL_SW1, 0x00);
		sm5504_clr_bits(chip, SM5504_REG_CONTROL, 1 << 2);
		is_close_uart_path = true;
		RTINFO("SM5504 disable UART PATH in sleep mode enable_muic_uart : %d \n", enable_muic_uart);
	}
	return 0;
}

static int sm5504_resume(struct device *dev)
{
	struct sm5504_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);
	chip = i2c_get_clientdata(client);

	if(is_close_uart_path == 1)
	{
		sm5504_reg_write(chip, SM5504_REG_MANUAL_SW1, 0x00);
		sm5504_set_bits(chip, SM5504_REG_CONTROL, 1 << 2);
		is_close_uart_path = false;
		RTINFO(" SM5504 enable UART PATH  in wake up mode \n");
	}
	return 0;
}

static const struct dev_pm_ops sm5504_pm_ops = {
	.suspend = sm5504_suspend,
	.resume = sm5504_resume,
};
#endif

static struct i2c_driver sm5504_driver = {
	.driver = {
		   .name = SM5504_DEVICE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = sm5504_match_table,
		   #if defined(CONFIG_MUIC_SUPPORT_RUSTPROOF) && defined(CONFIG_MUIC_SUPPORT_RUSTPROOF_INBATT) && !defined(CONFIG_MUIC_SUPPORT_FACTORY)
		   .pm = &sm5504_pm_ops,
		   #endif
		   },
	.probe = sm5504_probe,
	.remove = sm5504_remove,
	.shutdown = sm5504_shutdown,
	.id_table = sm5504_id_table,
};

static int __init sm5504_i2c_init(void)
{
	int ret;
	ret = i2c_add_driver(&sm5504_driver);
	if (ret != 0)
		pr_err("Failed to register SM5504 I2C driver: %d\n", ret);
	return ret;
}

late_initcall(sm5504_i2c_init);

static void __exit sm5504_i2c_exit(void)
{
	i2c_del_driver(&sm5504_driver);
}

module_exit(sm5504_i2c_exit);

MODULE_DESCRIPTION("SiliconMitus SM5504 MUIC Driver");
MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com>");
MODULE_VERSION(SM5504_DRV_VER);
MODULE_LICENSE("GPL");
