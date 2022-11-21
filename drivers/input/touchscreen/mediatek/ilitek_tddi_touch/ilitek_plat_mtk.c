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

#include "ilitek.h"
#include "ilitek_fw.h"
#include "tpd.h"
#ifdef SEC_TSP_FACTORY_TEST
#include <linux/input/sec_cmd.h>
#endif

#define DTS_INT_GPIO	"touch,irq-gpio"
#define DTS_RESET_GPIO	"touch,reset-gpio"
#define DTS_OF_NAME		"mediatek,cap_touch"
#define MTK_RST_GPIO	GTP_RST_PORT
#define MTK_INT_GPIO	GTP_INT_PORT

extern struct tpd_device *tpd;

//+add by songbinbo.wt for ILITEK tp driver compatible truly & txd vendor  20191005
unsigned char *CTPM_FW = NULL;

uint32_t name_modue = -1;
uint32_t size_ili = 0;

struct module_name_use{
	char *module_ini_name;
	char *module_bin_name;
	uint32_t size_ili;
};

struct module_name_use module_name_is_use[] = {
	{.module_ini_name = XL_ROW_INI_NAME_PATH, .module_bin_name = XL_ROW_MODULE_NAME_PATH, .size_ili = sizeof(CTPM_FW_XL_ROW)},
        {.module_ini_name = XL_NA_INI_NAME_PATH, .module_bin_name = XL_NA_MODULE_NAME_PATH, .size_ili = sizeof(CTPM_FW_XL_NA)},
	{.module_ini_name = TXD_ROW_INI_NAME_PATH, .module_bin_name = TXD_ROW_MODULE_NAME_PATH, .size_ili = sizeof(CTPM_FW_TXD_ROW)},
	{.module_ini_name = TXD_NA_INI_NAME_PATH, .module_bin_name = TXD_NA_MODULE_NAME_PATH, .size_ili = sizeof(CTPM_FW_TXD_NA)},
	{.module_ini_name = XL_ROW_INI_NAME_PATH, .module_bin_name = XL_MODULE_NAME_PATH, .size_ili = sizeof(CTPM_FW_XL)},
	{.module_ini_name = "/sdcard/mp.ini", .module_bin_name = "ILITEK_FW", .size_ili = sizeof(CTPM_FW_NONE)},
};
//-add by songbinbo.wt for ILITEK tp driver compatible truly & txd vendor  20191005

void ilitek_plat_tp_reset(void)
{
	ipio_info("edge delay = %d\n", idev->rst_edge_delay);

	/* Need accurate power sequence, do not change it to msleep */
	tpd_gpio_output(idev->tp_rst, 1);
	mdelay(1);
	tpd_gpio_output(idev->tp_rst, 0);
	mdelay(5);
	tpd_gpio_output(idev->tp_rst, 1);
	mdelay(idev->rst_edge_delay);
}

