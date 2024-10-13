// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Willsemi Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/version.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>

#include "../inc/pd_dbg_info.h"
#include "../inc/tcpci.h"
#include "../inc/wusb3812c.h"

#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* CONFIG_RT_REGMAP */

#include <linux/sched/rt.h>
#include <linux/power_supply.h>

#define WUSB3812_VID	0x34D0
#define WUSB3812_PID	0x0401

#define WS_DBG_EN	0	//0-no, 1-printk

#define WS_DBG(fmt, ...) \
	do { \
		if (WS_DBG_EN == 1) \
			printk("%s: " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
#define WUSB3812C_DRV_VERSION	"1.6.21a_MTK"
/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */

#define WUSB3812C_IRQ_WAKE_TIME	(500) /* ms */

/* A06 code for SR-AL7160A-01-272 by zhangziyi at 20240327 start */
struct wusb3812_chip {
	struct i2c_client *client;
	struct device *dev;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *m_dev;
#endif /* CONFIG_RT_REGMAP */
	struct semaphore io_lock;
	struct semaphore suspend_lock;
	struct tcpc_desc *tcpc_desc;
	struct tcpc_device *tcpc;
	struct kthread_worker irq_worker;
	struct kthread_work irq_work;
	struct task_struct *irq_worker_task;
	struct wakeup_source *irq_wake_lock;

	int irq_gpio;
	int irq;
	int chip_id;
	struct power_supply *typec_psy;
	struct power_supply_desc psd;
};

enum {
	TYPEC_STATUS_UNKNOWN = 0,
	TYPEC_STATUS_CC1,
	TYPEC_STATUS_CC2,
};
/* A06 code for SR-AL7160A-01-272 by zhangziyi at 20240327 end */

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(TCPC_V10_REG_VID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_DID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TYPEC_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PD_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PDIF_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT_MASK, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS_MASK, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS_MASK, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TCPC_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_ROLE_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_CC_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_COMMAND, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_MSG_HDR_INFO, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DETECT, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_RX_BYTE_CNT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_BUF_FRAME_TYPE, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_HDR, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DATA, 28, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TRANSMIT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_BYTE_CNT, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TX_HDR, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TX_DATA, 28, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_CONFIG_GPIO0, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_PHY_CTRL1, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_CLK_CTRL2, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_CLK_CTRL3, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_PRL_FSM_RESET, 1, RT_VOLATILE, {});
RT_REG_DECL(WUSB3812C_REG_BMC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(WUSB3812C_REG_BMCIO_RXDZSEL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_VCONN_CLIMITEN, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_RT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(WUSB3812C_REG_RT_INT, 1, RT_VOLATILE, {});
RT_REG_DECL(WUSB3812C_REG_RT_MASK, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_IDLE_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_INTRST_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_WATCHDOG_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_I2CRST_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_SWRESET, 1, RT_VOLATILE, {});
RT_REG_DECL(WUSB3812C_REG_TTCPC_FILTER, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_DRP_TOGGLE_CYCLE, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_DRP_DUTY_CTRL, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_BMCIO_RXDZEN, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(WUSB3812C_REG_UNLOCK_PW_2, 2, RT_VOLATILE, {});
RT_REG_DECL(WUSB3812C_REG_EFUSE5, 1, RT_VOLATILE, {});

static const rt_register_map_t wusb3812_chip_regmap[] = {
	RT_REG(TCPC_V10_REG_VID),
	RT_REG(TCPC_V10_REG_PID),
	RT_REG(TCPC_V10_REG_DID),
	RT_REG(TCPC_V10_REG_TYPEC_REV),
	RT_REG(TCPC_V10_REG_PD_REV),
	RT_REG(TCPC_V10_REG_PDIF_REV),
	RT_REG(TCPC_V10_REG_ALERT),
	RT_REG(TCPC_V10_REG_ALERT_MASK),
	RT_REG(TCPC_V10_REG_POWER_STATUS_MASK),
	RT_REG(TCPC_V10_REG_FAULT_STATUS_MASK),
	RT_REG(TCPC_V10_REG_TCPC_CTRL),
	RT_REG(TCPC_V10_REG_ROLE_CTRL),
	RT_REG(TCPC_V10_REG_FAULT_CTRL),
	RT_REG(TCPC_V10_REG_POWER_CTRL),
	RT_REG(TCPC_V10_REG_CC_STATUS),
	RT_REG(TCPC_V10_REG_POWER_STATUS),
	RT_REG(TCPC_V10_REG_FAULT_STATUS),
	RT_REG(TCPC_V10_REG_COMMAND),
	RT_REG(TCPC_V10_REG_MSG_HDR_INFO),
	RT_REG(TCPC_V10_REG_RX_DETECT),
	RT_REG(TCPC_V10_REG_RX_BYTE_CNT),
	RT_REG(TCPC_V10_REG_RX_BUF_FRAME_TYPE),
	RT_REG(TCPC_V10_REG_RX_HDR),
	RT_REG(TCPC_V10_REG_RX_DATA),
	RT_REG(TCPC_V10_REG_TRANSMIT),
	RT_REG(TCPC_V10_REG_TX_BYTE_CNT),
	RT_REG(TCPC_V10_REG_TX_HDR),
	RT_REG(TCPC_V10_REG_TX_DATA),
	RT_REG(WUSB3812C_REG_CONFIG_GPIO0),
	RT_REG(WUSB3812C_REG_PHY_CTRL1),
	RT_REG(WUSB3812C_REG_CLK_CTRL2),
	RT_REG(WUSB3812C_REG_CLK_CTRL3),
	RT_REG(WUSB3812C_REG_PRL_FSM_RESET),
	RT_REG(WUSB3812C_REG_BMC_CTRL),
	RT_REG(WUSB3812C_REG_BMCIO_RXDZSEL),
	RT_REG(WUSB3812C_REG_VCONN_CLIMITEN),
	RT_REG(WUSB3812C_REG_RT_STATUS),
	RT_REG(WUSB3812C_REG_RT_INT),
	RT_REG(WUSB3812C_REG_RT_MASK),
	RT_REG(WUSB3812C_REG_IDLE_CTRL),
	RT_REG(WUSB3812C_REG_INTRST_CTRL),
	RT_REG(WUSB3812C_REG_WATCHDOG_CTRL),
	RT_REG(WUSB3812C_REG_I2CRST_CTRL),
	RT_REG(WUSB3812C_REG_SWRESET),
	RT_REG(WUSB3812C_REG_TTCPC_FILTER),
	RT_REG(WUSB3812C_REG_DRP_TOGGLE_CYCLE),
	RT_REG(WUSB3812C_REG_DRP_DUTY_CTRL),
	RT_REG(WUSB3812C_REG_BMCIO_RXDZEN),
	RT_REG(WUSB3812C_REG_UNLOCK_PW_2),
	RT_REG(WUSB3812C_REG_EFUSE5),
};
#define WUSB3812_CHIP_REGMAP_SIZE ARRAY_SIZE(wusb3812_chip_regmap)

#endif /* CONFIG_RT_REGMAP */
/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
static int wusb3812_set_vconn(struct tcpc_device *tcpc, int enable);
/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
static int wusb3812_read_device(void *client, u32 reg, int len, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;
	int ret = 0, count = 5;

	while (count) {
		if (len > 1) {
			ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
			if (ret < 0)
				count--;
			else
				return ret;
		} else {
			ret = i2c_smbus_read_byte_data(i2c, reg);
			if (ret < 0)
				count--;
			else {
				*(u8 *)dst = (u8)ret;
				return ret;
			}
		}
		udelay(100);
	}
	return ret;
}

static int wusb3812_write_device(void *client, u32 reg, int len, const void *src)
{
	const u8 *data;
	struct i2c_client *i2c = (struct i2c_client *)client;
	int ret = 0, count = 5;

	while (count) {
		if (len > 1) {
			ret = i2c_smbus_write_i2c_block_data(i2c,
							reg, len, src);
			if (ret < 0)
				count--;
			else
				return ret;
		} else {
			data = src;
			ret = i2c_smbus_write_byte_data(i2c, reg, *data);
			if (ret < 0)
				count--;
			else
				return ret;
		}
		udelay(100);
	}
	return ret;
}

static int wusb3812_reg_read(struct i2c_client *i2c, u8 reg)
{
	struct wusb3812_chip *chip = i2c_get_clientdata(i2c);
	u8 val = 0;
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->m_dev, reg, 1, &val);
#else
	ret = wusb3812_read_device(chip->client, reg, 1, &val);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0) {
		dev_err(chip->dev, "wusb3812 reg read fail\n");
		return ret;
	}
	return val;
}

static int wusb3812_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
	struct wusb3812_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->m_dev, reg, 1, &data);
#else
	ret = wusb3812_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "wusb3812 reg write fail\n");
	return ret;
}

