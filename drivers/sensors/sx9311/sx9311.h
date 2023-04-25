/*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*/
#ifndef SX9311_H
#define SX9311_H

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#if 0
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif

/*
*  I2C Registers
*/
#define SX9311_IRQSTAT_REG    	0x00
#define SX9311_STAT0_REG    	0x01
#define SX9311_STAT1_REG    	0x02
#define SX9311_IRQ_ENABLE_REG	0x03
#define SX9311_IRQFUNC_REG		0x04
	
#define SX9311_CPS_CTRL0_REG    0x10
#define SX9311_CPS_CTRL1_REG    0x11
#define SX9311_CPS_CTRL2_REG    0x12
#define SX9311_CPS_CTRL3_REG    0x13
#define SX9311_CPS_CTRL4_REG    0x14
#define SX9311_CPS_CTRL5_REG    0x15
#define SX9311_CPS_CTRL6_REG    0x16
#define SX9311_CPS_CTRL7_REG    0x17
#define SX9311_CPS_CTRL8_REG    0x18
#define SX9311_CPS_CTRL9_REG	0x19
#define SX9311_CPS_CTRL10_REG	0x1A
#define SX9311_CPS_CTRL11_REG	0x1B
#define SX9311_CPS_CTRL12_REG	0x1C
#define SX9311_CPS_CTRL13_REG	0x1D
#define SX9311_CPS_CTRL14_REG	0x1E
#define SX9311_CPS_CTRL15_REG	0x1F
#define SX9311_CPS_CTRL16_REG	0x20
#define SX9311_CPS_CTRL17_REG	0x21
#define SX9311_CPS_CTRL18_REG	0x22
#define SX9311_CPS_CTRL19_REG	0x23
#define SX9311_SAR_CTRL0_REG	0x2A
#define SX9311_SAR_CTRL1_REG	0x2B
#define SX9311_SAR_CTRL2_REG	0x2C
/*      Sensor Readback */
#define SX9311_CPSRD          	0x30
#define SX9311_USEMSB         	0x31
#define SX9311_USELSB         	0x32
#define SX9311_AVGMSB         	0x33
#define SX9311_AVGLSB         	0x34
#define SX9311_DIFFMSB        	0x35
#define SX9311_DIFFLSB        	0x36
#define SX9311_OFFSETMSB		0x37
#define SX9311_OFFSETLSB		0x38
#define SX9311_SARMSB			0x39
#define SX9311_SARLSB			0x3A

#define SX9311_WHOAMI_REG		0x42
#define SX9311_SOFTRESET_REG  	0x7F

/*      SoftReset 	*/
#define SX9311_SOFTRESET  				0xDE
/*      Chip ID 	*/
#define SX9311_WHOAMI_VALUE				0x02

/*      IrqStat 0:Inactive 1:Active     */
#define SX9311_IRQSTAT_RESET_FLAG		0x80
#define SX9311_IRQSTAT_TOUCH_FLAG		0x40
#define SX9311_IRQSTAT_RELEASE_FLAG		0x20
#define SX9311_IRQSTAT_COMPDONE_FLAG	0x10
#define SX9311_IRQSTAT_CONV_FLAG		0x08
#define SX9311_IRQSTAT_PROG2_FLAG		0x04
#define SX9311_IRQSTAT_PROG1_FLAG		0x02
#define SX9311_IRQSTAT_PROG0_FLAG		0x01


/* RegStat0  */
#define SX9311_TCHCMPSTAT_TCHCOMB_FLAG    0x08
#define SX9311_TCHCMPSTAT_TCHSTAT2_FLAG   0x84
#define SX9311_TCHCMPSTAT_TCHSTAT1_FLAG   0x42
#define SX9311_TCHCMPSTAT_TCHSTAT0_FLAG   0x01


/**************************************
*   define platform data
*
**************************************/
struct smtc_reg_data {
	unsigned char reg;
	unsigned char val;
};
typedef struct smtc_reg_data smtc_reg_data_t;
typedef struct smtc_reg_data *psmtc_reg_data_t;


struct _buttonInfo {
	/*! The Key to send to the input */
	int keycode;
	/*! Mask to look for on Touch Status */
	int mask;
	/*! Current state of button. */
	int state;
};

struct totalButtonInformation {
	struct _buttonInfo *buttons;
	int buttonSize;
	struct input_dev *input;
};

typedef struct totalButtonInformation buttonInformation_t;
typedef struct totalButtonInformation *pbuttonInformation_t;