void ilitek_plat_input_register(void)
{
	int i;

	idev->input = tpd->dev;

	if (tpd_dts_data.use_tpd_button) {
		for (i = 0; i < tpd_dts_data.tpd_key_num; i++)
			input_set_capability(idev->input, EV_KEY, tpd_dts_data.tpd_key_local[i]);
	}

	idev->input->name = TDDI_DEV_ID;
	/* set the supported event type for input device */
	set_bit(EV_ABS, idev->input->evbit);
	set_bit(EV_SYN, idev->input->evbit);
	set_bit(EV_KEY, idev->input->evbit);
	set_bit(BTN_TOUCH, idev->input->keybit);
	set_bit(BTN_TOOL_FINGER, idev->input->keybit);
	set_bit(INPUT_PROP_DIRECT, idev->input->propbit);

	if (MT_PRESSURE)
		input_set_abs_params(idev->input, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (MT_B_TYPE) {
#if KERNEL_VERSION(3, 7, 0) <= LINUX_VERSION_CODE
		input_mt_init_slots(idev->input, MAX_TOUCH_NUM, INPUT_MT_DIRECT);
#else
		input_mt_init_slots(idev->input, MAX_TOUCH_NUM);
#endif /* LINUX_VERSION_CODE */
	} else {
		input_set_abs_params(idev->input, ABS_MT_TRACKING_ID, 0, MAX_TOUCH_NUM, 0, 0);
	}

	/* Gesture keys register */
	input_set_capability(idev->input, EV_KEY, KEY_POWER);
	input_set_capability(idev->input, EV_KEY, GESTURE_DOUBLE_TAP);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_UP);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_DOWN);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_LEFT);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_RIGHT);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_O);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_E);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_M);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_W);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_S);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_V);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_Z);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_C);
	input_set_capability(idev->input, EV_KEY, KEY_GESTURE_F);

	__set_bit(KEY_GESTURE_POWER, idev->input->keybit);
	__set_bit(GESTURE_DOUBLE_TAP, idev->input->keybit);
	__set_bit(KEY_GESTURE_UP, idev->input->keybit);
	__set_bit(KEY_GESTURE_DOWN, idev->input->keybit);
	__set_bit(KEY_GESTURE_LEFT, idev->input->keybit);
	__set_bit(KEY_GESTURE_RIGHT, idev->input->keybit);
	__set_bit(KEY_GESTURE_O, idev->input->keybit);
	__set_bit(KEY_GESTURE_E, idev->input->keybit);
	__set_bit(KEY_GESTURE_M, idev->input->keybit);
	__set_bit(KEY_GESTURE_W, idev->input->keybit);
	__set_bit(KEY_GESTURE_S, idev->input->keybit);
	__set_bit(KEY_GESTURE_V, idev->input->keybit);
	__set_bit(KEY_GESTURE_Z, idev->input->keybit);
	__set_bit(KEY_GESTURE_C, idev->input->keybit);
	__set_bit(KEY_GESTURE_F, idev->input->keybit);
}

#if REGULATOR_POWER
void ilitek_plat_regulator_power_on(bool status)
{
	ipio_info("%s\n", status ? "POWER ON" : "POWER OFF");

	if (status) {
		if (idev->vdd) {
			if (regulator_enable(idev->vdd) < 0)
				ipio_err("regulator_enable VDD fail\n");
		}
		if (idev->vcc) {
			if (regulator_enable(idev->vcc) < 0)
				ipio_err("regulator_enable VCC fail\n");
		}
	} else {
		if (idev->vdd) {
			if (regulator_disable(idev->vdd) < 0)
				ipio_err("regulator_enable VDD fail\n");
		}
		if (idev->vcc) {
			if (regulator_disable(idev->vcc) < 0)
				ipio_err("regulator_enable VCC fail\n");
		}
	}
	atomic_set(&idev->ice_stat, DISABLE);
	mdelay(5);
}
#endif

#if REGULATOR_POWER
static void ilitek_plat_regulator_power_init(void)
{
	const char *vdd_name = "vdd";
	const char *vcc_name = "vcc";

	idev->vdd = regulator_get(tpd->tpd_dev, vdd_name);
	if (ERR_ALLOC_MEM(idev->vdd)) {
		ipio_err("regulator_get VDD fail\n");
		idev->vdd = NULL;
	}

	tpd->reg = idev->vdd;

	if (regulator_set_voltage(idev->vdd, VDD_VOLTAGE, VDD_VOLTAGE) < 0)
		ipio_err("Failed to set VDD %d\n", VDD_VOLTAGE);

	idev->vcc = regulator_get(idev->dev, vcc_name);
	if (ERR_ALLOC_MEM(idev->vcc)) {
		ipio_err("regulator_get VCC fail.\n");
		idev->vcc = NULL;
	}
	if (regulator_set_voltage(idev->vcc, VCC_VOLTAGE, VCC_VOLTAGE) < 0)
		ipio_err("Failed to set VCC %d\n", VCC_VOLTAGE);

	ilitek_plat_regulator_power_on(true);
}
#endif

