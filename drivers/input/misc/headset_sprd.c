/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/headset_sprd.h>
#include <mach/board.h>
#include <linux/input.h>

#include <mach/adc.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <linux/module.h>
#include <linux/wakelock.h>

#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <mach/sci_glb_regs.h>
#include <mach/arch_misc.h>
#include <linux/notifier.h>
#ifdef CONFIG_OF
#include <linux/slab.h>
#include <linux/of_device.h>
#endif

//====================  debug  ====================
#define ENTER \
do{ if(debug_level >= 1) printk(KERN_INFO "[SPRD_HEADSET_DBG][%d] func: %s  line: %04d\n", adie_type, __func__, __LINE__); }while(0)

#define PRINT_DBG(format,x...)  \
do{ if(debug_level >= 1) printk(KERN_INFO "[SPRD_HEADSET_DBG][%d] " format, adie_type, ## x); }while(0)

#define PRINT_INFO(format,x...)  \
do{ printk(KERN_INFO "[SPRD_HEADSET_INFO][%d] " format, adie_type, ## x); }while(0)

#define PRINT_WARN(format,x...)  \
do{ printk(KERN_INFO "[SPRD_HEADSET_WARN][%d] " format, adie_type, ## x); }while(0)

#define PRINT_ERR(format,x...)  \
do{ printk(KERN_ERR "[SPRD_HEADSET_ERR][%d] func: %s  line: %04d  info: " format, adie_type, __func__, __LINE__, ## x); }while(0)
//====================  debug  ====================

/**********************************************
 * The micro ADPGAR_BYP_SELECT is used for avoiding the Bug#298417,
 * please refer to Bug#298417 to confirm your configuration.
 **********************************************/
#define ADPGAR_BYP_SELECT

#ifdef CONFIG_HEADSET_STS_POLLING_DISABLED
#undef SPRD_STS_POLLING_EN
#else
#define SPRD_STS_POLLING_EN
#endif

#define PLUG_CONFIRM_COUNT (2)
#define NO_MIC_RETRY_COUNT (0)
#define ADC_READ_COUNT (5)
#define ADC_READ_LOOP (2)
#define ADC_GND (100)
#define SCI_ADC_GET_VALUE_COUNT (50)

#define ARM_MF_REG (0x0110)
#define HEAD_INSERT2_EIC_EN (BIT(15))
#define HEAD_INSERT_EIC_EN (BIT(14))

#define EIC3_DBNC_CTRL (0x4C)
#define DBNC_CNT3_VALUE (30)
#define DBNC_CNT3_SHIFT (0)
#define DBNC_CNT3_MASK (0xFFF << DBNC_CNT3_SHIFT)

#define EIC5_DBNC_CTRL (0x54)
#define DBNC_CNT5_VALUE (25)
#define DBNC_CNT5_SHIFT (0)
#define DBNC_CNT5_MASK (0xFFF << DBNC_CNT5_SHIFT)

#if (defined(CONFIG_ARCH_SCX15))
        #define ADIE_CHID_LOW 0x0134
        #define ADIE_CHID_HIGH 0x0138
#else
        #if (defined(CONFIG_ARCH_SCX35))
                #define ADIE_CHID_LOW 0x0108
                #define ADIE_CHID_HIGH 0x010C
        #endif
#endif

#if (defined(CONFIG_ARCH_SCX15))
        #define CLK_AUD_HID_EN (BIT(4))
        #define CLK_AUD_HBD_EN (BIT(3))
#else
        #if (defined(CONFIG_ARCH_SCX35))
                #define CLK_AUD_HID_EN (BIT(5))
                #define CLK_AUD_HBD_EN (BIT(4))
        #endif
#endif

#define HEADMIC_DETECT_BASE (ANA_AUDCFGA_INT_BASE)
#define HEADMIC_DETECT_REG(X) (HEADMIC_DETECT_BASE + (X))

#define HDT_EN (BIT(5))
#define AUD_EN (BIT(4))

#define HEADMIC_BUTTON_BASE (ANA_HDT_INT_BASE)
#define HEADMIC_BUTTON_REG(X) (HEADMIC_BUTTON_BASE + (X))
#define HID_CFG0 (0x0080)
#define HID_CFG2 (0x0088)
#define HID_CFG3 (0x008C)
#define HID_CFG4 (0x0090)

#define ANA_CFG0 (0x0040)
#define ANA_CFG1 (0x0044)
#define ANA_CFG2 (0x0048)
#define ANA_CFG5 (0x0064)
#define ANA_CFG20 (0x00A0)
#define ANA_STS0 (0x00C0)

#define HEADMIC_DETECT_GLB_REG(X) (ANA_REGS_GLB_BASE + (X))

#define HID_TMR_T1T2_STEP_SHIFT (5)
#define HID_TMR_T0_SHIFT (0)
#define HID_TMR_T1_SHIFT (0)
#define HID_TMR_T2_SHIFT (0)

#define HID_TMR_T1T2_STEP_MASK (0x1F << HID_TMR_T1T2_STEP_SHIFT)
#define HID_TMR_T0_MASK (0x1F << HID_TMR_T0_SHIFT)
#define HID_TMR_T1_MASK (0xFFFF << HID_TMR_T1_SHIFT)
#define HID_TMR_T2_MASK (0xFFFF << HID_TMR_T2_SHIFT)

#define HID_TMR_T1T2_STEP_VAL (0x1)
#define HID_TMR_T0_VAL (0xF)
#define HID_TMR_T1_VAL (0xF)
#define HID_TMR_T2_VAL (0xF)

#define AUDIO_ICM_PLUS_EN (BIT(2))

#define AUDIO_HEADMIC_SLEEP_EN (BIT(1))
#define AUDIO_MICBIAS_HV_EN (BIT(2))
#define AUDIO_MICBIAS_V_SHIFT (3)
#define AUDIO_MICBIAS_V_MASK (0x3 << AUDIO_MICBIAS_V_SHIFT)
#define AUDIO_MICBIAS_V_1P7_OR_2P2 (0)
#define AUDIO_MICBIAS_V_1P9_OR_2P4 (1)
#define AUDIO_MICBIAS_V_2P1_OR_2P7 (2)
#define AUDIO_MICBIAS_V_2P3_OR_3P0 (3)

#define AUDIO_ADPGAR_BYP_SHIFT (10)
#define AUDIO_ADPGAR_BYP_MASK (0x3 << AUDIO_ADPGAR_BYP_SHIFT)
#define AUDIO_ADPGAR_BYP_NORMAL (0)
#define AUDIO_ADPGAR_BYP_ADC_PGAR1_2_ADCR (1)
#define AUDIO_ADPGAR_BYP_HEADMIC_2_ADCR (2)
#define AUDIO_ADPGAR_BYP_ALL_DISCONNECTED (3)

#if (defined(CONFIG_ARCH_SCX15))
        #define AUDIO_HEAD_SDET_SHIFT (4)
#else
        #if (defined(CONFIG_ARCH_SCX35))
                #define AUDIO_HEAD_SDET_SHIFT (5)
        #endif
#endif
#define AUDIO_HEAD_SDET_MASK (0x3 << AUDIO_HEAD_SDET_SHIFT)
#define AUDIO_HEAD_SDET_2P1_OR_1P4 (3)
#define AUDIO_HEAD_SDET_2P3_OR_1P5 (2)
#define AUDIO_HEAD_SDET_2P5_OR_1P6 (1)
#define AUDIO_HEAD_SDET_2P7_OR_1P7 (0)

#if (defined(CONFIG_ARCH_SCX15))
        #define AUDIO_HEAD_INS_VREF_SHIFT (7)
#else
        #if (defined(CONFIG_ARCH_SCX35))
                #define AUDIO_HEAD_INS_VREF_SHIFT (8)
        #endif
#endif
#define AUDIO_HEAD_INS_VREF_MASK (0x3 << AUDIO_HEAD_INS_VREF_SHIFT)
#define AUDIO_HEAD_INS_VREF_2P1_OR_1P4 (3)
#define AUDIO_HEAD_INS_VREF_2P3_OR_1P5 (2)
#define AUDIO_HEAD_INS_VREF_2P5_OR_1P6 (1)
#define AUDIO_HEAD_INS_VREF_2P7_OR_1P7 (0)

#if (defined(CONFIG_ARCH_SCX15))
        #define AUDIO_HEAD_SBUT_SHIFT (0)
#else
        #if (defined(CONFIG_ARCH_SCX35))
                #define AUDIO_HEAD_SBUT_SHIFT (1)
        #endif
#endif
#define AUDIO_HEAD_SBUT_MASK (0xF << AUDIO_HEAD_SBUT_SHIFT)

#define AUDIO_HEADMIC_ADC_SEL (BIT(13))
#define AUDIO_HEAD_INS_HMBIAS_EN (BIT(11))

#if (defined(CONFIG_ARCH_SCX15))
        #define AUDIO_V2ADC_EN (BIT(15))
        #define AUDIO_HEAD_BUF_EN (BIT(14))
        #define AUDIO_HEAD2ADC_SEL_SHIFT (12)
        #define AUDIO_HEAD2ADC_SEL_MASK (0x3 << AUDIO_HEAD2ADC_SEL_SHIFT)
        #define AUDIO_HEAD2ADC_SEL_DISABLE (0)
        #define AUDIO_HEAD2ADC_SEL_MIC_IN (1)
        #define AUDIO_HEAD2ADC_SEL_L_INT (2)
        #define AUDIO_HEAD2ADC_SEL_DRO_L (3)
#else
        #if (defined(CONFIG_ARCH_SCX35))
                #define AUDIO_V2ADC_EN (BIT(14))
                #define AUDIO_HEAD_BUF_EN (BIT(15))
                #define AUDIO_HEAD2ADC_SEL_SHIFT (13)
                #define AUDIO_HEAD2ADC_SEL_MASK (0x1 << AUDIO_HEAD2ADC_SEL_SHIFT)
                #define AUDIO_HEAD2ADC_SEL_DISABLE (0)
                #define AUDIO_HEAD2ADC_SEL_MIC_IN (1)
                #define AUDIO_HEAD2ADC_SEL_L_INT (0)
                #define AUDIO_HEAD2ADC_SEL_DRO_L (0)
        #endif
#endif

#define AUDIO_HEAD_BUTTON (BIT(7))
#define AUDIO_HEAD_INSERT (BIT(6))
#define AUDIO_HEAD_INSERT_2 (BIT(5))

#define ABS(x) (((x) < (0)) ? (-(x)) : (x))
#define MAX(x,y) (((x) > (y)) ? (x) : (y))

#define headset_reg_read(addr)	\
    do {	\
	sci_adi_read(addr);	\
} while(0)

#define headset_reg_set_val(addr, val, mask, shift)  \
    do {    \
        uint32_t temp;    \
        temp = sci_adi_read(addr);  \
        temp = (temp & (~mask)) | ((val) << (shift));   \
        sci_adi_raw_write(addr, temp);    \
    } while(0)

#define headset_reg_clr_bit(addr, bit)   \
    do {    \
        uint32_t temp;    \
        temp = sci_adi_read(addr);  \
        temp = temp & (~bit);   \
        sci_adi_raw_write(addr, temp);  \
    } while(0)

#define headset_reg_set_bit(addr, bit)   \
    do {    \
        uint32_t temp;    \
        temp = sci_adi_read(addr);  \
        temp = temp | bit;  \
        sci_adi_raw_write(addr, temp);  \
    } while(0)

static inline uint32_t headset_reg_get_bit(uint32_t addr, uint32_t bit)
{
        uint32_t temp;
        temp = sci_adi_read(addr);
        temp = temp & bit;
        return temp;
}

typedef enum sprd_headset_type {
        HEADSET_NORMAL,
        HEADSET_NO_MIC,
        HEADSET_NORTH_AMERICA,
        HEADSET_APPLE,
        HEADSET_TYPE_ERR = -1,
} SPRD_HEADSET_TYPE;

static struct delayed_work reg_dump_work;
static struct workqueue_struct *reg_dump_work_queue;

/***polling ana_sts0 to avoid the hardware defect***/
#ifdef SPRD_STS_POLLING_EN
static struct delayed_work sts_check_work;
static struct workqueue_struct *sts_check_work_queue;
static int plug_state_class_g_on = 0;
static int sts_check_work_need_to_cancel = 1;
#endif
/***polling ana_sts0 to avoid the hardware defect***/

#ifdef ADPGAR_BYP_SELECT
static struct delayed_work adpgar_byp_select_work;
static struct workqueue_struct *adpgar_byp_select_work_queue;
#endif

static DEFINE_SPINLOCK(headmic_sleep_disable_lock);
static DEFINE_SPINLOCK(headmic_bias_lock);
static DEFINE_SPINLOCK(irq_button_lock);
static DEFINE_SPINLOCK(irq_detect_lock);
static int debug_level = 0;
static int adie_type = 100;
static int gpio_detect_value_last = 0;
static int gpio_button_value_last = 0;
static volatile int button_state_last = 0; //0==released, 1==pressed
static int current_key_code = KEY_RESERVED;
static int plug_state_last = 0; //if the hardware detected the headset is plug in, set plug_state_last = 1
static struct wake_lock headset_detect_wakelock;
static struct wake_lock headset_button_wakelock;
static struct semaphore headset_sem;
static struct sprd_headset headset = {
        .sdev = {
                .name = "h2w",
        },
};

//========================  audio codec  ========================
#define to_device(x) container_of((x), struct device, platform_data)

static struct sprd_headset_power {
	struct regulator *head_mic;
	struct regulator *vcom_buf;
	struct regulator *vbo;
} sprd_hts_power;

static int sprd_headset_power_get(struct device *dev,
				  struct regulator **regu, const char *id)
{
	if (!*regu) {
		*regu = regulator_get(dev, id);
		if (IS_ERR(*regu)) {
			pr_err("ERR:Failed to request %ld: %s\n",
			       PTR_ERR(*regu), id);
			*regu = 0;
			return PTR_ERR(*regu);
		}
	}
	return 0;
}

static int sprd_headset_power_init(struct device *dev)
{
	int ret = 0;
	ret =
	    sprd_headset_power_get(dev, &sprd_hts_power.head_mic,
				   "HEADMICBIAS");
	if (ret || (sprd_hts_power.head_mic == NULL)) {
		sprd_hts_power.head_mic = 0;
		return ret;
	}
	regulator_set_voltage(sprd_hts_power.head_mic, 950000, 950000);

	ret = sprd_headset_power_get(dev, &sprd_hts_power.vcom_buf, "VCOM_BUF");
	if (ret) {
		sprd_hts_power.vcom_buf = 0;
		goto __err1;
	}

	ret = sprd_headset_power_get(dev, &sprd_hts_power.vbo, "VBO");
	if (ret) {
		sprd_hts_power.vbo = 0;
		goto __err2;
	}

	goto __ok;
__err2:
	regulator_put(sprd_hts_power.vcom_buf);
__err1:
	regulator_put(sprd_hts_power.head_mic);
__ok:
	return ret;
}

static void sprd_headset_power_deinit(void)
{
	regulator_put(sprd_hts_power.head_mic);
	regulator_put(sprd_hts_power.vcom_buf);
	regulator_put(sprd_hts_power.vbo);
}

static int sprd_headset_audio_block_is_running(struct device *dev)
{
	return regulator_is_enabled(sprd_hts_power.vcom_buf)
	    || regulator_is_enabled(sprd_hts_power.vbo);
}

static int sprd_headset_audio_headmic_sleep_disable(struct device *dev, int on)
{
	int ret = 0;
	if (!sprd_hts_power.head_mic) {
		return -1;
	}

	if (on) {
			ret = regulator_enable(sprd_hts_power.vbo);
			ret =
			    regulator_set_mode(sprd_hts_power.head_mic,
					       REGULATOR_MODE_NORMAL);
	} else {
		ret = regulator_disable(sprd_hts_power.vbo);
		if (sprd_headset_audio_block_is_running(dev)) {
			ret =
			    regulator_set_mode(sprd_hts_power.head_mic,
					       REGULATOR_MODE_NORMAL);
		} else {
			ret =
			    regulator_set_mode(sprd_hts_power.head_mic,
					       REGULATOR_MODE_STANDBY);
		}
	}
	return ret;
}

static int sprd_headset_headmic_bias_control(struct device *dev, int on)
{
	int ret = 0;
	if (!sprd_hts_power.head_mic) {
		return -1;
	}
	if (on) {
		ret = regulator_enable(sprd_hts_power.head_mic);
	} else {
		ret = regulator_disable(sprd_hts_power.head_mic);
	}
	if (!ret) {
		/* Set HEADMIC_SLEEP when audio block closed */
		if (sprd_headset_audio_block_is_running(dev)) {
			ret =
			    regulator_set_mode(sprd_hts_power.head_mic,
					       REGULATOR_MODE_NORMAL);
		} else {
			ret =
			    regulator_set_mode(sprd_hts_power.head_mic,
					       REGULATOR_MODE_STANDBY);
		}
	}
	return ret;
}

static BLOCKING_NOTIFIER_HEAD(hp_chain_list);
int hp_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&hp_chain_list, nb);
}
EXPORT_SYMBOL(hp_register_notifier);

int hp_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&hp_chain_list, nb);
}
EXPORT_SYMBOL(hp_unregister_notifier);

static int hp_notifier_call_chain(unsigned long val)
{
    return (blocking_notifier_call_chain(&hp_chain_list, val, NULL)
            == NOTIFY_BAD) ? -EINVAL : 0;
}

int get_hp_plug_state(void)
{
	return !!plug_state_last;
}
EXPORT_SYMBOL(get_hp_plug_state);
//========================  audio codec  ========================

static int wrap_sci_adc_get_value(unsigned int channel, int scale)
{
	int count = 0;
	int average = 0;

	while(count < SCI_ADC_GET_VALUE_COUNT) {
		average += sci_adc_get_value(channel, scale);
		count++;
	}
	average /= SCI_ADC_GET_VALUE_COUNT;

	return average;
}

/*  on = 0: open headmic detect circuit */
static void headset_detect_circuit(unsigned on)
{
        if (on) {
                headset_reg_clr_bit(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_HEAD_INS_HMBIAS_EN);
        } else {
                headset_reg_set_bit(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_HEAD_INS_HMBIAS_EN);
        }
}

static void headset_detect_clk_en(void)
{
        //address:0x4003_8800+0x00
        headset_reg_set_bit(HEADMIC_DETECT_GLB_REG(0x00), (HDT_EN | AUD_EN));
        //address:0x4003_8800+0x04
        headset_reg_set_bit(HEADMIC_DETECT_GLB_REG(0x04), (CLK_AUD_HID_EN | CLK_AUD_HBD_EN));
}

static void headset_detect_init(void)
{
        headset_detect_clk_en();
        /* set headset detect voltage */
        headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_HEAD_SDET_2P5_OR_1P6, AUDIO_HEAD_SDET_MASK, AUDIO_HEAD_SDET_SHIFT);
        headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_HEAD_INS_VREF_2P1_OR_1P4, AUDIO_HEAD_INS_VREF_MASK, AUDIO_HEAD_INS_VREF_SHIFT);
        /*set headmicbias voltage*/
        headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG0), AUDIO_MICBIAS_V_2P3_OR_3P0, AUDIO_MICBIAS_V_MASK, AUDIO_MICBIAS_V_SHIFT);
        headset_reg_set_bit(HEADMIC_DETECT_REG(ANA_CFG0), AUDIO_MICBIAS_HV_EN);
}

