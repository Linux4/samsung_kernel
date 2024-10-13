#ifndef __ZM918_MOTOR_H__
#define __ZM918_MOTOR_H__

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
#include <linux/regulator/consumer.h>
#include <linux/time.h>
#include <linux/vibrator/sec_vibrator_inputff.h>

#define ZINTIIX_SIGNATURE_STR "ZINITIX ZM918"
#define ZINITIX_HEADER_LEN 21
#define ZM918_I2C_NAME "zm918_haptic"

#define ZM918_BIN_NAME_MAX			(64)

#define	MOTOR_REG_MODE_00		0x00
#define	MOTOR_REG_MODE_01		0x01
#define MOTOR_REG_MODE_0F		0x0F
#define	MOTOR_REG_SOFT_EN		0x10
#define	MOTOR_REG_STRENGTH		0x11
#define	MOTOR_REG_ADC_SAMPLING_TIME		0x12
#define	MOTOR_REG_MODE_13		0x13
#define	MOTOR_REG_OVER_DRV		0x14
#define MOTOR_REG_ZERO_CROSS	0x16
#define	MOTOR_REG_REG_BR_ADC_VAL	0x17
#define MOTOR_REG_BR_LIMIT		0x18
#define	MOTOR_REG_START_STRENGTH	0x19
#define	MOTOR_REG_SEARCH_DRV_RATIO1	0x1A
#define	MOTOR_REG_SEARCH_DRV_RATIO2	0x1B
#define	MOTOR_REG_SEARCH_DRV_RATIO3	0x1C
#define MOTOR_REG_NOISE_CANCEL_T	0x1D
#define MOTOR_REG_WAIT_MAX			0X1E
#define	MOTOR_REG_DRV_FREQ_H		0x1F
#define	MOTOR_REG_DRV_FREQ_L		0x20
#define	MOTOR_REG_RESO_FREQ_H		0x21
#define	MOTOR_REG_RESO_FREQ_L		0x22
#define MOTOR_REG_MODE_25		0X25
#define	MOTOR_REG_FIFO_INTERVAL		0x27
#define MOTOR_REG_FIFO_OP_STRENGTH		0X2A
#define MOTOR_REG_FIFO_PWM_FREQ		0x2B
#define MOTOR_REG_OVER_DRV_STRENGTH		0x2C
#define MOTOR_REG_BR_STRENGTH_P0		0x30
#define MOTOR_REG_BR_STRENGTH_P1		0x31
#define MOTOR_REG_BR_STRENGTH_P2		0x32
#define MOTOR_REG_BR_STRENGTH_P3		0x33
#define MOTOR_REG_BR_STRENGTH_P4		0x34
#define MOTOR_REG_BR_STRENGTH_P5		0x35
#define MOTOR_REG_BR_STRENGTH_P6		0x36
#define MOTOR_REG_BR_STRENGTH_P7		0x37
#define MOTOR_REG_BR_STRENGTH_P8		0x38
#define MOTOR_REG_BR_STRENGTH_P9		0x39
#define MOTOR_REG_BR_STRENGTH_N0		0x3A
#define MOTOR_REG_BR_STRENGTH_N1		0x3B
#define MOTOR_REG_BR_STRENGTH_N2		0x3C
#define MOTOR_REG_BR_STRENGTH_N3		0x3D
#define MOTOR_REG_BR_STRENGTH_N4		0x3E
#define MOTOR_REG_BR_STRENGTH_N5		0x3F
#define MOTOR_REG_BR_STRENGTH_N6		0x40
#define MOTOR_REG_BR_STRENGTH_N7		0x41
#define MOTOR_REG_BR_STRENGTH_N8		0x42
#define MOTOR_REG_BR_STRENGTH_N9		0x43
#define MOTOR_REG_ADC_OUT		0x47
#define MOTOR_REG_CAL_EN		0x70
#define MOTOR_REG_PLAY_MODE		0x84
#define MOTOR_REG_PLAYBACK_CONTROL		0x85
#define MOTOR_REG_FIFO_WRITE		0xF2

#define PLAYBACK_STOP			0x00
#define PLAYBACK_START			0x01

#define	MOTOR_REG_STRENGTH_MASK		0x7F
#define MOTOR_REG_PLAYBACK_CONTROL_MASK		0x01
#define	MOTOR_REG_SOFT_EN_MASK		0x01
#define SOFT_DISABLE			0x00
#define SOFT_ENABLE			0x01

