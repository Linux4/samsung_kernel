/*!
 * @section LICENSE
 * (C) Copyright 2011~2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160_driver.c
 * @date     2014/06/24 9:00
 * @id       "c29f102"
 * @version  0.3
 *
 * @brief
 * The core code of BMI160 device driver
 *
 * @detail
 * This file implements the core code of BMI160 device driver,
 * which includes hardware related functions, input device register,
 * device attribute files, etc.
*/

#include "bmi160_driver.h"


/* bmi interrupt type enum */
enum BMI_SENSOR_INT_T {
	/* Interrupt enable0*/
	BMI_ANYMO_X_INT = 0,
	BMI_ANYMO_Y_INT,
	BMI_ANYMO_Z_INT,
	BMI_D_TAP_INT,
	BMI_S_TAP_INT,
	BMI_ORIENT_INT,
	BMI_FLAT_INT,
	/* Interrupt enable1*/
	BMI_HIGH_X_INT,
	BMI_HIGH_Y_INT,
	BMI_HIGH_Z_INT,
	BMI_LOW_INT,
	BMI_DRDY_INT,
	BMI_FFULL_INT,
	BMI_FWM_INT,
	/* Interrupt enable2 */
	BMI_NOMOTION_X_INT,
	BMI_NOMOTION_Y_INT,
	BMI_NOMOTION_Z_INT,
	INT_TYPE_MAX
};

/*bmi fifo sensor type combination*/
enum BMI_SENSOR_FIFO_COMBINATION {
	BMI_FIFO_A = 0,
	BMI_FIFO_G,
	BMI_FIFO_M,
	BMI_FIFO_G_A,
	BMI_FIFO_M_A,
	BMI_FIFO_M_G,
	BMI_FIFO_M_G_A,
	BMI_FIFO_COM_MAX
};

/*bmi fifo analyse return err status*/
enum BMI_FIFO_ANALYSE_RETURN_T {
	FIFO_OVER_READ_RETURN = -10,
	FIFO_SENSORTIME_RETURN = -9,
	FIFO_SKIP_OVER_LEN = -8,
	FIFO_M_G_A_OVER_LEN = -7,
	FIFO_M_G_OVER_LEN = -6,
	FIFO_M_A_OVER_LEN = -5,
	FIFO_G_A_OVER_LEN = -4,
	FIFO_M_OVER_LEN = -3,
	FIFO_G_OVER_LEN = -2,
	FIFO_A_OVER_LEN = -1
};

/*!bmi sensor generic power mode enum */
enum BMI_DEV_OP_MODE {
	SENSOR_PM_NORMAL = 0,
	SENSOR_PM_LP1,
	SENSOR_PM_SUSPEND,
	SENSOR_PM_LP2
};

/*! bmi acc sensor power mode enum */
enum BMI_ACC_PM_TYPE {
	BMI_ACC_PM_NORMAL = 0,
	BMI_ACC_PM_LP1,
	BMI_ACC_PM_SUSPEND,
	BMI_ACC_PM_LP2,
	BMI_ACC_PM_MAX
};

/*! bmi gyro sensor power mode enum */
enum BMI_GYRO_PM_TYPE {
	BMI_GYRO_PM_NORMAL = 0,
	BMI_GYRO_PM_FAST_START,
	BMI_GYRO_PM_SUSPEND,
	BMI_GYRO_PM_MAX
};

/*! bmi mag sensor power mode enum */
enum BMI_MAG_PM_TYPE {
	BMI_MAG_PM_NORMAL = 0,
	BMI_MAG_PM_LP1,
	BMI_MAG_PM_SUSPEND,
	BMI_MAG_PM_LP2,
	BMI_MAG_PM_MAX
};


/*! bmi sensor support type*/
enum BMI_SENSOR_TYPE {
	BMI_ACC_SENSOR,
	BMI_GYRO_SENSOR,
	BMI_MAG_SENSOR,
	BMI_SENSOR_TYPE_MAX
};

/*!bmi sensor generic power mode enum */
enum BMI_AXIS_TYPE {
	X_AXIS = 0,
	Y_AXIS,
	Z_AXIS,
	AXIS_MAX
};

/*!bmi sensor generic intterrupt enum */
enum BMI_INT_TYPE {
	BMI160_INT0 = 0,
	BMI160_INT1,
	BMI160_INT_MAX
};

/*! bmi sensor time resolution definition*/
enum BMI_SENSOR_TIME_RS_TYPE {
	TS_0_78_HZ = 1,/*0.78HZ*/
	TS_1_56_HZ,/*1.56HZ*/
	TS_3_125_HZ,/*3.125HZ*/
	TS_6_25_HZ,/*6.25HZ*/
	TS_12_5_HZ,/*12.5HZ*/
	TS_25_HZ,/*25HZ, odr=6*/
	TS_50_HZ,/*50HZ*/
	TS_100_HZ,/*100HZ*/
	TS_200_HZ,/*200HZ*/
	TS_400_HZ,/*400HZ*/
	TS_800_HZ,/*800HZ*/
	TS_1600_HZ,/*1600HZ*/
	TS_MAX_HZ
};

/*! bmi sensor interface mode */
enum BMI_SENSOR_IF_MODE_TYPE {
	/*primary interface:autoconfig/secondary interface off*/
	P_AUTO_S_OFF = 0,
	/*primary interface:I2C/secondary interface:OIS*/
	P_I2C_S_OIS,
	/*primary interface:autoconfig/secondary interface:Magnetometer*/
	P_AUTO_S_MAG,
	/*interface mode reseved*/
	IF_MODE_RESEVED

};

unsigned int reg_op_addr;

static const int bmi_pmu_cmd_acc_arr[BMI_ACC_PM_MAX] = {
	/*!bmi pmu for acc normal, low power1,
	 * suspend, low power2 mode command */
	CMD_PMU_ACC_NORMAL,
	CMD_PMU_ACC_LP1,
	CMD_PMU_ACC_SUSPEND,
	CMD_PMU_ACC_LP2
};

static const int bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_MAX] = {
	/*!bmi pmu for gyro normal, fast startup,
	 * suspend mode command */
	CMD_PMU_GYRO_NORMAL,
	CMD_PMU_GYRO_FASTSTART,
	CMD_PMU_GYRO_SUSPEND
};

static const int bmi_pmu_cmd_mag_arr[BMI_MAG_PM_MAX] = {
	/*!bmi pmu for mag normal, low power1,
	 * suspend, low power2 mode command */
	CMD_PMU_MAG_NORMAL,
	CMD_PMU_MAG_LP1,
	CMD_PMU_MAG_SUSPEND,
	CMD_PMU_MAG_LP2
};

static const char *bmi_axis_name[AXIS_MAX] = {"x", "y", "z"};

static const int bmi_interrupt_type[] = {
	/*!bmi interrupt type */
	/* Interrupt enable0 */
	BMI160_ANYMO_X_EN,
	BMI160_ANYMO_Y_EN,
	BMI160_ANYMO_Z_EN,
	BMI160_D_TAP_EN,
	BMI160_S_TAP_EN,
	BMI160_ORIENT_EN,
	BMI160_FLAT_EN,
	/* Interrupt enable1*/
	BMI160_HIGH_X_EN,
	BMI160_HIGH_Y_EN,
	BMI160_HIGH_Z_EN,
	BMI160_LOW_EN,
	BMI160_DRDY_EN,
	BMI160_FFULL_EN,
	BMI160_FWM_EN,
	/* Interrupt enable2 */
	BMI160_NOMOTION_X_EN,
	BMI160_NOMOTION_Y_EN,
	BMI160_NOMOTION_Z_EN
};

/*! bmi sensor time depend on ODR*/
struct bmi_sensor_time_odr_tbl {
	u32 ts_duration_lsb;
	u32 ts_duration_us;
};

struct bmi160_axis_data_t {
	s16 x;
	s16 y;
	s16 z;
};

struct bmi160_type_mapping_type {

	/*! bmi16x sensor chip id */
	uint16_t chip_id;

	/*! bmi16x chip revision code */
	uint16_t revision_id;

	/*! bma2x2 sensor name */
	const char *sensor_name;
};

/*! sensor support type map */
static const struct bmi160_type_mapping_type sensor_type_map[] = {

	{SENSOR_CHIP_ID_BMI, SENSOR_CHIP_REV_ID_BMI, "BMI160/162AB"},
	{SENSOR_CHIP_ID_BMI_D1, SENSOR_CHIP_REV_ID_BMI, "BMI160/162AB"},

};

/*!bmi160 sensor time depends on ODR */
static const struct bmi_sensor_time_odr_tbl
		sensortime_duration_tbl[TS_MAX_HZ] = {
	{0x010000, 2560000},/*2560ms, 0.39hz, odr=resver*/
	{0x008000, 1280000},/*1280ms, 0.78hz, odr_acc=1*/
	{0x004000, 640000},/*640ms, 1.56hz, odr_acc=2*/
	{0x002000, 320000},/*320ms, 3.125hz, odr_acc=3*/
	{0x001000, 160000},/*160ms, 6.25hz, odr_acc=4*/
	{0x000800, 80000},/*80ms, 12.5hz*/
	{0x000400, 40000},/*40ms, 25hz, odr_acc = odr_gyro =6*/
	{0x000200, 20000},/*20ms, 50hz, odr = 7*/
	{0x000100, 10000},/*10ms, 100hz, odr=8*/
	{0x000080, 5000},/*5ms, 200hz, odr=9*/
	{0x000040, 2500},/*2.5ms, 400hz, odr=10*/
	{0x000020, 1250},/*1.25ms, 800hz, odr=11*/
	{0x000010, 625},/*0.625ms, 1600hz, odr=12*/

};