static int wusb3812_block_read(struct i2c_client *i2c,
			u8 reg, int len, void *dst)
{
	struct wusb3812_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->m_dev, reg, len, dst);
#else
	ret = wusb3812_read_device(chip->client, reg, len, dst);
#endif /* #if IS_ENABLED(CONFIG_RT_REGMAP) */
	if (ret < 0)
		dev_err(chip->dev, "wusb3812 block read fail\n");
	return ret;
}

static int wusb3812_block_write(struct i2c_client *i2c,
			u8 reg, int len, const void *src)
{
	struct wusb3812_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->m_dev, reg, len, src);
#else
	ret = wusb3812_write_device(chip->client, reg, len, src);
#endif /* #if IS_ENABLED(CONFIG_RT_REGMAP) */
	if (ret < 0)
		dev_err(chip->dev, "wusb3812 block write fail\n");
	return ret;
}

static int32_t wusb3812_write_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t data)
{
	int ret;

	/* don't need swap */
	ret = wusb3812_block_write(client, reg_addr, 2, (uint8_t *)&data);
	return ret;
}

static int32_t wusb3812_read_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t *data)
{
	int ret;

	/* don't need swap */
	ret = wusb3812_block_read(client, reg_addr, 2, (uint8_t *)data);
	return ret;
}

static inline int wusb3812_i2c_write8(
	struct tcpc_device *tcpc, u8 reg, const u8 data)
{
	struct wusb3812_chip *chip = tcpc_get_dev_data(tcpc);

	return wusb3812_reg_write(chip->client, reg, data);
}

static inline int wusb3812_i2c_write16(
		struct tcpc_device *tcpc, u8 reg, const u16 data)
{
	struct wusb3812_chip *chip = tcpc_get_dev_data(tcpc);

	return wusb3812_write_word(chip->client, reg, data);
}

static inline int wusb3812_i2c_read8(struct tcpc_device *tcpc, u8 reg)
{
	struct wusb3812_chip *chip = tcpc_get_dev_data(tcpc);

	return wusb3812_reg_read(chip->client, reg);
}

static inline int wusb3812_i2c_read16(
	struct tcpc_device *tcpc, u8 reg)
{
	struct wusb3812_chip *chip = tcpc_get_dev_data(tcpc);
	u16 data;
	int ret;

	ret = wusb3812_read_word(chip->client, reg, &data);
	if (ret < 0)
		return ret;
	return data;
}

#ifdef CONFIG_RT_REGMAP
static struct rt_regmap_fops wusb3812_regmap_fops = {
	.read_device = wusb3812_read_device,
	.write_device = wusb3812_write_device,
};
#endif /* CONFIG_RT_REGMAP */

static int wusb3812_regmap_init(struct wusb3812_chip *chip)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_properties *props;
	char name[32];
	int len;

	props = devm_kzalloc(chip->dev, sizeof(*props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	props->register_num = WUSB3812_CHIP_REGMAP_SIZE;
	props->rm = wusb3812_chip_regmap;

	props->rt_regmap_mode = RT_MULTI_BYTE | RT_CACHE_DISABLE |
				RT_IO_PASS_THROUGH | RT_DBG_GENERAL;
	snprintf(name, sizeof(name), "wusb3812-%02x", chip->client->addr);

	len = strlen(name);
	props->name = kzalloc(len+1, GFP_KERNEL);
	props->aliases = kzalloc(len+1, GFP_KERNEL);

	if ((!props->name) || (!props->aliases))
		return -ENOMEM;

	strlcpy((char *)props->name, name, len+1);
	strlcpy((char *)props->aliases, name, len+1);
	props->io_log_en = 0;

	chip->m_dev = rt_regmap_device_register(props,
			&wusb3812_regmap_fops, chip->dev, chip->client, chip);
	if (!chip->m_dev) {
		dev_err(chip->dev, "wusb3812 chip rt_regmap register fail\n");
		return -EINVAL;
	}
#endif
	return 0;
}

static int wusb3812_regmap_deinit(struct wusb3812_chip *chip)
{
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->m_dev);
#endif
	return 0;
}

static inline int wusb3812_software_reset(struct tcpc_device *tcpc)
{
	int ret = 0;
	ret = wusb3812_i2c_write8(tcpc, WUSB3812C_REG_SWRESET, 1);

	usleep_range(1000, 2000);
	return ret;
}

static inline int wusb3812_command(struct tcpc_device *tcpc, uint8_t cmd)
{
	return wusb3812_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, cmd);
}

static int wusb3812_init_alert_mask(struct tcpc_device *tcpc)
{
	uint16_t mask;
	struct wusb3812_chip *chip = tcpc_get_dev_data(tcpc);

	mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;

#ifdef CONFIG_USB_POWER_DELIVERY
	/* Need to handle RX overflow */
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS | TCPC_V10_REG_ALERT_TX_DISCARDED
			| TCPC_V10_REG_ALERT_TX_FAILED
			| TCPC_V10_REG_ALERT_RX_HARD_RST
			| TCPC_V10_REG_ALERT_RX_STATUS
			| TCPC_V10_REG_RX_OVERFLOW;
#endif

	mask |= TCPC_REG_ALERT_FAULT | TCPC_V10_REG_VBUS_SINK_DISCONNECT
			| TCPC_V10_REG_ALERT_VENDOR_DEFINED;
	wusb3812_i2c_write8(tcpc, WUSB3812C_REG_RT_MASK, 0x2);
	WS_DBG("mask=%#x\n", mask | TCPC_REG_ALERT_EXT_VBUS_80);

	return wusb3812_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, mask);
}

static int wusb3812_init_power_status_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask = TCPC_V10_REG_POWER_STATUS_VBUS_PRES;

	return wusb3812_i2c_write8(tcpc,
			TCPC_V10_REG_POWER_STATUS_MASK, mask);
}