static int ilitek_plat_gpio_register(void)
{
	int ret = 0;

	idev->tp_int = MTK_INT_GPIO;
	idev->tp_rst = MTK_RST_GPIO;

	ipio_info("TP INT: %d\n", idev->tp_int);
	ipio_info("TP RESET: %d\n", idev->tp_rst);

	if (!gpio_is_valid(idev->tp_int)) {
		ipio_err("Invalid INT gpio: %d\n", idev->tp_int);
		return -EBADR;
	}

	if (!gpio_is_valid(idev->tp_rst)) {
		ipio_err("Invalid RESET gpio: %d\n", idev->tp_rst);
		return -EBADR;
	}

	ret = gpio_request(idev->tp_int, "TP_INT");
	if (ret < 0) {
		ipio_err("Request IRQ GPIO failed, ret = %d\n", ret);
		gpio_free(idev->tp_int);
		ret = gpio_request(idev->tp_int, "TP_INT");
		if (ret < 0) {
			ipio_err("Retrying request INT GPIO still failed , ret = %d\n", ret);
			goto out;
		}
	}

	ret = gpio_request(idev->tp_rst, "TP_RESET");
	if (ret < 0) {
		ipio_err("Request RESET GPIO failed, ret = %d\n", ret);
		gpio_free(idev->tp_rst);
		ret = gpio_request(idev->tp_rst, "TP_RESET");
		if (ret < 0) {
			ipio_err("Retrying request RESET GPIO still failed , ret = %d\n", ret);
			goto out;
		}
	}

out:
	gpio_direction_input(idev->tp_int);
	return ret;
}

void ilitek_plat_irq_disable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&idev->irq_spin, flag);

	if (atomic_read(&idev->irq_stat) == DISABLE)
		goto out;

	if (!idev->irq_num) {
		ipio_err("gpio_to_irq (%d) is incorrect\n", idev->irq_num);
		goto out;
	}

	disable_irq_nosync(idev->irq_num);
	atomic_set(&idev->irq_stat, DISABLE);
	ipio_debug("Disable irq success\n");

out:
	spin_unlock_irqrestore(&idev->irq_spin, flag);
}

void ilitek_plat_irq_enable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&idev->irq_spin, flag);

	if (atomic_read(&idev->irq_stat) == ENABLE)
		goto out;

	if (!idev->irq_num) {
		ipio_err("gpio_to_irq (%d) is incorrect\n", idev->irq_num);
		goto out;
	}

	enable_irq(idev->irq_num);
	atomic_set(&idev->irq_stat, ENABLE);
	ipio_debug("Enable irq success\n");

out:
	spin_unlock_irqrestore(&idev->irq_spin, flag);
}

