#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <soc/sprd/gpio.h>
#include <linux/regulator/consumer.h>
#include <soc/sprd/regulator.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/earlysuspend.h>
#include <linux/wait.h>
#include <linux/poll.h>

#define	POWER_CTL_NAME	"power_ctl"

#define DOWNLOAD_IOCTL_BASE	'z'
#define DOWNLOAD_POWER_ON	_IO(DOWNLOAD_IOCTL_BASE, 0x01)
#define DOWNLOAD_POWER_OFF	_IO(DOWNLOAD_IOCTL_BASE, 0x02)
#define DOWNLOAD_POWER_RST	_IO(DOWNLOAD_IOCTL_BASE, 0x03)
#define GNSS_CHIP_EN		_IO(DOWNLOAD_IOCTL_BASE, 0x04)
#define GNSS_CHIP_DIS		_IO(DOWNLOAD_IOCTL_BASE, 0x05)
#define GNSS_LNA_EN		_IO(DOWNLOAD_IOCTL_BASE, 0x06)
#define GNSS_LNA_DIS		_IO(DOWNLOAD_IOCTL_BASE, 0x07)

struct sprd_gnss
{
	u32 chip_en;
	struct regulator *vdd_lna;
};

static struct sprd_gnss gnss_dev;

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend	gnss_suspend;
wait_queue_head_t	gnss_sleep_wait;
bool gnss_flag_sleep = false;
bool gnss_flag_resume = false;
static char gnss_status[16] = {0};
#endif


static int sprd_power_ctl_open(struct inode *inode,struct file *filp)
{
	return 0;
}

static int sprd_power_ctl_release(struct inode *inode,struct file *filp)
{
	return 0;
}


static void download_clk_init(bool enable)
{
	int err;
	const char *clk_name;
	struct device_node *np;
	static struct clk *download_clk = NULL;

	np = of_find_node_by_name(NULL, "sprd-marlin");
	if (!np) {
		printk(KERN_ERR"sprd-marlin not found");
		return;
	}

	err = of_property_read_string(np, "clk-name", &clk_name);
	if (err) {
		printk(KERN_ERR"get clk-name failed");
		return;
	}

	if (download_clk == NULL) {
		download_clk = clk_get(NULL, clk_name);
		if (IS_ERR(download_clk)) {
			printk("failed to get parent %s\n", clk_name);
			return;
		}
		clk_set_rate(download_clk, 32000);
	}

	if (enable) {
		clk_prepare_enable(download_clk);
	}else{
		clk_disable_unprepare(download_clk);
	}
}

static DEFINE_MUTEX(power_ctl_lock);
static void sprd_download_poweron(bool enable)
{
	int ret;
	int gpio_dwnld;
	static bool request = 0;
	const char *vdd_dwnld = NULL;
	static struct regulator *download_vdd = NULL;
	struct device_node *np;
	static unsigned int power_count = 0;

	np = of_find_node_by_name(NULL, "sprd-marlin");
	if (!np) {
		printk(KERN_ERR"sprd-marlin not found");
		return;
	}

	gpio_dwnld = of_get_gpio(np, 5);
	if (gpio_is_valid(gpio_dwnld)) {  /*gpio 1v6 power supply*/
		if(0 == request) {
			ret = gpio_request (gpio_dwnld, "download");
			if (ret){
				printk ("gpio_dwnld request err: %d\n", gpio_dwnld);
			}
		}
		else{
			request = 1;
		}

		if (enable) {
			gpio_direction_output(gpio_dwnld,1);

		}
		else {
			gpio_direction_output(gpio_dwnld,0);
		}
	} else { /*ldo 1v6 power supply*/

		ret = of_property_read_string(np, "vdd-download", &vdd_dwnld);
		if (ret) {
			printk(KERN_ERR"get vdd-download failed");
			return;
		}

		if (download_vdd == NULL) {
			download_vdd = regulator_get(NULL, vdd_dwnld);

			if (IS_ERR(download_vdd)) {
				printk(KERN_ERR"Get regulator of vdd-download  error!\n");
				return;
			}
		}
		#ifdef CONFIG_MACH_SHARKLS_J1MINI
		gpio_request(131,"vdd_pa");
		gpio_direction_output(131,1);
		gpio_set_value(131, 1);
		#endif

		mutex_lock(&power_ctl_lock);
		if(enable){
			if(0 == power_count){
				regulator_set_voltage(download_vdd, 1600000, 1600000);
				ret = regulator_enable(download_vdd);
				printk("sprd_download_poweron on\n");
			}
			power_count++;
		}
		else {
		    power_count--;
		    if(0 == power_count)
		    {
				if(regulator_is_enabled(download_vdd))
				ret = regulator_disable(download_vdd);
				printk("sprd_download_poweron off\n");
		    }
		}
		mutex_unlock(&power_ctl_lock);
	}
}

static void sprd_download_rst(void)
{
	int ret;
	u32 gpio_rst;
	static bool request = 0;
	struct device_node *np;

	np = of_find_node_by_name(NULL, "sprd-marlin");
	if (!np) {
		printk(KERN_ERR"sprd-marlin not found");
		return;
	}
	gpio_rst = of_get_gpio(np, 4);
	if(0 == request)
	{
		ret = gpio_request (gpio_rst, "download");
		if (ret){
			printk (KERN_ERR"gpio_rst request err: %d\n", gpio_rst);
		}
		else{
			request = 1;
		}
	}
	gpio_direction_output(gpio_rst,1);
	msleep(1);
	gpio_direction_output(gpio_rst,0);
	msleep(1);
	gpio_direction_output(gpio_rst,1);
	msleep(1);
}