static int wusb3812_init_fault_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask =
		TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
		TCPC_V10_REG_FAULT_STATUS_VCONN_OC;

	return wusb3812_i2c_write8(tcpc,
			TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static void wusb3812_irq_work_handler(struct kthread_work *work)
{
	struct wusb3812_chip *chip =
			container_of(work, struct wusb3812_chip, irq_work);
	int regval = 0;
	int gpio_val;

	WS_DBG("\n");
	/* make sure I2C bus had resumed */
	down(&chip->suspend_lock);
	tcpci_lock_typec(chip->tcpc);

	do {
		regval = tcpci_alert(chip->tcpc);
		if (regval)
			break;
		gpio_val = gpio_get_value(chip->irq_gpio);
		WS_DBG("gpio[%d]_val = %d\n", chip->irq_gpio, gpio_val);
	} while (gpio_val == 0);

	tcpci_unlock_typec(chip->tcpc);


	up(&chip->suspend_lock);
}

static irqreturn_t wusb3812_intr_handler(int irq, void *data)
{
	struct wusb3812_chip *chip = data;

	__pm_wakeup_event(chip->irq_wake_lock, WUSB3812C_IRQ_WAKE_TIME);


	kthread_queue_work(&chip->irq_worker, &chip->irq_work);
	return IRQ_HANDLED;
}

static int wusb3812_init_alert(struct tcpc_device *tcpc)
{
	struct wusb3812_chip *chip = tcpc_get_dev_data(tcpc);
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret;
	char *name;
	int len;

	/* Clear Alert Mask & Status */
	wusb3812_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, 0);
	wusb3812_write_word(chip->client, TCPC_V10_REG_ALERT, 0xffff);

	len = strlen(chip->tcpc_desc->name);
	name = devm_kzalloc(chip->dev, len+5, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	ret = snprintf(name, PAGE_SIZE, "%s-IRQ", chip->tcpc_desc->name);
	if (ret < 0 || ret >= PAGE_SIZE)
		pr_info("%s-%d, snprintf fail\n", __func__, __LINE__);

	pr_info("%s name = %s, gpio = %d\n", __func__,
				chip->tcpc_desc->name, chip->irq_gpio);

	ret = devm_gpio_request(chip->dev, chip->irq_gpio, name);

	if (ret < 0) {
		pr_err("Error: failed to request GPIO%d (ret = %d)\n",
		chip->irq_gpio, ret);
		goto init_alert_err;
	}

	ret = gpio_direction_input(chip->irq_gpio);
	if (ret < 0) {
		pr_err("Error: failed to set GPIO%d as input pin(ret = %d)\n",
		chip->irq_gpio, ret);
		goto init_alert_err;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq <= 0) {
		pr_err("%s gpio to irq fail, chip->irq(%d)\n",
						__func__, chip->irq);
		goto init_alert_err;
	}

	pr_info("%s : IRQ number = %d\n", __func__, chip->irq);

	kthread_init_worker(&chip->irq_worker);
	chip->irq_worker_task = kthread_run(kthread_worker_fn,
			&chip->irq_worker, "%s", chip->tcpc_desc->name);
	if (IS_ERR(chip->irq_worker_task)) {
		pr_err("Error: Could not create tcpc task\n");
		goto init_alert_err;
	}

	sched_setscheduler(chip->irq_worker_task, SCHED_FIFO, &param);
	kthread_init_work(&chip->irq_work, wusb3812_irq_work_handler);

	WS_DBG("IRQF_NO_THREAD Test\n");
	ret = request_irq(chip->irq, wusb3812_intr_handler,
		IRQF_TRIGGER_FALLING | IRQF_NO_THREAD, name, chip);
	if (ret < 0) {
		pr_err("Error: failed to request irq%d (gpio = %d, ret = %d)\n",
			chip->irq, chip->irq_gpio, ret);
		goto init_alert_err;
	}

	enable_irq_wake(chip->irq);
	return 0;
init_alert_err:
	return -EINVAL;
}

int wusb3812_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
	int ret;
	uint16_t mask_t1;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t mask_t2;

	mask_t2 = mask >> 16;
	//mask_t2 = 0x02;
	if (mask_t2) {
		ret = wusb3812_i2c_write8(tcpc, WUSB3812C_REG_RT_INT, mask_t2);
		if (ret < 0)
			return ret;
	}
#endif

	WS_DBG("mask=%#x\n", mask);
	/* Write 1 clear */
	mask_t1 = (uint16_t) mask;
	if (mask_t1) {
		ret = wusb3812_i2c_write16(tcpc, TCPC_V10_REG_ALERT, mask_t1);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int wusb3812c_clear_rx_alert(struct tcpc_device *tcpc, bool en)
{
	int ret = 0;

#ifdef CONFIG_TCPC_CLOCK_GATING
	int i = 0;

	if (en) {
		for (i = 0; i < 2; i++)
			ret = wusb3812_alert_status_clear(tcpc,
				TCPC_REG_ALERT_RX_MASK);
	}

#endif	/* CONFIG_TCPC_CLOCK_GATING */

	return ret;
}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
int wusb3812_vsafe0v_detect(struct tcpc_device *tcpc, bool status)
{
    static int ret = 0;

    if (status) {
        ret = wusb3812_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, 0x33);
        if (ret < 0)
            return ret;
        ret = wusb3812_i2c_write8(tcpc, WUSB3812C_REG_VSAFE0_ENABLE, 0xB0);
        if (ret < 0)
            return ret;
        ret = wusb3812_i2c_write8(tcpc, WUSB3812C_REG_TTCPC_FILTER, 0x30);
        if (ret < 0)
            return ret;
        WS_DBG("line-%d alert=%#x\n", __LINE__,
				wusb3812_i2c_read16(tcpc, TCPC_V10_REG_ALERT));
		wusb3812_alert_status_clear(tcpc, 0x28000);
		usleep_range(10,11);
		WS_DBG("line-%d alert=%#x\n", __LINE__,
				wusb3812_i2c_read16(tcpc, TCPC_V10_REG_ALERT));
    } else {
        ret = wusb3812_i2c_write8(tcpc, WUSB3812C_REG_RT_INT, 0x02);
        if (ret < 0)
            return ret;
        ret = wusb3812_i2c_write8(tcpc, WUSB3812C_REG_TTCPC_FILTER, 0);
        if (ret < 0) {
            WS_DBG("exit vbus_safe0v 0xA1 fail,ret = %d\n", ret);
        }
        ret = wusb3812_i2c_write8(tcpc, WUSB3812C_REG_VSAFE0_ENABLE, 0);
        if (ret < 0) {
            WS_DBG("exit vbus_safe0v 0xB0 fail,ret = %d\n", ret);
        }
    }

    return ret;
}
static bool is_0v8 = true;
static int wusb3812_is_vsafe0v(struct tcpc_device *tcpc)
{
    WS_DBG("is_0v8=%#x\n", is_0v8);

    return is_0v8;
}
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

static int wusb3812_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	bool retry_discard_old = false;
	struct wusb3812_chip *chip = tcpc_get_dev_data(tcpc);

	WS_DBG("sw_reset = %d\n", sw_reset);

	if (sw_reset) {
		ret = wusb3812_software_reset(tcpc);
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_TCPC_I2CRST_EN
	wusb3812_i2c_write8(tcpc,
		WUSB3812C_REG_I2CRST_CTRL,
		WUSB3812C_REG_I2CRST_SET(true, 0x01));
#endif	/* CONFIG_TCPC_I2CRST_EN */

    mdelay(2);
	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	wusb3812_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL,
		TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));
	if (chip->chip_id == WUSB3812C_DID_A) {
		wusb3812_i2c_write8(tcpc, TCPC_V10_REG_FAULT_CTRL,
			TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV);
	}
	/*
	 * CC Detect Debounce : 26.7*val us
	 * Transition window count : spec 12~20us, based on 2.4MHz
	 * DRP Toggle Cycle : 51.2 + 6.4*val ms
	 * DRP Duyt Ctrl : dcSRC: /1024
	 */
	wusb3812_i2c_write8(tcpc, WUSB3812C_REG_TTCPC_FILTER, 10);
	wusb3812_i2c_write8(tcpc, WUSB3812C_REG_DRP_TOGGLE_CYCLE, 4);
	wusb3812_i2c_write16(tcpc,
		WUSB3812C_REG_DRP_DUTY_CTRL, TCPC_NORMAL_RP_DUTY);

	/* Vconn OC */
	wusb3812_i2c_write8(tcpc, WUSB3812C_REG_VCONN_CLIMITEN, 0x99);
	if (!(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD))
		retry_discard_old = true;

	tcpci_alert_status_clear(tcpc, 0xffffffff);
	wusb3812_init_power_status_mask(tcpc);
	wusb3812_init_alert_mask(tcpc);
	wusb3812_init_fault_mask(tcpc);
	mdelay(1);

	return 0;
}

