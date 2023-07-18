/****************************************************************************************
 *
 * @File Name   : lct_tp_gesture.c
 * @Author      : wanghan
 * @E-mail      : <wanghan@longcheer.com>
 * @Create Time : 2018-09-30 17:34:43
 * @Description : Enable/Disable touchpad.
 *
 ****************************************************************************************/

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/pinctrl/consumer.h>

/*
 * DEFINE CONFIGURATION
 ****************************************************************************************
 */
#define TP_GESTURE_NAME          "tp_gesture"
#define TP_GESTURE_LOG_ENABLE
#define TP_GESTURE_TAG           "LCT_TP_GESTURE"

#ifdef TP_GESTURE_LOG_ENABLE
#define TP_LOGW(log, ...) printk(KERN_WARNING "[%s] %s (line %d): " log, TP_GESTURE_TAG, __func__, __LINE__, ##__VA_ARGS__)
#define TP_LOGE(log, ...) printk(KERN_ERR "[%s] %s ERROR (line %d): " log, TP_GESTURE_TAG, __func__, __LINE__, ##__VA_ARGS__)
#else
#define TP_LOGW(log, ...) {}
#define TP_LOGE(log, ...) {}
#endif

/*
 * DATA STRUCTURES
 ****************************************************************************************
 */
typedef int (*tp_gesture_cb_t)(bool enable_tp);

typedef struct lct_tp{
	bool enable_tp_gesture_flag;
	struct proc_dir_entry *proc_entry_tp;
	tp_gesture_cb_t pfun;
}lct_tp_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
static lct_tp_t *lct_tp_p = NULL;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
// --- proc ---
static int lct_creat_proc_tp_entry(void);
static ssize_t lct_proc_tp_gesture_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
static ssize_t lct_proc_tp_gesture_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos);
static const struct file_operations lct_proc_tp_gesture_fops = {
	.read		= lct_proc_tp_gesture_read,
	.write		= lct_proc_tp_gesture_write,
};


int init_lct_tp_gesture(tp_gesture_cb_t callback)
{
	if (NULL == callback) {
		TP_LOGE("callback is NULL!\n");
		return -EINVAL;
	}

	TP_LOGW("Initialization tp_gesture node!\n");
	lct_tp_p = kzalloc(sizeof(lct_tp_t), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lct_tp_p)){
		TP_LOGE("kzalloc() request memory failed!\n");
		return -ENOMEM;
	}
	lct_tp_p->pfun = callback;
	lct_tp_p->enable_tp_gesture_flag = false;

	if (lct_creat_proc_tp_entry() < 0) {
		kfree(lct_tp_p);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(init_lct_tp_gesture);


bool get_tp_gesture_stat()
{
    if(IS_ERR_OR_NULL(lct_tp_p)){
        return false;
    }
    else{
        return lct_tp_p->enable_tp_gesture_flag;
    }
}
EXPORT_SYMBOL(get_tp_gesture_stat);


void uninit_lct_tp_gesture(void)
{
	TP_LOGW("uninit /proc/%s ...\n", TP_GESTURE_NAME);
	if (IS_ERR_OR_NULL(lct_tp_p))
		return;
	if (lct_tp_p->proc_entry_tp != NULL) {
		remove_proc_entry(TP_GESTURE_NAME, NULL);
		lct_tp_p->proc_entry_tp = NULL;
		TP_LOGW("remove /proc/%s\n", TP_GESTURE_NAME);
	}
	kfree(lct_tp_p);
	return;
}
EXPORT_SYMBOL(uninit_lct_tp_gesture);

int sprd_pin_set(struct device *dev, char* pinctrl_name)
{
    static struct pinctrl *p;
    struct pinctrl_state *pinctrl_state0;
    int ret;

    printk(KERN_EMERG"set pinctrl %s\n", pinctrl_name);

    p = devm_pinctrl_get(dev);
   //set function to gpio
    pinctrl_state0 = pinctrl_lookup_state(p, pinctrl_name);
    ret =  pinctrl_select_state(p, pinctrl_state0);

	return ret;
}
EXPORT_SYMBOL(sprd_pin_set);


static int lct_creat_proc_tp_entry(void)
{
	lct_tp_p->proc_entry_tp = proc_create_data(TP_GESTURE_NAME, 0666, NULL, &lct_proc_tp_gesture_fops, NULL);
	if (IS_ERR_OR_NULL(lct_tp_p->proc_entry_tp)) {
		TP_LOGE("add /proc/%s error!\n", TP_GESTURE_NAME);
		return -1;
	}
	TP_LOGW("/proc/%s is okay!\n", TP_GESTURE_NAME);
	return 0;
}

static ssize_t lct_proc_tp_gesture_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	ssize_t cnt=0;
	char *page = NULL;

	if (*ppos)
		return 0;

	page = kzalloc(128, GFP_KERNEL);
	if (IS_ERR_OR_NULL(page))
		return -ENOMEM;

	cnt = sprintf(page, "%s", (lct_tp_p->enable_tp_gesture_flag ? "1\n" : "0\n"));

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	if (*ppos != cnt)
		*ppos = cnt;
	TP_LOGW("Touchpad status : %s", page);

	kfree(page);
	return cnt;
}

static ssize_t lct_proc_tp_gesture_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	int ret;
	ssize_t cnt = 0;
	char *page = NULL;
	unsigned int input = 0;

	page = kzalloc(128, GFP_KERNEL);
	if (IS_ERR_OR_NULL(page))
		return -ENOMEM;

	cnt = simple_write_to_buffer(page, 128, ppos, buf, size);
	if (cnt <= 0){
		kfree(page);
		return -EINVAL;
	}

	if (sscanf(page, "%u", &input) != 1){
		kfree(page);
		return -EINVAL;
	}

	if (input > 0) {
		if (lct_tp_p->enable_tp_gesture_flag)
			goto exit;
		TP_LOGW("Enbale Touchpad Gesture ...\n");

		if(2 != input){//we need hold avdd&avee but no gesture
			ret = lct_tp_p->pfun(true);
			if (ret) {
				TP_LOGW("Enable Touchpad Gesture Failed! ret=%d\n", ret);
				goto exit;
			}
		}

		lct_tp_p->enable_tp_gesture_flag = true;
	} else {
		if (!lct_tp_p->enable_tp_gesture_flag)
			goto exit;
		TP_LOGW("Disable Touchpad Gesture ...\n");
		ret = lct_tp_p->pfun(false);
		if (ret) {
			TP_LOGW("Disable Touchpad Gesture Failed! ret=%d\n", ret);
			goto exit;
		}
		lct_tp_p->enable_tp_gesture_flag = false;
	}
	TP_LOGW("Set Touchpad Gesture successfully!\n");

exit:
	kfree(page);
	return cnt;
}

