#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/regulator/consumer.h>
#include <linux/sensors_core.h>

#ifdef CONFIG_BMA254_SMART_ALERT
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#endif
#include "sensors_head.h"

#define CHIP_DEV_NAME		"BMA254"
#define CHIP_DEV_VENDOR		"BOSCH"

#define I2C_RETRIES			5

#define BMA254_CHIP_ID		0xFA
#define SOFT_RESET			0xB6

#define alps_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args)
#define alps_errmsg(str, args...) pr_err("%s: " str, __func__, ##args)
#define alps_info(str, args...) pr_info("%s: " str, __func__, ##args)


/*
 *      register definitions
 */
#define BMA254_CHIP_ID_REG                      0x00
#define BMA254_X_AXIS_LSB_REG                   0x02
#define BMA254_X_AXIS_MSB_REG                   0x03
#define BMA254_Y_AXIS_LSB_REG                   0x04
#define BMA254_Y_AXIS_MSB_REG                   0x05
#define BMA254_Z_AXIS_LSB_REG                   0x06
#define BMA254_Z_AXIS_MSB_REG                   0x07
#define BMA254_RANGE_SEL_REG                    0x0F
#define BMA254_BW_SEL_REG                       0x10
#define BMA254_MODE_CTRL_REG                    0x11
#define BMA254_RESET_REG                        0x14
#define BMA254_THETA_BLOCK_REG                  0x2D
#define BMA254_THETA_FLAT_REG                   0x2E
#define BMA254_FLAT_HOLD_TIME_REG               0x2F
#define BMA254_SELF_TEST_REG                    0x32
#define BMA254_OFFSET_CTRL_REG                  0x36
#define BMA254_OFFSET_PARAMS_REG                0x37

#define BMA254_ACC_X_LSB__POS           4
#define BMA254_ACC_X_LSB__LEN           4
#define BMA254_ACC_X_LSB__MSK           0xF0
#define BMA254_ACC_X_LSB__REG           BMA254_X_AXIS_LSB_REG

#define BMA254_ACC_X_MSB__POS           0
#define BMA254_ACC_X_MSB__LEN           8
#define BMA254_ACC_X_MSB__MSK           0xFF
#define BMA254_ACC_X_MSB__REG           BMA254_X_AXIS_MSB_REG

#define BMA254_ACC_Y_LSB__POS           4
#define BMA254_ACC_Y_LSB__LEN           4
#define BMA254_ACC_Y_LSB__MSK           0xF0
#define BMA254_ACC_Y_LSB__REG           BMA254_Y_AXIS_LSB_REG

#define BMA254_ACC_Y_MSB__POS           0
#define BMA254_ACC_Y_MSB__LEN           8
#define BMA254_ACC_Y_MSB__MSK           0xFF
#define BMA254_ACC_Y_MSB__REG           BMA254_Y_AXIS_MSB_REG

#define BMA254_ACC_Z_LSB__POS           4
#define BMA254_ACC_Z_LSB__LEN           4
#define BMA254_ACC_Z_LSB__MSK           0xF0
#define BMA254_ACC_Z_LSB__REG           BMA254_Z_AXIS_LSB_REG

#define BMA254_ACC_Z_MSB__POS           0
#define BMA254_ACC_Z_MSB__LEN           8
#define BMA254_ACC_Z_MSB__MSK           0xFF
#define BMA254_ACC_Z_MSB__REG           BMA254_Z_AXIS_MSB_REG

#define BMA254_RANGE_SEL__POS           0
#define BMA254_RANGE_SEL__LEN           4
#define BMA254_RANGE_SEL__MSK           0x0F
#define BMA254_RANGE_SEL__REG           BMA254_RANGE_SEL_REG

#define BMA254_BANDWIDTH__POS           0
#define BMA254_BANDWIDTH__LEN           5
#define BMA254_BANDWIDTH__MSK           0x1F
#define BMA254_BANDWIDTH__REG           BMA254_BW_SEL_REG

#define BMA254_EN_LOW_POWER__POS        6
#define BMA254_EN_LOW_POWER__LEN        1
#define BMA254_EN_LOW_POWER__MSK        0x40
#define BMA254_EN_LOW_POWER__REG        BMA254_MODE_CTRL_REG

#define BMA254_EN_SUSPEND__POS          7
#define BMA254_EN_SUSPEND__LEN          1
#define BMA254_EN_SUSPEND__MSK          0x80
#define BMA254_EN_SUSPEND__REG          BMA254_MODE_CTRL_REG

