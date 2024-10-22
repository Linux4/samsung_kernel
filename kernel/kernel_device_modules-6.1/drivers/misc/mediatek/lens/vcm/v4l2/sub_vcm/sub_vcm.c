// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define DRIVER_NAME "sub_vcm"
#define SUB_VCM_I2C_SLAVE_ADDR 0x18

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define SUB_VCM_NAME				"sub_vcm"
#define SUB_VCM_MAX_FOCUS_POS			1023
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define SUB_VCM_FOCUS_STEPS			1

#define REGULATOR_MAXSIZE			16

static const char * const ldo_names[] = {
	"vin",
	"vdd",
};

/* sub_vcm device structure */
struct sub_vcm_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
	struct regulator *ldo[REGULATOR_MAXSIZE];
	struct work_struct poweroff_work;
	bool is_powered_off;
	bool is_stop_flag;
	wait_queue_head_t wait_queue_head;
};

static struct workqueue_struct *sub_vcm_init_wq;

static DEFINE_MUTEX(sub_vcm_mutex);

#define I2CCOMM_MAXSIZE 4

/* I2C format */
struct stVCM_I2CFormat {
	/* Register address */
	uint8_t Addr[I2CCOMM_MAXSIZE];
	uint8_t AddrNum;

	/* Register Data */
	uint8_t CtrlData[I2CCOMM_MAXSIZE];
	uint8_t BitR[I2CCOMM_MAXSIZE];
	uint8_t Mask1[I2CCOMM_MAXSIZE];
	uint8_t BitL[I2CCOMM_MAXSIZE];
	uint8_t Mask2[I2CCOMM_MAXSIZE];
	uint8_t DataNum;
};

#define I2CSEND_MAXSIZE 4

struct VcmDriverConfig {
	// Init param
	unsigned int ctrl_delay_us;
	char wr_table[16][3];

	// Per-frame param
	unsigned int slave_addr;
	uint8_t I2CSendNum;
	struct stVCM_I2CFormat I2Cfmt[I2CSEND_MAXSIZE];

	// Uninit param
	unsigned int origin_focus_pos;
	int32_t move_long_steps_pct;
	int32_t move_short_steps_pct;
	unsigned int move_delay_us;
	uint32_t delay_power_off_ms;
	char wr_rls_table[8][3];

	// resume and stand-by mode setting
	char resume_table[8][3];
	char suspend_table[8][3];

	// Capacity
	int32_t vcm_bits;
	int32_t af_calib_bits;
};

struct VcmDriverList {
	int32_t sensor_id;
	int32_t module_id;
	int32_t vcm_id;
	char    vcm_drv_name[32];
	uint64_t vcm_settling_time;

	struct VcmDriverConfig vcm_config;
};

struct mtk_vcm_info {
	struct VcmDriverList *p_vcm_info;
};

struct VcmDriverList g_vcmconfig;

/* Control commnad */
#define VIDIOC_MTK_S_LENS_INFO _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct mtk_vcm_info)
#define VCM_IOC_POWER_ON         _IO('V', BASE_VIDIOC_PRIVATE + 4)
#define VCM_IOC_POWER_OFF        _IO('V', BASE_VIDIOC_PRIVATE + 5)

static inline struct sub_vcm_device *to_sub_vcm_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct sub_vcm_device, ctrls);
}

static inline struct sub_vcm_device *sd_to_sub_vcm_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct sub_vcm_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static void stop_background_works(struct sub_vcm_device *sub_vcm)
{
	LOG_INF("+\n");
	sub_vcm->is_stop_flag = true;

	wake_up_interruptible(&sub_vcm->wait_queue_head);

	/* sub_vcm Workqueue */
	if (sub_vcm_init_wq) {
		LOG_INF("flush work queue\n");

		/* flush work queue */
		flush_work(&sub_vcm->poweroff_work);

		flush_workqueue(sub_vcm_init_wq);
		destroy_workqueue(sub_vcm_init_wq);
		sub_vcm_init_wq = NULL;
	}

	sub_vcm->is_stop_flag = false;

	LOG_INF("-\n");
}

