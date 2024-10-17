/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _AW8624X_H_
#define _AW8624X_H_

#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
#include <linux/vibrator/sec_vibrator_inputff.h>
#include <linux/usblog_proc_notify.h>
#endif

/*********************************************************
 *
 * Control Macro
 *
 *********************************************************/
#define INPUT_DEV
#define AW_CHECK_RAM_DATA
#define AW_READ_BIN_FLEXBALLY
/* #define AW_KERNEL_F0_CALIBRATION */
/* #define AW_UEFI_F0_CALIBRATION */
#define AW_LK_F0_CALIBRATION
#ifdef AW_UEFI_F0_CALIBRATION
#define	SMEM_AWINIC_PARAMS				(499)
#endif
#define AW_READ_CONT_F0
/* #define AW_RTP_CALI_TO_FILE */
#define AW_ADD_BRAKE_WAVE
#define AW_OVERRIDE_EN
#define AW_RAM_MAX_INDEX				(10)
#define AW_RAM_LONG_INDEX				(5)
/*********************************************************
 *
 * AW8624X CHIPID
 *
 *********************************************************/
#define AW8624X_CHIP_ID					(0x24)
#define AW86243_CHIP_ID					(0x30)
#define AW86245_CHIP_ID					(0x50)
/*********************************************************
 *
 * Normal Marco
 *
 *********************************************************/
#define AW8624X_I2C_NAME				"aw8624x_haptic"
#define AW8624X_FITTING_COEFF				(0)
#define AW8624X_I2C_RETRIES				(5)
#define AW8624X_I2C_READ_MSG_NUM			(2)
#define AW8624X_REG_MAX					(0xFF)
#define AW8624X_VBAT_MIN				(3000)
#define AW8624X_VBAT_MAX				(5250)
#define AW8624X_VBAT_REFER				(4200)
#define AW8624X_DEFAULT_GAIN				(0x80)
#define AW8624X_SEQUENCER_SIZE				(8)
#define AW8624X_SEQUENCER_LOOP_SIZE			(4)
#define AW8624X_DRV_WIDTH_MIN				(0)
#define AW8624X_DRV_WIDTH_MAX				(255)
#define AW8624X_DRV2_LVL_MAX				(127)
#define AW8624X_READ_CHIPID_RETRIES			(5)
#define AW8624X_RAMDATA_RD_BUFFER_SIZE			(512)//1024
#define AW8624X_RAMDATA_WR_BUFFER_SIZE			(512)//2048
#define AW8624X_RAMDATA_SHOW_COLUMN			(16)
#define AW8624X_RAMDATA_SHOW_UINT_SIZE			(6)
#define AW8624X_RAMDATA_SHOW_LINE_BUFFER_SZIE		(100)
#define AW8624X_OSC_CALI_MAX_LENGTH			(11000000)
#define AW8624X_PM_QOS_VALUE_VB				(400)
#define CPU_LATENCY_QOC_VALUE				(0)
#define AW_LONG_VIB_POINTS				(500)
#define AW8624X_REG_CONTCFG1_DEFAULT			(0xA0)

#define AW8624X_SET_RAMADDR_H(addr)			((addr) >> 8)
#define AW8624X_SET_RAMADDR_L(addr)			((addr) & 0x00FF)
#define AW8624X_SET_BASEADDR_H(addr)			((addr) >> 8)
#define AW8624X_SET_BASEADDR_L(addr)			((addr) & 0x00FF)
#define AW8624X_SET_AEADDR_H(addr)			((((addr) >> 1) >> 4) & 0xF0)
#define AW8624X_SET_AEADDR_L(addr)			(((addr) >> 1) & 0x00FF)
#define AW8624X_SET_AFADDR_H(addr)			((((addr) - ((addr) >> 2)) >> 8) & 0x0F)
#define AW8624X_SET_AFADDR_L(addr)			(((addr) - ((addr) >> 2)) & 0x00FF)
#define AW8624X_VBAT_FORMULA(code)			(6100 * (code) / 1023)
#define AW8624X_F0_FORMULA(code)			(384000 * 10 / (code))
#define AW8624X_RL_FORMULA(code, d2s_gain)		\
			(((code) * 610 * 1000) / (1023 * (d2s_gain)))
