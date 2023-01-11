/*
 * Copyright 2013 Marvell Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/* PXA1928 Thermal Implementation */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/io.h>
#include <linux/syscalls.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#ifdef CONFIG_CPU_FREQ
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#endif
#include <linux/devfreq.h>
#include <linux/cooling_dev_mrvl.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>

#define TRIP_POINTS_NUM	5
#define TRIP_POINTS_ACTIVE_NUM (TRIP_POINTS_NUM)
#define TRIP_POINTS_STATE_NUM (TRIP_POINTS_NUM + 1)
#define THSENS_NUM	3
#define THSEN_GAIN	3874
#define THSEN_OFFSET	2821
#define INVALID_TEMP	40000
#define PXA1928_THERMAL_POLLING_FREQUENCY_MS 500

#define TSEN_CFG_REG_0			(0x00)
#define TSEN_DEBUG_REG_0		(0x04)
#define TSEN_INT0_WDOG_THLD_REG_0	(0x1c)
#define TSEN_INT1_INT2_THLD_REG_0	(0x20)
#define TSEN_DATA_REG_0			(0x24)
#define TSEN_DATA_RAW_REG_0		(0x28)
#define TSEN_AUTO_READ_VALUE_REG_0	(0x2C)

#define TSEN_CFG_REG_1			(0x30)
#define TSEN_DEBUG_REG_1		(0x34)
#define TSEN_INT0_WDOG_THLD_REG_1	(0x3c)
#define TSEN_INT1_INT2_THLD_REG_1	(0x40)
#define TSEN_DATA_REG_1			(0x44)
#define TSEN_DATA_RAW_REG_1		(0x48)
#define TSEN_AUTO_READ_VALUE_REG_1	(0x4C)

#define TSEN_CFG_REG_2			(0x50)
#define TSEN_DEBUG_REG_2		(0x54)
#define TSEN_INT0_WDOG_THLD_REG_2	(0x5c)
#define TSEN_INT1_INT2_THLD_REG_2	(0x60)
#define TSEN_DATA_REG_2			(0x64)
#define TSEN_DATA_RAW_REG_2		(0x68)
#define TSEN_AUTO_READ_VALUE_REG_2	(0x6c)
/*bit defines of configure reg*/
#define TSEN_ENABLE (1 << 31)
#define TSEN_DIGITAL_RST (1 << 30)
#define TSEN_AUTO_READ (1 << 28)
#define TSEN_DATA_READY (1 << 24)
#define TSEN_AVG_NUM (1 << 25)
#define TSEN_WDOG_DIRECTION (1 << 11)
#define TSEN_WDOG_ENABLE (1 << 10)
#define TSEN_INT2_STATUS (1 << 9)
#define TSEN_INT2_DIRECTION (1 << 8)
#define TSEN_INT2_ENABLE (1 << 7)
#define TSEN_INT1_STATUS (1 << 6)
#define TSEN_INT1_DIRECTION (1 << 5)
#define TSEN_INT1_ENABLE (1 << 4)
#define TSEN_INT0_STATUS (1 << 3)
#define TSEN_INT0_DIRECTION (1 << 2)
#define TSEN_INT0_ENABLE (1 << 1)
#define BALANCE 5000

#define TSEN_THD0_OFF (12)
#define TSEN_THD0_MASK (0xfff000)
#define TSEN_THD1_OFF (0)
#define TSEN_THD1_MASK (0xfff)

#define TSEN_THD2_OFF (12)
#define TSEN_THD2_MASK (0xfff000)
#define TSEN_REG_OFFSET (0x20)
#define TSEN_CFG_REG_OFFSET (0x30)

static struct mutex con_lock;

struct cooling_device {
	struct thermal_cooling_device *combile_cool;
	int max_state, cur_state;
	struct thermal_cooling_device *cool_cpufreq;
	struct thermal_cooling_device *cool_cpuhotplug;
};

struct pxa1928_thermal_device {
	struct thermal_zone_device *therm_cpu;
	struct thermal_zone_device *therm_vpu;
	struct thermal_zone_device *therm_gc;
	int trip_range[3];
	struct resource *mem;
	void __iomem *base;
	struct clk *therclk_g, *therclk_vpu;
	struct clk *therclk_cpu, *therclk_gc;
	struct cooling_device cdev;
	struct cooling_device vdev;
	struct cooling_device gdev;
	int hit_trip_cnt[TRIP_POINTS_NUM];
	int irq;
	int temp_cpu, temp_vpu, temp_gc;
};

static struct pxa1928_thermal_device thermal_dev;
/*
 * 80000, bind to active type
 * 95000, bind to active type
 * 110000,bind to active type
 * 125000,bind to critical type
*/
static int thsens_trips_temp[THSENS_NUM][TRIP_POINTS_NUM] = {
	{80000, 90000, 100000, 110000, 115000},
	{80000, 90000, 100000, 110000, 115000},
	{80000, 90000, 100000, 110000, 115000},
};

static int thsens_trips_hyst[THSENS_NUM][TRIP_POINTS_NUM] = {
	{75000, 85000, 95000, 105000, 115000},
	{75000, 85000, 95000, 105000, 115000},
	{75000, 85000, 95000, 105000, 115000},
};

/* state 0 is related to eden cpu 1.2G,
 *  1 -> 1057M
 *  2 -> 800M
 *  3 -> 624M
 *  4 -> 400M
 *  6 -> 156M
 */
static int pxa1928_cpu_freq_state[TRIP_POINTS_STATE_NUM] = {
	0, 2, 3, 4, 6, 6,
};
static int pxa1928_cpu_plug_state[TRIP_POINTS_STATE_NUM] = {
	0, 2, 2, 3, 3, 0,
};
static int pxa1928_vpu_freq_state[TRIP_POINTS_STATE_NUM] = {
	0, 2, 3, 4, 5, 5,
};
static int pxa1928_gpu_freq_state[TRIP_POINTS_STATE_NUM] = {
	0, 1, 2, 3, 4, 5,
};

