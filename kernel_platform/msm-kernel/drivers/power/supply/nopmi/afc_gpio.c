#define pr_fmt(fmt)	"<afc_gpio-05122355> [%s,%d] " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#define UI 140
#define AFC_COMM_CNT 3
#define AFC_RETRY_MAX 10
#define VBUS_RETRY_MAX 5
#define WAIT_SPING_COUNT 5
#define RECV_SPING_RETRY 5
#define SPING_MIN_UI 10
#define SPING_MAX_UI 20

static spinlock_t afc_spin_lock;
static int afc_data, afc_switch;
static int afc_driver_probe;

static void cycle(int ui)
{
	gpio_direction_output(afc_data, 1);
	udelay(ui - 10);
	gpio_direction_output(afc_data, 0);
	udelay(ui - 10);
}

static void afc_send_Mping(void)
{
	gpio_direction_output(afc_data, 1);
	udelay(17 * UI);
	gpio_direction_output(afc_data, 0);
}

static int afc_recv_Sping(void)
{
	int i = 0, sp_cnt = 0;

	gpio_direction_input(afc_data);
	while (1) {
		udelay(UI);
		if (gpio_get_value(afc_data))
			break;

		if (WAIT_SPING_COUNT < i++) {
			pr_err("waiting time is over!\n");
			goto Sping_err;
		}
	}

	do {
		if (SPING_MAX_UI < sp_cnt++) {
			pr_err("Sping error\n");
			goto Sping_err;
		}
		udelay(UI);
	} while (gpio_get_value(afc_data));

	if (sp_cnt < SPING_MIN_UI) {
		pr_err("Sping timeout error\n");
		goto Sping_err;
	}

	return 0;

Sping_err:
	pr_err("Sping err - len(%d)\n", sp_cnt);

	return -1;
}

static int afc_send_parity_bit(int data)
{
	int cnt = 0, odd = 0;

	for (; data > 0; data = data >> 1)
		if (data & 0x1)
			cnt++;

	odd = cnt % 2;

	if (!odd)
		gpio_direction_output(afc_data, 1);
	else
		gpio_direction_output(afc_data, 0);

	udelay(UI);

	return odd;
}

static void afc_send_data(int data)
{
	int i = 0;

	udelay(UI-20);
	if (data & 0x80)
		cycle(UI / 4);
	else {
		gpio_direction_output(afc_data, 1);
		udelay(UI / 4);
		gpio_direction_output(afc_data, 0);
		udelay(UI / 4);
		// cycle(UI / 4);
		gpio_direction_output(afc_data, 1);
		udelay((UI / 4));
	}

	for (i = 0x80; i > 0; i = i >> 1) {
		gpio_direction_output(afc_data, data & i);
		udelay(UI);
	}

	if (afc_send_parity_bit(data)) {
		cycle(UI / 4);
	} else {
		gpio_direction_output(afc_data, 0);
		udelay((UI / 4));
		cycle(UI / 4);
	}
}

static int afc_recv_data(void)
{
	int ret = 0;

	ret = afc_recv_Sping();
	if (ret < 0) {
		pr_err("afc receive sping error\n");
		return -1;
	}

	mdelay(2);

	ret = afc_recv_Sping();
	if (ret < 0) {
		pr_err("afc receive data error\n");
		return -1;
	}

	return 0;
}

static int afc_communication(unsigned int afc_code)
{
	int ret = 0;

	spin_lock_irq(&afc_spin_lock);

	afc_send_Mping();

	ret = afc_recv_Sping();
	if (ret < 0) {
		pr_err("afc_recv_Sping failed\n");
		goto afc_fail;
	}

	afc_send_data(afc_code);
	afc_send_Mping();

	ret = afc_recv_data();
	if (ret < 0) {
		pr_err("afc receive adapter response error\n");
		goto afc_fail;
	}

	udelay(200);

	afc_send_Mping();

	ret = afc_recv_Sping();
	if (ret < 0) {
		pr_err("afc receive sping error\n");
		goto afc_fail;
	}

	spin_unlock_irq(&afc_spin_lock);

	return 0;

afc_fail:
	pr_err("afc communication failed\n");
	spin_unlock_irq(&afc_spin_lock);
	return -1;
}
int afc_set_voltage_workq(unsigned int afc_code)
{
	int cnt = 0, retrycnt = 0;
	int i = 0, ret = 0;
	if (afc_driver_probe)
		return afc_driver_probe;

	do {
		ret = 0;
		cnt = 0;
		for (i = 0; i < AFC_COMM_CNT; i++) {
			ret = afc_communication(afc_code);
			msleep(38);
			if (ret < 0) {
				pr_err("afc_communication error, i:%d\n", i);
				ret = -EINVAL;
				break;
			} else
				cnt++;
		}
		if (cnt == 3) {
			pr_info("afc_communication succeeded\n");
			break;
		}
		msleep(300);
		retrycnt++;
	} while (retrycnt != AFC_RETRY_MAX);

	gpio_direction_input(afc_data);

	return ret;
}
EXPORT_SYMBOL_GPL(afc_set_voltage_workq);

static int afc_gpio_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;

	pr_err("afc gpio driver start\n");
	afc_data = of_get_named_gpio(node, "afc_data_gpio", 0);
	if (!gpio_is_valid(afc_data )) {
		pr_err("get afc_data_gpio failed\n");
		//return -EINVAL;
	}

	afc_switch = of_get_named_gpio(node, "afc_switch_gpio", 0);
	if (!gpio_is_valid(afc_switch)) {
		pr_err("get afc_switch_gpio failed\n");
		//return -EINVAL;
	}


	spin_lock_init(&afc_spin_lock);
	ret = gpio_request_one(afc_data, GPIOF_IN, "afcdata");
	if (ret) {
		pr_err("request afc_data gpio failed\n");
		//return -EINVAL;
	}
	ret = gpio_request_one(afc_switch, GPIOF_OUT_INIT_HIGH, "afcswitch");
	if (ret) {
		pr_err("request afc_switch gpio failed\n");
		//return -EINVAL;
	}

	pr_err("afc gpio driver load result %d\n", ret);
	afc_driver_probe = ret;

	return 0;
}

static int afc_gpio_remove(struct platform_device *pdev)
{
	gpio_free(afc_switch);
	gpio_free(afc_data);
	
	return 0;
}

static const struct of_device_id afc_gpio_of_match[] = {
	{ .compatible = "afc_gpio", },
	{ },
};

static struct platform_driver afc_gpio_driver = {
	.probe		= afc_gpio_probe,
	.remove		= afc_gpio_remove,
	.driver		= {
		.name	= "afc_gpio",
		.of_match_table = afc_gpio_of_match,
	}
};

static int __init afc_gpio_init(void)
{
	return platform_driver_register(&afc_gpio_driver);
}

static void __exit afc_gpio_exit(void)
{
	platform_driver_unregister(&afc_gpio_driver);
}

module_init(afc_gpio_init);
module_exit(afc_gpio_exit);
MODULE_LICENSE("GPL v2");

