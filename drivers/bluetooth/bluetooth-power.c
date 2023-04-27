// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

/*
 * Bluetooth Power Switch Module
 * controls power to external Bluetooth device
 * with interface to power management device
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/bluetooth-power.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>

#if defined(CONFIG_CNSS)
#include <net/cnss.h>
#endif

#if defined CONFIG_BT_SLIM_QCA6390 || defined CONFIG_BTFM_SLIM_WCN3990
#include "btfm_slim.h"
#include "btfm_slim_slave.h"
#endif
#include <linux/fs.h>

#define BT_PWR_DBG(fmt, arg...)  pr_debug("%s: " fmt "\n", __func__, ## arg)
#define BT_PWR_INFO(fmt, arg...) pr_info("%s: " fmt "\n", __func__, ## arg)
#define BT_PWR_ERR(fmt, arg...)  pr_err("%s: " fmt "\n", __func__, ## arg)

//headers for supporting LPM/OOB
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/serial_core.h>
#include <linux/wakelock.h>
#include <linux/serial_s3c.h>
#include <soc/samsung/exynos-cpupm.h>

#define BT_UPORT 1  //refer BCM define
#define BT_LPM_ENABLE
//#define ACTIVE_LOW_WAKE_HOST_GPIO
#define STATUS_IDLE	1
#define STATUS_BUSY	0

int idle_ip_index;

#ifdef BT_LPM_ENABLE
extern s3c_wake_peer_t s3c2410_serial_wake_peer[CONFIG_SERIAL_SAMSUNG_UARTS];
static int bt_wake_state = -1;
struct _bt_lpm {
	int host_wake;
	int dev_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct uart_port *uport;

	struct wake_lock host_wake_lock;
	struct wake_lock bt_wake_lock;
} bt_lpm_q;
#endif // BT_LPM_ENABLE

enum {
   QCA_HSP_SOC_ID_0100 = 0x400C0100,
   QCA_HSP_SOC_ID_0110 = 0x400C0110,
   QCA_HSP_SOC_ID_0200 = 0x400C0200,
};

static const struct of_device_id bt_power_match_table[] = {
	{	.compatible = "qca,ar3002" },
	{	.compatible = "qca,qca6174" },
	{	.compatible = "qca,wcn3990" },
	{	.compatible = "qca,qca6390" },
	{	.compatible = "qca,wcn6750" },
	{}
};

static struct bluetooth_power_platform_data *bt_power_pdata;
static struct platform_device *btpdev;
static bool previous;
static int pwr_state;
struct class *bt_class;
static int bt_major;
static int soc_id;

static int bt_vreg_init(struct bt_power_vreg_data *vreg)
{
	int rc = 0;
	struct device *dev = &btpdev->dev;

	BT_PWR_DBG("vreg_get for : %s", vreg->name);

	/* Get the regulator handle */
	vreg->reg = regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		rc = PTR_ERR(vreg->reg);
		vreg->reg = NULL;
		pr_err("%s: regulator_get(%s) failed. rc=%d\n",
			__func__, vreg->name, rc);
		goto out;
	}

	if ((regulator_count_voltages(vreg->reg) > 0)
			&& (vreg->low_vol_level) && (vreg->high_vol_level))
		vreg->set_voltage_sup = 1;

out:
	return rc;
}

static int bt_vreg_enable(struct bt_power_vreg_data *vreg)
{
	int rc = 0;
	if (!vreg->is_enabled) {
		rc = regulator_enable(vreg->reg);
		if (rc < 0) {
			BT_PWR_ERR("regulator_enable(%s) failed. rc=%d\n",
					vreg->name, rc);
			goto out;
		}
		vreg->is_enabled = true;
	}

	BT_PWR_ERR("vreg_en successful for : %s", vreg->name);
out:
	return rc;
}

static int bt_vreg_unvote(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	if (!vreg)
		return rc;

	BT_PWR_ERR("vreg_unvote called for : %s", vreg->name);

	return rc;
}

static int bt_vreg_disable(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	if (!vreg)
		return rc;

	if (vreg->is_enabled) {
		rc = regulator_disable(vreg->reg);
		if (rc < 0) {
			BT_PWR_ERR("regulator_disable(%s) failed. rc=%d\n",
					vreg->name, rc);
			goto out;
		}
		vreg->is_enabled = false;
	}

	BT_PWR_ERR("vreg_disable successful for : %s", vreg->name);
out:
	return rc;
}

static int bt_configure_vreg(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	BT_PWR_DBG("config %s", vreg->name);

	/* Get the regulator handle for vreg */
	if (!(vreg->reg)) {
		rc = bt_vreg_init(vreg);
		if (rc < 0)
			return rc;
	}
	rc = bt_vreg_enable(vreg);

	return rc;
}

