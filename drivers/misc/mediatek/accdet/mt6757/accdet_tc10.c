/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define DEBUG
/*
 *
 * Filename:
 * ---------
 *   accdet.c
 *
 * Description:
 * ------------
 *   This file handles accdet configuration and data flow
 *   in differently with the different chip
 *
 */

#include "accdet.h"
#include <upmu_common.h>
#include <linux/timer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#if defined(CONFIG_SEC_JACK_EXTENSION)
#include <sec_jack_ex.h>
#endif

#if (defined(CONFIG_MTK_PMIC_CHIP_MT6355)) || (defined(CONFIG_MTK_TC10_FEATURE))
/* fix record no voice when press key */
#include "mtk-soc-codec-63xx.h"
#endif

#if (defined(CONFIG_MTK_TC10_FEATURE))
#ifdef CONFIG_ACCDET_EINT_IRQ
#define CONFIG_ACCDET_EINT
#else
#define CONFIG_ACCDET_EINT_IRQ
#endif
#include <mtk_boot.h>/* for support multi-HW rev */
#endif

#ifdef CONFIG_ACCDET_EINT
#include <linux/gpio.h>
#endif


#define DEBUG_THREAD 1
/*
 * static variable defination
 */

#ifdef CONFIG_MTK_TC10_FEATURE/* for titan */
/* Just for  NO use comp. with AB on type judge */
#define NO_USE_COMPARATOR	1
#else
#define NO_USE_COMPARATOR	0
#endif

#if NO_USE_COMPARATOR
/* for headset pole type definition  */
#define TYPE_AB_00		(0x00)/* 3-pole or hook_switch */
#define TYPE_AB_01		(0x01)/* 4-pole */
#define TYPE_AB_11		(0x03)/* plug-out */
#define TYPE_AB_10		(0x02)/* Illegal state */
struct Vol_Set {/* mv */
	unsigned int vol_min_3pole;
	unsigned int vol_max_3pole;
	unsigned int vol_min_4pole;
	unsigned int vol_max_4pole;
	unsigned int vol_bias;/* >2500: 2800; others: 2500 */
};
static struct Vol_Set cust_vol_set;
#endif

enum accdet_hw_rev {
	HW_V_00 = 0,
	HW_V_01,
	HW_V_02,
	HW_V_03,
	HW_V_04,
	HW_V_05,
	HW_V_06,
	HW_V_07
};

#if (defined(CONFIG_MTK_TC10_FEATURE))
static int hw_rev = HW_V_00;/* default support hw rev00 */
#else/* not titan */
#ifdef CONFIG_ACCDET_EINT_IRQ
static int hw_rev = HW_V_01;/* init hw rev01 */
#else
static int hw_rev = HW_V_00;/* init hw rev00 */
#endif
#endif

static int hw_mode_support;/* support hw path mode */

#define ACCDET_INIT_WAIT_TIMER   (10 * HZ)	/* 10 seconds */
/* for accdet_read_audio_res */
#define RET_LT_5K		-1/* less than 5k ohm, return -1 */
#define RET_GT_5K		0/* greater than 5k ohm, return 0 */

#define REGISTER_VALUE(x)   (x - 1)
static int button_press_debounce = 0x3c0; /* about 30ms */
int cur_key;
struct head_dts_data accdet_dts_data;
int accdet_auxadc_offset;
int accdet_irq;
unsigned int gpiopin, headsetdebounce;
unsigned int accdet_eint_type = IRQ_TYPE_LEVEL_LOW;/* default low_level trigger */
struct headset_mode_settings *cust_headset_settings;
#define ACCDET_DEBUG(format, args...) pr_debug(format, ##args)
#define ACCDET_INFO(format, args...) pr_warn(format, ##args)
#define ACCDET_ERROR(format, args...) pr_err(format, ##args)

static struct input_dev *kpd_accdet_dev;
static struct cdev *accdet_cdev;
static struct class *accdet_class;
static struct device *accdet_nor_device;
static dev_t accdet_devno;
static int pre_status;
static int pre_state_swctrl;
static int accdet_status = PLUG_OUT;
static int cable_type = NO_DEVICE;
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
/* add for new feature PIN recognition */
static int cable_pin_recognition;
static int show_icon_delay;
#endif

#define KEY_SAMPLE_PERIOD        (60)	/* ms */
#define MULTIKEY_ADC_CHANNEL	 (8)
static DEFINE_MUTEX(accdet_multikey_mutex);
#define NO_KEY			 (0x0)
#define UP_KEY			 (0x01)
#define MD_KEY			(0x02)
#define DW_KEY			 (0x04)
#define AS_KEY			 (0x08)

#if DEBUG_THREAD
static int g_start_debug_thread;
static struct task_struct *thread;
static int g_dump_register;
#endif

static int eint_accdet_sync_flag;
static atomic_t g_accdet_first = ATOMIC_INIT(1);
static bool IRQ_CLR_FLAG;
static int call_status;
static int button_status;
struct wake_lock accdet_suspend_lock;
struct wake_lock accdet_irq_lock;
struct wake_lock accdet_key_lock;
struct wake_lock accdet_timer_lock;
static struct work_struct accdet_work;
static struct workqueue_struct *accdet_workqueue;
static DEFINE_MUTEX(accdet_eint_irq_sync_mutex);
static inline void clear_accdet_interrupt(void);
static inline void clear_accdet_eint_interrupt(void);
static void send_key_event(int keycode, int flag);
static inline void check_cable_type(void);
static int dump_register(void);

static struct work_struct accdet_eint_work;
static struct workqueue_struct *accdet_eint_workqueue;
static inline void accdet_init(void);
#ifndef CONFIG_MTK_TC10_FEATURE/* for titan: remove 6secs  timer*/
#define MICBIAS_DISABLE_TIMER   (6 * HZ)	/* 6 seconds */
struct timer_list micbias_timer;
struct timer_list  accdet_init_timer;

static void disable_micbias(unsigned long a);
static struct work_struct accdet_disable_work;
static struct workqueue_struct *accdet_disable_workqueue;
#endif

/* Used to let accdet know if the pin has been fully plugged-in */
#define EINT_PIN_PLUG_IN        (1)
#define EINT_PIN_PLUG_OUT       (0)
int cur_eint_state = EINT_PIN_PLUG_OUT;

#ifdef CONFIG_ACCDET_EINT
struct pinctrl *accdet_pinctrl1;
struct pinctrl_state *pins_eint_int;
#endif

static u32 pmic_pwrap_read(u32 addr);
static void pmic_pwrap_write(u32 addr, unsigned int wdata);
#if defined(CONFIG_SEC_JACK_EXTENSION)
static struct jack_device_priv earjack_priv;
#endif
char *accdet_status_string[5] = {
	"Plug_out",
	"Headset_plug_in",
	/* "Double_check", */
	"Hook_switch",
	"External_cable",
	/*"Tvout_plug_in",*/
	"Stand_by"
};
char *accdet_report_string[4] = {
	"No_device",
	"Headset_mic",
	"Headset_no_mic",
	"Headset_five_pole",
	/* "HEADSET_illegal", */
	/* "Double_check" */
};

static void send_accdet_status_event(int cable, int status);
/*
 * export function
 */
/* get plug-in Resister just for audio call */
int accdet_read_audio_res(unsigned int res_value)
{
	ACCDET_DEBUG("[accdet_read_audio_res]resister value: R=%u(ohm)\n", res_value);
	/* res < 5k ohm normal device; res >= 5k ohm, lineout device */
	if (res_value < 5000)
		return RET_LT_5K;

	mutex_lock(&accdet_eint_irq_sync_mutex);
	if (eint_accdet_sync_flag == 1) {
		cable_type = LINE_OUT_DEVICE;
		accdet_status = LINE_OUT;
		/* update state */
		send_accdet_status_event(cable_type, 1);
		ACCDET_DEBUG("[accdet_read_audio_res]update state:%d\n", cable_type);
	}
	mutex_unlock(&accdet_eint_irq_sync_mutex);

	return RET_GT_5K;
}
EXPORT_SYMBOL(accdet_read_audio_res);

void accdet_detect(void)
{
	int ret = 0;

	ACCDET_DEBUG("[Accdet]accdet_detect\n");

	accdet_status = PLUG_OUT;
	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		ACCDET_DEBUG("[Accdet]accdet_detect:accdet_work return:%d!\n", ret);
}
EXPORT_SYMBOL(accdet_detect);

void accdet_state_reset(void)
{

	ACCDET_DEBUG("[Accdet]accdet_state_reset\n");

	accdet_status = PLUG_OUT;
	cable_type = NO_DEVICE;
}
EXPORT_SYMBOL(accdet_state_reset);

int accdet_get_cable_type(void)
{
	return cable_type;
}

void accdet_auxadc_switch(int enable)
{
	if (enable) {
		pmic_pwrap_write(ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG) | ACCDET_BF_ON);
		/* ACCDET_DEBUG("ACCDET enable switch\n"); */
	} else {
		pmic_pwrap_write(ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG) & ~(ACCDET_BF_ON));
		/* ACCDET_DEBUG("ACCDET disable switch\n"); */
	}
}

/*
 * static function defination
 */
static u64 accdet_get_current_time(void)
{
	return sched_clock();
}

static bool accdet_timeout_ns(u64 start_time_ns, u64 timeout_time_ns)
{
	u64 cur_time = 0;
	u64 elapse_time = 0;

	/* get current tick */
	cur_time = accdet_get_current_time();	/* ns */
	if (cur_time < start_time_ns) {
		ACCDET_DEBUG("@@@@Timer overflow! start%lld cur timer%lld\n", start_time_ns, cur_time);
		start_time_ns = cur_time;
		timeout_time_ns = 400 * 1000;	/* 400us */
		ACCDET_DEBUG("@@@@reset timer! start%lld setting%lld\n", start_time_ns, timeout_time_ns);
	}
	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time) {
		/* timeout */
		ACCDET_DEBUG("@@@@ACCDET IRQ clear Timeout\n");
		return false;
	}
	return true;
}

/* pmic wrap read and write func */
static u32 pmic_pwrap_read(u32 addr)
{
	u32 val = 0;

	pwrap_read(addr, &val);
	return val;
}

static void pmic_pwrap_write(unsigned int addr, unsigned int wdata)
{
	pwrap_write(addr, wdata);
}

static int Accdet_PMIC_IMM_GetOneChannelValue(int deCount)
{
	unsigned int reg_val = 0;
	unsigned int vol_val = 0;

	if (2 == deCount)
		mdelay(10);
	pmic_pwrap_write(ACCDET_AUXADC_CTL_SET, ACCDET_CH_REQ_EN);
	if (2 == deCount)
		mdelay(3);
	reg_val = pmic_pwrap_read(ACCDET_AUXADC_REG);
	while ((reg_val & ACCDET_DATA_READY) != ACCDET_DATA_READY)
		reg_val = pmic_pwrap_read(ACCDET_AUXADC_REG);

	vol_val = (reg_val & ACCDET_DATA_MASK);
	vol_val = (vol_val * 1800) / 4096;	/* mv */
	vol_val -= accdet_auxadc_offset;
	ACCDET_INFO("accdet_auxadc_offset: %d mv, MIC_Voltage1 = %d mv!\n\r", accdet_auxadc_offset, vol_val);
	return vol_val;
}

#ifdef CONFIG_ACCDET_PIN_SWAP

static void accdet_FSA8049_enable(void)
{
	mt_set_gpio_mode(GPIO_FSA8049_PIN, GPIO_FSA8049_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_FSA8049_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_FSA8049_PIN, GPIO_OUT_ONE);
}

static void accdet_FSA8049_disable(void)
{
	mt_set_gpio_mode(GPIO_FSA8049_PIN, GPIO_FSA8049_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_FSA8049_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_FSA8049_PIN, GPIO_OUT_ZERO);
}

#endif
static inline void headset_plug_out(void)
{
	/* send input event before switch cable_type to NO_device, to ensure cable_type valid */
	send_accdet_status_event(cable_type, 0);

	accdet_status = PLUG_OUT;
	cable_type = NO_DEVICE;
	/* update the cable_type */
	if (cur_key != 0) {
#if defined(CONFIG_SEC_JACK_EXTENSION)
		earjack_priv.jack.button_det = false;
		earjack_priv.jack.adc_val = 0;
#endif
		send_key_event(cur_key, 0);
		ACCDET_INFO(" [accdet] headset_plug_out send key = %d release\n", cur_key);
		cur_key = 0;
	}
#if defined(CONFIG_SEC_JACK_EXTENSION)
	earjack_priv.jack.jack_det = cable_type;
#endif
	ACCDET_INFO(" [accdet] set state in cable_type = NO_DEVICE\n");

}

/* Accdet only need this func */
static inline void enable_accdet(u32 state_swctrl)
{
	/* enable ACCDET unit */
	ACCDET_DEBUG("accdet: enable_accdet\n");
	/* enable clock */
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, pmic_pwrap_read(ACCDET_STATE_SWCTRL) | state_swctrl);
	pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) | ACCDET_ENABLE);

}