static void bmi_dump_reg(struct bmi_client_data *client_data)
{
	#define REG_MAX 0x7f
	int i;
	u8 dbg_buf[REG_MAX];
	u8 dbg_buf_str[REG_MAX * 3 + 1] = "";

	for (i = 0; i < BYTES_PER_LINE; i++) {
		dbg_buf[i] = i;
		sprintf(dbg_buf_str + i * 3, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_notice(client_data->dev, "%s\n", dbg_buf_str);

	client_data->device.bus_read(client_data->device.dev_addr,
			BMI_REG_NAME(USER_CHIP_ID), dbg_buf, REG_MAX);
	for (i = 0; i < REG_MAX; i++) {
		sprintf(dbg_buf_str + i * 3, "%02x%c", dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_notice(client_data->dev, "%s\n", dbg_buf_str);
}

/*!
* BMG160 sensor remapping function
* need to give some parameter in BSP files first.
*/
static const struct bosch_sensor_axis_remap
	bst_axis_remap_tab_dft[MAX_AXIS_REMAP_TAB_SZ] = {
	/* src_x src_y src_z  sign_x  sign_y  sign_z */
	{  0,	 1,    2,	  1,	  1,	  1 }, /* P0 */
	{  1,	 0,    2,	  1,	 -1,	  1 }, /* P1 */
	{  0,	 1,    2,	 -1,	 -1,	  1 }, /* P2 */
	{  1,	 0,    2,	 -1,	  1,	  1 }, /* P3 */

	{  0,	 1,    2,	 -1,	  1,	 -1 }, /* P4 */
	{  1,	 0,    2,	 -1,	 -1,	 -1 }, /* P5 */
	{  0,	 1,    2,	  1,	 -1,	 -1 }, /* P6 */
	{  1,	 0,    2,	  1,	  1,	 -1 }, /* P7 */
};

static void bst_remap_sensor_data(struct bosch_sensor_data *data,
			const struct bosch_sensor_axis_remap *remap)
{
	struct bosch_sensor_data tmp;

	tmp.x = data->v[remap->src_x] * remap->sign_x;
	tmp.y = data->v[remap->src_y] * remap->sign_y;
	tmp.z = data->v[remap->src_z] * remap->sign_z;

	memcpy(data, &tmp, sizeof(*data));
}

static void bst_remap_sensor_data_dft_tab(struct bosch_sensor_data *data,
			int place)
{
/* sensor with place 0 needs not to be remapped */
	if ((place <= 0) || (place >= MAX_AXIS_REMAP_TAB_SZ))
		return;
	bst_remap_sensor_data(data, &bst_axis_remap_tab_dft[place]);
}

static void bmi_remap_sensor_data(struct bmi160_axis_data_t *val,
		struct bmi_client_data *client_data)
{
	struct bosch_sensor_data bsd;

	if ((NULL == client_data->bst_pd) ||
			(BOSCH_SENSOR_PLACE_UNKNOWN
			 == client_data->bst_pd->place))
		return;

	bsd.x = val->x;
	bsd.y = val->y;
	bsd.z = val->z;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

	val->x = bsd.x;
	val->y = bsd.y;
	val->z = bsd.z;

}

static void bmi_remap_data_acc(struct bmi_client_data *client_data,
				struct bmi160acc_t *acc_frame)
{
	struct bosch_sensor_data bsd;

	if ((NULL == client_data->bst_pd) ||
			(BOSCH_SENSOR_PLACE_UNKNOWN
			 == client_data->bst_pd->place))
		return;

	bsd.x = acc_frame->x;
	bsd.y = acc_frame->y;
	bsd.z = acc_frame->z;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

	acc_frame->x = bsd.x;
	acc_frame->y = bsd.y;
	acc_frame->z = bsd.z;


}

static void bmi_remap_data_gyro(struct bmi_client_data *client_data,
					struct bmi160gyro_t *gyro_frame)
{
	struct bosch_sensor_data bsd;

	if ((NULL == client_data->bst_pd) ||
			(BOSCH_SENSOR_PLACE_UNKNOWN
			 == client_data->bst_pd->place))
		return;

	bsd.x = gyro_frame->x;
	bsd.y = gyro_frame->y;
	bsd.z = gyro_frame->z;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

	gyro_frame->x = bsd.x;
	gyro_frame->y = bsd.y;
	gyro_frame->z = bsd.z;


}



static int bmi_input_init(struct bmi_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, BMI_VALUE_MIN, BMI_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, BMI_VALUE_MIN, BMI_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, BMI_VALUE_MIN, BMI_VALUE_MAX, 0, 0);
	input_set_drvdata(dev, client_data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	client_data->input = dev;

	return err;
}


static void bmi_input_destroy(struct bmi_client_data *client_data)
{
	struct input_dev *dev = client_data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int bmi_check_chip_id(struct bmi_client_data *client_data)
{
	int8_t err = 0;
	int8_t i = 0;
	uint8_t chip_id = 0;
	uint8_t read_count = 0;
	u8 bmi_sensor_cnt = sizeof(sensor_type_map)
				/ sizeof(struct bmi160_type_mapping_type);
	/* read and check chip id */
	while (read_count++ < CHECK_CHIP_ID_TIME_MAX) {
		if (client_data->device.bus_read(client_data->device.dev_addr,
				BMI_REG_NAME(USER_CHIP_ID), &chip_id, 1) < 0) {

			dev_err(client_data->dev,
					"Bosch Sensortec Device not found"
						"read chip_id:%d\n", chip_id);
			continue;
		} else {
			for (i = 0; i < bmi_sensor_cnt; i++) {
				if (sensor_type_map[i].chip_id == chip_id) {
					client_data->chip_id = chip_id;
					dev_notice(client_data->dev,
					"Bosch Sensortec Device detected, "
			"HW IC name: %s\n", sensor_type_map[i].sensor_name);
					break;
				}
			}
			if (i < bmi_sensor_cnt)
				break;
			else {
				if (read_count == CHECK_CHIP_ID_TIME_MAX) {
					dev_err(client_data->dev,
				"Failed!Bosch Sensortec Device not found"
					" mismatch chip_id:%d\n", chip_id);
					err = -ENODEV;
					return err;
				}
			}
			mdelay(1);
		}
	}
	return err;

}

static int bmi_pmu_set_suspend(struct bmi_client_data *client_data)
{
	int err = 0;
	if (client_data == NULL)
		return -1;
	else {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[SENSOR_PM_SUSPEND]);
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[SENSOR_PM_SUSPEND]);
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_mag_arr[SENSOR_PM_SUSPEND]);
		client_data->pw.acc_pm = BMI_ACC_PM_SUSPEND;
		client_data->pw.gyro_pm = BMI_GYRO_PM_SUSPEND;
		client_data->pw.mag_pm = BMI_MAG_PM_SUSPEND;
	}

	return err;
}

static int bmi_get_err_status(struct bmi_client_data *client_data)
{
	int err = 0;

	err = BMI_CALL_API(get_error_status)(&client_data->err_st.fatal_err,
		&client_data->err_st.err_code, &client_data->err_st.i2c_fail,
	&client_data->err_st.drop_cmd, &client_data->err_st.mag_drdy_err);
	return err;
}

static void bmi_work_func(struct work_struct *work)
{
	struct bmi_client_data *client_data =
		container_of((struct delayed_work *)work,
			struct bmi_client_data, work);
	unsigned long delay =
		msecs_to_jiffies(atomic_read(&client_data->delay));

	/* TODO */
	input_sync(client_data->input);

	schedule_delayed_work(&client_data->work, delay);
}

static ssize_t bmi160_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	return sprintf(buf, "0x%x\n", client_data->chip_id);
}

static ssize_t bmi160_chip_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0;

	err = BMI_CALL_API(get_major_revision_id)(&data);

	if (err)
		return err;
	else
		return sprintf(buf, "0x%x\n", data);
}

static ssize_t bmi160_err_st_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	unsigned char err_st_all = 0xff;
	err = bmi_get_err_status(client_data);
	if (err)
		return err;
	else {
		err_st_all = client_data->err_st.err_st_all;
		return sprintf(buf, "0x%x\n", err_st_all);
	}
}

static ssize_t bmi160_sensor_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	u32 sensor_time;
	err = BMI_CALL_API(get_sensor_time)(&sensor_time);
	if (err)
		return err;
	else
		return sprintf(buf, "0x%x\n", sensor_time);
}

static ssize_t bmi160_fifo_full_stop_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 data = 0;
	BMI_CALL_API(get_fifo_stop_on_full)(&data);

	return sprintf(buf, "%d\n", data);

}

static ssize_t bmi160_fifo_full_stop_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long data;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	err = BMI_CALL_API(set_fifo_stop_on_full)(data);
	if (err)
		return err;

	return count;
}

static ssize_t bmi160_fifo_flush_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long enable;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable)
		err = BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);

	if (err)
		dev_err(client_data->dev, "fifo flush failed!\n");

	return count;

}


static ssize_t bmi160_fifo_bytecount_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned int fifo_bytecount = 0;

	BMI_CALL_API(fifo_length)(&fifo_bytecount);
	err = sprintf(buf, "%u\n", fifo_bytecount);
	return err;
}

static ssize_t bmi160_fifo_bytecount_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	client_data->fifo_bytecount = (unsigned int) data;

	return count;
}