#define reg_read(off) readl(thermal_dev.base + (off))
#define reg_write(val, off) __raw_writel((val), thermal_dev.base + (off))
#define reg_clr_set(off, clr, set) \
	reg_write(((reg_read(off) & ~(clr)) | (set)), off)
static int ts_get_trip_temp(struct thermal_zone_device *tz, int trip,
		int *temp);
static int pxa1928_set_threshold(int id, int range);

/*
 * 1: don't have read data feature.
 * 2: have the read data feature, but don't have the valid data check
 * 3: have the read data feature and valid data check
*/
static unsigned int g_flag;

static int celsius_decode(u32 tcode)
{
	int cels;
	cels = (tcode * THSEN_GAIN - THSEN_OFFSET * 1000) / 10000 + 1;
	return cels;
}

static int celsius_encode(int cels)
{
	u32 tcode;
	cels /= 1000;
	tcode = (cels * 10 + THSEN_OFFSET) * 1000 / (THSEN_GAIN);
	return tcode;
}

static void enable_thsens(void *reg)
{
	__raw_writel(readl(reg) | TSEN_ENABLE, reg);
	__raw_writel(readl(reg) & ~TSEN_DIGITAL_RST, reg);
}

static int thsens_are_enabled(void *reg)
{
	if (((readl(reg) & TSEN_ENABLE) == TSEN_ENABLE) &&
		((readl(reg) & TSEN_DIGITAL_RST) == !TSEN_DIGITAL_RST))
		return 1;

	return 0;
}

static void init_wd_int0_threshold(int id, void *reg)
{
	__raw_writel((readl(reg) & 0x000FFF) |
			(celsius_encode(thsens_trips_temp[id][0]) << 12), reg);
	__raw_writel((readl(reg) & 0xFFF000) |
			celsius_encode(thsens_trips_temp[id][4]), reg);
}

static void init_int1_int2_threshold(int id, void *reg)
{
	__raw_writel((readl(reg) & 0xFFF000) |
			celsius_encode(thsens_trips_temp[id][1]), reg);
	__raw_writel((readl(reg) & 0x000FFF) |
			(celsius_encode(thsens_trips_temp[id][2]) << 12), reg);
}

static void init_thsens_irq_dir(void *reg)
{
	__raw_writel(readl(reg) | TSEN_WDOG_DIRECTION |
		TSEN_WDOG_ENABLE | TSEN_INT2_DIRECTION | TSEN_INT2_ENABLE |
		TSEN_INT0_DIRECTION | TSEN_INT0_ENABLE, reg);
}

static void enable_auto_read(void *reg)
{
	__raw_writel(readl(reg) | TSEN_AUTO_READ, reg);
}

static void set_auto_read_interval(void *reg)
{
	__raw_writel(readl(reg) | PXA1928_THERMAL_POLLING_FREQUENCY_MS , reg);
}

u32 reg_offset(int id)
{
	u32 offset = id * TSEN_REG_OFFSET;
	return offset;
}

u32 cfg_reg_offset(int id)
{
	u32 offset = 0;
	if (id == 1)
		offset = TSEN_CFG_REG_OFFSET;
	else if (id == 2)
		offset = TSEN_CFG_REG_OFFSET + TSEN_REG_OFFSET;
	return offset;
}

static int thsens_data_read(void *config_reg, void *data_reg)
{
	int i, j, data = 0;
	int temp = 0;
	if (g_flag >= 3) {
		temp = celsius_decode(readl(data_reg)) * 1000;
		if (temp >= 120000 || temp < 0)
			temp = 30000;
/*
		while (cnt) {
			if (((readl(config_reg) & TSEN_DATA_READY) >> 24)) {
				temp = celsius_decode(readl(data_reg)) * 1000;
				break;
			} else
				cnt--;
		}
		if (cnt == 0) {
			pr_err("Invalid temperature!\n");
			temp = 30000;
		}
*/
	} else if ((g_flag == 2) && thsens_are_enabled(config_reg)) {
		for (i = 0, j = 0; i < 5; i++) {
			data = celsius_decode(readl(data_reg)) * 1000;
			if (data < 130000) {
				j++;
				temp += data;
			}
			usleep_range(1000, 10000);
		}
		if (j != 0)
			temp /= j;
		else
			temp = 30000;
	} else
		temp = 30000;

	return temp;
}

#ifdef DEBUG_TEMPERATURE
static int g_test_temp_vpu = 80000;
static int g_test_temp_cpu = 80000;
static int g_test_temp_gc = 80000;

static int thermal_temp_debug_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct thermal_zone_device *tz_dev = dev_get_drvdata(dev);
	switch (tz_dev->id - 1) {
	case 0:
		return sprintf(buf, "%d\n", g_test_temp_vpu);
		break;
	case 1:
		return sprintf(buf, "%d\n", g_test_temp_cpu);
		break;
	case 2:
		return sprintf(buf, "%d\n", g_test_temp_gc);
		break;
	default:
		pr_err("this is invalid device\n");
		return -INVALID_TEMP;
		break;
	}
}

static int thermal_temp_debug_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct thermal_zone_device *tz_dev = dev_get_drvdata(dev);
	int ret;

	switch (tz_dev->id - 1) {
	case 0:
		ret = sscanf(buf, "%d\n", &g_test_temp_vpu);
		break;
	case 1:
		ret = sscanf(buf, "%d\n", &g_test_temp_cpu);
		break;
	case 2:
		ret = sscanf(buf, "%d\n", &g_test_temp_gc);
		break;
	default:
		pr_err("this is invalid device\n");
		break;
	}
	return count;
}