static inline void disable_accdet(void)
{
	int irq_temp = 0;

/*
 * sync with accdet_irq_handler set clear accdet irq bit
 * to avoid  set clear accdet irq bit after disable accdet irq
 */
	pmic_pwrap_write(INT_CON_ACCDET_CLR, RG_ACCDET_IRQ_CLR);
	clear_accdet_interrupt();
	udelay(200);
	mutex_lock(&accdet_eint_irq_sync_mutex);
	while (pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT) {
		ACCDET_INFO("[Accdet]check_cable_type: Clear interrupt on-going....\n");
		msleep(20);
	}
	irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
	irq_temp = irq_temp & (~IRQ_CLR_BIT);
	pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
	mutex_unlock(&accdet_eint_irq_sync_mutex);
	/* disable ACCDET unit */
	ACCDET_DEBUG("accdet: disable_accdet\n");
	pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
#ifdef CONFIG_ACCDET_EINT
	if (hw_rev == HW_V_00) {
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0);
		pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
		/* disable clock and Analog control */
		/* mt6331_upmu_set_rg_audmicbias1vref(0x0); */
		pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
	}
#endif
#ifdef CONFIG_ACCDET_EINT_IRQ
	if (hw_rev == HW_V_01) {
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, ACCDET_EINT_PWM_EN);
		pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) & (~(ACCDET_ENABLE)));
		/* mt_set_gpio_out(GPIO_CAMERA_2_CMRST_PIN, GPIO_OUT_ZERO); */

		if (hw_mode_support) {/* close HW path */
			pmic_pwrap_write(ACCDET_HW_MODE_DFF, 0x8000);
		}
	}
#endif

}

#if NO_USE_COMPARATOR
static unsigned int check_pole_type(void)
{
	unsigned int vol = 0;

	vol = Accdet_PMIC_IMM_GetOneChannelValue(2);

	if ((vol < (cust_vol_set.vol_max_4pole + 1)) && (vol > (cust_vol_set.vol_min_4pole - 1))) {
		ACCDET_INFO("[accdet] 4pole check:%d mv, AB=%d\n", vol, TYPE_AB_01);
		return TYPE_AB_01;
	} else if ((vol < (cust_vol_set.vol_max_3pole + 1)) && (vol > cust_vol_set.vol_min_3pole)) {
		ACCDET_INFO("[accdet] 3pole check:%d mv, AB=%d\n", vol, TYPE_AB_00);
		return TYPE_AB_00;
	}
	/* external cable support */
	ACCDET_INFO("[accdet] ext_cable pole check:%d mv, AB=%d\n", vol, TYPE_AB_10);
	return TYPE_AB_10;
	/* illegal state */
	/* ACCDET_INFO("[accdet] error: pole check:%d mv, AB=%d\n", vol, TYPE_AB_11); */
}
#endif

#ifndef CONFIG_MTK_TC10_FEATURE/* for titan */
static void disable_micbias(unsigned long a)
{
	int ret = 0;

	ret = queue_work(accdet_disable_workqueue, &accdet_disable_work);
	if (!ret)
		ACCDET_ERROR("[Accdet]disable_micbias:accdet_work return:%d!\n", ret);
}

static void disable_micbias_callback(struct work_struct *work)
{
	if (cable_type == HEADSET_NO_MIC) {
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 0;
		cable_pin_recognition = 0;
		ACCDET_DEBUG("[Accdet] cable_pin_recognition = %d\n", cable_pin_recognition);
		pmic_pwrap_write(ACCDET_PWM_WIDTH, cust_headset_settings->pwm_width);
		pmic_pwrap_write(ACCDET_PWM_THRESH, cust_headset_settings->pwm_thresh);
#endif
		/* setting pwm idle; */
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, pmic_pwrap_read(ACCDET_STATE_SWCTRL) & ~ACCDET_SWCTRL_IDLE_EN);
#ifdef CONFIG_ACCDET_PIN_SWAP
		/* accdet_FSA8049_disable(); disable GPIOxxx for PIN swap */
		/* ACCDET_DEBUG("[Accdet] FSA8049 disable!\n"); */
#endif
		disable_accdet();
		ACCDET_INFO("[Accdet] more than 5s MICBIAS : Disabled\n");
	}
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
	else if (cable_type == HEADSET_MIC) {
		pmic_pwrap_write(ACCDET_PWM_WIDTH, cust_headset_settings->pwm_width);
		pmic_pwrap_write(ACCDET_PWM_THRESH, cust_headset_settings->pwm_thresh);
		ACCDET_DEBUG("[Accdet]pin recog after 5s recover micbias polling!\n");
	}
#endif
}
#endif

static void accdet_eint_work_callback(struct work_struct *work)
{
	unsigned int tmp_reg = 0;
	int irq_temp = 0;

	tmp_reg = 0;
	irq_temp = 0;
#ifdef CONFIG_ACCDET_EINT_IRQ
if (hw_rev == HW_V_01) {
	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		ACCDET_INFO("[Accdet]DCC EINT func :plug-in, cur_eint_state = %d\n", cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 1;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		wake_lock_timeout(&accdet_timer_lock, 7 * HZ);
#ifdef CONFIG_ACCDET_PIN_SWAP
		/* pmic_pwrap_write(0x0400, pmic_pwrap_read(0x0400)|(1<<14)); */
		msleep(800);
		accdet_FSA8049_enable();	/* enable GPIOxxx for PIN swap */
		ACCDET_DEBUG("[Accdet] FSA8049 enable!\n");
		msleep(250);	/* PIN swap need ms */
#endif
		accdet_init();	/* do set pwm_idle on in accdet_init */
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 1;
		/* micbias always on during detected PIN recognition */
		pmic_pwrap_write(ACCDET_PWM_WIDTH, cust_headset_settings->pwm_width);
		pmic_pwrap_write(ACCDET_PWM_THRESH, cust_headset_settings->pwm_width);
		ACCDET_DEBUG("[Accdet]pin recog start!  micbias always on!\n");
#endif

#if NO_USE_COMPARATOR
		/* enable mbias for detect voltage */
		pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) | ACCDET_ENABLE);
		pmic_pwrap_write(ACCDET_STATE_SWCTRL,
			(pmic_pwrap_read(ACCDET_STATE_SWCTRL)|ACCDET_CMP_EN|ACCDET_MICBIA_EN));
		mdelay(60);/* may be need delay more, relevant to Bias vol. */
		check_cable_type();
		send_accdet_status_event(cable_type, 1);/* report plug-state */
		ACCDET_INFO("[Accdet]report headset state[%d]=%d\n", accdet_status, cable_type);
#endif

		/* support mt6355, default open HW mode */
		if (hw_mode_support) {
			/* enable HW path */
			pmic_pwrap_write(ACCDET_HW_MODE_DFF, 0x8000|ACCDET_HWMODE_SEL|ACCDET_EINT_DEB_OUT_DFF);
			/* enable cmp,vth,bias, CLK */
			pmic_pwrap_write(ACCDET_STATE_SWCTRL, pmic_pwrap_read(ACCDET_STATE_SWCTRL) | ACCDET_SWCTRL_EN);
		} else {
			/* pmic_pwrap_write(ACCDET_HW_MODE_DFF, 0x8000); *//* SW path */
			/* sw path set PWM IDLE  on */
			pmic_pwrap_write(ACCDET_STATE_SWCTRL,
				(pmic_pwrap_read(ACCDET_STATE_SWCTRL) | ACCDET_SWCTRL_IDLE_EN));
			/* enable ACCDET unit */
			enable_accdet(ACCDET_SWCTRL_EN);
		}
	} else {
/* EINT_PIN_PLUG_OUT */
/* Disable ACCDET */
		ACCDET_INFO("[Accdet]DCC EINT func :plug-out, cur_eint_state = %d\n", cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 0;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifndef CONFIG_MTK_TC10_FEATURE/* for titan */
		del_timer_sync(&micbias_timer);
#endif
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 0;
		cable_pin_recognition = 0;
#endif
#ifdef CONFIG_ACCDET_PIN_SWAP
		/*pmic_pwrap_write(0x0400, pmic_pwrap_read(0x0400)&~(1<<14)); */
		accdet_FSA8049_disable();	/*disable GPIOxxx for PIN swap */
		ACCDET_DEBUG("[Accdet] FSA8049 disable!\n");
#endif
		/*accdet_auxadc_switch(0);*/
		disable_accdet();
		headset_plug_out();
		/*recover EINT irq clear bit */
		/*TODO: need think~~~*/
		irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
		irq_temp = irq_temp & (~IRQ_EINT_CLR_BIT);
		pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
	}
}
#endif

#ifdef CONFIG_ACCDET_EINT
if (hw_rev == HW_V_00) {
	/*KE under fastly plug in and plug out*/
	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		ACCDET_INFO("[Accdet]ACC EINT func :plug-in, cur_eint_state = %d\n", cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 1;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		wake_lock_timeout(&accdet_timer_lock, 7 * HZ);
#ifdef CONFIG_ACCDET_PIN_SWAP
		/*pmic_pwrap_write(0x0400, pmic_pwrap_read(0x0400)|(1<<14)); */
		msleep(800);
		accdet_FSA8049_enable();	/*enable GPIOxxx for PIN swap */
		ACCDET_DEBUG("[Accdet] FSA8049 enable!\n");
		msleep(250);	/*PIN swap need ms */
#endif
		accdet_init();	/* do set pwm_idle on in accdet_init*/

#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 1;
		/*micbias always on during detected PIN recognition*/
		pmic_pwrap_write(ACCDET_PWM_WIDTH, cust_headset_settings->pwm_width);
		pmic_pwrap_write(ACCDET_PWM_THRESH, cust_headset_settings->pwm_width);
		ACCDET_DEBUG("[Accdet]pin recog start!  micbias always on!\n");
#endif
		/*set PWM IDLE  on*/
		tmp_reg = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
		tmp_reg = tmp_reg | ACCDET_SWCTRL_EN | ACCDET_SWCTRL_IDLE_EN;
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, tmp_reg);
#if NO_USE_COMPARATOR
		pmic_pwrap_write(ACCDET_PWM_WIDTH, cust_headset_settings->pwm_width);
		pmic_pwrap_write(ACCDET_PWM_THRESH, cust_headset_settings->pwm_width);
		/* enable mbias for detect voltage */
		pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) | ACCDET_ENABLE);
		pmic_pwrap_write(ACCDET_STATE_SWCTRL,
			(pmic_pwrap_read(ACCDET_STATE_SWCTRL)|ACCDET_CMP_EN|ACCDET_MICBIA_EN));
		mdelay(60);/* may be need delay more, relevant to Bias vol. */
		check_cable_type();
#if defined(CONFIG_SEC_JACK_EXTENSION)
		earjack_priv.jack.jack_det = cable_type;
#endif
		send_accdet_status_event(cable_type, 1);/* report plug-state */
		ACCDET_INFO("[Accdet]report headset state[%d]=%d\n", accdet_status, cable_type);
#endif
		/*enable ACCDET unit*/
		enable_accdet(ACCDET_SWCTRL_EN);
	} else {
/*EINT_PIN_PLUG_OUT*/
/*Disable ACCDET*/
		ACCDET_INFO("[Accdet]ACC EINT func:plug-out, cur_eint_state = %d\n", cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 0;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifndef CONFIG_MTK_TC10_FEATURE/* for titan */
		del_timer_sync(&micbias_timer);
#endif
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		show_icon_delay = 0;
		cable_pin_recognition = 0;
#endif
#ifdef CONFIG_ACCDET_PIN_SWAP
		/*pmic_pwrap_write(0x0400, pmic_pwrap_read(0x0400)&~(1<<14));*/
		accdet_FSA8049_disable();	/*disable GPIOxxx for PIN swap*/
		ACCDET_DEBUG("[Accdet] FSA8049 disable!\n");
#endif
		/*accdet_auxadc_switch(0);*/
		disable_accdet();
		headset_plug_out();
	}
	enable_irq(accdet_irq);
	ACCDET_DEBUG("[Accdet]enable_irq  !!!!!!\n");
}
#endif
}

static irqreturn_t accdet_eint_func(int irq, void *data)
{
	int ret = 0;

	ACCDET_INFO("[Accdet]Enter accdet_eint_func !!!!!!\n");
	if (cur_eint_state == EINT_PIN_PLUG_IN) {
/*
 * To trigger EINT when the headset was plugged in
 * We set the polarity back as we initialed.
 */
#ifdef CONFIG_ACCDET_EINT
	if (hw_rev == HW_V_00) {
		if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_HIGH);
		else
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_LOW);
		gpio_set_debounce(gpiopin, headsetdebounce);
	}
#endif
#ifdef CONFIG_ACCDET_EINT_IRQ
	if (hw_rev == HW_V_01) {
		pmic_pwrap_write(ACCDET_EINT_CTL, pmic_pwrap_read(ACCDET_EINT_CTL) & EINT_PLUG_DEB_CLR);
		/* debounce=256ms */
		pmic_pwrap_write(ACCDET_EINT_CTL, pmic_pwrap_read(ACCDET_EINT_CTL) | EINT_IRQ_DE_IN);
		pmic_pwrap_write(ACCDET_DEBOUNCE3, cust_headset_settings->debounce3);
	}