static inline int wusb3812_fault_status_vconn_ov(struct tcpc_device *tcpc)
{
	int ret;

	ret = wusb3812_i2c_read8(tcpc, WUSB3812C_REG_VCONN_CLIMITEN);
	if (ret < 0)
		return ret;

	ret &= ~WUSB3812C_REG_DISCHARGE_EN;
	return wusb3812_i2c_write8(tcpc, WUSB3812C_REG_BMC_CTRL, ret);
}

int wusb3812_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
	int ret;

	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OV)
		ret = wusb3812_fault_status_vconn_ov(tcpc);

	wusb3812_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
	WS_DBG("\n");
	return 0;
}

int wusb3812_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
	int ret;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t v2;
#endif

	ret = wusb3812_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK);
	if (ret < 0)
		return ret;

	*mask = (uint16_t) ret;
	WS_DBG("Reg[13:12]=%#x\n", ret);
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = wusb3812_i2c_read8(tcpc, WUSB3812C_REG_RT_MASK);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*mask |= v2 << 16;
	WS_DBG("Reg[99]=%#x\n", ret);
#endif

	return 0;
}

/*int wusb3812_set_alert_mask(struct tcpc_device *tcpc, uint32_t mask)
{
	WS_DBG("mask = %#x\n", mask);

	mask &= 0x20E7F;
	return wusb3812_i2c_write16(tcpc, TCPC_V10_REG_ALERT_MASK, mask);
}*/