static ssize_t bmi160_fifo_data_sel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char fifo_acc_en, fifo_gyro_en, fifo_mag_en;
	unsigned char fifo_datasel;

	err += BMI_CALL_API(get_fifo_acc_en)(&fifo_acc_en);
	err += BMI_CALL_API(get_fifo_gyro_en)(&fifo_gyro_en);
	err += BMI_CALL_API(get_fifo_mag_en)(&fifo_mag_en);
	if (err)
		return err;
	fifo_datasel = (fifo_acc_en << BMI_ACC_SENSOR) |
			(fifo_gyro_en << BMI_GYRO_SENSOR) |
				(fifo_mag_en << BMI_MAG_SENSOR);

	return sprintf(buf, "%d\n", fifo_datasel);
}

/* write any value to clear all the fifo data. */
static ssize_t bmi160_fifo_data_sel_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;
	unsigned char fifo_datasel;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* data format: aimed 0b0000 0x(m)x(g)x(a), x:1 enable, 0:disable*/
	if (data > 7)
		return -EINVAL;

	fifo_datasel = (unsigned char)data;
	err += BMI_CALL_API(set_fifo_acc_en)
			((fifo_datasel & (1 << BMI_ACC_SENSOR)) ? 1 :  0);
	err += BMI_CALL_API(set_fifo_gyro_en)
			(fifo_datasel & (1 << BMI_GYRO_SENSOR) ? 1 : 0);
	err += BMI_CALL_API(set_fifo_mag_en)
			((fifo_datasel & (1 << BMI_MAG_SENSOR)) ? 1 : 0);

	if (err)
		return -EIO;
	else {
		dev_notice(client_data->dev, "FIFO A_en:%d, G_en:%d, M_en:%d\n",
			(fifo_datasel & (1 << BMI_ACC_SENSOR)) ? 1 :  0,
			(fifo_datasel & (1 << BMI_GYRO_SENSOR) ? 1 : 0),
			((fifo_datasel & (1 << BMI_MAG_SENSOR)) ? 1 : 0));
		client_data->fifo_data_sel = fifo_datasel;
	}

	return count;
}

static int bmi_fifo_analysis_handle(struct bmi_client_data *client_data,
				u8 *fifo_data, u16 fifo_length, char *buf)
{
	u8 frame_head;/* every frame head*/
	int len;
	struct bmi160_axis_data_t bmi160_udata;

	u8 acc_frm_cnt = 0;/*0~146*/
	u8 gyro_frm_cnt = 0;

	u32 fifo_time = 0;
	u16 fifo_index = 0;/* fifo data buff index*/
	s8 last_return_st = 0;
	int err = 0;
	struct fifo_sensor_time_t fifo_all_ts;

	struct bmi160acc_t acc_frame_arr[FIFO_FRAME_CNT];
	struct bmi160gyro_t gyro_frame_arr[FIFO_FRAME_CNT];

	struct odr_t odr;
	memset(&odr, 0, sizeof(odr));
	memset(&fifo_all_ts, 0, sizeof(fifo_all_ts));
	memset(&bmi160_udata, 0, sizeof(bmi160_udata));



	/* no fifo select for bmi sensor*/
	if (!client_data->fifo_data_sel) {
		dev_err(client_data->dev,
				"No select any sensor FIFO for BMI16x\n");
		return -1;
	}
	/*driver need read acc_odr/gyro_odr/mag_odr*/
	if ((client_data->fifo_data_sel) & (1 << BMI_ACC_SENSOR))
		odr.acc_odr = client_data->odr.acc_odr;
	if ((client_data->fifo_data_sel) & (1 << BMI_GYRO_SENSOR))
		odr.gyro_odr = client_data->odr.gyro_odr;
	if ((client_data->fifo_data_sel) & (1 << BMI_MAG_SENSOR))
		odr.mag_odr = client_data->odr.mag_odr;

	for (fifo_index = 0; fifo_index < fifo_length;) {
		frame_head = fifo_data[fifo_index];
		switch (frame_head) {
			/*skip frame 0x40 22 0x84*/
		case FIFO_HEAD_SKIP_FRAME:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 1 > fifo_length) {
				last_return_st = FIFO_SKIP_OVER_LEN;
				break;
			}
			fifo_index = fifo_index + 1;
			break;
		}
		/*fifo data frame, 7 type combination*/
		/*3 type sensors, 3 fifo type for every frame*/
		/*M & G & A*/
		case FIFO_HEAD_M_G_A:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 20 > fifo_length) {
				last_return_st = FIFO_M_G_A_OVER_LEN;
				break;
			}
			/*TODO*/
			fifo_index = fifo_index + 20;
			break;
		}
		/*3 or 2 type sensors, 2 fifo type for every frame*/
		/*G & A */
		case FIFO_HEAD_G_A:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 12 > fifo_length) {
				last_return_st = FIFO_G_A_OVER_LEN;
				break;
			}

			gyro_frame_arr[gyro_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			gyro_frame_arr[gyro_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			gyro_frame_arr[gyro_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];

			acc_frame_arr[acc_frm_cnt].x =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];
			acc_frame_arr[acc_frm_cnt].y =
				fifo_data[fifo_index + 9] << 8 |
					fifo_data[fifo_index + 8];
			acc_frame_arr[acc_frm_cnt].z =
				fifo_data[fifo_index + 11] << 8 |
					fifo_data[fifo_index + 10];
			/* Report FIFO to sensord */
			/* FIFO data output format: head_x  x y z*/
			bmi_remap_data_gyro(client_data,
				&gyro_frame_arr[gyro_frm_cnt]);
			bmi_remap_data_acc(client_data,
				&acc_frame_arr[acc_frm_cnt]);
			len = sprintf(buf, "%d %d %d %d %d %d %d %d ",
				GYRO_FIFO_HEAD,
				gyro_frame_arr[gyro_frm_cnt].x,
				gyro_frame_arr[gyro_frm_cnt].y,
				gyro_frame_arr[gyro_frm_cnt].z,
				ACC_FIFO_HEAD,
				acc_frame_arr[acc_frm_cnt].x,
				acc_frame_arr[acc_frm_cnt].y,
				acc_frame_arr[acc_frm_cnt].z);
			buf += len;
			err += len;

			gyro_frm_cnt++;
			acc_frm_cnt++;

			/*fifo AG, data frame index + 12(6+6)*/
			fifo_index = fifo_index + 12;

			break;
		}
		/* M & A */
		case FIFO_HEAD_M_A:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 14 > fifo_length) {
				last_return_st = FIFO_M_A_OVER_LEN;
				break;
			}
		/*TODO*/
			/*fifo AM data frame index + 14(8+6)*/
			fifo_index = fifo_index + 14;
			break;
		}
		/*M & G*/
		case FIFO_HEAD_M_G:
		{
			/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 14 > fifo_length) {
				last_return_st = FIFO_M_G_OVER_LEN;
				break;
			}

		/*TODO*/
			/*fifo GM data frame index + 14(8+6)*/
			fifo_index = fifo_index + 14;
			break;
		}
		/*3, 2 or 1 type sensor, signle fifo frame*/
		/*Accel*/
		case FIFO_HEAD_A:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 6 > fifo_length) {
				last_return_st = FIFO_A_OVER_LEN;
				break;
			}
			acc_frame_arr[acc_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			acc_frame_arr[acc_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			acc_frame_arr[acc_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];

			/* Report FIFO to sensord */
			/* FIFO data output format: head_x  x y z*/
			bmi_remap_data_acc(client_data,
				&acc_frame_arr[acc_frm_cnt]);
			len = sprintf(buf, "%d %d %d %d ",
				ACC_FIFO_HEAD,
				acc_frame_arr[acc_frm_cnt].x,
				acc_frame_arr[acc_frm_cnt].y,
				acc_frame_arr[acc_frm_cnt].z);
			buf += len;
			err += len;

			acc_frm_cnt++;
			/*fifo A data frame index + 6*/
			fifo_index = fifo_index + 6;

			break;
		}
		/*Gyro */
		case FIFO_HEAD_G:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 6 > fifo_length) {
				last_return_st = FIFO_G_OVER_LEN;
				break;
			}
			gyro_frame_arr[gyro_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			gyro_frame_arr[gyro_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			gyro_frame_arr[gyro_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			/* Report FIFO to sensord */
			/* FIFO data output format: head_x  x y z*/
			bmi_remap_data_gyro(client_data,
				&gyro_frame_arr[gyro_frm_cnt]);
			len = sprintf(buf, "%d %d %d %d ",
				GYRO_FIFO_HEAD,
				gyro_frame_arr[gyro_frm_cnt].x,
				gyro_frame_arr[gyro_frm_cnt].y,
				gyro_frame_arr[gyro_frm_cnt].z);
			buf += len;
			err += len;

			gyro_frm_cnt++;
			/*fifo G data frame index + 6*/
			fifo_index = fifo_index + 6;

			break;
		}
		/*Mag*/
		case FIFO_HEAD_M:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 8 > fifo_length) {
				last_return_st = FIFO_M_OVER_LEN;
				break;
			}
			/*TODO*/
			/*fifo M data frame index + 8*/
			fifo_index = fifo_index + 8;
			break;
		}
		/* sensor time frame*/
		case FIFO_HEAD_SENSOR_TIME:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 3 > fifo_length) {
				last_return_st = FIFO_SENSORTIME_RETURN;
				break;
			}
			fifo_time = fifo_data[fifo_index + 2] << 16 |
					fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];

			/* Report FIFO to sensord */
			/* FIFO data output format:
			* head_ts  sensor_time*/
			len = sprintf(buf, "%d %d ",
				FIFO_TIME_HEAD, fifo_time);
			buf += len;
			err += len;
			/*fifo sensor time frame index + 3*/
			fifo_index = fifo_index + 3;

			break;
		}
		/*sensor FIFO over read*/
		case FIFO_HEAD_OVER_READ_LSB:
		{
			/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 1 > fifo_length) {
				last_return_st = FIFO_OVER_READ_RETURN;
				break;
			}
			if (fifo_data[fifo_index] ==
					FIFO_HEAD_OVER_READ_MSB) {
				/*fifo over read frame index + 1*/
				fifo_index = fifo_index + 1;
				break;
			} else {
				last_return_st = FIFO_OVER_READ_RETURN;
				break;
			}
		}

		default:
			last_return_st = 1;
			break;
		}
		if (last_return_st)
			break;

	}

	return err;


}