static int bt_clk_enable(struct bt_power_clk_data *clk)
{
	int rc = 0;

	BT_PWR_DBG("%s", clk->name);

	/* Get the clock handle for vreg */
	if (!clk->clk || clk->is_enabled) {
		BT_PWR_ERR("error - node: %p, clk->is_enabled:%d",
			clk->clk, clk->is_enabled);
		return -EINVAL;
	}

	rc = clk_prepare_enable(clk->clk);
	if (rc) {
		BT_PWR_ERR("failed to enable %s, rc(%d)\n", clk->name, rc);
		return rc;
	}

	clk->is_enabled = true;
	return rc;
}

static int bt_clk_disable(struct bt_power_clk_data *clk)
{
	int rc = 0;

	BT_PWR_DBG("%s", clk->name);

	/* Get the clock handle for vreg */
	if (!clk->clk || !clk->is_enabled) {
		BT_PWR_ERR("error - node: %p, clk->is_enabled:%d",
			clk->clk, clk->is_enabled);
		return -EINVAL;
	}
	clk_disable_unprepare(clk->clk);

	clk->is_enabled = false;
	return rc;
}

static int bt_configure_gpios(int on)
{
	int rc = 0;
	int bt_reset_gpio = bt_power_pdata->bt_gpio_sys_rst;

	if (on) {
		gpio_direction_output(bt_reset_gpio, 0);
		msleep(50);
		BT_PWR_INFO("BTON:Turn Bt Off bt-reset-gpio(%d) value(%d)\n",
				bt_reset_gpio, gpio_get_value(bt_reset_gpio));

		gpio_direction_output(bt_reset_gpio, 1);
		msleep(50);
		BT_PWR_INFO("BTON:Turn Bt On bt-reset-gpio(%d) value(%d)\n",
		bt_reset_gpio, gpio_get_value(bt_reset_gpio));
	} else {
		gpio_set_value(bt_reset_gpio, 0);
		msleep(100);
		BT_PWR_INFO("BT-OFF:bt-reset-gpio(%d) value(%d)\n",
				bt_reset_gpio, gpio_get_value(bt_reset_gpio));

	}

	BT_PWR_INFO("bt_gpio= %d on: %d is successful", bt_reset_gpio, on);
	return rc;
}

static int bluetooth_power(int on)
{
	int rc = 0;

	BT_PWR_DBG("on: %d", on);

	if (on == 1) {
		// Power On
		if (bt_power_pdata->bt_vdd_io) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_io);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddio config failed");
				goto out;
			}
		}
		if (bt_power_pdata->bt_vdd_xtal) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_xtal);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddxtal config failed");
				goto vdd_xtal_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_core) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_core);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddcore config failed");
				goto vdd_core_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_pa) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_pa);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddpa config failed");
				goto vdd_pa_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_ldo) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_ldo);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddldo config failed");
				goto vdd_ldo_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_aon) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_aon);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddaon config failed");
				goto vdd_aon_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_dig) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_dig);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vdddig config failed");
				goto vdd_dig_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_rfa1) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_rfa1);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddrfa1 config failed");
				goto vdd_rfa1_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_rfa2) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_rfa2);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddrfa2 config failed");
				goto vdd_rfa2_fail;
			}
		}
		if (bt_power_pdata->bt_vdd_asd) {
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_asd);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddasd config failed");
				goto vdd_asd_fail;
			}
		}
		if (bt_power_pdata->bt_chip_pwd) {
			rc = bt_configure_vreg(bt_power_pdata->bt_chip_pwd);
			if (rc < 0) {
				BT_PWR_ERR("bt_power chippwd config failed");
				goto chip_pwd_fail;
			}
		}
		/* Parse dt_info and check if a target requires clock voting.
		 * Enable BT clock when BT is on and disable it when BT is off
		 */
		if (bt_power_pdata->bt_chip_clk) {
			rc = bt_clk_enable(bt_power_pdata->bt_chip_clk);
			if (rc < 0) {
				BT_PWR_ERR("bt_power gpio config failed");
				goto clk_fail;
			}
		}
		if (bt_power_pdata->bt_gpio_sys_rst > 0) {
			rc = bt_configure_gpios(on);
			if (rc < 0) {
				BT_PWR_ERR("bt_power gpio config failed");
				goto gpio_fail;
			}
		}
#ifdef BT_LPM_ENABLE
		if ( irq_set_irq_wake(bt_power_pdata->irq, 1)) {
			BT_PWR_ERR("[BT] Set_irq_wake failed.\n");
			goto gpio_fail;
		}