#define AW8624X_OS_FORMULA(os_code, d2s_gain)		\
			(2440 * ((os_code) - 512) / (1023 * ((d2s_gain) + 1)))
#define AW8624X_DRV2_LVL_FORMULA(f0, vrms)		\
			((((f0) < 1800) ? 1809920 : 1990912) / 1000 * (vrms) / 61000)
#define AW8624X_DRV_WIDTH_FORMULA(f0, margin, brk_gain) \
			((240000 / (f0)) - (margin) - (brk_gain) - 8)

#if defined(CONFIG_AW8624X_SAMSUNG_A54_PROJECT)
#define AW8624X_MAX_LEVEL				(0x7D)
#elif defined(CONFIG_AW8624X_SAMSUNG_M54_PROJECT)
#define AW8624X_MAX_LEVEL				(0x73)
#else
#define AW8624X_MAX_LEVEL				(0x80)
#endif

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
#define KERNEL_OVER_5_10
#endif

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
#define KERNEL_OVER_6_1
#endif
/*********************************************************
 *
 * Log Format
 *
 *********************************************************/

#ifdef CONFIG_USB_USING_ADVANCED_USBLOG
#define aw_err(format, ...)				\
	({										\
		pr_err("[%s][%04d]%s: " format "\n", AW8624X_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__);		\
		printk_usb(NOTIFY_PRINTK_USB_NORMAL, format "\n", ##__VA_ARGS__);	\
	})
#define aw_info(format, ...)				\
	({										\
		pr_info("[%s][%04d]%s: " format "\n", AW8624X_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__);			\
		printk_usb(NOTIFY_PRINTK_USB_NORMAL, format "\n", ##__VA_ARGS__);	\
	})
#define aw_dbg(format, ...)				\
	({										\
		pr_debug("[%s][%04d]%s: " format "\n", AW8624X_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__);			\
		printk_usb(NOTIFY_PRINTK_USB_NORMAL, format "\n", ##__VA_ARGS__);	\
	})
#else
#define aw_err(format, ...) \
	pr_err("[%s][%04d]%s: " format "\n", AW8624X_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__)

#define aw_info(format, ...) \
	pr_info("[%s][%04d]%s: " format "\n", AW8624X_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__)

#define aw_dbg(format, ...) \
	pr_debug("[%s][%04d]%s: " format "\n", AW8624X_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__)
#endif

/*********************************************************
 *
 * Enum Define
 *
 *********************************************************/

enum aw8624x_haptic_work_mode {
	AW8624X_STANDBY_MODE = 0,
	AW8624X_RAM_MODE = 1,
	AW8624X_RAM_LOOP_MODE = 2,
	AW8624X_CONT_MODE = 3,
	AW8624X_RTP_MODE = 4,
	AW8624X_TRIG_MODE = 5,
	AW8624X_NULL = 6,
};

enum aw8624x_haptic_cont_vbat_comp_mode {
	AW8624X_CONT_VBAT_SW_COMP_MODE = 0,
	AW8624X_CONT_VBAT_HW_COMP_MODE = 1,
};

enum aw8624x_haptic_ram_vbat_comp_mode {
	AW8624X_RAM_VBAT_COMP_DISABLE = 0,
	AW8624X_RAM_VBAT_COMP_ENABLE = 1,
};

enum aw8624x_haptic_pwm_mode {
	AW8624X_PWM_48K = 0,
	AW8624X_PWM_24K = 1,
	AW8624X_PWM_12K = 2,
};

enum aw8624x_haptic_cali_lra {
	AW8624X_WRITE_ZERO = 0,
	AW8624X_F0_CALI_LRA = 1,
	AW8624X_OSC_CALI_LRA = 2,
};

enum aw8624x_haptic_pin {
	AW8624X_TRIG1 = 0,
	AW8624X_TRIG2 = 1,
	AW8624X_TRIG3 = 2,
	AW8624X_IRQ = 3,
};
/*********************************************************
 *
 * Struct Define
 *
 *********************************************************/
struct aw8624x_haptic_ram {
	uint8_t ram_num;
	uint8_t version;
	uint8_t ram_shift;
	uint8_t baseaddr_shift;

	uint32_t len;
	uint32_t check_sum;
	uint32_t base_addr;
};

struct aw8624x_dts_info {
	bool is_enabled_auto_brk;
	bool is_enabled_smart_loop;
	bool is_enabled_inter_brake;

	uint32_t f0_cali_percent;
	uint32_t f0_pre;
	uint32_t lra_vrms;
	uint32_t lk_f0_cali;
	uint32_t cont_drv1_lvl;
	uint32_t cont_drv2_lvl;
	uint32_t cont_drv1_time;
	uint32_t cont_drv2_time;
	uint32_t cont_brk_time;
	uint32_t cont_track_margin;
	uint32_t cont_brk_gain;
	uint32_t d2s_gain;
	uint32_t f0_d2s_gain;
#if defined(CONFIG_AW8624X_SAMSUNG_FEATURE)
	uint32_t max_level_gain_val;
	int f0_offset;
	bool is_f0_tracking;
#endif //CONFIG_AW8624X_SAMSUNG_FEATURE
};

struct aw8624x {
#ifdef INPUT_DEV
	int effect_type;
	int effect_id;
	struct input_dev *input_dev;
#endif
	bool rtp_repeat;
	bool rtp_init;
	bool ram_init;
	bool input_flag;

	uint8_t gain;
	uint8_t play_mode;
	uint8_t activate_mode;
	uint8_t ram_vbat_comp;
	uint8_t seq[AW8624X_SEQUENCER_SIZE];
	uint8_t loop[AW8624X_SEQUENCER_SIZE];
	uint8_t wk_lock_flag;
	uint8_t fitting_coeff;

	uint16_t golden_param[8];
	uint16_t sample_param[9];

	int level;
	int state;
	int index;
	int irq_gpio;
	int duration;
	int amplitude;
	int drv_width;
	int rtp_drv_width;

	uint32_t f0;
	uint32_t lra;
	uint32_t vbat;
	uint32_t f0_pre;
	uint32_t lra_f0;
	uint32_t cont_f0;
	uint32_t rtp_cnt;
	uint32_t rtp_len;
	uint32_t theory_time;
	uint32_t rtp_file_num;
	uint32_t rtp_num_max;
	uint32_t rtp_cali_select;
	uint32_t timeval_flags;
	uint32_t f0_cali_data;
	uint32_t osc_cali_data;
	uint32_t microsecond;

	ktime_t kend;
	ktime_t kstart;
	atomic_t is_in_rtp_loop;
	wait_queue_head_t stop_wait_q;

	struct led_classdev vib_dev;
	struct mutex lock;
	struct device *dev;
	struct hrtimer timer;
	struct mutex rtp_lock;
	struct i2c_client *i2c;
	struct aw8624x_haptic_ram ram;
	struct aw8624x_dts_info dts_info;
	struct work_struct vibrator_work;
	struct work_struct set_gain_work;
	struct work_struct rtp_work;
	struct work_struct stop_work;
	struct delayed_work ram_work;
	struct aw8624x_container *aw_rtp;
	struct workqueue_struct *work_queue;
	struct pm_qos_request pm_qos_req_vb;

#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
	bool use_sep_index;
	struct sec_vib_inputff_drvdata sec_vib_ddata;
#endif
};

struct aw8624x_container {
	int len;
	uint8_t data[];
};
#endif