static ssize_t bmi160_fifo_data_out_frame_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	unsigned char fifo_data[1024] = {0};

	if (client_data->pw.acc_pm == 2 && client_data->pw.gyro_pm == 2
					&& client_data->pw.mag_pm == 2)
		return sprintf(buf, "pw_acc: %d, pw_gyro: %d\n",
			client_data->pw.acc_pm, client_data->pw.gyro_pm);
	if (!client_data->fifo_data_sel)
		return sprintf(buf,
			"no selsect sensor fifo, fifo_data_sel:%d\n",
						client_data->fifo_data_sel);

	if (client_data->fifo_bytecount == 0)
		return -EINVAL;

	/* need give attention for the time of burst read*/
	if (!err) {
		err = bmi_burst_read_wrapper(client_data->device.dev_addr,
			BMI160_USER_FIFO_DATA__REG, fifo_data,
						client_data->fifo_bytecount);
	} else
		dev_err(client_data->dev, "read fifo leght err");

	if (err)
		dev_err(client_data->dev, "brust read fifo err\n");
	else
		err = bmi_fifo_analysis_handle(client_data, fifo_data,
					client_data->fifo_bytecount, buf);

	return err;
}

static ssize_t bmi160_fifo_watermark_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0xff;

	err = BMI_CALL_API(get_fifo_watermark)(&data);

	if (err)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_fifo_watermark_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long data;
	unsigned char fifo_watermark;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	fifo_watermark = (unsigned char)data;
	err = BMI_CALL_API(set_fifo_watermark)(fifo_watermark);
	if (err)
		return -EIO;

	return count;
}


static ssize_t bmi160_fifo_header_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0xff;

	err = BMI_CALL_API(get_fifo_header_en)(&data);

	if (err)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_fifo_header_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;
	unsigned char fifo_header_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	if (data > 1)
		return -ENOENT;

	fifo_header_en = (unsigned char)data;
	err = BMI_CALL_API(set_fifo_header_en)(fifo_header_en);
	if (err)
		return -EIO;

	client_data->fifo_head_en = fifo_header_en;

	return count;
}

static ssize_t bmi160_fifo_time_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0;

	err = BMI_CALL_API(get_fifo_time_en)(&data);

	if (!err)
		err = sprintf(buf, "%d\n", data);

	return err;
}

static ssize_t bmi160_fifo_time_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long data;
	unsigned char fifo_ts_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	fifo_ts_en = (unsigned char)data;

	err = BMI_CALL_API(set_fifo_time_en)(fifo_ts_en);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_fifo_int_tag_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char fifo_tag_int1 = 0;
	unsigned char fifo_tag_int2 = 0;
	unsigned char fifo_tag_int;

	err += BMI_CALL_API(get_fifo_tag_int1_en)(&fifo_tag_int1);
	err += BMI_CALL_API(get_fifo_tag_int1_en)(&fifo_tag_int2);

	fifo_tag_int = (fifo_tag_int1 << BMI160_INT0) |
			(fifo_tag_int2 << BMI160_INT1);

	if (!err)
		err = sprintf(buf, "%d\n", fifo_tag_int);

	return err;
}

static ssize_t bmi160_fifo_int_tag_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;
	unsigned char fifo_tag_int_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	if (data > 3)
		return -EINVAL;

	fifo_tag_int_en = (unsigned char)data;

	err += BMI_CALL_API(set_fifo_tag_int1_en)
			((fifo_tag_int_en & (1 << BMI160_INT0)) ? 1 :  0);
	err += BMI_CALL_API(set_fifo_tag_int1_en)
			((fifo_tag_int_en & (1 << BMI160_INT1)) ? 1 :  0);

	if (err) {
		dev_err(client_data->dev, "fifo int tag en err:%d\n", err);
		return -EIO;
	}
	client_data->fifo_int_tag_en = fifo_tag_int_en;

	return count;
}


static ssize_t bmi160_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	s16 temp = 0xff;

	err = BMI_CALL_API(get_temperature)(&temp);

	if (!err)
		err = sprintf(buf, "0x%x\n", temp);

	return err;
}

static ssize_t bmi160_place_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

	if (NULL != client_data->bst_pd)
		place = client_data->bst_pd->place;

	return sprintf(buf, "%d\n", place);
}

static ssize_t bmi160_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", atomic_read(&client_data->delay));

}

static ssize_t bmi160_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	if (data == 0) {
		err = -EINVAL;
		return err;
	}

	if (data < BMI_DELAY_MIN)
		data = BMI_DELAY_MIN;

	atomic_set(&client_data->delay, data);

	return count;
}

static ssize_t bmi160_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err;

	mutex_lock(&client_data->mutex_enable);
	err = sprintf(buf, "%d\n", client_data->wkqueue_en);
	mutex_unlock(&client_data->mutex_enable);
	return err;
}

static ssize_t bmi160_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	data = data ? 1 : 0;
	mutex_lock(&client_data->mutex_enable);
	if (data != client_data->wkqueue_en) {
		if (data) {
			if (regulator_enable(client_data->avdd)) {
				dev_err(dev, "bosch sensor avdd power supply enable failed\n");
				goto out;
			}
			schedule_delayed_work(
					&client_data->work,
					msecs_to_jiffies(atomic_read(
							&client_data->delay)));
		} else {
			cancel_delayed_work_sync(&client_data->work);
			regulator_disable(client_data->avdd);
		}

		client_data->wkqueue_en = data;
	}
	mutex_unlock(&client_data->mutex_enable);

	return count;
out:
	return 0;
}

/* accel sensor part */
static ssize_t bmi160_anymot_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data;

	err = BMI_CALL_API(get_int_anymotion_duration)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_anymot_duration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_int_anymotion_duration)((unsigned char)data);
	if (err < 0)
		return -EIO;

	return count;
}

static ssize_t bmi160_anymot_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_int_anymotion_threshold)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_anymot_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_int_anymotion_threshold)((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_acc_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char range;
	err = BMI_CALL_API(get_acc_range)(&range);
	if (err)
		return err;
	return sprintf(buf, "%d\n", range);
}

static ssize_t bmi160_acc_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;

	err = BMI_CALL_API(set_acc_range)(range);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_acc_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char acc_odr;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = BMI_CALL_API(get_acc_bandwidth)(&acc_odr);
	if (err)
		return err;

	client_data->odr.acc_odr = acc_odr;
	return sprintf(buf, "%d\n", acc_odr);
}

static ssize_t bmi160_acc_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long acc_odr;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &acc_odr);
	if (err)
		return err;

	if (acc_odr < 1 || acc_odr > 12)
		return -EIO;

	if (acc_odr < 5)
		err = BMI_CALL_API(set_accel_undersampling_parameter)(1);
	else
		err = BMI_CALL_API(set_accel_undersampling_parameter)(0);

	if (err)
		return err;

	err = BMI_CALL_API(set_acc_bandwidth)(acc_odr);
	if (err)
		return -EIO;
	client_data->odr.acc_odr = acc_odr;
	return count;
}

static ssize_t bmi160_acc_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", client_data->pw.acc_pm);
}

static ssize_t bmi160_acc_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	unsigned long op_mode;
	int err;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	mutex_lock(&client_data->mutex_op_mode);

	if (op_mode < BMI_ACC_PM_MAX) {
		switch (op_mode) {
		case BMI_ACC_PM_NORMAL:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_NORMAL]);
			client_data->pw.acc_pm = BMI_ACC_PM_NORMAL;
			mdelay(3);
			break;
		case BMI_ACC_PM_LP1:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_LP1]);
			client_data->pw.acc_pm = BMI_ACC_PM_LP1;
			mdelay(3);
			break;
		case BMI_ACC_PM_SUSPEND:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_SUSPEND]);
			client_data->pw.acc_pm = BMI_ACC_PM_SUSPEND;
			mdelay(3);
			break;
		case BMI_ACC_PM_LP2:
			err = BMI_CALL_API(set_command_register)
					(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_LP2]);
			client_data->pw.acc_pm = BMI_ACC_PM_LP2;
			mdelay(3);
			break;
		default:
			mutex_unlock(&client_data->mutex_op_mode);
			return -EINVAL;
		}
	} else {
		mutex_unlock(&client_data->mutex_op_mode);
		return -EINVAL;
	}

	mutex_unlock(&client_data->mutex_op_mode);

	if (err)
		return err;
	else
		return count;

}

static ssize_t bmi160_acc_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	struct bmi160acc_t data;
	struct bmi160_axis_data_t bmi160_udata;
	int err;

	err = BMI_CALL_API(read_acc_xyz)(&data);
	if (err < 0)
		return err;

	bmi160_udata.x = data.x;
	bmi160_udata.y = data.y;
	bmi160_udata.z = data.z;

	bmi_remap_sensor_data(&bmi160_udata, client_data);
	return sprintf(buf, "%hd %hd %hd\n",
			bmi160_udata.x, bmi160_udata.y, bmi160_udata.z);
}

