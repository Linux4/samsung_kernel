#include <linux/bug.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/sort.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <mach/hardware.h>

#include <linux/irqflags.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/efuse.h>

#define REG_SYST_VALUE                  (SPRD_SYSCNT_BASE + 0x0004)

#define debug0(format, arg...) pr_debug("dcdc: " "@@@" format, ## arg)
#define debug(format, arg...) pr_info("dcdc: " "@@@" format, ## arg)
#define info(format, arg...) pr_info("dcdc: " "@@@" format, ## arg)

extern int in_calibration(void);
int sprd_get_adc_cal_type(void);
uint16_t sprd_get_adc_to_vol(uint16_t data);
int reguator_is_trimming(void *);
int regulator_set_trimming(struct regulator *, int, int);
int regulator_get_trimming_step(struct regulator *, int);

static uint32_t bat_numerators, bat_denominators = 0;

#define CALIBRATE_TO	(60 * 1)	/* one minute */
#define MEASURE_TIMES	(15)

static int cmp_val(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

int dcdc_adc_get(int adc_chan)
{
	int ret;
	u32 val[MEASURE_TIMES], adc_res = 0, adc_vol = 0;
	u32 chan_numerators, chan_denominators;
	uint32_t bat_numerators, bat_denominators;

	struct adc_sample_data adc_data = {
		.channel_id = adc_chan,
		.channel_type = 0,
		.hw_channel_delay = 0,
		.scale = true,
		.pbuf = &val[0],
		.sample_num = MEASURE_TIMES,
		.sample_bits = 1,
		.sample_speed = 0,
		.signal_mode = 0,
	};

	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
				  &bat_denominators);

	sci_adc_get_vol_ratio(adc_chan, true, &chan_numerators,
			      &chan_denominators);

	ret = sci_adc_get_values(&adc_data);
	if (0 != ret)
		goto exit;

	/*
	   for(i = 0; i < MEASURE_TIMES; i++) {
	   printk("%d\t", val[i]);
	   }
	   printk("\n");
	 */
	sort(val, MEASURE_TIMES, sizeof(u32), cmp_val, 0);

	adc_res = val[MEASURE_TIMES / 2];
	/* info("adc chan %d, value %d\n", adc_chan, adc_res); */
	adc_vol = sprd_get_adc_to_vol(adc_res) *
	    (bat_numerators * chan_denominators) /
	    (bat_denominators * chan_numerators);
exit:
	return adc_vol;
}

struct dcdc_cal_map {
	const char *name;	/* a-die DCDC or LDO name */
	int def_on;		/* 1: default ON, 0: default OFF */
	u32 def_vol;		/* default voltage (mV), could not read from a-die */
	u32 cal_sel;		/* only one ldo cal can be enable at the same time */
	int adc_chan;		/* multiplexed adc-channel id for LDOs */
};

struct dcdc_cal_map dcdc_cal_map[] = {
	{"vddcore", 1, 1100, 0, ADC_CHANNEL_DCDCCORE},
	{"vddarm", 1, 1200, 0, ADC_CHANNEL_DCDCARM},
	{"vddmem", 1, 1200, 0, ADC_CHANNEL_DCDCMEM},	/*DDR2 */
	{"vddmem1", 0, 1800, 0, ADC_CHANNEL_DCDCMEM},	/*DDR1 */
	{"dcdcldo", 1, 2200, 0, ADC_CHANNEL_DCDCLDO},

	{"vddrf", 1, 2850, BITS_LDO_RF_CAL_EN(-1), ADC_CHANNEL_LDO0},
	{"avddbb", 1, 3000, BITS_LDO_ABB_CAL_EN(-1), ADC_CHANNEL_LDO0},
	{"vddcama", 0, 2800, BITS_LDO_CAMA_CAL_EN(-1), ADC_CHANNEL_LDO0},