#endif
		/* update the eint status */
		cur_eint_state = EINT_PIN_PLUG_OUT;
	} else {
/*
 * To trigger EINT when the headset was plugged out
 * We set the opposite polarity to what we initialed.
 */
#ifdef CONFIG_ACCDET_EINT
	if (hw_rev == HW_V_00) {
		if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_LOW);
		else
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_HIGH);
	/* gpio_set_debounce(gpiopin, accdet_dts_data.accdet_plugout_debounce * 1000); */
		gpio_set_debounce(gpiopin, 16000);/* 16ms */
	}
#endif

#ifdef CONFIG_ACCDET_EINT_IRQ
	if (hw_rev == HW_V_01) {
		pmic_pwrap_write(ACCDET_EINT_CTL, pmic_pwrap_read(ACCDET_EINT_CTL) & EINT_PLUG_DEB_CLR);
		pmic_pwrap_write(ACCDET_EINT_CTL, pmic_pwrap_read(ACCDET_EINT_CTL) | EINT_IRQ_DE_OUT);
	}
#endif
		/* update the eint status */
		cur_eint_state = EINT_PIN_PLUG_IN;
#ifndef CONFIG_MTK_TC10_FEATURE/* for titan */
		mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER);
#endif
	}
#ifdef CONFIG_ACCDET_EINT
	if (hw_rev == HW_V_00)
		disable_irq_nosync(accdet_irq);
#endif
	ACCDET_INFO("[Accdet]accdet_eint_func after cur_eint_state=%d\n", cur_eint_state);

	ret = queue_work(accdet_eint_workqueue, &accdet_eint_work);
	return IRQ_HANDLED;
}

#ifdef CONFIG_ACCDET_EINT
static inline int accdet_setup_eint(struct platform_device *accdet_device)
{
	int ret = 0;
	u32 ints[2] = { 0, 0 };
	u32 ints1[2] = { 0, 0 };
	struct device_node *node = NULL;
	struct pinctrl_state *pins_default;

	/* configure to GPIO function, external interrupt */
	ACCDET_INFO("[Accdet]accdet_setup_eint\n");
	accdet_pinctrl1 = devm_pinctrl_get(&accdet_device->dev);
	if (IS_ERR(accdet_pinctrl1)) {
		ret = PTR_ERR(accdet_pinctrl1);
		dev_info(&accdet_device->dev, "[Accdet]Cannot find accdet_pinctrl1!\n");
		return ret;
	}

	pins_default = pinctrl_lookup_state(accdet_pinctrl1, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		/*dev_info(&accdet_device->dev, "fwq Cannot find accdet pinctrl default!\n");*/
	}

	pins_eint_int = pinctrl_lookup_state(accdet_pinctrl1, "state_eint_as_int");
	if (IS_ERR(pins_eint_int)) {
		ret = PTR_ERR(pins_eint_int);
		dev_info(&accdet_device->dev, "[Accdet]Cannot find pinctrl state_eint_accdet!\n");
		return ret;
	}
	if (hw_rev == HW_V_00) {
		pinctrl_select_state(accdet_pinctrl1, pins_eint_int);

		node = of_find_matching_node(node, accdet_of_match);
		if (node) {
			of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
			of_property_read_u32_array(node, "interrupts", ints1, ARRAY_SIZE(ints1));
			gpiopin = ints[0];
			headsetdebounce = ints[1];
			accdet_eint_type = ints1[1];
			gpio_set_debounce(gpiopin, headsetdebounce);
			accdet_irq = irq_of_parse_and_map(node, 0);
			ret = request_irq(accdet_irq, accdet_eint_func, IRQF_TRIGGER_NONE, "accdet-eint", NULL);
			if (ret != 0) {
				ACCDET_ERROR("[Accdet]EINT IRQ LINE NOT AVAILABLE\n");
			} else {
				ACCDET_ERROR("[Accdet]accdet set EINT finished, accdet_irq=%d, headsetdebounce=%d\n",
					     accdet_irq, headsetdebounce);
			}
		} else {
			ACCDET_ERROR("[Accdet]%s can't find compatible node\n", __func__);
		}
	} else {
		pinctrl_select_state(accdet_pinctrl1, pins_default);
		ACCDET_INFO("[Accdet] %s: defulat state selected\n", __func__);
	}
	return 0;
}
#endif/* CONFIG_ACCDET_EINT */

#ifndef CONFIG_FOUR_KEY_HEADSET
static int key_check(int b)
{
	/* ACCDET_DEBUG("adc_data: %d v\n",b); */

	/* 0.24V ~ */
	/* ACCDET_DEBUG("[accdet] come in key_check!!\n"); */
	if ((b < accdet_dts_data.three_key.down_key) && (b >= accdet_dts_data.three_key.up_key))
		return DW_KEY;
	else if ((b < accdet_dts_data.three_key.up_key) && (b >= accdet_dts_data.three_key.mid_key))
		return UP_KEY;
	else if (b < accdet_dts_data.three_key.mid_key)
		return MD_KEY;
	ACCDET_DEBUG("[accdet] leave key_check!!\n");
	return NO_KEY;
}
#else
static int key_check(int b)
{
	/* 0.24V ~ */
	/* ACCDET_DEBUG("[accdet] come in key_check!!\n"); */
	if ((b < accdet_dts_data.four_key.down_key_four) && (b >= accdet_dts_data.four_key.up_key_four))
		return DW_KEY;
	else if ((b < accdet_dts_data.four_key.up_key_four) && (b >= accdet_dts_data.four_key.voice_key_four))
		return UP_KEY;
	else if ((b < accdet_dts_data.four_key.voice_key_four) && (b >= accdet_dts_data.four_key.mid_key_four))
		return AS_KEY;
	else if (b < accdet_dts_data.four_key.mid_key_four)
		return MD_KEY;
	ACCDET_DEBUG("[accdet] leave key_check!!\n");
	return NO_KEY;
}

#endif

static void send_accdet_status_event(int cable, int status)
{
	switch (cable) {
	case HEADSET_NO_MIC:
		input_report_switch(kpd_accdet_dev, SW_HEADPHONE_INSERT, status);
		/* when plug 4-pole out, if both AB=3 AB=0 happen,3-pole plug in will be incorrectly reported,
		* then 3-pole plug-out is reported, if no mantory 4-pole plug-out,icon would be visible.
		*/
		if (status == 0)
			input_report_switch(kpd_accdet_dev, SW_MICROPHONE_INSERT, status);

		input_report_switch(kpd_accdet_dev, SW_JACK_PHYSICAL_INSERT, status);
		input_sync(kpd_accdet_dev);
		ACCDET_INFO("HEADPHONE(3-pole) %s\n", status?"PlugIn":"PlugOut");
		break;
	case HEADSET_MIC:
		/* when plug 4-pole out, 3-pole plug out should also be reported for slow plug-in case */
		if (status == 0)
			input_report_switch(kpd_accdet_dev, SW_HEADPHONE_INSERT, status);
		input_report_switch(kpd_accdet_dev, SW_MICROPHONE_INSERT, status);
		input_report_switch(kpd_accdet_dev, SW_JACK_PHYSICAL_INSERT, status);
		input_sync(kpd_accdet_dev);
		ACCDET_INFO("MICROPHONE(4-pole) %s\n", status?"PlugIn":"PlugOut");
		break;
	case LINE_OUT_DEVICE:
		input_report_switch(kpd_accdet_dev, SW_LINEOUT_INSERT, status);
		input_sync(kpd_accdet_dev);
		ACCDET_INFO("LineOut %s\n", status?"PlugIn":"PlugOut");
		break;
	default:
		ACCDET_DEBUG("Invalid cable type\n");
	}
}
static void send_key_event(int keycode, int flag)
{
	switch (keycode) {
	case DW_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOLUMEDOWN, flag);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("[accdet]KEY_VOLUMEDOWN %d\n", flag);
		break;
	case UP_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOLUMEUP, flag);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("[accdet]KEY_VOLUMEUP %d\n", flag);
		break;
	case MD_KEY:
		input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE, flag);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("[accdet]KEY_PLAYPAUSE %d\n", flag);
		break;
	case AS_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOICECOMMAND, flag);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("[accdet]KEY_VOICECOMMAND %d\n", flag);
		break;
	}
}

static void multi_key_detection(int current_status)
{
	int m_key = 0;
	int cali_voltage = 0;

	if (current_status == 0) {
		cali_voltage = Accdet_PMIC_IMM_GetOneChannelValue(1);
		/* ACCDET_DEBUG("[Accdet]adc cali_voltage1 = %d mv\n", cali_voltage); */
		m_key = cur_key = key_check(cali_voltage);
	}
#if NO_USE_COMPARATOR
	mdelay(10); /* decrease the time delay */
#else
	mdelay(30);
#endif
#ifdef CONFIG_ACCDET_EINT_IRQ
if (hw_rev == HW_V_01) {
	if (((pmic_pwrap_read(ACCDET_IRQ_STS) & EINT_IRQ_STATUS_BIT) != EINT_IRQ_STATUS_BIT) || eint_accdet_sync_flag) {
#if defined(CONFIG_SEC_JACK_EXTENSION)
		earjack_priv.jack.button_det = (!current_status) ? true : false;
		earjack_priv.jack.adc_val = (!current_status) ? cali_voltage : 0;
#endif
		send_key_event(cur_key, !current_status);
	} else {
		ACCDET_INFO("[Accdet]plug out side effect key press, do not report key = %d\n", cur_key);
		cur_key = NO_KEY;
	}
}
#endif

#ifdef CONFIG_ACCDET_EINT
if (hw_rev == HW_V_00) {
	if (((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT) != IRQ_STATUS_BIT) || eint_accdet_sync_flag) {
#if defined(CONFIG_SEC_JACK_EXTENSION)
		earjack_priv.jack.button_det = (!current_status) ? true : false;
		earjack_priv.jack.adc_val = (!current_status) ? cali_voltage : 0;
#endif
		send_key_event(cur_key, !current_status);
	} else {
		ACCDET_INFO("[Accdet]plug out side effect key press, do not report key = %d\n", cur_key);
		cur_key = NO_KEY;
	}
}
#endif
	if (current_status)
		cur_key = NO_KEY;
}

static void accdet_workqueue_func(void)
{
	int ret;

	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		ACCDET_ERROR("[Accdet]accdet_work return:%d!\n", ret);
}

int accdet_irq_handler(void)
{
	u64 cur_time = 0;

	cur_time = accdet_get_current_time();
	ACCDET_INFO("[accdet_irq_handler][0x%x]=0x%x\n", ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS));
#ifdef CONFIG_ACCDET_EINT_IRQ
	if (hw_rev == HW_V_01) {
		if ((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
		    && ((pmic_pwrap_read(ACCDET_IRQ_STS) & EINT_IRQ_STATUS_BIT) != EINT_IRQ_STATUS_BIT)) {
			clear_accdet_interrupt();
			if (accdet_status == MIC_BIAS) {
				/* accdet_auxadc_switch(1); */
				pmic_pwrap_write(ACCDET_PWM_WIDTH, REGISTER_VALUE(cust_headset_settings->pwm_width));
				pmic_pwrap_write(ACCDET_PWM_THRESH, REGISTER_VALUE(cust_headset_settings->pwm_width));
			}
			accdet_workqueue_func();
			while (((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
				&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))));
			/* clear accdet int, modify  for fix interrupt trigger twice error */
			pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS) & (~IRQ_CLR_BIT));
			pmic_pwrap_write(INT_STATUS_ACCDET, RG_INT_STATUS_ACCDET);
		} else if ((pmic_pwrap_read(ACCDET_IRQ_STS) & EINT_IRQ_STATUS_BIT) == EINT_IRQ_STATUS_BIT) {
			if (cur_eint_state == EINT_PIN_PLUG_IN) {
				if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
					pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS) | EINT_IRQ_POL_HIGH);
				else
					pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS) & ~EINT_IRQ_POL_LOW);
			} else {
				if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
					pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS) & ~EINT_IRQ_POL_LOW);
				else
					pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS) | EINT_IRQ_POL_HIGH);
			}
			clear_accdet_eint_interrupt();
			while (((pmic_pwrap_read(ACCDET_IRQ_STS) & EINT_IRQ_STATUS_BIT)
				&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))));
			/* clear eint int, modify  for fix interrupt trigger twice error */
			pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS) & (~IRQ_EINT_CLR_BIT));
			pmic_pwrap_write(INT_STATUS_ACCDET, RG_INT_STATUS_ACCDET_EINT);
			accdet_eint_func(accdet_irq, NULL);
		} else {
			ACCDET_ERROR("ACCDET IRQ and EINT IRQ don't be triggerred!!\n");
		}
	}
#endif