/* Define Registers that need to be initialized to values different than
* default
*/
static struct smtc_reg_data sx9311_i2c_reg_setup[] = {
		{
			.reg = SX9311_IRQ_ENABLE_REG,
			.val = 0xE0,//Set Stat0[7:0] for any interrupt 
		},
		{
			.reg = SX9311_IRQFUNC_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_CPS_CTRL1_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_CPS_CTRL2_REG,
			.val = 0x04,
		},
		{
			.reg = SX9311_CPS_CTRL3_REG,
			.val = 0x0A,
		},
		{
			.reg = SX9311_CPS_CTRL4_REG,
			.val = 0x0D,
		},
		{
			.reg = SX9311_CPS_CTRL5_REG,
			.val = 0xC1,
		},
		{
			.reg = SX9311_CPS_CTRL6_REG,
			.val = 0x20,
		},
		{
			.reg = SX9311_CPS_CTRL7_REG,
			.val = 0x4C,
		},
		{
			.reg = SX9311_CPS_CTRL8_REG,
			.val = 0x7B,
		},
		{
			.reg = SX9311_CPS_CTRL9_REG,
			.val = 0x7B,
		},
		{
			.reg = SX9311_CPS_CTRL10_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_CPS_CTRL11_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_CPS_CTRL12_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_CPS_CTRL13_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_CPS_CTRL14_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_CPS_CTRL15_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_CPS_CTRL16_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_CPS_CTRL17_REG,
			.val = 0x04,
		},
		{
			.reg = SX9311_CPS_CTRL18_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_CPS_CTRL19_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_SAR_CTRL0_REG,
			.val = 0x00,
		},
		{
			.reg = SX9311_SAR_CTRL1_REG,
			.val = 0x80,
		},
		{
			.reg = SX9311_SAR_CTRL2_REG,
			.val = 0x0C,
		},
		{
			.reg = SX9311_CPS_CTRL0_REG,
			.val = 0x27,
		},


};

static struct _buttonInfo psmtcButtons[] = {
	{
	  .keycode = KEY_0,
	  .mask = SX9311_TCHCMPSTAT_TCHSTAT0_FLAG,
	},
	{
	  .keycode = KEY_1,
	  .mask = SX9311_TCHCMPSTAT_TCHSTAT1_FLAG,
	},
	{
	  .keycode = KEY_2,
	  .mask = SX9311_TCHCMPSTAT_TCHSTAT2_FLAG,
	},
	{
	  .keycode = KEY_3,
	  .mask = SX9311_TCHCMPSTAT_TCHCOMB_FLAG,
	},

};

struct sx9311_platform_data {
	int i2c_reg_num;
	struct smtc_reg_data *pi2c_reg;
	struct regulator *vdd;
	struct regulator *vddio;
	int irq_gpio;
	pbuttonInformation_t pbuttonInformation;

	int (*get_is_nirq_low)(void);
  	
	int     (*init_platform_hw)(struct i2c_client *client);
	void    (*exit_platform_hw)(struct i2c_client *client);
};
typedef struct sx9311_platform_data sx9311_platform_data_t;
typedef struct sx9311_platform_data *psx9311_platform_data_t;

/***************************************
*  define data struct/interrupt
*
***************************************/

#define MAX_NUM_STATUS_BITS (8)

typedef struct sx93XX sx93XX_t, *psx93XX_t;
struct sx93XX 
{
	void * bus; /* either i2c_client or spi_client */

	struct device *pdev; /* common device struction for linux */

	void *pDevice; /* device specific struct pointer */

	/* Function Pointers */
	int (*init)(psx93XX_t this); /* (re)initialize device */

	/* since we are trying to avoid knowing registers, create a pointer to a
	* common read register which would be to read what the interrupt source
	* is from 
	*/
	int (*refreshStatus)(psx93XX_t this); /* read register status */

	int (*get_nirq_low)(void); /* get whether nirq is low (platform data) */

	/* array of functions to call for corresponding status bit */
	void (*statusFunc[MAX_NUM_STATUS_BITS])(psx93XX_t this); 
	
	/* Global variable */
	u8 		failStatusCode;	/*Fail status code*/
	bool	reg_in_dts;

	spinlock_t       lock; /* Spin Lock used for nirq worker function */
	int irq; /* irq number used */

	/* whether irq should be ignored.. cases if enable/disable irq is not used
	* or does not work properly */
	char irq_disabled;

	u8 useIrqTimer; /* older models need irq timer for pen up cases */

	int irqTimeout; /* msecs only set if useIrqTimer is true */

	/* struct workqueue_struct *ts_workq;  */  /* if want to use non default */
	struct delayed_work dworker; /* work struct for worker function */
	/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 start*/
	struct wakeup_source sx9311_wakelock;
	/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 end*/
};

static void sx93XX_suspend(psx93XX_t this);
static void sx93XX_resume(psx93XX_t this);
static int sx93XX_IRQ_init(psx93XX_t this);
static int sx93XX_remove(psx93XX_t this);

#endif