static irqreturn_t ilitek_plat_isr_top_half(int irq, void *dev_id)
{
	if (irq != idev->irq_num) {
		ipio_err("Incorrect irq number (%d)\n", irq);
		return IRQ_NONE;
	}

	if (atomic_read(&idev->mp_int_check) == ENABLE) {
		atomic_set(&idev->mp_int_check, DISABLE);
		ipio_debug("interrupt for mp test, ignore\n");
		wake_up(&(idev->inq));
		return IRQ_HANDLED;
	}

	if (idev->prox_near) {
		ipio_info("Proximity event, ignore interrupt!\n");
		return IRQ_HANDLED;
	}

	ipio_debug("report: %d, rst: %d, fw: %d, switch: %d, mp: %d, sleep: %d, esd: %d\n",
			idev->report,
			atomic_read(&idev->tp_reset),
			atomic_read(&idev->fw_stat),
			atomic_read(&idev->tp_sw_mode),
			atomic_read(&idev->mp_stat),
			atomic_read(&idev->tp_sleep),
			atomic_read(&idev->esd_stat));

	if (!idev->report || atomic_read(&idev->tp_reset) ||
		atomic_read(&idev->fw_stat) || atomic_read(&idev->tp_sw_mode) ||
		atomic_read(&idev->mp_stat) || atomic_read(&idev->tp_sleep) ||
		atomic_read(&idev->esd_stat)) {
			ipio_debug("ignore interrupt !\n");
			return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ilitek_plat_isr_bottom_half(int irq, void *dev_id)
{
	if (mutex_is_locked(&idev->touch_mutex)) {
		ipio_debug("touch is locked, ignore\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&idev->touch_mutex);
	ilitek_tddi_report_handler();
	mutex_unlock(&idev->touch_mutex);
	return IRQ_HANDLED;
}

void ilitek_plat_irq_unregister(void)
{
	devm_free_irq(idev->dev, idev->irq_num, NULL);
}

int ilitek_plat_irq_register(int type)
{
	int ret = 0;
	static bool get_irq_pin;
	struct device_node *node;

	atomic_set(&idev->irq_stat, DISABLE);

	if (get_irq_pin == false) {
		node = of_find_matching_node(NULL, touch_of_match);
		if (node)
			idev->irq_num = irq_of_parse_and_map(node, 0);

		ipio_info("idev->irq_num = %d\n", idev->irq_num);
		get_irq_pin = true;
	}

	ret = devm_request_threaded_irq(idev->dev, idev->irq_num,
				ilitek_plat_isr_top_half,
				ilitek_plat_isr_bottom_half,
				type | IRQF_ONESHOT, "ilitek", NULL);

	if (type == IRQF_TRIGGER_FALLING)
		ipio_info("IRQ TYPE = IRQF_TRIGGER_FALLING\n");
	if (type == IRQF_TRIGGER_RISING)
		ipio_info("IRQ TYPE = IRQF_TRIGGER_RISING\n");

	if (ret != 0)
		ipio_err("Failed to register irq handler, irq = %d, ret = %d\n", idev->irq_num, ret);

	atomic_set(&idev->irq_stat, ENABLE);

	return ret;
}

static void tpd_resume(struct device *h)
{
	if (ilitek_tddi_sleep_handler(TP_RESUME) < 0)
		ipio_err("TP resume failed\n");
}

static void tpd_suspend(struct device *h)
{
	if (ilitek_tddi_sleep_handler(TP_SUSPEND) < 0)
		ipio_err("TP suspend failed\n");
}

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
		ipio_err("command_line have non tp_color info.\n");
		return -1;
	 }

	temp = tp_color_start + strlen("tp_color=");
	memcpy(tp_color, temp, 4);
        ipio_info("Read tp_color from command_line is %s.\n", tp_color);

	/* convert tp color string to unsigned int*/
	ret =  kstrtou8(tp_color, 0, &g_tp_color);
	if (ret != 0){
		ipio_err("Convert tp color string to unsigned int error.\n");
		return -EINVAL;
	}
	ipio_info("Tp color is : 0x%02x.\n", g_tp_color);

       return ret;
}

static ssize_t ctp_lockdown_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
{
        char *ptr;
        int ret;

        if(*ppos){
           ipio_err("tp test again return\n");
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

        ipio_info("%s:Get tp color is:0x%02x.\n", __func__, g_tp_color);

	ret=  sprintf(ptr, "%02X%02X%02X%02X%02X%02X%02X%02X\n",
				   lockdown_info[0], lockdown_info[1], lockdown_info[2], lockdown_info[3],
				   lockdown_info[4], lockdown_info[5], lockdown_info[6], lockdown_info[7]);

	if (copy_to_user(buf, ptr, ret+1)) {
                ipio_err("%s: fail to copy default config\n", __func__);
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
	int result = 1;
	//int ret;
	int len = count;

	if(*ppos){
		ipio_err("ctp test again return\n");
		return 0;
	}
	*ppos += count;
	/* get test results start */
	result = wt_tp_openshort();
	/* get test results end */
	if(count > 9)
		len = 9;
	if(idev->wt_result == 0){
		if(copy_to_user(buf,"result=1\n",len)){
			ipio_err("copy_to_user fail\n");
			return -1;
		}
	}else{
		if(copy_to_user(buf,"result=0\n",len)){
			ipio_err("copy_to_user fail\n");
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

	ipio_info("openshort create_ctp_proc \n");
	if(ctp_device_proc == NULL){
		ctp_device_proc = proc_mkdir(CTP_PARENT_PROC_NAME,NULL);
		if(ctp_device_proc == NULL){
			ipio_err("create parent_proc fail\n");
			return;
		}
	}
	ctp_open_proc = proc_create(CTP_OPENSHORT_PROC_NAME,0777,ctp_device_proc,&ctp_open_procs_fops);
	if(ctp_open_proc == NULL){
		ipio_err("create open_proc fail\n");
		return;
	}

#ifdef  ILITEK_TOUCHSCREEN_LOCKDOWN_INFO
	ilitek_proc_lock_down_info_wt = proc_create(ILITEK_PROC_LOCK_DOWN_INFO_FILE_WT, 0777, ctp_device_proc, &ilitek_proc_lockdown_info_ops);
	if (ilitek_proc_lock_down_info_wt == NULL)
	{
		ipio_err(" proc lock_down info file create failed!\n");
		return;
	}
#endif

}
#endif
//+add by songbinbo.wt for ILITEK tp driver compatible truly & txd vendor  20191005

#ifdef SEC_TSP_FACTORY_TEST
extern struct touch_fw_data tfd;
extern char * ven_name[];
extern uint32_t name_modue;
extern struct ilitek_tddi_dev *idev;
extern bool gestrue_status;

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[64] = { 0 };

	sec_cmd_set_default_result(sec);
	ipio_info("new_fw_cb = 0x%x\n", tfd.new_fw_cb);

	snprintf(buff, sizeof(buff) ,"ILI7807G,VND:%s,FW:%d.%d.%d.%d",ven_name[name_modue],\
		 (tfd.new_fw_cb&0xff000000)>>24, (tfd.new_fw_cb&0xff0000)>>16, (tfd.new_fw_cb&0xff00)>>8, tfd.new_fw_cb&0xff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
        ipio_info( "%s: %s,sec->cmd_state == %d\n", __func__, buff, sec->cmd_state);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilitek_tddi_ic_get_fw_ver() < 0)
		ipio_err("Failed to read TP fw  info\n");

	snprintf(buff, sizeof(buff) ,"ILI7807G,VND:%s,FW:%d.%d.%d.%d",ven_name[name_modue],\
			  (idev->chip->fw_ver&0xff000000)>>24, (idev->chip->fw_ver&0xff0000)>>16, (idev->chip->fw_ver&0xff00)>>8, idev->chip->fw_ver&0xff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
        ipio_info( "%s: %s,sec->cmd_state == %d\n", __func__, buff, sec->cmd_state);
}

static void aot_enable(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    unsigned int buf[4];

    ipio_info("aot_enable Enter.\n");
    sec_cmd_set_default_result(sec);

    if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
        snprintf(buff, sizeof(buff), "NG");
        sec->cmd_state = SEC_CMD_STATUS_FAIL;
        sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
        ipio_err( "%s: sec->cmd_param[0] == %s, buff == %s,sec->cmd_state == %d\n", __func__, sec->cmd_param[0], buff, sec->cmd_state);
        sec_cmd_set_cmd_exit(sec);
	return;
    }

    buf[0] = sec->cmd_param[0];

    if(buf[0] == 1){
        idev->gesture = ENABLE;
        gestrue_status = idev->gesture;
    }else{
	idev->gesture = DISABLE;
        gestrue_status = idev->gesture;
    }

    snprintf(buff, sizeof(buff), "%s", "OK");
    sec->cmd_state = SEC_CMD_STATUS_OK;

    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

    ipio_info( "%s: %s, buff == %s,sec->cmd_state == %d\n", __func__, sec->cmd_param[0] ? "on" : "off", buff, sec->cmd_state);
    sec_cmd_set_cmd_exit(sec);

    return;
}

static void glove_mode(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    unsigned int buf[4];

    ipio_info("glove_mode Enter.\n");
    sec_cmd_set_default_result(sec);

      if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
        snprintf(buff, sizeof(buff), "NG");
        sec->cmd_state = SEC_CMD_STATUS_FAIL;
        sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
        ipio_err( "%s: sec->cmd_param[0] == %s, buff == %s,sec->cmd_state == %d\n", __func__, sec->cmd_param[0], buff, sec->cmd_state);
        sec_cmd_set_cmd_exit(sec);
	return;
    }

    buf[0] = sec->cmd_param[0];

    mutex_lock(&idev->touch_mutex);//+ add by songbinbo.wt for High sensitivity mode contrl 20191125
    if(buf[0] == 1){
            if (ilitek_tddi_ic_func_ctrl("glove", ENABLE) < 0){
                     ipio_err("Write glove start cmd failed\n");
            }
             idev->glove = true;
        }
    else {
        if (ilitek_tddi_ic_func_ctrl("glove", DISABLE) < 0){
                ipio_err("Write glove stop cmd failed\n");
	}
	idev->glove = false;
    }
    ipio_info("Write glove cmd success.glove = %d\n", idev->glove );
    mutex_unlock(&idev->touch_mutex);//- add by songbinbo.wt for High sensitivity mode contrl 20191125

    snprintf(buff, sizeof(buff), "%s", "OK");
    sec->cmd_state = SEC_CMD_STATUS_OK;

    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

    ipio_info( "%s: %s, buff == %s,sec->cmd_state == %d\n", __func__, sec->cmd_param[0] ? "on" : "off", buff, sec->cmd_state);
    sec_cmd_set_cmd_exit(sec);

    return;
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

	ipio_info( "%s: %s\n", __func__, buff);
}

struct sec_cmd ft_commands[] = {
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	 {SEC_CMD("aot_enable", aot_enable),},
        {SEC_CMD("glove_mode", glove_mode),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};
#endif

#ifdef CONFIG_OF
struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1];	/* this is the minimum size */
};
static char mtkfb_lcm_name[256] = { 0 };

static int __parse_tag_videolfb(struct device_node *node)
{
	struct tag_videolfb *videolfb_tag = NULL;
	unsigned long size = 0;

	videolfb_tag =
		(struct tag_videolfb *)of_get_property(node,
			"atag,videolfb", (int *)&size);
	if (videolfb_tag) {
		memset((void *)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
		strcpy((char *)mtkfb_lcm_name, videolfb_tag->lcmname);
		mtkfb_lcm_name[strlen(videolfb_tag->lcmname)] = '\0';
	}

	if(0==strcmp(mtkfb_lcm_name,"ili7807g_hd_plus_vdo_truly_row")){
		name_modue = XL_ROW_MODULE_NAME;
		ipio_info("modue:truly row\n");
	}else if(0==strcmp(mtkfb_lcm_name,"ili7807g_hd_plus_vdo_truly_na")){
		name_modue = XL_NA_MODULE_NAME;
		ipio_info("modue:truly na\n");
	}else if(0==strcmp(mtkfb_lcm_name,"ili7807g_hd_plus_vdo_txd_row")){
		name_modue = TXD_ROW_MODULE_NAME;
		ipio_info("modue:txd row\n");
	}else if(0==strcmp(mtkfb_lcm_name,"ili7807g_hd_plus_vdo_txd_na")){
		name_modue = TXD_NA_MODULE_NAME;
		ipio_info("modue:txd na\n");
	}if(0==strcmp(mtkfb_lcm_name,"ili7807g_hd_plus_vdo_tcl")){
		name_modue = XL_MODULE_NAME;
		ipio_info("modue:truly tcl\n");
	}

	ipio_info("name_modue=%d\n",name_modue);
	return 0;
}

static int _parse_tag_videolfb(void)
{
	int ret;
	struct device_node *chosen_node;

	chosen_node = of_find_node_by_path("/chosen");
	if (!chosen_node)
		chosen_node = of_find_node_by_path("/chosen@0");

	if (chosen_node)
		ret = __parse_tag_videolfb(chosen_node);

	return 0;
}
#endif

static int module_compatible_init(void)
{
	ipio_info("module_compatible_init enter\n");
	if(name_modue == XL_ROW_MODULE_NAME){
		idev->ili_name = module_name_is_use[XL_ROW_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[XL_ROW_MODULE_NAME].module_bin_name;

		size_ili = module_name_is_use[XL_ROW_MODULE_NAME].size_ili;

		CTPM_FW = vmalloc(size_ili);
		if (ERR_ALLOC_MEM(CTPM_FW)) {
			ipio_err("Failed to allocate CTPM_FW memory, %ld\n", PTR_ERR(CTPM_FW));
			return -ENOMEM;
		}
		CTPM_FW = CTPM_FW_XL_ROW;
		ipio_info("TP is xinli ROW\n");

	}else if(name_modue == XL_NA_MODULE_NAME){
		idev->ili_name = module_name_is_use[XL_NA_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[XL_NA_MODULE_NAME].module_bin_name;

		size_ili = module_name_is_use[XL_NA_MODULE_NAME].size_ili;
		CTPM_FW = vmalloc(size_ili);
		if (ERR_ALLOC_MEM(CTPM_FW)) {
			ipio_err("Failed to allocate CTPM_FW memory, %ld\n", PTR_ERR(CTPM_FW));
			return -ENOMEM;
		}
		CTPM_FW = CTPM_FW_XL_NA;
		ipio_info("TP is xinli NA\n");

	}else if(name_modue == TXD_ROW_MODULE_NAME){
		idev->ili_name = module_name_is_use[TXD_ROW_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[TXD_ROW_MODULE_NAME].module_bin_name;

		size_ili = module_name_is_use[TXD_ROW_MODULE_NAME].size_ili;
		CTPM_FW = vmalloc(size_ili);
		if (ERR_ALLOC_MEM(CTPM_FW)) {
			ipio_err("Failed to allocate CTPM_FW memory, %ld\n", PTR_ERR(CTPM_FW));
			return -ENOMEM;
		}
		CTPM_FW = CTPM_FW_TXD_ROW;
		ipio_info("TP is txd row\n");

	}else if(name_modue == TXD_NA_MODULE_NAME){
		idev->ili_name = module_name_is_use[TXD_NA_MODULE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[TXD_NA_MODULE_NAME].module_bin_name;

		size_ili = module_name_is_use[TXD_NA_MODULE_NAME].size_ili;
		CTPM_FW = vmalloc(size_ili);
		if (ERR_ALLOC_MEM(CTPM_FW)) {
			ipio_err("Failed to allocate CTPM_FW memory, %ld\n", PTR_ERR(CTPM_FW));
			return -ENOMEM;
		}
		CTPM_FW = CTPM_FW_TXD_NA;
		ipio_info("TP is txd na\n");
		
	} else if(name_modue == XL_MODULE_NAME){
			idev->ili_name = module_name_is_use[XL_MODULE_NAME].module_ini_name;
			idev->bin_name = module_name_is_use[XL_MODULE_NAME].module_bin_name;
	
			size_ili = module_name_is_use[XL_MODULE_NAME].size_ili;
	
			CTPM_FW = vmalloc(size_ili);
			if (ERR_ALLOC_MEM(CTPM_FW)) {
				ipio_err("Failed to allocate CTPM_FW memory, %ld\n", PTR_ERR(CTPM_FW));
				return -ENOMEM;
			}
			CTPM_FW = CTPM_FW_XL;
			ipio_info("TP is xinli\n");
	
	}else{
		idev->ili_name = module_name_is_use[NONE_NAME].module_ini_name;
		idev->bin_name = module_name_is_use[NONE_NAME].module_bin_name;

		size_ili = module_name_is_use[NONE_NAME].size_ili;
		CTPM_FW = vmalloc(size_ili);
		if (ERR_ALLOC_MEM(CTPM_FW)) {
			ipio_err("Failed to allocate CTPM_FW memory, %ld\n", PTR_ERR(CTPM_FW));
			return -ENOMEM;
		}
		CTPM_FW = CTPM_FW_NONE;
		ipio_info("TP is normal\n");

	}
	return 0;
}

//-add by songbinbo.wt for ILITEK tp driver compatible truly & txd vendor  20191005

static int ilitek_plat_probe(void)
{

#ifdef SEC_TSP_FACTORY_TEST
	int ret = 0;
#endif

	ipio_info("platform probe\n");

#if REGULATOR_POWER
	ilitek_plat_regulator_power_init();
#endif
	if (ilitek_plat_gpio_register() < 0)
		ipio_err("Register gpio failed\n");

	//add by songbinbo.wt for ILITEK tp driver compatible truly & txd vendor	20191005
	if(module_compatible_init() < 0){
		ipio_err("module compabible info init faild\n");
	}

	if (ilitek_tddi_init() < 0) {
		ipio_err("platform probe failed\n");
		return -ENODEV;
	}

	ilitek_plat_irq_register(idev->irq_tirgger_type);

#if WINGTECH_TP_OPENSHORT_EN
	create_ctp_proc();
#endif

#ifdef SEC_TSP_FACTORY_TEST
	ret = sec_cmd_init(&idev->sec, ft_commands,ARRAY_SIZE(ft_commands), SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		ipio_err("%s: Failed to sec_cmd_init\n", __func__);
		return -ENODEV;
	}
#endif
	ipio_info("ilitek Touch Screen(spi bus) driver probe successfully");

	tpd_load_status = 1;
	return 0;
}

static int ilitek_plat_remove(void)
{
	ipio_info();
	ilitek_tddi_dev_remove();

#ifdef SEC_TSP_FACTORY_TEST
	sec_cmd_exit(&idev->sec, SEC_CLASS_DEVT_TSP);
#endif

	return 0;
}

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
};

static int tpd_local_init(void)
{
	ipio_info("TPD init device driver\n");

#ifdef CONFIG_OF
	_parse_tag_videolfb();	//add by songbinbo.wt for ILITEK tp driver compatible truly & txd vendor	20191005
#endif
	if (name_modue >= ARRAY_SIZE(module_name_is_use)) {
		ipio_err("no compatable device found\n");
		return -ENODEV;
	}

	if (ilitek_tddi_dev_init(&hwif) < 0) {
		ipio_err("Failed to register i2c/spi bus driver\n");
		return -ENODEV;
	}
	if (tpd_load_status == 0) {
		ipio_err("Add error touch panel driver\n");
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

	ipio_info("ILITEK TP driver init for MTK\n");
	tpd_get_dts_info();
	ret = tpd_driver_add(&tpd_device_driver);
	if (ret < 0) {
		ipio_err("ILITEK add TP driver failed\n");
		tpd_driver_remove(&tpd_device_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit ilitek_plat_dev_exit(void)
{
	ipio_info("ilitek driver has been removed\n");
	tpd_driver_remove(&tpd_device_driver);
}

module_init(ilitek_plat_dev_init);
module_exit(ilitek_plat_dev_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");
