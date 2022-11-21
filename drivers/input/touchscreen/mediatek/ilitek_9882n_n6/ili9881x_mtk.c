/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ili9881x.h"
#include "tpd.h"

#define DTS_INT_GPIO	"touch,irq-gpio"
#define DTS_RESET_GPIO	"touch,reset-gpio"
#define DTS_OF_NAME	"mediatek,9882n_touch"
#define MTK_RST_GPIO	GTP_RST_PORT
#define MTK_INT_GPIO	GTP_INT_PORT

extern struct tpd_device *tpd;
void ili_tp_reset(void)
{
	ILI_INFO("edge delay = %d\n", ilits->rst_edge_delay);

	/* Need accurate power sequence, do not change it to msleep */
	tpd_gpio_output(ilits->tp_rst, 1);
	mdelay(1);
	tpd_gpio_output(ilits->tp_rst, 0);
	mdelay(5);
	tpd_gpio_output(ilits->tp_rst, 1);
	mdelay(ilits->rst_edge_delay);
}

void ili_input_register(void)
{
	int i;

	ilits->input = tpd->dev;

	if (tpd_dts_data.use_tpd_button) {
		for (i = 0; i < tpd_dts_data.tpd_key_num; i++)
			input_set_capability(ilits->input, EV_KEY, tpd_dts_data.tpd_key_local[i]);
	}

	/* set the supported event type for input device */
	set_bit(EV_ABS, ilits->input->evbit);
	set_bit(EV_SYN, ilits->input->evbit);
	set_bit(EV_KEY, ilits->input->evbit);
	set_bit(BTN_TOUCH, ilits->input->keybit);
	set_bit(BTN_TOOL_FINGER, ilits->input->keybit);
	set_bit(INPUT_PROP_DIRECT, ilits->input->propbit);

	if (MT_PRESSURE)
		input_set_abs_params(ilits->input, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (MT_B_TYPE) {
#if KERNEL_VERSION(3, 7, 0) <= LINUX_VERSION_CODE
		input_mt_init_slots(ilits->input, MAX_TOUCH_NUM, INPUT_MT_DIRECT);
#else
		input_mt_init_slots(ilits->input, MAX_TOUCH_NUM);
#endif /* LINUX_VERSION_CODE */
	} else {
		input_set_abs_params(ilits->input, ABS_MT_TRACKING_ID, 0, MAX_TOUCH_NUM, 0, 0);
	}

	/* Gesture keys register */
	input_set_capability(ilits->input, EV_KEY, KEY_POWER);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_UP);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_DOWN);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_LEFT);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_RIGHT);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_O);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_E);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_M);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_W);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_S);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_V);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_Z);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_C);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_F);

	__set_bit(KEY_GESTURE_POWER, ilits->input->keybit);
	__set_bit(KEY_GESTURE_UP, ilits->input->keybit);
	__set_bit(KEY_GESTURE_DOWN, ilits->input->keybit);
	__set_bit(KEY_GESTURE_LEFT, ilits->input->keybit);
	__set_bit(KEY_GESTURE_RIGHT, ilits->input->keybit);
	__set_bit(KEY_GESTURE_O, ilits->input->keybit);
	__set_bit(KEY_GESTURE_E, ilits->input->keybit);
	__set_bit(KEY_GESTURE_M, ilits->input->keybit);
	__set_bit(KEY_GESTURE_W, ilits->input->keybit);
	__set_bit(KEY_GESTURE_S, ilits->input->keybit);
	__set_bit(KEY_GESTURE_V, ilits->input->keybit);
	__set_bit(KEY_GESTURE_Z, ilits->input->keybit);
	__set_bit(KEY_GESTURE_C, ilits->input->keybit);
	__set_bit(KEY_GESTURE_F, ilits->input->keybit);
}

#if REGULATOR_POWER
void ili_plat_regulator_power_on(bool status)
{
	ILI_INFO("%s\n", status ? "POWER ON" : "POWER OFF");

	if (status) {
		if (ilits->vdd) {
			if (regulator_enable(ilits->vdd) < 0)
				ILI_ERR("regulator_enable VDD fail\n");
		}
		if (ilits->vcc) {
			if (regulator_enable(ilits->vcc) < 0)
				ILI_ERR("regulator_enable VCC fail\n");
		}
	} else {
		if (ilits->vdd) {
			if (regulator_disable(ilits->vdd) < 0)
				ILI_ERR("regulator_enable VDD fail\n");
		}
		if (ilits->vcc) {
			if (regulator_disable(ilits->vcc) < 0)
				ILI_ERR("regulator_enable VCC fail\n");
		}
	}
	atomic_set(&ilits->ice_stat, DISABLE);
	mdelay(5);
}

