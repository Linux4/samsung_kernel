/*
 *  SPI Driver Interface Functions
 *
 *  This file contains the SPI driver interface functions.
 *
 *  Copyright (C) 2011-2013 Validity Sensors, Inc.
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef VFS8XXX_H_
#define VFS8XXX_H_

#define SLOW_BAUD_RATE		9600000
#define MAX_BAUD_RATE		9600000

#define BAUD_RATE_COEF		1000
#define DRDY_ACTIVE_STATUS	1
#define BITS_PER_WORD		8
#define DRDY_IRQ_FLAG		IRQF_TRIGGER_RISING
#define DEFAULT_BUFFER_SIZE	(4096 * 16)
#define DRDY_IRQ_ENABLE		1
#define DRDY_IRQ_DISABLE	0

/* For Egis sensor */
#define FP_REGISTER_READ				0x01
#define FP_REGISTER_WRITE				0x02
#define FP_GET_ONE_IMG					0x03
#define FP_SENSOR_RESET					0x04
#define FP_POWER_CONTROL				0x05
#define FP_SET_SPI_CLOCK				0x06
#define FP_RESET_SET					0x07
#define FP_REGISTER_BREAD				0x20
#define FP_REGISTER_BWRITE				0x21
#define FP_REGISTER_MREAD				0x22
#define FP_REGISTER_MWRITE				0x23
#define FP_REGISTER_BREAD_BACKWARD		0x24
#define FP_REGISTER_BWRITE_BACKWARD		0x25
#define FP_VDM_READ						0x30
#define FP_VDM_WRITE					0x31
#define FP_NVM_READ						0X40
#define FP_NVM_WRITE					0x41
#define FP_NVM_OFF						0x42
#define FP_NVM_WRITEEX					0x43

#ifdef ENABLE_SENSORS_FPRINT_SECURE
#define FP_DISABLE_SPI_CLOCK				0x10
#define FP_CPU_SPEEDUP					0x11
#define FP_SET_SENSOR_TYPE				0x14
/* Do not use ioctl number 0x15 */
#define FP_SET_LOCKSCREEN				0x16
#define FP_SET_WAKE_UP_SIGNAL				0x17
#endif
#define FP_POWER_CONTROL_ET5XX				0x18
#define FP_SENSOR_ORIENT				0x19
#define FP_SPI_VALUE					0x1a
#define FP_IOCTL_RESERVED_01				0x1b

/* trigger signal initial routine */
#define INT_TRIGGER_INIT				0xa4
/* trigger signal close routine */
#define INT_TRIGGER_CLOSE				0xa5
/* read trigger status */
#define INT_TRIGGER_ABORT				0xa8
/* Sensor Registers */
#define FDATA_ET5XX_ADDR				0x00
#define FSTATUS_ET5XX_ADDR				0x01
/* Detect Define */
#define FRAME_READY_MASK				0x01

#define SHIFT_BYTE_OF_IMAGE 0
#define DIVISION_OF_IMAGE 4
#define LARGE_SPI_TRANSFER_BUFFER	64
/* NVM length in bytes (32 * 16 bits internally) */
#define MAX_NVM_LEN (32 * 2)
#define NVM_WRITE_LENGTH 4096
#define DETECT_ADM 1

struct fps_ioc_transfer {
	u8 *tx_buf;
	u8 *rx_buf;

	__u32 len;
	__u32 speed_hz;

	__u16 delay_usecs;
	__u8 bits_per_word;
	__u8 cs_change;
	__u8 opcode;
	__u8 pad[3];

};

#define EGIS_IOC_MAGIC			'j' /* it changed to 'j' */
#define EGIS_MSGSIZE(N) \
	((((N)*(sizeof(struct fps_ioc_transfer))) < (1 << _IOC_SIZEBITS)) \
		? ((N)*(sizeof(struct fps_ioc_transfer))) : 0)
#define EGIS_IOC_MESSAGE(N) _IOW(EGIS_IOC_MAGIC, 0, char[EGIS_MSGSIZE(N)])

/* IOCTL commands definitions */

/*
 * Magic number of IOCTL command
 */
#define VFSSPI_IOCTL_MAGIC	'k'

#ifndef ENABLE_SENSORS_FPRINT_SECURE
/*
 * Transmit data to the device and retrieve data from it simultaneously
 */
#define VFSSPI_IOCTL_RW_SPI_MESSAGE         _IOWR(VFSSPI_IOCTL_MAGIC,	\
							 1, unsigned int)
#endif

/*
 * Hard reset the device
 */
#define VFSSPI_IOCTL_DEVICE_RESET           _IO(VFSSPI_IOCTL_MAGIC,   2)
/*
 * Set the baud rate of SPI master clock
 */
#define VFSSPI_IOCTL_SET_CLK                _IOW(VFSSPI_IOCTL_MAGIC,	\
							 3, unsigned int)
#ifndef ENABLE_SENSORS_FPRINT_SECURE
/*
 * Get level state of DRDY GPIO
 */
#define VFSSPI_IOCTL_CHECK_DRDY             _IO(VFSSPI_IOCTL_MAGIC,   4)
#endif

/*
 * Register DRDY signal. It is used by SPI driver for indicating host that
 * DRDY signal is asserted.
 */
