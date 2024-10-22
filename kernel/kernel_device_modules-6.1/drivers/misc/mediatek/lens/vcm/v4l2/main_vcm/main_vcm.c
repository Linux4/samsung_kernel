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

#define DRIVER_NAME "main_vcm"
#define MAIN_VCM_I2C_SLAVE_ADDR 0x18

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define MAIN_VCM_NAME				"main_vcm"
#define MAIN_VCM_MAX_FOCUS_POS			1023
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define MAIN_VCM_FOCUS_STEPS			1

#define REGULATOR_MAXSIZE			16

static const char * const ldo_names[] = {
	"vin",
	"vdd",
};

/* main_vcm device structure */
struct main_vcm_device {
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

static struct workqueue_struct *main_vcm_init_wq;

static DEFINE_MUTEX(main_vcm_mutex);

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

static inline struct main_vcm_device *to_main_vcm_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct main_vcm_device, ctrls);
}

static inline struct main_vcm_device *sd_to_main_vcm_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct main_vcm_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static void stop_background_works(struct main_vcm_device *main_vcm)
{
	LOG_INF("+\n");
	main_vcm->is_stop_flag = true;

	wake_up_interruptible(&main_vcm->wait_queue_head);

	/* MAIN_VCM Workqueue */
	if (main_vcm_init_wq) {
		LOG_INF("flush work queue\n");

		/* flush work queue */
		flush_work(&main_vcm->poweroff_work);

		flush_workqueue(main_vcm_init_wq);
		destroy_workqueue(main_vcm_init_wq);
		main_vcm_init_wq = NULL;
	}

	main_vcm->is_stop_flag = false;

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

static int main_vcm_set_position(struct main_vcm_device *main_vcm, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&main_vcm->sd);
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

static int main_vcm_release(struct main_vcm_device *main_vcm)
{
	int ret = 0;
	int long_step, short_step;
	int diff_dac = 0;
	int nStep_count_long = 0, nStep_count_short = 0;
	int val = 0;
	int i = 0;

	struct i2c_client *client = v4l2_get_subdevdata(&main_vcm->sd);

	long_step = g_vcmconfig.vcm_config.move_long_steps_pct *
			(1 << g_vcmconfig.vcm_config.vcm_bits) / 100;
	short_step = g_vcmconfig.vcm_config.move_short_steps_pct *
			(1 << g_vcmconfig.vcm_config.vcm_bits) / 100;

	diff_dac = g_vcmconfig.vcm_config.origin_focus_pos - main_vcm->focus->val;
	val = main_vcm->focus->val;

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

		ret = main_vcm_set_position(main_vcm, val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d\n",
				__func__, ret);
			return ret;
		}

		ret = wait_event_interruptible_timeout(main_vcm->wait_queue_head,
				main_vcm->is_stop_flag,
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

		ret = main_vcm_set_position(main_vcm, val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d\n",
				__func__, ret);
			return ret;
		}

		ret = wait_event_interruptible_timeout(main_vcm->wait_queue_head,
				main_vcm->is_stop_flag,
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
	ret = main_vcm_set_position(main_vcm, g_vcmconfig.vcm_config.origin_focus_pos);
	if (ret < 0) {
		LOG_INF("%s I2C failure: %d\n",
			__func__, ret);
		return ret;
	}

	register_setting(client, g_vcmconfig.vcm_config.wr_rls_table, 8);

	return 0;
}

static int main_vcm_init(struct main_vcm_device *main_vcm)
{
	struct i2c_client *client = v4l2_get_subdevdata(&main_vcm->sd);
	int ret = 0;

	LOG_INF("+\n");

	client->addr = g_vcmconfig.vcm_config.slave_addr >> 1;

	register_setting(client, g_vcmconfig.vcm_config.wr_table, 16);

	LOG_INF("-\n");

	return ret;
}

/* Power handling */
static int main_vcm_power_off(struct main_vcm_device *main_vcm)
{
	int ret = 0;
	int ldo_num = 0;
	int i = 0;

	LOG_INF("+\n");
	main_vcm->is_stop_flag = false;

	// delay power off feature
	LOG_INF("delay power off feature start\n");
	ret = wait_event_interruptible_timeout(main_vcm->wait_queue_head,
				main_vcm->is_stop_flag,
				msecs_to_jiffies(g_vcmconfig.vcm_config.delay_power_off_ms));
	if (ret > 0) {
		LOG_INF("interrupting, ret = %d\n", ret);
		return ret;
	}
	LOG_INF("delay power off feature end\n");

	ret = main_vcm_release(main_vcm);
	if (ret < 0) {
		LOG_INF("main_vcm release failed!\n");
	} else if (ret > 0) {
		// interrupting
		return ret;
	}
	LOG_INF("main_vcm release end\n");

	ldo_num = ARRAY_SIZE(ldo_names);
	if (ldo_num > REGULATOR_MAXSIZE)
		ldo_num = REGULATOR_MAXSIZE;
	for (i = 0; i < ldo_num; i++) {
		if (main_vcm->ldo[i]) {
			ret = regulator_disable(main_vcm->ldo[i]);
			if (ret < 0)
				LOG_INF("cannot disable %d regulator\n", i);
		}
	}

	if (main_vcm->vcamaf_pinctrl && main_vcm->vcamaf_off)
		ret = pinctrl_select_state(main_vcm->vcamaf_pinctrl,
					main_vcm->vcamaf_off);

	main_vcm->is_powered_off = true;

	LOG_INF("-\n");

	return ret;
}

static void main_vcm_poweroff_work(struct work_struct *work)
{
	struct main_vcm_device *main_vcm = NULL;
	int ret = 0;

	LOG_INF("+\n");

	main_vcm = container_of(work, struct main_vcm_device, poweroff_work);

	ret = main_vcm_power_off(main_vcm);

	LOG_INF("power off main_vcm, return value:%d\n", ret);
	LOG_INF("-\n");
}

static int main_vcm_power_on(struct main_vcm_device *main_vcm)
{
	int ret = 0;
	int ldo_num = 0;
	int i = 0;

	LOG_INF("+\n");

	ldo_num = ARRAY_SIZE(ldo_names);
	if (ldo_num > REGULATOR_MAXSIZE)
		ldo_num = REGULATOR_MAXSIZE;
	for (i = 0; i < ldo_num; i++) {
		if (main_vcm->ldo[i]) {
			ret = regulator_enable(main_vcm->ldo[i]);
			if (ret < 0)
				LOG_INF("cannot enable %d regulator\n", i);
		}
	}

	if (main_vcm->vcamaf_pinctrl && main_vcm->vcamaf_on)
		ret = pinctrl_select_state(main_vcm->vcamaf_pinctrl,
					main_vcm->vcamaf_on);

	return ret;
}

static int main_vcm_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct main_vcm_device *main_vcm = to_main_vcm_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = main_vcm_set_position(main_vcm, ctrl->val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d\n",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops main_vcm_vcm_ctrl_ops = {
	.s_ctrl = main_vcm_set_ctrl,
};

static int main_vcm_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct main_vcm_device *main_vcm = sd_to_main_vcm_vcm(sd);

	LOG_INF("+\n");

	/* Protect the Multi Process */
	mutex_lock(&main_vcm_mutex);

	stop_background_works(main_vcm);

	if (main_vcm->is_powered_off) {
		ret = main_vcm_power_on(main_vcm);
		if (ret < 0) {
			LOG_INF("power on fail, ret = %d\n", ret);
			mutex_unlock(&main_vcm_mutex);
			return ret;
		}
	} else {
		LOG_INF("no need to power on again\n");
	}

	mutex_unlock(&main_vcm_mutex);
	LOG_INF("-\n");

	return 0;
}