#ifdef CONFIG_ACCDET_EINT
	if (hw_rev == HW_V_00) {
		if ((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT))
			clear_accdet_interrupt();
		if (accdet_status == MIC_BIAS) {
			/* accdet_auxadc_switch(1); */
			pmic_pwrap_write(ACCDET_PWM_WIDTH, REGISTER_VALUE(cust_headset_settings->pwm_width));
			pmic_pwrap_write(ACCDET_PWM_THRESH, REGISTER_VALUE(cust_headset_settings->pwm_width));
		}
		accdet_workqueue_func();
		while (((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))));
		/* clear accdet int, modify  for fix interrupt trigger twice error */
		pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS) & (~IRQ_CLR_BIT));
		pmic_pwrap_write(INT_STATUS_ACCDET, RG_INT_STATUS_ACCDET);
	}
#endif

#ifdef ACCDET_NEGV_IRQ
	cur_time = accdet_get_current_time();
	if ((pmic_pwrap_read(ACCDET_IRQ_STS) & NEGV_IRQ_STATUS_BIT) == NEGV_IRQ_STATUS_BIT) {
		ACCDET_DEBUG("[ACCDET NEGV detect]plug in a error Headset\n\r");
		pmic_pwrap_write(ACCDET_IRQ_STS, (IRQ_NEGV_CLR_BIT));
		while (((pmic_pwrap_read(ACCDET_IRQ_STS) & NEGV_IRQ_STATUS_BIT)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
		pmic_pwrap_write(ACCDET_IRQ_STS, (pmic_pwrap_read(ACCDET_IRQ_STS) & (~IRQ_NEGV_CLR_BIT)));
	}
#endif

	return 1;
}

/* clear ACCDET IRQ in accdet register */
static inline void clear_accdet_interrupt(void)
{
	/* it is safe by using polling to adjust when to clear IRQ_CLR_BIT */
	pmic_pwrap_write(ACCDET_IRQ_STS, ((pmic_pwrap_read(ACCDET_IRQ_STS)) & 0x8000) | (IRQ_CLR_BIT));
	ACCDET_INFO("[clear_accdet_interrupt][0x%x]=0x%x\n", ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS));
}

static inline void clear_accdet_eint_interrupt(void)
{
	pmic_pwrap_write(ACCDET_IRQ_STS, (((pmic_pwrap_read(ACCDET_IRQ_STS)) & 0x8000) | IRQ_EINT_CLR_BIT));
	ACCDET_INFO("[clear_accdet_eint_interrupt][0x%x]=0x%x\n", ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS));
}

static inline void check_cable_type(void)
{
	unsigned int current_status = 0;
	int irq_temp = 0;/* for clear IRQ_bit */
	int wait_clear_irq_times = 0;
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
	int pin_adc_value = 0;
#define PIN_ADC_CHANNEL 5
#endif

#if NO_USE_COMPARATOR
	current_status = check_pole_type();
#else
	current_status = ((pmic_pwrap_read(ACCDET_STATE_RG) & 0xc0) >> 6); /*A=bit1; B=bit0*/
	ACCDET_INFO("[Accdet]accdet interrupt happen:[%s]current AB = %d\n",
		 accdet_status_string[accdet_status], current_status);
#endif

	button_status = 0;
	pre_status = accdet_status;

	/* ACCDET_DEBUG("[Accdet]check_cable_type: ACCDET_IRQ_STS = 0x%x\n", pmic_pwrap_read(ACCDET_IRQ_STS)); */
	IRQ_CLR_FLAG = false;
	switch (accdet_status) {
	case PLUG_OUT:
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		pmic_pwrap_write(ACCDET_DEBOUNCE1, cust_headset_settings->debounce1);
#endif
		if (current_status == 0) {
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
			/* micbias always on during detected PIN recognition */
			pmic_pwrap_write(ACCDET_PWM_WIDTH, cust_headset_settings->pwm_width);
			pmic_pwrap_write(ACCDET_PWM_THRESH, cust_headset_settings->pwm_width);
			ACCDET_DEBUG("[Accdet]PIN recognition micbias always on!\n");
			ACCDET_DEBUG("[Accdet]before adc read, pin_adc_value = %d mv!\n", pin_adc_value);
			msleep(500);
			current_status = ((pmic_pwrap_read(ACCDET_STATE_RG) & 0xc0) >> 6);	/* A=bit1; B=bit0 */
			if (current_status == 0 && show_icon_delay != 0) {
				/* accdet_auxadc_switch(1);switch on when need to use auxadc read voltage */
				pin_adc_value = Accdet_PMIC_IMM_GetOneChannelValue(1);
				ACCDET_DEBUG("[Accdet]pin_adc_value = %d mv!\n", pin_adc_value);
				/*accdet_auxadc_switch(0);*/
				if (pin_adc_value < 180 && pin_adc_value > 90) {	/* 90mv illegal headset */
					/* mt_set_gpio_out(GPIO_CAMERA_2_CMRST_PIN, GPIO_OUT_ONE); */
					/* ACCDET_DEBUG("[Accdet]PIN recognition change GPIO_OUT!\n"); */
					mutex_lock(&accdet_eint_irq_sync_mutex);
					if (eint_accdet_sync_flag == 1) {
						cable_type = HEADSET_NO_MIC;
						accdet_status = HOOK_SWITCH;
						cable_pin_recognition = 1;
						ACCDET_DEBUG("[Accdet] cable_pin_recognition = %d\n",
							     cable_pin_recognition);
					} else {
						ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
					}
					mutex_unlock(&accdet_eint_irq_sync_mutex);
				} else {
					mutex_lock(&accdet_eint_irq_sync_mutex);
					if (eint_accdet_sync_flag == 1) {
						cable_type = HEADSET_NO_MIC;
						accdet_status = HOOK_SWITCH;
					} else {
						ACCDET_DEBUG("[Accdet] Headset has plugged out\n");
					}
					mutex_unlock(&accdet_eint_irq_sync_mutex);
				}
			}
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				cable_type = HEADSET_NO_MIC;
				accdet_status = HOOK_SWITCH;
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);

#ifdef CONFIG_MTK_TC10_FEATURE/* for titan */
			if ((cable_type == HEADSET_NO_MIC) && (hw_rev == HW_V_01)) {/* for Titan L+G detection */
				pmic_pwrap_write(ACCDET_STATE_SWCTRL,
					pmic_pwrap_read(ACCDET_STATE_SWCTRL) & (~ACCDET_SWCTRL_IDLE_EN));
				pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) & (~(ACCDET_ENABLE)));
				if (hw_mode_support)/* close HW path */
					pmic_pwrap_write(ACCDET_HW_MODE_DFF, 0x8000);

				pmic_pwrap_write(INT_CON_ACCDET_CLR, RG_ACCDET_IRQ_CLR);
				clear_accdet_interrupt();
				pmic_pwrap_write(ACCDET_STATE_SWCTRL, ACCDET_EINT_PWM_EN);
				ACCDET_INFO("[Accdet] Headset No MIC, disable ACCDET\n");
			} else {
				ACCDET_ERROR("[Accdet]No MIC Error:type:%d,hw_rev:%d\n", cable_type, hw_rev);
			}
#endif
#endif
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
#ifdef CONFIG_HEADSET_SUPPORT_FIVE_POLE
				msleep(20);
				if (pmic_pwrap_read(0x0F46) & 0x01) {
					/* check 5 pole headset */
					ACCDET_INFO("[Accdet]check 5 pole headset: YES\n");
					cable_type = HEADSET_FIVE_POLE;
				}
#endif
				/* AB=11 debounce=30ms */
				pmic_pwrap_write(ACCDET_DEBOUNCE3, cust_headset_settings->debounce3 * 30);
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			pmic_pwrap_write(ACCDET_DEBOUNCE0, button_press_debounce);
			/* recover polling set AB 00-01 */
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
			pmic_pwrap_write(ACCDET_PWM_WIDTH, REGISTER_VALUE(cust_headset_settings->pwm_width));
			pmic_pwrap_write(ACCDET_PWM_THRESH, REGISTER_VALUE(cust_headset_settings->pwm_thresh));
#endif
#if NO_USE_COMPARATOR
		} else if (current_status == 2) {/* external cable plug */
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = EXT_CABLE;
				cable_type = NO_DEVICE;
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out:2\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
		} else if (current_status == 3) {
			ACCDET_INFO("[Accdet]PLUG_OUT state not change!\n");
#ifdef CONFIG_ACCDET_EINT_IRQ
			if (hw_rev == HW_V_01) {
				mutex_lock(&accdet_eint_irq_sync_mutex);
				if (eint_accdet_sync_flag == 1) {
					accdet_status = PLUG_OUT;
					cable_type = NO_DEVICE;
				} else {
					ACCDET_ERROR("[Accdet] Headset has plugged out:3\n");
				}
				mutex_unlock(&accdet_eint_irq_sync_mutex);
			}
#endif
		} else {
			ACCDET_ERROR("[Accdet]PLUG_OUT can't change to this state!\n");
		}
		break;

	case MIC_BIAS:
		/* solution: resume hook switch debounce time */
		pmic_pwrap_write(ACCDET_DEBOUNCE0, cust_headset_settings->debounce0);

		if (current_status == 0) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				while ((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
					&& (wait_clear_irq_times < 3)) {
					ACCDET_INFO("[Accdet]check_cable_type: MIC BIAS clear IRQ on-going1....\n");
					wait_clear_irq_times++;
					msleep(20);
				}
				irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
				irq_temp = irq_temp & (~IRQ_CLR_BIT);
				pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
				IRQ_CLR_FLAG = true;
				accdet_status = HOOK_SWITCH;
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			button_status = 1;
			if (button_status) {
				mutex_lock(&accdet_eint_irq_sync_mutex);
				if (eint_accdet_sync_flag == 1)
					multi_key_detection(current_status);
				else
					ACCDET_ERROR("[Accdet] multi_key_detection: Headset has plugged out\n");
				mutex_unlock(&accdet_eint_irq_sync_mutex);
				/* accdet_auxadc_switch(0); */
				/* recover  pwm frequency and duty */
				pmic_pwrap_write(ACCDET_PWM_WIDTH, REGISTER_VALUE(cust_headset_settings->pwm_width));
				pmic_pwrap_write(ACCDET_PWM_THRESH, REGISTER_VALUE(cust_headset_settings->pwm_thresh));
			}
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
				ACCDET_INFO("[Accdet]MIC_BIAS state not change!\n");
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#if NO_USE_COMPARATOR
		} else if (current_status == 2) {/* external cable plug */
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = EXT_CABLE;
				cable_type = NO_DEVICE;
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out:2\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
		} else if (current_status == 3) {
			ACCDET_INFO("[Accdet]do not send plug ou in micbiast\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1)
				accdet_status = PLUG_OUT;
			else
				ACCDET_ERROR("[Accdet] Headset has plugged out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else {
			ACCDET_ERROR("[Accdet]MIC_BIAS can't change to this state!\n");
		}
		break;
	case HOOK_SWITCH:
		if (current_status == 0) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				/* for avoid 01->00 framework of Headset will report press key info for Audio */
				/* cable_type = HEADSET_NO_MIC; */
				/* accdet_status = HOOK_SWITCH; */
				ACCDET_INFO("[Accdet]HOOK_SWITCH state not change!\n");
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				multi_key_detection(current_status);
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			/* accdet_auxadc_switch(0); */
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
			cable_pin_recognition = 0;
			ACCDET_DEBUG("[Accdet] cable_pin_recognition = %d\n", cable_pin_recognition);
			pmic_pwrap_write(ACCDET_PWM_WIDTH, REGISTER_VALUE(cust_headset_settings->pwm_width));
			pmic_pwrap_write(ACCDET_PWM_THRESH, REGISTER_VALUE(cust_headset_settings->pwm_thresh));
#endif
			/* solution: reduce hook switch debounce time to 0x400 */
			pmic_pwrap_write(ACCDET_DEBOUNCE0, button_press_debounce);
#if (defined(CONFIG_MTK_PMIC_CHIP_MT6355)) || (defined(CONFIG_MTK_TC10_FEATURE))
		/* fix record no voice when press key */
		/* notify audio when assert--bit0=1,bit1=1,bit6=0,bit7=1 */
		if ((0xC3 & (pmic_pwrap_read(0x361E))) == 0x83)
			mtk_audio_reset_input_precharge();/* call audio to reset record */
#endif
#if NO_USE_COMPARATOR
		} else if (current_status == 2) {/* external cable plug */
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = EXT_CABLE;
				cable_type = NO_DEVICE;
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out:2\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
		} else if (current_status == 3) {
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
			cable_pin_recognition = 0;
			ACCDET_DEBUG("[Accdet] cable_pin_recognition = %d\n", cable_pin_recognition);
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1)
				accdet_status = PLUG_OUT;
			else
				ACCDET_ERROR("[Accdet] Headset has plugged out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
			ACCDET_INFO("[Accdet] do not send plug out event in hook switch\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1)
				accdet_status = PLUG_OUT;
			else
				ACCDET_ERROR("[Accdet] Headset has plugged out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else {
			ACCDET_ERROR("[Accdet]HOOK_SWITCH can't change to this state!\n");
		}
		break;
#if NO_USE_COMPARATOR
	case EXT_CABLE:
		if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				multi_key_detection(current_status);
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out:external cable\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == 0) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				cable_type = HEADSET_NO_MIC;
				accdet_status = HOOK_SWITCH;
			} else {
				ACCDET_ERROR("[Accdet] Headset has plugged out:external cable\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == 3) {
			ACCDET_INFO("[Accdet]PLUG_OUT state not change!\n");
#ifdef CONFIG_ACCDET_EINT_IRQ
			if (hw_rev == HW_V_01) {
				mutex_lock(&accdet_eint_irq_sync_mutex);
				if (eint_accdet_sync_flag == 1) {
					accdet_status = PLUG_OUT;
					cable_type = NO_DEVICE;
				} else {
					ACCDET_ERROR("[Accdet] Headset has plugged out:3\n");
				}
				mutex_unlock(&accdet_eint_irq_sync_mutex);
			}
#endif
		} else {
			ACCDET_ERROR("[Accdet]EXT_CABLE can't change to this state!\n");
		}
		break;
#endif
	case STAND_BY:
		if (current_status == 3)
			ACCDET_INFO("[Accdet]accdet do not send plug out event in stand by!\n");
		else
			ACCDET_ERROR("[Accdet]STAND_BY can't change to this state!\n");
		break;

	default:
		ACCDET_ERROR("[Accdet]check_cable_type: accdet current status error!\n");
		break;

	}

	if (!IRQ_CLR_FLAG) {
		mutex_lock(&accdet_eint_irq_sync_mutex);
		if (eint_accdet_sync_flag == 1) {
			while ((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT) && (wait_clear_irq_times < 3)) {
				ACCDET_INFO("[Accdet]check_cable_type: Clear interrupt on-going2....\n");
				wait_clear_irq_times++;
				msleep(20);
			}
		}
		irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
		irq_temp = irq_temp & (~IRQ_CLR_BIT);
		pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		IRQ_CLR_FLAG = true;
		ACCDET_INFO("[Accdet]check_cable_type:Clear interrupt:Done[0x%x]!\n", pmic_pwrap_read(ACCDET_IRQ_STS));

	} else {
		IRQ_CLR_FLAG = false;
	}
#if defined(CONFIG_SEC_JACK_EXTENSION)
	earjack_priv.jack.jack_det = cable_type;
#endif

	ACCDET_DEBUG("[Accdet]cable type:[%s], status switch:[%s]->[%s]\n",
		     accdet_report_string[cable_type], accdet_status_string[pre_status],
		     accdet_status_string[accdet_status]);
}

