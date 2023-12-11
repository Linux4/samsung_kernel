/****************************************************************************************
 *
 * @File Name   : lct_tp_info.c
 * @Author      : wanghan
 * @E-mail      : <wanghan@longcheer.com>
 * @Create Time : 2018-08-17 17:34:43
 * @Description : Display touchpad information.
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

/*
 * DEFINE CONFIGURATION
 ****************************************************************************************
 */
#define TP_INFO_NAME          "tp_info"
#define TP_LOCKDOWN_INFO_NAME "tp_lockdown_info"
#define LCT_STRING_SIZE       128
#define TP_CALLBACK_CMD_INFO      "CMD_INFO"
#define TP_CALLBACK_CMD_LOCKDOWN  "CMD_LOCKDOWN"

#define TP_INFO(fmt, arg...)						\
({									\
	pr_err("LCT_TP_INFO:[INFO] (%s, %d): " fmt, __func__, __LINE__, ##arg);	\
})									\

#define TP_ERR(fmt, arg...)						\
({									\
	pr_err("LCT_TP_INFO:[ERR] (%s, %d): " fmt, __func__, __LINE__, ##arg);	\
})									\

/*
 * DATA STRUCTURES
 ****************************************************************************************
 */
typedef struct lct_tp{
	struct kobject *tp_device;
	char tp_info_buf[LCT_STRING_SIZE];
	char tp_lockdown_info_buf[LCT_STRING_SIZE];
	struct proc_dir_entry *proc_entry_tp_info;
	struct proc_dir_entry *proc_entry_tp_lockdown_info;
	int (*pfun_info_cb)(const char *);
}lct_tp_t;
int tp_select_proximity;
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
static lct_tp_t *lct_tp_p = NULL;
static int (*pfun_lockdown_cb)(void) = NULL;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
// --- proc ---
static ssize_t lct_proc_tp_info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
static ssize_t lct_proc_tp_lockdown_info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
static const struct proc_ops lct_proc_tp_info_fops = {
	.proc_read		= lct_proc_tp_info_read,
};
static const struct proc_ops lct_proc_tp_lockdown_info_fops = {
	.proc_read		= lct_proc_tp_lockdown_info_read,
};

int init_lct_tp_info(char *tp_info_buf, char *tp_lockdown_info_buf)
{
	TP_INFO("init /proc/%s and /proc/%s ...\n", TP_INFO_NAME, TP_LOCKDOWN_INFO_NAME);
	lct_tp_p = kzalloc(sizeof(lct_tp_t), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lct_tp_p)){
		TP_ERR("kzalloc() request memory failed!\n");
		return -ENOMEM;
	}

	if (NULL != tp_info_buf)
		strcpy(lct_tp_p->tp_info_buf, tp_info_buf);
	if (NULL != tp_lockdown_info_buf)
		strcpy(lct_tp_p->tp_lockdown_info_buf, tp_lockdown_info_buf);

	lct_tp_p->proc_entry_tp_info = proc_create_data(TP_INFO_NAME, 0444, NULL, &lct_proc_tp_info_fops, NULL);
	if (IS_ERR_OR_NULL(lct_tp_p->proc_entry_tp_info)) {
		TP_ERR("add /proc/%s error\n", TP_INFO_NAME);
		goto err_tp_info;
	}
	lct_tp_p->proc_entry_tp_lockdown_info = proc_create_data(TP_LOCKDOWN_INFO_NAME, 0444, NULL, &lct_proc_tp_lockdown_info_fops, NULL);
	if (IS_ERR_OR_NULL(lct_tp_p->proc_entry_tp_lockdown_info)) {
		TP_ERR("add /proc/%s error\n", TP_LOCKDOWN_INFO_NAME);
		goto err_tp_lockdown;
	}

	TP_INFO("done\n");
	return 0;

err_tp_lockdown:
	remove_proc_entry(TP_INFO_NAME, NULL);
err_tp_info:
	kfree(lct_tp_p);
	return -1;
}
EXPORT_SYMBOL(init_lct_tp_info);

void uninit_lct_tp_info(void)
{
	TP_INFO("uninit /proc/%s and /proc/%s ...\n", TP_INFO_NAME, TP_LOCKDOWN_INFO_NAME);

	if (IS_ERR_OR_NULL(lct_tp_p))
		return;

	if (lct_tp_p->proc_entry_tp_info != NULL) {
		remove_proc_entry(TP_INFO_NAME, NULL);
		lct_tp_p->proc_entry_tp_info = NULL;
		TP_INFO("remove /proc/%s\n", TP_INFO_NAME);
	}

	if (lct_tp_p->proc_entry_tp_lockdown_info != NULL) {
		remove_proc_entry(TP_LOCKDOWN_INFO_NAME, NULL);
		lct_tp_p->proc_entry_tp_lockdown_info = NULL;
		TP_INFO("remove /proc/%s\n", TP_LOCKDOWN_INFO_NAME);
	}

	kfree(lct_tp_p);
	TP_INFO("done\n");
}
EXPORT_SYMBOL(uninit_lct_tp_info);

void update_lct_tp_info(char *tp_info_buf, char *tp_lockdown_info_buf)
{
	if (NULL != tp_info_buf) {
		memset(lct_tp_p->tp_info_buf, 0, sizeof(lct_tp_p->tp_info_buf));
		strcpy(lct_tp_p->tp_info_buf, tp_info_buf);
	}
	if (NULL != tp_lockdown_info_buf) {
		memset(lct_tp_p->tp_lockdown_info_buf, 0, sizeof(lct_tp_p->tp_lockdown_info_buf));
		strcpy(lct_tp_p->tp_lockdown_info_buf, tp_lockdown_info_buf);
	}
	return;
}
EXPORT_SYMBOL(update_lct_tp_info);

void set_lct_tp_info_callback(int (*pfun)(const char *))
{
	if (NULL != lct_tp_p)
		lct_tp_p->pfun_info_cb = pfun;
	return;
}
EXPORT_SYMBOL(set_lct_tp_info_callback);

void set_lct_tp_lockdown_info_callback(int (*pfun)(void))
{
	pfun_lockdown_cb = pfun;
	return;
}
EXPORT_SYMBOL(set_lct_tp_lockdown_info_callback);

static ssize_t lct_proc_tp_info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt=0;
	char *page = NULL;

	//TP_INFO("size = %lu, pos = %lld\n", size, *ppos);
	if (*ppos)
		return 0;

	if (NULL != lct_tp_p->pfun_info_cb)
		lct_tp_p->pfun_info_cb(TP_CALLBACK_CMD_INFO);

	page = kzalloc(128, GFP_KERNEL);

	//if(NULL == lct_tp_p->tp_info_buf)
	if(0 == strlen(lct_tp_p->tp_info_buf))
		cnt = sprintf(page, "No touchpad\n");
	else
		cnt = sprintf(page, "%s", (strlen(lct_tp_p->tp_info_buf) ? lct_tp_p->tp_info_buf : "Unknown touchpad"));

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	if (*ppos != cnt)
		*ppos = cnt;
	TP_INFO("page=%s", page);

	kfree(page);
	return cnt;
}

