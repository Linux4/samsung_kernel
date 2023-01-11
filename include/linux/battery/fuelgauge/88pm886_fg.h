#ifndef __88PM886_FG_H
#define __88PM886_FG_H
//Vishwa
#include <linux/mfd/88pm886.h>

#define PM886_VBAT_MEAS_EN		(1 << 1)
#define PM886_GPADC_BD_PREBIAS		(1 << 4)
#define PM886_GPADC_BD_EN		(1 << 5)
#define PM886_BD_GP_SEL			(1 << 6)

#define PM886_CC_CONFIG1		(0x01)
#define PM886_CC_EN			(1 << 0)
#define PM886_CC_CLR_ON_RD		(1 << 2)
#define PM886_SD_PWRUP			(1 << 3)

#define PM886_CC_CONFIG2		(0x02)
#define PM886_CC_READ_REQ		(1 << 0)
#define PM886_OFFCOMP_EN		(1 << 1)

#define PM886_CC_VAL1			(0x03)
#define PM886_CC_VAL2			(0x04)
#define PM886_CC_VAL3			(0x05)
#define PM886_CC_VAL4			(0x06)
#define PM886_CC_VAL5			(0x07)

#define PM886_IBAT_VAL1			(0x08)		/* LSB */
#define PM886_IBAT_VAL2			(0x09)
#define PM886_IBAT_VAL3			(0x0a)

#define PM886_IBAT_EOC_CONFIG		(0x0f)
#define PM886_IBAT_MEAS_EN		(1 << 0)

#define PM886_IBAT_EOC_MEAS1		(0x10)		/* bit [7 : 0] */
#define PM886_IBAT_EOC_MEAS2		(0x11)		/* bit [15: 8] */

#define PM886_CC_LOW_TH1		(0x12)		/* bit [7 : 0] */
#define PM886_CC_LOW_TH2		(0x13)		/* bit [15 : 8] */

#define PM886_VBAT_AVG_MSB		(0xa0)
#define PM886_VBAT_AVG_LSB		(0xa1)

#define PM886_VBAT_SLP_MSB		(0xb0)
#define PM886_VBAT_SLP_LSB		(0xb1)

#define PM886_SLP_CNT1			(0xce)

#define PM886_SLP_CNT2			(0xcf)
#define PM886_SLP_CNT_HOLD		(1 << 6)
#define PM886_SLP_CNT_RST		(1 << 7)

#define MONITOR_INTERVAL		(HZ * 30)
#define LOW_BAT_INTERVAL		(HZ * 5)
#define LOW_BAT_CAP			(15)

/* this flag is used to decide whether the ocv_flag needs update */
static atomic_t in_resume = ATOMIC_INIT(0);

enum {
	ALL_SAVED_DATA,
	SLEEP_COUNT,
};

enum {
	VBATT_CHAN,
	VBATT_SLP_CHAN,
	TEMP_CHAN,
	MAX_CHAN = 3,
};

struct pm886_battery_params {
	int status;
	int present;
	int volt;	/* µV */
	int ibat;	/* mA */
	int soc;	/* percents: 0~100% */
	int health;
	int tech;
	int temp;
};

struct temp_vs_ohm {
	unsigned int ohm;
	int temp;
};

struct pm886_battery_info {
	struct pm886_chip	*chip;
	struct device	*dev;
	struct pm886_battery_params	bat_params;

	struct power_supply	battery;
	struct delayed_work	monitor_work;
	struct delayed_work	charged_work;
	struct delayed_work	cc_work; /* sigma-delta offset compensation */
	struct workqueue_struct *bat_wqueue;

	int			total_capacity;

	bool			use_ntc;
	int			gpadc_det_no;
	int			gpadc_temp_no;

	int			ocv_is_realiable;
	int			range_low_th;
	int			range_high_th;
	int			sleep_counter_th;

	int			power_off_th;
	int			safe_power_off_th;

	int			alart_percent;

	int			irq_nums;
	int			irq[7];

	struct iio_channel	*chan[MAX_CHAN];
	struct temp_vs_ohm	*temp_ohm_table;
	int			temp_ohm_table_size;
	int			zero_degree_ohm;

	int			abs_lowest_temp;
	int			t1;
	int			t2;
	int			t3;
	int			t4;
};

static int ocv_table[100];

static char *supply_interface[] = {
	"ac",
	"usb",
};

struct ccnt {
	int soc;	/* mC, 1mAH = 1mA * 3600 = 3600 mC */
	int max_cc;
	int last_cc;
	int cc_offs;
	int alart_cc;
};
static struct ccnt ccnt_data;

/*
 * the save_buffer mapping:
 *
 * | o o o o   o o | o o ||         o      | o o o   o o o o |
 * |<--- temp ---> |     ||ocv_is_realiable|      SoC        |
 * |---RTC_SPARE6(0xef)--||-----------RTC_SPARE5(0xee)-------|
 */
struct save_buffer {
	int soc;
	bool ocv_is_realiable;
	int temp;
};
static struct save_buffer extern_data;

#endif /* __88PM886_FG_H */
