/*
 * core.h  --  Core Driver for Dialog Semiconductor D2199 PMIC
 *
 * Copyright 2013 Dialog Semiconductor Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __D2199_BOBCAT_CORE_H_
#define __D2199_BOBCAT_CORE_H_

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/d2199/pmic.h>
#include <linux/d2199/rtc.h>
#include <linux/d2199/hwmon.h>
#include <linux/power_supply.h>
#include <linux/d2199/d2199_battery.h>
#include <linux/battery/sec_fuelgauge.h>

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif /*CONFIG_HAS_EARLYSUSPEND*/

/*
 * Register values.
 */  

#define I2C								1

#define D2199_I2C						"d2199"

/* Module specific error codes */
#define INVALID_REGISTER				2
#define INVALID_READ					3
#define INVALID_PAGE					4

/* Total number of registers in D2199 */
#define D2199_MAX_REGISTER_CNT			0x0100

#define D2199_PMIC_I2C_ADDR				(0x92 >> 1)   // 0x49
#define D2199_CODEC_I2C_ADDR			(0x30 >> 1)   // 0x18
#define D2199_AAD_I2C_ADDR				(0x32 >> 1)   // 0x19

#define D2199_IOCTL_READ_REG  0xc0025083
#define D2199_IOCTL_WRITE_REG 0x40025084

#define __CONFIG_BOBCAT_BATTERY

typedef struct {
	unsigned long reg;
	unsigned short val;
} pmu_reg;

/*
 * DA1980 Number of Interrupts
 */
enum D2199_IRQ {
	// EVENT_A register IRQ
	D2199_IRQ_EVF = 0,
	D2199_IRQ_EADCIN1,
	D2199_IRQ_ETBAT2,
	D2199_IRQ_EVDD_LOW,
	D2199_IRQ_EVDD_MON,
	D2199_IRQ_EALARM,
	D2199_IRQ_ESEQRDY,
	D2199_IRQ_ETICK,

	// EVENT_B register IRQ
	D2199_IRQ_ENONKEY_LO,
	D2199_IRQ_ENONKEY_HI,
	D2199_IRQ_ENONKEY_HOLDON,
	D2199_IRQ_ENONKEY_HOLDOFF,
	D2199_IRQ_ETBAT1,
	D2199_IRQ_EADCEOM,

	// EVENT_C register IRQ
	D2199_IRQ_ETA,
	D2199_IRQ_ENJIGON,
	D2199_IRQ_EACCDET,
	D2199_IRQ_EJACKDET,

	D2199_NUM_IRQ
};


#define D2199_MCTL_MODE_INIT(_reg_id, _dsm_mode, _default_pm_mode) \
	[_reg_id] = { \
		.reg_id = _reg_id, \
		.dsm_opmode = _dsm_mode, \
		.default_pm_mode = _default_pm_mode, \
	}

// for DEBUGGING and Troubleshooting
#if 1	//defined(DEBUG)
#define dlg_crit(fmt, ...) printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define dlg_err(fmt, ...) printk(KERN_ERR fmt, ##__VA_ARGS__)
#define dlg_warn(fmt, ...) printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define dlg_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)
#define dlg_dbg(fmt, ...) printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#else
#define dlg_crit(fmt, ...) 	do { } while(0);
#define dlg_err(fmt, ...)	do { } while(0);
#define dlg_warn(fmt, ...)	do { } while(0);
#define dlg_info(fmt, ...)	do { } while(0);
#define dlg_dbg(fmt, ...) 	do { } while(0);
#endif

typedef u32 (*pmu_platform_callback)(int event, int param);

struct d2199_irq {
	irq_handler_t handler;
	void *data;
};

struct d2199_onkey {
	struct platform_device *pdev;
	struct input_dev *input;
};

struct d2199_regl_init_data {
	int regl_id;
	struct regulator_init_data   *initdata;
};


struct d2199_regl_map {
	u8 reg_id;
	u8 dsm_opmode;
	u8 default_pm_mode;
};


/**
 * Data to be supplied by the platform to initialise the D2199.
 *
 * @init: Function called during driver initialisation.  Should be
 *        used by the platform to configure GPIO functions and similar.
 * @irq_high: Set if D2199 IRQ is active high.
 * @irq_base: Base IRQ for genirq (not currently used).
 */

struct temp2adc_map {
	int temp;
	int adc;
};

struct d2199_hwmon_platform_data {
	u32	battery_capacity;
	u32	vf_lower;
	u32	vf_upper;
	int battery_technology;
	struct temp2adc_map *bcmpmu_temp_map;
	int bcmpmu_temp_map_len;
};
//AFO struct i2c_slave_platform_data { };

struct d2199_battery_platform_data {
	u32	battery_capacity;
	u32	vf_lower;
	u32	vf_upper;
	int battery_technology;
};

#if defined(CONFIG_D2199_DVC)
struct d2199_dvc_pdata {
	int dvc1;
	int dvc2;
	unsigned int *vol_val;
	int size;
	int gpio_dvc;	/* 0 when gpios are not used for dvc */
	int reg_dvc;	/* 0 when direct register is used for dvc */
	int (*set_dvc)(int reg_id, int volt);
	void __iomem *write_reg; /* Register to set level */
	void __iomem *read_reg;  /* Register to read level */
};
#endif

