/* 	to use pr_debug: add #define DEBUG 	*/
#if defined(CONFIG_PWM_GPCLK_DEBUG_FEATURE)
#define DEBUG
#endif

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/pwm.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pwm-gpclk.h>
#include <linux/pm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_qos.h>
#include <linux/pm_wakeup.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

static struct pwm_gc *g_pwm_gc;
static struct pm_qos_request pm_qos_req;
static struct wakeup_source *ws;

static int32_t g_nlra_gp_clk_m = GP_CLK_M_DEFAULT;
static int32_t g_nlra_gp_clk_n = GP_CLK_N_DEFAULT;
void __iomem *virt_mmss_gp1_base;

static void hwio_settling_delay(void) 
{
	if(g_pwm_gc->allow_delay)
		usleep_range(g_pwm_gc->hwio_reg_update_delay, g_pwm_gc->hwio_reg_update_delay);
}

#if defined(CONFIG_PWM_GPCLK_DEBUG_FEATURE)
static ssize_t allow_delay_show(struct device *dev, \
		struct device_attribute *attr, char *buf)
{
	pr_info("[VIB]start! %s %d, val:%d\n", __func__, __LINE__,
			g_pwm_gc->allow_delay);
	
	return snprintf(buf, PAGE_SIZE, "%d\n", g_pwm_gc->allow_delay);
}

static ssize_t allow_delay_store(struct device *dev, \
		struct device_attribute *attr, const char *buf, size_t len)
{
	int val = 20;	
	sscanf(buf, "%d", &val);	
	pr_info("[VIB] %s %d: val.new: %d, val.old: %d\n", __func__, __LINE__, 
			val, g_pwm_gc->allow_delay);
	
	g_pwm_gc->allow_delay = val;	
	return len;
}
static DEVICE_ATTR_RW(allow_delay);


static ssize_t udelay_val_show(struct device *dev, \
		struct device_attribute *attr, char *buf)
{
	pr_info("[VIB]start! %s %d, val:%d\n", __func__, __LINE__,
			g_pwm_gc->hwio_reg_update_delay);
	
	return snprintf(buf, PAGE_SIZE, "%d\n", g_pwm_gc->hwio_reg_update_delay);
}

static ssize_t udelay_val_store(struct device *dev, \
		struct device_attribute *attr, const char *buf, size_t len)
{
	int val = 20;	
	sscanf(buf, "%d", &val);	
	pr_info("[VIB] %s %d: val.new: %d, val.old: %d\n", __func__, __LINE__, 
		val, g_pwm_gc->hwio_reg_update_delay);
	
	g_pwm_gc->hwio_reg_update_delay = val;
	return len;
}
static DEVICE_ATTR_RW(udelay_val);

static struct attribute *gpclk_attributes[] = {
	&dev_attr_allow_delay.attr,
	&dev_attr_udelay_val.attr,
	NULL,
};
 
static struct attribute_group gpclk_attr_group = {
	.attrs = gpclk_attributes,
};
#endif //#if defined(CONFIG_PWM_GPCLK_DEBUG_FEATURE)

static int pwm_gpclk_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	u32 data;

	mutex_lock(&g_pwm_gc->lock);

	HWIO_OUTM(GPx_CMD_RCGR, HWIO_UPDATE_VAL_BMSK,
				1 << HWIO_UPDATE_VAL_SHFT); /* UPDATE ACTIVE */
	hwio_settling_delay();
	HWIO_OUTM(GPx_CMD_RCGR, HWIO_ROOT_EN_VAL_BMSK,
				1 << HWIO_ROOT_EN_VAL_SHFT);/* ROOT_EN */
	hwio_settling_delay();
	HWIO_OUTM(CAMSS_GPx_CBCR, HWIO_CLK_ENABLE_VAL_BMSK,
				1 << HWIO_CLK_ENABLE_VAL_SHFT); /* CLK_ENABLE */
	hwio_settling_delay();
	__pm_stay_awake(ws);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	pm_qos_update_request(&pm_qos_req, PM_QOS_NONIDLE_VALUE);