static void accdet_work_callback(struct work_struct *work)
{
	wake_lock(&accdet_irq_lock);
	check_cable_type();

#ifdef CONFIG_ACCDET_PIN_SWAP
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
	if (cable_pin_recognition == 1) {
		cable_pin_recognition = 0;
		accdet_FSA8049_disable();
		cable_type = HEADSET_NO_MIC;
		accdet_status = PLUG_OUT;
	}
#endif
#endif
	mutex_lock(&accdet_eint_irq_sync_mutex);
	if (eint_accdet_sync_flag == 1) {
#if defined(CONFIG_SEC_JACK_EXTENSION)
		earjack_priv.jack.jack_det = cable_type;
#endif
		send_accdet_status_event(cable_type, 1);
	} else
		ACCDET_ERROR("[Accdet] Headset has plugged out don't set accdet state\n");
	mutex_unlock(&accdet_eint_irq_sync_mutex);
	ACCDET_DEBUG(" [accdet] set state in cable_type %d\n", cable_type);

	wake_unlock(&accdet_irq_lock);
}

int accdet_get_dts_data(void)
{
	struct device_node *node = NULL;
	int debounce[7];
	#ifdef CONFIG_FOUR_KEY_HEADSET
	int four_key[5];
	#else
	int three_key[4];
	#endif
	#if NO_USE_COMPARATOR
	unsigned int vol_thresh[5] = { 0 };
	#endif
	#if (defined(CONFIG_MTK_TC10_FEATURE))
	int tmp_hw_rev = HW_V_00;
	#endif

	ACCDET_INFO("[ACCDET]Start accdet_get_dts_data");
	node = of_find_matching_node(node, accdet_of_match);
	if (node) {
		of_property_read_u32_array(node, "headset-mode-setting", debounce, ARRAY_SIZE(debounce));
		of_property_read_u32(node, "accdet-mic-vol", &accdet_dts_data.mic_mode_vol);
		of_property_read_u32(node, "accdet-plugout-debounce", &accdet_dts_data.accdet_plugout_debounce);
		of_property_read_u32(node, "accdet-mic-mode", &accdet_dts_data.accdet_mic_mode);
		of_property_read_u32(node, "headset-eint-level-pol", &accdet_dts_data.eint_level_pol);
#ifdef CONFIG_FOUR_KEY_HEADSET
		of_property_read_u32_array(node, "headset-four-key-threshold", four_key, ARRAY_SIZE(four_key));
		memcpy(&accdet_dts_data.four_key, four_key+1, sizeof(struct four_key_threshold));
		ACCDET_INFO("[Accdet]mid-Key = %d, voice = %d, up_key = %d, down_key = %d\n",
		     accdet_dts_data.four_key.mid_key_four, accdet_dts_data.four_key.voice_key_four,
		     accdet_dts_data.four_key.up_key_four, accdet_dts_data.four_key.down_key_four);
#else
		#ifdef CONFIG_HEADSET_TRI_KEY_CDD
		of_property_read_u32_array(node, "headset-three-key-threshold-CDD", three_key, ARRAY_SIZE(three_key));
		#else
		of_property_read_u32_array(node, "headset-three-key-threshold", three_key, ARRAY_SIZE(three_key));
		#endif
		memcpy(&accdet_dts_data.three_key, three_key+1, sizeof(struct three_key_threshold));
		ACCDET_INFO("[Accdet]mid-Key = %d, up_key = %d, down_key = %d\n",
		     accdet_dts_data.three_key.mid_key, accdet_dts_data.three_key.up_key,
		     accdet_dts_data.three_key.down_key);
		#endif

#if NO_USE_COMPARATOR
		of_property_read_u32_array(node, "headset-vol-threshold", vol_thresh, ARRAY_SIZE(vol_thresh));
		memcpy(&cust_vol_set, vol_thresh, sizeof(vol_thresh));
		ACCDET_INFO("[Accdet]min_3pole = %d, max_3pole = %d, min_4pole = %d, max_4pole = %d\n",
		     cust_vol_set.vol_min_3pole, cust_vol_set.vol_max_3pole,
			cust_vol_set.vol_min_4pole, cust_vol_set.vol_max_4pole);
		if (cust_vol_set.vol_bias > 2500) {
			cust_vol_set.vol_bias = 2700;/* 2700mv */
			ACCDET_ERROR("[Accdet]bias vol set %d mv--->2700 mv\n", cust_vol_set.vol_bias);
		} else {
			ACCDET_ERROR("[Accdet]bias vol set %d mv\n", cust_vol_set.vol_bias);
		}
#endif
		memcpy(&accdet_dts_data.headset_debounce, debounce, sizeof(debounce));
		cust_headset_settings = &accdet_dts_data.headset_debounce;
		ACCDET_INFO("[Accdet]pwm_width = %x, pwm_thresh = %x\n deb0 = %x, deb1 = %x, mic_mode = %d\n",
		     cust_headset_settings->pwm_width, cust_headset_settings->pwm_thresh,
		     cust_headset_settings->debounce0, cust_headset_settings->debounce1,
		     accdet_dts_data.accdet_mic_mode);
	} else {
		ACCDET_ERROR("[Accdet]%s can't find compatible dts node\n", __func__);
		return -1;
	}
#if (defined(CONFIG_MTK_TC10_FEATURE))
	tmp_hw_rev = get_multi_dts_hw_rev();/* for titan only */
	if (HW_V_00 == tmp_hw_rev) {
		hw_rev = HW_V_00;
		hw_mode_support = 0;
		accdet_dts_data.accdet_mic_mode = 1;/* mode1 */
	} else {
		hw_rev = HW_V_01;
		hw_mode_support = 1;
		if (accdet_dts_data.accdet_mic_mode == 1)
			accdet_dts_data.accdet_mic_mode = 2;/* mode1-->mode2 */
	}
	ACCDET_INFO("[accdet]hw_rev=%d, mic-mode:%d\n",
		    tmp_hw_rev, accdet_dts_data.accdet_mic_mode);
#else/* CONFIG_MTK_PMIC_CHIP_MT6355 */
#if (defined(CONFIG_ACCDET_EINT_IRQ))
	hw_mode_support = 1;
#elif (defined(CONFIG_ACCDET_EINT))
	hw_mode_support = 0;
#else/* default */
	hw_mode_support = 0;
#endif
#endif
	ACCDET_INFO("[accdet] hw_mode_support:%d\n", hw_mode_support);
	return 0;
}
void accdet_pmic_Read_Efuse_HPOffset(void)
{
	unsigned int efusevalue = 0;
	unsigned int tmp_val = 0;

	efusevalue = 0;
	tmp_val = 0;

/* support mt6355, default open HW mode */
#if (defined(CONFIG_MTK_PMIC_CHIP_MT6355)) || (defined(CONFIG_MTK_TC10_FEATURE))
#ifdef CONFIG_FOUR_KEY_HEADSET
	/* [Notice] must confirm the bias vol is 2.7v */
	/* get 8bit from efuse rigister, so need extend to 12bit, shift right 2*/
	if (accdet_dts_data.accdet_mic_mode == 6) {/* use for 4-key, 2.7V, internal bias, mode6 */
		/* AD */
		tmp_val = pmic_pwrap_read(REG_ACCDET_AD_CALI_1);
		efusevalue = (tmp_val>>RG_ACCDET_BIT_SHIFT)&ACCDET_CALI_MASK2;
		tmp_val = pmic_pwrap_read(REG_ACCDET_AD_CALI_2);
		efusevalue = efusevalue | ((tmp_val & 0x01) << RG_ACCDET_HIGH_BIT_SHIFT);
		accdet_dts_data.four_key.mid_key_four = efusevalue;

		/* DB */
		tmp_val = pmic_pwrap_read(REG_ACCDET_AD_CALI_2);
		efusevalue = (tmp_val>>0x01)&ACCDET_CALI_MASK3;
		accdet_dts_data.four_key.voice_key_four = efusevalue;

		/* BC */
		tmp_val = pmic_pwrap_read(REG_ACCDET_AD_CALI_2);
		efusevalue = (tmp_val>>RG_ACCDET_BIT_SHIFT)&ACCDET_CALI_MASK4;
		tmp_val = pmic_pwrap_read(REG_ACCDET_AD_CALI_3);
		efusevalue = efusevalue | ((tmp_val & 0x01) << RG_ACCDET_HIGH_BIT_SHIFT);
		accdet_dts_data.four_key.up_key_four = efusevalue;
		accdet_dts_data.four_key.down_key_four = 1000;/* ?? need check */
		ACCDET_INFO("[accdet_Efuse] update 4-key max-thresh: mid[%d],voice[%d],up[%d],down[%d]\n",
			accdet_dts_data.four_key.mid_key_four, accdet_dts_data.four_key.voice_key_four,
			accdet_dts_data.four_key.up_key_four, accdet_dts_data.four_key.down_key_four);
	} else {
		ACCDET_INFO("[accdet_Efuse]--use dts 4key thresh..\n");
	}
#endif

	/*  not same with mt6351 */
	tmp_val = pmic_pwrap_read(REG_ACCDET_AD_CALI_0);/* read HW reg */
	efusevalue = (int)((tmp_val>>RG_ACCDET_BIT_SHIFT)&ACCDET_CALI_MASK0);
	tmp_val = pmic_pwrap_read(REG_ACCDET_AD_CALI_1);/* read HW reg */
	accdet_auxadc_offset = efusevalue | ((tmp_val & 0x01) << RG_ACCDET_HIGH_BIT_SHIFT);
	if (accdet_auxadc_offset > 128)
		accdet_auxadc_offset -= 256;
	accdet_auxadc_offset = (accdet_auxadc_offset / 2);
	ACCDET_INFO("[accdet_Efuse]--efuse=0x%x,auxadc_value=%d mv\n", efusevalue, accdet_auxadc_offset);
#else/* used on mt6351 */
	efusevalue =  pmic_Read_Efuse_HPOffset(RG_OTP_PA_ADDR_WORD_INDEX);
	accdet_auxadc_offset = (int)((efusevalue >> RG_OTP_PA_ACCDET_BIT_SHIFT) & 0xFF);
	if (accdet_auxadc_offset > 128)/* modify to solve the sign data issue */
		accdet_auxadc_offset -= 256;
	accdet_auxadc_offset = (accdet_auxadc_offset / 2);
	ACCDET_INFO(" efusevalue = 0x%x, accdet_auxadc_offset = %d\n", efusevalue, accdet_auxadc_offset);
#endif
}