#define BMA254_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define BMA254_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

/* range registers */
#define BMA254_RANGE_2G                 3
#define BMA254_RANGE_4G                 5
#define BMA254_RANGE_8G                 8
#define BMA254_RANGE_16G               12

/* bandwidth registers */
#define BMA254_BW_7DOT81HZ          0x08
#define BMA254_BW_15DOT63HZ         0x09
#define BMA254_BW_31DOT25HZ         0x0A
#define BMA254_BW_62DOT50HZ         0x0B
#define BMA254_BW_125HZ			0x0C
#define BMA254_BW_250HZ			0x0D
#define BMA254_BW_500HZ			0x0E
#define BMA254_BW_1000HZ		0x0F

#define BMA254_RANGE_SET		BMA254_RANGE_2G
#define BMA254_BW_SET			BMA254_BW_15DOT63HZ

/* mode settings */
#define BMA254_MODE_NORMAL          0
#define BMA254_MODE_LOWPOWER        1
#define BMA254_MODE_SUSPEND         2

#define BMA254_EN_SELF_TEST__POS                0
#define BMA254_EN_SELF_TEST__LEN                2
#define BMA254_EN_SELF_TEST__MSK                0x03
#define BMA254_EN_SELF_TEST__REG                BMA254_SELF_TEST_REG

#define BMA254_NEG_SELF_TEST__POS               2
#define BMA254_NEG_SELF_TEST__LEN               1
#define BMA254_NEG_SELF_TEST__MSK               0x04
#define BMA254_NEG_SELF_TEST__REG               BMA254_SELF_TEST_REG

#define BMA254_EN_FAST_COMP__POS                5
#define BMA254_EN_FAST_COMP__LEN                2
#define BMA254_EN_FAST_COMP__MSK                0x60
#define BMA254_EN_FAST_COMP__REG                BMA254_OFFSET_CTRL_REG

#define BMA254_FAST_COMP_RDY_S__POS             4
#define BMA254_FAST_COMP_RDY_S__LEN             1
#define BMA254_FAST_COMP_RDY_S__MSK             0x10
#define BMA254_FAST_COMP_RDY_S__REG             BMA254_OFFSET_CTRL_REG

#define BMA254_COMP_TARGET_OFFSET_X__POS        1
#define BMA254_COMP_TARGET_OFFSET_X__LEN        2
#define BMA254_COMP_TARGET_OFFSET_X__MSK        0x06
#define BMA254_COMP_TARGET_OFFSET_X__REG        BMA254_OFFSET_PARAMS_REG

#define BMA254_COMP_TARGET_OFFSET_Y__POS        3
#define BMA254_COMP_TARGET_OFFSET_Y__LEN        2
#define BMA254_COMP_TARGET_OFFSET_Y__MSK        0x18
#define BMA254_COMP_TARGET_OFFSET_Y__REG        BMA254_OFFSET_PARAMS_REG

#define BMA254_COMP_TARGET_OFFSET_Z__POS        5
#define BMA254_COMP_TARGET_OFFSET_Z__LEN        2
#define BMA254_COMP_TARGET_OFFSET_Z__MSK        0x60
#define BMA254_COMP_TARGET_OFFSET_Z__REG        BMA254_OFFSET_PARAMS_REG

#define CALIBRATION_FILE_PATH	"/efs/calibration_data"
#define CALIBRATION_DATA_AMOUNT	100

#ifdef CONFIG_BMA254_SMART_ALERT
#define BMA254_STATUS1_REG						0x09
#define BMA254_INT1_PAD_SEL_REG					0x19
#define BMA254_SLOPE_DURN_REG					0x27
#define BMA254_INT_ENABLE1_REG					0x16
#define BMA254_SLOPE_THRES_REG					0x28
#define BMA254_SLOPE_INT_S__REG				BMA254_STATUS1_REG
#define BMA254_EN_INT1_PAD_SLOPE__REG			BMA254_INT1_PAD_SEL_REG
#define BMA254_SLOPE_DUR__REG				BMA254_SLOPE_DURN_REG
#define BMA254_EN_SLOPE_X_INT__REG			BMA254_INT_ENABLE1_REG
#define BMA254_SLOPE_THRES__REG				BMA254_SLOPE_THRES_REG
#endif
#define WHO_AM_I		0x00