#else
	cpu_latency_qos_update_request(&pm_qos_req, PM_QOS_NONIDLE_VALUE);
#endif
	data = HWIO_GPx_CMD_RCGR_IN;
	pr_debug("[VIB] %s HWIO_GPx_CMD_RCGR_IN: 0x%x\n", __func__, data);

	data = HWIO_CAMSS_GPx_CBCR_IN;
	pr_debug("[VIB] %s HWIO_CAMSS_GP0_CBCR_IN: 0x%x\n", __func__, data);

	mutex_unlock(&g_pwm_gc->lock);

	pr_debug("[VIB] %s done\n", __func__);

	return 0;
}

static void pwm_gpclk_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	pr_debug("[VIB] %s called\n", __func__);

	mutex_lock(&g_pwm_gc->lock);
	HWIO_OUTM(GPx_CMD_RCGR, HWIO_UPDATE_VAL_BMSK,
				0 << HWIO_UPDATE_VAL_SHFT);
	hwio_settling_delay();
	HWIO_OUTM(GPx_CMD_RCGR, HWIO_ROOT_EN_VAL_BMSK,
				0 << HWIO_ROOT_EN_VAL_SHFT);
	hwio_settling_delay();
	HWIO_OUTM(CAMSS_GPx_CBCR, HWIO_CLK_ENABLE_VAL_BMSK,
				0 << HWIO_CLK_ENABLE_VAL_SHFT);
	hwio_settling_delay();

	__pm_relax(ws);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	pm_qos_update_request(&pm_qos_req, PM_QOS_DEFAULT_VALUE);
#else
	cpu_latency_qos_update_request(&pm_qos_req, PM_QOS_DEFAULT_VALUE);
#endif
	mutex_unlock(&g_pwm_gc->lock);

	pr_debug("[VIB] %s done\n", __func__);
	return;
}