int wusb3812_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
    static bool flg_0v8_en = false;
    static int pwr_stt = 0, pwr_stt_old = 0;
    int ret;
    int vendor_irq = 0;

    ret = wusb3812_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
    if (ret < 0)
        return ret;

    *alert = (uint16_t) ret;
    if (ret & TCPC_V10_REG_ALERT_VENDOR_DEFINED){
		vendor_irq = wusb3812_i2c_read8(tcpc, WUSB3812C_REG_RT_STATUS);
		if (vendor_irq & 0x02) {
			vendor_irq &= wusb3812_i2c_read8(tcpc, WUSB3812C_REG_RT_INT);
		}
    }
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
    if (ret & TCPC_REG_ALERT_POWER_STATUS){
        pwr_stt = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
		pwr_stt &= TCPC_V10_REG_POWER_STATUS_VBUS_PRES;
        if (pwr_stt < 0){
            return ret;
        } else if (pwr_stt!=pwr_stt_old){
            if (!(pwr_stt & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
                && (!flg_0v8_en)) {
                flg_0v8_en = true;
                WS_DBG("flg_0v8_en=%#x\n", flg_0v8_en);
                ret = wusb3812_vsafe0v_detect(tcpc, true);
            } else if (pwr_stt & TCPC_V10_REG_POWER_STATUS_VBUS_PRES){
                is_0v8 = false;
            }
            pwr_stt_old = pwr_stt;
        }
    }
    if (flg_0v8_en && (vendor_irq & 0x02)) {
        ret = wusb3812_vsafe0v_detect(tcpc, false);
        *alert |= vendor_irq << 16;	//read 98h
        flg_0v8_en = false;
        is_0v8 = true;
        /* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
        WS_DBG("is_0v8=%d flg_0v8_en=%d\n", is_0v8, flg_0v8_en);
        /* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
    }
#endif

    pr_err("%s alert=%#x\n", __func__, *alert);
    WS_DBG("vendor_irq=%#x\n", vendor_irq);


	if ((tcpc->pd_transmit_state == PD_TX_STATE_WAIT_HARD_RESET) &&
		(*alert == TCPC_REG_ALERT_TX_SUCCESS)) {
			*alert = TCPC_REG_ALERT_HRESET_SUCCESS;
	}

	return 0;
}

static int wusb3812_get_power_status(
        struct tcpc_device *tcpc, uint16_t *pwr_status)
{
    int ret;

    ret = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
    if (ret < 0)
        return ret;
    WS_DBG("Reg[1E]=%#x\n", ret);
    *pwr_status = 0;

    if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
        *pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
    else {
        ret = tcpci_is_vsafe0v(tcpc);
        if (ret < 0)
            return ret;
        tcpc->vbus_safe0v = ret;
        if (ret)
            *pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;
    }
#endif

    return 0;
}

int wusb3812_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{
	int ret;

	ret = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_FAULT_STATUS);
	if (ret < 0)
		return ret;
	*status = (uint8_t) ret;
	WS_DBG("Reg[1F] = %#x\n", ret);

	return 0;
}

static int wusb3812_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	int status, role_ctrl, cc_role;
	bool act_as_sink, act_as_drp;

	status = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS);
	if (status < 0)
		return status;

	role_ctrl = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
	if (role_ctrl < 0)
		return role_ctrl;
	WS_DBG("status=%#x role_ctrl=%#x\n", status, role_ctrl);
/*
	 if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	} 	*/

	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
	if (!(*cc1 + *cc2))
		wusb3812_set_vconn(tcpc, false);
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
	act_as_drp = TCPC_V10_REG_ROLE_CTRL_DRP & role_ctrl;

	if (act_as_drp) {
		act_as_sink = TCPC_V10_REG_CC_STATUS_DRP_RESULT(status);
	} else {
		/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
		if (tcpc->typec_polarity)
			cc_role = TCPC_V10_REG_CC_STATUS_CC2(role_ctrl);
		else
			cc_role = TCPC_V10_REG_CC_STATUS_CC1(role_ctrl);
		/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
		if (cc_role == TYPEC_CC_RP)
			act_as_sink = false;
		else
			act_as_sink = true;
	}

	/*
	 * If status is not open, then OR in termination to convert to
	 * enum tcpc_cc_voltage_status.
	 */

	if (*cc1 != TYPEC_CC_VOLT_OPEN)
		*cc1 |= (act_as_sink << 2);

	if (*cc2 != TYPEC_CC_VOLT_OPEN)
		*cc2 |= (act_as_sink << 2);

	return 0;
}

static int wusb3812_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret, cc1, cc2, role_ctrl, tcpc_ctrl;
	bool is_role_rp;
	uint8_t data, pwr_ctrl, data3;
    uint16_t alert_mask, unmask_power_cc;
	uint16_t OTGFlag = 0;
    struct wusb3812_chip *chip = tcpc_get_dev_data(tcpc);
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2;

	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
	if (tcpc->typec_is_attached_src &&
		((tcpc->typec_remote_cc[0] + tcpc->typec_remote_cc[1]) == (TYPEC_CC_VOLT_RA + TYPEC_CC_VOLT_RD)))
		wusb3812_set_vconn(tcpc, true);
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */

	pull = TYPEC_CC_PULL_GET_RES(pull);
	role_ctrl = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);

	WS_DBG("pull = %#x\n", pull);
	if (pull == TYPEC_CC_DRP) {
		/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
		data3 = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
		data3 &= ~TCPC_V10_REG_TCPC_CTRL_EN_LOOK4CONNECTION_ALERT;
		wusb3812_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data3);
		/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(
				1, rp_lvl, TYPEC_CC_RD, TYPEC_CC_RD);

		ret = wusb3812_i2c_write8(
			tcpc, TCPC_V10_REG_ROLE_CTRL, data);

		data3 = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
		data3 = data3 & 0xEF;
		wusb3812_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data3);
		/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
		WS_DBG("1_Reg[1A]=%#x Reg[1C]=%#x\n", data, data3);
		/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
		if (ret == 0) {
			ret = wusb3812_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
		}
	} else {
		/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
		if (pull == TYPEC_CC_OPEN) {
			wusb3812_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL,
				TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT);
		}
		/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
		pull1 = pull2 = pull;
		if ((pull == TYPEC_CC_RP_DFT || pull == TYPEC_CC_RP_1_5 ||
			pull == TYPEC_CC_RP_3_0) &&
			tcpc->typec_is_attached_src) {
			if (tcpc->typec_polarity)
				pull1 = TYPEC_CC_OPEN;
			else
				pull2 = TYPEC_CC_OPEN;
		}
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull1, pull2);

		/*
		Verify OTG mode:
		Rp bit2==0 CC1[1:0]==Rd & CC2[1:0]==Open || CC1[1:0]==open && CC2[1:0]==Rd  or
		Rp bit2==0 CC1[1:0]==Rd & CC2[1:0]==Ra || CC1[1:0]==Ra && CC2[1:0]==Rd
		*/
        WS_DBG("CC1=%#x,CC2=%#x\n",
				tcpc->typec_remote_cc[0], tcpc->typec_remote_cc[1]);

		cc1 = tcpc->typec_remote_cc[0];
		is_role_rp = (cc1 & 0x4) ? false : true;
		cc1 = cc1 & 0x3;
		cc2 = tcpc->typec_remote_cc[1] & 0x3;

		if (is_role_rp) {
			if (((cc1 == TYPEC_CC_VOLT_RD) && (cc2 == TYPEC_CC_VOLT_OPEN)) ||
				((cc2 == TYPEC_CC_VOLT_RD) && (cc1 == TYPEC_CC_VOLT_OPEN))) {
				OTGFlag = 1;
			} else if (((cc1 == TYPEC_CC_VOLT_RD) && (cc2 == TYPEC_CC_VOLT_RA)) ||
				((cc2 == TYPEC_CC_VOLT_RA) && (cc1 == TYPEC_CC_VOLT_RD))) {
				OTGFlag = 1;
			}
		}
		//OTGFlag
		WS_DBG("OTGFlag=%#x\n", OTGFlag);
        if (OTGFlag && ((data & WUSB3812C_REG_ROLE_NRP) == WUSB3812C_REG_ROLE_NRP
			|| (data & WUSB3812C_REG_ROLE_PRP) == WUSB3812C_REG_ROLE_PRP)) {
			//mask cc status interupt for src,and cc status+power status for snk
			wusb3812_read_word(chip->client, TCPC_V10_REG_ALERT_MASK, &alert_mask);
			unmask_power_cc = alert_mask & 0xfffc;
			wusb3812_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, unmask_power_cc);
			//set plug orientation
			tcpc_ctrl = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
			if((data & WUSB3812C_REG_ROLE_NRP) == WUSB3812C_REG_ROLE_NRP) {
				//set CC1 priority
				tcpc_ctrl |= 0x01;
			} else {  //set CC2 priority
				tcpc_ctrl &= 0xfe;
			}
			/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
			tcpc_ctrl |= TCPC_V10_REG_TCPC_CTRL_EN_LOOK4CONNECTION_ALERT;
			wusb3812_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, tcpc_ctrl);
			WS_DBG("L1 Reg[19]=%#x\n", tcpc_ctrl);
			/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
        } else if (((data & WUSB3812C_REG_ROLE_NRP) == WUSB3812C_REG_ROLE_NRP
				|| (data & WUSB3812C_REG_ROLE_PRP) == WUSB3812C_REG_ROLE_PRP)||
				((data & 0x0b) == 0xb || (data & 0x0e) == 0xe)){
			//mask cc status interupt for src,and cc status+power status for snk
			wusb3812_read_word(chip->client, TCPC_V10_REG_ALERT_MASK, &alert_mask);
			unmask_power_cc = alert_mask & 0xfffc;
			wusb3812_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, unmask_power_cc);
			//set plug orientation
			tcpc_ctrl = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
			if((data & WUSB3812C_REG_ROLE_NRP) == WUSB3812C_REG_ROLE_NRP
				|| (data & 0x0b) == 0xb){
				//priority cc1
				tcpc_ctrl |= 0x01;
			} else {
				//priority cc2
				tcpc_ctrl &= 0xfe;
			}
			/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
			tcpc_ctrl |= TCPC_V10_REG_TCPC_CTRL_EN_LOOK4CONNECTION_ALERT;
			wusb3812_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, tcpc_ctrl);
			WS_DBG("L2 Reg[19]=%#x\n", tcpc_ctrl);
			/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
		}

        ret = wusb3812_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
		WS_DBG("2_Reg[1A]=%#x\n", data);
		if(data==0x4A ||data==0x5A){

			WS_DBG("3_Reg[1A]=%#x\n", data);
			data3 = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
			data3 = data3 & 0xEF;
			wusb3812_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data3);
			wusb3812_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, 0x99);
		}

        if (OTGFlag && ((data & WUSB3812C_REG_ROLE_NRP) == WUSB3812C_REG_ROLE_NRP
			|| (data & WUSB3812C_REG_ROLE_PRP) == WUSB3812C_REG_ROLE_PRP)) {
           //set power:b4=1 auto discharge disconnect
           pwr_ctrl = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
           pwr_ctrl |= TCPC_V10_REG_AUTO_DISCHG_DISCNT;
           wusb3812_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, pwr_ctrl);
           //udelay(100);
           usleep_range(100, 150);
           //clear cc alert for src, and cc alert+power alert for snk
           wusb3812_write_word(chip->client, TCPC_V10_REG_ALERT,
		   						TCPC_V10_REG_ALERT_CC_STATUS);
           //enable cc status interrupt
           wusb3812_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, alert_mask);
        } else if ( ((data & WUSB3812C_REG_ROLE_NRP) == WUSB3812C_REG_ROLE_NRP
			|| (data & WUSB3812C_REG_ROLE_PRP) == WUSB3812C_REG_ROLE_PRP)||
			((data & 0x0b) == 0xb || (data & 0x0e) == 0xe)){
           //set power:b4=1 auto discharge disconnect
           pwr_ctrl = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
           pwr_ctrl |= TCPC_V10_REG_AUTO_DISCHG_DISCNT;
           wusb3812_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, pwr_ctrl);
           //udelay(100);
           usleep_range(100, 150);
           //clear cc alert for src, and cc alert+power alert for snk
           wusb3812_write_word(chip->client, TCPC_V10_REG_ALERT,
		   						TCPC_V10_REG_ALERT_CC_STATUS);
           //enable cc status interrupt
           wusb3812_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, alert_mask);
		}
	}
	WS_DBG("Reg[1A]=%#x\n", data);
	return 0;
}
// 1956
static int wusb3812_set_polarity(struct tcpc_device *tcpc, int polarity)
{

	int data;

	data = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;
	data &= ~TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT;
	data |= polarity ? TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT : 0;
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
	WS_DBG("Reg[19]=%#x\n", data);
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
	return wusb3812_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}

static int wusb3812_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
	WS_DBG("\n");
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
	return 0;
}

static int wusb3812_set_vconn(struct tcpc_device *tcpc, int enable)
{

	int rv;
	int data;
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
	WS_DBG("enable=%d\n",  enable);
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
	data = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
	if ((tcpc->typec_remote_cc[0] + tcpc->typec_remote_cc[1]) ==
		(TYPEC_CC_VOLT_RA + TYPEC_CC_VOLT_RD))
		data |= enable ? TCPC_V10_REG_POWER_CTRL_VCONN : 0;
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
	rv = wusb3812_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data);
	if (rv < 0)
		return rv;
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
	return 0;
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
}