static inline void accdet_init(void)
{
	unsigned int reg_val = 0;

	reg_val = 0;
	ACCDET_DEBUG("[Accdet]accdet hardware init\n");
	/* clock */
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	/* ACCDET_DEBUG("[Accdet]accdet TOP_CKPDN=0x%x!\n", pmic_pwrap_read(TOP_CKPDN)); */
	/* reset the accdet unit */
	/* ACCDET_DEBUG("ACCDET reset : reset start!\n\r"); */
	pmic_pwrap_write(TOP_RST_ACCDET_SET, ACCDET_RESET_SET);
	/* ACCDET_DEBUG("ACCDET reset function test: reset finished!!\n\r"); */
	pmic_pwrap_write(TOP_RST_ACCDET_CLR, ACCDET_RESET_CLR);
	/* init  pwm frequency and duty */
	pmic_pwrap_write(ACCDET_PWM_WIDTH, REGISTER_VALUE(cust_headset_settings->pwm_width));
	pmic_pwrap_write(ACCDET_PWM_THRESH, REGISTER_VALUE(cust_headset_settings->pwm_thresh));
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0x07);

	/* rise and fall delay of PWM */
	pmic_pwrap_write(ACCDET_EN_DELAY_NUM,
			 (cust_headset_settings->fall_delay << 15 | cust_headset_settings->rise_delay));
	/* init the debounce time */
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
	pmic_pwrap_write(ACCDET_DEBOUNCE0, cust_headset_settings->debounce0);
	pmic_pwrap_write(ACCDET_DEBOUNCE1, 0xFFFF);	/* 2.0s */
	pmic_pwrap_write(ACCDET_DEBOUNCE3, cust_headset_settings->debounce3);
	pmic_pwrap_write(ACCDET_DEBOUNCE4, ACCDET_DE4);
#else
	pmic_pwrap_write(ACCDET_DEBOUNCE0, cust_headset_settings->debounce0);
	pmic_pwrap_write(ACCDET_DEBOUNCE1, cust_headset_settings->debounce1);
	pmic_pwrap_write(ACCDET_DEBOUNCE3, cust_headset_settings->debounce3);
	pmic_pwrap_write(ACCDET_DEBOUNCE4, ACCDET_DE4);
#endif

	/* enable INT */
#ifdef CONFIG_ACCDET_EINT_IRQ
if (hw_rev == HW_V_01) {
	if (cur_eint_state == EINT_PIN_PLUG_OUT) {
		if (accdet_dts_data.eint_level_pol == IRQ_TYPE_LEVEL_HIGH) {
			accdet_eint_type = IRQ_TYPE_LEVEL_HIGH;
			pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS)|EINT_IRQ_POL_HIGH);
			reg_val = pmic_pwrap_read(ACCDET_IRQ_STS);
			ACCDET_INFO("[accdet]high:[0x%x]=0x%x\n", ACCDET_IRQ_STS, reg_val);

			if (atomic_read(&g_accdet_first) == 1) {/* set pmic eint default value */
				reg_val = pmic_pwrap_read(ACCDET_CTRL);
				ACCDET_INFO("[accdet]high:1.[0x%x]=0x%x\n", ACCDET_CTRL, reg_val);
				/* set bit3 to enable default EINT init status */
				pmic_pwrap_write(ACCDET_CTRL, reg_val|(0x01<<3));
				mdelay(2);
				reg_val = pmic_pwrap_read(ACCDET_CTRL);
				ACCDET_INFO("[accdet]high:2.[0x%x]=0x%x\n", ACCDET_CTRL, reg_val);
				reg_val = pmic_pwrap_read(ACCDET_DEFAULT_STATE_RG);
				ACCDET_INFO("[accdet]high:1.[0x%x]=0x%x\n", ACCDET_DEFAULT_STATE_RG, reg_val);
				/* set default EINT init status */
				pmic_pwrap_write(ACCDET_DEFAULT_STATE_RG, (reg_val|(1<<14))&(0x4333));
				mdelay(2);
				reg_val = pmic_pwrap_read(ACCDET_DEFAULT_STATE_RG);
				ACCDET_INFO("[accdet]high:2.[0x%x]=0x%x\n", ACCDET_DEFAULT_STATE_RG, reg_val);
				/* clear bit3 to disable default EINT init status */
				pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL)&(~(0x01<<3)));
			}
		}
	}
	pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS) & (~IRQ_EINT_CLR_BIT));
}
#endif

#ifdef CONFIG_ACCDET_EINT
	if (hw_rev == HW_V_00)
		pmic_pwrap_write(ACCDET_IRQ_STS, pmic_pwrap_read(ACCDET_IRQ_STS) & (~IRQ_CLR_BIT));
#endif
	pmic_pwrap_write(INT_CON_ACCDET_SET, RG_ACCDET_IRQ_SET);
#ifdef CONFIG_ACCDET_EINT_IRQ
	if (hw_rev == HW_V_01)
		pmic_pwrap_write(INT_CON_ACCDET_SET, RG_ACCDET_EINT_IRQ_SET);
#endif
#ifdef ACCDET_NEGV_IRQ
	pmic_pwrap_write(INT_CON_ACCDET_SET, RG_ACCDET_NEGV_IRQ_SET);
#endif

   /* ACCDET Analog Setting */
	pmic_pwrap_write(ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG)|RG_AUDACCDETPULLLOW);
	pmic_pwrap_write(ACCDET_MICBIAS_REG, pmic_pwrap_read(ACCDET_MICBIAS_REG)
		| (accdet_dts_data.mic_mode_vol<<4) | RG_AUDMICBIAS1LOWPEN);
	pmic_pwrap_write(ACCDET_RSV, 0x0010);
#ifdef CONFIG_ACCDET_EINT_IRQ
	if (hw_rev == HW_V_01) {
		pmic_pwrap_write(ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG)
			| RG_EINTCONFIGACCDET); /* Internal connection between ACCDET and EINT */
	}
#endif
#ifdef ACCDET_NEGV_IRQ
	pmic_pwrap_write(ACCDET_EINT_NV, pmic_pwrap_read(ACCDET_EINT_NV) | ACCDET_NEGV_DT_EN);
#endif

	if (accdet_dts_data.accdet_mic_mode == 1)	{/* ACC mode */
		ACCDET_INFO("[accdet]init mode1\n");
		/* Just for TItan as HW use DCC mode */
		pmic_pwrap_write(ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG) | RG_ACCDETSEL);
	} else if (accdet_dts_data.accdet_mic_mode == 2) {/* Low cost mode without internal bias */
#ifdef CONFIG_MTK_TC10_FEATURE
		/* set 2.4v, and L detection PU disable, G detection PU can not disable */
		pmic_pwrap_write(ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG) | (0x01<<11) | RG_ACCDETSEL);
#else
		pmic_pwrap_write(ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG) | RG_EINT_ANA_CONFIG);
#endif
		ACCDET_INFO("[accdet]init mode2\n");
	} else if (accdet_dts_data.accdet_mic_mode == 6) {	/* Low cost mode with internal bias */
		pmic_pwrap_write(ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG) | RG_EINT_ANA_CONFIG);
		pmic_pwrap_write(ACCDET_MICBIAS_REG, pmic_pwrap_read(ACCDET_MICBIAS_REG)|RG_MICBIAS1DCSWPEN);
		ACCDET_INFO("[accdet]init mode6\n");
	}

	if (atomic_read(&g_accdet_first) == 1) {
#ifdef CONFIG_MTK_TC10_FEATURE
	/* for titan support L/G detection */
		pmic_pwrap_write(AUDENC_ANA_CON8, 0x0200);
#endif
		pmic_pwrap_write(ACCDET_HW_MODE_DFF, 0x8000);/* SW path, init */
#if 0
		/* support mt6355, default open HW mode */
		if (hw_mode_support)/* HW path */
			pmic_pwrap_write(ACCDET_HW_MODE_DFF, 0x8000|ACCDET_HWMODE_SEL|ACCDET_EINT_DEB_OUT_DFF);
		else
			pmic_pwrap_write(ACCDET_HW_MODE_DFF, 0x8000);/* SW path */
#endif
	}

#if defined CONFIG_ACCDET_EINT
	/* disable ACCDET unit */
	if (hw_rev == HW_V_00) {
		pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
		pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0x0);
		pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
	}
#endif
#if defined CONFIG_ACCDET_EINT_IRQ
	if (hw_rev == HW_V_01) {
		if (cur_eint_state == EINT_PIN_PLUG_OUT)/* debounce=256ms */
			pmic_pwrap_write(ACCDET_EINT_CTL, pmic_pwrap_read(ACCDET_EINT_CTL) | EINT_IRQ_DE_IN);
		pmic_pwrap_write(ACCDET_EINT_CTL, pmic_pwrap_read(ACCDET_EINT_CTL) | EINT_PWM_THRESH);
		/* disable ACCDET unit, except CLK of ACCDET */
		pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
		pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) | ACCDET_DISABLE);
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, pmic_pwrap_read(ACCDET_STATE_SWCTRL) & (~ACCDET_SWCTRL_EN));
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, pmic_pwrap_read(ACCDET_STATE_SWCTRL) | ACCDET_EINT_PWM_EN);
		pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) | ACCDET_EINT_EN);
	}
#endif

#ifdef ACCDET_NEGV_IRQ
	pmic_pwrap_write(ACCDET_EINT_PWM_DELAY, pmic_pwrap_read(ACCDET_EINT_PWM_DELAY) & (~0x1F));
	pmic_pwrap_write(ACCDET_EINT_PWM_DELAY, pmic_pwrap_read(ACCDET_EINT_PWM_DELAY) | 0x0F);
	pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) | ACCDET_NEGV_EN);
#endif

#if NO_USE_COMPARATOR
	/* for open test mode(mbias=2.8V) as the external bias=2.8V of Titan */
	if (cust_vol_set.vol_bias == 2800)
		pmic_pwrap_write(ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG) | (1<<7));
#endif

   /************ AUXADC enable auto sample ****************/
	pmic_pwrap_write(ACCDET_AUXADC_AUTO_SPL, (pmic_pwrap_read(ACCDET_AUXADC_AUTO_SPL) | ACCDET_AUXADC_AUTO_SET));
}

/* sysfs */
#if DEBUG_THREAD
#if defined(ACCDET_TS3A225E_PIN_SWAP)
static ssize_t show_TS3A225EConnectorType(struct device_driver *ddri, char *buf)
{
	ACCDET_INFO("[Accdet] TS3A225E ts3a225e_connector_type=%d\n", ts3a225e_connector_type);
	return sprintf(buf, "%u\n", ts3a225e_connector_type);
}
static DRIVER_ATTR(TS3A225EConnectorType, 0664, show_TS3A225EConnectorType, NULL);
#endif
static int dump_register(void)
{
	int i = 0;

#if (defined(CONFIG_MTK_PMIC_CHIP_MT6355)) || (defined(CONFIG_MTK_TC10_FEATURE))/* support mt6355 */
	for (i = ACCDET_RSV; i <= ACCDET_HW_MODE_DFF; i += 2)
		ACCDET_INFO(" ACCDET_BASE + 0x%x=0x%x\n", i, pmic_pwrap_read(ACCDET_BASE + i));
#else
	for (i = ACCDET_RSV; i <= ACCDET_RSV_CON1; i += 2)
		ACCDET_INFO(" ACCDET_BASE + 0x%x=0x%x\n", i, pmic_pwrap_read(ACCDET_BASE + i));
#endif

	ACCDET_INFO(" (0x%x) =0x%x\n", TOP_RST_ACCDET, pmic_pwrap_read(TOP_RST_ACCDET));
	ACCDET_INFO(" (0x%x) =0x%x\n", INT_CON_ACCDET, pmic_pwrap_read(INT_CON_ACCDET));
	ACCDET_INFO(" (0x%x) =0x%x\n", TOP_CKPDN, pmic_pwrap_read(TOP_CKPDN));
	ACCDET_INFO(" (0x%x) =0x%x\n", ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG));
	ACCDET_INFO(" (0x%x) =0x%x\n", ACCDET_MICBIAS_REG, pmic_pwrap_read(ACCDET_MICBIAS_REG));

	ACCDET_INFO(" (0x%x) = 0x%x\n", AUDENC_ANA_CON8, pmic_pwrap_read(AUDENC_ANA_CON8));
	ACCDET_INFO(" (0x%x) = 0x%x\n", REG_ACCDET_AD_CALI_0, pmic_pwrap_read(REG_ACCDET_AD_CALI_0));
	ACCDET_INFO(" (0x%x) = 0x%x\n", REG_ACCDET_AD_CALI_1, pmic_pwrap_read(REG_ACCDET_AD_CALI_1));
	ACCDET_INFO(" (0x%x) = 0x%x\n", REG_ACCDET_AD_CALI_2, pmic_pwrap_read(REG_ACCDET_AD_CALI_2));
	ACCDET_INFO(" (0x%x) = 0x%x\n", REG_ACCDET_AD_CALI_3, pmic_pwrap_read(REG_ACCDET_AD_CALI_3));