static void ilitek_plat_regulator_power_init(void)
{
	const char *vdd_name = "vdd";
	const char *vcc_name = "vcc";

	ilits->vdd = regulator_get(tpd->tpd_dev, vdd_name);
	if (ERR_ALLOC_MEM(ilits->vdd)) {
		ILI_ERR("regulator_get VDD fail\n");
		ilits->vdd = NULL;
	}

	tpd->reg = ilits->vdd;

	if (regulator_set_voltage(ilits->vdd, VDD_VOLTAGE, VDD_VOLTAGE) < 0)
		ILI_ERR("Failed to set VDD %d\n", VDD_VOLTAGE);

	ilits->vcc = regulator_get(ilits->dev, vcc_name);
	if (ERR_ALLOC_MEM(ilits->vcc)) {
		ILI_ERR("regulator_get VCC fail.\n");
		ilits->vcc = NULL;
	}
	if (regulator_set_voltage(ilits->vcc, VCC_VOLTAGE, VCC_VOLTAGE) < 0)
		ILI_ERR("Failed to set VCC %d\n", VCC_VOLTAGE);

	ili_plat_regulator_power_on(true);
}
#endif

static int ilitek_plat_gpio_register(void)
{
	int ret = 0;

	ilits->tp_int = MTK_INT_GPIO;
	ilits->tp_rst = MTK_RST_GPIO;

	ILI_INFO("TP INT: %d\n", ilits->tp_int);
	ILI_INFO("TP RESET: %d\n", ilits->tp_rst);
	return ret;
}

void ili_irq_disable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ilits->irq_spin, flag);

	if (atomic_read(&ilits->irq_stat) == DISABLE)
		goto out;

	if (!ilits->irq_num) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", ilits->irq_num);
		goto out;
	}

	disable_irq_nosync(ilits->irq_num);
	atomic_set(&ilits->irq_stat, DISABLE);
	ILI_DBG("Disable irq success\n");

out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

void ili_irq_enable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ilits->irq_spin, flag);

	if (atomic_read(&ilits->irq_stat) == ENABLE)
		goto out;

	if (!ilits->irq_num) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", ilits->irq_num);
		goto out;
	}

	enable_irq(ilits->irq_num);
	atomic_set(&ilits->irq_stat, ENABLE);
	ILI_DBG("Enable irq success\n");

out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