#ifdef CONFIG_TCPC_LOW_POWER_MODE
static int wusb3812_is_low_power_mode(struct tcpc_device *tcpc)
{
	return 0;
	// int rv = wusb3812_i2c_read8(tcpc, WUSB3812C_REG_BMC_CTRL);

	// if (rv < 0)
	// 	return rv;

	// return (rv & WUSB3812C_REG_BMCIO_LPEN) != 0;
}

static int wusb3812_set_low_power_mode(
		struct tcpc_device *tcpc, bool en, int pull)
{
	return 0;
// 	int ret = 0;
// 	uint8_t data;

//     WS_DBG("(%s)\n",  WUSB3812C_DRV_VERSION);
// 	ret = wusb3812_i2c_write8(tcpc, WUSB3812C_REG_IDLE_CTRL,
// 		WUSB3812C_REG_IDLE_SET(0, 1, en ? 0 : 1, 0));
// 	if (ret < 0)
// 		return ret;
// 	if (en) {
// 		data = WUSB3812C_REG_BMCIO_LPEN;

// 		if (pull & TYPEC_CC_RP)
// 			data |= WUSB3812C_REG_BMCIO_LPRPRD;

// #ifdef CONFIG_TYPEC_CAP_NORP_SRC
// 		data |= WUSB3812C_REG_BMCIO_BG_EN | WUSB3812C_REG_VBUS_DET_EN;
// #endif
// 	} else {
// 		data = WUSB3812C_REG_BMCIO_BG_EN |
// 			WUSB3812C_REG_VBUS_DET_EN | WUSB3812C_REG_BMCIO_OSC_EN;
// 	}

// 	return wusb3812_i2c_write8(tcpc, WUSB3812C_REG_BMC_CTRL, data);
}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */
// 1956
#ifdef CONFIG_TCPC_WATCHDOG_EN
int wusb3812c_set_watchdog(struct tcpc_device *tcpc, bool en)
{
	uint8_t data = WUSB3812C_REG_WATCHDOG_CTRL_SET(en, 7);

	return wusb3812_i2c_write8(tcpc,
		WUSB3812C_REG_WATCHDOG_CTRL, data);
}
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#ifdef CONFIG_TCPC_INTRST_EN
int wusb3812c_set_intrst(struct tcpc_device *tcpc, bool en)
{
	return wusb3812_i2c_write8(tcpc,
		WUSB3812C_REG_INTRST_CTRL, WUSB3812C_REG_INTRST_SET(en, 3));
}
#endif	/* CONFIG_TCPC_INTRST_EN */

static int wusb3812_tcpc_deinit(struct tcpc_device *tcpc)
{

#ifdef CONFIG_TCPC_SHUTDOWN_CC_DETACH
	wusb3812_set_cc(tcpc, TYPEC_CC_DRP);
	wusb3812_set_cc(tcpc, TYPEC_CC_OPEN);

	wusb3812_i2c_write8(tcpc,
		WUSB3812C_REG_I2CRST_CTRL,
		WUSB3812C_REG_I2CRST_SET(true, 1));
#else
	wusb3812_i2c_write8(tcpc, WUSB3812C_REG_SWRESET, 1);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */

	WS_DBG("\n");

	return 0;
}

static int wusb3812_alert_vendor_defined_handler(struct tcpc_device *tcpc)
{
	return 0;
}

/*static int wusb3812_set_auto_dischg_discnt(struct tcpc_device *tcpc, bool en)
{
	u8 val = 0;
	int ret = 0;

	WS_DBG("en=%d\n", en);
	ret = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
	val = en ? (ret | TCPC_V10_REG_AUTO_DISCHG_DISCNT)
		    : (ret & (~TCPC_V10_REG_AUTO_DISCHG_DISCNT));

	return wusb3812_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, val);
}*/

#ifdef CONFIG_USB_POWER_DELIVERY
static int wusb3812_set_msg_header(
	struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
	uint8_t msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(
		data_role, power_role);
	WS_DBG("\n");
	return wusb3812_i2c_write8(
		tcpc, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int wusb3812_protocol_reset(struct tcpc_device *tcpc)
{
	WS_DBG("\n");
	wusb3812_i2c_write8(tcpc, WUSB3812C_REG_I2CRST_CTRL, 0x02);
	mdelay(1);
	wusb3812_init_alert_mask(tcpc);

	return 0;
}

static int wusb3812_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
	int ret = 0;

	WS_DBG("\n");
	ret = wusb3812_i2c_write8(tcpc, TCPC_V10_REG_RX_DETECT, enable);

	if ((ret == 0) && (!enable)) {
		wusb3812_protocol_reset(tcpc);
		ret = wusb3812c_clear_rx_alert(tcpc, true);
	}

	return ret;
}

static int wusb3812_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	struct wusb3812_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	uint8_t type, cnt = 0;
	uint8_t buf[4];
	const uint16_t alert_rx =
		TCPC_V10_REG_ALERT_RX_STATUS|TCPC_V10_REG_RX_OVERFLOW;

	rv = wusb3812_block_read(chip->client,
			TCPC_V10_REG_RX_BYTE_CNT, 4, buf);
	cnt = buf[0];
	type = buf[1];
	*msg_head = *(uint16_t *) & buf[2];
	WS_DBG("MessageType=%d\n", PD_HEADER_TYPE(*msg_head));
	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (rv >= 0 && cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		rv = wusb3812_block_read(chip->client, TCPC_V10_REG_RX_DATA, cnt,
				(uint8_t *) payload);
	}

	*frame_type = (enum tcpm_transmit_type) type;

	/* Read complete, clear RX status alert bit */
	tcpci_alert_status_clear(tcpc, alert_rx);

	/*mdelay(1); */
	return rv;
}

static int wusb3812_set_bist_carrier_mode(
	struct tcpc_device *tcpc, uint8_t pattern)
{
	WS_DBG("\n");
	/* Don't support this function */
	return 0;
}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
static int wusb3812_retransmit(struct tcpc_device *tcpc)
{
	WS_DBG("\n");

	return wusb3812_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, TCPC_TX_SOP));
}
#endif

#pragma pack(push, 1)
struct tcpc_transmit_packet {
	uint8_t cnt;
	uint16_t msg_header;
	uint8_t data[sizeof(uint32_t)*7];
};
#pragma pack(pop)

static int wusb3812_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	struct wusb3812_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	int data_cnt;
	struct tcpc_transmit_packet packet;

	WS_DBG("\n");

	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = sizeof(uint32_t) * PD_HEADER_CNT(header);

		packet.cnt = data_cnt + sizeof(uint16_t);
		packet.msg_header = header;

		if (data_cnt > 0)
			memcpy(packet.data, (uint8_t *) data, data_cnt);

		rv = wusb3812_block_write(chip->client,
				TCPC_V10_REG_TX_BYTE_CNT,
				packet.cnt+1, (uint8_t *) &packet);
		if (rv < 0)
			return rv;
	}

	rv = wusb3812_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, type));
	return rv;
}