static void headmic_sleep_disable(struct device *dev, int on);

static void headset_adc_en(int en)
{
        struct sprd_headset *ht = &headset;
        struct sprd_headset_platform_data *pdata = ht->platform_data;

        if (en) {
                headset_reg_set_bit(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_HEAD_BUF_EN);
                headset_reg_set_bit(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_V2ADC_EN);
                headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_HEAD2ADC_SEL_MIC_IN, AUDIO_HEAD2ADC_SEL_MASK, AUDIO_HEAD2ADC_SEL_SHIFT);
                headset_reg_set_bit(HEADMIC_DETECT_REG(ANA_CFG1), AUDIO_ICM_PLUS_EN);
                headmic_sleep_disable(to_device(pdata), 1);
        } else {
                if(debug_level <= 2) {
                        headset_reg_clr_bit(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_HEAD_BUF_EN);
                        headset_reg_clr_bit(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_V2ADC_EN);
                        headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_HEAD2ADC_SEL_DISABLE, AUDIO_HEAD2ADC_SEL_MASK, AUDIO_HEAD2ADC_SEL_SHIFT);
                }
                headmic_sleep_disable(to_device(pdata), 0);
        }
}

/* is_set = 1, headset_mic to AUXADC */
static void set_adc_to_headmic(unsigned is_set)
{
        headset_adc_en(1);

        if (is_set) {
                headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_HEAD2ADC_SEL_MIC_IN, AUDIO_HEAD2ADC_SEL_MASK, AUDIO_HEAD2ADC_SEL_SHIFT);
        } else {
                headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG20), AUDIO_HEAD2ADC_SEL_L_INT, AUDIO_HEAD2ADC_SEL_MASK, AUDIO_HEAD2ADC_SEL_SHIFT);
        }
}

