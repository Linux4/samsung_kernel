/*
 * s2mu106_haptic.h
 * Samsung S2MU106 Haptic Header
 *
 * Copyright (C) 2018 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __S2MU106_HAPTIC_H
#define __S2MU106_HAPTIC_H __FILE__

#define S2MU106_REG_HAPTIC_INT		0x00
#define S2MU106_REG_HBST_INT		0x01
#define S2MU106_REG_HAPTIC_INT_MASK	0x02
#define S2MU106_REG_HBST_INT_MASK	0x03
#define S2MU106_REG_HBST_STATUS1	0x04
#define S2MU106_REG_PERI_TAR1		0x05
#define S2MU106_REG_PERI_TAR2		0x06
#define S2MU106_REG_DUTY_TAR1		0x07
#define S2MU106_REG_DUTY_TAR2		0x08
#define S2MU106_REG_HAPTIC_MODE		0x09
#define S2MU106_REG_OV_BK_OPTION	0x0A
#define S2MU106_REG_OV_WAVE_NUM		0x0B
#define S2MU106_REG_OV_AMP		0x0C
#define S2MU106_REG_PWM_CNT_NUM		0x10
#define S2MU106_REG_FILTERCOEF1		0x13
#define S2MU106_REG_FILTERCOEF2		0x14
#define S2MU106_REG_FILTERCOEF3		0x15
#define S2MU106_REG_IMPCONF1		0x16
#define S2MU106_REG_IMPCONF2		0x17
#define S2MU106_REG_IMPCONF3		0x18
#define S2MU106_REG_AMPCOEF1		0x19
#define S2MU106_REG_AMPCOEF2		0x1A
#define S2MU106_REG_AMPCOEF3		0x1B
#define S2MU106_REG_HT_OTP0		0x20
#define S2MU106_REG_HT_OTP2		0x22
#define S2MU106_REG_HT_OTP3		0x23
#define S2MU106_REG_HBST_CTRL0		0x2B
#define S2MU106_REG_HBST_CTRL1		0x2C

/* S2MU106_REG_HBST_CTRL1 */
#define HAPTIC_BOOST_VOLTAGE_MASK	0x3F

/* S2MU106_REG_HT_OTP0 */
#define HBST_OK_FORCE_EN		0x01
#define HBST_OK_MASK_EN			0x02

/* S2MU106_REG_HT_OTP2 */
#define VCEN_SEL_MASK			0xC0

/* S2MU106_REG_HT_OTP3 */
#define VCENUP_TRIM_MASK		0x03

/* S2MU106_REG_HBST_CTRL0 */
#define EN_HBST_EXT_MASK		0x01
#define SEL_HBST_HAPTIC_MASK		0x02

/* S2MU106_REG_OV_BK_OPTION */
#define LRA_MODE_SET_MASK		0x80
#define LRA_BST_MODE_SET_MASK		0x10

/* S2MU106_REG_HAPTIC_MODE */
#define LRA_MODE_EN			0x20
#define ERM_HDPWM_MODE_EN		0x41
#define ERM_MODE_ON			0x01
#define HAPTIC_MODE_OFF			0x00

#define TEST_MODE_TIME			10000
#define MAX_INTENSITY			10000
#define MAX_FREQUENCY			5
#define VIB_BUFSIZE                     30
#define PACKET_MAX_SIZE         	1000
#define MAX_STR_LEN_EVENT_CMD		32

#define HAPTIC_ENGINE_FREQ_MIN          1200
#define HAPTIC_ENGINE_FREQ_MAX          3500

#define MULTI_FREQ_DIVIDER              78125000 /* 1s/128(pdiv)=7812500 */

struct vib_packet {
	int time;
	int intensity;
	int freq;
	int overdrive;
};

enum {
	VIB_PACKET_TIME = 0,
	VIB_PACKET_INTENSITY,
	VIB_PACKET_FREQUENCY,
	VIB_PACKET_OVERDRIVE,
	VIB_PACKET_MAX,
};

enum VIB_EVENT {
	VIB_EVENT_NONE = 0,
	VIB_EVENT_FOLDER_CLOSE,
	VIB_EVENT_FOLDER_OPEN,
	VIB_EVENT_FOLDER_OPEN_SHORT_DURATION,
	VIB_EVENT_FOLDER_OPEN_LONG_DURATION,
	VIB_EVENT_ACCESSIBILITY_BOOST_ON,
	VIB_EVENT_ACCESSIBILITY_BOOST_OFF,
	VIB_EVENT_MAX,
};

enum EVENT_CMD {
	EVENT_CMD_NONE = 0,
	EVENT_CMD_FOLDER_CLOSE,
	EVENT_CMD_FOLDER_OPEN,
	EVENT_CMD_ACCESSIBILITY_BOOST_ON,
	EVENT_CMD_ACCESSIBILITY_BOOST_OFF,
	EVENT_CMD_MAX,
};

typedef union
{
	uint32_t	DATA;
	struct {
	uint32_t	FOLDER_STATE:1,
			SHORT_DURATION:1,
			ACCESSIBILITY_BOOST:1,
			RESERVED:29;
	} EVENTS;
} EVENT_STATUS;

enum s2mu106_haptic_operation_type {
	S2MU106_HAPTIC_ERM_I2C,
	S2MU106_HAPTIC_ERM_GPIO,
	S2MU106_HAPTIC_LRA,
};

enum s2mu106_haptic_pulse_mode {
	S2MU106_EXTERNAL_MODE,
	S2MU106_INTERNAL_MODE,
};

struct s2mu106_haptic_boost {
	/* haptic boost */
	bool en;
	bool automode;
	int level;
};

struct s2mu106_haptic_platform_data {
	unsigned int timeout;
	u16 max_timeout;
	u32 intensity;
	u32 duty;
	u32 period;
	int ratio;
	int normal_ratio;
	int overdrive_ratio;
	int folder_ratio;
	int high_temp_ratio;
	int temperature;
	u16 reg2;
	int motor_en;
	int multi_frequency;
	int freq;
	u32 *multi_freq_period;
	const char *vib_type;
	bool f_packet_en;
	bool packet_running;
	int packet_size;
	int packet_cnt;
	struct vib_packet haptic_engine[PACKET_MAX_SIZE];

	EVENT_STATUS save_vib_event;
	int event_idx;

	/* haptic drive mode */
	enum s2mu106_haptic_operation_type hap_mode;

	/* haptic boost */
	struct s2mu106_haptic_boost hbst;

	void (*init_hw)(void);
};

#endif /* __S2MU106_HAPTIC_H */
