/*
 * Copyright 2012 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com/
 *
 * Header file for tmu support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_TMU_H
#define __ASM_ARCH_TMU_H

#include <linux/platform_data/exynos_thermal.h>

/**********************************************/
/* 		      SFRs		      */
/**********************************************/
/*Exynos generic registers*/
#define EXYNOS_TMU_REG_TRIMINFO		0x0
#define EXYNOS_TMU_TRIMINFO_CON		0x14
#define EXYNOS_TMU_REG_CONTROL		0x20
#define EXYNOS_TMU_REG_STATUS		0x28
#define EXYNOS_TMU_REG_CURRENT_TEMP	0x40
#ifdef CONFIG_SOC_EXYNOS5260
#define EXYNOS_TMU_REG_INTEN		0xb0
#define EXYNOS_TMU_REG_RISE2_INTEN	0xc0
#define EXYNOS_TMU_REG_INTSTAT		0xb4
#define EXYNOS_TMU_REG_INTCLEAR		0xb8
#define EXYNOS_EMUL_CON			0x100
#else
#define EXYNOS_TMU_REG_INTEN		0x70
#define EXYNOS_TMU_REG_INTSTAT		0x74
#define EXYNOS_TMU_REG_INTCLEAR		0x78
#define EXYNOS_EMUL_CON			0x80
#endif
/*Exynos4 specific registers*/
#define EXYNOS4_TMU_REG_THRESHOLD_TEMP	0x44
#define EXYNOS4_TMU_REG_TRIG_LEVEL0	0x50
#define EXYNOS4_TMU_REG_TRIG_LEVEL1	0x54
#define EXYNOS4_TMU_REG_TRIG_LEVEL2	0x58
#define EXYNOS4_TMU_REG_TRIG_LEVEL3	0x5C
/*Exynos5 specific registers*/
#define EXYNOS5_TMU_REG_CONTROL1	0x24
#define EXYNOS5_THD_TEMP_RISE		0x50
#define EXYNOS5_THD_TEMP_FALL		0x54

/**********************************************/
/* 		  Definitions		      */
/**********************************************/
/*Definitions for E-Fuse value*/
#define EXYNOS5_TRIMINFO_RELOAD		0x1
#define EXYNOS_TMU_TRIM_TEMP_MASK	0xff
#define EFUSE_MIN_VALUE 		16
#define EFUSE_MAX_VALUE 		76
/*Definitions for threshold temperature*/
#define THRESH_LEVE1_SHIFT		8
#define THRESH_LEVE2_SHIFT		16
#define THRESH_LEVE3_SHIFT		24
/*Definitions for TMU_CONTROL register*/
#define EXYNOS_TMU_GAIN_SHIFT		8
#define EXYNOS_TMU_REF_VOLTAGE_SHIFT	24
#define EXYNOS_TMU_TRIP_MODE_SHIFT	13
#define EXYNOS_TMU_CORE_ON		3
#define EXYNOS_TMU_CORE_OFF		2
#define EXYNOS_MUX_ADDR_VALUE		6
#define EXYNOS_MUX_ADDR_SHIFT		20
#define EXYNOS_THERM_TRIP_EN		(1 << 12)
/*Definitions for INTEN register*/
#define RISE_LEVEL1_SHIFT		4
#define RISE_LEVEL2_SHIFT		8
#define RISE_LEVEL3_SHIFT		12
#define FALL_LEVEL0_SHIFT		16
#define FALL_LEVEL1_SHIFT		20
#define FALL_LEVEL2_SHIFT		24
#define FALL_LEVEL3_SHIFT		28
/*Definitions for interrupt*/
#define EXYNOS4_TMU_INTCLEAR_VAL	0x1111
#define EXYNOS5_TMU_CLEAR_RISE_INT	0x1111
#define EXYNOS5_TMU_CLEAR_FALL_INT	(0x1111 << 16)
/*In-kernel thermal framework related macros & definations*/
#define SENSOR_NAME_LEN			16
#define MAX_TRIP_COUNT			8
#define MAX_COOLING_DEVICE 		4
#define MCELSIUS			1000
#define kHz				1000
#define GAP_WITH_RISE			2
#define EXYNOS_TMU_DEF_CODE_TO_TEMP_OFFSET	50
#define MAX_FREQ			2300
#define MIN_FREQ			400
/*platform specific definition of TMU*/
#if defined(CONFIG_SOC_EXYNOS5410)
#define EXYNOS_TMU_COUNT		4
#define COLD_TEMP			20
#elif defined(CONFIG_SOC_EXYNOS5420)
#define EXYNOS_TMU_COUNT		5
#define COLD_TEMP			10
#elif defined(CONFIG_SOC_EXYNOS5260)
#define EXYNOS_TMU_COUNT		5
#define COLD_TEMP			20
#else
#define EXYNOS_TMU_COUNT		1
#define COLD_TEMP			10
#endif
#define EXYNOS_GPU_NUMBER		4
#ifdef CONFIG_SOC_EXYNOS5260
#define EXYNOS_GPU0_NUMBER		3
#define EXYNOS_GPU1_NUMBER		4
#endif
/*Definitions for thermal zone*/
#define GET_ZONE(trip) (trip + 2)
#define GET_TRIP(zone) (zone - 2)
#define EXYNOS_ZONE_COUNT		3
/* CPU Zone information */
#define PANIC_ZONE      		4
#define WARN_ZONE       		3
#define MONITOR_ZONE    		2
#define SAFE_ZONE       		1
/*Polling intervals*/
#define PASSIVE_INTERVAL 		100
#define ACTIVE_INTERVAL 		300
#define IDLE_INTERVAL			1000
/*Definitions for notification*/
#define HOT_NORMAL_TEMP			95
#define HOT_CRITICAL_TEMP		110
#define HOT_95				95
#define HOT_109				104
#define HOT_110				105
#define MEM_TH_TEMP1			85
#define MEM_TH_TEMP2			95
#define GPU_TH_TEMP1			80
#define GPU_TH_TEMP2			85
#define GPU_TH_TEMP3			90
#define GPU_TH_TEMP4			95
#define GPU_TH_TEMP5			100
/*macro for debugging*/
#ifdef CONFIG_THERMAL_DEBUG
#define DTM_DBG(x...) printk(x)
#else
#define DTM_DBG(x...) do {} while (0)
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
#define NUM_UNCOOLED_CLUSTER		2
#else
#define NUM_UNCOOLED_CLUSTER		1
#endif