static void headset_button_irq_threshold(int enable)
{
        struct sprd_headset *ht = &headset;
        struct sprd_headset_platform_data *pdata = ht->platform_data;
        int audio_head_sbut = 0;

        audio_head_sbut = pdata->irq_threshold_buttont;

        if (enable)
                headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG20), audio_head_sbut, AUDIO_HEAD_SBUT_MASK, AUDIO_HEAD_SBUT_SHIFT);
        else
                headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG20), 0xF, AUDIO_HEAD_SBUT_MASK, AUDIO_HEAD_SBUT_SHIFT);
}

static void headset_micbias_polling_en(int en)
{
        if(en) {
                //step 1: enable clk for register accessable
                headset_detect_clk_en();
                //step 2: start polling & set timer
                headset_reg_set_val(HEADMIC_BUTTON_REG(HID_CFG2), HID_TMR_T1T2_STEP_VAL, HID_TMR_T1T2_STEP_MASK, HID_TMR_T1T2_STEP_SHIFT);//step for T1 & T2 [9:5]
                headset_reg_set_val(HEADMIC_BUTTON_REG(HID_CFG2), HID_TMR_T0_VAL, HID_TMR_T0_MASK, HID_TMR_T0_SHIFT);//T0 timer count [4:0]
                headset_reg_set_val(HEADMIC_BUTTON_REG(HID_CFG3), HID_TMR_T1_VAL, HID_TMR_T1_MASK, HID_TMR_T1_SHIFT);//T1 timer count [15:0]
                headset_reg_set_val(HEADMIC_BUTTON_REG(HID_CFG4), HID_TMR_T2_VAL, HID_TMR_T2_MASK, HID_TMR_T2_SHIFT);//T2 timer count [15:0]
                //step 3: disable headmicbias
                headset_reg_clr_bit(HEADMIC_DETECT_REG(ANA_CFG0), BIT(5));
                //headset_reg_set_bit(HEADMIC_DETECT_REG(ANA_CFG0), BIT(1));
                PRINT_INFO("headmicbias polling enable\n");
                PRINT_DBG("ANA_CFG0(0x%08X)  HID_CFG0(0x%08X)  HID_CFG2(0x%08X)  HID_CFG3(0x%08X)  HID_CFG4(0x%08X)\n",
                          sci_adi_read(HEADMIC_DETECT_REG(ANA_CFG0)),
                          sci_adi_read(HEADMIC_BUTTON_REG(HID_CFG0)),
                          sci_adi_read(HEADMIC_BUTTON_REG(HID_CFG2)),
                          sci_adi_read(HEADMIC_BUTTON_REG(HID_CFG3)),
                          sci_adi_read(HEADMIC_BUTTON_REG(HID_CFG4)));
        } else {
                //step 1: enable headmicbias
                //headset_reg_clr_bit(HEADMIC_DETECT_REG(ANA_CFG0), BIT(1));
                headset_reg_set_bit(HEADMIC_DETECT_REG(ANA_CFG0), BIT(5));
                //step 2: stop polling
                PRINT_INFO("headmicbias polling disable\n");
                PRINT_DBG("ANA_CFG0(0x%08X)  HID_CFG0(0x%08X)  HID_CFG2(0x%08X)  HID_CFG3(0x%08X)  HID_CFG4(0x%08X)\n",
                          sci_adi_read(HEADMIC_DETECT_REG(ANA_CFG0)),
                          sci_adi_read(HEADMIC_BUTTON_REG(HID_CFG0)),
                          sci_adi_read(HEADMIC_BUTTON_REG(HID_CFG2)),
                          sci_adi_read(HEADMIC_BUTTON_REG(HID_CFG3)),
                          sci_adi_read(HEADMIC_BUTTON_REG(HID_CFG4)));
        }
}

static void headset_irq_button_enable(int enable, unsigned int irq)
{
        static int current_irq_state = 1;//irq is enabled after request_irq()

        if (1 == enable) {
                if (0 == current_irq_state) {
                        enable_irq(irq);
                        current_irq_state = 1;
                }
        } else {
                if (1 == current_irq_state) {
                        disable_irq_nosync(irq);
                        current_irq_state = 0;
                }
        }

        return;
}

static void headset_irq_detect_enable(int enable, unsigned int irq)
{
        static int current_irq_state = 1;//irq is enabled after request_irq()

        if (1 == enable) {
                if (0 == current_irq_state) {
                        enable_irq(irq);
                        current_irq_state = 1;
                }
        } else {
                if (1 == current_irq_state) {
                        disable_irq_nosync(irq);
                        current_irq_state = 0;
                }
        }

        return;
}

static void headmic_sleep_disable(struct device *dev, int on)
{
	static int current_power_state = 0;

	if (1 == on) {
		if (0 == current_power_state) {
			sprd_headset_audio_headmic_sleep_disable(dev, 1);
			current_power_state = 1;
		}
	} else {
		if (1 == current_power_state) {
			sprd_headset_audio_headmic_sleep_disable(dev, 0);
			current_power_state = 0;
		}
	}

	return;
}

static void headmicbias_power_on(struct device *dev, int on)
{
	static int current_power_state = 0;
	struct sprd_headset *ht = &headset;

	if (1 == on) {
		if (0 == current_power_state) {
			if(NULL != ht->platform_data->external_headmicbias_power_on)
				ht->platform_data->external_headmicbias_power_on(1);
			sprd_headset_headmic_bias_control(dev, 1);
			current_power_state = 1;
		}
	} else {
		if (1 == current_power_state) {
			if(NULL != ht->platform_data->external_headmicbias_power_on)
				ht->platform_data->external_headmicbias_power_on(0);
			sprd_headset_headmic_bias_control(dev, 0);
			current_power_state = 0;
		}
	}

	return;
}

static int ana_sts0_confirm(void)
{
        if(0 == headset_reg_get_bit(HEADMIC_DETECT_REG(ANA_STS0), AUDIO_HEAD_INSERT))
                return 0;
        else
                return 1;
}

static int adc_compare(int x1, int x2)
{
        int delta = 0;
        int max = 0;

        if((x1<100) && (x2<100))
                return 1;

        x1 = ((0==x1) ? 1 : (x1*100));
        x2 = ((0==x2) ? 1 : (x2*100));

        delta = ABS(x1-x2);
        max = MAX(x1, x2);

        if(delta < ((max*10)/100))
                return 1;
        else
                return 0;
}

static int adc_get_average(int gpio_num, int gpio_value)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int adc_average = 0;
	int success = 1;
	int adc[ADC_READ_COUNT] = {0};
	struct sprd_headset *ht = &headset;
	struct sprd_headset_platform_data *pdata = ht->platform_data;

	//================= debug =====================
	if(debug_level >= 3) {
		int count = 0;
		int adc_val = 0;
		int gpio_button = 0;
		int ana_cfg0 = 0;
		int ana_cfg1 = 0;
		int ana_cfg20 = 0;
		int ana_sts0 = 0;

		while(count < 20)
		{
			count++;
			adc_val = wrap_sci_adc_get_value(ADC_CHANNEL_HEADMIC, 0);
			gpio_button = gpio_get_value(pdata->gpio_button);
			ana_cfg0 = sci_adi_read(HEADMIC_DETECT_REG(ANA_CFG0));
			ana_cfg1 = sci_adi_read(HEADMIC_DETECT_REG(ANA_CFG1));
			ana_cfg20 = sci_adi_read(HEADMIC_DETECT_REG(ANA_CFG20));
			ana_sts0 = sci_adi_read(HEADMIC_DETECT_REG(ANA_STS0));

			PRINT_INFO("%2d:  gpio_button=%d  adc=%4d  ana_cfg0=0x%08X  ana_cfg1=0x%08X  ana_cfg20=0x%08X  ana_sts0=0x%08X\n",
				count, gpio_button, adc_val, ana_cfg0, ana_cfg1, ana_cfg20, ana_sts0);
			msleep(1);
		}
	}
	//================= debug =====================

	msleep(1);
	if(0 == headset_reg_get_bit(HEADMIC_DETECT_REG(ANA_CFG0), AUDIO_HEADMIC_SLEEP_EN))
		PRINT_INFO("AUDIO_HEADMIC_SLEEP_EN is disabled\n");

	for(i=0; i< ADC_READ_LOOP; i++) {
		if(gpio_get_value(gpio_num) != gpio_value) {
			PRINT_INFO("gpio value changed!!! the adc read operation aborted (step1)\n");
			return -1;
		}
		if(0 == ana_sts0_confirm()) {
			PRINT_INFO("ana_sts0_confirm failed!!! the adc read operation aborted (step2)\n");
			return -1;
		}
		adc[0] = wrap_sci_adc_get_value(ADC_CHANNEL_HEADMIC, 0);
		for(j=0; j<ADC_READ_COUNT-1; j++) {
			udelay(100);
			if(gpio_get_value(gpio_num) != gpio_value) {
				PRINT_INFO("gpio value changed!!! the adc read operation aborted (step3)\n");
				return -1;
			}
			if(0 == ana_sts0_confirm()) {
				PRINT_INFO("ana_sts0_confirm failed!!! the adc read operation aborted (step4)\n");
				return -1;
			}
			adc[j+1] = wrap_sci_adc_get_value(ADC_CHANNEL_HEADMIC, 0);
			if(0 == adc_compare(adc[j], adc[j+1])) {
				success = 0;
				for(k=0; k<=j+1; k++) {
					PRINT_DBG("adc[%d] = %d\n", k, adc[k]);
				}
				break;
			}
		}
		if(1 == success) {
			msleep(DBNC_CNT3_VALUE);
			if(gpio_get_value(gpio_num) != gpio_value) {
				PRINT_INFO("gpio value changed!!! the adc read operation aborted (step5)\n");
				return -1;
			}
			if(0 == ana_sts0_confirm()) {
				PRINT_INFO("ana_sts0_confirm failed!!! the adc read operation aborted (step6)\n");
				return -1;
			}
			for(i=0; i<ADC_READ_COUNT; i++) {
				adc_average += adc[i];
				PRINT_DBG("adc[%d] = %d\n", i, adc[i]);
			}
			adc_average = adc_average / ADC_READ_COUNT;
			PRINT_DBG("adc_get_average success, adc_average = %d\n", adc_average);
			return adc_average;
		}
		else if(i+1< ADC_READ_LOOP) {
			PRINT_INFO("adc_get_average failed, retrying count = %d\n", i+1);
			msleep(1);
		}
	}

	return -1;
}

