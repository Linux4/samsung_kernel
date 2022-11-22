/*
 * Marvell HDMI UIO driver, support hdmi@ MMP3/NEVO
 *
 * Yifan Zhang <zhangyf@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 * (c) 2010
 *
 */

#include <linux/module.h>
#include <linux/uio_driver.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <mach/uio_hdmi.h>
#if defined(CONFIG_CPU_MMP3)
#include <mach/addr-map.h>
#include <mach/cputype.h>
#endif

#define HDMI_FREQ_CONSTRAINT (-1)

static atomic_t hdmi_state = ATOMIC_INIT(0);
enum connect_lock con_lock;
static bool timer_inited = 0;
static int late_disable_flag;
static int early_suspend_flag;
#if defined(CONFIG_CPU_PXA978)
static int suspend_flag;
extern int hdmi_conv_on;
static int dvfm_dev_idx;
static unsigned int *arb_f_mc, *arb_n_mc;
static unsigned int val_f_mc, val_n_mc;
extern void update_lcd_controller_clock(int on);
#endif

enum connect_status {
	CABLE_CONNECT = 0,
	CABLE_DISCNCT = 1,
};

enum hdmi_status {
	HDMI_OFF = 0,
	HDMI_ON = 1,
};

struct hdmi_instance {
	struct clk *clk;
	void *reg_base;
	void *sspa1_reg_base;
	unsigned int hpd_in; /* if cable plug in, this is the gpio value*/
	unsigned int gpio;
	unsigned int edid_bus_num;
	struct timer_list jitter_timer;
	struct work_struct work;
	struct delayed_work delay_resumed;
	struct delayed_work delay_disable;
	struct uio_info uio_info;
	int (*hdmi_power)(int on);
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend    early_suspend;
#endif
};

static void set_power_constraint(struct hdmi_instance *hi, int min)
{
}

static void unset_power_constraint(struct hdmi_instance *hi)
{
}

#if defined(CONFIG_CPU_MMP3)
static u32 hdmi_direct_read(unsigned addr)
{
	u32 *hdmi_addr = AXI_VIRT_BASE + 0xbc00;

	return __raw_readl(hdmi_addr + addr);
}
static void hdmi_direct_write(unsigned addr, unsigned data)
{
	u32 *hdmi_addr = AXI_VIRT_BASE + 0xbc00;

	__raw_writel(data, hdmi_addr + addr);
}

void hdmi_3d_sync_view(int right)
{
	u32 v = right ? 0x7 : 0xf;
	hdmi_direct_write(0x30, v);
}
EXPORT_SYMBOL(hdmi_3d_sync_view);
#endif

int hdmi_open(struct uio_info *info, struct inode *inode, void *file_priv)
{
	return 0;
}

int hdmi_release(struct uio_info *info, struct inode *indoe, void *file_priv)
{
	return 0;
}