#ifdef CONFIG_ACCDET_PIN_SWAP
	/*ACCDET_INFO(" 0x00004000 =%x\n",pmic_pwrap_read(0x00004000));*/
	/*VRF28 power for PIN swap feature*/
#endif
	return 0;
}

static int cat_register(char *buf)
{
	int i = 0;
	char buf_temp[128] = { 0 };

#ifdef CONFIG_ACCDET_EINT_IRQ
	if (hw_rev == HW_V_01)
		strncat(buf, "[CONFIG_ACCDET_EINT_IRQ]dump_register:\n", 64);
#endif
#ifdef CONFIG_ACCDET_EINT
	if (hw_rev == HW_V_00)
		strncat(buf, "[CONFIG_ACCDET_EINT]dump_register:\n", 64);
#endif

#if (defined(CONFIG_MTK_PMIC_CHIP_MT6355)) || (defined(CONFIG_MTK_TC10_FEATURE))/* support mt6355 */
	for (i = ACCDET_RSV; i <= ACCDET_HW_MODE_DFF; i += 2) {
		sprintf(buf_temp, "ACCDET_ADDR[0x%x]=0x%x\n", (ACCDET_BASE + i), pmic_pwrap_read(ACCDET_BASE + i));
		strncat(buf, buf_temp, strlen(buf_temp));
	}
#else
	for (i = ACCDET_RSV; i <= ACCDET_RSV_CON1; i += 2) {
		sprintf(buf_temp, "ACCDET_ADDR[0x%x]=0x%x\n", (ACCDET_BASE + i), pmic_pwrap_read(ACCDET_BASE + i));
		strncat(buf, buf_temp, strlen(buf_temp));
	}
#endif

	sprintf(buf_temp, "[0x%x]=0x%x\n", TOP_RST_ACCDET, pmic_pwrap_read(TOP_RST_ACCDET));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "[0x%x]=0x%x\n", INT_CON_ACCDET, pmic_pwrap_read(INT_CON_ACCDET));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "[0x%x]=0x%x\n", TOP_CKPDN, pmic_pwrap_read(TOP_CKPDN));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "[0x%x]=0x%x\n", ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "[0x%x]=0x%x\n", ACCDET_AUXADC_REG, pmic_pwrap_read(ACCDET_AUXADC_REG));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "[0x%x]=0x%x\n", ACCDET_MICBIAS_REG, pmic_pwrap_read(ACCDET_MICBIAS_REG));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "0x%x=0x%x\n", REG_ACCDET_AD_CALI_0, pmic_pwrap_read(REG_ACCDET_AD_CALI_0));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "0x%x=0x%x\n", REG_ACCDET_AD_CALI_1, pmic_pwrap_read(REG_ACCDET_AD_CALI_1));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "0x%x=0x%x\n", REG_ACCDET_AD_CALI_2, pmic_pwrap_read(REG_ACCDET_AD_CALI_2));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "0x%x=0x%x\n", REG_ACCDET_AD_CALI_3, pmic_pwrap_read(REG_ACCDET_AD_CALI_3));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "L/G,0x%x=0x%x\n", AUDENC_ANA_CON8, pmic_pwrap_read(AUDENC_ANA_CON8));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "record,0x361E=0x%x\n", pmic_pwrap_read(0x361E));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "mode:%d,rev:%d,hw_en:%d,offset:%d mv,vol:%d mv\n", accdet_dts_data.accdet_mic_mode,
		hw_rev, hw_mode_support, accdet_auxadc_offset, Accdet_PMIC_IMM_GetOneChannelValue(1));
	strncat(buf, buf_temp, strlen(buf_temp));

	return 0;
}

static ssize_t store_accdet_call_state(struct device_driver *ddri, const char *buf, size_t count)
{
	int ret = 0;

	if (strlen(buf) < 1) {
		ACCDET_ERROR("[%s] Invalid input!!\n",  __func__);
		return -EINVAL;
	}

	ret = kstrtoint(buf, 10, &call_status);
	if (ret < 0)
		return ret;

	switch (call_status) {
	case CALL_IDLE:
		ACCDET_INFO("[%s]accdet call: Idle state!\n", __func__);
		break;
	case CALL_RINGING:
		ACCDET_INFO("[%s]accdet call: ringing state!\n", __func__);
		break;
	case CALL_ACTIVE:
		ACCDET_INFO("[%s]accdet call: active or hold state!\n", __func__);
		/*return button_status;*/
		break;
	default:
		ACCDET_INFO("[%s]accdet call: Invalid value=%d\n", __func__, call_status);
		break;
	}
	return count;
}

static ssize_t show_pin_recognition_state(struct device_driver *ddri, char *buf)
{
	if (buf == NULL) {
		ACCDET_ERROR("[%s] *buf is NULL Pointer\n",  __func__);
		return -EINVAL;
	}
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
	ACCDET_INFO("ACCDET show_pin_recognition_state = %d\n", cable_pin_recognition);
	return sprintf(buf, "%u\n", cable_pin_recognition);
#else
	return sprintf(buf, "Not support: %u\n", 0);
#endif
}

static DRIVER_ATTR(accdet_pin_recognition, 0664, show_pin_recognition_state, NULL);
static DRIVER_ATTR(accdet_call_state, 0664, NULL, store_accdet_call_state);

static ssize_t store_accdet_set_headset_mode(struct device_driver *ddri, const char *buf, size_t count)
{
	int ret = 0;
	int tmp_headset_mode = 0;

	if (strlen(buf) < 1) {
		ACCDET_ERROR("[%s] Invalid input!!\n",  __func__);
		return -EINVAL;
	}

	ret = kstrtoint(buf, 10, &tmp_headset_mode);
	if (ret < 0)
		return ret;

	ACCDET_DEBUG("[%s]get accdet mode: %d\n", __func__, tmp_headset_mode);
	switch (tmp_headset_mode&0x0F) {
	case 1:
		ACCDET_INFO("[%s]Don't support accdet mode_1 to configure!!\n", __func__);
		/* accdet_dts_data.accdet_mic_mode = tmp_headset_mode; */
		/* accdet_init(); */
		break;
	case 2:
		accdet_dts_data.accdet_mic_mode = tmp_headset_mode;
		accdet_init();
		break;
	case 6:
		accdet_dts_data.accdet_mic_mode = tmp_headset_mode;
		accdet_init();
		break;
	default:
		ACCDET_INFO("[%s]Not support accdet mode: %d\n", __func__, tmp_headset_mode);
		break;
	}

	return count;
}

static ssize_t show_accdet_dump_register(struct device_driver *ddri, char *buf)
{
	if (buf == NULL) {
		ACCDET_ERROR("[%s] *buf is NULL Pointer\n",  __func__);
		return -EINVAL;
	}

	cat_register(buf);
	ACCDET_INFO("[%s] buf_size:%d\n", __func__, (int)strlen(buf));

	return strlen(buf);
}

static int dbug_thread(void *unused)
{
	while (g_start_debug_thread) {
		if (g_dump_register)
			dump_register();
		msleep(500);
	}
	return 0;
}

static ssize_t store_accdet_start_debug_thread(struct device_driver *ddri, const char *buf, size_t count)
{
	int error = 0;
	int ret = 0;

	if (strlen(buf) < 1) {
		ACCDET_ERROR("[%s] Invalid input!!\n",  __func__);
		return -EINVAL;
	}

	/* if write 0, Invalid; otherwise, valid */
	ret = strncmp(buf, "0", 1);
	if (ret) {
		g_start_debug_thread = 1;
		thread = kthread_run(dbug_thread, 0, "ACCDET");
		if (IS_ERR(thread)) {
			error = PTR_ERR(thread);
			ACCDET_ERROR("[%s]failed to create kernel thread: %d\n",  __func__, error);
		} else {
			ACCDET_INFO("[%s]start debug thread!\n",  __func__);
		}
	} else {
		g_start_debug_thread = 0;
		ACCDET_INFO("[%s]stop debug thread!\n",  __func__);
	}

	return count;
}

static ssize_t store_accdet_dump_register(struct device_driver *ddri, const char *buf, size_t count)
{
	int ret = 0;

	if (strlen(buf) < 1) {
		ACCDET_ERROR("[%s] Invalid input!!\n",  __func__);
		return -EINVAL;
	}

	/* if write 0, Invalid; otherwise, valid */
	ret = strncmp(buf, "0", 1);
	if (ret) {
		g_dump_register = 1;
		ACCDET_INFO("[%s]start dump regs!\n",  __func__);
	} else {
		g_dump_register = 0;
		ACCDET_INFO("[%s]stop dump regs!\n",  __func__);
	}

	return count;
}