static int headset_irq_set_irq_type(unsigned int irq, unsigned int type)
{
        struct sprd_headset *ht = &headset;
        struct irq_desc *irq_desc = NULL;
        unsigned int irq_flags = 0;
        int ret = -1;

        ret = irq_set_irq_type(irq, type);
        irq_desc = irq_to_desc(irq);
        irq_flags = irq_desc->action->flags;

        if(irq == ht->irq_button) {
                if(IRQF_TRIGGER_HIGH == type) {
                        PRINT_DBG("IRQF_TRIGGER_HIGH is set for irq_button(%d). irq_flags = 0x%08X, ret = %d\n",
							ht->irq_button, irq_flags, ret);
                }
                else if(IRQF_TRIGGER_LOW == type) {
                        PRINT_DBG("IRQF_TRIGGER_LOW is set for irq_button(%d). irq_flags = 0x%08X, ret = %d\n",
							ht->irq_button, irq_flags, ret);
                }
        }
        else if(irq == ht->irq_detect) {
                if(IRQF_TRIGGER_HIGH == type) {
                        PRINT_DBG("IRQF_TRIGGER_HIGH is set for irq_detect(%d). irq_flags = 0x%08X, ret = %d\n",
							ht->irq_detect, irq_flags, ret);
                }
                else if(IRQF_TRIGGER_LOW == type) {
                        PRINT_DBG("IRQF_TRIGGER_LOW is set for irq_detect(%d). irq_flags = 0x%08X, ret = %d\n",
							ht->irq_detect, irq_flags, ret);
                }
        }
        return 0;
}

static int headset_button_valid(int gpio_detect_value_current)
{
        struct sprd_headset *ht = &headset;
        struct sprd_headset_platform_data *pdata = ht->platform_data;
        int button_is_valid = 0;

        if(1 == pdata->irq_trigger_level_detect) {
                if(1 == gpio_detect_value_current)
                        button_is_valid = 1;
                else
                        button_is_valid = 0;
        } else {
                if(0 == gpio_detect_value_current)
                        button_is_valid = 1;
                else
                        button_is_valid = 0;
        }

        return button_is_valid;
}

static int headset_gpio_2_button_state(int gpio_button_value_current)
{
        struct sprd_headset *ht = &headset;
        struct sprd_headset_platform_data *pdata = ht->platform_data;
        int button_state_current = 0;

        if(1 == pdata->irq_trigger_level_button) {
                if(1 == gpio_button_value_current)
                        button_state_current = 1;
                else
                        button_state_current = 0;
        } else {
                if(0 == gpio_button_value_current)
                        button_state_current = 1;
                else
                        button_state_current = 0;
        }

        return button_state_current; //0==released, 1==pressed
}

static int headset_plug_confirm_by_adc(int last_gpio_detect_value)
{
	struct sprd_headset *ht = &headset;
	struct sprd_headset_platform_data *pdata = ht->platform_data;
	int adc_last = 0;
	int adc_current = 0;
	int count = 0;
	int adc_read_interval = 10;

	msleep(adc_read_interval);
	adc_last = adc_get_average(pdata->gpio_detect, last_gpio_detect_value);
	if(-1 == adc_last) {
		PRINT_INFO("headset_plug_confirm_by_adc failed!!!\n");
		return -1;
	}

	while(count < PLUG_CONFIRM_COUNT) {
		adc_current = adc_get_average(pdata->gpio_detect, last_gpio_detect_value);
		if(-1 == adc_current) {
			PRINT_INFO("headset_plug_confirm_by_adc failed!!!\n");
			return -1;
		}
		if(0 == adc_compare(adc_last, adc_current)) {
			PRINT_INFO("headset_plug_confirm_by_adc failed!!!\n");
			return -1;
		}
		adc_last = adc_current;
		count++;
		msleep(adc_read_interval);
	}
	PRINT_INFO("headset_plug_confirm_by_adc success!!!\n");
	return adc_current;
}

static SPRD_HEADSET_TYPE headset_type_detect(int last_gpio_detect_value)
{
        struct sprd_headset *ht = &headset;
        struct sprd_headset_platform_data *pdata = ht->platform_data;
        int adc_mic_average = 0;
        int adc_left_average = 0;
        int no_mic_retry_count = NO_MIC_RETRY_COUNT;

        ENTER;

        if(0 != pdata->gpio_switch)
                gpio_direction_output(pdata->gpio_switch, 0);
        else
                PRINT_INFO("automatic type switch is unsupported\n");

        headset_button_irq_threshold(1);
        headset_detect_init();
        headset_detect_circuit(1);

no_mic_retry:

        //get adc value of left
        if(EIC_AUD_HEAD_INST2 == pdata->gpio_detect) {
                PRINT_INFO("HEADSET_DETECT_GPIO=%d, matched the reference phone, now getting adc value of left\n", pdata->gpio_detect);
                set_adc_to_headmic(0);
                msleep(50);
                adc_left_average = headset_plug_confirm_by_adc(last_gpio_detect_value);
                if(-1 == adc_left_average)
                        return HEADSET_TYPE_ERR;
        } else {
                PRINT_INFO("HEADSET_DETECT_GPIO=%d, NOT matched the reference phone(GPIO_%d), set adc_left_average to 0\n",
					pdata->gpio_detect, EIC_AUD_HEAD_INST2);
                adc_left_average = 0;
        }

        //get adc value of mic
        set_adc_to_headmic(1);
        msleep(50);
        adc_mic_average = headset_plug_confirm_by_adc(last_gpio_detect_value);
        if(-1 == adc_mic_average)
                return HEADSET_TYPE_ERR;

        PRINT_INFO("adc_mic_average = %d\n", adc_mic_average);
        PRINT_INFO("adc_left_average = %d\n", adc_left_average);

        if((gpio_get_value(pdata->gpio_detect)) != last_gpio_detect_value) {
                PRINT_INFO("software debance (gpio check)!!!(headset_type_detect)\n");
                return HEADSET_TYPE_ERR;
        }

        if(0 == ana_sts0_confirm()) {
                PRINT_INFO("software debance (ana_sts0_confirm)!!!(headset_type_detect)\n");
                return HEADSET_TYPE_ERR;
        }

        if((adc_left_average < ADC_GND) && (adc_mic_average < pdata->adc_threshold_3pole_detect)) {
                if(0 != no_mic_retry_count) {
                        PRINT_INFO("no_mic_retry\n");
                        no_mic_retry_count--;
                        goto no_mic_retry;
                }
                return HEADSET_NO_MIC;
        }
        else if((adc_left_average < ADC_GND) && (adc_mic_average > ADC_GND))
                return HEADSET_NORMAL;
        else if((adc_left_average > ADC_GND) && (adc_mic_average > ADC_GND)
                && (ABS(adc_mic_average - adc_left_average) < ADC_GND))
                return HEADSET_NORTH_AMERICA;
        else
                return HEADSET_TYPE_ERR;

        return HEADSET_TYPE_ERR;
}

static void button_release_verify(void)
{
        struct sprd_headset *ht = &headset;

        if(1 == button_state_last) {
                input_event(ht->input_dev, EV_KEY, current_key_code, 0);
                input_sync(ht->input_dev);
                button_state_last = 0;
                PRINT_INFO("headset button released by force!!! current_key_code = %d(0x%04X)\n", current_key_code, current_key_code);

                if (1 == ht->platform_data->irq_trigger_level_button)
                        headset_irq_set_irq_type(ht->irq_button, IRQF_TRIGGER_HIGH);
                else
                        headset_irq_set_irq_type(ht->irq_button, IRQF_TRIGGER_LOW);
        }
}

static void headset_button_work_func(struct work_struct *work)
{
        struct sprd_headset *ht = &headset;
        struct sprd_headset_platform_data *pdata = ht->platform_data;
        int gpio_button_value_current = 0;
        int button_state_current = 0;
        int adc_mic_average = 0;
        int i = 0;

        down(&headset_sem);
        set_adc_to_headmic(1);

        ENTER;

        gpio_button_value_current = gpio_get_value(pdata->gpio_button);
        if(gpio_button_value_current != gpio_button_value_last) {
                PRINT_INFO("software debance (step 1: gpio check)!!!(headset_button_work_func)\n");
                #ifdef ADPGAR_BYP_SELECT
                queue_delayed_work(adpgar_byp_select_work_queue, &adpgar_byp_select_work, msecs_to_jiffies(0));
                #endif
                goto out;
        }

        button_state_current = headset_gpio_2_button_state(gpio_button_value_current);

        if(1 == button_state_current) {//pressed!
                if(ht->platform_data->nbuttons > 0) {
                        adc_mic_average = adc_get_average(pdata->gpio_button, gpio_button_value_last);
                        if(-1 == adc_mic_average) {
                                PRINT_INFO("software debance (step 3: adc check)!!!(headset_button_work_func)(pressed)\n");
                                #ifdef ADPGAR_BYP_SELECT
                                queue_delayed_work(adpgar_byp_select_work_queue, &adpgar_byp_select_work, msecs_to_jiffies(0));
                                #endif
                                goto out;
                        }
                        PRINT_INFO("adc_mic_average = %d\n", adc_mic_average);
                        for (i = 0; i < ht->platform_data->nbuttons; i++) {
                                if (adc_mic_average >= ht->platform_data->headset_buttons[i].adc_min &&
                                    adc_mic_average < ht->platform_data->headset_buttons[i].adc_max) {
                                        current_key_code = ht->platform_data->headset_buttons[i].code;
                                        break;
                                }
                                current_key_code = KEY_RESERVED;
                        }
                }

                if(0 == button_state_last) {
                        input_event(ht->input_dev, EV_KEY, current_key_code, 1);
                        input_sync(ht->input_dev);
                        button_state_last = 1;
                        PRINT_INFO("headset button pressed! current_key_code = %d(0x%04X)\n", current_key_code, current_key_code);
                } else {
                        PRINT_ERR("headset button pressed already! current_key_code = %d(0x%04X)\n", current_key_code, current_key_code);
                        #ifdef ADPGAR_BYP_SELECT
                        queue_delayed_work(adpgar_byp_select_work_queue, &adpgar_byp_select_work, msecs_to_jiffies(0));
                        #endif
                }

                if (1 == ht->platform_data->irq_trigger_level_button)
                        headset_irq_set_irq_type(ht->irq_button, IRQF_TRIGGER_LOW);
                else
                        headset_irq_set_irq_type(ht->irq_button, IRQF_TRIGGER_HIGH);

                headset_irq_button_enable(1, ht->irq_button);
        } else { //released!
                if(1 == button_state_last) {
                        input_event(ht->input_dev, EV_KEY, current_key_code, 0);
                        input_sync(ht->input_dev);
                        button_state_last = 0;
                        PRINT_INFO("headset button released! current_key_code = %d(0x%04X)\n", current_key_code, current_key_code);
                } else
                        PRINT_ERR("headset button released already! current_key_code = %d(0x%04X)\n", current_key_code, current_key_code);

                if (1 == ht->platform_data->irq_trigger_level_button)
                        headset_irq_set_irq_type(ht->irq_button, IRQF_TRIGGER_HIGH);
                else
                        headset_irq_set_irq_type(ht->irq_button, IRQF_TRIGGER_LOW);

                headset_irq_button_enable(1, ht->irq_button);
        }
out:
        headset_adc_en(0);
        headset_irq_button_enable(1, ht->irq_button);
        //wake_unlock(&headset_button_wakelock);
        up(&headset_sem);
        return;
}

