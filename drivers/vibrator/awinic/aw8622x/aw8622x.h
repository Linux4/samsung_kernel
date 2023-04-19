/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _AW8622X_H_
#define _AW8622X_H_

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <linux/timekeeping.h>
#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
#include <linux/vibrator/sec_vibrator_inputff.h>
#endif

/*********************************************************
 *
 * Control Macro
 *
 ********************************************************/
#define INPUT_DEV
#define AW_CHECK_RAM_DATA
#define AW_READ_BIN_FLEXBALLY
#define AW_OSC_COARSE_CALI
/* #define AW_RTP_CALI_TO_FILE */

/*********************************************************
 *
 * Normal Marco
 *
 ********************************************************/
#define AW8622X_I2C_NAME			("aw8622x_haptic")
#define AW8622X_I2C_READ_MSG_NUM		(2)
#define AW8622X_CHIP_ID				(0x00)
#define AW8622X_VBAT_MIN			(3000)
#define AW8622X_VBAT_MAX			(5500)
#define AW8622X_VBAT_REFER			(4200)
#define AW8622X_I2C_RETRIES			(5)
#define AW8622X_I2C_RETRY_DELAY			(2)
#define AW8622X_RTP_NAME_MAX			(64)
#define AW8622X_SEQUENCER_SIZE			(8)
#define AW8622X_SEQUENCER_LOOP_SIZE		(4)
#define AW8622X_OSC_CALI_MAX_LENGTH		(11000000)
#define AW8622x_READ_CHIPID_RETRIES		(5)
#define AW8622X_PM_QOS_VALUE_VB			(400)
#define AW8622X_CUSTOM_DATA_LEN			(1)
#define AW8622X_RAMDATA_RD_BUFFER_SIZE		(512)
#define AW8622X_RAMDATA_WR_BUFFER_SIZE		(512)
#define AW8622X_DRV_WIDTH_MAX			(255)
#define AW8622X_DRV_WIDTH_MIN			(40)
#define AW8622X_DRV2_LVL_MAX			(127)
#define SMEM_ID_VENDOR1				(135) /* The value must be equal to uefi driver code */

#define AW8622X_RL_DELAY_MIN			(3000)
#define AW8622X_RL_DELAY_MAX			(3500)
#define AW8622X_F0_DELAY_MIN			(10000)
#define AW8622X_F0_DELAY_MAX			(10500)
#define AW8622X_RTP_DELAY_MIN			(2000)
#define AW8622X_RTP_DELAY_MAX			(2500)
#define AW8622X_PLAY_DELAY_MIN			(2000)
#define AW8622X_PLAY_DELAY_MAX			(2500)
#define AW8622X_STOP_DELAY_MIN			(2000)
#define AW8622X_STOP_DELAY_MAX			(2500)
#define AW8622X_VBAT_DELAY_MIN			(2000)
#define AW8622X_VBAT_DELAY_MAX			(2500)
#define CPU_LATENCY_QOC_VALUE			(0)

#define AW8622X_DRV_WIDTH_FORMULA(f0, track_margin, brk_gain) \
						((240000 / (f0)) - (track_margin) - (brk_gain) - 8)
#define AW8622X_DRV2_LVL_FORMULA(f0, vrms)	((((f0) < 1800) ? 1809920 : 1990912) / 1000 * (vrms) / 61000)

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
#define KERNEL_OVER_5_10
#endif

#if KERNEL_VERSION(5, 14, 0) <= LINUX_VERSION_CODE
#define KERNEL_OVER_5_14
#endif

/*********************************************************
 *
 * enum
 *
 ********************************************************/
enum aw8622x_flags {
	AW8622X_FLAG_NONR = 0,
	AW8622X_FLAG_SKIP_INTERRUPTS = 1,
};

enum aw8622x_haptic_work_mode {
	AW8622X_STANDBY_MODE = 0,
	AW8622X_RAM_MODE = 1,
	AW8622X_RAM_LOOP_MODE = 2,
	AW8622X_CONT_MODE = 3,
	AW8622X_RTP_MODE = 4,
	AW8622X_TRIG_MODE = 5,
};

enum aw8622x_haptic_cont_vbat_comp_mode {
	AW8622X_HAPTIC_CONT_VBAT_SW_ADJUST_MODE = 0,
	AW8622X_HAPTIC_CONT_VBAT_HW_ADJUST_MODE = 1,
};

enum aw8622x_haptic_ram_vbat_compensate_mode {
	AW8622X_HAPTIC_RAM_VBAT_COMP_DISABLE = 0,
	AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE = 1,
};

enum aw8622x_sram_size_flag {
	AW8622X_HAPTIC_SRAM_1K = 0,
	AW8622X_HAPTIC_SRAM_2K = 1,
	AW8622X_HAPTIC_SRAM_3K = 2,
};