#endif // BT_LPM_ENABLE		
	} else if (on == 0) {
		// Power Off
#ifdef BT_LPM_ENABLE
		if (irq_set_irq_wake(bt_power_pdata->irq, 0)) {
			BT_PWR_ERR("[BT] Release_irq_wake failed.\n");
			goto gpio_fail;
		}
#endif //BT_LPM_ENABLE	

		if (bt_power_pdata->bt_gpio_sys_rst > 0)
			bt_configure_gpios(on);
gpio_fail:
		if (bt_power_pdata->bt_gpio_sys_rst > 0)
			gpio_free(bt_power_pdata->bt_gpio_sys_rst);
		if  (bt_power_pdata->bt_gpio_sw_ctrl  >  0)
			gpio_free(bt_power_pdata->bt_gpio_sw_ctrl);
		if  (bt_power_pdata->bt_gpio_debug  >  0)
			gpio_free(bt_power_pdata->bt_gpio_debug);
		if (bt_power_pdata->bt_chip_clk)
			bt_clk_disable(bt_power_pdata->bt_chip_clk);
clk_fail:
		if (bt_power_pdata->bt_chip_pwd)
			bt_vreg_disable(bt_power_pdata->bt_chip_pwd);
chip_pwd_fail:
		if (bt_power_pdata->bt_vdd_asd)
			bt_vreg_disable(bt_power_pdata->bt_vdd_asd);
vdd_asd_fail:
		if (bt_power_pdata->bt_vdd_rfa2)
			bt_vreg_disable(bt_power_pdata->bt_vdd_rfa2);
vdd_rfa2_fail:
		if (bt_power_pdata->bt_vdd_rfa1)
			bt_vreg_disable(bt_power_pdata->bt_vdd_rfa1);
vdd_rfa1_fail:
		if (bt_power_pdata->bt_vdd_dig)
			bt_vreg_disable(bt_power_pdata->bt_vdd_dig);
vdd_dig_fail:
		if (bt_power_pdata->bt_vdd_aon)
			bt_vreg_disable(bt_power_pdata->bt_vdd_aon);
vdd_aon_fail:
		if (bt_power_pdata->bt_vdd_ldo)
			bt_vreg_disable(bt_power_pdata->bt_vdd_ldo);
vdd_ldo_fail:
		if (bt_power_pdata->bt_vdd_pa)
			bt_vreg_disable(bt_power_pdata->bt_vdd_pa);
vdd_pa_fail:
		if (bt_power_pdata->bt_vdd_core)
			bt_vreg_disable(bt_power_pdata->bt_vdd_core);
vdd_core_fail:
		if (bt_power_pdata->bt_vdd_xtal)
			bt_vreg_disable(bt_power_pdata->bt_vdd_xtal);
vdd_xtal_fail:
		if (bt_power_pdata->bt_vdd_io)
			bt_vreg_disable(bt_power_pdata->bt_vdd_io);
#ifdef BT_LPM_ENABLE
		BT_PWR_ERR("[BT] bluetooth_power will be off. release host wakelock in 1s\n");
		wake_lock_timeout(&bt_lpm_q.host_wake_lock, HZ/2);
#endif //BT_LPM_ENABLE
	} else if (on == 2) {
		/* Retention mode */
		if (bt_power_pdata->bt_vdd_rfa2)
			bt_vreg_unvote(bt_power_pdata->bt_vdd_rfa2);
		if (bt_power_pdata->bt_vdd_rfa1)
			bt_vreg_unvote(bt_power_pdata->bt_vdd_rfa1);
		if (bt_power_pdata->bt_vdd_dig)
			bt_vreg_unvote(bt_power_pdata->bt_vdd_dig);
		if (bt_power_pdata->bt_vdd_aon)
			bt_vreg_unvote(bt_power_pdata->bt_vdd_aon);
	} else {
		BT_PWR_ERR("Invalid power mode: %d", on);
		rc = -1;
	}
out:
	return rc;
}

static int bluetooth_toggle_radio(void *data, bool blocked)
{
	int ret = 0;
	int (*power_control)(int enable);

	power_control =
		((struct bluetooth_power_platform_data *)data)->bt_power_setup;

	if (previous != blocked)
		ret = (*power_control)(!blocked);
	if (!ret)
		previous = blocked;
	return ret;
}

static const struct rfkill_ops bluetooth_power_rfkill_ops = {
	.set_block = bluetooth_toggle_radio,
};