static int main_vcm_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct main_vcm_device *main_vcm = sd_to_main_vcm_vcm(sd);

	LOG_INF("+\n");

	/* Protect the Multi Process */
	mutex_lock(&main_vcm_mutex);

	/* MAIN_VCM Workqueue */
	if (main_vcm_init_wq == NULL) {
		LOG_INF("create_singlethread_workqueue\n");
		main_vcm_init_wq =
			create_singlethread_workqueue("main_vcm_init_work");
		if (!main_vcm_init_wq) {
			LOG_INF("create_singlethread_workqueue fail\n");
			mutex_unlock(&main_vcm_mutex);
			return -ENOMEM;
		}
	} else {
		// wait work queue done
		LOG_INF("flush work queue\n");

		/* flush work queue */
		flush_work(&main_vcm->poweroff_work);
	}

	queue_work(main_vcm_init_wq, &main_vcm->poweroff_work);
	mutex_unlock(&main_vcm_mutex);
	LOG_INF("-\n");

	return 0;
}

static long main_vcm_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct main_vcm_device *main_vcm = sd_to_main_vcm_vcm(sd);
	struct i2c_client *client = v4l2_get_subdevdata(&main_vcm->sd);

	client->addr = g_vcmconfig.vcm_config.slave_addr >> 1;

	switch (cmd) {

	case VIDIOC_MTK_S_LENS_INFO:
	{
		struct mtk_vcm_info *info = arg;

		if (main_vcm->is_powered_off == false) {
			LOG_INF("no need to init again\n");
			ret = 0;
			break;
		}

		// reset power off flag
		main_vcm->is_powered_off = false;

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

		ret = main_vcm_init(main_vcm);
		if (ret < 0)
			LOG_INF("init error\n");
	}
	break;
	case VCM_IOC_POWER_ON:
	{
		// customized area
		LOG_INF("active mode, current pos:%d\n", main_vcm->focus->val);

		register_setting(client, g_vcmconfig.vcm_config.resume_table, 8);
		main_vcm_set_position(main_vcm, main_vcm->focus->val);
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

static const struct v4l2_subdev_internal_ops main_vcm_int_ops = {
	.open = main_vcm_open,
	.close = main_vcm_close,
};

static const struct v4l2_subdev_core_ops main_vcm_ops_core = {
	.ioctl = main_vcm_ops_core_ioctl,
};

static const struct v4l2_subdev_ops main_vcm_ops = {
	.core = &main_vcm_ops_core,
};

static void main_vcm_subdev_cleanup(struct main_vcm_device *main_vcm)
{
	v4l2_async_unregister_subdev(&main_vcm->sd);
	v4l2_ctrl_handler_free(&main_vcm->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&main_vcm->sd.entity);
#endif
}

static int main_vcm_init_controls(struct main_vcm_device *main_vcm)
{
	struct v4l2_ctrl_handler *hdl = &main_vcm->ctrls;
	const struct v4l2_ctrl_ops *ops = &main_vcm_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	main_vcm->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, MAIN_VCM_MAX_FOCUS_POS, MAIN_VCM_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	main_vcm->sd.ctrl_handler = hdl;

	return 0;
}

static int main_vcm_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct main_vcm_device *main_vcm;
	int ret;
	int ldo_num;
	int i;

	LOG_INF("+\n");

	main_vcm = devm_kzalloc(dev, sizeof(*main_vcm), GFP_KERNEL);
	if (!main_vcm)
		return -ENOMEM;

	init_waitqueue_head(&main_vcm->wait_queue_head);
	LOG_INF("init_waitqueue_head done\n");

	ldo_num = ARRAY_SIZE(ldo_names);
	if (ldo_num > REGULATOR_MAXSIZE)
		ldo_num = REGULATOR_MAXSIZE;
	for (i = 0; i < ldo_num; i++) {
		main_vcm->ldo[i] = devm_regulator_get(dev, ldo_names[i]);
		if (IS_ERR(main_vcm->ldo[i])) {
			LOG_INF("cannot get %s regulator\n", ldo_names[i]);
			main_vcm->ldo[i] = NULL;
		}
	}

	main_vcm->is_powered_off = true;
	main_vcm->is_stop_flag = false;

	main_vcm->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(main_vcm->vcamaf_pinctrl)) {
		ret = PTR_ERR(main_vcm->vcamaf_pinctrl);
		main_vcm->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		main_vcm->vcamaf_on = pinctrl_lookup_state(
			main_vcm->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(main_vcm->vcamaf_on)) {
			ret = PTR_ERR(main_vcm->vcamaf_on);
			main_vcm->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		main_vcm->vcamaf_off = pinctrl_lookup_state(
			main_vcm->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(main_vcm->vcamaf_off)) {
			ret = PTR_ERR(main_vcm->vcamaf_off);
			main_vcm->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	/* init work queue */
	INIT_WORK(&main_vcm->poweroff_work, main_vcm_poweroff_work);

	v4l2_i2c_subdev_init(&main_vcm->sd, client, &main_vcm_ops);
	main_vcm->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	main_vcm->sd.internal_ops = &main_vcm_int_ops;

	ret = main_vcm_init_controls(main_vcm);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&main_vcm->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	main_vcm->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&main_vcm->sd);
	if (ret < 0)
		goto err_cleanup;

	return 0;

err_cleanup:
	main_vcm_subdev_cleanup(main_vcm);
	return ret;
}

static void main_vcm_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct main_vcm_device *main_vcm = sd_to_main_vcm_vcm(sd);

	LOG_INF("+\n");

	main_vcm_subdev_cleanup(main_vcm);
}

static const struct i2c_device_id main_vcm_id_table[] = {
	{ MAIN_VCM_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, main_vcm_id_table);

static const struct of_device_id main_vcm_of_table[] = {
	{ .compatible = "mediatek,main_vcm" },
	{ },
};
MODULE_DEVICE_TABLE(of, main_vcm_of_table);

static struct i2c_driver main_vcm_i2c_driver = {
	.driver = {
		.name = MAIN_VCM_NAME,
		.of_match_table = main_vcm_of_table,
	},
	.probe_new  = main_vcm_probe,
	.remove = main_vcm_remove,
	.id_table = main_vcm_id_table,
};

module_i2c_driver(main_vcm_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("MAIN_VCM VCM driver");
MODULE_LICENSE("GPL");