static ssize_t bmi160_acc_fast_calibration_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_acc_x)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_acc_fast_calibration_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = BMI_CALL_API(set_accel_foc_trigger)(X_AXIS, data);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_acc_fast_calibration_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_acc_y)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_acc_fast_calibration_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = BMI_CALL_API(set_accel_foc_trigger)(Y_AXIS, data);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_acc_fast_calibration_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_acc_z)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_acc_fast_calibration_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = BMI_CALL_API(set_accel_foc_trigger)(Z_AXIS, data);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_acc_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_acc_off_comp_xaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}


static ssize_t bmi160_acc_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_offset_compensation_xaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_acc_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_acc_off_comp_yaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_acc_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_offset_compensation_yaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_acc_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_acc_off_comp_zaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_acc_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_offset_compensation_zaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	u8 raw_data[15] = {0};
	u32 sensor_time = 0;

	int err;
	memset(raw_data, 0, sizeof(raw_data));

	err = client_data->device.bus_read(client_data->device.dev_addr,
			BMI160_USER_DATA_8_GYR_X_LSB__REG, raw_data, 15);
	if (err)
		return err;

	udelay(10);
	sensor_time = (u32)(raw_data[14] << 16 | raw_data[13] << 8
						| raw_data[12]);

	return sprintf(buf, "%d %d %d %d %d %d %d",
					(s16)(raw_data[1] << 8 | raw_data[0]),
				(s16)(raw_data[3] << 8 | raw_data[2]),
				(s16)(raw_data[5] << 8 | raw_data[4]),
				(s16)(raw_data[7] << 8 | raw_data[6]),
				(s16)(raw_data[9] << 8 | raw_data[8]),
				(s16)(raw_data[11] << 8 | raw_data[10]),
				sensor_time);

}


static ssize_t bmi160_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	return sprintf(buf, "0x%x\n",
				atomic_read(&client_data->selftest_result));
}

/*!
 * @brief store selftest result which make up of acc and gyro
 * format: 0b 0000 xxxx  x:1 failed, 0 success
 * bit3:     gyro_self
 * bit2..0: acc_self z y x
 */
static ssize_t bmi160_selftest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	int i = 0;
	u8 acc_selftest = 0;
	u8 gyro_selftest = 0;
	u8 bmi_selftest = 0;
	s16 axis_p_value, axis_n_value;
	u16 diff_axis[3] = {0xff, 0xff, 0xff};

	/*soft reset*/
	err = BMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	mdelay(10);
	/* set to 8G range*/
	err += BMI_CALL_API(set_acc_range)(BMI160_ACCEL_RANGE_8G);
	/* set to self amp high */
	err += BMI_CALL_API(set_acc_selftest_amp)(BMI_SELFTEST_AMP_HIGH);

	for (i = X_AXIS; i < AXIS_MAX; i++) {
		axis_n_value = 0;
		axis_p_value = 0;
		/* set every selftest axis */
		/*set_acc_selftest_axis(param),param x:1, y:2, z:3
		* but X_AXIS:0, Y_AXIS:1, Z_AXIS:2
		* so we need to +1*/
		err += BMI_CALL_API(set_acc_selftest_axis)(i + 1);
		switch (i) {
		case X_AXIS:
			/* set negative sign */
			err += BMI_CALL_API(set_acc_selftest_sign)(0);
			mdelay(50);
			err += BMI_CALL_API(read_acc_x)(&axis_n_value);
			/* set postive sign */
			err += BMI_CALL_API(set_acc_selftest_sign)(1);
			mdelay(50);
			err += BMI_CALL_API(read_acc_x)(&axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;

		case Y_AXIS:
			/* set negative sign */
			err += BMI_CALL_API(set_acc_selftest_sign)(0);
			mdelay(50);
			err += BMI_CALL_API(read_acc_y)(&axis_n_value);
			/* set postive sign */
			err += BMI_CALL_API(set_acc_selftest_sign)(1);
			mdelay(50);
			err += BMI_CALL_API(read_acc_y)(&axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;

		case Z_AXIS:
			/* set negative sign */
			err += BMI_CALL_API(set_acc_selftest_sign)(0);
			mdelay(50);
			err += BMI_CALL_API(read_acc_z)(&axis_n_value);
			/* set postive sign */
			err += BMI_CALL_API(set_acc_selftest_sign)(1);

			/* also start gyro self test */
			err += BMI_CALL_API(set_gyr_self_test_start)(1);
			mdelay(50);
			err += BMI_CALL_API(get_gyr_self_test)(&gyro_selftest);

			err += BMI_CALL_API(read_acc_z)(&axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;
		default:
			err += -EINVAL;
			break;
		}
		if (err) {
			dev_err(client_data->dev,
				"Failed selftest axis:%s, p_val=%d, n_val=%d\n",
				bmi_axis_name[i], axis_p_value, axis_n_value);
			return -EINVAL;
		}

		/*400mg for acc z axis*/
		if (Z_AXIS == i) {
			if (diff_axis[i] < 1639) {
				acc_selftest |= 1 << i;
				dev_err(client_data->dev,
					"Over selftest minimum for "
					"axis:%s,diff=%d,p_val=%d, n_val=%d\n",
					bmi_axis_name[i], diff_axis[i],
						axis_p_value, axis_n_value);
			}
		} else {
			/*800mg for x or y axis*/
			if (diff_axis[i] < 3277) {
				acc_selftest |= 1 << i;

				if (bmi_get_err_status(client_data) < 0)
					return err;
				dev_err(client_data->dev,
					"Over selftest minimum for "
					"axis:%s,diff=%d, p_val=%d, n_val=%d\n",
					bmi_axis_name[i], diff_axis[i],
						axis_p_value, axis_n_value);
				dev_err(client_data->dev, "err_st:0x%x\n",
						client_data->err_st.err_st_all);

			}
		}

	}
	/* gyro_selftest==1,gyro selftest successfully,
	* but bmi_result bit4 0 is successful, 1 is failed*/
	bmi_selftest = (acc_selftest & 0x0f) | ((!gyro_selftest) << AXIS_MAX);
	atomic_set(&client_data->selftest_result, bmi_selftest);
	/*soft reset*/
	err = BMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	if (err)
		return err;
	mdelay(5);
	dev_notice(client_data->dev, "Selftest for BMI16x finished\n");

	return count;
}

/* gyro sensor part */
static ssize_t bmi160_gyro_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", client_data->pw.gyro_pm);
}

static ssize_t bmi160_gyro_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	unsigned long op_mode;
	int err;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	mutex_lock(&client_data->mutex_op_mode);

	if (op_mode < BMI_GYRO_PM_MAX) {
		switch (op_mode) {
		case BMI_GYRO_PM_NORMAL:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_NORMAL]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_NORMAL;
			mdelay(3);
			break;
		case BMI_GYRO_PM_FAST_START:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_FAST_START]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_FAST_START;
			mdelay(3);
			break;
		case BMI_GYRO_PM_SUSPEND:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_SUSPEND]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_SUSPEND;
			mdelay(3);
			break;
		default:
			mutex_unlock(&client_data->mutex_op_mode);
			return -EINVAL;
		}
	} else {
		mutex_unlock(&client_data->mutex_op_mode);
		return -EINVAL;
	}

	mutex_unlock(&client_data->mutex_op_mode);

	if (err)
		return err;
	else
		return count;

}

static ssize_t bmi160_gyro_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	struct bmi160gyro_t data;
	struct bmi160_axis_data_t bmi160_udata;
	int err;

	err = BMI_CALL_API(read_gyro_xyz)(&data);
	if (err < 0)
		return err;

	bmi160_udata.x = data.x;
	bmi160_udata.y = data.y;
	bmi160_udata.z = data.z;

	bmi_remap_sensor_data(&bmi160_udata, client_data);

	return sprintf(buf, "%hd %hd %hd\n", bmi160_udata.x,
				bmi160_udata.y, bmi160_udata.z);
}

static ssize_t bmi160_gyro_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char range;
	err = BMI_CALL_API(get_gyr_range)(&range);
	if (err)
		return err;
	return sprintf(buf, "%d\n", range);
}

static ssize_t bmi160_gyro_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyr_range)(range);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_gyro_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char gyro_odr;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = BMI_CALL_API(get_gyro_bandwidth)(&gyro_odr);
	if (err)
		return err;

	client_data->odr.gyro_odr = gyro_odr;
	return sprintf(buf, "%d\n", gyro_odr);
}

static ssize_t bmi160_gyro_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long gyro_odr;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &gyro_odr);
	if (err)
		return err;

	if (gyro_odr < 6 || gyro_odr > 13)
		return -EIO;

	err = BMI_CALL_API(set_gyro_bandwidth)(gyro_odr);
	if (err)
		return -EIO;

	client_data->odr.gyro_odr = gyro_odr;
	return count;
}