static DEVICE_ATTR(thermal_debug_temp, 0644, thermal_temp_debug_get,
		thermal_temp_debug_set);

#endif

static struct attribute *thermal_attrs[] = {
#ifdef DEBUG_TEMPERATURE
	&dev_attr_thermal_debug_temp.attr,
#endif
	NULL,
};

static struct attribute_group thermal_attr_grp = {
	.attrs = thermal_attrs,
};

static int
ts_sys_get_temp(struct thermal_zone_device *tz, int *temp)
{
	*temp = 0;
	tz->devdata = thermal_dev.base;
	switch (tz->id - 1) {
	case 0:
#ifdef DEBUG_TEMPERATURE
		*temp = g_test_temp_vpu;
#else
		if (tz->devdata)
			*temp = thsens_data_read(tz->devdata + TSEN_CFG_REG_0,
					tz->devdata + TSEN_DATA_REG_0);
#endif
		thermal_dev.temp_vpu = *temp;
		break;
	case 1:
#ifdef DEBUG_TEMPERATURE
		*temp = g_test_temp_cpu;
#else
		if (tz->devdata)
			*temp = thsens_data_read(tz->devdata + TSEN_CFG_REG_1,
				tz->devdata + TSEN_DATA_REG_1);
#endif
		thermal_dev.temp_cpu = *temp;
		break;
	case 2:
#ifdef DEBUG_TEMPERATURE
		*temp = g_test_temp_gc;
#else
		if (tz->devdata)
			*temp = thsens_data_read(tz->devdata + TSEN_CFG_REG_2,
				tz->devdata + TSEN_DATA_REG_2);
		thermal_dev.temp_gc = *temp;
#endif
		break;
	default:
		*temp = -INVALID_TEMP;
		return -EINVAL;
	}

	return 0;
}

void print_reg(void)
{
	void __iomem *reg_base = thermal_dev.base;
	u32 cfg, int0, int1, data, auto_read, temp;
	int thld0 = 0, thld1 = 0, thld2 = 0;

	cfg = readl(reg_base + TSEN_CFG_REG_1);
	int0 = readl(reg_base + TSEN_INT0_WDOG_THLD_REG_1);
	int1 = readl(reg_base + TSEN_INT1_INT2_THLD_REG_1);
	data = readl(reg_base + TSEN_DATA_REG_1);
	auto_read = readl(reg_base + TSEN_AUTO_READ_VALUE_REG_1);
	temp = celsius_decode(data) * 1000;

	if (thermal_dev.therm_cpu != NULL) {
		ts_get_trip_temp(thermal_dev.therm_cpu, 0, &thld0);
		ts_get_trip_temp(thermal_dev.therm_cpu, 1, &thld1);
		ts_get_trip_temp(thermal_dev.therm_cpu, 2, &thld2);
	}

	pr_info("thermal_cpu temp %u, cfg 0x%x,int0 0x%x,int1 0x%x, data 0x%x,auto_read 0x%x,thld0 %d,thld1 %d,thld2 %d\n",
	temp, cfg, int0, int1, data, auto_read, thld0, thld1, thld2);

	cfg = readl(reg_base + TSEN_CFG_REG_0);
	int0 = readl(reg_base + TSEN_INT0_WDOG_THLD_REG_0);
	int1 = readl(reg_base + TSEN_INT1_INT2_THLD_REG_0);
	data = readl(reg_base + TSEN_DATA_REG_0);
	auto_read = readl(reg_base + TSEN_AUTO_READ_VALUE_REG_0);
	temp = celsius_decode(data) * 1000;

	if (thermal_dev.therm_vpu != NULL) {
		ts_get_trip_temp(thermal_dev.therm_vpu, 0, &thld0);
		ts_get_trip_temp(thermal_dev.therm_vpu, 1, &thld1);
		ts_get_trip_temp(thermal_dev.therm_vpu, 2, &thld2);
	}

	pr_info("thermal_vpu temp %u, cfg 0x%x,int0 0x%x,int1 0x%x, data 0x%x,auto_read 0x%x,thld0 %d,thld1 %d,thld2 %d\n",
	temp, cfg, int0, int1, data, auto_read, thld0, thld1, thld2);

}


static int ts_sys_get_mode(struct thermal_zone_device *thermal,
		enum thermal_device_mode *mode)
{
	*mode = THERMAL_DEVICE_ENABLED;
	return 0;

}

static int ts_sys_get_trip_type(struct thermal_zone_device *thermal, int trip,
		enum thermal_trip_type *type)
{
	if ((trip >= 0) && (trip < TRIP_POINTS_ACTIVE_NUM))
		*type = THERMAL_TRIP_ACTIVE;
	else if (TRIP_POINTS_ACTIVE_NUM == trip)
		*type = THERMAL_TRIP_CRITICAL;
	else
		*type = (enum thermal_trip_type)(-1);
	return 0;
}