//#define SMART_ALERT_REV_CHECK
#define ACC_INT_REV			5
// Loganre(S7275R) has h/w problem. Apply s/w workaround about raw data offset
#ifdef CONFIG_MACH_LOGANRE
#define ACC_RAW_DATA_OFFSET
#endif
#ifdef ACC_RAW_DATA_OFFSET
#define ACC_RAW_DATA_OFFSET_X	13
#define ACC_RAW_DATA_OFFSET_Y	-135
#endif

struct acc_data {
	int x;
	int y;
	int z;
};

#ifdef CONFIG_BMA254_SMART_ALERT
struct bma254_platform_data {
	int p_out;				/* acc-sensor-irq gpio */
};
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend bma254_early_suspend_handler;
#endif

static int accel_open_calibration(void);

struct acc_data caldata;
struct i2c_client *this_client;
//static struct bma254_power_data bma254_power;

static struct i2c_driver bma254_driver;

static atomic_t flgEna;
static atomic_t delay;

static int probe_done;
extern int system_rev;

#ifdef CONFIG_BMA254_SMART_ALERT
struct bma254_data {
	struct i2c_client *bma254_client;
	struct bma254_platform_data *pdata;
	struct mutex data_mutex;
	struct work_struct alert_work;
	struct wake_lock reactive_wake_lock;
	atomic_t reactive_state;
	atomic_t reactive_enable;
	bool factory_mode;
	int accsns_activate_flag;
	int pin_check_fail;
	int IRQ;
};

static struct bma254_data bma254_data;

#endif

static int bma254_i2c_writem(u8 *txData, int length)
{
	int err;
	int tries = 0;
	struct i2c_msg msg[] = {
		{
			.addr = this_client->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};

	do {
		err = i2c_transfer(this_client->adapter, msg, 1);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&this_client->adapter->dev,
					"write transfer error [%d]\n", err);
		err = -EIO;
	} else
		err = 0;

	return err;
}

static int bma254_i2c_readm(u8 *rxData, int length)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = this_client->addr,
			.flags = 0,
			.len = 1,
			.buf = rxData,
		},
		{
			.addr = this_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		},
	};

	do {
		err = i2c_transfer(this_client->adapter, msgs, 2);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&this_client->adapter->dev, "read transfer error\n");
		err = -EIO;
	} else
		err = 0;

	return err;
}

static int bma254_smbus_read_byte_block(unsigned char reg_addr,
unsigned char *data, unsigned char len)
{
	s32 dummy;
	dummy = i2c_smbus_read_i2c_block_data(this_client, reg_addr, len, data);
	if (dummy < 0)
		return -1;

	return 0;
}

int bma254_get_acceleration_data(int *xyz)
{
	u8 buf[6];
	int err;

	if (this_client == NULL) {
		xyz[0] = xyz[1] = xyz[2] = 0;
		return -ENODEV;
	}

	err = bma254_smbus_read_byte_block(BMA254_ACC_X_LSB__REG, buf, 6);

	xyz[0] = BMA254_GET_BITSLICE(buf[0], BMA254_ACC_X_LSB)
		|(BMA254_GET_BITSLICE(buf[1], BMA254_ACC_X_MSB)
				<< BMA254_ACC_X_LSB__LEN);
	xyz[0] = xyz[0] << (sizeof(int) * 8 - (BMA254_ACC_X_LSB__LEN
				+ BMA254_ACC_X_MSB__LEN));
	xyz[0] = xyz[0] >> (sizeof(int) * 8 - (BMA254_ACC_X_LSB__LEN
				+ BMA254_ACC_X_MSB__LEN));

	xyz[1] = BMA254_GET_BITSLICE(buf[2], BMA254_ACC_Y_LSB)
		| (BMA254_GET_BITSLICE(buf[3], BMA254_ACC_Y_MSB)
				<<BMA254_ACC_Y_LSB__LEN);
	xyz[1] = xyz[1] << (sizeof(int) * 8 - (BMA254_ACC_Y_LSB__LEN
				+ BMA254_ACC_Y_MSB__LEN));
	xyz[1] = xyz[1] >> (sizeof(int) * 8 - (BMA254_ACC_Y_LSB__LEN
				+ BMA254_ACC_Y_MSB__LEN));

	xyz[2] = BMA254_GET_BITSLICE(buf[4], BMA254_ACC_Z_LSB)
		| (BMA254_GET_BITSLICE(buf[5], BMA254_ACC_Z_MSB)
				<<BMA254_ACC_Z_LSB__LEN);
	xyz[2] = xyz[2] << (sizeof(int) * 8 - (BMA254_ACC_Z_LSB__LEN
				+ BMA254_ACC_Z_MSB__LEN));
	xyz[2] = xyz[2] >> (sizeof(int) * 8 - (BMA254_ACC_Z_LSB__LEN
				+ BMA254_ACC_Z_MSB__LEN));

#ifdef ACC_RAW_DATA_OFFSET
	xyz[0] -= ACC_RAW_DATA_OFFSET_X;
	xyz[1] -= ACC_RAW_DATA_OFFSET_Y;
#endif

	xyz[0] -= caldata.x;
	xyz[1] -= caldata.y;
	xyz[2] -= caldata.z;

	return err;
}
EXPORT_SYMBOL(bma254_get_acceleration_data);