	{"vdd3v", 0, 3000, BITS_LDO_VDD3V_CAL_EN(-1), ADC_CHANNEL_LDO1},
	{"vdd28", 1, 2800, BITS_LDO_VDD28_CAL_EN(-1), ADC_CHANNEL_LDO1},
	{"vddsim0", 0, 1800, BITS_LDO_VSIM_CAL_EN(-1), ADC_CHANNEL_LDO1},
	{"vddsim1", 0, 1800, BITS_LDO_VSIM_CAL_EN(-1), ADC_CHANNEL_LDO1},
	{"vddcammot", 0, 2800, BITS_LDO_CAMMOT_CAL_EN(-1), ADC_CHANNEL_LDO1},
	{"vddsd0", 0, 2800, BITS_LDO_SD0_CAL_EN(-1), ADC_CHANNEL_LDO1},
	{"vddusb", 0, 3300, BITS_LDO_USB_CAL_EN(-1), ADC_CHANNEL_LDO1},
	{"vdd_a", 1, 1800, BITS_LDO_DVDD18_CAL_EN(-1), ADC_CHANNEL_LDO1},	/* alias dvdd18 */
	{"vdd25", 1, 2500, BITS_LDO_VDD25_CAL_EN(-1), ADC_CHANNEL_LDO1},

	{"vddcamio", 0, 1800, BITS_LDO_CAMIO_CAL_EN(-1), ADC_CHANNEL_LDO2},
	{"vddcamcore", 0, 1500, BITS_LDO_CAMCORE_CAL_EN(-1), ADC_CHANNEL_LDO2},
	{"vddcmmb1p2", 0, 1200, BITS_LDO_CMMB1P2_CAL_EN(-1), ADC_CHANNEL_LDO2},
	{"vddcmmb1p8", 0, 1800, BITS_LDO_CMMB1P8_CAL_EN(-1), ADC_CHANNEL_LDO2},
	{"vdd18", 1, 1800, BITS_LDO_VDD18_CAL_EN(-1), ADC_CHANNEL_LDO2},
	{"vddsd1", 0, 1800, BITS_LDO_SD1_CAL_EN(-1), ADC_CHANNEL_LDO2},
	{"vddsd3", 0, 1800, BITS_LDO_SD3_CAL_EN(-1), ADC_CHANNEL_LDO2},
};

int ldo_adc_get(const char *name)
{
	int i, adc_vol;
	for (i = 0; i < ARRAY_SIZE(dcdc_cal_map); i++) {
		if (0 == strcmp(name, dcdc_cal_map[i].name))
			break;
	}

	if (i >= ARRAY_SIZE(dcdc_cal_map))
		return -EINVAL;	/* not found */

	/* enable ldo cal before adc sampling and ldo calibration */
	if (0 != dcdc_cal_map[i].cal_sel) {
		sci_adi_write(ANA_REG_GLB2_LDO_TRIM_SEL,
			      dcdc_cal_map[i].cal_sel, -1);
	}

	adc_vol = dcdc_adc_get(dcdc_cal_map[i].adc_chan);

	/* close ldo cal */
	if (0 != dcdc_cal_map[i].cal_sel) {
		sci_adi_write(ANA_REG_GLB2_LDO_TRIM_SEL, 0, -1);
	}

	return adc_vol;
}

/* FIXME: sometime, adc vol is untrusted, not match the real voltage */
static int __check_adc_validity_one(const char *name, int ctl_vol)
{
	int ret = 0;
	int cal_vol, adc_vol = ldo_adc_get(name);
	cal_vol = abs(adc_vol - ctl_vol);
	info("check %s default %dmv, from %dmv, %c%d.%02d%%\n",
	     name, adc_vol, ctl_vol,
	     (adc_vol > ctl_vol) ? '+' : '-',
	     cal_vol * 100 / ctl_vol, cal_vol * 100 * 100 / ctl_vol % 100);
	ret = cal_vol < ctl_vol / 50;	/* margin 2% */
	return ret;
}

static int __check_adc_validity(void)
{
	int ret = 0;
	ret |= __check_adc_validity_one("vdd18", 1800);
	ret |= __check_adc_validity_one("vdd25", 2500);
	ret |= __check_adc_validity_one("vdd28", 2800);
	ret |= __check_adc_validity_one("vdd_a", 1800);
	ret |= __check_adc_validity_one("vddarm", 1200);
	ret |= __check_adc_validity_one("vddcore", 1100);
	return ret;
}