static ssize_t bmi160_gyro_fast_calibration_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_gyr_en)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_gyro_fast_calibration_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_foc_gyr_en)((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_gyro_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_gyro_off_comp_xaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_gyro_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_offset_compensation_xaxis)
				((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_gyro_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_gyro_off_comp_yaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_gyro_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_offset_compensation_yaxis)
							((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_gyro_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_gyro_off_comp_zaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_gyro_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_offset_compensation_zaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}


/* mag sensor part */
#ifdef BMI160_MAG_INTERFACE_SUPPORT
static int bmm150_setting_init(struct bmi_client_data *client_data)
{
	int err = 0;
	struct bmi160mag_t data;

	/* need to modify as mag sensor connected,
	 * set write address to 0x4 band triggers write operation
	 * 0x4b(bmm150, power control reg, bit0)
	 * enables power in magnetometer, wake up bmm device
	 * from suspend mode to sleep mode */
	err += BMI_CALL_API(set_mag_write_data)(0x01);/*4f*/
	mdelay(1);
	err += BMI_CALL_API(set_mag_write_addr)(0x4b);/*4e*/

	/* set xy repetition with 3 times */
	err += BMI_CALL_API(set_mag_write_data)(0x01);
	mdelay(1);
	err += BMI_CALL_API(set_mag_write_addr)(0x51);
	/* set z repetition with 3 times */
	err += BMI_CALL_API(set_mag_write_data)(0x02);
	mdelay(1);
	err += BMI_CALL_API(set_mag_write_addr)(0x52);
	/*! force mode in bmm150  */
	err += BMI_CALL_API(set_mag_write_data)(0x02);
	mdelay(1);
	err += BMI_CALL_API(set_mag_write_addr)(0x4c);

	err += BMI_CALL_API(set_mag_manual_en)(0);

	err = BMI_CALL_API(read_mag_xyzr)(&data);

	mdelay(2);
	if (err < 0)
		return err;

	if (err)
		dev_err(client_data->dev, "bmm setting init err:%d\n", err);

	return err;

}

static int bmi160_mag_interface_init(struct bmi_client_data *client_data)
{
	int err = 0;
	u8 resistance = 0;

	err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_mag_arr[BMI_MAG_PM_NORMAL]);
	mdelay(2);
	/*mag interface 25Hz*/
	err += BMI_CALL_API(set_mag_bandwidth)(0x06);
	/*primary autoconfig secondary interface:mag */
	err += BMI_CALL_API(set_if_mode)(P_AUTO_S_MAG);
	mdelay(2);
	/*enable mag_manual_en*/
	err += BMI_CALL_API(set_mag_manual_en)(1);

	/*!set secondary pullup register to 2k*/
	/* set sequence for extended mode */
	err += BMI_CALL_API(set_command_register)(0x37);/* extmode_en_first*/
	err += BMI_CALL_API(set_command_register)(0x9a);/* extmode_en_middle*/
	err += BMI_CALL_API(set_command_register)(0xc0);/* extmode_en_last*/
	/*go to page 1 com*/
	err += BMI_CALL_API(set_target_page)(1);/*com*/
	err += BMI_CALL_API(set_paging_enable)(1);/*page enable*/
	mdelay(2);
	/*set register 2k*/
	err += client_data->device.bus_read(client_data->device.dev_addr,
							0x05, &resistance, 1);
	resistance |= 0x30;
	err += client_data->device.bus_write(client_data->device.dev_addr,
							0x05, &resistance, 1);
	/*go to page 0*/
	err += BMI_CALL_API(set_target_page)(0);/*usr*/
	err += BMI_CALL_API(set_paging_enable)(1);/*page enable*/
	mdelay(2);

	client_data->pw.mag_pm = BMI_MAG_PM_NORMAL;
	if (err)
		dev_err(client_data->dev,
				"bmi160 mag interface init err:%d\n", err);

	err = bmm150_setting_init(client_data);

	return err;

}

static ssize_t bmi160_mag_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", client_data->pw.mag_pm);
}

static ssize_t bmi160_mag_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	unsigned long op_mode;
	int err;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	mutex_lock(&client_data->mutex_op_mode);

	if (op_mode < BMI_MAG_PM_MAX) {

		switch (op_mode) {
		case BMI_MAG_PM_NORMAL:
			/* need to modify as mag sensor connected,
			 * set write address to 0x4c and triggers
			 * write operation
			 * 0x4c(op mode control reg)
			 * enables normal mode in magnetometer */
			err += BMI_CALL_API(set_mag_manual_en)(1);
			/*default sensor odr(10hz), normal*/
			err += BMI_CALL_API(set_mag_write_data)(0x00);
			err += BMI_CALL_API(set_mag_write_addr)(0x4c);
			mdelay(2);
			err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_mag_arr[BMI_MAG_PM_NORMAL]);
			err += BMI_CALL_API(set_mag_manual_en)(0);
			client_data->pw.mag_pm = op_mode;
		case	BMI_MAG_PM_LP1:
			/* need to modify as mag sensor connected,
			 * set write address to 0x4 band triggers
			 * write operation
			 * 0x4b(bmm150, power control reg, bit0)
			 * enables power in magnetometer*/
			/*err += BMI_CALL_API(set_mag_write_data)(0x01);
			err += BMI_CALL_API(set_mag_write_addr)(0x4b);
			mdelay(2);*/
			/* need to modify as mag sensor connected,
			 * set write address to 0x4c and triggers
			 * write operation
			 * 0x4c(op mode control reg)
			 * enables normal mode in magnetometer */
			 err += BMI_CALL_API(set_mag_manual_en)(1);
			/*default sensor odr, force*/
			err += BMI_CALL_API(set_mag_write_data)(0x02);
			err += BMI_CALL_API(set_mag_write_addr)(0x4c);
			mdelay(2);
			err += BMI_CALL_API(set_mag_manual_en)(0);
			err += BMI_CALL_API(set_command_register)
					(bmi_pmu_cmd_mag_arr[BMI_MAG_PM_LP1]);
			client_data->pw.mag_pm = op_mode;
			mdelay(3);
			break;
		case BMI_MAG_PM_SUSPEND:
		case BMI_MAG_PM_LP2:
			err += BMI_CALL_API(set_mag_manual_en)(1);

			/* need to modify as mag sensor connected,
			* set write address to 4b and triggers
			* write operation (disable power in magnetometer)
			*/
			err += BMI_CALL_API(set_mag_write_data)(0x00);
			/*bmm sensor suspend*/
			err += BMI_CALL_API(set_mag_write_addr)(0x4b);
			err += BMI_CALL_API(set_mag_manual_en)(0);
			err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_mag_arr[BMI_MAG_PM_SUSPEND]);
			client_data->pw.mag_pm = BMI_MAG_PM_SUSPEND;
			mdelay(3);
			break;
		default:
			mutex_unlock(&client_data->mutex_op_mode);
			return -EINVAL;
		}
	} else {
		mutex_unlock(&client_data->mutex_op_mode);
		return -EINVAL;
	}

	mutex_unlock(&client_data->mutex_op_mode);

	if (err)
		return err;
	else
		return count;

}

static ssize_t bmi160_mag_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char mag_odr;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = BMI_CALL_API(get_mag_bandwidth)(&mag_odr);
	if (err)
		return err;

	client_data->odr.mag_odr = mag_odr;
	return sprintf(buf, "%d\n", mag_odr);
}

static ssize_t bmi160_mag_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long mag_odr;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &mag_odr);
	if (err)
		return err;
	/*1~25/32hz,..6(25hz),7(50hz),...9(200hz) */
	err = BMI_CALL_API(set_mag_bandwidth)(mag_odr);
	if (err)
		return -EIO;

	client_data->odr.mag_odr = mag_odr;
	return count;
}

static ssize_t bmi160_mag_i2c_address_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(set_mag_manual_en)(1);
	err += BMI_CALL_API(get_i2c_device_addr)(&data);
	err += BMI_CALL_API(set_mag_manual_en)(0);

	if (err < 0)
		return err;
	return sprintf(buf, "0x%x\n", data);
}