int bma254_get_acceleration_rawdata(int *xyz)
{
	u8 buf[6];
	int err;

	err = bma254_smbus_read_byte_block(BMA254_ACC_X_LSB__REG, buf, 6);

	xyz[0] = BMA254_GET_BITSLICE(buf[0], BMA254_ACC_X_LSB)
		|(BMA254_GET_BITSLICE(buf[1], BMA254_ACC_X_MSB)
				<< BMA254_ACC_X_LSB__LEN);
	xyz[0] = xyz[0] << (sizeof(int) * 8 - (BMA254_ACC_X_LSB__LEN
				+ BMA254_ACC_X_MSB__LEN));
	xyz[0] = xyz[0] >> (sizeof(int) * 8 - (BMA254_ACC_X_LSB__LEN
				+ BMA254_ACC_X_MSB__LEN));

	xyz[1] = BMA254_GET_BITSLICE(buf[2], BMA254_ACC_Y_LSB)
		| (BMA254_GET_BITSLICE(buf[3], BMA254_ACC_Y_MSB)
				<<BMA254_ACC_Y_LSB__LEN);
	xyz[1] = xyz[1] << (sizeof(int) * 8 - (BMA254_ACC_Y_LSB__LEN
				+ BMA254_ACC_Y_MSB__LEN));
	xyz[1] = xyz[1] >> (sizeof(int) * 8 - (BMA254_ACC_Y_LSB__LEN
				+ BMA254_ACC_Y_MSB__LEN));

	xyz[2] = BMA254_GET_BITSLICE(buf[4], BMA254_ACC_Z_LSB)
		| (BMA254_GET_BITSLICE(buf[5], BMA254_ACC_Z_MSB)
				<<BMA254_ACC_Z_LSB__LEN);
	xyz[2] = xyz[2] << (sizeof(int) * 8 - (BMA254_ACC_Z_LSB__LEN
				+ BMA254_ACC_Z_MSB__LEN));
	xyz[2] = xyz[2] >> (sizeof(int) * 8 - (BMA254_ACC_Z_LSB__LEN
				+ BMA254_ACC_Z_MSB__LEN));

#ifdef ACC_RAW_DATA_OFFSET
	xyz[0] -= ACC_RAW_DATA_OFFSET_X;
	xyz[1] -= ACC_RAW_DATA_OFFSET_Y;
#endif

	return err;
}

