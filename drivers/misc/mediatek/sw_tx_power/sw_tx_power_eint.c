#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/printk.h>

/* #include <mach/eint.h> */

/* #include <cust_eint.h> */
/* #include <cust_gpio_usage.h> */
/* #include <mach/mt_gpio.h> */

#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "sw_tx_power.h"

#define EINT_PIN_PLUG_IN        (1)
#define EINT_PIN_PLUG_OUT       (0)

#ifndef ENABLE_EINT_SWTP
#define ENABLE_EINT_SWTP
#endif

#if defined(ENABLE_EINT_SWTP)
static int swtp_eint_state;
#ifdef SWTP_2_RF_CON
static int swtp_eint_state2;
#endif

#define SWTP_OPPOSE_EINT_TYPE	\
	((swtx_eint_type == IRQF_TRIGGER_HIGH)?(IRQF_TRIGGER_LOW):(IRQF_TRIGGER_HIGH))

#define SWTP_TRIGGERING ((swtx_eint_type == IRQF_TRIGGER_HIGH)?1:0)


/* static struct switch_dev swtx_data; */

/*
struct swtx_dts_data {
	struct headset_mode_settings swtx_debounce;
	int swtx_plugout_debounce;
	int swtx_mode;
};
*/

int isirqenable = 0;
int swtx_irq;
unsigned int swtx_gpiopin, swtxdebounce;
unsigned int swtx_eint_type;
#ifdef SWTP_2_RF_CON
int isirq2enable = 0;
int swtx_2_irq;
unsigned int swtx_2_gpiopin, swtx2debounce;
unsigned int swtx_2_eint_type;
#endif

struct pinctrl *swtx_pinctrl1;
struct pinctrl_state *swtx_pins_default, *swtx_pins_eint_int;
#ifdef SWTP_2_RF_CON
struct pinctrl *swtx_pinctrl2;
struct pinctrl_state *swtx_pins_eint_2_int;
#endif
/* struct swtx_dts_data swtx_dts_data; */

/* static void swtp_eint_handler(void) */

#define SWTP_USE_WQ

#ifdef SWTP_USE_WQ
static struct work_struct swtp_eint_work;
static struct workqueue_struct *swtp_eint_workqueue;
#ifdef SWTP_2_RF_CON
static struct work_struct swtp_eint_2_work;
static struct workqueue_struct *swtp_eint_2_workqueue;
#endif
#endif

void swtp_enable_irq(int irq)
{
	if (irq == swtx_irq) {
		if (isirqenable == 0) {
			enable_irq(irq);
			isirqenable++;
			SWTX_DBG("[SWTP] swtp eint[%d] enable[%d]!\n", irq, isirqenable);
		}
	}
#ifdef SWTP_2_RF_CON
	if (irq == swtx_2_irq) {
		if (isirq2enable == 0) {
			enable_irq(irq);
			isirq2enable++;
			SWTX_DBG("[SWTP] swtp 2nd eint[%d] enable[%d]!\n", irq, isirq2enable);
		}
	}
#endif
}

void swtp_disable_irq(int irq)
{
	if (irq == swtx_irq) {
		if (isirqenable > 0) {
			disable_irq_nosync(irq);
			isirqenable--;
			SWTX_DBG("[SWTP] swtp eint[%d] disable[%d]!\n", irq, isirqenable);
		}
	}
#ifdef SWTP_2_RF_CON
	if (irq == swtx_2_irq) {
		if (isirq2enable > 0) {
			disable_irq_nosync(irq);
			isirq2enable--;
			SWTX_DBG("[SWTP] swtp 2nd eint[%d] disable[%d]!\n", irq, isirq2enable);
		}
	}
#endif
}

#ifdef SWTP_USE_WQ
static void swtp_eint_work_callback(struct work_struct *work)
{
	unsigned int rfcable_enable;

	if (swtp_eint_state == EINT_PIN_PLUG_IN)
		rfcable_enable = SWTP_MODE_ON;
	else
		rfcable_enable = SWTP_MODE_OFF;

	swtp_set_mode_unlocked(SWTP_CTRL_SUPER_SET, rfcable_enable);
	swtp_enable_irq(swtx_irq);
}

#ifdef SWTP_2_RF_CON
static void swtp_eint_2_work_callback(struct work_struct *work)
{
	unsigned int rfcable_enable;

	if (swtp_eint_state2 == EINT_PIN_PLUG_IN)
		rfcable_enable = SWTP_MODE_ON;
	else
		rfcable_enable = SWTP_MODE_OFF;

	swtp_set_mode_unlocked(SWTP_CTRL_SUPER_SET, rfcable_enable);
	swtp_enable_irq(swtx_2_irq);
}
#endif
#endif