static void headset_detect_work_func(struct work_struct *work)
{
        struct sprd_headset *ht = &headset;
        struct sprd_headset_platform_data *pdata = ht->platform_data;
        SPRD_HEADSET_TYPE headset_type;
        int plug_state_current = 0;
        int gpio_detect_value_current = 0;

        down(&headset_sem);

        ENTER;

        if(0 == plug_state_last) {
                if(adie_type >= 3) {
			headmicbias_power_on(to_device(pdata), 1);
                }
        }

        if(0 == plug_state_last) {
                gpio_detect_value_current = gpio_get_value(pdata->gpio_detect);
                PRINT_INFO("gpio_detect_value_current = %d, gpio_detect_value_last = %d, plug_state_last = %d\n",
                           gpio_detect_value_current, gpio_detect_value_last, plug_state_last);

                if(gpio_detect_value_current != gpio_detect_value_last) {
                        PRINT_INFO("software debance (step 1)!!!(headset_detect_work_func)\n");
                        goto out;
                }

                if(1 == pdata->irq_trigger_level_detect) {
                        if(1 == gpio_detect_value_current)
                                plug_state_current = 1;
                        else
                                plug_state_current = 0;
                } else {
                        if(0 == gpio_detect_value_current)
                                plug_state_current = 1;
                        else
                                plug_state_current = 0;
                }
        }
        else
                plug_state_current = 0;//no debounce for plug out!!!

        if(1 == plug_state_current && 0 == plug_state_last) {
                headset_type = headset_type_detect(gpio_detect_value_last);
                headset_adc_en(0);
                switch (headset_type) {
                case HEADSET_TYPE_ERR:
                        PRINT_INFO("headset_type = %d (HEADSET_TYPE_ERR)\n", headset_type);
                        goto out;
                case HEADSET_NORTH_AMERICA:
                        PRINT_INFO("headset_type = %d (HEADSET_NORTH_AMERICA)\n", headset_type);
                        if(0 != pdata->gpio_switch)
                                gpio_direction_output(pdata->gpio_switch, 1);
                        break;
                case HEADSET_NORMAL:
                        PRINT_INFO("headset_type = %d (HEADSET_NORMAL)\n", headset_type);
                        if(0 != pdata->gpio_switch)
                                gpio_direction_output(pdata->gpio_switch, 0);
                        break;
                case HEADSET_NO_MIC:
                        PRINT_INFO("headset_type = %d (HEADSET_NO_MIC)\n", headset_type);
                        if(0 != pdata->gpio_switch)
                                gpio_direction_output(pdata->gpio_switch, 0);
                        break;
                case HEADSET_APPLE:
                        PRINT_INFO("headset_type = %d (HEADSET_APPLE)\n", headset_type);
                        PRINT_INFO("we have not yet implemented this in the code\n");
                        break;
                default:
                        PRINT_INFO("headset_type = %d (HEADSET_UNKNOWN)\n", headset_type);
                        break;
                }

                if(headset_type == HEADSET_NO_MIC)
                        ht->headphone = 1;
                else
                        ht->headphone = 0;

                if (ht->headphone) {
                        headset_button_irq_threshold(0);
                        headset_irq_button_enable(0, ht->irq_button);

                        ht->type = BIT_HEADSET_NO_MIC;
			hp_notifier_call_chain(ht->type);
                        switch_set_state(&ht->sdev, ht->type);
                        PRINT_INFO("headphone plug in (headset_detect_work_func)\n");
                } else {
                        headset_button_irq_threshold(1);

                        if((HEADSET_NORTH_AMERICA == headset_type) && (0 == pdata->gpio_switch)) {
                                headset_irq_button_enable(0, ht->irq_button);
                                PRINT_INFO("HEADSET_NORTH_AMERICA is not supported by your hardware! so disable the button irq!\n");
                        } else {
                                if (1 == ht->platform_data->irq_trigger_level_button)
                                        headset_irq_set_irq_type(ht->irq_button, IRQF_TRIGGER_HIGH);
                                else
                                        headset_irq_set_irq_type(ht->irq_button, IRQF_TRIGGER_LOW);

                                headset_irq_button_enable(1, ht->irq_button);
                        }

                        ht->type = BIT_HEADSET_MIC;
			hp_notifier_call_chain(ht->type);
                        switch_set_state(&ht->sdev, ht->type);
                        PRINT_INFO("headset plug in (headset_detect_work_func)\n");
                        #ifdef ADPGAR_BYP_SELECT
                        queue_delayed_work(adpgar_byp_select_work_queue, &adpgar_byp_select_work, msecs_to_jiffies(500));
                        #endif
                }

                /***polling ana_sts0 to avoid the hardware defect***/
#ifdef SPRD_STS_POLLING_EN
                if ((1 != adie_type) && (adie_type < 5)) {
                        plug_state_class_g_on = 1;
                        sts_check_work_need_to_cancel = 0;
                        queue_delayed_work(sts_check_work_queue, &sts_check_work, msecs_to_jiffies(1000));
                        PRINT_INFO("start sts_check_work_queue\n");
                }
                else
                        PRINT_INFO("sts_check_work_queue is no need to start\n");
#else
                PRINT_INFO("sts_check_work_queue is not used\n");
#endif
                /***polling ana_sts0 to avoid the hardware defect***/

                plug_state_last = 1;

                if(1 == pdata->irq_trigger_level_detect)
                        headset_irq_set_irq_type(ht->irq_detect, IRQF_TRIGGER_LOW);
                else
                        headset_irq_set_irq_type(ht->irq_detect, IRQF_TRIGGER_HIGH);

                if (adie_type >= 5) {
                        headset_reg_set_bit(ANA_CTL_GLB_BASE + ARM_MF_REG, HEAD_INSERT_EIC_EN);
                        headset_reg_clr_bit(ANA_CTL_GLB_BASE + ARM_MF_REG, HEAD_INSERT2_EIC_EN);
                        PRINT_INFO("disable HEAD_INSERT2_EIC_EN, enable HEAD_INSERT_EIC_EN\n");
                }

                headset_irq_detect_enable(1, ht->irq_detect);
        } else if(0 == plug_state_current && 1 == plug_state_last) {

                headset_irq_button_enable(0, ht->irq_button);
                button_release_verify();

                /***polling ana_sts0 to avoid the hardware defect***/
#ifdef SPRD_STS_POLLING_EN
                if ((1 != adie_type) && (adie_type < 5)) {
                        sts_check_work_need_to_cancel = 1;
                        if(0 == plug_state_class_g_on) {
                                PRINT_INFO("plug out already!!!\n");
                                goto plug_out_already;
                        }
                        plug_state_class_g_on = 0;
                }
#endif
                /***polling ana_sts0 to avoid the hardware defect***/

                if (ht->headphone) {
                        PRINT_INFO("headphone plug out (headset_detect_work_func)\n");
                } else {
                        PRINT_INFO("headset plug out (headset_detect_work_func)\n");
                }
                ht->type = BIT_HEADSET_OUT;
		hp_notifier_call_chain(ht->type);
                switch_set_state(&ht->sdev, ht->type);
plug_out_already:
                plug_state_last = 0;

                if(1 == pdata->irq_trigger_level_detect)
                        headset_irq_set_irq_type(ht->irq_detect, IRQF_TRIGGER_HIGH);
                else
                        headset_irq_set_irq_type(ht->irq_detect, IRQF_TRIGGER_LOW);

                if (adie_type >= 5) {
                        headset_reg_set_bit(ANA_CTL_GLB_BASE + ARM_MF_REG, HEAD_INSERT2_EIC_EN);
                        headset_reg_clr_bit(ANA_CTL_GLB_BASE + ARM_MF_REG, HEAD_INSERT_EIC_EN);
                        PRINT_INFO("disable HEAD_INSERT_EIC_EN, enable HEAD_INSERT2_EIC_EN\n");
                }

                headset_irq_detect_enable(1, ht->irq_detect);
        } else {
                PRINT_INFO("irq_detect must be enabled anyway!!!\n");
                goto out;
        }
out:

        if(0 == plug_state_last) {
                if(adie_type >= 3) {
			headmicbias_power_on(to_device(pdata), 0);
                }
        }

        headset_irq_detect_enable(1, ht->irq_detect);
        //wake_unlock(&headset_detect_wakelock);
        up(&headset_sem);
        return;
}