static int ts_get_trip_temp(struct thermal_zone_device *tz, int trip,
		int *temp)
{
	void *reg = NULL;
	tz->devdata = thermal_dev.base;
	switch (tz->id - 1) {
	case 0:
		if (0 == trip || 3 == trip)
			reg = tz->devdata + TSEN_INT0_WDOG_THLD_REG_0;
		else if (1 == trip || 2 == trip)
			reg = tz->devdata  + TSEN_INT1_INT2_THLD_REG_0;
		break;
	case 1:
		if (0 == trip || 3 == trip)
			reg = tz->devdata  + TSEN_INT0_WDOG_THLD_REG_1;
		else if (1 == trip || 2 == trip)
			reg = tz->devdata  + TSEN_INT1_INT2_THLD_REG_1;
		break;
	case 2:
		if (0 == trip || 3 == trip)
			reg = tz->devdata  + TSEN_INT0_WDOG_THLD_REG_2;
		else if (1 == trip || 2 == trip)
			reg = tz->devdata  + TSEN_INT1_INT2_THLD_REG_2;
		break;
	default:
		break;
	}

	switch (trip) {
	case 0:
	case 2:
		*temp = (unsigned long)celsius_decode(__raw_readl(reg) >> 12)
			* 1000;
		break;
	case 1:
	case 3:
		*temp = (unsigned long)celsius_decode(__raw_readl(reg) &
				(0x0000FFF)) * 1000;
		break;
	default:
		*temp = -INVALID_TEMP;
		break;
	}
	return 0;
}


static int ts_sys_get_trip_temp(struct thermal_zone_device *thermal, int trip,
		int *temp)
{
	*temp = thsens_trips_temp[thermal->id - 1][trip];
	return 0;
}

static int cpu_sys_get_trip_hyst(struct thermal_zone_device *thermal,
		int trip, int *temp)
{
	if ((trip >= 0) && (trip < TRIP_POINTS_NUM))
		*temp = thsens_trips_hyst[thermal->id - 1][trip];
	else
		*temp = -1;
	return 0;
}

static int cpu_sys_set_trip_hyst(struct thermal_zone_device *tz,
		int trip, int temp)
{
	int id = tz->id - 1;
	if ((trip >= 0) && (trip < TRIP_POINTS_ACTIVE_NUM))
		thsens_trips_hyst[id][trip] = temp;
	if ((TRIP_POINTS_NUM - 1) == trip)
		pr_warn("critical down doesn't used\n");
	else
		pxa1928_set_threshold(id, thermal_dev.trip_range[id]);

	return 0;
}

static int pxa1928_set_threshold(int id, int range)
{
	u32 tmp;
	u32 off = reg_offset(id);
	u32 cfg_off = cfg_reg_offset(id);
	if (range < 0 || range > TRIP_POINTS_ACTIVE_NUM) {
		pr_err("soc thermal: invalid threshold %d\n", range);
		return -1;
	}

	if (0 == range) {
		tmp = (celsius_encode(thsens_trips_temp[id][0])
				<< TSEN_THD0_OFF) & TSEN_THD0_MASK;
		reg_clr_set(off + TSEN_INT0_WDOG_THLD_REG_0,
				TSEN_THD0_MASK, tmp);

		tmp = (celsius_encode(thsens_trips_hyst[id][0])
				<< TSEN_THD1_OFF) & TSEN_THD1_MASK;
		reg_clr_set(off + TSEN_INT1_INT2_THLD_REG_0,
				TSEN_THD1_MASK, tmp);

		reg_clr_set(cfg_off + TSEN_CFG_REG_0, 0, TSEN_INT0_ENABLE);
		reg_clr_set(cfg_off + TSEN_CFG_REG_0, TSEN_INT1_ENABLE, 0);

	} else if (TRIP_POINTS_ACTIVE_NUM == range) {
		tmp = (celsius_encode(thsens_trips_temp[id][range - 1]) <<
						TSEN_THD0_OFF) & TSEN_THD0_MASK;
		reg_clr_set(off + TSEN_INT0_WDOG_THLD_REG_0,
				TSEN_THD0_MASK, tmp);
		tmp = (celsius_encode(thsens_trips_hyst[id][range - 1]) <<
						TSEN_THD1_OFF) & TSEN_THD1_MASK;
		reg_clr_set(off + TSEN_INT1_INT2_THLD_REG_0,
				TSEN_THD1_MASK, tmp);

		reg_clr_set(cfg_off + TSEN_CFG_REG_0, TSEN_INT0_ENABLE, 0);
		reg_clr_set(cfg_off + TSEN_CFG_REG_0, 0, TSEN_INT1_ENABLE);

	} else {
		tmp = (celsius_encode(thsens_trips_temp[id][range]) <<
						TSEN_THD0_OFF) & TSEN_THD0_MASK;
		reg_clr_set(off + TSEN_INT0_WDOG_THLD_REG_0,
				TSEN_THD0_MASK, tmp);
		tmp = (celsius_encode(thsens_trips_hyst[id][range - 1]) <<
						TSEN_THD1_OFF) & TSEN_THD1_MASK;
		reg_clr_set(off + TSEN_INT1_INT2_THLD_REG_0,
				TSEN_THD1_MASK, tmp);
		reg_clr_set(cfg_off + TSEN_CFG_REG_0, 0, TSEN_INT0_ENABLE);
		reg_clr_set(cfg_off + TSEN_CFG_REG_0, 0, TSEN_INT1_ENABLE);
	}
	return 0;
}

static int ts_sys_set_trip_temp(struct thermal_zone_device *tz, int trip,
		int temp)
{
	int id = tz->id - 1;
	if ((trip >= 0) && (trip < TRIP_POINTS_NUM))
		thsens_trips_temp[id][trip] = temp;
	if ((TRIP_POINTS_NUM - 1) == trip) {
		u32 tmp = (celsius_encode
		(thsens_trips_hyst[tz->id - 1][TRIP_POINTS_NUM - 1]) <<
				TSEN_THD2_OFF) & TSEN_THD2_MASK;
		u32 off = reg_offset(id);
		reg_clr_set(off + TSEN_INT1_INT2_THLD_REG_0,
				TSEN_THD2_MASK, tmp);
	} else
		pxa1928_set_threshold(id, thermal_dev.trip_range[id]);
	return 0;
}