static int dcdc_calibrate(struct regulator *dcdc, int adc_chan,
			  const char *id, int def_vol, int to_vol, int is_cal)
{
	int ret = 0;
	int acc_vol, adc_vol = 0, ctl_vol, cal_vol = 0;

	if (0 != regulator_is_enabled(dcdc))
		adc_vol = dcdc_adc_get(adc_chan);

	cal_vol = abs(adc_vol - to_vol);

	info("%s default %dmv, from %dmv to %dmv, %c%d.%02d%%\n", __FUNCTION__,
	     def_vol, adc_vol, to_vol,
	     (adc_vol > to_vol) ? '+' : '-',
	     cal_vol * 100 / to_vol, cal_vol * 100 * 100 / to_vol % 100);

	if (!def_vol || !to_vol || !adc_vol)
		goto exit;

	if (cal_vol > to_vol / 10)	/* adjust limit 10% */
		goto exit;
	else if (cal_vol < to_vol / 100 && !is_cal) {	/* margin 1% */
		info("%s %s is ok\n", __FUNCTION__, id);
		return 0;
	}

	acc_vol = regulator_get_trimming_step(dcdc, to_vol);

	/* always set valid vol ctrl bits */
	ctl_vol = DIV_ROUND_UP(def_vol * to_vol, adc_vol) + acc_vol;
	ret = regulator_set_trimming(dcdc, ctl_vol * 1000, to_vol * 1000);
	if (IS_ERR_VALUE(ret))
		goto exit;

	WARN(!is_cal,
	     "warning: regulator (%s) voltage ctrl %dmv\n", id, ctl_vol);
	return ctl_vol;

exit:
	info("%s %s failure\n", __FUNCTION__, id);
	return -1;
}

int sci_dcdc_calibrate(const char *name, int def_vol, int to_vol)
{
	int i, ret, adc_chan, ctl_vol;
	struct regulator *dcdc;
	static int is_ddr2 = 0;
	if (bat_denominators == 0) {
		u32 chan_numerators, chan_denominators;
		sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
				      &bat_denominators);
		sci_adc_get_vol_ratio(ADC_CHANNEL_DCDCCORE, true,
				      &chan_numerators, &chan_denominators);
		is_ddr2 = 0 == (sci_adi_read(ANA_REG_GLB_ANA_STATUS) & BIT(6));
		debug
		    ("vbat cal type %d, chan scale ratio %u/%u, and dcdc %u/%u, %s\n",
		     sprd_get_adc_cal_type(), bat_numerators, bat_denominators,
		     chan_numerators, chan_denominators,
		     is_ddr2 ? "ddr2" : "ddr");

		__check_adc_validity();
	}

	if (!is_ddr2 && 0 == strcmp(name, "vddmem"))
		name = "vddmem1";

	for (i = 0; i < ARRAY_SIZE(dcdc_cal_map); i++) {
		if (0 == strcmp(name, dcdc_cal_map[i].name))
			break;
	}

	if (i >= ARRAY_SIZE(dcdc_cal_map))
		return -EINVAL;	/* not found */

	dcdc = regulator_get(0, name);
	if (IS_ERR(dcdc))
		goto exit;

	if (reguator_is_trimming(regulator_get_drvdata(dcdc)))
		goto exit;	/* already trimming, do nothing */

	adc_chan = dcdc_cal_map[i].adc_chan;
	ctl_vol = regulator_get_voltage(dcdc);
	if (IS_ERR_VALUE(ctl_vol)) {
		debug0("no valid %s vol ctrl bits\n", name);
	} else			/* dcdc maybe had been adjusted in uboot-spl */
		ctl_vol /= 1000;

	if (!def_vol)
		def_vol =
		    (IS_ERR_VALUE(ctl_vol)) ? dcdc_cal_map[i].def_vol : ctl_vol;

	if (!to_vol)
		to_vol =
		    (IS_ERR_VALUE(ctl_vol)) ? dcdc_cal_map[i].def_vol : ctl_vol;

	/* enable ldo cal before adc sampling and ldo calibration */
	if (0 != dcdc_cal_map[i].cal_sel) {
		sci_adi_write(ANA_REG_GLB2_LDO_TRIM_SEL,
			      dcdc_cal_map[i].cal_sel, -1);
	}

	ret = dcdc_calibrate(dcdc, adc_chan, name, def_vol, to_vol, 1);
	debug0("dcdc_calibrate return %d\n", ret);
	if (ret >= 0) {
		msleep(10);	/* wait a moment before cal verify */
		ret = dcdc_calibrate(dcdc, adc_chan, name,
				     (0 == ret) ? def_vol : ret, to_vol, 0);
		if (0 == ret) {	/* cal is ok, do not cal any more */
			dcdc_cal_map[i].def_on = 0;
		}
	}

	/* close ldo cal */
	if (0 != dcdc_cal_map[i].cal_sel) {
		sci_adi_write(ANA_REG_GLB2_LDO_TRIM_SEL, 0, -1);
	}