/***polling ana_sts0 to avoid the hardware defect***/
#ifdef SPRD_STS_POLLING_EN
static void headset_sts_check_func(struct work_struct *work)
{
        int ana_sts0 = 0;
        int ana_sts0_debance = 0;
        struct sprd_headset *ht = &headset;
        struct sprd_headset_platform_data *pdata = ht->platform_data;
        SPRD_HEADSET_TYPE headset_type;

        down(&headset_sem);

        ENTER;

        if(1 == sts_check_work_need_to_cancel)
                goto out;

        ana_sts0_debance = sci_adi_read(ANA_AUDCFGA_INT_BASE+ANA_STS0);//arm base address:0x40038600
        msleep(100);
        ana_sts0 = sci_adi_read(ANA_AUDCFGA_INT_BASE+ANA_STS0);//arm base address:0x40038600
        if((0x00000060 & ana_sts0) != (0x00000060 & ana_sts0_debance)) {
                PRINT_INFO("software debance!!!(headset_sts_check_func)\n");
                goto out;
        }

        if(((0x00000060 & ana_sts0) != 0x00000060) && (1 == plug_state_class_g_on)) {

                headset_irq_button_enable(0, ht->irq_button);
                button_release_verify();

                if (ht->headphone) {
                        PRINT_INFO("headphone plug out (headset_sts_check_func)\n");
                } else {
                        PRINT_INFO("headset plug out (headset_sts_check_func)\n");
                }
                ht->type = BIT_HEADSET_OUT;
		hp_notifier_call_chain(ht->type);
                switch_set_state(&ht->sdev, ht->type);
                plug_state_class_g_on = 0;
        }
        if(((0x00000060 & ana_sts0) == 0x00000060) && (0 == plug_state_class_g_on)) {
                headset_type = headset_type_detect(gpio_get_value(pdata->gpio_detect));
                headset_adc_en(0);
                switch (headset_type) {
                case HEADSET_TYPE_ERR:
                        PRINT_INFO("headset_type = %d (HEADSET_TYPE_ERR)\n", headset_type);
                        goto out;
                case HEADSET_NORTH_AMERICA:
                        PRINT_INFO("headset_type = %d (HEADSET_NORTH_AMERICA)\n", headset_type);
                        if(0 != pdata->gpio_switch)
                                gpio_direction_output(pdata->gpio_switch, 1);
                        break;
                case HEADSET_NORMAL:
                        PRINT_INFO("headset_type = %d (HEADSET_NORMAL)\n", headset_type);
                        if(0 != pdata->gpio_switch)
                                gpio_direction_output(pdata->gpio_switch, 0);
                        break;
                case HEADSET_NO_MIC:
                        PRINT_INFO("headset_type = %d (HEADSET_NO_MIC)\n", headset_type);
                        if(0 != pdata->gpio_switch)
                                gpio_direction_output(pdata->gpio_switch, 0);
                        break;
                case HEADSET_APPLE:
                        PRINT_INFO("headset_type = %d (HEADSET_APPLE)\n", headset_type);
                        PRINT_INFO("we have not yet implemented this in the code\n");
                        break;
                default:
                        PRINT_INFO("headset_type = %d (HEADSET_UNKNOWN)\n", headset_type);
                        break;
                }

                if(headset_type == HEADSET_NO_MIC)
                        ht->headphone = 1;
                else
                        ht->headphone = 0;

                if (ht->headphone) {
                        headset_button_irq_threshold(0);
                        headset_irq_button_enable(0, ht->irq_button);

                        ht->type = BIT_HEADSET_NO_MIC;
			hp_notifier_call_chain(ht->type);
                        switch_set_state(&ht->sdev, ht->type);
                        PRINT_INFO("headphone plug in (headset_sts_check_func)\n");
                } else {
                        headset_button_irq_threshold(1);

                        if((HEADSET_NORTH_AMERICA == headset_type) && (0 == pdata->gpio_switch)) {
                                headset_irq_button_enable(0, ht->irq_button);
                                PRINT_INFO("HEADSET_NORTH_AMERICA is not supported by your hardware! so disable the button irq!\n");
                        } else {
                                if (1 == ht->platform_data->irq_trigger_level_button)
                                        headset_irq_set_irq_type(ht->irq_button, IRQF_TRIGGER_HIGH);
                                else
                                        headset_irq_set_irq_type(ht->irq_button, IRQF_TRIGGER_LOW);

                                headset_irq_button_enable(1, ht->irq_button);
                        }

                        ht->type = BIT_HEADSET_MIC;
			hp_notifier_call_chain(ht->type);
                        switch_set_state(&ht->sdev, ht->type);
                        PRINT_INFO("headset plug in (headset_sts_check_func)\n");
                }
                plug_state_class_g_on = 1;
        }
out:
        if(0 == sts_check_work_need_to_cancel)
                queue_delayed_work(sts_check_work_queue, &sts_check_work, msecs_to_jiffies(1000));
        else
                PRINT_INFO("sts_check_work cancelled\n");

        up(&headset_sem);
        return;
}
#endif
/***polling ana_sts0 to avoid the hardware defect***/

#ifdef ADPGAR_BYP_SELECT
static void adpgar_byp_select_func(struct work_struct *work)
{
        headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG5), AUDIO_ADPGAR_BYP_HEADMIC_2_ADCR, AUDIO_ADPGAR_BYP_MASK, AUDIO_ADPGAR_BYP_SHIFT);
        PRINT_INFO("ANA_CFG5 = 0x%08X\n", sci_adi_read(HEADMIC_DETECT_REG(ANA_CFG5)));
        msleep(100);
        headset_reg_set_val(HEADMIC_DETECT_REG(ANA_CFG5), AUDIO_ADPGAR_BYP_NORMAL, AUDIO_ADPGAR_BYP_MASK, AUDIO_ADPGAR_BYP_SHIFT);
        PRINT_INFO("ANA_CFG5 = 0x%08X\n", sci_adi_read(HEADMIC_DETECT_REG(ANA_CFG5)));
        PRINT_INFO("adpgar_byp_select_func\n");
}
#endif

static void reg_dump_func(struct work_struct *work)
{
        int adc_mic = 0;

        int gpio_detect = 0;
        int gpio_button = 0;

        int ana_sts0 = 0;
        int ana_cfg0 = 0;
        int ana_cfg1 = 0;
        int ana_cfg20 = 0;

        int hid_cfg0 = 0;
        int hid_cfg2 = 0;
        int hid_cfg3 = 0;
        int hid_cfg4 = 0;

        int arm_module_en = 0;
        int arm_clk_en = 0;

        adc_mic = wrap_sci_adc_get_value(ADC_CHANNEL_HEADMIC, 0);

        gpio_detect = gpio_get_value(headset.platform_data->gpio_detect);
        gpio_button = gpio_get_value(headset.platform_data->gpio_button);

        sci_adi_write(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_AUD_EN, BIT_ANA_AUD_EN);//arm base address:0x40038800 for register accessable
        ana_cfg0 = sci_adi_read(ANA_AUDCFGA_INT_BASE+ANA_CFG0);//arm base address:0x40038600
        ana_cfg1 = sci_adi_read(ANA_AUDCFGA_INT_BASE+ANA_CFG1);//arm base address:0x40038600
        ana_cfg20 = sci_adi_read(ANA_AUDCFGA_INT_BASE+ANA_CFG20);//arm base address:0x40038600
        ana_sts0 = sci_adi_read(ANA_AUDCFGA_INT_BASE+ANA_STS0);//arm base address:0x40038600

        sci_adi_write(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_HDT_EN, BIT_ANA_HDT_EN);//arm base address:0x40038800 for register accessable
        sci_adi_write(ANA_REG_GLB_ARM_CLK_EN, BIT_CLK_AUD_HID_EN, BIT_CLK_AUD_HID_EN);//arm base address:0x40038800 for register accessable

        hid_cfg2 = sci_adi_read(ANA_HDT_INT_BASE+HID_CFG2);//arm base address:0x40038700
        hid_cfg3 = sci_adi_read(ANA_HDT_INT_BASE+HID_CFG3);//arm base address:0x40038700
        hid_cfg4 = sci_adi_read(ANA_HDT_INT_BASE+HID_CFG4);//arm base address:0x40038700
        hid_cfg0 = sci_adi_read(ANA_HDT_INT_BASE+HID_CFG0);//arm base address:0x40038700

        arm_module_en = sci_adi_read(ANA_REG_GLB_ARM_MODULE_EN);
        arm_clk_en = sci_adi_read(ANA_REG_GLB_ARM_CLK_EN);

        PRINT_INFO("GPIO_%03d(det)=%d    GPIO_%03d(but)=%d    adc_mic=%d\n",
                   headset.platform_data->gpio_detect, gpio_detect,
                   headset.platform_data->gpio_button, gpio_button,
                   adc_mic);
        PRINT_INFO("arm_module_en|arm_clk_en|ana_cfg0  |ana_cfg1  |ana_cfg20 |ana_sts0  |hid_cfg2  |hid_cfg3  |hid_cfg4  |hid_cfg0\n");
        PRINT_INFO("0x%08X   |0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X\n",
                   arm_module_en, arm_clk_en, ana_cfg0, ana_cfg1, ana_cfg20, ana_sts0, hid_cfg2, hid_cfg3, hid_cfg4, hid_cfg0);
#if 0
        int i = 0;
        int hbd[20] = {0};

        for(i=0; i<20; i++)
                hbd[i] = sci_adi_read(ANA_HDT_INT_BASE + (i*4));

        PRINT_INFO(" hbd_cfg0 | hbd_cfg1 | hbd_cfg2 | hbd_cfg3 | hbd_cfg4 | hbd_cfg5 | hbd_cfg6 | hbd_cfg7 | hbd_cfg8 | hbd_cfg9\n");
        PRINT_INFO("0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X\n",
                   hbd[0], hbd[1], hbd[2], hbd[3], hbd[4], hbd[5], hbd[6], hbd[7], hbd[8], hbd[9]);

        PRINT_INFO("hbd_cfg10 |hbd_cfg11 |hbd_cfg12 |hbd_cfg13 |hbd_cfg14 |hbd_cfg15 |hbd_cfg16 | hbd_sts0 | hbd_sts1 | hbd_sts2\n");
        PRINT_INFO("0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X\n",
                   hbd[10], hbd[11], hbd[12], hbd[13], hbd[14], hbd[15], hbd[16], hbd[17], hbd[18], hbd[19]);
#endif

        if (debug_level >= 2)
                queue_delayed_work(reg_dump_work_queue, &reg_dump_work, msecs_to_jiffies(500));
        return;
}

/**
 * some phone boards, when no headset plugin, the button headset_button_valid()
 * will always return 0, so we need to disable headset_button irq in here
 * otherwise button irq will always happen.
 */
volatile int headset_button_hardware_bug_fix_stage = 1;
static void headset_button_hardware_bug_fix(struct sprd_headset *ht)
{
	if (headset_button_hardware_bug_fix_stage) {
		headset_irq_button_enable(0, ht->irq_button); // disable button irq before headset detected
		pr_err("This phone board headset button circuit has bug, please fix the hardware\n");
	}
}