static void register_setting(struct i2c_client *client, char table[][3], int table_size)
{
	int ret = 0, read_count = 0, i = 0, j = 0;

	for (i = 0; i < table_size; ++i) {

		LOG_INF("table[%d] = [0x%x 0x%x 0x%x]\n",
			i, table[i][0],
			table[i][1],
			table[i][2]);

		if (table[i][0] == 0)
			break;

		// write command
		if (table[i][0] == 0x1) {

			// write register
			ret = i2c_smbus_write_byte_data(client,
					table[i][1],
					table[i][2]);
			if (ret < 0) {
				LOG_INF(
					"i2c write fail: %d, table[%d] = [0x%x 0x%x 0x%x]\n",
					ret, i, table[i][0],
					table[i][1],
					table[i][2]);
			}

		// read command w/ check result value
		} else if (table[i][0] == 0x2) {
			read_count = 0;

			// read register and check value
			do {
				if (read_count > 10) {
					LOG_INF(
						"timeout, i2c read fail: %d, table[%d] = [0x%x 0x%x 0x%x]\n",
						ret, i, table[i][0],
						table[i][1],
						table[i][2]);
					break;
				}
				ret = i2c_smbus_read_byte_data(client,
					table[i][1]);
				read_count++;
			} while (ret != table[i][2]);

		// delay command
		} else if (table[i][0] == 0x3) {

			LOG_INF("delay time: %dms\n", table[i][1]);
			mdelay(table[i][1]);

		// read command w/o check result value
		} else if (table[i][0] == 0x4) {

			// read register
			for (j = 0; j < table[i][2]; ++j) {
				ret = i2c_smbus_read_byte_data(client,
					table[i][1]);
				if (ret < 0)
					LOG_INF(
						"i2c read fail: %d, table[%d] = [0x%x 0x%x 0x%x]\n",
						ret, i, table[i][0],
						table[i][1],
						table[i][2]);
				else
					LOG_INF(
						"table[%d] = [0x%x 0x%x 0x%x], result value is 0x%x",
						i, table[i][0], table[i][1],
						table[i][2], ret);
			}

		} else {
			// reserved
		}
		udelay(100);
	}
}

static int sub_vcm_set_position(struct sub_vcm_device *sub_vcm, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sub_vcm->sd);
	int ret = -1;
	char puSendCmd[3] = {0};
	unsigned int nArrayIndex = 0;
	int i = 0, j = 0;
	int retry = 3;

	for (i = 0; i < g_vcmconfig.vcm_config.I2CSendNum; ++i, nArrayIndex = 0) {
		int nCommNum = g_vcmconfig.vcm_config.I2Cfmt[i].AddrNum +
			g_vcmconfig.vcm_config.I2Cfmt[i].DataNum;

		// Fill address
		for (j = 0; j < g_vcmconfig.vcm_config.I2Cfmt[i].AddrNum; ++j) {
			if (nArrayIndex >= ARRAY_SIZE(puSendCmd)) {
				LOG_INF("nArrayIndex(%d) exceeds the size of puSendCmd(%d)\n",
					nArrayIndex, (int)ARRAY_SIZE(puSendCmd));
				return -1;
			}
			puSendCmd[nArrayIndex] = g_vcmconfig.vcm_config.I2Cfmt[i].Addr[j];
			++nArrayIndex;
		}

		// Fill data
		for (j = 0; j < g_vcmconfig.vcm_config.I2Cfmt[i].DataNum; ++j) {
			if (nArrayIndex >= ARRAY_SIZE(puSendCmd)) {
				LOG_INF("nArrayIndex(%d) exceeds the size of puSendCmd(%d)\n",
					nArrayIndex, (int)ARRAY_SIZE(puSendCmd));
				return -1;
			}
			puSendCmd[nArrayIndex] =
				((((val >> g_vcmconfig.vcm_config.I2Cfmt[i].BitR[j]) &
				g_vcmconfig.vcm_config.I2Cfmt[i].Mask1[j]) <<
				g_vcmconfig.vcm_config.I2Cfmt[i].BitL[j]) &
				g_vcmconfig.vcm_config.I2Cfmt[i].Mask2[j]) |
				g_vcmconfig.vcm_config.I2Cfmt[i].CtrlData[j];
			++nArrayIndex;
		}

		while (retry-- > 0) {
			ret = i2c_master_send(client, puSendCmd, nCommNum);
			if (ret >= 0)
				break;
		}

		if (ret < 0) {
			LOG_INF(
				"puSendCmd I2C failure i2c_master_send: %d, I2Cfmt[%d].AddrNum/DataNum: %d/%d\n",
				ret, i, g_vcmconfig.vcm_config.I2Cfmt[i].AddrNum,
				g_vcmconfig.vcm_config.I2Cfmt[i].DataNum);
			return ret;
		}
	}

	return ret;
}