void bma254_activate(int flgatm, int flg, int dtime)
{
#ifdef CONFIG_BMA254_SMART_ALERT
	struct bma254_data *bma254 =  &bma254_data;
#endif
	u8 buf[2];
	if (this_client == NULL)
		return;
	else if ((atomic_read(&delay) == dtime) && (atomic_read(&flgEna) == flg)
				&& (flgatm == 1)) {
		pr_info("%s: returned\n", __func__);
		return;
	}
	alps_info("is called\n");

#ifdef CONFIG_BMA254_SMART_ALERT
if (!bma254->accsns_activate_flag) {
	if (flg != 0) {
		buf[0] = BMA254_RESET_REG;
		buf[1] = SOFT_RESET;
		bma254_i2c_writem(buf, 2);
		msleep(20);
	}
}
#else

	if (flg != 0) {
		buf[0] = BMA254_RESET_REG;
		buf[1] = SOFT_RESET;
		bma254_i2c_writem(buf, 2);
		msleep(20);
	}
#endif
	/* accelerometer g-range set */
	buf[0] = BMA254_RANGE_SEL_REG;
	buf[1] = 0x03;		/*g-range +/-2g*/
	bma254_i2c_writem(buf, 2);

	/* filter bandwidth set */
	buf[0] = BMA254_BW_SEL_REG;
	buf[1] = BMA254_BW_31DOT25HZ;
	bma254_i2c_writem(buf, 2);

#ifdef CONFIG_BMA254_SMART_ALERT
	/* low power mode set */
	bma254_i2c_readm(buf, 1);
	buf[0] = BMA254_MODE_CTRL_REG;

	if ((flg == 0) && (bma254->accsns_activate_flag)) {
		pr_info("%s: low power mode\n", __func__);
		buf[1] = 0x40; /*low power mode */
	} else if (flg == 0) {
		buf[1] = 0x80; /*sleep*/
	} else {
		buf[1] = 0x00;
	}
	bma254_i2c_writem(buf, 2);
#else
	/* low power mode set */
	bma254_i2c_readm(buf, 1);
	buf[0] = BMA254_MODE_CTRL_REG;
	if (flg == 0)
		buf[1] = 0x80; /*sleep*/
	else
		buf[1] = 0x00;

	bma254_i2c_writem(buf, 2);
#endif
	if (flgatm) {
		atomic_set(&flgEna, flg);
		atomic_set(&delay, dtime);
	}
}
EXPORT_SYMBOL(bma254_activate);

static int accel_open_calibration(void)
{
	int err = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);

		caldata.x = 0;
		caldata.y = 0;
		caldata.z = 0;

		return err;
	}

	err = cal_filp->f_op->read(cal_filp,
			(char *)&caldata, 3 * sizeof(int), &cal_filp->f_pos);
	if (err != 3 * sizeof(int))
		err = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	if ((caldata.x == 0xffff) && (caldata.y == 0xffff)
			&& (caldata.z == 0xffff)) {
		caldata.x = 0;
		caldata.y = 0;
		caldata.z = 0;

		return -1;
	}

	pr_info("%s: %d, %d, %d\n", __func__,
			caldata.x, caldata.y, caldata.z);
	return err;
}

static int accel_do_calibrate(int enable)
{
	int sum[3] = { 0, };
	int err = 0;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	if (enable) {
		int data[3] = { 0, };
		int cnt;
		for (cnt = 0; cnt < CALIBRATION_DATA_AMOUNT; cnt++) {
			err = bma254_get_acceleration_rawdata(data);
			if (err < 0) {
				pr_err("%s: accel_read_accel_raw_xyz() "
						"failed in the %dth loop\n",
						__func__, cnt);
				return err;
			}

			sum[0] += data[0];
			sum[1] += data[1];
			sum[2] += data[2];
		}

		caldata.x = (sum[0] / CALIBRATION_DATA_AMOUNT);
		caldata.y = (sum[1] / CALIBRATION_DATA_AMOUNT);
		/* Set z-axis calibration target based on current z-axis value */
		if ( sum[2] >= 0)
			caldata.z = (sum[2] / CALIBRATION_DATA_AMOUNT) - 1024;
		else
			caldata.z = (sum[2] / CALIBRATION_DATA_AMOUNT) + 1024;
	} else {
		caldata.x = 0xffff;
		caldata.y = 0xffff;
		caldata.z = 0xffff;
	}

	pr_info("%s: cal data (%d,%d,%d)\n", __func__,
			caldata.x, caldata.y, caldata.z);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->write(cal_filp,
			(char *)&caldata, 3 * sizeof(int), &cal_filp->f_pos);
	if (err != 3 * sizeof(int)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}

int bma254_open(void)
{
	pr_info("[ACC] bma254 probe_done: %d \n", probe_done);
	if(probe_done == PROBE_SUCCESS)
		accel_open_calibration();
	return probe_done;
}
EXPORT_SYMBOL(bma254_open);

#ifdef CONFIG_BMA254_SMART_ALERT
static int bma254_get_motion_interrupt(struct i2c_client *client)
{
	int result = 0;
	unsigned char data;
	u8 buf[2];
	data = bma254_smbus_read_byte_block(BMA254_SLOPE_INT_S__REG, buf, 2);
	pr_info("%s: call : return %d\n", __func__, data);
	result = 1;
	return result;
}

static void bma254_set_motion_interrupt(struct i2c_client *client, bool enable,
	bool factorytest)
{
	u8 buf[2];
	//unsigned char data;
	if (enable) {
		bma254_activate(0, 0, atomic_read(&delay));
		usleep_range(5000, 6000);
		pr_info("%s : enable\n", __func__);
		buf[0] = BMA254_EN_INT1_PAD_SLOPE__REG;
		buf[1] = 0x04;
		bma254_i2c_writem(buf, 2);
		if (factorytest) {
			buf[0] = BMA254_SLOPE_DUR__REG;
			buf[1] = 0x00;
			bma254_i2c_writem(buf, 2);

			buf[0] = BMA254_SLOPE_THRES__REG;
			buf[1] = 0x00;
			bma254_i2c_writem(buf, 2);
		} else {
			buf[0] = BMA254_SLOPE_DUR__REG;
			buf[1] = 0x02;
			bma254_i2c_writem(buf, 2);

			buf[0] = BMA254_SLOPE_THRES__REG;
			buf[1] = 0x10;
			bma254_i2c_writem(buf, 2);
		}

		buf[0] = BMA254_INT_ENABLE1_REG;
		buf[1] = 0x07;
		bma254_i2c_writem(buf, 2);
	} else {
		pr_info("%s : disable\n", __func__);
		buf[0] = BMA254_EN_INT1_PAD_SLOPE__REG;
		buf[1] = 0x00;
		bma254_i2c_writem(buf, 2);

		buf[0] = BMA254_SLOPE_DUR__REG;
		buf[1] = 0x03;
		bma254_i2c_writem(buf, 2);

		buf[0] = BMA254_INT_ENABLE1_REG;
		buf[1] = 0x00;
		bma254_i2c_writem(buf, 2);

		buf[0] = BMA254_SLOPE_THRES__REG;
		buf[1] = 0xff;
		bma254_i2c_writem(buf, 2);

		usleep_range(5000, 6000);
	}
}
#endif
static ssize_t accel_calibration_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int err;
	int count = 0;

	err = accel_open_calibration();

	if (err < 0)
		pr_err("%s: accel_open_calibration() failed\n", __func__);

	pr_info("%d %d %d %d\n",
			err, caldata.x, caldata.y, caldata.z);

	count = sprintf(buf, "%d %d %d %d\n",
					err, caldata.x, caldata.y, caldata.z);
	return count;
}