static void auto_read_temp(struct thermal_zone_device *tz, const u32 reg,
		const u32 datareg)
{
	int int0, int1, int2, id;
	int temp = 0;
	tz->devdata = thermal_dev.base;
	id = tz->id - 1;

	mutex_lock(&con_lock);

	int0 = (readl(tz->devdata + reg) & TSEN_INT0_STATUS) >> 3;
	int1 = (readl(tz->devdata + reg) & TSEN_INT1_STATUS) >> 6;
	int2 = (readl(tz->devdata + reg) & TSEN_INT2_STATUS) >> 9;

	if (int0 | int1 | int2) {
		if (int0) {
			__raw_writel(readl(tz->devdata + reg),
					tz->devdata + reg);
			thermal_dev.trip_range[id]++;
			if (thermal_dev.trip_range[id] > TRIP_POINTS_ACTIVE_NUM)
				thermal_dev.trip_range[id] =
					TRIP_POINTS_ACTIVE_NUM;
			pxa1928_set_threshold(id, thermal_dev.trip_range[id]);

		} else if (int1) {
			__raw_writel(readl(tz->devdata + reg),
					tz->devdata + reg);
			thermal_dev.trip_range[id]--;
		if (thermal_dev.trip_range[id] < 0)
			thermal_dev.trip_range[id] = 0;
			pxa1928_set_threshold(id, thermal_dev.trip_range[id]);

		} else {
			__raw_writel(readl(tz->devdata + reg),
					tz->devdata + reg);
			/* wait framework shutdown */
			ts_sys_get_temp(thermal_dev.therm_cpu, &temp);
			pr_info("critical temp = %d, shutdown\n", temp);
		}

		thermal_zone_device_update(tz);
	}

	mutex_unlock(&con_lock);

}

static irqreturn_t thermal_threaded_handle_irq(int irq, void *dev_id)
{
	auto_read_temp(thermal_dev.therm_vpu, TSEN_CFG_REG_0, TSEN_DATA_REG_0);
	auto_read_temp(thermal_dev.therm_cpu, TSEN_CFG_REG_1, TSEN_DATA_REG_1);
	if (g_flag != 3)
		auto_read_temp(thermal_dev.therm_gc,
				TSEN_CFG_REG_2, TSEN_DATA_REG_2);
	return IRQ_HANDLED;
}


static int ts_sys_get_crit_temp(struct thermal_zone_device *thermal,
		int *temp)
{
	return thsens_trips_temp[thermal->id - 1][TRIP_POINTS_NUM - 1];
}

static int ts_sys_notify(struct thermal_zone_device *thermal, int count,
		enum thermal_trip_type trip_type)
{
	if (THERMAL_TRIP_CRITICAL == trip_type)
		pr_info("notify critical temp hit\n");
	else
		pr_err("unexpected temp notify\n");
	return 0;
}

static struct thermal_zone_device_ops ts_ops = {
	.get_mode = ts_sys_get_mode,
	.get_temp = ts_sys_get_temp,
	.get_trip_type = ts_sys_get_trip_type,
	.get_trip_temp = ts_sys_get_trip_temp,
	.get_trip_hyst = cpu_sys_get_trip_hyst,
	.set_trip_temp = ts_sys_set_trip_temp,
	.set_trip_hyst = cpu_sys_set_trip_hyst,
	.get_crit_temp = ts_sys_get_crit_temp,
	.notify = ts_sys_notify,
};

#ifdef CONFIG_PM
static int thermal_suspend(struct device *dev)
{
	clk_disable_unprepare(thermal_dev.therclk_vpu);
	clk_disable_unprepare(thermal_dev.therclk_cpu);
	clk_disable_unprepare(thermal_dev.therclk_gc);
	clk_disable_unprepare(thermal_dev.therclk_g);
	return 0;
}

static int thermal_resume(struct device *dev)
{
	clk_prepare_enable(thermal_dev.therclk_g);
	clk_prepare_enable(thermal_dev.therclk_vpu);
	clk_prepare_enable(thermal_dev.therclk_cpu);
	clk_prepare_enable(thermal_dev.therclk_gc);
	return 0;
}