static int sub_vcm_release(struct sub_vcm_device *sub_vcm)
{
	int ret = 0;
	int long_step, short_step;
	int diff_dac = 0;
	int nStep_count_long = 0, nStep_count_short = 0;
	int val = 0;
	int i = 0;

	struct i2c_client *client = v4l2_get_subdevdata(&sub_vcm->sd);

	long_step = g_vcmconfig.vcm_config.move_long_steps_pct *
			(1 << g_vcmconfig.vcm_config.vcm_bits) / 100;
	short_step = g_vcmconfig.vcm_config.move_short_steps_pct *
			(1 << g_vcmconfig.vcm_config.vcm_bits) / 100;

	diff_dac = g_vcmconfig.vcm_config.origin_focus_pos - sub_vcm->focus->val;
	val = sub_vcm->focus->val;

	if (long_step == 0) {
		LOG_INF("no need to process long step\n");
		goto PROCESS_SHORT_STEP;
	}

	// process long step
	nStep_count_long = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) / long_step;
	LOG_INF("long_step: %d, short_step: %d, diff_dac: %d, nStep_count_long: %d\n",
		long_step, short_step, diff_dac, nStep_count_long);

	// If the diff_dac is a multiple of long_step, the last long_step is processed using short_step
	if (((diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) % long_step) == 0) {
		nStep_count_long--;
		LOG_INF("the diff_dac is a multiple of long_step, nStep_count_long: %d\n",
			nStep_count_long);
	}

	for (i = 0; i < nStep_count_long; ++i) {
		val += (diff_dac < 0 ? (long_step*(-1)) : long_step);

		LOG_INF("long step index %d, val: %d\n", i, val);

		ret = sub_vcm_set_position(sub_vcm, val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d\n",
				__func__, ret);
			return ret;
		}

		ret = wait_event_interruptible_timeout(sub_vcm->wait_queue_head,
				sub_vcm->is_stop_flag,
				msecs_to_jiffies(g_vcmconfig.vcm_config.move_delay_us/1000));
		if (ret > 0) {
			LOG_INF("back to origin, long step, interrupting, ret = %d\n", ret);
			return ret;
		}
	}

PROCESS_SHORT_STEP:
	if (short_step == 0) {
		LOG_INF("no need to process short step\n");
		goto LAST_STEP_TO_ORIGIN;
	}

	// process short step
	diff_dac = g_vcmconfig.vcm_config.origin_focus_pos - val;
	nStep_count_short = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) / short_step;
	LOG_INF("process short step: diff_dac: %d, nStep_count_short: %d\n",
			diff_dac, nStep_count_short);

	for (i = 0; i < nStep_count_short; ++i) {
		val += (diff_dac < 0 ? (short_step*(-1)) : short_step);

		LOG_INF("short step index %d, val: %d\n", i, val);

		ret = sub_vcm_set_position(sub_vcm, val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d\n",
				__func__, ret);
			return ret;
		}

		ret = wait_event_interruptible_timeout(sub_vcm->wait_queue_head,
				sub_vcm->is_stop_flag,
				msecs_to_jiffies(g_vcmconfig.vcm_config.move_delay_us/1000));
		if (ret > 0) {
			LOG_INF("back to origin, short step, interrupting, ret = %d\n", ret);
			return ret;
		}
	}

	// if diff_dac is less than short_step, it means it is very close to the origin
	if (((diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) % short_step) == 0) {
		LOG_INF("already back to origin\n");
		return 0;
	}