static int wusb3812_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	int data;

	WS_DBG("en=%d\n", en);

	data = wusb3812_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
	data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 start */
	WS_DBG("Reg[19]=%#x\n", data);
	/* A06 code for AL7160A-3018 by xiongxiaoliang at 202400621 end */
	return wusb3812_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static struct tcpc_ops wusb3812_tcpc_ops = {
	.init = wusb3812_tcpc_init,
	.alert_status_clear = wusb3812_alert_status_clear,
	.fault_status_clear = wusb3812_fault_status_clear,
	.get_alert_mask = wusb3812_get_alert_mask,
	//.set_alert_mask = wusb3812_set_alert_mask,
	.get_alert_status = wusb3812_get_alert_status,
	.get_power_status = wusb3812_get_power_status,
	.get_fault_status = wusb3812_get_fault_status,
	.get_cc = wusb3812_get_cc,
	.set_cc = wusb3812_set_cc,
	.set_polarity = wusb3812_set_polarity,
	.set_low_rp_duty = wusb3812_set_low_rp_duty,
	.set_vconn = wusb3812_set_vconn,
	.deinit = wusb3812_tcpc_deinit,
	.alert_vendor_defined_handler = wusb3812_alert_vendor_defined_handler,
	//.set_auto_dischg_discnt = wusb3812_set_auto_dischg_discnt,

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	.is_vsafe0v = wusb3812_is_vsafe0v,
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	.is_low_power_mode = wusb3812_is_low_power_mode,
	.set_low_power_mode = wusb3812_set_low_power_mode,
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN
	.set_watchdog = wusb3812c_set_watchdog,
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#ifdef CONFIG_TCPC_INTRST_EN
	.set_intrst = wusb3812c_set_intrst,
#endif	/* CONFIG_TCPC_INTRST_EN */

#ifdef CONFIG_USB_POWER_DELIVERY
	.set_msg_header = wusb3812_set_msg_header,
	.set_rx_enable = wusb3812_set_rx_enable,
	.protocol_reset = wusb3812_protocol_reset,
	.get_message = wusb3812_get_message,
	.transmit = wusb3812_transmit,
	.set_bist_test_mode = wusb3812_set_bist_test_mode,
	.set_bist_carrier_mode = wusb3812_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = wusb3812_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
};

static int ws_parse_dt(struct wusb3812_chip *chip, struct device *dev)
{
	struct device_node *np = NULL;
	int ret = 0;

	WS_DBG("\n");

	np = of_find_node_by_name(NULL, "type_c_port0");
	if (!np) {
		pr_notice("%s find node wusb3812 type_c_port0 fail\n", __func__);
		return -ENODEV;
	}
	dev->of_node = np;
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "wusb3812pd,intr_gpio", 0);
	if (ret < 0)
		pr_err("%s no intr_gpio info\n", __func__);
	chip->irq_gpio = ret;
	WS_DBG("%d intr-pins=%d\n", __LINE__, chip->irq_gpio);
#else
	ret = of_property_read_u32(
		np, "wusb3812pd,intr_gpio_num", &chip->irq_gpio);
	if (ret < 0)
		pr_err("%s no intr_gpio info\n", __func__);
	WS_DBG("%d intr-pins=%d\n", __LINE__, chip->irq_gpio);
#endif

	return ret < 0 ? ret : 0;
}

/*
 * In some platform WS_DBG may spend too much time on printing debug message.
 * So we use this function to test the printk performance.
 * If your platform cannot not pass this check function, please config
 * PD_DBG_INFO, this will provide the threaded debug message for you.
 */
#if TCPC_ENABLE_ANYMSG
static void check_printk_performance(void)
{
	int i;
	u64 t1, t2;
	u32 nsrem;

#ifdef CONFIG_PD_DBG_INFO
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pd_dbg_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pd_dbg_info("pd_dbg_info : t2-t1 = %lu\n",
				(unsigned long)nsrem / 1000);
	}
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		WS_DBG("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		WS_DBG("pr_info :t2-t1 = %lu\n",
				(unsigned long)nsrem / 1000);
	}
#else
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		WS_DBG("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		WS_DBG("t2-t1 = %lu\n",
				(unsigned long)nsrem /  1000);
		PD_BUG_ON(nsrem > 100*1000);
	}
#endif /* CONFIG_PD_DBG_INFO */
}
#endif /* TCPC_ENABLE_ANYMSG */

static int wusb3812_tcpcdev_init(struct wusb3812_chip *chip, struct device *dev)
{
	struct tcpc_desc *desc;
	struct device_node *np = dev->of_node;
	u32 val, len;
	const char *name = "default";

	dev_info(dev, "%s\n", __func__);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	if (of_property_read_u32(np, "ws-tcpc,role_def", &val) >= 0) {
		if (val >= TYPEC_ROLE_NR)
			desc->role_def = TYPEC_ROLE_DRP;
		else
			desc->role_def = val;
	} else {
		dev_info(dev, "use default Role DRP\n");
		desc->role_def = TYPEC_ROLE_DRP;
	}

	if (of_property_read_u32(
		np, "ws-tcpc,notifier_supply_num", &val) >= 0) {
		if (val < 0)
			desc->notifier_supply_num = 0;
		else
			desc->notifier_supply_num = val;
	} else
		desc->notifier_supply_num = 0;

	if (of_property_read_u32(np, "ws-tcpc,rp_level", &val) >= 0) {
		switch (val) {
		case 0: /* RP Default */
			desc->rp_lvl = TYPEC_CC_RP_DFT;
			break;
		case 1: /* RP 1.5V */
			desc->rp_lvl = TYPEC_CC_RP_1_5;
			break;
		case 2: /* RP 3.0V */
			desc->rp_lvl = TYPEC_CC_RP_3_0;
			break;
		default:
			break;
		}
	}

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	if (of_property_read_u32(np, "ws-tcpc,vconn_supply", &val) >= 0) {
		if (val >= TCPC_VCONN_SUPPLY_NR)
			desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
		else
			desc->vconn_supply = val;
	} else {
		dev_info(dev, "use default VconnSupply\n");
		desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	if (of_property_read_string(np, "ws-tcpc,name",
				(char const **)&name) < 0) {
		dev_info(dev, "use default name\n");
	}

	len = strlen(name);
	desc->name = kzalloc(len+1, GFP_KERNEL);
	if (!desc->name)
		return -ENOMEM;

	strlcpy((char *)desc->name, name, len+1);

	chip->tcpc_desc = desc;

	chip->tcpc = tcpc_device_register(dev,
			desc, &wusb3812_tcpc_ops, chip);
	if (IS_ERR(chip->tcpc))
		return -EINVAL;

	chip->tcpc->tcpc_flags = TCPC_FLAGS_LPM_WAKEUP_WATCHDOG |
			TCPC_FLAGS_VCONN_SAFE5V_ONLY;

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	if (chip->chip_id > WUSB3812C_DID_D)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif  /* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_USB_PD_REV30
	if (chip->chip_id >= WUSB3812C_DID_D)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;

	if (chip->tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
		dev_info(dev, "PD_REV30\n");
	else
		dev_info(dev, "PD_REV20\n");
#endif	/* CONFIG_USB_PD_REV30 */

	return 0;
}

static inline int wusb3812c_check_revision(struct i2c_client *client)
{
	u16 vid, pid, did;
	int ret;
	u8 data = 1;

	ret = wusb3812_read_device(client, TCPC_V10_REG_VID, 2, &vid);
	if (ret < 0) {
		dev_err(&client->dev, "read chip ID fail\n");
		return -EIO;
	}

	if (vid != WUSB3812_VID) {
		WS_DBG("failed, VID=%#x\n", vid);
		return -ENODEV;
	}

	ret = wusb3812_read_device(client, TCPC_V10_REG_PID, 2, &pid);
	if (ret < 0) {
		dev_err(&client->dev, "read product ID fail\n");
		return -EIO;
	}

	if (pid != WUSB3812_PID) {
		WS_DBG("failed, PID=%#x\n", pid);
		return -ENODEV;
	}

	ret = wusb3812_write_device(client, WUSB3812C_REG_SWRESET, 1, &data);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);

	ret = wusb3812_read_device(client, TCPC_V10_REG_DID, 2, &did);
	if (ret < 0) {
		dev_err(&client->dev, "read device ID fail\n");
		return -EIO;
	}

	return did;
}