static irqreturn_t swtp_eint_handler(int irq, void *data)
{
#ifdef SWTP_USE_WQ
	int ret;
#else
	unsigned int rfcable_enable;
#endif

	SWTX_DBG("[SWTP] swtp_eint_func\n");

	swtp_disable_irq(swtx_irq);

	if (swtp_eint_state == EINT_PIN_PLUG_IN) {
		if (swtx_eint_type == IRQF_TRIGGER_HIGH)
			irq_set_irq_type(swtx_irq, IRQF_TRIGGER_HIGH);
		else
			irq_set_irq_type(swtx_irq, IRQF_TRIGGER_LOW);

		swtp_eint_state = EINT_PIN_PLUG_OUT;
	} else {
		if (swtx_eint_type == IRQF_TRIGGER_HIGH)
			irq_set_irq_type(swtx_irq, IRQF_TRIGGER_LOW);
		else
			irq_set_irq_type(swtx_irq, IRQF_TRIGGER_HIGH);

		swtp_eint_state = EINT_PIN_PLUG_IN;
	}

	SWTX_INF("[SWTP] rfcable_enable: %d\n", swtp_eint_state);

#ifdef SWTP_USE_WQ
	ret = queue_work(swtp_eint_workqueue, &swtp_eint_work);
	if (!ret)
		SWTX_ERR("[SWTP]swtp_eint_work return:%d!\n", ret);

#else
	rfcable_enable = (swtp_eint_state == EINT_PIN_PLUG_IN) ? SWTP_MODE_ON : SWTP_MODE_OFF;

	swtp_set_mode_unlocked(SWTP_CTRL_SUPER_SET, rfcable_enable);
	swtp_enable_irq(swtx_irq);
#endif

	return IRQ_HANDLED;
}

#ifdef SWTP_2_RF_CON
static irqreturn_t swtp_eint_2_handler(int irq, void *data)
{
#ifdef SWTP_USE_WQ
	int ret;
#else
	unsigned int rfcable_enable;
#endif

	SWTX_DBG("[SWTP] swtp_eint_2_handler\n");

	swtp_disable_irq(swtx_2_irq);

	if (swtp_eint_state2 == EINT_PIN_PLUG_IN) {
		if (swtx_2_eint_type == IRQF_TRIGGER_HIGH)
			irq_set_irq_type(swtx_2_irq, IRQF_TRIGGER_HIGH);
		else
			irq_set_irq_type(swtx_2_irq, IRQF_TRIGGER_LOW);

		swtp_eint_state2 = EINT_PIN_PLUG_OUT;
	} else {
		if (swtx_2_eint_type == IRQF_TRIGGER_HIGH)
			irq_set_irq_type(swtx_2_irq, IRQF_TRIGGER_LOW);
		else
			irq_set_irq_type(swtx_2_irq, IRQF_TRIGGER_HIGH);

		swtp_eint_state2 = EINT_PIN_PLUG_IN;
	}

	SWTX_INF("[SWTP]: rfcable_2_enable: %d\n", swtp_eint_state2);

#ifdef SWTP_USE_WQ
	ret = queue_work(swtp_eint_2_workqueue, &swtp_eint_2_work);
	if (!ret)
		SWTX_ERR("[SWTP]swtp_eint_2_work return:%d!\n", ret);
#else
	if (swtp_eint_state2 == EINT_PIN_PLUG_IN)
		rfcable_enable = SWTP_MODE_ON;
	else
		rfcable_enable = SWTP_MODE_OFF;

	swtp_set_mode_unlocked(SWTP_CTRL_SUPER_SET, rfcable_enable);
	swtp_enable_irq(swtx_2_irq);
#endif

	return IRQ_HANDLED;
}
#endif