static ssize_t accel_calibration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int err;
	int64_t enable;

	err = strict_strtoll(buf, 10, &enable);
	if (err < 0)
		return err;

	err = accel_do_calibrate((int)enable);
	if (err < 0)
		pr_err("%s: accel_do_calibrate() failed\n", __func__);

	if (!enable) {
		caldata.x = 0;
		caldata.y = 0;
		caldata.z = 0;
	}

	return size;
}

static ssize_t raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int xyz[3] = {0, };

	bma254_get_acceleration_data(xyz);

return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
			xyz[0], xyz[1], xyz[2]);
}

static ssize_t raw_data_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("raw_data_write is work");
	return size;
}

static ssize_t name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_NAME);
}

static ssize_t vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_VENDOR);
}


#ifdef CONFIG_BMA254_SMART_ALERT
static ssize_t bma254_reactive_enable_show(struct device *dev,
					struct device_attribute
						*attr, char *buf)
{
	struct bma254_data *bma254 = &bma254_data;
	return sprintf(buf, "%d\n",
		atomic_read(&bma254->reactive_state));
}

static ssize_t bma254_reactive_enable_store(struct device *dev,
					struct device_attribute
						*attr, const char *buf,
							size_t count)
{
	struct bma254_data *bma254 =  &bma254_data;
	bool onoff = false;
	bool factory_test = false;
	unsigned long value = 0;
	int err = count;
	if (strict_strtoul(buf, 10, &value)) {
		err = -EINVAL;
		return err;
	}

	switch (value) {
	case 0:
		bma254->accsns_activate_flag = 0;
		break;
	case 1:
		bma254->accsns_activate_flag = 1;
		onoff = true;
		break;
	case 2:
		bma254->accsns_activate_flag = 1;
		onoff = true;
		factory_test = true;
		break;
	default:
		//err = -EINVAL;
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return count;
	}
#if defined(SMART_ALERT_REV_CHECK)
	if (system_rev >= ACC_INT_REV) {
#endif
		if (!bma254->pin_check_fail) {
			if (bma254->IRQ) {
				if (!value) {
					disable_irq_wake(bma254->IRQ);
					disable_irq(bma254->IRQ);
				} else {
					enable_irq(bma254->IRQ);
					enable_irq_wake(bma254->IRQ);
				}
			}
			mutex_lock(&bma254->data_mutex);
			atomic_set(&bma254->reactive_enable, onoff);

	if (bma254->IRQ) {
		bma254_set_motion_interrupt(bma254->bma254_client,
			onoff, factory_test);
	}
	atomic_set(&bma254->reactive_state, false);
	mutex_unlock(&bma254->data_mutex);
	pr_info("%s: value = %lu, onoff = %d, state =%d\n",
		__func__, value,
		atomic_read(&bma254->reactive_enable),
		atomic_read(&bma254->reactive_state));
#if defined(SMART_ALERT_REV_CHECK)
		}
#endif
	}
	return count;
}
#endif