#if defined(CONFIG_SOC_EXYNOS4212) || defined(CONFIG_SOC_EXYNOS4412)
static struct exynos_tmu_platform_data const exynos4_default_tmu_data = {
	.threshold = 80,
	.trigger_levels[0] = 5,
	.trigger_levels[1] = 20,
	.trigger_levels[2] = 30,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.trigger_level3_en = 0,
	.gain = 15,
	.reference_voltage = 7,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.freq_tab[0] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 85,
	},
	.freq_tab[1] = {
		.freq_clip_max = 200 * 1000,
		.temp_level = 100,
	},
	.freq_tab_count = 2,
	.type = SOC_ARCH_EXYNOS4,
	.clk_name = "tmu_apbif",
};
#define EXYNOS4_TMU_DRV_DATA (&exynos4_default_tmu_data)
#else
#define EXYNOS4_TMU_DRV_DATA (NULL)
#endif

#if defined(CONFIG_SOC_EXYNOS5410)
static struct exynos_tmu_platform_data const exynos5_default_tmu_data = {
	.trigger_levels[0] = 70,
	.trigger_levels[1] = 75,
	.trigger_levels[2] = 110,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.trigger_level3_en = 0,
	.gain = 8,
	.reference_voltage = 16,
	.noise_cancel_mode = 4,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.efuse_value = 55,
	.freq_tab[0] = {
		.freq_clip_max = 1600 * 1000,
		.temp_level = 70,
	},
	.freq_tab[1] = {
		.freq_clip_max = 1400 * 1000,
		.temp_level = 75,
	},
	.freq_tab[2] = {
		.freq_clip_max = 1200 * 1000,
		.temp_level = 80,
	},
	.freq_tab[3] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 85,
	},
	.freq_tab[4] = {
		.freq_clip_max = 400 * 1000,
		.temp_level = 100,
	},
	.size[THERMAL_TRIP_ACTIVE] = 1,
	.size[THERMAL_TRIP_PASSIVE] = 3,
	.size[THERMAL_TRIP_HOT] = 1,
	.freq_tab_count = 5,
	.type = SOC_ARCH_EXYNOS5,
	.clk_name = "tmu_apbif",
};
#define EXYNOS5_TMU_DRV_DATA (&exynos5_default_tmu_data)
#else
#define EXYNOS5_TMU_DRV_DATA (NULL)
#endif