int swtp_mod_eint_read(void)
{
	int status;

	SWTX_DBG("[SWTP] gpio value[%d]\n", gpio_get_value(swtx_gpiopin));

#ifdef SWTP_2_RF_CON
	if ((gpio_get_value(swtx_gpiopin) == SWTP_TRIGGERING) ||
	    (gpio_get_value(swtx_2_gpiopin) == SWTP_TRIGGERING)) {
#else
	if (gpio_get_value(swtx_gpiopin) == SWTP_TRIGGERING) {
#endif
		status = swtp_rfcable_tx_power();
		SWTX_INF("[SWTP] cable in! [%d]\n", status);
	} else {
		status = swtp_reset_tx_power();
		SWTX_INF("[SWTP] cable out! [%d]\n", status);
	}

	return status;
}

int swtp_mod_eint_enable(int enable)
{
	if (enable) {
		swtp_enable_irq(swtx_irq);
#ifdef SWTP_2_RF_CON
		swtp_enable_irq(swtx_2_irq);
#endif
	} else {
		swtp_disable_irq(swtx_irq);
#ifdef SWTP_2_RF_CON
		swtp_disable_irq(swtx_2_irq);
#endif
	}

	return 0;
}

static int swtx_get_dts_data(struct platform_device *swtx_dev)
{
	int ret;
	u32 ints[2] = { 0, 0 };
	u32 ints1[2] = { 0, 0 };
#ifdef SWTP_2_RF_CON
	u32 ints_2[2] = { 0, 0 };
	u32 ints1_2[2] = { 0, 0 };
#endif
	struct device_node *node = NULL;
/* struct pinctrl_state *pins_default; */

	SWTX_DBG("[SWTP] Start swtx_get_dts_data\n");

	/*configure to GPIO function, external interrupt */
	swtx_pinctrl1 = devm_pinctrl_get(&swtx_dev->dev);
	if (IS_ERR(swtx_pinctrl1)) {
		ret = PTR_ERR(swtx_pinctrl1);
		SWTX_ERR("[SWTP] fwq Cannot find swtx_pinctrl1!\n");
		return ret;
	}

	swtx_pins_default = pinctrl_lookup_state(swtx_pinctrl1, "default");
	if (IS_ERR(swtx_pins_default)) {
		ret = PTR_ERR(swtx_pins_default);
		/*dev_err("[SWTP] fwq Cannot find swtx pinctrl default!\n"); */
	}

	swtx_pins_eint_int = pinctrl_lookup_state(swtx_pinctrl1, "state_eint_as_int");
	if (IS_ERR(swtx_pins_eint_int)) {
		ret = PTR_ERR(swtx_pins_eint_int);
		SWTX_ERR("[SWTP] fwq Cannot find pinctrl state_eint_as_int!\n");
		return ret;
	}

	pinctrl_select_state(swtx_pinctrl1, swtx_pins_eint_int);

#ifdef SWTP_2_RF_CON
	swtx_pinctrl2 = devm_pinctrl_get(&swtx_dev->dev);
	if (IS_ERR(swtx_pinctrl)) {
		ret = PTR_ERR(swtx_pinctrl2);
		SWTX_ERR("[SWTP] fwq Cannot find swtx_pinctrl1!\n");
		return ret;
	}

	swtx_pins_eint_2_int = pinctrl_lookup_state(swtx_pinctrl2, "state_eint_2_as_int");
	if (IS_ERR(swtx_pins_eint_2_int)) {
		ret = PTR_ERR(swtx_pins_eint_2_int);
		SWTX_ERR("[SWTP] fwq Cannot find pinctrl state_eint_2_as_int!\n");
		return ret;
	}

	pinctrl_select_state(swtx_pinctrl2, swtx_pins_eint_2_int);
#endif


	node = of_find_matching_node(node, swtp_of_match);
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		of_property_read_u32_array(node, "interrupts", ints1, ARRAY_SIZE(ints1));

		swtx_gpiopin = ints[0];
		swtxdebounce = ints[1];
		swtx_eint_type = ints1[1];

/* gpio_direction_input(swtx_gpiopin); */
/* gpio_set_debounce(swtx_gpiopin, swtxdebounce*100); */
/* gpio_to_irq(swtx_gpiopin); */
		gpio_set_debounce(swtx_gpiopin, swtxdebounce);
/* swtx_irq = mt_gpio_to_irq(swtx_gpiopin); */

		swtx_irq = irq_of_parse_and_map(node, 0);

		SWTX_DBG("[SWTP] 1st EINT pin[%d], debounce[%d], type[%x], irq[%d]\n", swtx_gpiopin,
			 swtxdebounce, swtx_eint_type, swtx_irq);

		ret =
		    request_irq(swtx_irq, swtp_eint_handler, IRQF_TRIGGER_NONE, "swtp-eint", NULL);
		if (ret != 0)
			SWTX_ERR("[SWTP] EINT IRQ LINE NOT AVAILABLE\n");
		else
			SWTX_ERR("[SWTP] swtx set EINT finished, swtx_irq=%d, swtxdebounce=%d\n",
				 swtx_irq, swtxdebounce);

		disable_irq(swtx_irq);
		SWTX_DBG("[SWTP] disable irq[%d]\n", swtx_irq);
	} else
		SWTX_ERR("[SWTP] %s can't find compatible node\n", __func__);

#ifdef SWTP_2_RF_CON
	node = of_find_matching_node(node, swtp2_of_match);
	if (node) {
		of_property_read_u32_array(node, "debounce", ints_2, ARRAY_SIZE(ints_2));
		of_property_read_u32_array(node, "interrupts", ints_2, ARRAY_SIZE(ints1_2));

		swtx_2_gpiopin = ints_2[0];
		swtx2debounce = ints_2[1];
		swtx_2_eint_type = ints1_2[1];
		gpio_set_debounce(swtx_2_gpiopin, swtx2debounce);
		swtx_2_irq = irq_of_parse_and_map(node, 0);

		SWTX_DBG("[SWTP] 2nd EINT pin[%d], debounce[%d], type[%x], irq[%d]\n",
			 swtx_2_gpiopin, swtx2debounce, swtx_2_eint_type, swtx_2_irq);

		ret =
		    request_irq(swtx_2_irq, swtp_eint_2_handler, IRQF_TRIGGER_NONE, "swtp-2-eint",
				NULL);
		if (ret != 0)
			SWTX_ERR("[swtx] EINT IRQ LINE NOT AVAILABLE\n");
		else
			SWTX_ERR("[swtx] swtx set EINT finished, swtx_2_irq=%d, swtx2debounce=%d\n",
				 swtx_2_irq, swtx2debounce);

		disable_irq(swtx_2_irq);
		SWTX_DBG("[SWTP] disable 2nd irq[%d]\n", swtx_2_irq);
	} else
		SWTX_ERR("[SWTP] %s can't find 2nd compatible node\n", __func__);
#endif
	return 0;
}