static int hdmi_ioctl(struct uio_info *info, unsigned cmd, unsigned long arg,
		void *file_priv)
{
	unsigned offset, val;
	void *argp = (void *)arg;
	struct hdmi_instance *hi =
		container_of(info, struct hdmi_instance, uio_info);
	int hpd = CABLE_DISCNCT;

	switch (cmd) {
	case SSPA1_GET_VALUE:
		if (copy_from_user(&offset, argp, sizeof(offset)))
			return -EFAULT;
		val = readl(hi->sspa1_reg_base + offset);
		if (copy_to_user(argp, &val, sizeof(val)))
			return -EFAULT;
		break;
	case HPD_PIN_READ:
		/* when resume, force disconnect/connect HDMI */
#if defined(CONFIG_CPU_MMP3)
		if (con_lock == FIRST_ACCESS_LOCK) {
			hpd = CABLE_DISCNCT;
			con_lock = SECOND_ACCESS_LOCK;
		} else if (con_lock == SECOND_ACCESS_LOCK) {
			hpd = CABLE_CONNECT;
			con_lock = UNLOCK;
		}
#elif defined(CONFIG_CPU_PXA978)
		if (con_lock == FIRST_ACCESS_LOCK) {
			hpd = CABLE_DISCNCT;
			con_lock = UNLOCK;
		}
#endif
		else {
			if (atomic_read(&hdmi_state) == HDMI_ON)
				hpd = CABLE_CONNECT;
			else {
				hpd = CABLE_DISCNCT;
				/*if disconnected HDMI,
				 * 300 ms is the time wait for HDMI is
				 * disabled, then disp1_axi_bus can be disabled.
				 * disp1_axi_bus clear will cause HDMI clock
				 * disbaled and any operation not takes effects*/
				if (late_disable_flag)
					schedule_delayed_work(&hi->delay_disable,
						msecs_to_jiffies(200));
			}
		}
		if (copy_to_user(argp, &hpd, sizeof(int))) {
			pr_err("copy_to_user error !~!\n");
			return -EFAULT;
		}
		printk("uio_hdmi: report cable %s to hdmi-service\n",
			(hpd == CABLE_CONNECT) ? "pulg in" : "pull out");
		break;
	case EDID_NUM:
		if (copy_to_user(argp, &hi->edid_bus_num,
					sizeof(unsigned int))) {
			pr_err("copy to user error !\n");
			return -EFAULT;
		}
		break;
#ifdef CONFIG_CPU_PXA978
	case HDMI_PLL_ENABLE:
		clk_enable(hi->clk);
		break;
	case HDMI_PLL_DISABLE:
		clk_disable(hi->clk);
		break;
	case HDMI_PLL_SETRATE: {
			int hdmi_freq = 0;
			if (copy_from_user(&hdmi_freq, argp, sizeof(hdmi_freq)))
				return -EFAULT;
			printk("uio_hdmi: set TMDS clk freq = %dMhz\n", hdmi_freq/5);
			if (clk_set_rate(hi->clk, hdmi_freq * 1000000)) {
				pr_err(KERN_ERR "uio_hdmi: HDMI PLL set failed!\n");
				return -EFAULT;
			}
		}
		break;
#endif
	default:
		pr_err("uio_hdmi: no suppprt ioctl!\n");
		return -EFAULT;
	}
	return 0;
}

static int hdmi_remove(struct platform_device *pdev)
{
	return 0;
}

#if defined(CONFIG_CPU_PXA978)
static void arbiter_init(void)
{
	arb_f_mc = ioremap_nocache(0x7ff007b0, 4);
	arb_n_mc = ioremap_nocache(0x7ff00280, 4);
	val_f_mc = *arb_f_mc;
	val_n_mc = *arb_n_mc;
}

static void arbiter_set(void)
{
	/* raise priority of display controller mc arbiter*/
	val_f_mc = *arb_f_mc;
	val_n_mc = *arb_n_mc;
	*arb_f_mc = 0x010f0101;
	*arb_n_mc = 0x010f0101;
}