/* A06 code for SR-AL7160A-01-272 by zhangziyi at 20240327 start */
static int typec_port_get_property(struct power_supply *psy, enum power_supply_property psp,
                union power_supply_propval *val)
{
	int ret = 0;
	int cc1 = 0;
	int cc2 = 0;
	struct wusb3812_chip *chip = container_of(psy->desc, struct wusb3812_chip, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		ret = wusb3812_get_cc(chip->tcpc, &cc1, &cc2);
		if ((cc1 == TYPEC_CC_VOLT_OPEN && cc2 == TYPEC_CC_VOLT_OPEN) ||
			(cc1 == TYPEC_CC_DRP_TOGGLING && cc2 == TYPEC_CC_DRP_TOGGLING)) {
			val->intval = TYPEC_STATUS_UNKNOWN;
		} else if (cc1 > TYPEC_CC_VOLT_OPEN && cc1 < TYPEC_CC_DRP_TOGGLING) {
			val->intval = TYPEC_STATUS_CC1;
		} else if (cc2 > TYPEC_CC_VOLT_OPEN && cc2 < TYPEC_CC_DRP_TOGGLING) {
			val->intval = TYPEC_STATUS_CC2;
		} else {
			val->intval = TYPEC_STATUS_UNKNOWN;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property typec_port_properties[] = {
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
};
/* A06 code for SR-AL7160A-01-272 by zhangziyi at 20240327 end */

static int wusb3812_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct wusb3812_chip *chip;
	int ret = 0, chip_id;
	bool use_dt = client->dev.of_node;

	WS_DBG("(%s)\n",  WUSB3812C_DRV_VERSION);
	if (i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA))
		WS_DBG("I2C functionality : OK...\n");
	else
		WS_DBG("I2C functionality check : failuare...\n");

	chip_id = wusb3812c_check_revision(client);
	if (chip_id < 0)
		return chip_id;
#if TCPC_ENABLE_ANYMSG
	check_printk_performance();
#endif /* TCPC_ENABLE_ANYMSG */
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->dev = &client->dev;
	chip->client = client;
	if (use_dt) {
		ret = ws_parse_dt(chip, chip->dev);
		if (ret < 0)
			return ret;
	} else {
		dev_err(&client->dev, "no dts node\n");
		return -ENODEV;
	}
	sema_init(&chip->io_lock, 1);
	sema_init(&chip->suspend_lock, 1);
	i2c_set_clientdata(client, chip);
	chip->irq_wake_lock =
		wakeup_source_register(chip->dev, "wusb3812c_irq_wake_lock");

	chip->chip_id = chip_id;
	WS_DBG("wusb3812c_chipID = %#x\n", chip_id);

	ret = wusb3812_regmap_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "wusb3812 regmap init fail\n");
		goto err_regmap_init;
	}
	ret = wusb3812_tcpcdev_init(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "wusb3812 tcpc dev init fail\n");
		goto err_tcpc_reg;
	}

	ret = wusb3812_init_alert(chip->tcpc);
	if (ret < 0) {
		pr_err("wusb3812 init alert fail\n");
		goto err_irq_init;
	}

	tcpc_schedule_init_work(chip->tcpc);
	/* A06 code for SR-AL7160A-01-272 by zhangziyi at 20240327 start */
	chip->psd.name = "typec_port";
	chip->psd.properties = typec_port_properties;
	chip->psd.type = POWER_SUPPLY_TYPE_USB_TYPE_C;
	chip->psd.num_properties = ARRAY_SIZE(typec_port_properties);
	chip->psd.get_property = typec_port_get_property;
	chip->typec_psy = devm_power_supply_register(chip->dev, &chip->psd, NULL);
	if (IS_ERR(chip->typec_psy)) {
		pr_err("wusb3812c failed to register power supply: %ld\n",
		PTR_ERR(chip->typec_psy));
	}
	/* A06 code for SR-AL7160A-01-272 by zhangziyi at 20240327 end */
	tcpc_info = WUSB3812C;
	WS_DBG("probe OK!\n");
	return 0;

err_irq_init:
err_tcpc_reg:
	tcpc_device_unregister(chip->dev, chip->tcpc);
err_regmap_init:
	wusb3812_regmap_deinit(chip);
	wakeup_source_unregister(chip->irq_wake_lock);
	return ret;
}

static int wusb3812_i2c_remove(struct i2c_client *client)
{
	struct wusb3812_chip *chip = i2c_get_clientdata(client);

	if (chip) {
		tcpc_device_unregister(chip->dev, chip->tcpc);
		wusb3812_regmap_deinit(chip);
	}

	return 0;
}

#ifdef CONFIG_PM
static int wusb3812_i2c_suspend(struct device *dev)
{
	struct wusb3812_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			down(&chip->suspend_lock);
	}

	return 0;
}

static int wusb3812_i2c_resume(struct device *dev)
{
	struct wusb3812_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			up(&chip->suspend_lock);
	}

	return 0;
}

static void wusb3812_shutdown(struct i2c_client *client)
{
	struct wusb3812_chip *chip = i2c_get_clientdata(client);

	/* Please reset IC here */
	if (chip != NULL) {
		if (chip->irq)
			disable_irq(chip->irq);
		tcpm_shutdown(chip->tcpc);
	} else {
		i2c_smbus_write_byte_data(
			client, WUSB3812C_REG_SWRESET, 0x01);
	}
}

#ifdef CONFIG_PM_RUNTIME
static int wusb3812_pm_suspend_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: suspending...\n");
	return 0;
}

static int wusb3812_pm_resume_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: resuming...\n");
	return 0;
}
#endif /* CONFIG_PM_RUNTIME */

static const struct dev_pm_ops wusb3812_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			wusb3812_i2c_suspend,
			wusb3812_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(
		wusb3812_pm_suspend_runtime,
		wusb3812_pm_resume_runtime,
		NULL
	)
#endif /* CONFIG_PM_RUNTIME */
};
#define WUSB3812_PM_OPS	(&wusb3812_pm_ops)
#else
#define WUSB3812_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id wusb3812_id_table[] = {
	{"wusb3812", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, wusb3812_id_table);

static const struct of_device_id wusb_match_table[] = {
	{.compatible = "mediatek,usb_type_c",},
	{},
};

static struct i2c_driver wusb3812_driver = {
	.driver = {
		.name = "usb_type_c",
		.owner = THIS_MODULE,
		.of_match_table = wusb_match_table,
		.pm = WUSB3812_PM_OPS,
	},
	.probe = wusb3812_i2c_probe,
	.remove = wusb3812_i2c_remove,
	.shutdown = wusb3812_shutdown,
	.id_table = wusb3812_id_table,
};

static int __init wusb3812_init(void)
{
	struct device_node *np;

	WS_DBG("(%s): initializing...\n",  WUSB3812C_DRV_VERSION);
	np = of_find_node_by_name(NULL, "usb_type_c");
	if (np != NULL)
		WS_DBG("wusb3812 node found...\n");
	else
		WS_DBG("wusb3812 node not found...\n");

	return i2c_add_driver(&wusb3812_driver);
}
subsys_initcall(wusb3812_init);

static void __exit wusb3812_exit(void)
{
	i2c_del_driver(&wusb3812_driver);
}
module_exit(wusb3812_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ray Deng <ray.deng@corp.ovt.com>");
MODULE_DESCRIPTION("WUSB3812 TCPC Driver");
MODULE_VERSION(WUSB3812C_DRV_VERSION);

/**** Release Note ****
 *
 * 1.0.0_MTK
 * First released PD3.0 Driver on MTK platform
 */