LAST_STEP_TO_ORIGIN:
	// last step to origin
	LOG_INF("last step to origin\n");
	ret = sub_vcm_set_position(sub_vcm, g_vcmconfig.vcm_config.origin_focus_pos);
	if (ret < 0) {
		LOG_INF("%s I2C failure: %d\n",
			__func__, ret);
		return ret;
	}

	register_setting(client, g_vcmconfig.vcm_config.wr_rls_table, 8);

	return 0;
}

static int sub_vcm_init(struct sub_vcm_device *sub_vcm)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sub_vcm->sd);
	int ret = 0;

	LOG_INF("+\n");

	client->addr = g_vcmconfig.vcm_config.slave_addr >> 1;

	register_setting(client, g_vcmconfig.vcm_config.wr_table, 16);

	LOG_INF("-\n");

	return ret;
}

/* Power handling */
static int sub_vcm_power_off(struct sub_vcm_device *sub_vcm)
{
	int ret = 0;
	int ldo_num = 0;
	int i = 0;

	LOG_INF("+\n");
	sub_vcm->is_stop_flag = false;

	// delay power off feature
	LOG_INF("delay power off feature start\n");
	ret = wait_event_interruptible_timeout(sub_vcm->wait_queue_head,
				sub_vcm->is_stop_flag,
				msecs_to_jiffies(g_vcmconfig.vcm_config.delay_power_off_ms));
	if (ret > 0) {
		LOG_INF("interrupting, ret = %d\n", ret);
		return ret;
	}
	LOG_INF("delay power off feature end\n");

	ret = sub_vcm_release(sub_vcm);
	if (ret < 0) {
		LOG_INF("sub_vcm release failed!\n");
	} else if (ret > 0) {
		// interrupting
		return ret;
	}
	LOG_INF("sub_vcm release end\n");

	ldo_num = ARRAY_SIZE(ldo_names);
	if (ldo_num > REGULATOR_MAXSIZE)
		ldo_num = REGULATOR_MAXSIZE;
	for (i = 0; i < ldo_num; i++) {
		if (sub_vcm->ldo[i]) {
			ret = regulator_disable(sub_vcm->ldo[i]);
			if (ret < 0)
				LOG_INF("cannot disable %d regulator\n", i);
		}
	}

	if (sub_vcm->vcamaf_pinctrl && sub_vcm->vcamaf_off)
		ret = pinctrl_select_state(sub_vcm->vcamaf_pinctrl,
					sub_vcm->vcamaf_off);

	sub_vcm->is_powered_off = true;

	LOG_INF("-\n");

	return ret;
}

static void sub_vcm_poweroff_work(struct work_struct *work)
{
	struct sub_vcm_device *sub_vcm = NULL;
	int ret = 0;

	LOG_INF("+\n");

	sub_vcm = container_of(work, struct sub_vcm_device, poweroff_work);

	ret = sub_vcm_power_off(sub_vcm);

	LOG_INF("power off sub_vcm, return value:%d\n", ret);
	LOG_INF("-\n");
}

static int sub_vcm_power_on(struct sub_vcm_device *sub_vcm)
{
	int ret = 0;
	int ldo_num = 0;
	int i = 0;

	LOG_INF("+\n");

	ldo_num = ARRAY_SIZE(ldo_names);
	if (ldo_num > REGULATOR_MAXSIZE)
		ldo_num = REGULATOR_MAXSIZE;
	for (i = 0; i < ldo_num; i++) {
		if (sub_vcm->ldo[i]) {
			ret = regulator_enable(sub_vcm->ldo[i]);
			if (ret < 0)
				LOG_INF("cannot enable %d regulator\n", i);
		}
	}

	if (sub_vcm->vcamaf_pinctrl && sub_vcm->vcamaf_on)
		ret = pinctrl_select_state(sub_vcm->vcamaf_pinctrl,
					sub_vcm->vcamaf_on);

	return ret;
}

static int sub_vcm_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct sub_vcm_device *sub_vcm = to_sub_vcm_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = sub_vcm_set_position(sub_vcm, ctrl->val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d\n",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops sub_vcm_vcm_ctrl_ops = {
	.s_ctrl = sub_vcm_set_ctrl,
};