static irqreturn_t ilitek_plat_isr_top_half(int irq, void *dev_id)
{
	if (irq != ilits->irq_num) {
		ILI_ERR("Incorrect irq number (%d)\n", irq);
		return IRQ_NONE;
	}

	if (atomic_read(&ilits->cmd_int_check) == ENABLE) {
		atomic_set(&ilits->cmd_int_check, DISABLE);
		ILI_DBG("CMD INT detected, ignore\n");
		wake_up(&(ilits->inq));
		return IRQ_HANDLED;
	}

	if (ilits->prox_near) {
		ILI_INFO("Proximity event, ignore interrupt!\n");
		return IRQ_HANDLED;
	}

	ILI_DBG("report: %d, rst: %d, fw: %d, switch: %d, mp: %d, sleep: %d, esd: %d\n",
			ilits->report,
			atomic_read(&ilits->tp_reset),
			atomic_read(&ilits->fw_stat),
			atomic_read(&ilits->tp_sw_mode),
			atomic_read(&ilits->mp_stat),
			atomic_read(&ilits->tp_sleep),
			atomic_read(&ilits->esd_stat));

	if (!ilits->report || atomic_read(&ilits->tp_reset) ||
		atomic_read(&ilits->fw_stat) || atomic_read(&ilits->tp_sw_mode) ||
		atomic_read(&ilits->mp_stat) || atomic_read(&ilits->tp_sleep) ||
		atomic_read(&ilits->esd_stat)) {
			ILI_DBG("ignore interrupt !\n");
			return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ilitek_plat_isr_bottom_half(int irq, void *dev_id)
{
	if (mutex_is_locked(&ilits->touch_mutex)) {
		ILI_DBG("touch is locked, ignore\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&ilits->touch_mutex);
	ili_report_handler();
	mutex_unlock(&ilits->touch_mutex);
	return IRQ_HANDLED;
}

void ili_irq_unregister(void)
{
	devm_free_irq(ilits->dev, ilits->irq_num, NULL);
}

int ili_irq_register(int type)
{
	int ret = 0;
	static bool get_irq_pin;
	struct device_node *node;

	atomic_set(&ilits->irq_stat, DISABLE);

	if (get_irq_pin == false) {
		node = of_find_matching_node(NULL, touch_of_match);
		if (node)
			ilits->irq_num = irq_of_parse_and_map(node, 0);

		ILI_INFO("ilits->irq_num = %d\n", ilits->irq_num);
		get_irq_pin = true;
	}

	ret = devm_request_threaded_irq(ilits->dev, ilits->irq_num,
				ilitek_plat_isr_top_half,
				ilitek_plat_isr_bottom_half,
				type | IRQF_ONESHOT, "ilitek", NULL);

	if (type == IRQF_TRIGGER_FALLING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_FALLING\n");
	if (type == IRQF_TRIGGER_RISING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_RISING\n");

	if (ret != 0)
		ILI_ERR("Failed to register irq handler, irq = %d, ret = %d\n", ilits->irq_num, ret);

	atomic_set(&ilits->irq_stat, ENABLE);

	return ret;
}

static void tpd_resume(struct device *h)
{
	if (ili_sleep_handler(TP_RESUME) < 0)
		ILI_ERR("TP resume failed\n");
}

static void tpd_suspend(struct device *h)
{
	if (ili_sleep_handler(TP_SUSPEND) < 0)
		ILI_ERR("TP suspend failed\n");
}

#ifdef SEC_TSP_FACTORY_TEST
extern struct ilitek_ts_data *ilits;

static void get_fw_ver_bin(void *device_data)
{


	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[64] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "ILI9882N,VND:TXD,FW:%d.%d.%d.%d",
		 ilits->fw_info[48], ilits->fw_info[49], ilits->fw_info[50],
		 ilits->fw_info[51]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	ILI_INFO("buff: %s, sec->cmd_state == %d\n", buff, sec->cmd_state);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ili_ic_get_fw_ver() < 0)
		ILI_ERR("Failed to read TP fw  info\n");

	snprintf(buff, sizeof(buff), "ILI9882N,VND:TXD,FW:%d.%d.%d.%d",
		(ilits->chip->fw_ver & 0xff000000) >> 24, (ilits->chip->fw_ver & 0xff0000) >> 16,
		(ilits->chip->fw_ver & 0xff00) >> 8, ilits->chip->fw_ver & 0xff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	ILI_INFO("buff: %s,sec->cmd_state = %d\n", buff, sec->cmd_state);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[64] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	ILI_INFO("buff: %s\n", buff);
}

struct sec_cmd ft_commands[] = {
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};
#endif

#ifdef  ILITEK_TOUCHSCREEN_LOCKDOWN_INFO
struct proc_dir_entry *ilitek_proc_lock_down_info_wt;
static u8 lockdown_info[8];
unsigned char g_tp_color = 0x00;

static int get_tp_color(void)
{
	int ret = 0;
	char *tp_color_start=NULL;
        char *temp;
	char tp_color[8]={'\0'};

	/*parse tp color from command line*/
	tp_color_start = strstr(saved_command_line, "tp_color=");
	 if(tp_color_start == NULL){
		ILI_ERR("command_line have non tp_color info.\n");
		return -1;
	 }

	temp = tp_color_start + strlen("tp_color=");
	memcpy(tp_color, temp, 4);
        ILI_INFO("Read tp_color from command_line is %s.\n", tp_color);

	/* convert tp color string to unsigned int*/
	ret =  kstrtou8(tp_color, 0, &g_tp_color);
	if (ret != 0){
		ILI_ERR("Convert tp color string to unsigned int error.\n");
		return -EINVAL;
	}
	ILI_INFO("Tp color is : 0x%02x.\n", g_tp_color);

       return ret;
}
static ssize_t ctp_lockdown_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
{
        char *ptr;
        int ret;

        if(*ppos){
           ILI_ERR("tp test again return\n");
            return 0;
        }

        *ppos += count;
        ptr = kzalloc(count, GFP_KERNEL);

        /*
       * get lock down info
       */
        memset(lockdown_info, 0, sizeof(lockdown_info));
	get_tp_color(); //get tp color
	lockdown_info[2] = g_tp_color;

        ILI_INFO("%s:Get tp color is:0x%02x.\n", __func__, g_tp_color);

	ret=  sprintf(ptr, "%02X%02X%02X%02X%02X%02X%02X%02X\n",
				   lockdown_info[0], lockdown_info[1], lockdown_info[2], lockdown_info[3],
				   lockdown_info[4], lockdown_info[5], lockdown_info[6], lockdown_info[7]);

	if (copy_to_user(buf, ptr, ret+1)) {
                ILI_ERR("%s: fail to copy default config\n", __func__);
                ret = -EFAULT;
        }

	kfree(ptr);

        return  ret;
}
static ssize_t ctp_lockdown_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos)
{
    return -1;
}
static const struct file_operations ilitek_proc_lockdown_info_ops = {
    .write = ctp_lockdown_proc_write,
    .read = ctp_lockdown_proc_read,
    .owner = THIS_MODULE,
};
#endif

#if WINGTECH_TP_OPENSHORT_EN
#define CTP_PARENT_PROC_NAME  "touchscreen"
#define CTP_OPENSHORT_PROC_NAME "ctp_openshort_test"
extern int wt_tp_openshort(void);

static ssize_t ctp_open_proc_write(struct file *file,const char __user *userbuf,size_t count,loff_t *ppos)
{
	return -1;
}
static ssize_t ctp_open_proc_read(struct file *file,char __user *buf,size_t count,loff_t *ppos)
{
	int len = count;

	if(*ppos){
		ILI_ERR("ctp test again return\n");
		return 0;
	}
	*ppos += count;
	/* get test results start */
	ilits->wt_result = wt_tp_openshort();
	/* get test results end */
	if(count > 9)
		len = 9;
	if(ilits->wt_result == 0){
		if(copy_to_user(buf,"result=1\n",len)){
			ILI_ERR("copy_to_user fail\n");
			return -1;
		}
	} else {
		if (copy_to_user(buf,"result=0\n",len)) {
			ILI_ERR("copy_to_user fail\n");
			return -1;
		}
	}
	return len;
}

static struct file_operations ctp_open_procs_fops = {
	.write = ctp_open_proc_write,
	.read = ctp_open_proc_read,
	.owner = THIS_MODULE,
};

static struct proc_dir_entry *ctp_device_proc = NULL;
static void create_ctp_proc(void)
{
	struct proc_dir_entry *ctp_open_proc = NULL;

	ILI_INFO("openshort create_ctp_proc \n");
	if (ctp_device_proc == NULL) {
		ctp_device_proc = proc_mkdir(CTP_PARENT_PROC_NAME,NULL);
		if (ctp_device_proc == NULL) {
			ILI_ERR("create parent_proc fail\n");
			return;
		}
	}
	ctp_open_proc = proc_create(CTP_OPENSHORT_PROC_NAME,0777,ctp_device_proc,&ctp_open_procs_fops);
	if (ctp_open_proc == NULL) {
		ILI_ERR("create open_proc fail\n");
		return;
	}

#ifdef  ILITEK_TOUCHSCREEN_LOCKDOWN_INFO
	ilitek_proc_lock_down_info_wt = proc_create(ILITEK_PROC_LOCK_DOWN_INFO_FILE_WT, 0777, ctp_device_proc, &ilitek_proc_lockdown_info_ops);
	if (ilitek_proc_lock_down_info_wt == NULL) {
		ILI_ERR(" proc lock_down info file create failed!\n");
		return;
	}
#endif

}
#endif




static int ilitek_tp_pm_suspend(struct device *dev)
{
	ILI_INFO("CALL BACK TP PM SUSPEND");
	ilits->pm_suspend = true;
	reinit_completion(&ilits->pm_completion);
	return 0;
}

static int ilitek_tp_pm_resume(struct device *dev)
{
	ILI_INFO("CALL BACK TP PM RESUME");
	ilits->pm_suspend = false;
	complete(&ilits->pm_completion);
	return 0;
}

#if SPRD_SYSFS_SUSPEND_RESUME
static ssize_t ts_suspend_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", ilits->tp_suspend ? "true" : "false");
}

static ssize_t ts_suspend_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] == '1') && !ilits->tp_suspend)
		ili_sleep_handler(TP_DEEP_SLEEP);
	else if ((buf[0] == '0') && ilits->tp_suspend)
		ili_sleep_handler(TP_RESUME);

	return count;
}
static DEVICE_ATTR_RW(ts_suspend);