static int pwm_gpclk_config(struct pwm_chip *chip, struct pwm_device *pwm,
			      int duty_ns, int period_ns)
{
	/* multiply 10 to ip clock value to avoid error
	 * because of rounding off values below decimal point
	 */
	unsigned int ip_clock = BASE_IP_CLK * 10;
	unsigned int freq = (unsigned int)(NSEC_PER_SEC / period_ns);
	unsigned int base_n = 0, n_m2 = 0, n_m3 = 0;
	unsigned int m2_freq = 0, m3_freq = 0;
	int32_t base_d = 0;
	int32_t calc_d;
	int32_t calc_n;
	u32 data;

	pr_debug("[VIB] %s: duty_ns(%d) period_ns(%d)\n", __func__, duty_ns, period_ns);

	if (duty_ns > period_ns) {
		pr_err("[VIB] %s: duty_ns(%d) > period_ns(%d), return\n", __func__, duty_ns, period_ns);
		return -EINVAL;
	}

	mutex_lock(&g_pwm_gc->lock);
	base_n = ip_clock / freq;
	n_m2 = base_n * 2;
	n_m3 = base_n * 3;
	m2_freq = (ip_clock / n_m2) * 2;
	m3_freq = (ip_clock / n_m3) * 3;
	if (abs(freq - m2_freq) <= abs(freq - m3_freq)) {
		g_nlra_gp_clk_m = 2;
		g_nlra_gp_clk_n = (n_m2 + 5) / 10;
	} else {
		g_nlra_gp_clk_m = 3;
		g_nlra_gp_clk_n = (n_m3 + 5) / 10;

		if (g_nlra_gp_clk_n > 150) {
			pr_debug("[VIB] %s: N is larger than 150, N: (%d)\n", __func__, g_nlra_gp_clk_n);
			g_nlra_gp_clk_m = 2;
			g_nlra_gp_clk_n = (n_m2 + 5) / 10;
		}
	}

	pr_debug("[VIB] %s: pwm_freq(%d)\n", __func__, freq);
	pr_debug("[VIB] %s: base_n(%d)\n", __func__, base_n);
	pr_debug("[VIB] %s: n_m2(%d)\n", __func__, n_m2);
	pr_debug("[VIB] %s: n_m3(%d)\n", __func__, n_m3);
	pr_debug("[VIB] %s: m2_freq(%d)\n", __func__, m2_freq);
	pr_debug("[VIB] %s: m3_freq(%d)\n", __func__, m3_freq);
	pr_debug("[VIB] %s: M(%d) N(%d)\n", __func__, g_nlra_gp_clk_m, g_nlra_gp_clk_n);

	/* Put the MND counter in reset mode for programming */
	HWIO_OUTM(GPx_CFG_RCGR, HWIO_GP_SRC_SEL_VAL_BMSK,
				0 << HWIO_GP_SRC_SEL_VAL_SHFT); /* SRC_SEL = 000(cxo) */
	hwio_settling_delay();
	HWIO_OUTM(GPx_CFG_RCGR, HWIO_GP_SRC_DIV_VAL_BMSK,
				31 << HWIO_GP_SRC_DIV_VAL_SHFT); /* SRC_DIV = 11111 (Div 16) */
	hwio_settling_delay();
	HWIO_OUTM(GPx_CFG_RCGR, HWIO_GP_MODE_VAL_BMSK,
				2 << HWIO_GP_MODE_VAL_SHFT); /* Mode Select 10 */
	hwio_settling_delay();

	/* M value */
	HWIO_OUTM(GPx_M_REG, HWIO_GP_MD_REG_M_VAL_BMSK,
		g_nlra_gp_clk_m << HWIO_GP_MD_REG_M_VAL_SHFT);
	hwio_settling_delay();

	calc_n = (~(g_nlra_gp_clk_n - g_nlra_gp_clk_m) & HWIO_GP_N_REG_N_VAL_BMSK);

	base_d = g_nlra_gp_clk_n * duty_ns / period_ns;

	pr_debug("[VIB] %s: D(%d)\n", __func__, base_d);

	if (base_d > 127) {
		pr_info("[VIB] %s: D is larger than 127 which causes overflow, so set D as 127\n", __func__);
		base_d = 127;
	}
	calc_d = (~(base_d << 1) & HWIO_GP_MD_REG_D_VAL_BMSK);

	if (calc_d == HWIO_GP_MD_REG_D_VAL_BMSK)
		calc_d = HWIO_GP_MD_REG_D_VAL_BMSK - 1;

	/* D value */
	HWIO_OUTM(GPx_D_REG, HWIO_GP_MD_REG_D_VAL_BMSK, calc_d);
	hwio_settling_delay();
	/* N value */
	HWIO_OUTM(GPx_N_REG, HWIO_GP_N_REG_N_VAL_BMSK, calc_n);
	hwio_settling_delay();

	data = HWIO_GPx_CFG_RCGR_IN;
	pr_debug("%s HWIO_GPx_CFG_RCGR_IN: 0x%x\n", __func__, data);

	data = HWIO_GPx_D_REG_IN;
	pr_debug("%s HWIO_GPx_D_REG_IN: 0x%x\n", __func__, data);

	data = HWIO_GPx_N_REG_IN;
	pr_debug("%s HWIO_GPx_N_REG_IN: 0x%x\n", __func__, data);

	pr_info("%s done\n", __func__);

	/* Update new M/N:D value */
	HWIO_OUTM(GPx_CMD_RCGR, HWIO_UPDATE_VAL_BMSK,
				1 << HWIO_UPDATE_VAL_SHFT); /* UPDATE ACTIVE */
	hwio_settling_delay();
	
	mutex_unlock(&g_pwm_gc->lock);

	return 0;
}

static int pwm_gpclk_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	pr_info("%s is called! \n", __func__);
	return 0;
}

static void pwm_gpclk_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
			  struct pwm_state *state)
{
	pr_info("%s is called! \n", __func__);
	state->enabled = g_pwm_gc->def_enabled;
}

