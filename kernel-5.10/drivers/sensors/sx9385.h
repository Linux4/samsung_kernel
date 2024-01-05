/*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*/
#ifndef SX9385_H
#define SX9385_H


#define VENDOR_NAME              "SEMTECH"
#define NOTI_MODULE_NAME         "grip_notifier"


enum ic_num {
	MAIN_GRIP = 0,
	SUB_GRIP,
	SUB2_GRIP,
	WIFI_GRIP,
	GRIP_MAX_CNT
};

const char *grip_name[GRIP_MAX_CNT] = {
	"MAIN",
	"SUB",
	"SUB2",
	"WIFI"
};

const char *device_name[GRIP_MAX_CNT] = {
	"SX9385",
	"SX9385_SUB",
	"SX9385_SUB2",
	"SX9385_WIFI"
};

const char *module_name[GRIP_MAX_CNT] = {
	"grip_sensor",
	"grip_sensor_sub",
	"grip_sensor_sub2",
	"grip_sensor_wifi"
};

/* IrqStat 0:Inactive 1:Active */
#define IRQSTAT_RESET_FLAG		0x80
#define IRQSTAT_TOUCH_FLAG		0x20
#define IRQSTAT_RELEASE_FLAG	0x10
#define IRQSTAT_COMPDONE_FLAG	0x08
#define IRQSTAT_CONV_FLAG		0x04

#define SX9385_STAT_COMPSTAT_ALL_FLAG (0x02 | 0x04)

//Registers refered by the driver
#define REG_IRQ_SRC         0x0 //smtc_read_and_clear_irq
#define REG_PROX_STATUS     0x1 //smtc_i2c_read(self, REG_PROX_STATUS, &prox_state); if (prox_state & phase->prox_mask)
#define REG_COMPENSATION    0x2 //    ret = smtc_i2c_write(self, REG_COMPENSATION, 0xF);
#define REG_IRQ_MSK			0x4
#define REG_PHEN            0x9
#define REG_GNRL_CTRL2      0xB
#define REG_RESET           0x38
#define REG_DEV_INFO        0x3E
#define REG_PH0_OFFSET      0x4B
#define REG_PH0_USEFUL      0xBA
#define REG_DLT_VAR_PH2     0xC2
#define REG_PH0_AVERAGE     0xC7
#define REG_PH0_DIFF        0xCB

#define REG_AFE_PARAM2_PH1  0x49
#define REG_AFE_PARAM2_PH2  0x4F
#define REG_AFE_PARAM2_PH3  0x56
#define REG_AFE_PARAM2_PH4  0x5C

#define REG_AFE_PARAM3_PH1  0x49

#define REG_PROX_CTRL4_PH2  0x7D
#define REG_PROX_CTRL5_PH2  0x7E

#define REG_PROX_CTRL4_PH4  0x83
#define REG_PROX_CTRL5_PH4  0x84
#define REG_PROX_CTRL3_PH2	0x7C
#define REG_PROX_CTRL3_PH4	0x82

#define REG_PROX_CTRL1_PH2	0x7A
#define REG_PROX_CTRL0_PH12	0x79
#define REG_PROX_CTRL0_PH34	0x7F

#define REG_PROX_CTRL1_PH4	0x80
#define REG_AFE_PARAM0_PH2	0x4D
#define REG_AFE_PARAM0_PH4	0x5A

#define REG_AFE_PARAM0_PH2_MSB	0x4D
#define REG_AFE_PARAM1_PH2_LSB	0x4E
#define REG_AFE_PARAM0_PH4_MSB	0x5A
#define REG_AFE_PARAM1_PH4_LSB	0x5B

#define REG_USEFILTER0_PH2		0x92
#define REG_USEFILTER0_PH4		0x9E


#define REG_PROX_CTRL4_PH2		0x7D
#define REG_PROX_CTRL4_PH4		0x83

/* RegStat0  */
#define SX9385_PROXSTAT_CH1_FLAG			(1 << 4)
#define SX9385_PROXSTAT_CH2_FLAG			(1 << 5)

/*      SoftReset */
#define SX9385_SOFTRESET				0xDE
#define SX9385_WHOAMI_VALUE				0x13
#define SX9385_REV_VALUE				0x13

#define MAX_NUM_STATUS_BITS (8)

enum {
	OFF = 0,
	ON = 1
};

#define GRIP_ERR(fmt, ...) pr_err("[GRIP_%s] %s "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)
#define GRIP_INFO(fmt, ...) pr_info("[GRIP_%s] %s "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)
#define GRIP_WARN(fmt, ...) pr_warn("[GRIP_%s] %s "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)

#if !IS_ENABLED(CONFIG_SENSORS_CORE_AP)
extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern int sensors_register(struct device **dev, void *drvdata,
			    struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
			       struct device_attribute *attributes[]);
#endif
#endif