enum aw8622x_haptic_pwm_mode {
	AW8622X_PWM_48K = 0,
	AW8622X_PWM_24K = 1,
	AW8622X_PWM_12K = 2,
};

enum aw8622x_haptic_cali_lra {
	AW8622X_WRITE_ZERO = 0,
	AW8622X_F0_CALI = 1,
	AW8622X_OSC_CALI = 2,
};

enum aw8622x_haptic_rtp_mode {
	AW8622X_RTP_SHORT = 4,
	AW8622X_RTP_LONG = 5,
	AW8622X_RTP_SEGMENT = 6,
};

enum aw8622x_ef_id {
	AW86223_EF_ID = 0x01,
	AW86224_5_EF_ID = 0x00,
};
/*********************************************************
 *
 * Log Format
 *
 *********************************************************/
#define aw_err(format, ...) \
	pr_err("[%s][%04d]%s: " format "\n", AW8622X_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__)

#define aw_info(format, ...) \
	pr_info("[%s][%04d]%s: " format "\n", AW8622X_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__)

#define aw_dbg(format, ...) \
	pr_debug("[%s][%04d]%s: " format "\n", AW8622X_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__)

/*********************************************************
 *
 * Struct
 *
 ********************************************************/
struct aw8622x_dts_info {
	bool is_enabled_auto_brk;
	bool is_enabled_cont_f0;
	unsigned int mode;
	unsigned int f0_ref;
	unsigned int f0_cali_percent;
	unsigned int cont_drv1_lvl_dt;
	unsigned int lra_vrms;
	unsigned int cont_drv1_time_dt;
	unsigned int cont_drv2_time_dt;
	unsigned int cont_brk_time_dt;
	unsigned int cont_track_margin;
	unsigned int cont_drv_width;
	unsigned int cont_brk_gain;
	unsigned int d2s_gain;
	unsigned int max_level_gain_val;
};

struct ram {
	unsigned int len;
	unsigned int ram_num;
	unsigned int check_sum;
	unsigned int base_addr;
	unsigned char version;
	unsigned char ram_shift;
	unsigned char baseaddr_shift;
};

struct aw8622x {
#ifdef INPUT_DEV
	int effect_type;
	int effect_id;
	struct input_dev *input_dev;
#endif

	bool rtp_repeat;
	bool haptic_ready;
	bool input_flag;
	bool isUsedIntn;

	unsigned char level;
	unsigned char seq[AW8622X_SEQUENCER_SIZE];
	unsigned char loop[AW8622X_SEQUENCER_SIZE];
	unsigned char rtp_init;
	unsigned char ram_init;
	unsigned char rtp_routine_on;
	unsigned char max_pos_beme;
	unsigned char max_neg_beme;
	unsigned char f0_cali_flag;
	unsigned char ram_vbat_compensate;
	unsigned char hwen_flag;
	unsigned char flags;
	unsigned char chipid;
	unsigned char play_mode;
	unsigned char activate_mode;
	unsigned char ram_state;
	unsigned char wk_lock_flag;

	u16 new_gain;

	atomic_t is_in_rtp_loop;
	atomic_t exit_in_rtp_loop;

	int reset_gpio;
	int irq_gpio;
	int state;
	int duration;
	int amplitude;
	int index;
	int vmax;
	int gain;
	int sysclk;
	int rate;
	int width;
	int pstream;
	int cstream;

	unsigned int rtp_cnt;
	unsigned int rtp_file_num;
	unsigned int f0;
	unsigned int cont_drv1_lvl;
	unsigned int cont_drv2_lvl;
	unsigned int cont_brk_time;
	unsigned int cont_drv1_time;
	unsigned int cont_drv2_time;
	unsigned int theory_time;
	unsigned int vbat;
	unsigned int lra;
	unsigned int ram_update_flag;
	unsigned int rtp_update_flag;
	unsigned int osc_cali_data;
	unsigned int f0_cali_data;
	unsigned int timeval_flags;
	unsigned int osc_cali_flag;
	unsigned int sys_frequency;
	unsigned int rtp_len;
	unsigned int osc_cali_run;
	unsigned int rtp_cali_select;

	unsigned long microsecond;
	wait_queue_head_t wait_q;
	wait_queue_head_t stop_wait_q;

	struct led_classdev vib_dev;

	struct workqueue_struct *work_queue;
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct device *dev;
	struct input_dev *input;
	struct mutex lock;
	struct mutex rtp_lock;
	struct hrtimer timer;
	struct work_struct vibrator_work;
	struct work_struct rtp_work;
	struct work_struct stop_work;
	struct work_struct set_gain_work;
	struct delayed_work ram_work;
	struct aw8622x_dts_info dts_info;
	struct ram ram;
	struct timespec64 start, end;
	struct aw8622x_container *rtp_container;

#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
	bool use_sep_index;
	struct sec_vib_inputff_drvdata sec_vib_ddata;
#endif
};

struct aw8622x_container {
	int len;
	unsigned char data[];
};

#endif