exit:
	regulator_put(dcdc);
	return 0;
}

int mpll_calibrate(int cpu_freq)
{
	u32 val = 0;
	unsigned long flags;
	//BUG_ON(cpu_freq != 1200);     /* only upgrade 1.2G */
	cpu_freq /= 4;
	local_irq_save(flags);
	val = sci_glb_raw_read(REG_GLB_M_PLL_CTL0);
	if ((val & MASK_MPLL_N) == cpu_freq)
		goto exit;
	val = (val & ~MASK_MPLL_N) | cpu_freq;
	sci_glb_set(REG_GLB_GEN1, BIT_MPLL_CTL_WE);	/* mpll unlock */
	sci_glb_write(REG_GLB_M_PLL_CTL0, val, MASK_MPLL_N);
	sci_glb_clr(REG_GLB_GEN1, BIT_MPLL_CTL_WE);
exit:
	local_irq_restore(flags);
	debug("%s 0x%08x\n", __FUNCTION__, val);
	return 0;
}

struct dcdc_delayed_work {
	struct delayed_work work;
	u32 uptime;
	int cal_typ;
};

static struct dcdc_delayed_work dcdc_work = {
	.work.work.func = NULL,
	.uptime = 0,
	.cal_typ = -1,		/* Invalid */
};

static u32 sci_syst_read(void)
{
	u32 t = __raw_readl(REG_SYST_VALUE);
	while (t != __raw_readl(REG_SYST_VALUE))
		t = __raw_readl(REG_SYST_VALUE);
	return t;
}

static void do_dcdc_work(struct work_struct *work)
{
	int i, cnt = CALIBRATE_TO;

	/* debug("%s %d\n", __FUNCTION__, sprd_get_adc_cal_type()); */
	if (dcdc_work.cal_typ == sprd_get_adc_cal_type())
		goto exit;	/* no change, set next delayed work */

	dcdc_work.cal_typ = sprd_get_adc_cal_type();
	debug0("%s %d %d\n", __FUNCTION__, dcdc_work.cal_typ, cnt);

	/* four DCDCs and all LDOs Trimming if default ON */
	for (i = 0; i < ARRAY_SIZE(dcdc_cal_map); i++) {
		if (dcdc_cal_map[i].def_on)
			sci_dcdc_calibrate(dcdc_cal_map[i].name, 0, 0);
	}

exit:
	if (sci_syst_read() - dcdc_work.uptime < CALIBRATE_TO * 1000) {
		schedule_delayed_work(&dcdc_work.work, msecs_to_jiffies(1000));
	} else {
		debug0("%s end.\n", __FUNCTION__);
	}

	return;
}

void dcdc_calibrate_callback(void *data)
{
	if (!dcdc_work.work.work.func) {
		INIT_DELAYED_WORK(&dcdc_work.work, do_dcdc_work);
		dcdc_work.uptime = sci_syst_read();
	}

	dcdc_work.cal_typ = (int)data;
	schedule_delayed_work(&dcdc_work.work, msecs_to_jiffies(1000));
}

int ldo_trimming_callback(void *data)
{
	int i, ret = 0;
	const char *name = data;
	for (i = 0; i < ARRAY_SIZE(dcdc_cal_map); i++) {
		if (0 == strcmp(name, dcdc_cal_map[i].name))
			break;
	}

	if (i >= ARRAY_SIZE(dcdc_cal_map))
		return -EINVAL;	/* not found */

	if (!dcdc_cal_map[i].def_on) {
		dcdc_cal_map[i].def_on = 1;
		debug0("%s trimming ...\n", name);
	}

	if (dcdc_work.cal_typ >= 0)
		dcdc_calibrate_callback(0);
	return ret;
}

static int __init dcdc_init(void)
{
	if (!in_calibration())	/* bypass if in CFT */
		dcdc_calibrate_callback(0);
	return 0;
}

late_initcall(dcdc_init);
