/* linux/driver/video/samsung/lcdfreq.c
 *
 * EXYNOS4 - support LCD PixelClock change at runtime
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/reboot.h>
#include <linux/suspend.h>
#include <linux/cpufreq.h>
#include <linux/sysfs.h>
#include <linux/gcd.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>

#include <plat/clock.h>
#include <plat/clock-clksrc.h>
#include <plat/regs-fb.h>
#ifdef CONFIG_FB_S5P_MDNIE
#include <plat/regs-ielcd.h>
#endif

#include <mach/map.h>

#include "exynos_display_handler.h"

#define FIMD_REG_BASE			S5P_PA_FIMD1
#ifdef CONFIG_FB_EXYNOS_FIMD_V8
#define FIMD_MAP_SIZE			SZ_256K
#else
#define FIMD_MAP_SIZE			SZ_32K
#endif

#define IELCD_REG_BASE		S3C_IELCD_PHY_BASE
#define IELCD_MAP_SIZE		SZ_1K

#ifdef CONFIG_FB_S5P_MDNIE
#define REG_BASE			IELCD_REG_BASE
#define MAP_SIZE			IELCD_MAP_SIZE
#define REG_VIDCON1			IELCD_VIDCON1
#else
#define REG_BASE			FIMD_REG_BASE
#define MAP_SIZE			FIMD_MAP_SIZE
#define REG_VIDCON1			VIDCON1
#endif

#define VSTATUS_IS_ACTIVE(reg)	(reg == VIDCON1_VSTATUS_ACTIVE)
#define VSTATUS_IS_FRONT(reg)		(reg == VIDCON1_VSTATUS_FRONTPORCH)

#define reg_mask(shift, size)		((0xffffffff >> (32 - size)) << shift)

enum lcdfreq_level {
	NORMAL,
	LIMIT,
	LEVEL_MAX,
};

struct lcdfreq_t {
	enum lcdfreq_level level;
	u32 hz;
	u32 vclk;
	u32 cmu_div;
	u32 pixclock;
};

struct lcdfreq_info {
	struct lcdfreq_t	table[LEVEL_MAX];
	enum lcdfreq_level	level;
	atomic_t		usage;
	struct mutex		lock;
	spinlock_t		slock;

	struct device		*dev;
	struct clksrc_clk	*clksrc;

	unsigned int		enable;
	struct notifier_block	pm_noti;
	struct notifier_block	reboot_noti;
	struct notifier_block	fb_notif;

	struct delayed_work	work;

	void __iomem		*reg;

	struct clk		*bus_clk;
	struct clk		*lcd_clk;
};

static int match_name(struct device *dev, void *data)
{
	const char *name = data;

	return !strncmp(dev_name(dev), name, strlen(name));
}

static inline struct lcdfreq_info *dev_get_lcdfreq(struct device *dev)
{
	struct device *child;
	struct lcdfreq_info *info = NULL;

	child = device_find_child(dev->parent, "lcdfreq", match_name);

	info = dev_get_drvdata(child);

	return info;
}

static unsigned int get_vstatus(struct device *dev)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);
	u32 reg;

	reg = readl(info->reg + REG_VIDCON1);

	reg &= VIDCON1_VSTATUS_MASK;

	return reg;
}

static struct clksrc_clk *get_clksrc(struct device *dev)
{
	struct clk *clk;
	struct clksrc_clk *clksrc;

	clk = clk_get(dev, "sclk_fimd");

	clksrc = container_of(clk, struct clksrc_clk, clk);

	return clksrc;
}

static void reset_div(struct device *dev)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);
	struct clksrc_clk *clksrc = info->clksrc;

	u32 reg = __raw_readl(clksrc->reg_div.reg);
	u32 mask = reg_mask(clksrc->reg_div.shift, clksrc->reg_div.size);

	reg &= ~(mask);
	reg |= (info->table[info->level].cmu_div << clksrc->reg_div.shift);

	writel(reg, clksrc->reg_div.reg);
}

static int get_div(struct device *dev)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);
	struct clksrc_clk *clksrc = info->clksrc;

	u32 reg = __raw_readl(clksrc->reg_div.reg);
	u32 mask = reg_mask(clksrc->reg_div.shift, clksrc->reg_div.size);

	reg &= mask;
	reg >>= clksrc->reg_div.shift;

	return reg;
}

static int set_div(struct device *dev, u32 div)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);
	struct clksrc_clk *clksrc = info->clksrc;
	u32 mask = reg_mask(clksrc->reg_div.shift, clksrc->reg_div.size);

	unsigned long flags;
	u32 reg, count = 1000000;

	do {
		spin_lock_irqsave(&info->slock, flags);
		reg = __raw_readl(clksrc->reg_div.reg);

		if ((reg & mask) == ((div << clksrc->reg_div.shift) & mask)) {
			spin_unlock_irqrestore(&info->slock, flags);
			return -EINVAL;
		}

		reg &= ~(mask);
		reg |= (div << clksrc->reg_div.shift);

		if (VSTATUS_IS_ACTIVE(get_vstatus(dev))) {
			if (VSTATUS_IS_FRONT(get_vstatus(dev))) {
				writel(reg, clksrc->reg_div.reg);
				spin_unlock_irqrestore(&info->slock, flags);
				dev_info(dev, "%x, %d\n", __raw_readl(clksrc->reg_div.reg), 1000000-count);
				return 0;
			}
		}
		spin_unlock_irqrestore(&info->slock, flags);
		count--;
	} while (count && info->enable);

	dev_err(dev, "%s skip, div=%d\n", __func__, div);

	return -EINVAL;
}

static unsigned char get_fimd_divider(void)
{
	void __iomem *fimd_reg;
	unsigned int reg;

	fimd_reg = ioremap(FIMD_REG_BASE, FIMD_MAP_SIZE);

	reg = readl(fimd_reg + VIDCON0);
	reg &= VIDCON0_CLKVAL_F_MASK;
	reg >>= VIDCON0_CLKVAL_F_SHIFT;

	iounmap(fimd_reg);

	return reg;
}

static int get_divider(struct device *dev)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);
	struct clksrc_clk *clksrc;
	struct clk *clk;
	u32 rate, reg, fimd_div, i;

	info->clksrc = clksrc = get_clksrc(dev->parent);
	clk = clk_get_parent(&clksrc->clk);
	rate = clk_get_rate(clk);

	for (i = 0; i < LEVEL_MAX; i++)
		info->table[i].cmu_div = DIV_ROUND_CLOSEST(rate, info->table[i].vclk);

	if (info->table[LIMIT].cmu_div > (1 << clksrc->reg_div.size))
		fimd_div = gcd(info->table[NORMAL].cmu_div, info->table[LIMIT].cmu_div);
	else
		fimd_div = 1;

	dev_info(dev, "%s rate is %d, fimd div=%d\n", clk->name, rate, fimd_div);

	reg = get_fimd_divider() + 1;

	if ((!fimd_div) || (fimd_div >= VIDCON0_CLKVAL_F_LIMIT) || (fimd_div != reg)) {
		dev_info(dev, "%s skip, fimd div=%d, reg=%d\n", __func__, fimd_div, reg);
		goto err;
	}

	for (i = 0; i < LEVEL_MAX; i++) {
		info->table[i].cmu_div /= fimd_div;
		if (info->table[i].cmu_div > (1 << clksrc->reg_div.size)) {
			dev_info(dev, "%s skip, cmu div=%d\n", __func__, info->table[i].cmu_div);
			goto err;
		}
		dev_info(dev, "%d div is %d\n", info->table[i].hz, info->table[i].cmu_div);
		info->table[i].cmu_div--;
	}

	reg = get_div(dev);
	if (info->table[NORMAL].cmu_div != reg) {
		dev_info(dev, "%s skip, cmu div=%d, reg=%d\n", __func__, info->table[NORMAL].cmu_div, reg);
		goto err;
	}

	for (i = 0; i < LEVEL_MAX; i++) {
		reg = info->table[i].cmu_div;
		info->table[i].cmu_div = reg;
	}

	return 0;

err:
	return -EINVAL;
}

static int set_lcdfreq_div(struct device *dev, enum lcdfreq_level level)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);

	u32 ret;

	mutex_lock(&info->lock);

	if (!info->enable) {
		dev_err(dev, "%s reject. enable flag is %d\n", __func__, info->enable);
		ret = -EINVAL;
		goto exit;
	}

	ret = set_div(dev, info->table[level].cmu_div);

	if (ret) {
		dev_err(dev, "skip to change lcd freq\n");
		goto exit;
	}

	info->level = level;

exit:
	mutex_unlock(&info->lock);

	return ret;
}

static int lcdfreq_lock(struct device *dev, int value)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);

	int ret;

	if (!atomic_read(&info->usage))
		ret = set_lcdfreq_div(dev, value);
	else {
		dev_err(dev, "lcd freq is already limit state\n");
		return -EINVAL;
	}

	if (!ret) {
		mutex_lock(&info->lock);
		atomic_inc(&info->usage);
		mutex_unlock(&info->lock);
		schedule_delayed_work(&info->work, 0);
	}

	return ret;
}

static int lcdfreq_lock_free(struct device *dev)
{
	struct lcdfreq_info *info = dev_get_drvdata(dev);

	int ret;

	if (atomic_read(&info->usage))
		ret = set_lcdfreq_div(dev, NORMAL);
	else {
		dev_err(dev, "lcd freq is already normal state\n");
		return -EINVAL;
	}

	if (!ret) {
		mutex_lock(&info->lock);
		atomic_dec(&info->usage);
		mutex_unlock(&info->lock);
		cancel_delayed_work(&info->work);
	}

	return ret;
}

static ssize_t level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcdfreq_info *info = dev_get_lcdfreq(dev);

	if (!info->enable) {
		dev_err(info->dev, "%s reject. enable flag is %d\n", __func__, info->enable);
		return -EINVAL;
	}

	return sprintf(buf, "%d, div=%d\n", info->table[info->level].hz, get_div(info->dev));
}

static ssize_t level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct lcdfreq_info *info = dev_get_lcdfreq(dev);
	unsigned int value;
	int ret;

	if (!info->enable) {
		dev_err(info->dev, "%s reject. enable flag is %d\n", __func__, info->enable);
		return -EINVAL;
	}

	ret = kstrtoul(buf, 0, (unsigned long *)&value);

	dev_info(info->dev, "%s :: value=%d\n", __func__, value);

	if (value >= LEVEL_MAX)
		return -EINVAL;

	if (value)
		ret = lcdfreq_lock(info->dev, value);
	else
		ret = lcdfreq_lock_free(info->dev);

	if (ret) {
		dev_err(info->dev, "%s skip\n", __func__);
		return -EINVAL;
	}
	return count;
}

static ssize_t usage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcdfreq_info *info = dev_get_lcdfreq(dev);

	return sprintf(buf, "%d\n", atomic_read(&info->usage));
}

static DEVICE_ATTR(level, S_IRUGO|S_IWUSR, level_show, level_store);
static DEVICE_ATTR(usage, S_IRUGO, usage_show, NULL);

static struct attribute *lcdfreq_attributes[] = {
	&dev_attr_level.attr,
	&dev_attr_usage.attr,
	NULL,
};

static struct attribute_group lcdfreq_attr_group = {
	.name = "lcdfreq",
	.attrs = lcdfreq_attributes,
};

static int lcdfreq_enable(void *data, unsigned long power)
{
	struct lcdfreq_info *info = data;

	if ((power == FB_BLANK_POWERDOWN) && (info->enable)) {
		mutex_lock(&info->lock);
		info->enable = false;
		info->level = NORMAL;
		reset_div(info->dev);
		atomic_set(&info->usage, 0);
		mutex_unlock(&info->lock);

		pm_runtime_put_sync(info->dev);

		dev_info(info->dev, "%s, %ld, %d\n", __func__, power, info->enable);
	}

	return 0;
}

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct lcdfreq_info *info;
	struct fb_event *evdata = data;
	int blank;

	switch (event) {
	case FB_EVENT_BLANK:
		break;
	default:
		return 0;
	}

	info = container_of(self, struct lcdfreq_info, fb_notif);
	if (!info)
		return 0;

	blank = *(int *)evdata->data;

	if ((blank == FB_BLANK_UNBLANK) && (!info->enable)) {
		pm_runtime_get_sync(info->dev);

		mutex_lock(&info->lock);
		info->enable = true;
		mutex_unlock(&info->lock);

		dev_info(info->dev, "%s, %d, %d\n", __func__, blank, info->enable);
	}

	return 0;
}

static int lcdfreq_reboot_notify(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct lcdfreq_info *info =
		container_of(this, struct lcdfreq_info, reboot_noti);

	mutex_lock(&info->lock);
	if ((info->enable) && (info->level))
		reset_div(info->dev);
	info->level = NORMAL;
	info->enable = false;
	atomic_set(&info->usage, 0);
	mutex_unlock(&info->lock);

	dev_info(info->dev, "%s\n", __func__);

	return NOTIFY_DONE;
}

static void lcdfreq_status_work(struct work_struct *work)
{
	struct lcdfreq_info *info =
		container_of(work, struct lcdfreq_info, work.work);

	u32 hz = info->table[info->level].hz;

	cancel_delayed_work(&info->work);

	dev_info(info->dev, "%d, %d\n", hz, atomic_read(&info->usage));

	schedule_delayed_work(&info->work, HZ*120);
}

static struct fb_videomode *get_videmode(struct list_head *list)
{
	struct fb_modelist *modelist;
	struct list_head *pos;
	struct fb_videomode *m = NULL;

	if (!list->prev || !list->next || list_empty(list))
		goto exit;

	list_for_each(pos, list) {
		modelist = list_entry(pos, struct fb_modelist, list);
		m = &modelist->mode;
	}

exit:
	return m;
}

static int clk_init(struct device *dev)
{
	int ret = 0;
	struct lcdfreq_info *info = dev_get_drvdata(dev);

	info->bus_clk = clk_get(dev->parent, "lcd");
	if (IS_ERR(info->bus_clk)) {
		dev_err(dev, "failed to get bus clock\n");
		ret = PTR_ERR(info->bus_clk);
		goto exit;
	}

	info->lcd_clk = clk_get(dev->parent, "sclk_fimd");
	if (IS_ERR(info->lcd_clk)) {
		dev_err(dev, "failed to get lcd clock\n");
		ret = PTR_ERR(info->lcd_clk);
		goto err_bus_clk;
	}

	return 0;

err_bus_clk:
	clk_disable(info->bus_clk);
	clk_put(info->bus_clk);
exit:
	return ret;
}

static int lcdfreq_probe(struct platform_device *pdev)
{
	struct fb_info *fb = registered_fb[0];
	struct fb_videomode *m;
	struct lcdfreq_info *info;
	unsigned int vclk, limit;
	int ret = 0;

	m = get_videmode(&fb->modelist);
	if (!m) {
		pr_err("fail to get_videmode\n");
		goto err_1;
	}

	info = kzalloc(sizeof(struct lcdfreq_info), GFP_KERNEL);
	if (!info) {
		pr_err("fail to allocate for lcdfreq\n");
		ret = -ENOMEM;
		goto err_1;
	}

	limit = (unsigned int)pdev->dev.platform_data;
	if (!limit) {
		pr_err("skip to get platform data for lcdfreq\n");
		ret = -EINVAL;
		goto err_2;
	}

	platform_set_drvdata(pdev, info);

	info->dev = &pdev->dev;
	info->level = NORMAL;

	vclk = (m->left_margin + m->right_margin + m->hsync_len + m->xres) *
		(m->upper_margin + m->lower_margin + m->vsync_len + m->yres);

	info->table[NORMAL].level = NORMAL;
	info->table[NORMAL].vclk = vclk * m->refresh;
	info->table[NORMAL].pixclock = m->pixclock;
	info->table[NORMAL].hz = m->refresh;

	info->table[LIMIT].level = LIMIT;
	info->table[LIMIT].vclk = vclk * limit;
	info->table[LIMIT].pixclock = KHZ2PICOS((vclk * limit)/1000);
	info->table[LIMIT].hz = limit;

	ret = get_divider(info->dev);
	if (ret < 0) {
		pr_err("%s skip: get_divider", __func__);
		goto err_2;
	}

	atomic_set(&info->usage, 0);
	mutex_init(&info->lock);
	spin_lock_init(&info->slock);

	INIT_DELAYED_WORK_DEFERRABLE(&info->work, lcdfreq_status_work);

	ret = sysfs_create_group(&fb->dev->kobj, &lcdfreq_attr_group);
	if (ret < 0) {
		pr_err("%s skip: sysfs_create_group\n", __func__);
		goto err_2;
	}

	info->reboot_noti.notifier_call = lcdfreq_reboot_notify;
	register_reboot_notifier(&info->reboot_noti);

	info->fb_notif.notifier_call = fb_notifier_callback;
	fb_register_client(&info->fb_notif);

	register_display_handler(lcdfreq_enable, SUSPEND_LEVEL_LCDFREQ, info);

	info->reg = ioremap(REG_BASE, MAP_SIZE);
	info->enable = true;

	ret = clk_init(&pdev->dev);
	if (ret) {
		pr_err("%s skip: clk_init\n", __func__);
		goto err_3;
	}

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	dev_info(info->dev, "%s is done\n", __func__);

	return 0;

err_3:
	iounmap(info->reg);
	sysfs_remove_group(&fb->dev->kobj, &lcdfreq_attr_group);
err_2:
	kfree(info);
err_1:
	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int lcdfreq_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lcdfreq_info *info = platform_get_drvdata(pdev);

	clk_disable(info->lcd_clk);
	clk_disable(info->bus_clk);

	return 0;
}

static int lcdfreq_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lcdfreq_info *info = platform_get_drvdata(pdev);

	clk_enable(info->bus_clk);
	clk_enable(info->lcd_clk);

	return 0;
}
#endif

static const struct dev_pm_ops lcdfreq_pm_ops = {
	SET_RUNTIME_PM_OPS(lcdfreq_runtime_suspend, lcdfreq_runtime_resume, NULL)
};


static struct platform_driver lcdfreq_driver = {
	.probe		= lcdfreq_probe,
	.driver		= {
		.name	= "lcdfreq",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM_RUNTIME
		.pm	= &lcdfreq_pm_ops,
#endif
	},
};

static int __init lcdfreq_init(void)
{
	return platform_driver_register(&lcdfreq_driver);
}
late_initcall(lcdfreq_init);

static void __exit lcdfreq_exit(void)
{
	platform_driver_unregister(&lcdfreq_driver);
}
module_exit(lcdfreq_exit);