static int sub_vcm_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct sub_vcm_device *sub_vcm = sd_to_sub_vcm_vcm(sd);

	LOG_INF("+\n");

	/* Protect the Multi Process */
	mutex_lock(&sub_vcm_mutex);

	stop_background_works(sub_vcm);

	if (sub_vcm->is_powered_off) {
		ret = sub_vcm_power_on(sub_vcm);
		if (ret < 0) {
			LOG_INF("power on fail, ret = %d\n", ret);
			mutex_unlock(&sub_vcm_mutex);
			return ret;
		}
	} else {
		LOG_INF("no need to power on again\n");
	}

	mutex_unlock(&sub_vcm_mutex);
	LOG_INF("-\n");

	return 0;
}

static int sub_vcm_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sub_vcm_device *sub_vcm = sd_to_sub_vcm_vcm(sd);

	LOG_INF("+\n");

	/* Protect the Multi Process */
	mutex_lock(&sub_vcm_mutex);

	/* sub_vcm Workqueue */
	if (sub_vcm_init_wq == NULL) {
		LOG_INF("create_singlethread_workqueue\n");
		sub_vcm_init_wq =
			create_singlethread_workqueue("sub_vcm_init_work");
		if (!sub_vcm_init_wq) {
			LOG_INF("create_singlethread_workqueue fail\n");
			mutex_unlock(&sub_vcm_mutex);
			return -ENOMEM;
		}
	} else {
		// wait work queue done
		LOG_INF("flush work queue\n");

		/* flush work queue */
		flush_work(&sub_vcm->poweroff_work);
	}

	queue_work(sub_vcm_init_wq, &sub_vcm->poweroff_work);
	mutex_unlock(&sub_vcm_mutex);
	LOG_INF("-\n");

	return 0;
}