static DEVICE_ATTR(raw_data, S_IRUGO | S_IWUSR | S_IWGRP,
	raw_data_read, raw_data_write);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
	accel_calibration_show, accel_calibration_store);
static DEVICE_ATTR(name, S_IRUGO | S_IWUSR | S_IWGRP, name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO | S_IWUSR | S_IWGRP, vendor_show, NULL);

#ifdef CONFIG_BMA254_SMART_ALERT
static DEVICE_ATTR(reactive_alert, S_IRUGO|S_IWUSR|S_IWGRP,
		bma254_reactive_enable_show, bma254_reactive_enable_store);
#endif

static struct device_attribute *bma254_attrs[] = {
	&dev_attr_raw_data,
	&dev_attr_calibration,
	&dev_attr_name,
	&dev_attr_vendor,
#ifdef CONFIG_BMA254_SMART_ALERT
	&dev_attr_reactive_alert,
#endif
	NULL,
};
#ifdef CONFIG_BMA254_SMART_ALERT
static void bma254_work_func_alert(struct work_struct *work)
{
	int result;

	struct bma254_data *bma254 =  &bma254_data;
	result = bma254_get_motion_interrupt(bma254->bma254_client);
	if (result || bma254->factory_mode) {
		/*handle motion recognition*/
		atomic_set(&bma254->reactive_state, true);
		bma254->factory_mode = false;

		pr_info("%s: motion interrupt happened\n",
			__func__);
		wake_lock_timeout(&bma254->reactive_wake_lock,
			msecs_to_jiffies(2000));
	}
}

irqreturn_t bma254_acc_irq_thread(int irq, void *dev)
{
	struct bma254_data *data = dev;
	schedule_work(&data->alert_work);
	return IRQ_HANDLED;
}

static int bma254_setup_irq(struct bma254_data *bma254)
{
	int irq = -1;
	struct bma254_platform_data *pdata = bma254->pdata;
	int rc;
	pr_info("%s\n", __func__);

	rc = gpio_request(pdata->p_out, "gpio_bma254_int");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, pdata->p_out, rc);
		return rc;
	}

	rc = gpio_direction_input(pdata->p_out);
	if (rc < 0) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, pdata->p_out, rc);
		goto err_gpio_direction_input;
	}

	irq = gpio_to_irq(pdata->p_out);

	if (irq > 0) {
		rc = request_threaded_irq(irq,
			NULL, bma254_acc_irq_thread,
			IRQF_TRIGGER_FALLING,
			"accelerometer", bma254);

		pr_info("%s: irq = %d\n", __func__, irq);

		if (rc < 0) {
			pr_err("%s request_threaded_irq fail err=%d\n",
				__func__, rc);
			return rc;
		}
		/* start with interrupts disabled */
		disable_irq(irq);
	}
	bma254_data.IRQ = irq;
	goto done;

err_gpio_direction_input:
	gpio_free(pdata->p_out);
done:
	return rc;
}
#endif

static int bma254_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct device *bma_device = NULL;
#ifdef CONFIG_BMA254_SMART_ALERT
	struct bma254_data *data = &bma254_data;
	struct bma254_platform_data *pdata = client->dev.platform_data;