static const struct dev_pm_ops thermal_pm_ops = {
	.suspend = thermal_suspend,
	.resume = thermal_resume,
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id pxa1928_thermal_match[] = {
	{ .compatible = "mrvl,thermal", },
	{},
};
MODULE_DEVICE_TABLE(of, pxa1928_thermal_match);
#endif

void set_flag(unsigned int flag)
{
	g_flag = flag;
}
EXPORT_SYMBOL(set_flag);


static int combile_get_max_state(struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	*state = thermal_dev.cdev.max_state;
	return 0;
}

static int combile_get_cur_state(struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	*state = thermal_dev.cdev.cur_state;
	return 0;
}

static int combile_set_cur_state(struct thermal_cooling_device *cdev,
		unsigned long state)
{
	struct thermal_cooling_device *c_freq = thermal_dev.cdev.cool_cpufreq;
	struct thermal_cooling_device *c_plug =
		thermal_dev.cdev.cool_cpuhotplug;
	unsigned long freq_state = 0, plug_state = 0;
	static u32 prev_state;

	if (state > thermal_dev.cdev.max_state)
		return -EINVAL;
	thermal_dev.cdev.cur_state = state;

	freq_state = pxa1928_cpu_freq_state[state];
	plug_state = pxa1928_cpu_plug_state[state];

	if (c_freq)
		c_freq->ops->set_cur_state(c_freq, freq_state);
	if (c_plug)
		c_plug->ops->set_cur_state(c_plug, plug_state);

	pr_info("Thermal cpu temp %d, state %lu, cpufreq qos %lu, core_num qos %lu\n",
	thermal_dev.temp_cpu, state, freq_state, plug_state);
	if (prev_state < state)
		pr_info("Thermal cpu frequency limitation, performance impact expected!");
	prev_state = state;

	return 0;
}

static struct thermal_cooling_device_ops const cpu_combile_cooling_ops = {
	.get_max_state = combile_get_max_state,
	.get_cur_state = combile_get_cur_state,
	.set_cur_state = combile_set_cur_state,
};

/* Register with the in-kernel thermal management */
static int pxa1928_register_cpu_thermal(void)
{
	int i, trip_w_mask = 0;

	thermal_dev.cdev.cool_cpufreq = cpufreq_cool_register("cpu");
	thermal_dev.cdev.cool_cpuhotplug = cpuhotplug_cool_register("eden");

	thermal_dev.cdev.combile_cool = thermal_cooling_device_register(
		"cpu-combile-cool", &thermal_dev, &cpu_combile_cooling_ops);
	thermal_dev.cdev.max_state = TRIP_POINTS_STATE_NUM;
	thermal_dev.cdev.cur_state = 0;

	for (i = 0; i < TRIP_POINTS_NUM; i++)
		trip_w_mask |= (1 << i);

	thermal_dev.therm_cpu = thermal_zone_device_register(
			"thsens_cpu", TRIP_POINTS_NUM, trip_w_mask,
			&thermal_dev, &ts_ops, NULL, 0, 0);

	/*
	 * enable bi_direction state machine, then it didn't care
	 * whether up/down trip points are crossed or not
	 */
	thermal_dev.therm_cpu->tzdctrl.state_ctrl = true;
	/* bind combile cooling */
	thermal_zone_bind_cooling_device(thermal_dev.therm_cpu,
			0, thermal_dev.cdev.combile_cool,
			THERMAL_NO_LIMIT, THERMAL_NO_LIMIT);

	i = sysfs_create_group(&((thermal_dev.therm_cpu->device).kobj),
			&thermal_attr_grp);
	if (i >= 0)
		pr_info("Pxa: Kernel CPU Thermal interface registered\n");

	return 0;

}

void pxa1928_cpu_thermal_initialize(void)
{
	u32 tmp;
	void __iomem *reg_base = thermal_dev.base;
	enable_thsens(reg_base + TSEN_CFG_REG_1);

	init_wd_int0_threshold(1, reg_base +
			TSEN_INT0_WDOG_THLD_REG_1);
	init_int1_int2_threshold(1, reg_base +
			TSEN_INT1_INT2_THLD_REG_1);

	tmp = (celsius_encode(thsens_trips_hyst[1][TRIP_POINTS_NUM - 1]) <<
			TSEN_THD2_OFF) & TSEN_THD2_MASK;
	reg_clr_set(TSEN_INT1_INT2_THLD_REG_1, TSEN_THD2_MASK, tmp);

	init_thsens_irq_dir(reg_base + TSEN_CFG_REG_1);
	enable_auto_read(reg_base + TSEN_CFG_REG_1);
	reg_clr_set(TSEN_CFG_REG_1, TSEN_INT1_DIRECTION, 0);

	thermal_dev.trip_range[1] = 0;
	pxa1928_set_threshold(1, thermal_dev.trip_range[1]);

	set_auto_read_interval(reg_base + TSEN_AUTO_READ_VALUE_REG_1);

}


static int vpu_get_max_state(struct thermal_cooling_device *vdev,
		unsigned long *state)
{
	*state = thermal_dev.vdev.max_state;
	return 0;
}

static int vpu_get_cur_state(struct thermal_cooling_device *vdev,
		unsigned long *state)
{
	*state = thermal_dev.vdev.cur_state;
	return 0;
}

static int vpu_set_cur_state(struct thermal_cooling_device *vdev,
		unsigned long state)
{
	struct thermal_cooling_device *c_freq = thermal_dev.vdev.cool_cpufreq;

	unsigned long freq_state = 0;

	if (state > thermal_dev.vdev.max_state)
		return -EINVAL;
	thermal_dev.vdev.cur_state = state;

	freq_state = pxa1928_vpu_freq_state[state];

	if (c_freq)
		c_freq->ops->set_cur_state(c_freq, freq_state);

	pr_info("Thermal vpu temp %d, state %lu, vpufreq qos %lu\n",
	thermal_dev.temp_vpu, state, freq_state);

	return 0;
}

static struct thermal_cooling_device_ops const vpu_cooling_ops = {
	.get_max_state = vpu_get_max_state,
	.get_cur_state = vpu_get_cur_state,
	.set_cur_state = vpu_set_cur_state,
};

/* Register with the in-kernel thermal management */
static int pxa1928_register_vpu_thermal(unsigned int index)
{
	int i, trip_w_mask = 0;

	thermal_dev.vdev.cool_cpufreq = vpufreq_cool_register(index);

	thermal_dev.vdev.combile_cool = thermal_cooling_device_register(
			"vpu-combile-cool", &thermal_dev, &vpu_cooling_ops);
	thermal_dev.vdev.max_state = TRIP_POINTS_STATE_NUM;
	thermal_dev.vdev.cur_state = 0;

	for (i = 0; i < TRIP_POINTS_NUM; i++)
		trip_w_mask |= (1 << i);

	thermal_dev.therm_vpu = thermal_zone_device_register(
			"thsens_vpu", TRIP_POINTS_NUM, trip_w_mask,
			&thermal_dev, &ts_ops, NULL, 0, 0);

	/*
	 * enable bi_direction state machine, then it didn't care
	 * whether up/down trip points are crossed or not
	 */
	thermal_dev.therm_vpu->tzdctrl.state_ctrl = true;
	/* bind combile cooling */
	thermal_zone_bind_cooling_device(thermal_dev.therm_vpu,
			0, thermal_dev.vdev.combile_cool,
			THERMAL_NO_LIMIT, THERMAL_NO_LIMIT);

	i = sysfs_create_group(&((thermal_dev.therm_vpu->device).kobj),
			&thermal_attr_grp);
	if (i >= 0)
		pr_info("Pxa: Kernel VPU Thermal interface registered\n");

	return 0;
}

void pxa1928_vpu_thermal_initialize(void)
{
	u32 tmp;
	void __iomem *reg_base = thermal_dev.base;
	enable_thsens(reg_base + TSEN_CFG_REG_0);

	init_wd_int0_threshold(1, reg_base +
			TSEN_INT0_WDOG_THLD_REG_0);
	init_int1_int2_threshold(1, reg_base +
			TSEN_INT1_INT2_THLD_REG_0);

	tmp = (celsius_encode(thsens_trips_hyst[0][TRIP_POINTS_NUM - 1]) <<
			TSEN_THD2_OFF) & TSEN_THD2_MASK;
	reg_clr_set(TSEN_INT1_INT2_THLD_REG_0, TSEN_THD2_MASK, tmp);

	thermal_dev.trip_range[0] = 0;
	pxa1928_set_threshold(0, thermal_dev.trip_range[0]);

	init_thsens_irq_dir(reg_base + TSEN_CFG_REG_0);
	enable_auto_read(reg_base + TSEN_CFG_REG_0);
	reg_clr_set(TSEN_CFG_REG_0, TSEN_INT1_DIRECTION, 0);

	set_auto_read_interval(reg_base + TSEN_AUTO_READ_VALUE_REG_0);

}


static int gpu_get_max_state(struct thermal_cooling_device *gdev,
		unsigned long *state)
{
	*state = thermal_dev.gdev.max_state;
	return 0;
}

static int gpu_get_cur_state(struct thermal_cooling_device *gdev,
		unsigned long *state)
{
	*state = thermal_dev.gdev.cur_state;
	return 0;
}

static int gpu_set_cur_state(struct thermal_cooling_device *gdev,
		unsigned long state)
{
	struct thermal_cooling_device *c_freq = thermal_dev.gdev.cool_cpufreq;

	unsigned long freq_state = 0;

	if (state > thermal_dev.gdev.max_state)
		return -EINVAL;
	thermal_dev.gdev.cur_state = state;

	freq_state = pxa1928_gpu_freq_state[state];

	if (c_freq)
		c_freq->ops->set_cur_state(c_freq, freq_state);

	pr_info("Thermal gpu temp %d, state %lu, gpufreq(gc2d) qos %lu\n",
	thermal_dev.temp_gc, state, freq_state);

	return 0;
}

static struct thermal_cooling_device_ops const gpu_cooling_ops = {
	.get_max_state = gpu_get_max_state,
	.get_cur_state = gpu_get_cur_state,
	.set_cur_state = gpu_set_cur_state,
};


/* Register with the in-kernel thermal management */
static int pxa1928_register_gc_thermal(const char *gc_name)
{
	int i, trip_w_mask = 0;

	thermal_dev.gdev.cool_cpufreq = gpufreq_cool_register(gc_name);

	thermal_dev.gdev.combile_cool = thermal_cooling_device_register(
			"gpu-combile-cool", &thermal_dev, &gpu_cooling_ops);
	thermal_dev.gdev.max_state = TRIP_POINTS_STATE_NUM;
	thermal_dev.gdev.cur_state = 0;

	for (i = 0; i < TRIP_POINTS_NUM; i++)
		trip_w_mask |= (1 << i);

	thermal_dev.therm_gc = thermal_zone_device_register(
			"thsens_gpu", TRIP_POINTS_NUM, trip_w_mask,
			&thermal_dev, &ts_ops, NULL, 0, 0);

	/*
	 * enable bi_direction state machine, then it didn't care
	 * whether up/down trip points are crossed or not
	 */
	thermal_dev.therm_gc->tzdctrl.state_ctrl = true;
	/* bind combile cooling */
	thermal_zone_bind_cooling_device(thermal_dev.therm_gc,
			0, thermal_dev.gdev.combile_cool,
			THERMAL_NO_LIMIT, THERMAL_NO_LIMIT);

	i = sysfs_create_group(&((thermal_dev.therm_gc->device).kobj),
			&thermal_attr_grp);
	if (i >= 0)
		pr_info("Pxa: Kernel GC Thermal interface registered\n");

	return 0;
}

void pxa1928_gc_thermal_initialize(void)
{
	u32 tmp;
	void __iomem *reg_base = thermal_dev.base;
	enable_thsens(reg_base + TSEN_CFG_REG_2);

	init_wd_int0_threshold(1, reg_base +
			TSEN_INT0_WDOG_THLD_REG_2);
	init_int1_int2_threshold(1, reg_base +
			TSEN_INT1_INT2_THLD_REG_2);

	tmp = (celsius_encode(thsens_trips_hyst[2][TRIP_POINTS_NUM - 1]) <<
			TSEN_THD2_OFF) & TSEN_THD2_MASK;
	reg_clr_set(TSEN_INT1_INT2_THLD_REG_2, TSEN_THD2_MASK, tmp);

	thermal_dev.trip_range[2] = 0;
	pxa1928_set_threshold(2, thermal_dev.trip_range[2]);

	init_thsens_irq_dir(reg_base + TSEN_CFG_REG_2);
	enable_auto_read(reg_base + TSEN_CFG_REG_2);
	reg_clr_set(TSEN_CFG_REG_2, TSEN_INT1_DIRECTION, 0);

	set_auto_read_interval(reg_base + TSEN_AUTO_READ_VALUE_REG_2);

}


static int pxa1928_thermal_probe(struct platform_device *pdev)
{
	int ret = 0, irq;
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	void *reg_base;
	/* get resources from platform data */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		pr_err("%s: no IO memory defined\n", __func__);
		ret = -ENOENT;
		goto res_fail;
	}

	/* map registers.*/
	if (!devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), "thermal")) {
		pr_err("can't request region for resource %p\n", res);
		ret = -EINVAL;
		goto res_fail;
	}

	reg_base = devm_ioremap_nocache(&pdev->dev,
			res->start, resource_size(res));

	if (reg_base == NULL) {
		pr_err("%s: res %lx - %lx map failed\n", __func__,
			(unsigned long)res->start, (unsigned long)res->end);
		ret = -ENOMEM;
		goto remap_fail;
	}
	thermal_dev.base = reg_base;

	thermal_dev.therclk_g = clk_get(NULL, "THERMALCLK_G");
	if (IS_ERR(thermal_dev.therclk_g)) {
		pr_err("Could not get thermal clock global\n");
		ret = PTR_ERR(thermal_dev.therclk_g);
		goto clk_fail;
	}
	clk_prepare_enable(thermal_dev.therclk_g);

	thermal_dev.therclk_vpu = clk_get(NULL, "THERMALCLK_VPU");
	if (IS_ERR(thermal_dev.therclk_vpu)) {
		pr_err("Could not get thermal vpu clock\n");
		ret = PTR_ERR(thermal_dev.therclk_vpu);
		goto clk_disable_g;
	}
	clk_prepare_enable(thermal_dev.therclk_vpu);

	thermal_dev.therclk_cpu = clk_get(NULL, "THERMALCLK_CPU");
	if (IS_ERR(thermal_dev.therclk_cpu)) {
		pr_err("Could not get thermal cpu clock\n");
		ret = PTR_ERR(thermal_dev.therclk_cpu);
		goto clk_disable_0;
	}
	clk_prepare_enable(thermal_dev.therclk_cpu);

	thermal_dev.therclk_gc = clk_get(NULL, "THERMALCLK_GC");
	if (IS_ERR(thermal_dev.therclk_gc)) {
		pr_err("Could not get thermal gc clock\n");
		ret = PTR_ERR(thermal_dev.therclk_gc);
		goto clk_disable_1;
	}
	clk_prepare_enable(thermal_dev.therclk_gc);

	irq = platform_get_irq(pdev, 0);
	thermal_dev.irq = irq;
	if (irq < 0) {
		dev_err(&pdev->dev, "no IRQ resource defined\n");
		ret = -ENXIO;
		goto clk_disable_2;
	}

	ret = request_threaded_irq(irq, NULL, thermal_threaded_handle_irq,
		IRQF_ONESHOT, pdev->name, NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request IRQ\n");
		ret = -ENXIO;
		goto clk_disable_2;
	}

	mutex_init(&con_lock);

	if (IS_ENABLED(CONFIG_OF)) {
		if (np)
			of_property_read_u32(np, "marvell,version-flag",
					&g_flag);
	}

	pxa1928_vpu_thermal_initialize();
	pxa1928_register_vpu_thermal(1);

	pxa1928_cpu_thermal_initialize();
	pxa1928_register_cpu_thermal();

	if (g_flag != 3) {
		pxa1928_gc_thermal_initialize();
		pxa1928_register_gc_thermal("gc2d");
	}

	return ret;

