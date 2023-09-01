/*
 * Vibrator driver for DW7720
 *
 * Copyright (C) 2018 Dongwoon Anatech Co. Ltd. All Rights Reserved.
 *
 * license : GPL/BSD Dual 
 *
 * The DW7720 is Digital Hall Sensor for Mobile phone. 
 *
 * DW7720 includes internal hall and A/D converter and D/A converter for biasing of internal Hall sensor.
 */

int dw772x_reverse_byte_read(u8 addr);
int dw772x_reverse_byte_write(u8 addr, u8 data);

int dw772x_reverse_word_read(u8 addr);
int dw772x_reverse_word_write(u8 addr, u32 data);

int dw772x_reverse_init_mode_sel(int mode, u32 data);
int dw772x_reverse_hall_read(void);
int dw772x_reverse_hall_out(void);

static int dw772x_reverse_magnet_cal_set(void);
static int dw772x_reverse_device_init(struct i2c_client *client);
#if 0 // disable interrupt routine
static int irq_rtp_exit(void);
static int irq_rtp_init(void);
#endif
static int dw772x_reverse_seq_read(u32 addr, u32 ram_addr, u32 ram_bit, u8* data, u32 size);
static int dw772x_reverse_seq_write(u32 addr, u32 ram_addr, u32 ram_bit, u8* data, u32 size);

struct dw772x_reverse_priv {
	s8 dev_name[6];
	struct device *dev;
	s32 enable;
	s32 irq_gpio;
	u32 device;
	u32 irq_num;
	u32 checksum;
	u32 version;	
	struct mutex dev_lock;
	struct hrtimer hr_timer;
	struct i2c_client *dwclient;

	struct input_dev *input;
	bool digital_reverse_hall;
	bool is_suspend;
	int cal1,cal2;
};

static struct kobject *android_hall_kobj;
static struct dw772x_reverse_priv *dw772x_reverse = NULL;

#ifdef CONFIG_OF
static struct of_device_id dw772x_reverse_i2c_dt_ids[] = {
	{ .compatible = "dwanatech,dw772x_reverse"},
	{ }
};
#endif

static const struct i2c_device_id dw772x_reverse_drv_id[] = {
	{"dw772x_reverse", 0},
	{}
};

/*----------- Common Register ------------*/

#define DW7720

#define RAM_ADDR8		0
#define RAM_ADDR16		16

#define SLEEP_MODE		0
#define INT_MODE		1
#define SEQ_MODE		2
#define SEQ_STORE_MODE	3
#define HALL_SET_MODE	4
#define ACTIVE_MODE		5
#define HALL_READOUT	6
#define HALL_CAL_MODE	7
#define STANDBY_MODE	8

#define HALL_MSB	0x08
#define HALL_LSB	0x07