static ssize_t bmi160_mag_i2c_address_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err += BMI_CALL_API(set_mag_manual_en)(1);
	if (err == 0)
		err += BMI_CALL_API(set_i2c_device_addr)((unsigned char)data);
	err += BMI_CALL_API(set_mag_manual_en)(0);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_mag_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	struct bmi160mag_t data;
	struct bmi160_axis_data_t bmi160_udata;
	int err;
	/* raw data without compensation */
	/*TO DO */
	err = BMI_CALL_API(read_mag_xyzr)(&data);

	if (err < 0)
		return err;
	bmi160_udata.x = data.x;
	bmi160_udata.y = data.y;
	bmi160_udata.z = data.z;
	bmi_remap_sensor_data(&bmi160_udata, client_data);
	return sprintf(buf, "%hd %hd %hd\n", bmi160_udata.x,
					bmi160_udata.y, bmi160_udata.z);
}
#endif

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static ssize_t bmi_enable_int_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int interrupt_type, value;

	sscanf(buf, "%3d %3d", &interrupt_type, &value);

	if (interrupt_type < 0 || interrupt_type > 16)
		return -EINVAL;

	if (interrupt_type <= BMI_FLAT_INT) {
		if (BMI_CALL_API(set_int_en_0)
				(bmi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	} else if (interrupt_type <= BMI_FWM_INT) {
		if (BMI_CALL_API(set_int_en_1)
				(bmi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	} else {
		if (BMI_CALL_API(set_int_en_2)
				(bmi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	}

	return count;
}

#endif

static DEVICE_ATTR(chip_id, S_IRUGO,
		bmi160_chip_id_show, NULL);
static DEVICE_ATTR(chip_ver, S_IRUGO,
		bmi160_chip_ver_show, NULL);
static DEVICE_ATTR(err_st, S_IRUGO,
		bmi160_err_st_show, NULL);
static DEVICE_ATTR(sensor_time, S_IRUGO,
		bmi160_sensor_time_show, NULL);

static DEVICE_ATTR(selftest, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_selftest_show, bmi160_selftest_store);

static DEVICE_ATTR(fifo_full_stop, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_full_stop_show, bmi160_fifo_full_stop_store);
static DEVICE_ATTR(fifo_flush, S_IWUSR|S_IWGRP|S_IWOTH,
		NULL, bmi160_fifo_flush_store);
static DEVICE_ATTR(fifo_bytecount, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_bytecount_show, bmi160_fifo_bytecount_store);
static DEVICE_ATTR(fifo_data_sel, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_data_sel_show, bmi160_fifo_data_sel_store);
static DEVICE_ATTR(fifo_data_frame, S_IRUGO,
		bmi160_fifo_data_out_frame_show, NULL);

static DEVICE_ATTR(fifo_watermark, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_watermark_show, bmi160_fifo_watermark_store);

static DEVICE_ATTR(fifo_header_en, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_header_en_show, bmi160_fifo_header_en_store);
static DEVICE_ATTR(fifo_time_en, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_time_en_show, bmi160_fifo_time_en_store);
static DEVICE_ATTR(fifo_int_tag_en, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_int_tag_en_show, bmi160_fifo_int_tag_en_store);

static DEVICE_ATTR(temperature, S_IRUGO,
		bmi160_temperature_show, NULL);
static DEVICE_ATTR(place, S_IRUGO,
		bmi160_place_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_delay_show, bmi160_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_enable_show, bmi160_enable_store);
static DEVICE_ATTR(acc_range, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_range_show, bmi160_acc_range_store);
static DEVICE_ATTR(acc_odr, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_odr_show, bmi160_acc_odr_store);
static DEVICE_ATTR(acc_op_mode, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_op_mode_show, bmi160_acc_op_mode_store);
static DEVICE_ATTR(acc_value, S_IRUGO,
		bmi160_acc_value_show, NULL);
static DEVICE_ATTR(acc_fast_calibration_x, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_fast_calibration_x_show,
		bmi160_acc_fast_calibration_x_store);
static DEVICE_ATTR(acc_fast_calibration_y, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_fast_calibration_y_show,
		bmi160_acc_fast_calibration_y_store);
static DEVICE_ATTR(acc_fast_calibration_z, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_fast_calibration_z_show,
		bmi160_acc_fast_calibration_z_store);
static DEVICE_ATTR(acc_offset_x, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_offset_x_show,
		bmi160_acc_offset_x_store);
static DEVICE_ATTR(acc_offset_y, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_offset_y_show,
		bmi160_acc_offset_y_store);
static DEVICE_ATTR(acc_offset_z, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_offset_z_show,
		bmi160_acc_offset_z_store);
static DEVICE_ATTR(test, S_IRUGO,
		bmi160_test_show, NULL);
/* gyro part */
static DEVICE_ATTR(gyro_op_mode, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_gyro_op_mode_show, bmi160_gyro_op_mode_store);
static DEVICE_ATTR(gyro_value, S_IRUGO,
		bmi160_gyro_value_show, NULL);
static DEVICE_ATTR(gyro_range, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_gyro_range_show, bmi160_gyro_range_store);
static DEVICE_ATTR(gyro_odr, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_gyro_odr_show, bmi160_gyro_odr_store);
static DEVICE_ATTR(gyro_fast_calibration_en, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
bmi160_gyro_fast_calibration_en_show, bmi160_gyro_fast_calibration_en_store);
static DEVICE_ATTR(gyro_offset_x, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
bmi160_gyro_offset_x_show, bmi160_gyro_offset_x_store);
static DEVICE_ATTR(gyro_offset_y, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
bmi160_gyro_offset_y_show, bmi160_gyro_offset_y_store);
static DEVICE_ATTR(gyro_offset_z, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
bmi160_gyro_offset_z_show, bmi160_gyro_offset_z_store);





#ifdef BMI160_MAG_INTERFACE_SUPPORT
static DEVICE_ATTR(mag_op_mode, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_mag_op_mode_show, bmi160_mag_op_mode_store);
static DEVICE_ATTR(mag_odr, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_mag_odr_show, bmi160_mag_odr_store);
static DEVICE_ATTR(mag_i2c_addr, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_mag_i2c_address_show, bmi160_mag_i2c_address_store);
static DEVICE_ATTR(mag_value, S_IRUGO,
		bmi160_mag_value_show, NULL);
#endif


#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static DEVICE_ATTR(enable_int, S_IWUSR|S_IWGRP|S_IWOTH,
		NULL, bmi_enable_int_store);
static DEVICE_ATTR(anymot_duration, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_anymot_duration_show, bmi160_anymot_duration_store);
static DEVICE_ATTR(anymot_threshold, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_anymot_threshold_show, bmi160_anymot_threshold_store);

#endif


static struct attribute *bmi160_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_chip_ver.attr,
	&dev_attr_err_st.attr,
	&dev_attr_sensor_time.attr,
	&dev_attr_selftest.attr,

	&dev_attr_test.attr,
	&dev_attr_fifo_full_stop.attr,
	&dev_attr_fifo_flush.attr,
	&dev_attr_fifo_header_en.attr,
	&dev_attr_fifo_time_en.attr,
	&dev_attr_fifo_int_tag_en.attr,
	&dev_attr_fifo_bytecount.attr,
	&dev_attr_fifo_data_sel.attr,
	&dev_attr_fifo_data_frame.attr,

	&dev_attr_fifo_watermark.attr,

	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_temperature.attr,
	&dev_attr_place.attr,

	&dev_attr_acc_range.attr,
	&dev_attr_acc_odr.attr,
	&dev_attr_acc_op_mode.attr,
	&dev_attr_acc_value.attr,

	&dev_attr_acc_fast_calibration_x.attr,
	&dev_attr_acc_fast_calibration_y.attr,
	&dev_attr_acc_fast_calibration_z.attr,
	&dev_attr_acc_offset_x.attr,
	&dev_attr_acc_offset_y.attr,
	&dev_attr_acc_offset_z.attr,

	&dev_attr_gyro_op_mode.attr,
	&dev_attr_gyro_value.attr,
	&dev_attr_gyro_range.attr,
	&dev_attr_gyro_odr.attr,
	&dev_attr_gyro_fast_calibration_en.attr,
	&dev_attr_gyro_offset_x.attr,
	&dev_attr_gyro_offset_y.attr,
	&dev_attr_gyro_offset_z.attr,

#ifdef BMI160_MAG_INTERFACE_SUPPORT
	&dev_attr_mag_op_mode.attr,
	&dev_attr_mag_odr.attr,
	&dev_attr_mag_i2c_addr.attr,
	&dev_attr_mag_value.attr,
#endif

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
	&dev_attr_enable_int.attr,
	&dev_attr_anymot_duration.attr,
	&dev_attr_anymot_threshold.attr,
#endif
	NULL
};

static struct attribute_group bmi160_attribute_group = {
	.attrs = bmi160_attributes
};

static void bmi_delay(u32 msec)
{
	mdelay(msec);
}

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static void bmi_slope_interrupt_handle(struct bmi_client_data *client_data)
{
	/* anym_first[0..2]: x, y, z */
	u8 anym_first[3] = {0};
	u8 status2;
	u8 anym_sign;
	u8 i = 0;

	client_data->device.bus_read(client_data->device.dev_addr,
				BMI160_USER_INT_STATUS_2_ADDR, &status2, 1);
	anym_first[0] = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INT_STATUS_2_ANYM_FIRST_X);
	anym_first[1] = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INT_STATUS_2_ANYM_FIRST_Y);
	anym_first[2] = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INT_STATUS_2_ANYM_FIRST_Z);
	anym_sign = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INT_STATUS_2_ANYM_SIGN);

	for (i = 0; i < 3; i++) {
		if (anym_first[i]) {
			/*1: negative*/
			if (anym_sign)
				dev_notice(client_data->dev,
				"Anymotion interrupt happend!"
				"%s axis, negative sign\n", bmi_axis_name[i]);
			else
				dev_notice(client_data->dev,
				"Anymotion interrupt happend!"
				"%s axis, postive sign\n", bmi_axis_name[i]);
		}
	}


}

static void bmi_fifo_watermark_interrupt_handle
					(struct bmi_client_data *client_data)
{
	int err = 0;
	unsigned char *fifo_data;
	unsigned char *fifo_out_data;
	unsigned int fifo_len0 = 0;

	if (client_data->pw.acc_pm == 2 && client_data->pw.gyro_pm == 2
					&& client_data->pw.mag_pm == 2)
		pr_info("pw_acc: %d, pw_gyro: %d\n",
			client_data->pw.acc_pm, client_data->pw.gyro_pm);
	if (!client_data->fifo_data_sel)
		pr_info("no selsect sensor fifo, fifo_data_sel:%d\n",
						client_data->fifo_data_sel);

	fifo_data = kzalloc(1024 * sizeof(unsigned char), GFP_KERNEL);
	if (!fifo_data) {
		pr_info("failed to allocate memory for fifo_data\n");
		return;
	}

	fifo_out_data = kzalloc(1024 * sizeof(unsigned char), GFP_KERNEL);
	if (!fifo_out_data) {
		pr_info("failed to allocate memory for fifo_out_data\n");
		goto exit_fifo_data;
	}

	err = BMI_CALL_API(fifo_length)(&fifo_len0);
	client_data->fifo_bytecount = fifo_len0;

	if (client_data->fifo_bytecount == 0 || err)
		goto exit_fifo_out_data;

	/* need give attention for the time of burst read*/
	if (!err) {
		err = bmi_burst_read_wrapper(client_data->device.dev_addr,
			BMI160_USER_FIFO_DATA__REG, fifo_data,
					client_data->fifo_bytecount + 20);
	} else
		dev_err(client_data->dev, "read fifo leght err");

	if (err)
		dev_err(client_data->dev, "brust read fifo err\n");
	err = bmi_fifo_analysis_handle(client_data, fifo_data,
			client_data->fifo_bytecount + 20, fifo_out_data);
	/* TO DO*/
exit_fifo_out_data:
	kfree(fifo_out_data);
exit_fifo_data:
	kfree(fifo_data);
	return;
}