static struct attribute *ilitek_dev_suspend_atts[] = {
	&dev_attr_ts_suspend.attr,
	NULL
};

static const struct attribute_group ilitek_dev_suspend_atts_group = {
	.attrs = ilitek_dev_suspend_atts,
};

static const struct attribute_group *ilitek_dev_attr_groups[] = {
	&ilitek_dev_suspend_atts_group,
	NULL
};

int ili_sysfs_add_device(struct device *dev) {
	int ret = 0, i;

	for (i = 0; ilitek_dev_attr_groups[i]; i++) {
		ret = sysfs_create_group(&dev->kobj, ilitek_dev_attr_groups[i]);
		if (ret) {
			while (--i >= 0) {
				sysfs_remove_group(&dev->kobj, ilitek_dev_attr_groups[i]);
			}
			break;
		}
	}

	return ret;
}

int ili_sysfs_remove_device(struct device *dev) {
	int i;

	sysfs_remove_link(NULL, "touchscreen");
	for (i = 0; ilitek_dev_attr_groups[i]; i++) {
		sysfs_remove_group(&dev->kobj, ilitek_dev_attr_groups[i]);
	}

	return 0;
}
#endif

static int ilitek_plat_probe(void)
{
#ifdef SEC_TSP_FACTORY_TEST
	int ret;
#endif

	ILI_INFO("platform probe\n");

#if REGULATOR_POWER
	ilitek_plat_regulator_power_init();
#endif

	if (ilitek_plat_gpio_register() < 0)
		ILI_ERR("Register gpio failed\n");

	ili_irq_register(ilits->irq_tirgger_type);

	if (ili_tddi_init() < 0) {
		ILI_ERR("ILITEK Driver probe failed\n");
		ili_irq_unregister();
		return -ENODEV;
	}	
	tpd_load_status = 1;
	ilits->pm_suspend = false;
	init_completion(&ilits->pm_completion);

#if SPRD_SYSFS_SUSPEND_RESUME
	ili_sysfs_add_device(ilits->dev);
	if (sysfs_create_link(NULL, &ilits->dev->kobj, "touchscreen") < 0)
		ILI_INFO("Failed to create link!\n");
#endif

#if WINGTECH_TP_OPENSHORT_EN
			create_ctp_proc();
#endif

#ifdef SEC_TSP_FACTORY_TEST
	ret = sec_cmd_init(&ilits->sec, ft_commands, ARRAY_SIZE(ft_commands),
		SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		ILI_ERR("%s: Failed to sec_cmd_init\n", __func__);
		return -ENODEV;
	}
#endif

	ILI_INFO("ILITEK Driver loaded successfully!");
	return 0;
}