#if defined(CONFIG_CNSS) && defined(CONFIG_CLD_LL_CORE)
static ssize_t extldo_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int ret;
	bool enable = false;
	struct cnss_platform_cap cap;

	ret = cnss_get_platform_cap(&cap);
	if (ret) {
		BT_PWR_ERR("Platform capability info from CNSS not available!");
		enable = false;
	} else if (!ret && (cap.cap_flag & CNSS_HAS_EXTERNAL_SWREG)) {
		enable = true;
	}
	return snprintf(buf, 6, "%s", (enable ? "true" : "false"));
}
#else
static ssize_t extldo_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, 6, "%s", "false");
}
#endif

static DEVICE_ATTR_RO(extldo);

static int bluetooth_power_rfkill_probe(struct platform_device *pdev)
{
	struct rfkill *rfkill;
	int ret;

	rfkill = rfkill_alloc("bt_power", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			      &bluetooth_power_rfkill_ops,
			      pdev->dev.platform_data);

	if (!rfkill) {
		dev_err(&pdev->dev, "rfkill allocate failed\n");
		return -ENOMEM;
	}

	/* add file into rfkill0 to handle LDO27 */
	ret = device_create_file(&pdev->dev, &dev_attr_extldo);
	if (ret < 0)
		BT_PWR_ERR("device create file error!");

	/* force Bluetooth off during init to allow for user control */
	rfkill_init_sw_state(rfkill, 1);
	previous = true;

	ret = rfkill_register(rfkill);
	if (ret) {
		dev_err(&pdev->dev, "rfkill register failed=%d\n", ret);
		rfkill_destroy(rfkill);
		return ret;
	}

	platform_set_drvdata(pdev, rfkill);

	return 0;
}

static void bluetooth_power_rfkill_remove(struct platform_device *pdev)
{
	struct rfkill *rfkill;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	rfkill = platform_get_drvdata(pdev);
	if (rfkill)
		rfkill_unregister(rfkill);
	rfkill_destroy(rfkill);
	platform_set_drvdata(pdev, NULL);
}

#define MAX_PROP_SIZE 32
static int bt_dt_parse_vreg_info(struct device *dev,
		struct bt_power_vreg_data **vreg_data, const char *vreg_name)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];
	struct bt_power_vreg_data *vreg;
	struct device_node *np = dev->of_node;

	BT_PWR_DBG("vreg dev tree parse for %s", vreg_name);

	*vreg_data = NULL;
	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", vreg_name);
	if (of_parse_phandle(np, prop_name, 0)) {
		vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
		if (!vreg) {
			BT_PWR_ERR("No memory for vreg: %s", vreg_name);
			ret = -ENOMEM;
			goto err;
		}

		vreg->name = vreg_name;

		/* Parse voltage-level from each node */
		snprintf(prop_name, MAX_PROP_SIZE,
				"%s-voltage-level", vreg_name);
		prop = of_get_property(np, prop_name, &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_warn(dev, "%s %s property\n",
				prop ? "invalid format" : "no", prop_name);
		} else {
			vreg->low_vol_level = be32_to_cpup(&prop[0]);
			vreg->high_vol_level = be32_to_cpup(&prop[1]);
		}

		/* Parse current-level from each node */
		snprintf(prop_name, MAX_PROP_SIZE,
				"%s-current-level", vreg_name);
		ret = of_property_read_u32(np, prop_name, &vreg->load_uA);
		if (ret < 0) {
			BT_PWR_DBG("%s property is not valid\n", prop_name);
			vreg->load_uA = -1;
			ret = 0;
		}

		*vreg_data = vreg;
		BT_PWR_DBG("%s: vol=[%d %d]uV, current=[%d]uA\n",
			vreg->name, vreg->low_vol_level,
			vreg->high_vol_level,
			vreg->load_uA);
	} else
		BT_PWR_INFO("%s: is not provided in device tree", vreg_name);

err:
	return ret;
}

static int bt_dt_parse_clk_info(struct device *dev,
		struct bt_power_clk_data **clk_data)
{
	int ret = 0;
	struct bt_power_clk_data *clk = NULL;
	struct device_node *np = dev->of_node;

	BT_PWR_DBG("");

	*clk_data = NULL;
	if (of_parse_phandle(np, "clocks", 0)) {
		clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
		if (!clk) {
			BT_PWR_ERR("No memory for clocks");
			ret = -ENOMEM;
			goto err;
		}

		/* Allocated 20 bytes size buffer for clock name string */
		clk->name = devm_kzalloc(dev, 20, GFP_KERNEL);

		/* Parse clock name from node */
		ret = of_property_read_string_index(np, "clock-names", 0,
				&(clk->name));
		if (ret < 0) {
			BT_PWR_ERR("reading \"clock-names\" failed");
			return ret;
		}

		clk->clk = devm_clk_get(dev, clk->name);
		if (IS_ERR(clk->clk)) {
			ret = PTR_ERR(clk->clk);
			BT_PWR_ERR("failed to get %s, ret (%d)",
				clk->name, ret);
			clk->clk = NULL;
			return ret;
		}

		*clk_data = clk;
	} else {
		BT_PWR_INFO("clocks is not provided in device tree");
	}

err:
	return ret;
}