static long sub_vcm_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sub_vcm_device *sub_vcm = sd_to_sub_vcm_vcm(sd);
	struct i2c_client *client = v4l2_get_subdevdata(&sub_vcm->sd);

	client->addr = g_vcmconfig.vcm_config.slave_addr >> 1;

	switch (cmd) {

	case VIDIOC_MTK_S_LENS_INFO:
	{
		struct mtk_vcm_info *info = arg;

		if (sub_vcm->is_powered_off == false) {
			LOG_INF("no need to init again\n");
			ret = 0;
			break;
		}

		// reset power off flag
		sub_vcm->is_powered_off = false;

		if (copy_from_user(&g_vcmconfig,
				   (void *)info->p_vcm_info,
				   sizeof(struct VcmDriverList)) != 0) {
			LOG_INF("VIDIOC_MTK_S_LENS_INFO copy_from_user failed\n");
			ret = -EFAULT;
			break;
		}

		// Confirm hardware requirements and adjust/remove the delay.
		usleep_range(g_vcmconfig.vcm_config.ctrl_delay_us,
			g_vcmconfig.vcm_config.ctrl_delay_us + 100);

		LOG_INF("slave_addr: 0x%x\n", g_vcmconfig.vcm_config.slave_addr);

		ret = sub_vcm_init(sub_vcm);
		if (ret < 0)
			LOG_INF("init error\n");
	}
	break;
	case VCM_IOC_POWER_ON:
	{
		// customized area
		LOG_INF("active mode, current pos:%d\n", sub_vcm->focus->val);

		register_setting(client, g_vcmconfig.vcm_config.resume_table, 8);
		sub_vcm_set_position(sub_vcm, sub_vcm->focus->val);
	}
	break;
	case VCM_IOC_POWER_OFF:
	{
		// customized area
		LOG_INF("stand by mode\n");

		register_setting(client, g_vcmconfig.vcm_config.suspend_table, 8);
	}
	break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_internal_ops sub_vcm_int_ops = {
	.open = sub_vcm_open,
	.close = sub_vcm_close,
};

static const struct v4l2_subdev_core_ops sub_vcm_ops_core = {
	.ioctl = sub_vcm_ops_core_ioctl,
};

static const struct v4l2_subdev_ops sub_vcm_ops = {
	.core = &sub_vcm_ops_core,
};

static void sub_vcm_subdev_cleanup(struct sub_vcm_device *sub_vcm)
{
	v4l2_async_unregister_subdev(&sub_vcm->sd);
	v4l2_ctrl_handler_free(&sub_vcm->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sub_vcm->sd.entity);
#endif
}

static int sub_vcm_init_controls(struct sub_vcm_device *sub_vcm)
{
	struct v4l2_ctrl_handler *hdl = &sub_vcm->ctrls;
	const struct v4l2_ctrl_ops *ops = &sub_vcm_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	sub_vcm->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, SUB_VCM_MAX_FOCUS_POS, SUB_VCM_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	sub_vcm->sd.ctrl_handler = hdl;

	return 0;
}

static int sub_vcm_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sub_vcm_device *sub_vcm;
	int ret;
	int ldo_num;
	int i;

	LOG_INF("+\n");

	sub_vcm = devm_kzalloc(dev, sizeof(*sub_vcm), GFP_KERNEL);
	if (!sub_vcm)
		return -ENOMEM;

	init_waitqueue_head(&sub_vcm->wait_queue_head);
	LOG_INF("init_waitqueue_head done\n");

	ldo_num = ARRAY_SIZE(ldo_names);
	if (ldo_num > REGULATOR_MAXSIZE)
		ldo_num = REGULATOR_MAXSIZE;
	for (i = 0; i < ldo_num; i++) {
		sub_vcm->ldo[i] = devm_regulator_get(dev, ldo_names[i]);
		if (IS_ERR(sub_vcm->ldo[i])) {
			LOG_INF("cannot get %s regulator\n", ldo_names[i]);
			sub_vcm->ldo[i] = NULL;
		}
	}

	sub_vcm->is_powered_off = true;
	sub_vcm->is_stop_flag = false;

	sub_vcm->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(sub_vcm->vcamaf_pinctrl)) {
		ret = PTR_ERR(sub_vcm->vcamaf_pinctrl);
		sub_vcm->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		sub_vcm->vcamaf_on = pinctrl_lookup_state(
			sub_vcm->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(sub_vcm->vcamaf_on)) {
			ret = PTR_ERR(sub_vcm->vcamaf_on);
			sub_vcm->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		sub_vcm->vcamaf_off = pinctrl_lookup_state(
			sub_vcm->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(sub_vcm->vcamaf_off)) {
			ret = PTR_ERR(sub_vcm->vcamaf_off);
			sub_vcm->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	/* init work queue */
	INIT_WORK(&sub_vcm->poweroff_work, sub_vcm_poweroff_work);

	v4l2_i2c_subdev_init(&sub_vcm->sd, client, &sub_vcm_ops);
	sub_vcm->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sub_vcm->sd.internal_ops = &sub_vcm_int_ops;

	ret = sub_vcm_init_controls(sub_vcm);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&sub_vcm->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	sub_vcm->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&sub_vcm->sd);
	if (ret < 0)
		goto err_cleanup;

	return 0;

err_cleanup:
	sub_vcm_subdev_cleanup(sub_vcm);
	return ret;
}

static void sub_vcm_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sub_vcm_device *sub_vcm = sd_to_sub_vcm_vcm(sd);

	LOG_INF("+\n");

	sub_vcm_subdev_cleanup(sub_vcm);
}

static const struct i2c_device_id sub_vcm_id_table[] = {
	{ SUB_VCM_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, sub_vcm_id_table);

static const struct of_device_id sub_vcm_of_table[] = {
	{ .compatible = "mediatek,sub_vcm" },
	{ },
};
MODULE_DEVICE_TABLE(of, sub_vcm_of_table);

static struct i2c_driver sub_vcm_i2c_driver = {
	.driver = {
		.name = SUB_VCM_NAME,
		.of_match_table = sub_vcm_of_table,
	},
	.probe_new  = sub_vcm_probe,
	.remove = sub_vcm_remove,
	.id_table = sub_vcm_id_table,
};

module_i2c_driver(sub_vcm_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("SUB_VCM VCM driver");
MODULE_LICENSE("GPL");