static ssize_t lct_proc_tp_lockdown_info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt=0;
	char *page = NULL;

	if (*ppos)
		return 0;

	if (NULL != lct_tp_p->pfun_info_cb)
		lct_tp_p->pfun_info_cb(TP_CALLBACK_CMD_LOCKDOWN);

	if (NULL != pfun_lockdown_cb)
		pfun_lockdown_cb();

	page = kzalloc(128, GFP_KERNEL);

	//if(NULL == lct_tp_p->tp_lockdown_info_buf)
	if(0 == strlen(lct_tp_p->tp_lockdown_info_buf))
		cnt = sprintf(page, "No touchpad\n");
	else
		cnt = sprintf(page, "%s", (strlen(lct_tp_p->tp_lockdown_info_buf) ? lct_tp_p->tp_lockdown_info_buf : "Unknown touchpad"));

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	if (*ppos != cnt)
		*ppos = cnt;
	TP_INFO("page=%s", page);

	kfree(page);
	return cnt;
}

static int __init tp_info_init(void)
{
	int ret;
	TP_INFO("init");
	//create longcheer procfs node
    ret = init_lct_tp_info("[Vendor]unkown,[FW]unkown,[IC]unkown\n", NULL);
	if (ret < 0) {
		TP_ERR("init_lct_tp_info Failed!\n");
        return ret;
	}
	TP_INFO("init_lct_tp_info Succeeded!\n");
	return 0;
}

static void __exit tp_info_exit(void)
{
	TP_INFO("exit");
	uninit_lct_tp_info();
	return;
}


int tp_proximity(void)
{
	return tp_select_proximity;	
}

EXPORT_SYMBOL(tp_proximity);
module_init(tp_info_init);
module_exit(tp_info_exit);

MODULE_DESCRIPTION("Touchpad Information Driver");
MODULE_LICENSE("GPL v2");