static const struct pwm_ops pwm_gpclk_ops = {
	.config = pwm_gpclk_config,
	.enable = pwm_gpclk_enable,
	.disable = pwm_gpclk_disable,
	.request = pwm_gpclk_request,
	.get_state = pwm_gpclk_get_state,
	.owner = THIS_MODULE,
};


static int pwm_gc_parse_dt(struct pwm_gc *gc)
{
	struct device_node *np = gc->dev->of_node;
	int rc;
	int val = 0;

	rc = of_property_read_u32(np, "samsung,gp_clk", &gc->gp_clk);
	if (rc) {
		pr_info("gp_clk not specified so using default address\n");
		gc->gp_clk = MSM_GCC_GPx_BASE;
		rc = 0;
	}

	gc->def_enabled = of_property_read_bool(np, "samsung,enabled_default");

	gc->allow_delay = of_property_read_bool(np, "samsung,use_wr_delay");
	if (!of_property_read_u32(np, "samsung,wr_delay", &val))
		gc->hwio_reg_update_delay = val;

	pr_info("def_enabled: (%d), allow_delay: %d, delay_val: %d\n", 
		gc->def_enabled, gc->allow_delay, gc->hwio_reg_update_delay);

	return rc;
}


static int pwm_gpclk_probe(struct platform_device *pdev)
{
	struct pwm_chip *chip;
	struct pwm_gc *gc;
	int ret;

	pr_info("pwm_gpclk: probe\n");
	gc = devm_kzalloc(&pdev->dev, sizeof(*gc), GFP_KERNEL);
	if (!gc)
		return -ENOMEM;

	if (!pdev->dev.of_node) {
		pr_err("pwm_gpclk: %s failed, DT is NULL", __func__);
		return -ENODEV;
	}

	gc->dev = &pdev->dev;
	g_pwm_gc = gc;
	ret = pwm_gc_parse_dt(g_pwm_gc);
	if (ret)
		return ret;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	chip->ops = &pwm_gpclk_ops;
	chip->npwm = 1;
	chip->base = -1;
	ret = pwmchip_add(chip);
	if (ret < 0) {
		dev_err(chip->dev, "pwm_gpclk: Add pwmchip failed, ret=%d\n", ret);
	}

	virt_mmss_gp1_base = ioremap(g_pwm_gc->gp_clk, 0x28);
	if (!virt_mmss_gp1_base)
		panic("pwm_gpclk: Unable to ioremap MSM_MMSS_GP1 memory!");

	g_pwm_gc->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(g_pwm_gc->pinctrl)) {
		pr_err("pwm_gpclk: Failed to get pinctrl(%d)\n", IS_ERR(g_pwm_gc->pinctrl));
	} else {
		g_pwm_gc->pin_active = pinctrl_lookup_state(g_pwm_gc->pinctrl, "tlmm_motor_active");
		if (IS_ERR(g_pwm_gc->pin_active))
			pr_err("pwm_gpclk: Failed to get pin_active(%d)\n", IS_ERR(g_pwm_gc->pin_active));
		g_pwm_gc->pin_suspend = pinctrl_lookup_state(g_pwm_gc->pinctrl, "tlmm_motor_suspend");
		if (IS_ERR(g_pwm_gc->pin_suspend)) {
			pr_err("pwm_gpclk: Failed to get pin_suspend(%d)\n", IS_ERR(g_pwm_gc->pin_suspend));
		} else {
			ret = pinctrl_select_state(g_pwm_gc->pinctrl, g_pwm_gc->pin_active);
			if (ret)
				pr_err("pwm_gpclk: can not change pin_suspend\n");
		}
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 110)
	ws = wakeup_source_register(&pdev->dev, "pwm_present");