#endif
	this_client = client;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->adapter->dev, "client not i2c capable\n");
		ret = -ENOMEM;
		goto err_bma254_sensors;
	}

	/* read chip id */
	ret = i2c_smbus_read_byte_data(this_client, WHO_AM_I);
	pr_info("%s : device ID = 0x%x, reading ID = 0x%x\n", __func__,
		BMA254_CHIP_ID, ret);
	if (ret == BMA254_CHIP_ID) /* Normal Operation */
		ret = 0;
	else {
		if (ret < 0)
			pr_err("%s: i2c for reading chip id failed\n",
			       __func__);
		else {
			pr_err("%s : Device identification failed\n",
			       __func__);
			ret = -ENODEV;
		}
		goto err_bma254_sensors;
	}

	atomic_set(&flgEna, 0);
	atomic_set(&delay, 100);
	pr_info("%s:  system_rev success %d\n", __func__, system_rev);
#ifdef CONFIG_BMA254_SMART_ALERT
#if defined(SMART_ALERT_REV_CHECK)
	if (system_rev >= ACC_INT_REV) {
#endif
		data->pdata = pdata;
		INIT_WORK(&data->alert_work, bma254_work_func_alert);
		wake_lock_init(&data->reactive_wake_lock, WAKE_LOCK_SUSPEND,
			"reactive_wake_lock");
		ret = bma254_setup_irq(data);
		if (ret) {
			data->pin_check_fail = true;
			pr_err("%s: could not setup irq\n", __func__);
			goto err_bma254_sensors_setup_irq;
		}
		mutex_init(&data->data_mutex);
#if defined(SMART_ALERT_REV_CHECK)
	}
#endif
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&bma254_early_suspend_handler);
#endif
	ret = sensors_register(bma_device, NULL, bma254_attrs,
		"accelerometer_sensor");
	if (ret < 0) {
		pr_info("%s: could not sensors_register\n", __func__);
		goto err_bma254_sensors_register;
	}

	probe_done = PROBE_SUCCESS;

	pr_info("%s: success.\n", __func__);
	return 0;

err_bma254_sensors_register:
#ifdef CONFIG_BMA254_SMART_ALERT
	mutex_destroy(&data->data_mutex);
	free_irq(bma254_data.IRQ, &bma254_data);
	gpio_free(data->pdata->p_out);
err_bma254_sensors_setup_irq:
	wake_lock_destroy(&data->reactive_wake_lock);
#endif
err_bma254_sensors:
	this_client = NULL;
	pr_err("%s: failed!\n", __func__);
	return ret;
}

static int bma254_suspend(struct i2c_client *client, pm_message_t mesg)
{
	alps_info("is called\n");

	if (atomic_read(&flgEna))
		bma254_activate(0, 0, atomic_read(&delay));

	return 0;
}

static int bma254_resume(struct i2c_client *client)
{
	alps_info("is called\n");

	if (atomic_read(&flgEna))
		bma254_activate(0, atomic_read(&flgEna), atomic_read(&delay));

	return 0;
}

static int __devexit bma254_remove(struct i2c_client *client)
{
#ifdef CONFIG_BMA254_SMART_ALERT
	struct bma254_data *bma254 = &bma254_data;
#endif
	pr_info("%s\n", __func__);
	bma254_activate(0, 0, atomic_read(&delay));
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&bma254_early_suspend_handler);
#endif
#ifdef CONFIG_BMA254_SMART_ALERT
	wake_lock_destroy(&bma254->reactive_wake_lock);
#endif
	this_client = NULL;
return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma254_early_suspend(struct early_suspend *handler)
{
	pr_info("%s\n", __func__);

	bma254_suspend(this_client, PMSG_SUSPEND);
}

static void bma254_early_resume(struct early_suspend *handler)
{
	pr_info("%s\n", __func__);

	bma254_resume(this_client);
}
#endif

static const struct i2c_device_id bma254_id[] = {
	{ "bma254", 0 },
	{ }
};

static struct i2c_driver bma254_driver = {
	.probe     = bma254_probe,
	.remove   = bma254_remove,
	.id_table  = bma254_id,
	.driver    = {
		.name	= "bma254",
	},
	.suspend = bma254_suspend,
	.resume = bma254_resume
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend bma254_early_suspend_handler = {
	.suspend = bma254_early_suspend,
	.resume  = bma254_early_resume,
};
#endif

static int __init bma254_init(void)
{
	probe_done = PROBE_FAIL;
	return i2c_add_driver(&bma254_driver);
}

static void __exit bma254_exit(void)
{
	i2c_del_driver(&bma254_driver);
}

module_init(bma254_init);
module_exit(bma254_exit);

MODULE_DESCRIPTION("Bosch BMA254 Accelerometer Sensor");
MODULE_AUTHOR("Samsung");
MODULE_LICENSE("GPL v2");