struct d2199_headset_pdata {
	u8		send_min;
	u8		send_max;
	u8		vol_up_min;
	u8		vol_up_max;
	u8		vol_down_min;
	u8		vol_down_max;
	u8		jack_3pole_max;
	u8		jack_4pole_max;
};

struct d2199_platform_data {
	//AFO struct i2c_slave_platform_data i2c_pdata;
	int i2c_adapter_id;

	int 	(*init)(struct d2199 *d2199);
	int 	(*irq_init)(struct d2199 *d2199);
	int		irq_mode;	/* Clear interrupt by read/write(0/1) */
	int		irq_base;	/* IRQ base number of D2199 */
	struct d2199_regl_init_data	*regulator_data;
	//struct  regulator_consumer_supply *regulator_data;
	struct d2199_hwmon_platform_data *hwmon_pdata;
#if defined(CONFIG_BATTERY_SAMSUNG)
	sec_battery_platform_data_t *pbat_platform;
#else
	struct d2199_battery_platform_data *pbat_platform;
#endif
#if defined(CONFIG_SEC_PM)
	int	onkey_long_press_time;
#endif

	// DLG. eric. 2012/10/16 pmu_platform_callback pmu_event_cb;

	//unsigned char regl_mapping[BOBCAT_IOCTL_REGL_MAPPING_NUM];	/* Regulator mapping for IOCTL */
	struct d2199_regl_map regl_map[D2199_NUMBER_OF_REGULATORS];
#if defined(CONFIG_D2199_DVC)
	struct d2199_dvc_pdata *dvc;
#endif
	int		(*sync)(unsigned int ticks);	/* for RTC */
	struct d2199_headset_pdata *headset;  /* for headset ADC range */
};

struct d2199 {
	struct device *dev;
#if defined(CONFIG_SEC_PM)
	struct device *sec_power;
#endif

	struct i2c_client *pmic_i2c_client;
	struct i2c_client *codec_i2c_client;
	struct mutex i2c_mutex;

	int (*read_dev)(struct d2199 * const d2199, char reg, int size, void *dest);
	int (*write_dev)(struct d2199 * const d2199, char reg, int size, u8 *src);

	u8 *reg_cache;
	u16 vbat_init_adc[3];
	u16 average_vbat_init_adc;

	/* Interrupt handling */
    struct work_struct irq_work;
    struct task_struct *irq_task;
	struct mutex irq_mutex; /* IRQ table mutex */
	struct d2199_irq irq[D2199_NUM_IRQ];
	int chip_irq;

	struct d2199_pmic pmic;
	struct d2199_rtc rtc;
	struct d2199_onkey onkey;
	struct d2199_hwmon hwmon;
	struct d2199_battery batt;

    struct d2199_platform_data *pdata;
	struct mutex d2199_io_mutex; 
#if defined(CONFIG_D2199_DVC)
	struct d2199_dvc_pdata *dvc;
	int dvc_val;
#endif
};

/*
 * d2199 Core device initialisation and exit.
 */
int	d2199_device_init(struct d2199 *d2199, int irq, struct d2199_platform_data *pdata);
void	d2199_device_exit(struct d2199 *d2199);

/*
 * d2199 device IO
 */
int	d2199_clear_bits(struct d2199 * const d2199, u8 const reg, u8 const mask);
int	d2199_set_bits(struct d2199* const d2199, u8 const reg, u8 const mask);
int    d2199_reg_read(struct d2199 * const d2199, u8 const reg, u8 *dest);
int	d2199_reg_write(struct d2199 * const d2199, u8 const reg, u8 const val);
int	d2199_reg_write_lock(struct d2199 * const d2199, u8 const reg, u8 const val);
int	d2199_block_read(struct d2199 * const d2199, u8 const start_reg, u8 const regs, u8 * const dest);
int	d2199_block_write(struct d2199 * const d2199, u8 const start_reg, u8 const regs, u8 * const src);


/*
 * d2199 internal interrupts
 */
int	d2199_register_irq(struct d2199 *d2199, int irq, irq_handler_t handler,
				            unsigned long flags, const char *name, void *data);
int	d2199_free_irq(struct d2199 *d2199, int irq);
int	d2199_mask_irq(struct d2199 *d2199, int irq);
int	d2199_unmask_irq(struct d2199 *d2199, int irq);
int	d2199_irq_init(struct d2199 *d2199, int irq, struct d2199_platform_data *pdata);
int	d2199_irq_exit(struct d2199 *d2199);

#if defined(CONFIG_MACH_RHEA_SS_IVORY) || defined(CONFIG_MACH_RHEA_SS_NEVIS)
/* DLG IOCTL interface */
extern int d2199_ioctl_regulator(struct d2199 *d2199, unsigned int cmd, unsigned long arg);
#endif /* CONFIG_MACH_RHEA_SS_IVORY */


extern void d2199_clk32k_enable(int onoff);
extern int d2199_extern_reg_read(u8 const reg, u8 *dest);
extern int d2199_extern_reg_write(u8 const reg, u8 const val);
extern int d2199_extern_block_read(u8 const start_reg, u8 *dest, u8 num_regs);

/* DLG new prototype */
void 	d2199_system_poweroff(void);
void 	d2199_set_mctl_enable(void);

#if defined(CONFIG_D2199_DVC)
extern int d2199_extern_dvc_write(int level, unsigned int reg_val);
extern int d2199_extern_dvc_read(int level);
#endif
#endif /* __D2199_BOBCAT_CORE_H_ */