static int ilitek_plat_remove(void)
{
	ILI_INFO("remove plat dev\n");
#if SPRD_SYSFS_SUSPEND_RESUME
	ili_sysfs_remove_device(ilits->dev);
#endif
	ili_dev_remove();

#ifdef SEC_TSP_FACTORY_TEST
	sec_cmd_exit(&ilits->sec, SEC_CLASS_DEVT_TSP);
#endif

	return 0;
}

static const struct dev_pm_ops tp_pm_ops = {
	.suspend = ilitek_tp_pm_suspend,
	.resume = ilitek_tp_pm_resume,
};

static const struct of_device_id tp_match_table[] = {
	{.compatible = DTS_OF_NAME},
	{},
};

static struct ilitek_hwif_info hwif = {
	.bus_type = TDDI_INTERFACE,
	.plat_type = TP_PLAT_MTK,
	.owner = THIS_MODULE,
	.name = TDDI_DEV_ID,
	.of_match_table = of_match_ptr(tp_match_table),
	.plat_probe = ilitek_plat_probe,
	.plat_remove = ilitek_plat_remove,
	.pm = &tp_pm_ops,
};

static int tpd_local_init(void)
{
	ILI_INFO("TPD init device driver\n");

	if (ili_dev_init(&hwif) < 0) {
		ILI_ERR("Failed to register i2c/spi bus driver\n");
		return -ENODEV;
	}
	if (tpd_load_status == 0) {
		ILI_ERR("Add error touch panel driver\n");
		return -1;
	}
	if (tpd_dts_data.use_tpd_button) {
		tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
				   tpd_dts_data.tpd_key_dim_local);
	}
	tpd_type_cap = 1;
	return 0;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = TDDI_DEV_ID,
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
};

static int __init ilitek_plat_dev_init(void)
{
	int ret = 0;

	ILI_INFO("ILITEK TP driver init for MTK\n");
	tpd_get_dts_info();
	ret = tpd_driver_add(&tpd_device_driver);
	if (ret < 0) {
		ILI_ERR("ILITEK add TP driver failed\n");
		tpd_driver_remove(&tpd_device_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit ilitek_plat_dev_exit(void)
{
	ILI_INFO("ilitek driver has been removed\n");
	tpd_driver_remove(&tpd_device_driver);
}

module_init(ilitek_plat_dev_init);
module_exit(ilitek_plat_dev_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");