static void sprd_gnss_chip_en(bool enable)
{
	if(enable) {
		gpio_direction_output(gnss_dev.chip_en,0);
		msleep(1);
		gpio_direction_output(gnss_dev.chip_en,1);
		msleep(1);
	}else {
		gpio_direction_output(gnss_dev.chip_en,0);
	}
}

static void gnss_lna_enable(bool on)
{
	int ret;

	if(gnss_dev.vdd_lna != NULL){
		if (on){
			regulator_set_voltage(gnss_dev.vdd_lna, 1800000, 1800000);
			ret = regulator_enable(gnss_dev.vdd_lna);
		}else if(regulator_is_enabled(gnss_dev.vdd_lna)) {
			ret = regulator_disable(gnss_dev.vdd_lna);
		}
	}
}

static long sprd_power_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	printk("DOWNLOAD IOCTL: 0x%x.\n", cmd);

	switch (cmd) {
	case DOWNLOAD_POWER_ON:
		sprd_download_poweron(1);
		download_clk_init(1);
		break;
	case DOWNLOAD_POWER_OFF:
		sprd_download_poweron(0);
		break;
	case DOWNLOAD_POWER_RST:
		sprd_download_rst();
		break;
	case GNSS_CHIP_EN:
		sprd_gnss_chip_en(1);
		break;
	case GNSS_CHIP_DIS:
		sprd_gnss_chip_en(0);
		break;
	case GNSS_LNA_EN:
		gnss_lna_enable(1);
		break;
	case GNSS_LNA_DIS:
		gnss_lna_enable(0);
		break;
	}
	return 0;
}

static void sprd_download_dts_init(void)
{
	int ret;
	struct device_node *np;
	const char *vdd_lna = NULL;

	np = of_find_node_by_name(NULL, "sprd-ge2");
	if (!np) {
		printk(KERN_ERR"sprd-gnss not found");
		return;
	}

	ret = of_property_read_string(np, "vdd-lna", &vdd_lna);
	if (ret){
		printk(KERN_ERR"get vdd-lna failed");
		return;
	}

	gnss_dev.vdd_lna = regulator_get(NULL, vdd_lna);
	if (IS_ERR(gnss_dev.vdd_lna)){
		printk(KERN_ERR"Get regulator of vdd-lna  error!\n");
		return;
	}

	gnss_dev.chip_en = of_get_gpio(np, 0);
	ret = gpio_request (gnss_dev.chip_en, "ge2_chip_en");
	if (ret){
		printk (KERN_ERR"gnss_dev.chip_en request err: %d\n", gnss_dev.chip_en);
	}
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void gnss_early_suspend(struct early_suspend *handler)
{
	printk("%s\n",__func__);
	gnss_flag_sleep = true;
	gnss_flag_resume = false;
}

static void gnss_late_resume(struct early_suspend *handler)
{
	printk("%s\n",__func__);
	gnss_flag_resume = true;
	gnss_flag_sleep = false;
}

static unsigned int gnss_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &gnss_sleep_wait, wait);
	if(gnss_flag_sleep)
	{
		printk("%s gnss_flag_sleep:%d\n",__func__,gnss_flag_sleep);
		gnss_flag_sleep = false;
		memcpy(gnss_status,"gnss_sleep ",11);
		mask |= POLLIN | POLLRDNORM;
	}

	if(gnss_flag_resume)
	{
		printk("%s gnss_flag_resume:%d\n",__func__,gnss_flag_resume);
		gnss_flag_resume = false;
		memcpy(gnss_status,"gnss_resume",11);
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

static int sprd_gnss_read(struct file *filp,char __user *buf,size_t count,loff_t *pos)
{
	if(count < 11)
		return -EINVAL;

	if(count > 11)
		count = 11;

	if(copy_to_user(buf,gnss_status,count)){
			return -EFAULT;
	}

	return count;
}
#endif

static struct file_operations sprd_power_ctl__fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl	= sprd_power_ctl_ioctl,
	#ifdef CONFIG_COMPAT
	.compat_ioctl = sprd_power_ctl_ioctl,
	#endif
	.open  = sprd_power_ctl_open,
	.release = sprd_power_ctl_release,
	#ifdef CONFIG_HAS_EARLYSUSPEND
	.read  = sprd_gnss_read,
	.poll  = gnss_poll,
	#endif
};

static struct miscdevice sprd_power_ctl_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = POWER_CTL_NAME,
	.fops = &sprd_power_ctl__fops,
};

static int __init sprd_power_ctl_init(void)
{
	int err=0;
	printk("sprd_power_ctl_init\n");
	err = misc_register(&sprd_power_ctl_device);

	if(err){
		printk(KERN_INFO "download power control dev add failed!!!\n");
	}

	sprd_download_dts_init();

	#ifdef CONFIG_HAS_EARLYSUSPEND
	gnss_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING + 30;
	gnss_suspend.suspend = gnss_early_suspend;
	gnss_suspend.resume	= gnss_late_resume;
	register_early_suspend(&gnss_suspend);

	init_waitqueue_head(&gnss_sleep_wait);
	#endif
	return err;
}

static void __exit sprd_power_ctl_cleanup(void)
{
	misc_deregister(&sprd_power_ctl_device);
}
module_init(sprd_power_ctl_init);
module_exit(sprd_power_ctl_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sprd download img driver");