static void arbiter_clr(void)
{
	*arb_f_mc = val_f_mc;
	*arb_n_mc = val_n_mc;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
static void hdmi_early_suspend_nevo(struct early_suspend *h)
{
	return;
}

static void hdmi_late_resume_nevo(struct early_suspend *h)
{
	struct hdmi_instance *hi =
		container_of(h, struct hdmi_instance, early_suspend);
	if (suspend_flag == 1) {
		/* always turn on 5v power*/
		if (hi->hdmi_power)
			hi->hdmi_power(1);

		con_lock = FIRST_ACCESS_LOCK;
		/* send disconnect event to upper layer */
		uio_event_notify(&hi->uio_info);
		mod_timer(&hi->jitter_timer, jiffies + HZ/4);
		suspend_flag = 0;
	}
	printk("uio_hdmi: hdmi late resume done!\n");
	return;
}
#endif

static int hdmi_suspend_nevo(struct platform_device *pdev, pm_message_t mesg)
{
	struct hdmi_instance *hi = platform_get_drvdata(pdev);
	suspend_flag = 1;
	if (atomic_read(&hdmi_state) == HDMI_ON) {
		clk_disable(hi->clk);
		if (hi->hdmi_power)
			hi->hdmi_power(0);
		arbiter_clr();
		atomic_set(&hdmi_state, HDMI_OFF);
		unset_power_constraint(hi);
		update_lcd_controller_clock(0);
	}
	/* always turn off 5v power*/
	if (hi->hdmi_power)
		hi->hdmi_power(0);
	pdev->dev.power.power_state = mesg;
	del_timer(&hi->jitter_timer);
	printk("uio_hdmi: suspend done!\n");
	return 0;
}

static int hdmi_resume_nevo(struct platform_device *pdev)
{
	return 0;
}

#elif defined(CONFIG_CPU_MMP3)
#ifdef CONFIG_HAS_EARLYSUSPEND
static void hdmi_early_suspend_mmp(struct early_suspend *h)
{
	struct hdmi_instance *hi =
		container_of(h, struct hdmi_instance, early_suspend);
	if (atomic_read(&hdmi_state) == HDMI_ON)
		unset_power_constraint(hi);
	early_suspend_flag = 1;
	return;
}
static void hdmi_late_resume_mmp(struct early_suspend *h)
{
	struct hdmi_instance *hi =
		container_of(h, struct hdmi_instance, early_suspend);
	if (atomic_read(&hdmi_state) == HDMI_ON)
		set_power_constraint(hi, HDMI_FREQ_CONSTRAINT);
	early_suspend_flag = 0;
	return;
}
#endif
static int hdmi_suspend_mmp(struct platform_device *pdev, pm_message_t mesg)
{
	struct hdmi_instance *hi = platform_get_drvdata(pdev);

	/* MMP3 */
	clk_disable(hi->clk);
	if (hi->hdmi_power)
		hi->hdmi_power(0);
	pdev->dev.power.power_state = mesg;
	printk("uio_hdmi: suspend done!\n");
	return 0;
}

static int hdmi_resume_mmp(struct platform_device *pdev)
{
	struct hdmi_instance *hi = platform_get_drvdata(pdev);

	/* always turn on 5v power and clock*/
	if (hi->hdmi_power)
		hi->hdmi_power(1);
	clk_enable(hi->clk);

	if (gpio_get_value(hi->gpio) == hi->hpd_in) {
		/*if connected, reset HDMI*/
		atomic_set(&hdmi_state, HDMI_ON);
		con_lock = FIRST_ACCESS_LOCK;
		/* send disconnect event to upper layer */
		uio_event_notify(&hi->uio_info);
		/*if uio_event_notify both directly, 1 event will be
		 * missed, so delayed_work*/
		schedule_delayed_work(&hi->delay_resumed,
			msecs_to_jiffies(1500));
	} else if (atomic_read(&hdmi_state) == HDMI_ON) {
		atomic_set(&hdmi_state, HDMI_OFF);
		if (early_suspend_flag == 0)
			unset_power_constraint(hi);
		uio_event_notify(&hi->uio_info);
	}

	return 0;
}

static void delayed_resume(struct work_struct *work)
{
	struct hdmi_instance *hi = container_of((struct delayed_work *)work,
			struct hdmi_instance, delay_resumed);
	/* send connect event to upper layer */
	uio_event_notify(&hi->uio_info);
}
#endif

static void delayed_disable(struct work_struct *work)
{
	struct hdmi_instance *hi = container_of((struct delayed_work *)work,
			struct hdmi_instance, delay_disable);
	if (late_disable_flag) {
#if defined(CONFIG_CPU_PXA978)
		if (atomic_read(&hdmi_state) == HDMI_OFF) {
			if (hdmi_conv_on) {
				printk(KERN_ERR "uio_hdmi: ERROR!!! hdmi clk"
				"should be off but lcd is still on ?!!\n");
				WARN_ON(1);
				return;
			}
			update_lcd_controller_clock(0);
		} else {
			printk(KERN_WARNING "uio_hdmi: WARN!!!"
			"hdmi is connected but ask for clk off!!\n");
		}
		clk_disable(hi->clk);
#endif
		late_disable_flag = 0;
	}
}

static void hdmi_switch_work(struct work_struct *work)
{
	struct hdmi_instance *hi =
		container_of(work, struct hdmi_instance, work);
	int state = gpio_get_value(hi->gpio);
	if (state != hi->hpd_in) {
		if (atomic_cmpxchg(&hdmi_state, HDMI_ON, HDMI_OFF) == HDMI_ON) {
			late_disable_flag = 1;
#if defined(CONFIG_CPU_MMP3)
			if (early_suspend_flag == 0)
				unset_power_constraint(hi);
#elif defined(CONFIG_CPU_PXA978)
			arbiter_clr();
			unset_power_constraint(hi);
#endif
			/*if hdmi_state change, report hpd*/
			uio_event_notify(&hi->uio_info);
		}
	} else {
		if (atomic_cmpxchg(&hdmi_state, HDMI_OFF, HDMI_ON) == HDMI_OFF) {
#if defined(CONFIG_CPU_MMP3)
			if (early_suspend_flag == 0)
				set_power_constraint(hi, HDMI_FREQ_CONSTRAINT);
#elif defined(CONFIG_CPU_PXA978)
			set_power_constraint(hi, HDMI_FREQ_CONSTRAINT);
			clk_enable(hi->clk);
			arbiter_set();
			update_lcd_controller_clock(1);
#endif
			/*if hdmi_state change, report hpd*/
			uio_event_notify(&hi->uio_info);
		}
	}
	pr_debug("++++++++++++ %s state %x hdmi_state %d\n", __func__,
		state, atomic_read(&hdmi_state));
}

/* set_power_constraint can't be called in interrupt context, use timer to
 * workaround this issue.
 */
void work_launch(unsigned long data)
{
	struct hdmi_instance *hi = (struct hdmi_instance *)data;
	pr_debug("%s\n", __func__);
	schedule_work(&hi->work);
}

/* use timer to remove jitter
 */
static irqreturn_t hpd_handler(int irq, struct uio_info *dev_info)
{
	struct hdmi_instance *hi =
		container_of(dev_info, struct hdmi_instance, uio_info);

	pr_debug("%s\n", __func__);
#ifdef CONFIG_CPU_PXA978
	/* during suspend, need not response to any irq*/
	if (suspend_flag) {
		printk("uio_hdmi: ~ignore all hpd during suspend~\n");
		return IRQ_NONE;
	}
#endif
	if (timer_inited)
		mod_timer(&hi->jitter_timer, jiffies + HZ);
#ifdef CONFIG_CPU_PXA978
	/*
	*cable in -> lcd on, but if lcd on is not completed,
	*and cable out at this momoent, lcd mixer failed happend if hdmi clk disable
	*/
	if (HDMI_ON == atomic_read(&hdmi_state)
		&& (hi->hpd_in != gpio_get_value(hi->gpio))) {
		if (0 == hdmi_conv_on) {
			printk("uio_hdmi: delay 5s to handle cable pull out,"
			"as lcd is not turn on completed at last time cable in!!!\n");
			mod_timer(&hi->jitter_timer, jiffies + 5*HZ);
		}
	}
#endif
	/*printk("uio_hdmi: irq HDMI cable is %s\n",
		(hi->hpd_in == gpio_get_value(hi->gpio))?"plug in":"pull out");*/
	/*Don't report hpd in top half, wait for jitter is gone.*/
	return IRQ_NONE;
}

static int hdmi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct hdmi_instance *hi;
	int ret;
	struct uio_hdmi_platform_data *pdata;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pdata = pdev->dev.platform_data;
	if (res == NULL) {
		printk(KERN_ERR "hdmi_probe: no memory resources given");
		return -ENODEV;
	}

	hi = kzalloc(sizeof(*hi), GFP_KERNEL);
	if (hi == NULL) {
		printk(KERN_ERR "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	hi->clk = clk_get(NULL, "HDMICLK");
	if (IS_ERR(hi->clk)) {
		pr_err("%s: can't get HDMICLK\n", __func__);
		kfree(hi);
		return  -EIO;
	}

	hi->reg_base = ioremap(res->start, res->end - res->start + 1);
	if (hi->reg_base == NULL) {
		printk(KERN_ERR "%s: can't remap resgister area", __func__);
		ret =  -ENOMEM;
		goto out_free;
	}

	if (pdata->sspa_reg_base) {
		hi->sspa1_reg_base = ioremap_nocache(pdata->sspa_reg_base, 0xff);
		if (hi->sspa1_reg_base == NULL) {
			printk(KERN_WARNING "failed to request register memory\n");
			ret = -EBUSY;
			goto out_free;
		}
	}
	platform_set_drvdata(pdev, hi);

	hi->uio_info.name = "uio-hdmi";
	hi->uio_info.version = "build1";
	hi->uio_info.irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
	hi->uio_info.handler = hpd_handler;
	hi->gpio = pdata->gpio;
	hi->hpd_in = pdata->hpd_val;
	hi->edid_bus_num = pdata->edid_bus_num;
	if (hi->edid_bus_num == 0)
		hi->edid_bus_num = 6;

	ret = gpio_request(pdata->gpio, pdev->name);
	if (ret < 0) {
		printk(KERN_ERR "%s: failed in gpio_request\n", __func__);
		goto out_free;
	}
	ret = gpio_direction_input(pdata->gpio);
	if (ret < 0) {
		printk(KERN_ERR "%s: failed in gpio_direction_input\n", __func__);
		goto out_free;
	}
	hi->uio_info.irq = gpio_to_irq(pdata->gpio);
	if (hi->uio_info.irq < 0) {
		printk(KERN_ERR "%s: failed in gpio_to_irq\n", __func__);
		ret = hi->uio_info.irq;
		goto out_free;
	}

	hi->uio_info.mem[0].internal_addr = hi->reg_base;
	hi->uio_info.mem[0].addr = res->start;
	hi->uio_info.mem[0].memtype = UIO_MEM_PHYS;
	hi->uio_info.mem[0].size = res->end - res->start + 1;
	hi->uio_info.mem[0].name = "hdmi-iomap";
	hi->uio_info.priv = hi;

	if (pdata->itlc_reg_base) {
		hi->uio_info.mem[1].internal_addr =
			ioremap_nocache(pdata->itlc_reg_base, 0xff);
		hi->uio_info.mem[1].addr = pdata->itlc_reg_base;
		hi->uio_info.mem[1].memtype = UIO_MEM_PHYS;
		hi->uio_info.mem[1].size = 0xff;
	}

	hi->uio_info.open = hdmi_open;
	hi->uio_info.release = hdmi_release;
	hi->uio_info.ioctl = hdmi_ioctl;
	if (pdata->hdmi_v5p_power)
		hi->hdmi_power = pdata->hdmi_v5p_power;

#if defined(CONFIG_CPU_PXA978)
	arbiter_init();
	/* nevo need 5v to detect hpd*/
	if (hi->hdmi_power)
		hi->hdmi_power(1);
#endif

#if	defined(CONFIG_CPU_MMP3)
	/* mmp3 need 5v to detect hpd*/
	if (hi->hdmi_power)
		hi->hdmi_power(1);
	/* mmp3 always enable clk*/
	clk_enable(hi->clk);
#endif
	/* Check HDMI cable when boot up */
	ret = gpio_get_value(hi->gpio);
	printk(KERN_INFO"%s hpd %s\n",
		__func__, (ret == hi->hpd_in) ? "plug in" : "pull out");

	if (ret == hi->hpd_in) {
		atomic_set(&hdmi_state, HDMI_ON);
		set_power_constraint(hi, HDMI_FREQ_CONSTRAINT);

#if defined(CONFIG_CPU_PXA978)
		arbiter_set();
		clk_enable(hi->clk);
		update_lcd_controller_clock(1);
#endif
	}

	ret = uio_register_device(&pdev->dev, &hi->uio_info);
	if (ret) {
		printk(KERN_ERR"%s: register device fails !!!\n", __func__);
		goto out_free;
	}

	/* avoid cable hot plug/pull out jitter within 1s*/
	setup_timer(&hi->jitter_timer, work_launch, (unsigned long)hi);
	INIT_WORK(&hi->work, hdmi_switch_work);
	timer_inited = 1;

	/* silicon issue on MMP: delayed 300ms to disable clk when cable pull out*/
	INIT_DELAYED_WORK(&hi->delay_disable, delayed_disable);
#if defined(CONFIG_CPU_MMP3)
	/* during resume: simulate cable pull out->wait 1.5s->plug in*/
	INIT_DELAYED_WORK(&hi->delay_resumed, delayed_resume);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#if defined(CONFIG_CPU_MMP3)
	hi->early_suspend.suspend = hdmi_early_suspend_mmp;
	hi->early_suspend.resume = hdmi_late_resume_mmp;
#elif defined(CONFIG_CPU_PXA978)
	hi->early_suspend.suspend = hdmi_early_suspend_nevo;
	hi->early_suspend.resume = hdmi_late_resume_nevo;
#endif
	hi->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&hi->early_suspend);
#endif
	platform_set_drvdata(pdev, hi);
	return 0;

out_free:
	clk_disable(hi->clk);
	clk_put(hi->clk);
	if (hi->hdmi_power)
		hi->hdmi_power(0);
	kfree(hi);
	return ret;
}

static struct platform_driver hdmi_driver = {
	.probe	= hdmi_probe,
	.remove	= hdmi_remove,
	.driver = {
		.name	= "uio-hdmi",
		.owner	= THIS_MODULE,
	},
#ifdef CONFIG_PM
#if defined(CONFIG_CPU_PXA978)
	.suspend = hdmi_suspend_nevo,
	.resume  = hdmi_resume_nevo,
#elif defined(CONFIG_CPU_MMP3)
	.suspend = hdmi_suspend_mmp,
	.resume  = hdmi_resume_mmp,
#endif
#endif
};

static void __init hdmi_exit(void)
{
	platform_driver_unregister(&hdmi_driver);
}

static int __init hdmi_init(void)
{
	return platform_driver_register(&hdmi_driver);
}

late_initcall(hdmi_init);
module_exit(hdmi_exit);

MODULE_DESCRIPTION("UIO driver for Marvell hdmi");
MODULE_LICENSE("GPL");