static int bt_power_populate_dt_pinfo(struct platform_device *pdev)
{
	int rc;

	BT_PWR_DBG("");

	if (!bt_power_pdata)
		return -ENOMEM;

	if (pdev->dev.of_node) {
/***
		bt_power_pdata->bt_gpio_sys_rst =
			of_get_named_gpio(pdev->dev.of_node,
						"qca,bt-reset-gpio", 0);
		if (bt_power_pdata->bt_gpio_sys_rst < 0)
			BT_PWR_INFO("bt-reset-gpio not provided in devicetree");

		bt_power_pdata->bt_gpio_sw_ctrl  =
			of_get_named_gpio(pdev->dev.of_node,
						"qca,bt-sw-ctrl-gpio",  0);

		bt_power_pdata->bt_gpio_debug  =
			of_get_named_gpio(pdev->dev.of_node,
						"qca,bt-debug-gpio",  0);
***/
		bt_power_pdata->bt_gpio_sys_rst = of_get_gpio(pdev->dev.of_node, 0);
		if (bt_power_pdata->bt_gpio_sys_rst < 0) {
			BT_PWR_ERR("bt-reset-gpio not provided in device tree");
			return bt_power_pdata->bt_gpio_sys_rst;
		}
		if ((rc = gpio_request(bt_power_pdata->bt_gpio_sys_rst, "bten_gpio")) < 0) {
			BT_PWR_ERR("bt-reset-gpio request failed.\n");
			return rc;	
		}

		bt_power_pdata->bt_gpio_bt_wake = of_get_gpio(pdev->dev.of_node, 1);
		if (bt_power_pdata->bt_gpio_bt_wake < 0) {
			BT_PWR_ERR("bt-wake-gpio not provided in device tree");
			return bt_power_pdata->bt_gpio_bt_wake;
		}

		if ((rc = gpio_request(bt_power_pdata->bt_gpio_bt_wake, "btwake_gpio")) < 0) {
			BT_PWR_ERR("bt-wake-gpio request failed.\n");
			return rc;	
		}
		
		bt_power_pdata->bt_gpio_host_wake = of_get_gpio(pdev->dev.of_node, 2);
		if (bt_power_pdata->bt_gpio_host_wake < 0) {
			BT_PWR_ERR("bt-hostwake-gpio not provided in device tree");
			return bt_power_pdata->bt_gpio_host_wake;
		}
		if ((rc = gpio_request(bt_power_pdata->bt_gpio_host_wake, "bthostwake_gpio")) < 0) {
			BT_PWR_ERR("host-wake-gpio request failed.\n");
			return rc;	
		}

		gpio_direction_input(bt_power_pdata->bt_gpio_host_wake);
		gpio_direction_output(bt_power_pdata->bt_gpio_bt_wake, 0);
		gpio_direction_output(bt_power_pdata->bt_gpio_sys_rst, 0);



		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_core,
					"qca,bt-vdd-core");

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_io,
					"qca,bt-vdd-io");

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_xtal,
					"qca,bt-vdd-xtal");

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_pa,
					"qca,bt-vdd-pa");

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_ldo,
					"qca,bt-vdd-ldo");

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_chip_pwd,
					"qca,bt-chip-pwd");

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_aon,
					"qca,bt-vdd-aon");

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_dig,
					"qca,bt-vdd-dig");

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_rfa1,
					"qca,bt-vdd-rfa1");

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_rfa2,
					"qca,bt-vdd-rfa2");

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_asd,
					"qca,bt-vdd-asd");

		rc = bt_dt_parse_clk_info(&pdev->dev,
					&bt_power_pdata->bt_chip_clk);
	}

	bt_power_pdata->bt_power_setup = bluetooth_power;

	return 0;
}

#ifdef BT_LPM_ENABLE
static void set_wake_locked(int wake)
{
	if (wake)
		wake_lock(&bt_lpm_q.bt_wake_lock);

	gpio_set_value(bt_power_pdata->bt_gpio_bt_wake, wake);
	bt_lpm_q.dev_wake = wake;

	if (bt_wake_state != wake)
	{
		BT_PWR_ERR("[BT] set_wake_locked value = %d\n", wake);
		bt_wake_state = wake;
	}
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer)
{
	if (bt_lpm_q.uport != NULL)
		set_wake_locked(0);

    if (bt_lpm_q.host_wake == 0)
	    exynos_update_ip_idle_status(idle_ip_index, STATUS_IDLE);
	wake_lock_timeout(&bt_lpm_q.bt_wake_lock, HZ/2);

	BT_PWR_ERR("LPM Timer Expred\n");
	return HRTIMER_NORESTART;
}