static ssize_t store_accdet_set_register(struct device_driver *ddri, const char *buf, size_t count)
{
	int ret = 0;
	unsigned int addr_temp = 0;
	unsigned int value_temp = 0;

	if (strlen(buf) < 3) {
		ACCDET_ERROR("[%s] Invalid input!!\n",  __func__);
		return -EINVAL;
	}

	ret = sscanf(buf, "0x%x,0x%x", &addr_temp, &value_temp);
	if (ret < 0)
		return ret;

	ACCDET_INFO("[%s] set addr[0x%x]=0x%x\n",  __func__, addr_temp, value_temp);

	/* comfirm PMIC addr is legal */
	if ((addr_temp < PMIC_REG_BASE_START) || (addr_temp > PMIC_REG_BASE_END))
		ACCDET_ERROR("[%s] No support illegal addr[0x%x]!!\n", __func__, addr_temp);
	else if (addr_temp&0x01)
		ACCDET_ERROR("[%s] No set illegal addr[0x%x]!!\n", __func__, addr_temp);
	else
		pmic_pwrap_write(addr_temp, value_temp);/* set reg */

	return count;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(dump_register, S_IWUSR | S_IRUGO, show_accdet_dump_register, store_accdet_dump_register);
static DRIVER_ATTR(set_headset_mode, S_IWUSR | S_IRUGO, NULL, store_accdet_set_headset_mode);
static DRIVER_ATTR(start_debug, S_IWUSR | S_IRUGO, NULL, store_accdet_start_debug_thread);
static DRIVER_ATTR(set_register, S_IWUSR | S_IRUGO, NULL, store_accdet_set_register);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *accdet_attr_list[] = {
	&driver_attr_start_debug,
	&driver_attr_set_register,
	&driver_attr_dump_register,
	&driver_attr_set_headset_mode,
	&driver_attr_accdet_call_state,
/*#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION*/
	&driver_attr_accdet_pin_recognition,
/*#endif*/
#if defined(ACCDET_TS3A225E_PIN_SWAP)
	&driver_attr_TS3A225EConnectorType,
#endif
};

static int accdet_create_attr(struct device_driver *driver)
{
	int idx = sizeof(accdet_attr_list);
	int err = sizeof(accdet_attr_list[0]);
	int num = idx / err;

	idx = err = 0;
	if (driver == NULL)
		return -EINVAL;
	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, accdet_attr_list[idx]);
		if (err) {
			ACCDET_DEBUG("driver_create_file (%s) = %d\n", accdet_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
#endif

void accdet_int_handler(void)
{
	int ret = 0;

	ACCDET_DEBUG("[accdet_int_handler]....\n");
	ret = accdet_irq_handler();
	if (ret == 0)
		ACCDET_DEBUG("[accdet_int_handler] don't finished\n");
}

void accdet_eint_int_handler(void)
{
	int ret = 0;

	ACCDET_DEBUG("[accdet_eint_int_handler]....\n");
	ret = accdet_irq_handler();
	if (ret == 0)
		ACCDET_DEBUG("[accdet_int_handler] don't finished\n");
}

/* just be called by audio module for DC trim */
void accdet_late_init(unsigned long a)
{
#ifndef CONFIG_MTK_TC10_FEATURE
	if (atomic_cmpxchg(&g_accdet_first, 1, 0)) {
		del_timer_sync(&accdet_init_timer);
		accdet_init();
		/* schedule a work for the first detection */
		queue_work(accdet_workqueue, &accdet_work);
	} else {
		ACCDET_INFO("[accdet_late_init]err: accdet have been done or get dts failed!\n");
	}
#endif
}
EXPORT_SYMBOL(accdet_late_init);

/* just be called by audio module for DC trim */
void accdet_delay_callback(unsigned long a)
{
#ifndef CONFIG_MTK_TC10_FEATURE
	if (atomic_cmpxchg(&g_accdet_first, 1, 0)) {
		accdet_init();
		/* schedule a work for the first detection */
		queue_work(accdet_workqueue, &accdet_work);
	} else {
		ACCDET_INFO("[accdet_delay_callback]err: accdet have been done or get dts failed!\n");
	}
#endif
}
int mt_accdet_probe(struct platform_device *dev)
{
	int ret = 0;

#if DEBUG_THREAD
	struct platform_driver accdet_driver_hal = accdet_driver_func();
#endif
#if defined(CONFIG_SEC_JACK_EXTENSION)
	struct jack_device_priv *pEarjack_priv = &earjack_priv;	
#endif

	ACCDET_INFO("[Accdet]accdet_probe begin!\n");

/*
* below register accdet as switch class
*/

/* Create normal device for auido use */
	ret = alloc_chrdev_region(&accdet_devno, 0, 1, ACCDET_DEVNAME);
	if (ret)
		ACCDET_ERROR("[Accdet]alloc_chrdev_region: Get Major number error!\n");

	accdet_cdev = cdev_alloc();
	accdet_cdev->owner = THIS_MODULE;
	accdet_cdev->ops = accdet_get_fops();
	ret = cdev_add(accdet_cdev, accdet_devno, 1);
	if (ret)
		ACCDET_ERROR("[Accdet]accdet error: cdev_add\n");

	accdet_class = class_create(THIS_MODULE, ACCDET_DEVNAME);

	/* if we want auto creat device node, we must call this */
	accdet_nor_device = device_create(accdet_class, NULL, accdet_devno, NULL, ACCDET_DEVNAME);

/* Create input device */
	kpd_accdet_dev = input_allocate_device();
	if (!kpd_accdet_dev) {
		ACCDET_ERROR("[Accdet]kpd_accdet_dev : fail!\n");
		return -ENOMEM;
	}
#ifndef CONFIG_MTK_TC10_FEATURE/* for titan */
	/* INIT the timer to disable micbias. */
	init_timer(&micbias_timer);
	micbias_timer.expires = jiffies + MICBIAS_DISABLE_TIMER;
	micbias_timer.function = &disable_micbias;
	micbias_timer.data = ((unsigned long)0);

	/* INIT the timer for comfirm the accdet can also init when audio can't callback in any case*/
	init_timer(&accdet_init_timer);
	accdet_init_timer.expires = jiffies + ACCDET_INIT_WAIT_TIMER;
	accdet_init_timer.function = &accdet_delay_callback;
	accdet_init_timer.data = ((unsigned long)0);
#endif

	/* define multi-key keycode */
	__set_bit(EV_KEY, kpd_accdet_dev->evbit);
	__set_bit(KEY_PLAYPAUSE, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEDOWN, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEUP, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOICECOMMAND, kpd_accdet_dev->keybit);

	__set_bit(EV_SW, kpd_accdet_dev->evbit);
	__set_bit(SW_HEADPHONE_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_MICROPHONE_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_JACK_PHYSICAL_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_LINEOUT_INSERT, kpd_accdet_dev->swbit);

	kpd_accdet_dev->id.bustype = BUS_HOST;
	kpd_accdet_dev->name = "ACCDET";
	if (input_register_device(kpd_accdet_dev))
		ACCDET_ERROR("[Accdet]kpd_accdet_dev register : fail!\n");

/* Create workqueue */
	accdet_workqueue = create_singlethread_workqueue("accdet");
	INIT_WORK(&accdet_work, accdet_work_callback);

/* wake lock */
	wake_lock_init(&accdet_suspend_lock, WAKE_LOCK_SUSPEND, "accdet wakelock");
	wake_lock_init(&accdet_irq_lock, WAKE_LOCK_SUSPEND, "accdet irq wakelock");
	wake_lock_init(&accdet_key_lock, WAKE_LOCK_SUSPEND, "accdet key wakelock");
	wake_lock_init(&accdet_timer_lock, WAKE_LOCK_SUSPEND, "accdet timer wakelock");
#if DEBUG_THREAD
	ret = accdet_create_attr(&accdet_driver_hal.driver);
	if (ret != 0)
		ACCDET_ERROR("create attribute err = %d\n", ret);
#endif

	if (atomic_read(&g_accdet_first) == 1) {
		eint_accdet_sync_flag = 1;
		/* Accdet Hardware Init */
		ret = accdet_get_dts_data();
		if (ret == 0) {
#if (defined(CONFIG_MTK_PMIC_CHIP_MT6355)) || (defined(CONFIG_MTK_TC10_FEATURE))
			pmic_register_interrupt_callback(INT_ACCDET, accdet_int_handler);
#ifdef CONFIG_ACCDET_EINT_IRQ
			if (hw_rev == HW_V_01)
				pmic_register_interrupt_callback(INT_ACCDET_EINT, accdet_eint_int_handler);
#endif
#endif

#ifdef CONFIG_ACCDET_EINT_IRQ
			if (hw_rev == HW_V_01) {
				accdet_eint_workqueue = create_singlethread_workqueue("accdet_eint");
				INIT_WORK(&accdet_eint_work, accdet_eint_work_callback);
			}
#endif
#ifndef CONFIG_MTK_TC10_FEATURE/* for titan */
			accdet_disable_workqueue = create_singlethread_workqueue("accdet_disable");
			INIT_WORK(&accdet_disable_work, disable_micbias_callback);
#endif
			accdet_init();
			accdet_pmic_Read_Efuse_HPOffset();
			/* schedule a work for the first detection */
			/* queue_work(accdet_workqueue, &accdet_work); *//* delete for check first error AB=00 */
#ifdef CONFIG_ACCDET_EINT
			if (hw_rev == HW_V_00) {
				accdet_eint_workqueue = create_singlethread_workqueue("accdet_eint");
				INIT_WORK(&accdet_eint_work, accdet_eint_work_callback);
				accdet_setup_eint(dev);
			}
#endif
#ifdef CONFIG_MTK_TC10_FEATURE/* for titan */
			atomic_set(&g_accdet_first, 0);
#else
			atomic_set(&g_accdet_first, 1);
			mod_timer(&accdet_init_timer, (jiffies + ACCDET_INIT_WAIT_TIMER));
#endif
		} else {
#ifndef CONFIG_MTK_TC10_FEATURE/* for titan */
			atomic_set(&g_accdet_first, 0);
#endif
			ACCDET_INFO("[Accdet]accdet_probe failed as dts get error\n");
		}
	}
#if defined(CONFIG_SEC_JACK_EXTENSION)
	/* pEarjack_priv->sdev = accdet_data; */
	create_jack_ex_devices(pEarjack_priv);
#endif
	ACCDET_INFO("[Accdet]accdet_probe done!\n");
	return 0;
}

void mt_accdet_remove(void)
{
	ACCDET_DEBUG("[Accdet]accdet_remove begin!\n");

	/* cancel_delayed_work(&accdet_work); */
	destroy_workqueue(accdet_eint_workqueue);
	destroy_workqueue(accdet_workqueue);

	device_del(accdet_nor_device);
	class_destroy(accdet_class);
	cdev_del(accdet_cdev);
	unregister_chrdev_region(accdet_devno, 1);
	input_unregister_device(kpd_accdet_dev);
	ACCDET_DEBUG("[Accdet]accdet_remove Done!\n");
}

void mt_accdet_suspend(void)/* only one suspend mode */
{
#if defined CONFIG_ACCDET_EINT || defined CONFIG_ACCDET_EINT_IRQ
	ACCDET_DEBUG("[Accdet]suspend: ACCDET_IRQ_STS = 0x%x\n", pmic_pwrap_read(ACCDET_IRQ_STS));
#else
	ACCDET_DEBUG("[Accdet]accdet_suspend: ACCDET_CTRL=[0x%x], STATE=[0x%x]->[0x%x]\n",
	       pmic_pwrap_read(ACCDET_CTRL), pre_state_swctrl, pmic_pwrap_read(ACCDET_STATE_SWCTRL));
#endif
}

void mt_accdet_resume(void)	/* wake up */
{
#if defined CONFIG_ACCDET_EINT || defined CONFIG_ACCDET_EINT_IRQ
	ACCDET_DEBUG("[Accdet]resume: ACCDET_IRQ_STS = 0x%x\n", pmic_pwrap_read(ACCDET_IRQ_STS));
#else
	ACCDET_DEBUG("[Accdet]accdet_resume: ACCDET_CTRL=[0x%x], STATE_SWCTRL=[0x%x]\n",
	       pmic_pwrap_read(ACCDET_CTRL), pmic_pwrap_read(ACCDET_STATE_SWCTRL));

#endif
}

/* add for IPO-H need update headset state when resume */
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
struct timer_list accdet_disable_ipoh_timer;
static void mt_accdet_pm_disable(unsigned long a)
{
	if (cable_type == NO_DEVICE && eint_accdet_sync_flag == 0) {
		/* disable accdet */
		pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0);
#ifdef CONFIG_ACCDET_EINT
		if (hw_rev == HW_V_00) {
			pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
			/* disable clock */
			pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
		}
#endif
#ifdef CONFIG_ACCDET_EINT_IRQ
		if (hw_rev == HW_V_01)
			pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) & (~(ACCDET_ENABLE)));
#endif
		ACCDET_DEBUG("[Accdet]daccdet_pm_disable: disable!\n");
	} else {
		ACCDET_DEBUG("[Accdet]daccdet_pm_disable: enable!\n");
	}
}
#endif
void mt_accdet_pm_restore_noirq(void)
{
	int current_status_restore = 0;

	ACCDET_DEBUG("[Accdet]accdet_pm_restore_noirq start!\n");
	/* enable ACCDET unit */
	ACCDET_DEBUG("accdet: enable_accdet\n");
	/* enable clock */
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
#ifdef CONFIG_ACCDET_EINT_IRQ
	if (hw_rev == HW_V_01) {
		pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_EINT_IRQ_CLR);
		pmic_pwrap_write(ACCDET_RSV, pmic_pwrap_read(ACCDET_RSV) | ACCDET_INPUT_MICP);
		pmic_pwrap_write(ACCDET_ADC_REG, pmic_pwrap_read(ACCDET_ADC_REG) | RG_EINTCONFIGACCDET);
		pmic_pwrap_write(ACCDET_CTRL, ACCDET_EINT_EN);
	}
#endif
#ifdef ACCDET_NEGV_IRQ
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_NEGV_IRQ_CLR);
	pmic_pwrap_write(ACCDET_EINT_NV, pmic_pwrap_read(ACCDET_EINT_NV) | ACCDET_NEGV_DT_EN);
	pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) | ACCDET_NEGV_EN);
#endif
	enable_accdet(ACCDET_SWCTRL_EN);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, (pmic_pwrap_read(ACCDET_STATE_SWCTRL) | ACCDET_SWCTRL_IDLE_EN));

	eint_accdet_sync_flag = 1;
	current_status_restore = ((pmic_pwrap_read(ACCDET_STATE_RG) & 0xc0) >> 6);	/* AB */

	switch (current_status_restore) {
	case 0:		/* AB=0 */
		cable_type = HEADSET_NO_MIC;
		accdet_status = HOOK_SWITCH;
		send_accdet_status_event(cable_type, 1);
		break;
	case 1:		/* AB=1 */
		cable_type = HEADSET_MIC;
		accdet_status = MIC_BIAS;
		send_accdet_status_event(cable_type, 1);
		break;
	case 3:		/* AB=3 */
		/* send input event before cable_type swutch to no_device, which is invalid type for input */
		send_accdet_status_event(cable_type, 0);
		cable_type = NO_DEVICE;
		accdet_status = PLUG_OUT;
		break;
	default:
		ACCDET_DEBUG("[Accdet]accdet_pm_restore_noirq: accdet current status error!\n");
		break;
	}
#if defined(CONFIG_SEC_JACK_EXTENSION)
	earjack_priv.jack.jack_det = cable_type;
#endif
	if (cable_type == NO_DEVICE) {
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
		init_timer(&accdet_disable_ipoh_timer);
		accdet_disable_ipoh_timer.expires = jiffies + 3 * HZ;
		accdet_disable_ipoh_timer.function = &mt_accdet_pm_disable;
		accdet_disable_ipoh_timer.data = ((unsigned long)0);
		add_timer(&accdet_disable_ipoh_timer);
		ACCDET_DEBUG("[Accdet]enable! pm timer\n");

#else
		/* disable accdet */
		pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
#ifdef CONFIG_ACCDET_EINT
		if (hw_rev == HW_V_00) {
			pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0);
			pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
			/* disable clock */
			pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
		}
#endif
#ifdef CONFIG_ACCDET_EINT_IRQ
		if (hw_rev == HW_V_01) {
			pmic_pwrap_write(ACCDET_STATE_SWCTRL, ACCDET_EINT_PWM_EN);
			pmic_pwrap_write(ACCDET_CTRL, pmic_pwrap_read(ACCDET_CTRL) & (~(ACCDET_ENABLE)));
		}
#endif
#endif
	}
}
/* IPO_H end */

long mt_accdet_unlocked_ioctl(unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case ACCDET_INIT:
		break;
	case SET_CALL_STATE:
		call_status = (int)arg;
		ACCDET_DEBUG("[Accdet]accdet_ioctl : CALL_STATE=%d\n", call_status);
		break;
	case GET_BUTTON_STATUS:
		ACCDET_DEBUG("[Accdet]accdet_ioctl : Button_Status=%d\n", button_status);
		return button_status;
	default:
		ACCDET_DEBUG("[Accdet]accdet_ioctl : default\n");
		break;
	}
	return 0;
}