int swtp_mod_eint_init(struct platform_device *dev)
{
	int init_value = 0;
	unsigned int init_flag = IRQF_TRIGGER_NONE;
#ifdef SWTP_2_RF_CON
	int init_value2 = 0;
	unsigned int init_flag2 = IRQF_TRIGGER_NONE;
#endif

#ifdef SWTP_USE_WQ
	swtp_eint_workqueue = create_singlethread_workqueue("swtp_eint");
	INIT_WORK(&swtp_eint_work, swtp_eint_work_callback);
#ifdef SWTP_2_RF_CON
	swtp_eint_2_workqueue = create_singlethread_workqueue("swtp_2_eint");
	INIT_WORK(&swtp_eint_2_work, swtp_eint_2_work_callback);
#endif
#endif

	swtx_get_dts_data(dev);

	init_value = gpio_get_value(swtx_gpiopin);
	if (init_value == SWTP_TRIGGERING) {
		init_flag = SWTP_OPPOSE_EINT_TYPE;
		swtp_eint_state = EINT_PIN_PLUG_IN;
	} else {
		init_flag = swtx_eint_type;
		swtp_eint_state = EINT_PIN_PLUG_OUT;
	}
#ifdef SWTP_2_RF_CON
	init_value2 = gpio_get_value(swtx_2_gpiopin);
	if (init_value2 == SWTP_TRIGGERING) {
		init_flag2 = SWTP_OPPOSE_EINT_TYPE;
		swtp_eint_state2 = EINT_PIN_PLUG_IN;
	} else {
		init_flag2 = swtx_2_eint_type;
		swtp_eint_state2 = EINT_PIN_PLUG_OUT;
	}
#endif


	SWTX_DBG("[SWTP] set type[0x%x / 0x04(high) 0x08(low)] swtp_eint_state[%d], gpio[%d]\n",
		 init_flag, swtp_eint_state, gpio_get_value(swtx_gpiopin));
	irq_set_irq_type(swtx_irq, init_flag);
/* swtp_enable_irq(swtx_irq); */
#ifdef SWTP_2_RF_CON
	SWTX_DBG("[SWTP] set type2[0x%x / 0x04(high) 0x08(low)] swtp_eint_state2[%d], gpio2[%d]\n",
		 init_flag, swtp_eint_state2, gpio_get_value(swtx_gpiopin));
	irq_set_irq_type(swtx_2_irq, init_flag);
/* swtp_enable_irq(swtx_2_irq); */
#endif

#if 0
	mt_eint_set_sens(SWTP_EINT_NUM, SWTP_SENSITIVE_TYPE);
	mt_eint_set_hw_debounce(SWTP_EINT_NUM, SWTP_DEBOUNCE_CN);
	mt_eint_registration(SWTP_EINT_NUM, init_flag, swtp_eint_handler, 0);
#ifdef SWTP_2_RF_CON
	mt_eint_set_sens(SWTP_EINT_2_NUM, SWTP_SENSITIVE_TYPE);
	mt_eint_set_hw_debounce(SWTP_EINT_2_NUM, SWTP_DEBOUNCE_CN);
	mt_eint_registration(SWTP_EINT_2_NUM, init_flag2, swtp_eint_2_handler, 0);
#endif
#endif
	return 0;
}
#else
int swtp_mod_eint_enable(int)
{
	return 0;
}

int swtp_mod_eint_init(struct platform_device *dev)
{
	return 0;
}

int swtp_mod_eint_read(void)
{
	return 0;
}
#endif