enum tmu_status_t {
	TMU_STATUS_INIT = 0,
	TMU_STATUS_NORMAL,
	TMU_STATUS_THROTTLED,
	TMU_STATUS_TRIPPED,
};

enum mif_noti_state_t {
	MEM_TH_LV1 = 4,
	MEM_TH_LV2,
	MEM_TH_LV3,
};

enum tmu_noti_state_t {
	TMU_NORMAL,
	TMU_COLD,
	TMU_HOT,
	TMU_CRITICAL,
	TMU_95,
	TMU_109,
	TMU_110,
	TMU_111, // for detect thermal runaway caused by fimc
};

enum gpu_noti_state_t {
	GPU_NORMAL,
	GPU_COLD,
	GPU_THROTTLING1,
	GPU_THROTTLING2,
	GPU_THROTTLING3,
	GPU_THROTTLING4,
	GPU_TRIPPING,
};

struct exynos_tmu_data {
	struct exynos_tmu_platform_data *pdata;
	struct resource *mem[EXYNOS_TMU_COUNT];
	void __iomem *base[EXYNOS_TMU_COUNT];
	int id[EXYNOS_TMU_COUNT];
	int irq[EXYNOS_TMU_COUNT];
	enum soc_type soc;
	struct work_struct irq_work;
	struct mutex lock;
	struct clk *clk;
	u8 temp_error1[EXYNOS_TMU_COUNT];
	u8 temp_error2[EXYNOS_TMU_COUNT];
};

struct	thermal_trip_point_conf {
	int trip_val[MAX_TRIP_COUNT];
	int trip_count;
};

struct	thermal_cooling_conf {
	struct freq_clip_table freq_data[MAX_TRIP_COUNT];
	int size[THERMAL_TRIP_CRITICAL + 1];
	int freq_clip_count;
};

struct thermal_sensor_conf {
	char name[SENSOR_NAME_LEN];
	int (*read_temperature)(void *data);
	struct thermal_trip_point_conf trip_data;
	struct thermal_cooling_conf cooling_data;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	struct thermal_cooling_conf cooling_data_kfc;
#endif
	void *private_data;
};

struct exynos_thermal_zone {
	enum thermal_device_mode mode;
	struct thermal_zone_device *therm_dev;
	struct thermal_cooling_device *cool_dev[MAX_COOLING_DEVICE];
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	struct thermal_cooling_device *cool_dev_kfc[MAX_COOLING_DEVICE];
#endif
	unsigned int cool_dev_size;
	struct platform_device *exynos4_dev;
	struct thermal_sensor_conf *sensor_conf;
};

extern void exynos_tmu_set_platdata(struct exynos_tmu_platform_data *pd);
extern struct tmu_info *exynos_tmu_get_platdata(void);
extern int exynos_tmu_get_irqno(int num);
extern struct platform_device exynos_device_tmu;
#ifdef CONFIG_EXYNOS_THERMAL
extern int exynos_tmu_add_notifier(struct notifier_block *n);
extern int exynos_gpu_add_notifier(struct notifier_block *n);
#else
static inline int exynos_tmu_add_notifier(struct notifier_block *n)
{
	return 0;
}
#endif

/************************************************************************/
/************************************************************************/
/*           		for the lecacy driver	      			*/
/************************************************************************/
/************************************************************************/
/*Definitions*/
#define MUX_ADDR_VALUE			6
#define TMU_SAVE_NUM 			10
#define UNUSED_THRESHOLD		0xFF

struct temperature_params {
	unsigned int stop_throttle;
	unsigned int start_throttle;
	unsigned int start_tripping; /* temp to do tripping */
	unsigned int start_emergency;
	unsigned int stop_mem_throttle;
	unsigned int start_mem_throttle;
};

struct tmu_data {
	struct temperature_params ts;
	unsigned int efuse_value;
	unsigned int slope;
	int mode;
};

struct tmu_info {
	int id;
	void __iomem	*tmu_base;
	struct device	*dev;
	struct resource *ioarea;
	int irq;

	unsigned int te1; /* triminfo_25 */
	unsigned int te2; /* triminfo_85 */
	int tmu_state;

	bool mem_throttled;
	unsigned int auto_refresh_mem_throttle;
	unsigned int auto_refresh_normal;

	/* monitoring rate */
	unsigned int sampling_rate;

	struct delayed_work polling;
	struct delayed_work monitor;
	unsigned int reg_save[TMU_SAVE_NUM];
};


#endif /* __ASM_ARCH_TMU_H */
