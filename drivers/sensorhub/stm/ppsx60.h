#ifndef _PPSX60_H_
#define _PPSX60_H_

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>

#define PPSX60_REGFILE_PATH "/sdcard/PPSX60_REG.txt"
#define MAX_REG_NUM 64
#define PPSX60_DEBUG

#define PPSX60_SLAVE_ADDR		0x58
#define HRM_LDO_ON	1
#define HRM_LDO_OFF	0
#define I2C_WRITE_ENABLE			0x00
#define I2C_READ_ENABLE			0x01
#define PPS_LIB_VER 20

/*PPSX60 Registers*/
enum {
	AFE_CONTROL0 = 0x0,
	AFE_LED2STC,				/*Sample LED2 start(IR)*/
	AFE_LED2ENDC,				/* Sample LED2 end(IR)*/
	AFE_LED1LEDSTC,			/* LED1 start(GREEN)*/
	AFE_LED1LEDENDC,			/* LED1 end(GREEN)*/
	AFE_LED3STC,				/* Sample Ambient2/Sample LED3 start(RED)*/
	AFE_LED3ENDC,				/* Sample Ambient2/Sample LED3 end(RED)*/
	AFE_LED1STC,				/* Sample LED1 start(GREEN)*/
	AFE_LED1ENDC,				/* Sample LED1 end(GREEN)*/
	AFE_LED2LEDSTC,			/* LED2 start(IR)*/
	AFE_LED2LEDENDC,			/* LED2 end(IR)*/
	AFE_ALED1STC,				/* Sample Ambient1 start(AMBIENT)*/
	AFE_ALED1ENDC,			/* Sample Ambient1 end(AMBIENT)*/
	AFE_LED2CONVST,			/* LED2 convert phase start(IR)*/
	AFE_LED2CONVEND,			/* LED2 convert phase end(IR)*/
	AFE_LED3CONVST,			/* Ambient2/LED3 convert phase start(RED)*/
	AFE_LED3CONVEND = 0x10,	/* Ambient2/LED3 convert phase end(RED)*/
	AFE_LED1CONVST,			/* LED1 convert phase start(GREEN)*/
	AFE_LED1CONVEND,			/* LED1 convert phase end(GREEN)*/
	AFE_ALED1CONVST,			/* Ambient1 convert phase start(AMBIENT)*/
	AFE_ALED1CONVEND,			/* Ambient1 convert phase end(AMBIENT)*/
	AFE_ADCRSTSTCT0,			/* ADC reset phase 0 start*/
	AFE_ADCRSTENDCT0,			/* ADC reset phase 0 end*/
	AFE_ADCRSTSTCT1,			/* ADC reset phase 1 start*/
	AFE_ADCRSTENDCT1,			/* ADC reset phase 1 end*/
	AFE_ADCRSTSTCT2,			/* ADC reset phase 2 start*/
	AFE_ADCRSTENDCT2,			/* ADC reset phase 2 end*/
	AFE_ADCRSTSTCT3,			/* ADC reset phase 3 start*/
	AFE_ADCRSTENDCT3,			/* ADC reset phase 3 end*/
	AFE_PRPCOUNT,
	AFE_CONTROL1,
	AFE_SPARE1,
	AFE_TIAGAIN = 0x20,
	AFE_TIAAMBGAIN,
	AFE_LEDCNTRL,
	AFE_CONTROL2,
	AFE_SPARE2,
	AFE_SPARE3,
	AFE_SPARE4,
	AFE_RESERVED1,
	AFE_RESERVED2,
	AFE_ALARM,
	AFE_LED2VAL,				/* IR_DATA(LED2)*/
	AFE_LED3VAL,				/* Ambient2/RED_DATA(LED3)*/
	AFE_LED1VAL,				/* GREEN_DATA(LED1)*/
	AFE_ALED1VAL,				/* AMBIENT_DATA(NONE)*/
	AFE_LED2ALED2VAL,			/* NOT USE*/
	AFE_LED1ALED1VAL,
	AFE_DIAG = 0x30,
	AFE_CONTROL3,
	AFE_PDNCYCLESTC,			/* Powerdown cycle start*/
	AFE_PDNCYCLEENDC,			/* Powerdown cycle end*/
	AFE_DPD2STC,
	AFE_DPD2ENDC,
	AFE_LED3LEDSTC,			/* LED3 start(RED)*/
	AFE_LED3LEDENDC,			/* LED3 end(RED)*/
	AFE_RESERVED3,
	AFE_CLK_DIV_REG,
	AFE_DAC_SETTING_REG = 0x3a,
/*	AFE_PROX_L_THRESH_REG,*/
/*	AFE_PROX_H_THRESH_REG,*/
/*	AFE_PROX_SETTING_REG,*/
/*	AFE_COUNTER_32K_REG,*/
/*	AFE_PROX_AVG_REG,*/

	NUM_OF_PPG_REG
};


struct ppsX60_platform_data {
	int (*init)(void);
	int (*deinit)(void);
};

struct ppsX60_device_data {
	struct i2c_client *hrm_i2c_client;          /* represents the slave device*/
	struct device *dev;
	struct input_dev *hrm_input_dev;
	struct input_dev *hrmled_input_dev;
	struct mutex i2clock;
	struct mutex activelock;
	struct regulator *vdd_1p8;
	struct regulator *vdd_3p3;
	const char *led_3p3;
	atomic_t hrm_is_enable;
	atomic_t isEnable_led;
	atomic_t is_suspend;
	u8 look_mode_ir;
	u8 look_mode_red;
	u16 sample_cnt;
	u32 sysfslastreadval;
	int hrm_int;
	int hrm_rst;
	int hrm_en;
	int led_3p3_en;
	int irq;
	int ir_sum;
	int r_sum;
	char *lib_ver;
	u32 led_current;
	int hrm_threshold;
};

extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern int sensors_register(struct device *dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);
#endif

