#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/maxim_dsm.h>

static int param[PARAM_DSM_MAX];

static DEFINE_MUTEX(dsm_lock);

extern int32_t dsm_open(int32_t port_id,uint32_t*  dsm_params, u8* param);

int get_dsm_onoff_status(void)	{
	return param[PARAM_ONOFF];
}

static int maxdsm_open(struct inode *inode, struct file *filep)
{
	return 0;
}

static ssize_t maxdsm_read(struct file *filep, char __user *buf,
			size_t count, loff_t *ppos)
{
	int rc;
	uint32_t filter_set;

	mutex_lock(&dsm_lock);

	filter_set = DSM_ID_FILTER_GET_AFE_PARAMS;

	dsm_open(DSM_RX_PORT_ID, &filter_set, (u8 *) param);

	rc = copy_to_user(buf, param, count);
	if (rc != 0) {
		pr_err("%s: copy_to_user failed - %d\n", __func__, rc);
		mutex_unlock(&dsm_lock);
		return 0;
	}

	mutex_unlock(&dsm_lock);

	return rc;
}

static ssize_t maxdsm_write(struct file *filep, const char __user *buf,
			size_t count, loff_t *ppos)
{
	int x, rc;
	uint32_t filter_set;

	mutex_lock(&dsm_lock);

	rc = copy_from_user(param, buf, count);
	if (rc != 0) {
		pr_err("%s: copy_from_user failed - %d\n", __func__, rc);
		mutex_unlock(&dsm_lock);
		return rc;
	}

	if(param[PARAM_WRITE_FLAG] == 0)	{
		// validation check
		for(x=PARAM_THERMAL_LIMIT;x<PARAM_DSM_MAX;x+=2)	{
			if(x!= PARAM_ONOFF && param[x]!=0) {
				param[PARAM_WRITE_FLAG] = FLAG_WRITE_ALL;
				break;
			}
		}		
	}

	if(param[PARAM_WRITE_FLAG]==FLAG_WRITE_ONOFF_ONLY || param[PARAM_WRITE_FLAG] == FLAG_WRITE_ALL) {
		/* set params from the algorithm to application */
		filter_set = DSM_ID_FILTER_SET_AFE_CNTRLS;
		dsm_open(DSM_RX_PORT_ID, &filter_set, (u8*)param);
	}
	
	mutex_unlock(&dsm_lock);

	return rc;
}

static const struct file_operations dsm_ctrl_fops = {
	.owner		= THIS_MODULE,
	.open		= maxdsm_open,
	.release	= NULL,
	.read		= maxdsm_read,
	.write		= maxdsm_write,
	.mmap		= NULL,
	.poll		= NULL,
	.fasync		= NULL,
	.llseek		= NULL,
};

static struct miscdevice dsm_ctrl_miscdev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     "dsm_ctrl_dev",
	.fops =     &dsm_ctrl_fops
};

int maxdsm_init(void)
{
	memset(param, 0, sizeof(param));
    return misc_register(&dsm_ctrl_miscdev);;
}

int maxdsm_deinit(void)
{
    return misc_deregister(&dsm_ctrl_miscdev);
}