static irqreturn_t headset_button_irq_handler(int irq, void *dev)
{
        struct sprd_headset *ht = dev;
        int button_state_current = 0;
        int gpio_button_value_current = 0;

        if(0 == headset_button_valid(gpio_get_value(ht->platform_data->gpio_detect))) {
                PRINT_INFO("headset_button_irq_handler: button is invalid!!! IRQ_%d(GPIO_%d) = %d, ANA_STS0 = 0x%08X\n",
                        ht->irq_button, ht->platform_data->gpio_button, gpio_button_value_last,
                        sci_adi_read(ANA_AUDCFGA_INT_BASE+ANA_STS0));
				headset_button_hardware_bug_fix(ht);

                if(0 == plug_state_last)
                        headset_irq_button_enable(0, ht->irq_button);

                return IRQ_HANDLED;
        }

        gpio_button_value_current = gpio_get_value(ht->platform_data->gpio_button);
        button_state_current = headset_gpio_2_button_state(gpio_button_value_current);
        #ifdef ADPGAR_BYP_SELECT
        if(0 == button_state_current) {
                queue_delayed_work(adpgar_byp_select_work_queue, &adpgar_byp_select_work, msecs_to_jiffies(0));
        }
        #endif
        if(button_state_current == button_state_last) {
                PRINT_INFO("button state check failed!!! maybe the release is too quick. button_state_current=%d, button_state_last=%d\n",
					button_state_current, button_state_last);
                return IRQ_HANDLED;
        }

        headset_irq_button_enable(0, ht->irq_button);
        wake_lock_timeout(&headset_button_wakelock, msecs_to_jiffies(2000));
        gpio_button_value_last = gpio_button_value_current;
        PRINT_DBG("headset_button_irq_handler: IRQ_%d(GPIO_%d) = %d, ANA_STS0 = 0x%08X\n",
                  ht->irq_button, ht->platform_data->gpio_button, gpio_button_value_last,
                  sci_adi_read(ANA_AUDCFGA_INT_BASE+ANA_STS0));
        queue_work(ht->button_work_queue, &ht->work_button);
        return IRQ_HANDLED;
}

static irqreturn_t headset_detect_irq_handler(int irq, void *dev)
{
        struct sprd_headset *ht = dev;

        headset_irq_button_enable(0, ht->irq_button);
        headset_irq_detect_enable(0, ht->irq_detect);
        wake_lock_timeout(&headset_detect_wakelock, msecs_to_jiffies(2000));
        gpio_detect_value_last = gpio_get_value(ht->platform_data->gpio_detect);
        PRINT_DBG("headset_detect_irq_handler: IRQ_%d(GPIO_%d) = %d, ANA_STS0 = 0x%08X\n",
                  ht->irq_detect, ht->platform_data->gpio_detect, gpio_detect_value_last,
                  sci_adi_read(ANA_AUDCFGA_INT_BASE+ANA_STS0));
        queue_delayed_work(ht->detect_work_queue, &ht->work_detect, msecs_to_jiffies(0));
        return IRQ_HANDLED;
}

//================= create sys fs for debug ===================
//============= /sys/kernel/headset/debug_level ===============

static ssize_t headset_debug_level_show(struct kobject *kobj, struct kobj_attribute *attr, char *buff)
{
        PRINT_INFO("debug_level = %d\n", debug_level);
        return sprintf(buff, "%d\n", debug_level);
}

static ssize_t headset_debug_level_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buff, size_t len)
{
        unsigned long level = simple_strtoul(buff, NULL, 10);
        debug_level = level;
        PRINT_INFO("debug_level = %d\n", debug_level);
        if(debug_level >= 2)
                queue_delayed_work(reg_dump_work_queue, &reg_dump_work, msecs_to_jiffies(500));
        return len;
}

static struct kobject *headset_debug_kobj = NULL;
static struct kobj_attribute headset_debug_attr =
        __ATTR(debug_level, 0644, headset_debug_level_show, headset_debug_level_store);

static int headset_debug_sysfs_init(void)
{
        int ret = -1;

        headset_debug_kobj = kobject_create_and_add("headset", kernel_kobj);
        if (headset_debug_kobj == NULL) {
                ret = -ENOMEM;
                PRINT_ERR("register sysfs failed. ret = %d\n", ret);
                return ret;
        }

        ret = sysfs_create_file(headset_debug_kobj, &headset_debug_attr.attr);
        if (ret) {
                PRINT_ERR("create sysfs failed. ret = %d\n", ret);
                return ret;
        }

        PRINT_INFO("headset_debug_sysfs_init success\n");
        return ret;
}
//================= create sys fs for debug ===================