static void bmi_irq_work_func(struct work_struct *work)
{
	struct bmi_client_data *client_data =
		container_of((struct work_struct *)work,
			struct bmi_client_data, irq_work);

	unsigned char int_status[4] = {0, 0, 0, 0};

	client_data->device.bus_read(client_data->device.dev_addr,
				BMI160_USER_INT_STATUS_0_ADDR, int_status, 4);

	if (BMI160_GET_BITSLICE(int_status[0], BMI160_USER_INT_STATUS_0_ANYM))
		bmi_slope_interrupt_handle(client_data);
	if (BMI160_GET_BITSLICE(int_status[1],
				BMI160_USER_INT_STATUS_1_FWM_INT))
		bmi_fifo_watermark_interrupt_handle(client_data);
}

static irqreturn_t bmi_irq_handler(int irq, void *handle)
{
	struct bmi_client_data *client_data = handle;

	if (client_data == NULL)
		return IRQ_HANDLED;
	if (client_data->dev == NULL)
		return IRQ_HANDLED;
	schedule_work(&client_data->irq_work);

	return IRQ_HANDLED;
}
#endif /* defined(BMI_ENABLE_INT1)||defined(BMI_ENABLE_INT2) */

int bmi_probe(struct bmi_client_data *client_data, struct device *dev)
{
	int err = 0;
	u8 mag_dev_addr;

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
	struct bosch_sensor_specific *pdata;
#endif


	/* check chip id */
	err = bmi_check_chip_id(client_data);
	if (err)
		goto exit_err_clean;

	dev_set_drvdata(dev, client_data);
	client_data->dev = dev;

	mutex_init(&client_data->mutex_enable);
	mutex_init(&client_data->mutex_op_mode);

	/* input device init */
	err = bmi_input_init(client_data);
	if (err < 0)
		goto exit_err_clean;

	/* sysfs node creation */
	err = sysfs_create_group(&client_data->input->dev.kobj,
			&bmi160_attribute_group);

	if (err < 0)
		goto exit_err_sysfs;

	if (NULL != dev->platform_data) {
		client_data->bst_pd = kzalloc(sizeof(*client_data->bst_pd),
				GFP_KERNEL);

		if (NULL != client_data->bst_pd) {
			memcpy(client_data->bst_pd, dev->platform_data,
					sizeof(*client_data->bst_pd));
			dev_notice(dev, "%s sensor driver set place: p%d\n",
					client_data->bst_pd->name,
					client_data->bst_pd->place);
		}
	}


	/* workqueue init */
	INIT_DELAYED_WORK(&client_data->work, bmi_work_func);
	atomic_set(&client_data->delay, BMI_DELAY_DEFAULT);

	/* h/w init */
	client_data->device.delay_msec = bmi_delay;
	err = BMI_CALL_API(init)(&client_data->device);

	bmi_dump_reg(client_data);

	/*power on detected*/
	/*or softrest(cmd 0xB6) */
	/*fatal err check*/
	/*soft reset*/
	err += BMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	mdelay(3);
	if (err)
		dev_err(dev, "Failed soft reset, er=%d", err);
	/*usr data config page*/
	err += BMI_CALL_API(set_target_page)(USER_DAT_CFG_PAGE);
	if (err)
		dev_err(dev, "Failed cffg page, er=%d", err);
	err += bmi_get_err_status(client_data);
	if (err) {
		dev_err(dev, "Failed to bmi16x init!err_st=0x%x\n",
				client_data->err_st.err_st_all);
		goto exit_err_sysfs;
	}

	err += BMI_CALL_API(get_i2c_device_addr)(&mag_dev_addr);

#ifdef BMI160_MAG_INTERFACE_SUPPORT
		err += BMI_CALL_API(set_i2c_device_addr)(0x10);
		bmi160_mag_interface_init(client_data);
#endif
	if (err < 0)
		goto exit_err_sysfs;


#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
	pdata = client_data->bst_pd;
	if (pdata) {
		if (pdata->irq_gpio_cfg && (pdata->irq_gpio_cfg() < 0)) {
			dev_err(client_data->dev, "IRQ GPIO conf. error %d\n",
						pdata->irq);
		}
	}

#ifdef BMI160_ENABLE_INT1
		/* maps interrupt to INT1/InT2 pin */
		BMI_CALL_API(set_int_anymo)(BMI_INT0, ENABLE);
		BMI_CALL_API(set_int_fwm)(BMI_INT0, ENABLE);

		/*Set interrupt trige level way */
		BMI_CALL_API(set_int_edge_ctrl)(BMI_INT0, BMI_INT_LEVEL);
		bmi160_set_int_lvl(BMI_INT0, 1);
		/*set interrupt latch temporary, 5 ms*/
		/*bmi160_set_latch_int(5);*/

		BMI_CALL_API(set_output_en)(BMI160_INT1_OUTPUT_EN, ENABLE);
#endif

#ifdef BMI160_ENABLE_INT2
		/* maps interrupt to INT1/InT2 pin */
		BMI_CALL_API(set_int_anymo)(BMI_INT1, ENABLE);
		BMI_CALL_API(set_int_fwm)(BMI_INT1, ENABLE);
		BMI_CALL_API(set_int_drdy)(BMI_INT1, ENABLE);

		BMI_CALL_API(set_output_en)(BMI160_INT2_OUTPUT_EN, ENABLE);
#endif

		client_data->IRQ = client_data->bst_pd->irq;
		err = request_irq(client_data->IRQ, bmi_irq_handler,
				IRQF_TRIGGER_RISING, "bmi160", client_data);
		if (err)
			dev_err(client_data->dev,  "could not request irq\n");

		INIT_WORK(&client_data->irq_work, bmi_irq_work_func);
#endif

	client_data->wkqueue_en = 0x00;
	client_data->fifo_data_sel = 0;
	BMI_CALL_API(get_acc_bandwidth)(&client_data->odr.acc_odr);
	BMI_CALL_API(get_gyro_bandwidth)(&client_data->odr.gyro_odr);
	BMI_CALL_API(get_mag_bandwidth)(&client_data->odr.mag_odr);
	/* now it's power on which is considered as resuming from suspend */

	/* set sensor PMU into suspend power mode for all */
	if (bmi_pmu_set_suspend(client_data) < 0) {
		dev_err(dev, "Failed to set BMI160 to suspend power mode\n");
		goto exit_err_sysfs;
	}

	dev_notice(dev, "sensor %s probed successfully", SENSOR_NAME);
	regulator_disable(client_data->avdd);

	return 0;

exit_err_sysfs:
	if (err)
		bmi_input_destroy(client_data);

exit_err_clean:
	if (err) {
		if (client_data != NULL) {
			if (NULL != client_data->bst_pd) {
				kfree(client_data->bst_pd);
				client_data->bst_pd = NULL;
			}
			kfree(client_data);
			client_data = NULL;
		}
	}
	regulator_disable(client_data->avdd);
	return err;
}
EXPORT_SYMBOL(bmi_probe);

/*!
 * @brief remove bmi client
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
int bmi_remove(struct device *dev)
{
	int err = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	if (NULL != client_data) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&client_data->early_suspend_handler);
#endif
		mutex_lock(&client_data->mutex_enable);
		if (BMI_ACC_PM_NORMAL == client_data->pw.acc_pm ||
			BMI_GYRO_PM_NORMAL == client_data->pw.gyro_pm ||
				BMI_MAG_PM_NORMAL == client_data->pw.mag_pm) {
			cancel_delayed_work_sync(&client_data->work);
		}
		mutex_unlock(&client_data->mutex_enable);

		err = bmi_pmu_set_suspend(client_data);

		mdelay(5);

		sysfs_remove_group(&client_data->input->dev.kobj,
				&bmi160_attribute_group);
		bmi_input_destroy(client_data);

		if (NULL != client_data->bst_pd) {
			kfree(client_data->bst_pd);
			client_data->bst_pd = NULL;
		}
		kfree(client_data);
	}

	return err;
}
EXPORT_SYMBOL(bmi_remove);

static int bmi_post_resume(struct bmi_client_data *client_data)
{
	int err = 0;

	mutex_lock(&client_data->mutex_enable);
	if (client_data->wkqueue_en) {
		schedule_delayed_work(&client_data->work,
				msecs_to_jiffies(
					atomic_read(&client_data->delay)));
	}
	mutex_unlock(&client_data->mutex_enable);

	return err;
}


int bmi_suspend(struct device *dev)
{
	int err = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	dev_info(client_data->dev, "bmi suspend function entrance");


	if (client_data->wkqueue_en)
		cancel_delayed_work_sync(&client_data->work);
	if (client_data->pw.acc_pm != BMI_ACC_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_SUSPEND]);
		client_data->pw.acc_pm = BMI_ACC_PM_SUSPEND;
		mdelay(3);
	}
	if (client_data->pw.gyro_pm != BMI_GYRO_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_SUSPEND]);
		client_data->pw.gyro_pm = BMI_GYRO_PM_SUSPEND;
		mdelay(3);
	}

	return err;
}
EXPORT_SYMBOL(bmi_suspend);

int bmi_resume(struct device *dev)
{
	int err = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	/* post resume operation */
	err = bmi_post_resume(client_data);

	return err;
}
EXPORT_SYMBOL(bmi_resume);