void qcomm_bt_lpm_exit_lpm_locked(struct uart_port *uport)
{
	bt_lpm_q.uport = uport;

	hrtimer_try_to_cancel(&bt_lpm_q.enter_lpm_timer);
	exynos_update_ip_idle_status(idle_ip_index, STATUS_BUSY);
	set_wake_locked(1);

	BT_PWR_ERR("host has data to send\n");
	hrtimer_start(&bt_lpm_q.enter_lpm_timer, bt_lpm_q.enter_lpm_delay,
		HRTIMER_MODE_REL);
}

static void update_host_wake_locked(int host_wake)
{
	if (host_wake == bt_lpm_q.host_wake) {
		BT_PWR_ERR("No Change in Host LPM State\n");
		return;
	}

	BT_PWR_ERR("soc has data to send - %d\n", host_wake);
	bt_lpm_q.host_wake = host_wake;

	if (host_wake) {
		exynos_update_ip_idle_status(idle_ip_index, STATUS_BUSY);
		wake_lock(&bt_lpm_q.host_wake_lock);
	} else  {
		/* Take a timed wakelock, so that upper layers can take it.
		 * The chipset deasserts the hostwake lock, when there is no
		 * more data to send.
		 */
		BT_PWR_ERR("[BT] update_host_wake_locked host_wake is deasserted. release wakelock in 1s\n");
		wake_lock_timeout(&bt_lpm_q.host_wake_lock, HZ/2);
		
        if (bt_lpm_q.dev_wake == 0)
            exynos_update_ip_idle_status(idle_ip_index, STATUS_IDLE);
	}
}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;

	host_wake = gpio_get_value(bt_power_pdata->bt_gpio_host_wake);
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);

#ifdef ACTIVE_LOW_WAKE_HOST_GPIO
	host_wake = host_wake ? 0 : 1;
#endif

	if (!bt_lpm_q.uport) {
		bt_lpm_q.host_wake = host_wake;
		BT_PWR_ERR("[BT] host_wake_isr uport is null\n");
		return IRQ_HANDLED;
	}

	update_host_wake_locked(host_wake);

	return IRQ_HANDLED;
}

static int qcomm_bt_lpm_init(struct platform_device *pdev)
{
	int ret;
	BT_PWR_ERR("BT LPM Initialization\n");

	hrtimer_init(&bt_lpm_q.enter_lpm_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	bt_lpm_q.enter_lpm_delay = ktime_set(1, 0);  /* 1 sec */ /*1->3*//*3->4*/
	bt_lpm_q.enter_lpm_timer.function = enter_lpm;

	bt_lpm_q.host_wake = 0;

	wake_lock_init(&bt_lpm_q.host_wake_lock, WAKE_LOCK_SUSPEND,
			 "BT_host_wake");
	wake_lock_init(&bt_lpm_q.bt_wake_lock, WAKE_LOCK_SUSPEND,
			 "BT_bt_wake");

	s3c2410_serial_wake_peer[BT_UPORT] = (s3c_wake_peer_t) qcomm_bt_lpm_exit_lpm_locked;


	bt_power_pdata->irq = gpio_to_irq(bt_power_pdata->bt_gpio_host_wake);
#ifdef ACTIVE_LOW_WAKE_HOST_GPIO
	ret = request_irq(bt_power_pdata->irq, host_wake_isr, IRQF_TRIGGER_FALLING, "bt_host_wake", NULL);
#else
	ret = request_irq(bt_power_pdata->irq, host_wake_isr, IRQF_TRIGGER_RISING, "bt_host_wake", NULL);
#endif

	if (ret) {
		BT_PWR_ERR("[BT] Request_host wake irq failed.\n");
		return ret;
	}

	return 0;
}
#endif

static int bt_power_probe(struct platform_device *pdev)
{
	int ret = 0;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	bt_power_pdata =
		kzalloc(sizeof(struct bluetooth_power_platform_data),
			GFP_KERNEL);

	if (!bt_power_pdata) {
		BT_PWR_ERR("Failed to allocate memory");
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		ret = bt_power_populate_dt_pinfo(pdev);
		if (ret < 0) {
			BT_PWR_ERR("Failed to populate device tree info");
			goto free_pdata;
		}
		pdev->dev.platform_data = bt_power_pdata;
	} else if (pdev->dev.platform_data) {
		/* Optional data set to default if not provided */
		if (!((struct bluetooth_power_platform_data *)
			(pdev->dev.platform_data))->bt_power_setup)
			((struct bluetooth_power_platform_data *)
				(pdev->dev.platform_data))->bt_power_setup =
						bluetooth_power;

		memcpy(bt_power_pdata, pdev->dev.platform_data,
			sizeof(struct bluetooth_power_platform_data));
		pwr_state = 0;
	} else {
		BT_PWR_ERR("Failed to get platform data");
		goto free_pdata;
	}

	if (bluetooth_power_rfkill_probe(pdev) < 0)
		goto free_pdata;

	btpdev = pdev;

#ifdef BT_LPM_ENABLE
	qcomm_bt_lpm_init(pdev);
#endif
	idle_ip_index = exynos_get_idle_ip_index("bluetooth");
    exynos_update_ip_idle_status(idle_ip_index, STATUS_IDLE);

	return 0;
free_pdata:
	kfree(bt_power_pdata);
	return ret;
}

static int bt_power_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s\n", __func__);

	bluetooth_power_rfkill_remove(pdev);

	if (bt_power_pdata->bt_chip_pwd->reg)
		regulator_put(bt_power_pdata->bt_chip_pwd->reg);

	kfree(bt_power_pdata);
#ifdef BT_LPM_ENABLE		
		wake_lock_destroy(&bt_lpm_q.host_wake_lock);
		wake_lock_destroy(&bt_lpm_q.bt_wake_lock);
#endif
	return 0;
}