#ifdef CONFIG_OF
static struct sprd_headset_platform_data *headset_detect_parse_dt(
                         struct device *dev)
{
	struct sprd_headset_platform_data *pdata;
	struct device_node *np = dev->of_node,*buttons_np = NULL;
	int ret;
	struct headset_buttons *buttons_data;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}
	ret = of_property_read_u32(np, "gpio_switch", &pdata->gpio_switch);
	if(ret){
		dev_err(dev, "fail to get gpio_switch\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "gpio_detect", &pdata->gpio_detect);
	if(ret){
		dev_err(dev, "fail to get gpio_detect\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "gpio_button", &pdata->gpio_button);
	if(ret){
		dev_err(dev, "fail to get gpio_button\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "irq_trigger_level_detect", &pdata->irq_trigger_level_detect);
	if(ret){
		dev_err(dev, "fail to get irq_trigger_level_detect\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "irq_trigger_level_button", &pdata->irq_trigger_level_button);
	if(ret){
		dev_err(dev, "fail to get irq_trigger_level_button\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "adc_threshold_3pole_detect", &pdata->adc_threshold_3pole_detect);
	if(ret){
		dev_err(dev, "fail to get adc_threshold_3pole_detect\n");
		goto fail;
	}

	ret = of_property_read_u32(np, "adc_threshold_4pole_detect", &pdata->adc_threshold_4pole_detect);
	if(ret){
		dev_err(dev, "fail to get adc_threshold_4pole_detect\n");
		goto fail;
	}

	ret = of_property_read_u32(np, "irq_threshold_buttont", &pdata->irq_threshold_buttont);
	if(ret){
		dev_err(dev, "fail to get irq_threshold_buttont\n");
		goto fail;
	}

	ret = of_property_read_u32(np, "voltage_headmicbias", &pdata->voltage_headmicbias);
	if(ret){
		dev_err(dev, "fail to get voltage_headmicbias\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "nbuttons", &pdata->nbuttons);
	if(ret){
		dev_err(dev, "fail to get nbuttons\n");
		goto fail;
	}

	buttons_data = kzalloc(pdata->nbuttons*sizeof(*buttons_data),GFP_KERNEL);
	if (!buttons_data) {
		dev_err(dev, "could not allocate memory for headset_buttons\n");
		goto fail;
	}
        pdata->headset_buttons = buttons_data;

	for_each_child_of_node(np,buttons_np){

		ret = of_property_read_u32(buttons_np, "adc_min", &buttons_data->adc_min);
		if (ret) {
			dev_err(dev, "fail to get adc_min\n");
			goto fail_buttons_data;
		}
		ret = of_property_read_u32(buttons_np, "adc_max", &buttons_data->adc_max);
		if (ret) {
			dev_err(dev, "fail to get adc_max\n");
			goto fail_buttons_data;
		}
		ret = of_property_read_u32(buttons_np, "code", &buttons_data->code);
		if (ret) {
			dev_err(dev, "fail to get code\n");
			goto fail_buttons_data;
		}
		ret = of_property_read_u32(buttons_np, "type", &buttons_data->type);
		if (ret) {
			dev_err(dev, "fail to get type\n");
			goto fail_buttons_data;
		}
		printk("device tree data: adc_min = %d adc_max = %d code = %d type = %d \n", buttons_data->adc_min,
			buttons_data->adc_max, buttons_data->code, buttons_data->type);
		buttons_data++;
	};

	return pdata;

fail_buttons_data:
	kfree(buttons_data);
	pdata->headset_buttons = NULL;
fail:
	kfree(pdata);
	return NULL;
}
#endif

static int headset_detect_probe(struct platform_device *pdev)
{
        struct sprd_headset_platform_data *pdata = pdev->dev.platform_data;
        struct sprd_headset *ht = &headset;
        unsigned long irqflags = 0;
        struct input_dev *input_dev = NULL;
        int i = 0;
        int ret = -1;
        int ana_sts0 = 0;
        int adie_chip_id_low = 0;
        int adie_chip_id_high = 0;

#ifdef CONFIG_OF
        struct device_node *np = pdev->dev.of_node;
        if (pdev->dev.of_node && !pdata){
                pdata = headset_detect_parse_dt(&pdev->dev);
                if(pdata)
                        pdev->dev.platform_data = pdata;
        }
        if (!pdata) {
                printk(KERN_WARNING "headset_detect_probe get platform_data NULL\n");
                ret = -EINVAL;
                goto fail_to_get_platform_data;
        }
#endif

        adie_chip_id_low = sci_adi_read(ANA_CTL_GLB_BASE+ADIE_CHID_LOW);//A-die chip id LOW
        adie_chip_id_high = sci_adi_read(ANA_CTL_GLB_BASE+ADIE_CHID_HIGH);//A-die chip id HIGH

        sci_adi_write(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_AUD_EN, BIT_ANA_AUD_EN);//arm base address:0x40038800 for register accessable
        ana_sts0 = sci_adi_read(ANA_AUDCFGA_INT_BASE+ANA_STS0);//arm base address:0x40038600

	switch (adie_chip_id_high) {
		case 0x2713: {//chip is 2713
			switch (adie_chip_id_low) {
				case 0xA000: {
					adie_type = 1; //AC
					break;
				}
				case 0xA001: {
					if ((0x00000040 & ana_sts0) == 0x00000040) {
						adie_type = 2; //BA
						break;
					} else {
						adie_type = 3; //BB
						break;
					}
					break;
				}
				case 0xCA00: {
					adie_type = 5; //CA
					break;
				}
				default: {
					adie_type = 100; //unknow
					PRINT_ERR("unknow 2713 chip ID!!!\n");
					break;
				}
			}
			PRINT_INFO("adie chip is 2713, type = %d\n", adie_type);
			break;//break from 2713
		}

		case 0x2711: {//chip is 2711
			adie_type = 4; //Dolphin
			PRINT_INFO("adie chip is 2711, type = %d\n", adie_type);
			break; //break from 2711
		}

		default: {
			adie_type = 100; //unknow
			PRINT_ERR("unknow adie chip !!!\n");
			break;
		}
	}

        ht->platform_data = pdata;
        headset_detect_init();

        ret = sprd_headset_power_init(&pdev->dev);
        if (ret)
                goto fail_to_sprd_headset_power_init;

        if(adie_type < 3) {
                headmicbias_power_on(&pdev->dev, 1);
                msleep(5);//this time delay is necessary here
        }

        PRINT_INFO("====================================================================\n");
        PRINT_INFO(" write 0~3 to \"/sys/kernel/headset/debug_level\" for headset debug\n");
        PRINT_INFO("====================================================================\n");
        PRINT_INFO("D-die chip id = 0x%08X\n", __raw_readl(REG_AON_APB_CHIP_ID));
        PRINT_INFO("A-die chip id HIGH = 0x%08X\n", adie_chip_id_high);
        PRINT_INFO("A-die chip id LOW = 0x%08X\n", adie_chip_id_low);

        if (1 == adie_type) {
                pdata->gpio_detect -= 1;
                PRINT_WARN("use EIC_AUD_HEAD_INST (EIC4, GPIO_%d) for insert detecting\n", pdata->gpio_detect);
        } else {
                PRINT_INFO("use GPIO_%d for insert detecting\n", pdata->gpio_detect);
        }

        if(0 != pdata->gpio_switch) {
                ret = gpio_request(pdata->gpio_switch, "headset_switch");
                if (ret < 0) {
                        PRINT_ERR("failed to request GPIO_%d(headset_switch)\n", pdata->gpio_switch);
                        goto failed_to_request_gpio_switch;
                }
        }
        else
                PRINT_INFO("automatic type switch is unsupported\n");

        ret = gpio_request(pdata->gpio_detect, "headset_detect");
        if (ret < 0) {
                PRINT_ERR("failed to request GPIO_%d(headset_detect)\n", pdata->gpio_detect);
                goto failed_to_request_gpio_detect;
        }

        ret = gpio_request(pdata->gpio_button, "headset_button");
        if (ret < 0) {
                PRINT_ERR("failed to request GPIO_%d(headset_button)\n", pdata->gpio_button);
                goto failed_to_request_gpio_button;
        }

        if(0 != pdata->gpio_switch)
                gpio_direction_output(pdata->gpio_switch, 0);
        gpio_direction_input(pdata->gpio_detect);
        gpio_direction_input(pdata->gpio_button);
        ht->irq_detect = gpio_to_irq(pdata->gpio_detect);
        ht->irq_button = gpio_to_irq(pdata->gpio_button);

        ret = switch_dev_register(&ht->sdev);
        if (ret < 0) {
                PRINT_ERR("switch_dev_register failed!\n");
                goto failed_to_register_switch_dev;
        }

        input_dev = input_allocate_device();
        if ( !input_dev) {
                PRINT_ERR("input_allocate_device for headset_button failed!\n");
                ret = -ENOMEM;
                goto failed_to_allocate_input_device;
        }

        input_dev->name = "headset-keyboard";
        input_dev->id.bustype = BUS_HOST;
        ht->input_dev = input_dev;

        for (i = 0; i < pdata->nbuttons; i++) {
                struct headset_buttons *buttons = &pdata->headset_buttons[i];
                unsigned int type = buttons->type ?: EV_KEY;
                input_set_capability(input_dev, type, buttons->code);
        }

        ret = input_register_device(input_dev);
        if (ret) {
                PRINT_ERR("input_register_device for headset_button failed!\n");
                goto failed_to_register_input_device;
        }

        sema_init(&headset_sem, 1);

        INIT_WORK(&ht->work_button, headset_button_work_func);
        ht->button_work_queue = create_singlethread_workqueue("headset_button");
        if(ht->button_work_queue == NULL) {
                PRINT_ERR("create_singlethread_workqueue for headset_button failed!\n");
                goto failed_to_create_singlethread_workqueue_for_headset_button;
        }

        INIT_DELAYED_WORK(&ht->work_detect, headset_detect_work_func);
        ht->detect_work_queue = create_singlethread_workqueue("headset_detect");
        if(ht->detect_work_queue == NULL) {
                PRINT_ERR("create_singlethread_workqueue for headset_detect failed!\n");
                goto failed_to_create_singlethread_workqueue_for_headset_detect;
        }

        /***polling ana_sts0 to avoid the hardware defect***/
#ifdef SPRD_STS_POLLING_EN
        INIT_DELAYED_WORK(&sts_check_work, headset_sts_check_func);
        sts_check_work_queue = create_singlethread_workqueue("headset_sts_check");
        if(sts_check_work_queue == NULL) {
                PRINT_ERR("create_singlethread_workqueue for headset_sts_check failed!\n");
                goto failed_to_create_singlethread_workqueue_for_headset_sts_check;
        }
#endif
        /***polling ana_sts0 to avoid the hardware defect***/

        INIT_DELAYED_WORK(&reg_dump_work, reg_dump_func);
        reg_dump_work_queue = create_singlethread_workqueue("headset_reg_dump");
        if(reg_dump_work_queue == NULL) {
                PRINT_ERR("create_singlethread_workqueue for headset_reg_dump failed!\n");
                goto failed_to_create_singlethread_workqueue_for_headset_reg_dump;
        }
        if (debug_level >= 2)
                queue_delayed_work(reg_dump_work_queue, &reg_dump_work, msecs_to_jiffies(500));

#ifdef ADPGAR_BYP_SELECT
        //work queue for Bug#298417
        INIT_DELAYED_WORK(&adpgar_byp_select_work, adpgar_byp_select_func);
        adpgar_byp_select_work_queue = create_singlethread_workqueue("adpgar_byp_select");
        if(adpgar_byp_select_work_queue == NULL) {
                PRINT_ERR("create_singlethread_workqueue for adpgar_byp_select failed!\n");
                goto failed_to_create_singlethread_workqueue_for_adpgar_byp_select;
        }
        PRINT_INFO("ADPGAR_BYP_SELECT is enabled!\n");
#endif

        wake_lock_init(&headset_detect_wakelock, WAKE_LOCK_SUSPEND, "headset_detect_wakelock");
        wake_lock_init(&headset_button_wakelock, WAKE_LOCK_SUSPEND, "headset_button_wakelock");

        //set EIC3 de-bounce time for button irq
        headset_reg_set_val((ANA_EIC_BASE+EIC3_DBNC_CTRL), DBNC_CNT3_VALUE, DBNC_CNT3_MASK, DBNC_CNT3_SHIFT);
        //set EIC5 de-bounce time for inser irq
        headset_reg_set_val((ANA_EIC_BASE+EIC5_DBNC_CTRL), DBNC_CNT5_VALUE, DBNC_CNT5_MASK, DBNC_CNT5_SHIFT);

        irqflags = pdata->irq_trigger_level_button ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
        ret = request_irq(ht->irq_button, headset_button_irq_handler, irqflags | IRQF_NO_SUSPEND, "headset_button", ht);
        if (ret) {
                PRINT_ERR("failed to request IRQ_%d(GPIO_%d)\n", ht->irq_button, pdata->gpio_button);
                goto failed_to_request_irq_headset_button;
        }
        headset_irq_button_enable(0, ht->irq_button);//disable button irq before headset detected
		headset_button_hardware_bug_fix_stage = 0;

        irqflags = pdata->irq_trigger_level_detect ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
        ret = request_irq(ht->irq_detect, headset_detect_irq_handler, irqflags | IRQF_NO_SUSPEND, "headset_detect", ht);
        if (ret < 0) {
                PRINT_ERR("failed to request IRQ_%d(GPIO_%d)\n", ht->irq_detect, pdata->gpio_detect);
                goto failed_to_request_irq_headset_detect;
        }

        ret = headset_debug_sysfs_init();

        PRINT_INFO("headset_detect_probe success\n");
        return ret;

failed_to_request_irq_headset_detect:
        free_irq(ht->irq_button, ht);
failed_to_request_irq_headset_button:

#ifdef ADPGAR_BYP_SELECT
        cancel_delayed_work_sync(&adpgar_byp_select_work);
        destroy_workqueue(adpgar_byp_select_work_queue);
failed_to_create_singlethread_workqueue_for_adpgar_byp_select:
#endif

        cancel_delayed_work_sync(&reg_dump_work);
        destroy_workqueue(reg_dump_work_queue);
failed_to_create_singlethread_workqueue_for_headset_reg_dump:

#ifdef SPRD_STS_POLLING_EN
        destroy_workqueue(sts_check_work_queue);
failed_to_create_singlethread_workqueue_for_headset_sts_check:
#endif

        destroy_workqueue(ht->detect_work_queue);
failed_to_create_singlethread_workqueue_for_headset_detect:
        destroy_workqueue(ht->button_work_queue);
failed_to_create_singlethread_workqueue_for_headset_button:
        input_unregister_device(input_dev);
failed_to_register_input_device:
        input_free_device(input_dev);
failed_to_allocate_input_device:
        switch_dev_unregister(&ht->sdev);
failed_to_register_switch_dev:
        gpio_free(pdata->gpio_button);
failed_to_request_gpio_button:
        gpio_free(pdata->gpio_detect);
failed_to_request_gpio_detect:
        if(0 != pdata->gpio_switch)
                gpio_free(pdata->gpio_switch);
failed_to_request_gpio_switch:
        headmicbias_power_on(&pdev->dev, 0);
fail_to_sprd_headset_power_init:
fail_to_get_platform_data:
        PRINT_ERR("headset_detect_probe failed\n");
        return ret;
}

#ifdef CONFIG_PM
static int headset_suspend(struct platform_device *dev, pm_message_t state)
{
        PRINT_INFO("suspend (det_irq = %d, but_irq = %d, plug_state_last = %d)\n",
			headset.irq_detect, headset.irq_button, plug_state_last);
        return 0;
}

static int headset_resume(struct platform_device *dev)
{
        PRINT_INFO("resume (det_irq = %d, but_irq = %d, plug_state_last = %d)\n",
			headset.irq_detect, headset.irq_button, plug_state_last);
        return 0;
}
#else
#define headset_suspend NULL
#define headset_resume NULL
#endif

static const struct of_device_id headset_detect_of_match[] = {
	{.compatible = "sprd,headset-detect",},
	{ }
};
static struct platform_driver headset_detect_driver = {
        .driver = {
                .name = "headset-detect",
                .owner = THIS_MODULE,
                .of_match_table = headset_detect_of_match,
        },
        .probe = headset_detect_probe,
        .suspend = headset_suspend,
        .resume = headset_resume,
};

static int __init headset_init(void)
{
        int ret;
        ret = platform_driver_register(&headset_detect_driver);
        return ret;
}

static void __exit headset_exit(void)
{
	sprd_headset_power_deinit();
        platform_driver_unregister(&headset_detect_driver);
}

module_init(headset_init);
module_exit(headset_exit);

MODULE_DESCRIPTION("headset & button detect driver");
MODULE_AUTHOR("Yaochuan Li <yaochuan.li@spreadtrum.com>");
MODULE_LICENSE("GPL");