#else
	ws = wakeup_source_register("pwm_present");
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	pm_qos_add_request(&pm_qos_req, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
#else
	cpu_latency_qos_add_request(&pm_qos_req, PM_QOS_DEFAULT_VALUE);
#endif
	dev_set_drvdata(&pdev->dev, g_pwm_gc);

	mutex_init(&gc->lock);

#if defined(CONFIG_PWM_GPCLK_DEBUG_FEATURE)
	/* create /sys/class/gpclk/ */
	gc->gpclk_class = class_create(THIS_MODULE, "gpclk");
	
	/* create /sys/class/gpclk/hwio_delay/ */
	gc->gpclk_dev = device_create(gc->gpclk_class, NULL, MKDEV(0, 0), g_pwm_gc, "hwio_delay");
	
	/*
		/sys/class/gpclk/hwio_delay/allow_delay 
		/sys/class/gpclk/hwio_delay/udelay_val 
	*/
	sysfs_create_group(&gc->gpclk_dev->kobj, &gpclk_attr_group);
#endif //#if defined(CONFIG_PWM_GPCLK_DEBUG_FEATURE)

	pr_info("pwm_gpclk : probe done\n");
	return 0;
}

static int pwm_gpclk_remove(struct platform_device *pdev)
{
	struct pwm_chip *chip = dev_get_drvdata(&pdev->dev);
	int ret = 0;

	iounmap(virt_mmss_gp1_base);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
	ret = pwmchip_remove(chip);
	if (ret < 0)
		dev_err(chip->dev, "Remove pwmchip failed, ret=%d\n", ret);
#else
	pwmchip_remove(chip);
#endif

	wakeup_source_unregister(ws);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	pm_qos_remove_request(&pm_qos_req);
#else
	cpu_latency_qos_remove_request(&pm_qos_req);
#endif
	dev_set_drvdata(chip->dev, NULL);

	return ret;
}

#if defined(CONFIG_PM)
static int pwm_gpclk_suspend(struct device *dev)
{
	struct pwm_gc *gc = dev_get_drvdata(dev);
	int ret = 0;

	pr_info("pwm_gpclk: %s\n", __func__);

	if (IS_ERR(gc->pinctrl)) {
		pr_debug("pwm_gpclk: pinctrl error(%d)\n", IS_ERR(gc->pinctrl));
	} else if (IS_ERR(gc->pin_suspend)) {
		pr_debug("pwm_gpclk: pin_suspend error(%d)\n", IS_ERR(gc->pin_suspend));
	} else {
		ret = pinctrl_select_state(gc->pinctrl, gc->pin_suspend);
		if (ret)
			pr_err("pwm_gpclk: can not change pin_suspend\n");
	}

	return 0;
}

static int pwm_gpclk_resume(struct device *dev)
{
	struct pwm_gc *gc = dev_get_drvdata(dev);
	int ret = 0;

	pr_info("pwm_gpclk: %s\n", __func__);

	if (IS_ERR(gc->pinctrl)) {
		pr_debug("pwm_gpclk: pinctrl error(%d)\n", IS_ERR(gc->pinctrl));
	} else if (IS_ERR(gc->pin_active)) {
		pr_debug("pwm_gpclk: pin_active error(%d)\n", IS_ERR(gc->pin_active));
	} else {
		ret = pinctrl_select_state(gc->pinctrl, gc->pin_active);
		if (ret)
			pr_err("pwm_gpclk: can not change pin_active\n");
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(pwm_gpclk_pm_ops, pwm_gpclk_suspend, pwm_gpclk_resume);

static const struct of_device_id pwm_gpclk_match[] = {
	{	.compatible = "samsung_pwm_gpclk",
	},
	{}
};

static struct platform_driver pwm_gpclk_pdrv = {
	.driver = {
		.name = "samsung_pwm_gpclk",
		.owner = THIS_MODULE,
		.of_match_table = pwm_gpclk_match,
		.pm	= &pwm_gpclk_pm_ops,
	},
	.probe = pwm_gpclk_probe,
	.remove = pwm_gpclk_remove,
};

static int __init pwm_gpclk_pdrv_init(void)
{
	return platform_driver_register(&pwm_gpclk_pdrv);
}
subsys_initcall_sync(pwm_gpclk_pdrv_init);

MODULE_AUTHOR("Samsung Corporation");
MODULE_DESCRIPTION("Pwm generating by gp clk control");
MODULE_LICENSE("GPL v2");