int bt_register_slimdev(struct device *dev)
{
	BT_PWR_DBG("");
	if (!bt_power_pdata || (dev == NULL)) {
		BT_PWR_ERR("Failed to allocate memory");
		return -EINVAL;
	}
	bt_power_pdata->slim_dev = dev;
	return 0;
}

int get_chipset_version(void)
{
	BT_PWR_DBG("");
	return soc_id;
}

int bt_disable_asd(void)
{
	int rc = 0;
	if (bt_power_pdata->bt_vdd_asd) {
		BT_PWR_INFO("Disabling ASD regulator");
		rc = bt_vreg_disable(bt_power_pdata->bt_vdd_asd);
	} else {
		BT_PWR_INFO("ASD regulator is not configured");
	}
	return rc;
}

static long bt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0, pwr_cntrl = 0;
	int chipset_version = 0;
	long  value  =  -1;

	switch (cmd) {
	case BT_CMD_SLIM_TEST:
#if defined CONFIG_BT_SLIM_QCA6390 || defined CONFIG_BTFM_SLIM_WCN3990
		if (!bt_power_pdata->slim_dev) {
			BT_PWR_ERR("slim_dev is null\n");
			return -EINVAL;
		}
		ret = btfm_slim_hw_init(
			bt_power_pdata->slim_dev->platform_data
		);
#endif
		break;
	case BT_CMD_PWR_CTRL:
		pwr_cntrl = (int)arg;
		BT_PWR_ERR("BT_CMD_PWR_CTRL pwr_cntrl:%d", pwr_cntrl);
		if (pwr_state != pwr_cntrl) {
			ret = bluetooth_power(pwr_cntrl);
			if (!ret)
				pwr_state = pwr_cntrl;
		} else {
			BT_PWR_ERR("BT state already:%d no change done\n"
				, pwr_state);
			ret = 0;
		}
		break;
	case BT_CMD_CHIPSET_VERS:
		chipset_version = (int)arg;
		BT_PWR_ERR("unified Current SOC Version : %x", chipset_version);
		if (chipset_version) {
			soc_id = chipset_version;
			if (soc_id == QCA_HSP_SOC_ID_0100 ||
				soc_id == QCA_HSP_SOC_ID_0110 ||
				soc_id == QCA_HSP_SOC_ID_0200) {
				ret = bt_disable_asd();
			}
		} else {
			BT_PWR_ERR("got invalid soc version");
			soc_id = 0;
		}
		break;
	case BT_CMD_GETVAL_RESET_GPIO:
		if (bt_power_pdata->bt_gpio_sys_rst > 0) {
			value = (long)gpio_get_value(
				bt_power_pdata->bt_gpio_sys_rst);
			BT_PWR_ERR("GET_RESET_GPIO(%d) value(%d)",
				bt_power_pdata->bt_gpio_sys_rst, value);
			ret = value;
		} else {
			BT_PWR_ERR("RESET_GPIO not configured");
			ret = -EINVAL;
		}
		break;
	case BT_CMD_GETVAL_SW_CTRL_GPIO:
		if (bt_power_pdata->bt_gpio_sw_ctrl > 0) {
			value = (long)gpio_get_value(
				bt_power_pdata->bt_gpio_sw_ctrl);
			BT_PWR_ERR("GET_SWCTRL_GPIO(%d) value(%d)",
				bt_power_pdata->bt_gpio_sw_ctrl,  value);
			ret = value;
		} else {
			BT_PWR_ERR("SW_CTRL_GPIO not configured");
			ret = -EINVAL;
		}
		break;
	case BT_CMD_GETVAL_VDD_AON_LDO:
		if ((bt_power_pdata->bt_vdd_aon) &&
				(bt_power_pdata->bt_vdd_aon->is_enabled) &&
				(regulator_is_enabled(
					bt_power_pdata->bt_vdd_aon->reg))) {
			value = (int)regulator_get_voltage(
				bt_power_pdata->bt_vdd_aon->reg);
			BT_PWR_ERR("GET_VDD_AON_LDO(%d) value(%d)",
				bt_power_pdata->bt_vdd_aon, value);
			ret = value;
		} else {
			BT_PWR_ERR("VDD-AON_LDO not configure/enabled");
			ret = -EINVAL;
		}
		break;
	case BT_CMD_GETVAL_VDD_DIG_LDO:
		if ((bt_power_pdata->bt_vdd_dig) &&
				(bt_power_pdata->bt_vdd_dig->is_enabled) &&
				(regulator_is_enabled(
					bt_power_pdata->bt_vdd_dig->reg))) {
			value = (int)regulator_get_voltage(
				bt_power_pdata->bt_vdd_dig->reg);
			BT_PWR_ERR("GET_VDD_DIG_LDO(%d) value(%d)",
				bt_power_pdata->bt_vdd_dig, value);
			ret = value;
		} else {
			BT_PWR_ERR("VDD-DIG-LDO not configured/enabled");
			ret = -EINVAL;
		}
		break;
	case BT_CMD_GETVAL_VDD_RFA1_LDO:
		if ((bt_power_pdata->bt_vdd_rfa1) &&
				(bt_power_pdata->bt_vdd_rfa1->is_enabled) &&
				(regulator_is_enabled(
					bt_power_pdata->bt_vdd_rfa1->reg))) {
			value = (int)regulator_get_voltage(
				bt_power_pdata->bt_vdd_rfa1->reg);
			BT_PWR_ERR("GET_VDD_RFA1_LDO(%d) value(%d)",
				bt_power_pdata->bt_vdd_rfa1, value);
			ret = value;
		} else {
			BT_PWR_ERR("VDD-RFA1-LDO not configure/enabled");
			ret = -EINVAL;
		}
		break;
	case BT_CMD_GETVAL_VDD_RFA2_LDO:
		if ((bt_power_pdata->bt_vdd_rfa2) &&
				(bt_power_pdata->bt_vdd_rfa2->is_enabled) &&
				(regulator_is_enabled(
					bt_power_pdata->bt_vdd_rfa2->reg))) {
			value = (int)regulator_get_voltage(
				bt_power_pdata->bt_vdd_rfa2->reg);
			BT_PWR_ERR("GET_VDD_RFA2_LDO(%d) value(%d)",
				bt_power_pdata->bt_vdd_rfa2, value);
			ret = value;
		}  else  {
			BT_PWR_ERR("VDD-RFA2-LDO not configure/enabled");
			ret = -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static struct platform_driver bt_power_driver = {
	.probe = bt_power_probe,
	.remove = bt_power_remove,
	.driver = {
		.name = "bt_power",
		.of_match_table = bt_power_match_table,
	},
};

static const struct file_operations bt_dev_fops = {
	.unlocked_ioctl = bt_ioctl,
	.compat_ioctl = bt_ioctl,
};

static int __init bluetooth_power_init(void)
{
	int ret;

	ret = platform_driver_register(&bt_power_driver);

	bt_major = register_chrdev(0, "bt", &bt_dev_fops);
	if (bt_major < 0) {
		BT_PWR_ERR("failed to allocate char dev\n");
		goto chrdev_unreg;
	}

	bt_class = class_create(THIS_MODULE, "bt-dev");
	if (IS_ERR(bt_class)) {
		BT_PWR_ERR("coudn't create class");
		goto chrdev_unreg;
	}


	if (device_create(bt_class, NULL, MKDEV(bt_major, 0),
		NULL, "btpower") == NULL) {
		BT_PWR_ERR("failed to allocate char dev\n");
		goto chrdev_unreg;
	}
	return 0;

chrdev_unreg:
	unregister_chrdev(bt_major, "bt");
	class_destroy(bt_class);
	return ret;
}

static void __exit bluetooth_power_exit(void)
{
	platform_driver_unregister(&bt_power_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM Bluetooth power control driver");

module_init(bluetooth_power_init);
module_exit(bluetooth_power_exit);