#define VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL   _IOW(VFSSPI_IOCTL_MAGIC,	\
							 5, unsigned int)
/*
 * Store the user data into the SPI driver. Currently user data is a
 * device info data, which is obtained from announce packet.

#define VFSSPI_IOCTL_SET_USER_DATA          _IOW(VFSSPI_IOCTL_MAGIC,	\
							 6, unsigned int)
*/

/*
 * Retrieve user data from the SPI driver

#define VFSSPI_IOCTL_GET_USER_DATA          _IOWR(VFSSPI_IOCTL_MAGIC,	\
							 7, unsigned int)
*/

/*
 * Enable/disable DRDY interrupt handling in the SPI driver
 */
#define VFSSPI_IOCTL_SET_DRDY_INT           _IOW(VFSSPI_IOCTL_MAGIC,	\
							8, unsigned int)
/*
 * Put device in suspend state
 */
#define VFSSPI_IOCTL_DEVICE_SUSPEND         _IO(VFSSPI_IOCTL_MAGIC,   9)

#ifndef ENABLE_SENSORS_FPRINT_SECURE
/*
 * Indicate the fingerprint buffer size for read
 */
#define VFSSPI_IOCTL_STREAM_READ_START      _IOW(VFSSPI_IOCTL_MAGIC,	\
						 10, unsigned int)
/*
 * Indicate that fingerprint acquisition is completed
 */
#define VFSSPI_IOCTL_STREAM_READ_STOP       _IO(VFSSPI_IOCTL_MAGIC,   11)
#endif
/* Turn on the power to the sensor */
#define VFSSPI_IOCTL_POWER_ON               _IO(VFSSPI_IOCTL_MAGIC,   13)
/* Turn off the power to the sensor */
#define VFSSPI_IOCTL_POWER_OFF              _IO(VFSSPI_IOCTL_MAGIC,   14)
#ifdef ENABLE_SENSORS_FPRINT_SECURE
/* To disable spi core clock */
#define VFSSPI_IOCTL_DISABLE_SPI_CLOCK     _IO(VFSSPI_IOCTL_MAGIC,    15)
/* To set SPI configurations like gpio, clks */
#define VFSSPI_IOCTL_SET_SPI_CONFIGURATION _IO(VFSSPI_IOCTL_MAGIC,    16)
/* To reset SPI configurations */
#define VFSSPI_IOCTL_RESET_SPI_CONFIGURATION _IO(VFSSPI_IOCTL_MAGIC,  17)
/* To switch core */
#define VFSSPI_IOCTL_CPU_SPEEDUP     _IOW(VFSSPI_IOCTL_MAGIC,	\
						19, unsigned int)
#define VFSSPI_IOCTL_SET_SENSOR_TYPE     _IOW(VFSSPI_IOCTL_MAGIC,	\
							20, unsigned int)
/* IOCTL #21 was already used Synaptics service. Do not use #21 */
#define VFSSPI_IOCTL_SET_LOCKSCREEN     _IOW(VFSSPI_IOCTL_MAGIC,	\
							22, unsigned int)
#endif
/* To control the power */
#define VFSSPI_IOCTL_POWER_CONTROL     _IOW(VFSSPI_IOCTL_MAGIC,	\
							23, unsigned int)
/* get sensor orienation from the SPI driver*/
#define VFSSPI_IOCTL_GET_SENSOR_ORIENT	\
	_IOR(VFSSPI_IOCTL_MAGIC, 18, unsigned int)

/*
 * Used by IOCTL command:
 *         VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL
 *
 * @user_pid:Process ID to which SPI driver sends signal indicating that
 *			DRDY is asserted
 * @signal_id:signal_id
 */
struct fps_ioctl_register_signal {
	int user_pid;
	int signal_id;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_mutex);
static DECLARE_WAIT_QUEUE_HEAD(interrupt_waitq);
static struct class *fps_device_class;
static int gpio_irq;

struct fps_device_data {
	dev_t devt;
	struct cdev cdev;
	struct spi_device *spi;
	struct list_head device_entry;
	struct mutex buffer_mutex;
	unsigned int drdy_pin;
	unsigned int sleep_pin;
	unsigned int ldo_pin;
	const char *btp_vdd;
	struct regulator *regulator_3p3;
	int cnt_irq;
	const char *chipid;
	struct task_struct *t;
	int user_pid;
	int signal_id;
	unsigned int current_spi_speed;
	atomic_t irq_enabled;
	bool ldo_onoff;
	spinlock_t irq_lock;
	unsigned short drdy_irq_flag;
	unsigned int min_cpufreq_limit;
	int detect_mode;

	struct work_struct work_debug;
	struct workqueue_struct *wq_dbg;
	struct timer_list dbg_timer;
	struct pinctrl *p;
	struct pinctrl_state *pins_sleep;
	struct pinctrl_state *pins_idle;
	bool tz_mode;
	struct device *fp_device;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	bool enabled_clk;
	struct wake_lock fp_spi_lock;
#endif
	struct wake_lock fp_signal_lock;
	int sensortype;
	unsigned int orient;
	unsigned int egis_sensor; /* 0: namsan, 1: egis */

	/* for egis*/
	int detect_period;
	int detect_threshold;
	bool finger_on;
	u32 spi_value;
};

static struct fps_device_data *g_data;

#endif /* VFS8XXX_H_ */