#define MOTOR_VCC (3000000)

#define DEFAULT_MOTOR_CONT_LEVEL 0x42
#define DEFAULT_MOTOR_FIFO_LEVEL 0x80
#define MIN_MOTOR_CONT_LEVEL 0x09
#define MIN_MOTOR_FIFO_LEVEL 0x00
#define DEFAULT_BRAKE_DELAY 0
#define DEFAULT_ADC_SAMPLING_TIME 0x86
#define DEFAULT_SOFT_EN_DELAY 0

#define ZM918_PLAY_MODE_RAM	0x00
#define ZM918_PLAY_MODE_RTP	0x04
#define ZM918_PLAY_MODE_CONT	0x08

#define ZM918_PLAY_MODE_MASK	(3<<2)


#define ZM918_VDD3P0_ENABLE_DELAY_MIN (12000)
#define ZM918_VDD3P0_ENABLE_DELAY_MAX (13000)
#define ZM918_BEFORE_INIT_DELAY_MIN (200)
#define ZM918_BEFORE_INIT_DELAY_MAX (200)
#define ZM918_VM_ENABLE_DELAY_MIN (11000)
#define ZM918_VM_ENABLE_DELAY_MAX (11000)

#define HIGH_8_BITS(n) ((n >> 8) & 0xFF)
#define LOW_8_BITS(n) (n & 0xFF)
/*********************************************************
 *
 * Log Format
 *
 *********************************************************/
#define zm_err(format, ...) \
	pr_err("[%s][%04d]%s: " format "\n", ZM918_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__)

#define zm_info(format, ...) \
	pr_info("[%s][%04d]%s: " format "\n", ZM918_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__)

#define zm_dbg(format, ...) \
	pr_debug("[%s][%04d]%s: " format "\n", ZM918_I2C_NAME, __LINE__, __func__, ##__VA_ARGS__)

enum VIBRATOR_CONTROL {
	VIBRATOR_DISABLE = 0,
	VIBRATOR_ENABLE = 1,
};

enum actuator_type {
	ACTUATOR_1030 = 0,
	ACTUATOR_080935 = 1
};

struct reg_data {
	u32 addr;
	u32 data;
};

enum zm918_haptic_work_mode {
	ZM918_STANDBY_MODE = 0,
	ZM918_CONT_MODE = 1,
	ZM918_RTP_MODE = 2,
	ZM918_CONT_DURATION_MODE = 3,
	ZM918_BIN_PARSE_MODE = 4,		// commands in bin format
};

struct zm918_dts_info {
	enum actuator_type motor_type;
	int cont_level;
	int fifo_level;
	int drv_freq;
	int reso_freq;
	int gpio_en;
	unsigned int mode;
	const char *regulator_name;
	const char *vib_type;
	const char *file_path;
};

struct zm918_container {
	int len;
	unsigned char data[];
};

struct zm918 {
	int effect_type;
	int effect_id;
	struct input_dev *input_dev;

	bool fifo_repeat;
	bool haptic_ready;
	bool input_flag;
	bool adb_control;

	unsigned short int f0;
	unsigned char cont_level;
	unsigned char fifo_level;
	unsigned char rtp_init;
	unsigned char ram_init;
	unsigned char play_mode;
	unsigned char activate_mode;
	unsigned char wk_lock_flag;
	
	u16 new_gain;
	
	int state;
	int duration;
	int amplitude;
	int index;
	int gain;
	
	atomic_t is_in_rtp_loop;
	atomic_t exit_in_rtp_loop;

	unsigned long microsecond;
	wait_queue_head_t wait_q;
	wait_queue_head_t stop_wait_q;

	struct workqueue_struct *work_queue;
	struct device *dev;
	struct regmap *regmap;
	struct input_dev *input;
	struct i2c_client *i2c;
	struct zm918_dts_info dts_info;
	struct regulator *regulator;
	struct mutex lock;
	struct hrtimer timer;
	struct work_struct vib_work;
	struct work_struct stop_work;
	struct work_struct set_gain_work;
	struct timespec64 start, end;
	int gpio_en;
	
	struct zm918_container *p_container;
	bool stop_now; /* stop_now 1 - interrupt pattern play, stop_now 0 - continue pattern */

	bool use_sep_index;
	struct sec_vib_inputff_drvdata sec_vib_ddata;
};
#endif