clk_disable_2:
	clk_disable_unprepare(thermal_dev.therclk_gc);
clk_disable_1:
	clk_disable_unprepare(thermal_dev.therclk_cpu);
clk_disable_0:
	clk_disable_unprepare(thermal_dev.therclk_vpu);
clk_disable_g:
	clk_disable_unprepare(thermal_dev.therclk_g);
clk_fail:
	devm_iounmap(&pdev->dev, thermal_dev.base);
remap_fail:
	devm_release_mem_region(&pdev->dev, res->start, resource_size(res));
res_fail:
	pr_err("device init failed\n");

	return ret;
}

static int pxa1928_thermal_remove(struct platform_device *pdev)
{
	clk_disable_unprepare(thermal_dev.therclk_vpu);
	clk_disable_unprepare(thermal_dev.therclk_cpu);
	clk_disable_unprepare(thermal_dev.therclk_gc);
	clk_disable_unprepare(thermal_dev.therclk_g);

	cpufreq_cool_unregister(thermal_dev.cdev.cool_cpufreq);
	cpuhotplug_cool_unregister(thermal_dev.cdev.cool_cpuhotplug);
	thermal_cooling_device_unregister(thermal_dev.cdev.combile_cool);

	cpufreq_cool_unregister(thermal_dev.vdev.cool_cpufreq);
	thermal_cooling_device_unregister(thermal_dev.vdev.combile_cool);
	thermal_zone_device_unregister(thermal_dev.therm_cpu);

	free_irq(thermal_dev.irq, NULL);
	pr_info("PXA1928: Kernel Thermal management unregistered\n");

	return 0;
}

static struct platform_driver pxa1928_thermal_driver = {
	.driver = {
		.name	= "thermal",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &thermal_pm_ops,
#endif
		.of_match_table = of_match_ptr(pxa1928_thermal_match),
	},
	.probe		= pxa1928_thermal_probe,
	.remove		= pxa1928_thermal_remove,
};

static int __init pxa1928_thermal_init(void)
{
	return platform_driver_register(&pxa1928_thermal_driver);
}

static void __exit pxa1928_thermal_exit(void)
{
	platform_driver_unregister(&pxa1928_thermal_driver);
}

module_init(pxa1928_thermal_init);
module_exit(pxa1928_thermal_exit);

MODULE_AUTHOR("Marvell Semiconductor");
MODULE_DESCRIPTION("PXA1928 SoC thermal driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pxa1928-thermal");
