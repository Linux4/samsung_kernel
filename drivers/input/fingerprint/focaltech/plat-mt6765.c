/**
 * plat-mt6735.c
 *
**/

#include <linux/stddef.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#if !defined(CONFIG_MTK_CLKMGR)
# include <linux/clk.h>
#else
# include <mach/mt_clkmgr.h>
#endif

#include "ff_log.h"

/* TODO: */
//#define FF_COMPATIBLE_NODE_1 "fingerprint,focaltech"
#define FF_COMPATIBLE_NODE_1 "mediatek,finger-focaltech"

#define SUPPLY_3V3               3300000

extern struct spi_device *ff_spi;
//extern struct pinctrl *ff_pinctrl;

//#include <mach/gpio_const.h>
//#include "mtk_spi_hal.h"
extern void mt_spi_enable_master_clk(struct spi_device *ms);
extern void mt_spi_disable_master_clk(struct spi_device *ms);



/* Define pinctrl state types. */
typedef enum {
    FF_PINCTRL_DEFAULT,
    FF_PINCTRL_STATE_RST_ACT,
    FF_PINCTRL_STATE_RST_CLR,
    FF_PINCTRL_STATE_INT_ACT,
    FF_PINCTRL_STATE_PINS_DEFAULT,  
    FF_PINCTRL_STATE_MAXIMUM /* Array size */
} ff_pinctrl_state_t;

/* Define pinctrl state names. */
static const char *g_pinctrl_state_names[FF_PINCTRL_STATE_MAXIMUM] = {
	"default", "state_rst_output0", "state_rst_output1", "state_eint", "state_pins_default",
};

/* Native context and its singleton instance. */
typedef struct {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_states[FF_PINCTRL_STATE_MAXIMUM];
#if !defined(CONFIG_MTK_CLKMGR)
	struct clk *spiclk;
#endif
	struct regulator *ff_vdd;
	bool b_spiclk_enabled;
} ff_mt6735_context_t;
static ff_mt6735_context_t ff_mt6735_context = {.b_spiclk_enabled = false}, *g_context = &ff_mt6735_context;
int ff_ctl_init_pins(int *irq_num)
{
	int err = 0, i;
	struct device_node *dev_node = NULL;
	struct platform_device *pdev = NULL;
	FF_LOGV("'%s' enter.", __func__);

	/* Find device tree node. */
	dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE_1);
	if (!dev_node) {
		FF_LOGE("of_find_compatible_node(.., '%s') failed.", FF_COMPATIBLE_NODE_1);
		return (-ENODEV);
	}

	/* Convert to platform device */
	pdev = of_find_device_by_node(dev_node);
	if (!pdev) {
		FF_LOGE("of_find_device_by_node(..) failed.");
		return (-ENODEV);
	}
	//g_context->pinctrl = ff_pinctrl;
	/* Retrieve the pinctrl handler. */
	g_context->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!g_context->pinctrl) {
		FF_LOGE("devm_pinctrl_get(..) failed.");
		return (-ENODEV);
	}

	/* Get regulator */
	g_context->ff_vdd = regulator_get(&ff_spi->dev, "fp_vdd");
	if (IS_ERR(g_context->ff_vdd)) {
		FF_LOGE("Can't find vcc_core regulator.");
		g_context->ff_vdd = NULL;
	} else {
		FF_LOGI("Found vcc_core regulator.");
		err = regulator_set_voltage(g_context->ff_vdd, SUPPLY_3V3, SUPPLY_3V3);
	    if (err < 0){
		    FF_LOGE("Can't set voltage.");
		    return err;
	    }
	}

#if 0

	/* Convert to platform device. */
	pdev = of_find_device_by_node(dev_node);
	if (!pdev) {
		FF_LOGE("of_find_device_by_node(..) failed.");
		return (-ENODEV);
	}

	/* Retrieve the pinctrl handler. */
	g_context->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!g_context->pinctrl) {
		FF_LOGE("devm_pinctrl_get(..) failed.");
		return (-ENODEV);
	}
#endif

	/* Register all pins. */
	for (i = 0; i < FF_PINCTRL_STATE_MAXIMUM; ++i) {
		g_context->pin_states[i] = pinctrl_lookup_state(g_context->pinctrl, g_pinctrl_state_names[i]);
		if (!g_context->pin_states[i]) {
			FF_LOGE("can't find pinctrl state for '%s'.", g_pinctrl_state_names[i]);
			err = (-ENODEV);
			break;
		}
	}

	if (i < FF_PINCTRL_STATE_MAXIMUM) {
		return (-ENODEV);
	}
	err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_RST_ACT]);

	/* Initialize the INT pin. */
	err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_INT_ACT]);

	err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_PINS_DEFAULT]);

	/* Retrieve the irq number. */
	/*dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE_1);
	if (!dev_node) {
		FF_LOGE("of_find_compatible_node(.., '%s') failed.", FF_COMPATIBLE_NODE_1);
		return (-ENODEV);
	}*/
	*irq_num = irq_of_parse_and_map(dev_node, 0);
	FF_LOGD("irq number is %d.", *irq_num);

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_free_pins(void)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);

	if (g_context->ff_vdd != NULL)
	{
		regulator_put(g_context->ff_vdd);
		g_context->ff_vdd = NULL;
	}

	if (g_context->pinctrl != NULL )
	{
		devm_pinctrl_put(g_context->pinctrl);
		g_context->pinctrl = NULL;
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_enable_spiclk(bool on)
{
	int err = 0;

	FF_LOGV("'%s' enter.", __func__);
	FF_LOGD("clock: '%s'.", on ? "enable" : "disabled");

	/* Control the clock source. */
	if (on == g_context->b_spiclk_enabled) {
		FF_LOGE("'%s' skip duplicated spiclk %d", __func__, on);
		return err;
	}

	if (on) {
		mt_spi_enable_master_clk(ff_spi);
		g_context->b_spiclk_enabled = true;
	} else {
		mt_spi_disable_master_clk(ff_spi);
		g_context->b_spiclk_enabled = false;
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_enable_power(bool on)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);
#if 0
	if(on)
	{
	   err = regulator_set_voltage(g_context->ff_vdd, 2800000, 2800000);
		if (err < 0)
		{
		   FF_LOGE("Can't set voltage.");
		   return err;
		}
		err = regulator_enable(g_context->ff_vdd);
	}else{
		err = regulator_set_voltage(g_context->ff_vdd, 0, 0);
		if (err < 0)
		{
			FF_LOGE("Can't set voltage.");
			return err;
		}
		err = regulator_enable(g_context->ff_vdd);
	}
#endif
	if (on)
	{
		err = regulator_enable(g_context->ff_vdd);
		if (err < 0) {
			FF_LOGE("Can't enable voltage\n");
		}
	}else{
		/* TODO */
	}

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_reset_device(void)
{
	int err = 0;
	FF_LOGV("'%s' enter.", __func__);

	if (unlikely(!g_context->pinctrl)) {
		return (-ENOSYS);
	}

	/* 3-1: Pull down RST pin. */
	err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_RST_CLR]);

	/* 3-2: Delay for 10ms. */
	usleep_range(10000, 10000);

	/* Pull up RST pin. */
	err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_RST_ACT]);

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

const char *ff_ctl_arch_str(void)
{
	return CONFIG_MTK_PLATFORM;
}

