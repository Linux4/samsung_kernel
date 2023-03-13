/*
 * Copyright (C) 2020 Hynetek Inc.
 *
 * Hynetek HUSB311 Type-C Port Control Driver
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#define DEBUG
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

#include "inc/pd_dbg_info.h"
#include "inc/tcpci.h"
#include "inc/husb311.h"
/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 start */
#include <linux/power_supply.h>
/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 end */

#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
#include "inc/husb-regmap.h"
#endif /* CONFIG_HUSB_REGMAP */

#if 1 /*  #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))*/
#include <linux/sched/rt.h>
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)) */

/* #define DEBUG_GPIO	66 */

#define HUSB311_DRV_VERSION	"2.0.5_G"

#define HUSB311_IRQ_WAKE_TIME	(500) /* ms */

struct husb311_chip {
	struct i2c_client *client;
	struct device *dev;
#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	struct husb_regmap_device *m_dev;
#endif /* CONFIG_HUSB_REGMAP */
	struct tcpc_desc *tcpc_desc;
	struct tcpc_device *tcpc;

	atomic_t poll_count;
	struct delayed_work poll_work;

	int irq_gpio;
	int irq;
	int chip_id;
	int husb311_version;

	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 start */
	struct power_supply *typec_psy;
	struct power_supply_desc psd;
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 end */
};

#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_VID, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_PID, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_DID, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_TYPEC_REV, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_PD_REV, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_PDIF_REV, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_ALERT, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_ALERT_MASK, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_POWER_STATUS_MASK, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_FAULT_STATUS_MASK, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_HUSB311_TCPC_CTRL, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_ROLE_CTRL, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_FAULT_CTRL, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_POWER_CTRL, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_CC_STATUS, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_POWER_STATUS, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_FAULT_STATUS, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_COMMAND, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_MSG_HDR_INFO, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_RX_DETECT, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_RX_BYTE_CNT, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_RX_BUF_FRAME_TYPE, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_RX_HDR, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_RX_DATA, 28, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_TRANSMIT, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_TX_BYTE_CNT, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_TX_HDR, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_TCPC_V10_REG_TX_DATA, 28, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_CONFIG_GPIO0, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_PHY_CTRL1, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_CLK_CTRL2, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_CLK_CTRL3, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_PRL_FSM_RESET, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_BMC_CTRL, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_BMCIO_RXDZSEL, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_VCONN_CLIMITEN, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_HUSB_STATUS, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_HUSB_INT, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_HUSB_MASK, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_IDLE_CTRL, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_INTRST_CTRL, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_WATCHDOG_CTRL, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_I2CRST_CTRL, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_SWRESET, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_TTCPC_FILTER, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_DRP_TOGGLE_CYCLE, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_DRP_DUTY_CTRL, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_BMCIO_RXDZEN, 1, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_UNLOCK_PW_2, 2, HUSB_VOLATILE, {});
HUSB_REG_DECL(HUSB311_REG_EFUSE5, 1, HUSB_VOLATILE, {});

static const husb_register_map_t husb311_chip_regmap[] = {
	HUSB_REG(HUSB311_TCPC_V10_REG_VID),
	HUSB_REG(HUSB311_TCPC_V10_REG_PID),
	HUSB_REG(HUSB311_TCPC_V10_REG_DID),
	HUSB_REG(HUSB311_TCPC_V10_REG_TYPEC_REV),
	HUSB_REG(HUSB311_TCPC_V10_REG_PD_REV),
	HUSB_REG(HUSB311_TCPC_V10_REG_PDIF_REV),
	HUSB_REG(HUSB311_TCPC_V10_REG_ALERT),
	HUSB_REG(HUSB311_TCPC_V10_REG_ALERT_MASK),
	HUSB_REG(HUSB311_TCPC_V10_REG_POWER_STATUS_MASK),
	HUSB_REG(HUSB311_TCPC_V10_REG_FAULT_STATUS_MASK),
	HUSB_REG(HUSB311_TCPC_V10_REG_HUSB311_TCPC_CTRL),
	HUSB_REG(HUSB311_TCPC_V10_REG_ROLE_CTRL),
	HUSB_REG(HUSB311_TCPC_V10_REG_FAULT_CTRL),
	HUSB_REG(HUSB311_TCPC_V10_REG_POWER_CTRL),
	HUSB_REG(HUSB311_TCPC_V10_REG_CC_STATUS),
	HUSB_REG(HUSB311_TCPC_V10_REG_POWER_STATUS),
	HUSB_REG(HUSB311_TCPC_V10_REG_FAULT_STATUS),
	HUSB_REG(HUSB311_TCPC_V10_REG_COMMAND),
	HUSB_REG(HUSB311_TCPC_V10_REG_MSG_HDR_INFO),
	HUSB_REG(HUSB311_TCPC_V10_REG_RX_DETECT),
	HUSB_REG(HUSB311_TCPC_V10_REG_RX_BYTE_CNT),
	HUSB_REG(HUSB311_TCPC_V10_REG_RX_BUF_FRAME_TYPE),
	HUSB_REG(HUSB311_TCPC_V10_REG_RX_HDR),
	HUSB_REG(HUSB311_TCPC_V10_REG_RX_DATA),
	HUSB_REG(HUSB311_TCPC_V10_REG_TRANSMIT),
	HUSB_REG(HUSB311_TCPC_V10_REG_TX_BYTE_CNT),
	HUSB_REG(HUSB311_TCPC_V10_REG_TX_HDR),
	HUSB_REG(HUSB311_TCPC_V10_REG_TX_DATA),
	HUSB_REG(HUSB311_REG_CONFIG_GPIO0),
	HUSB_REG(HUSB311_REG_PHY_CTRL1),
	HUSB_REG(HUSB311_REG_CLK_CTRL2),
	HUSB_REG(HUSB311_REG_CLK_CTRL3),
	HUSB_REG(HUSB311_REG_PRL_FSM_RESET),
	HUSB_REG(HUSB311_REG_BMC_CTRL),
	HUSB_REG(HUSB311_REG_BMCIO_RXDZSEL),
	HUSB_REG(HUSB311_REG_VCONN_CLIMITEN),
	HUSB_REG(HUSB311_REG_HUSB_STATUS),
	HUSB_REG(HUSB311_REG_HUSB_INT),
	HUSB_REG(HUSB311_REG_HUSB_MASK),
	HUSB_REG(HUSB311_REG_IDLE_CTRL),
	HUSB_REG(HUSB311_REG_INTRST_CTRL),
	HUSB_REG(HUSB311_REG_WATCHDOG_CTRL),
	HUSB_REG(HUSB311_REG_I2CRST_CTRL),
	HUSB_REG(HUSB311_REG_SWRESET),
	HUSB_REG(HUSB311_REG_TTCPC_FILTER),
	HUSB_REG(HUSB311_REG_DRP_TOGGLE_CYCLE),
	HUSB_REG(HUSB311_REG_DRP_DUTY_CTRL),
	HUSB_REG(HUSB311_REG_BMCIO_RXDZEN),
	HUSB_REG(HUSB311_REG_UNLOCK_PW_2),
	HUSB_REG(HUSB311_REG_EFUSE5),
};

#define HUSB311_CHIP_REGMAP_SIZE ARRAY_SIZE(husb311_chip_regmap)

#endif /* CONFIG_HUSB_REGMAP */

static int husb311_read_device(void *client, u32 reg, int len, void *dst)
{
	struct i2c_client *i2c = client;
	int ret = 0, count = 5;
	u64 t1 = 0, t2 = 0;

	while (1) {
		t1 = local_clock();
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
		t2 = local_clock();
		HUSB311_INFO("%s del = %lluus, reg = %02X, len = %d\n",
			    __func__, (t2 - t1) / NSEC_PER_USEC, reg, len);
		if (ret < 0 && count > 1)
			count--;
		else
			break;
		udelay(100);
	}
	return ret;
}

static int husb311_write_device(void *client, u32 reg, int len, const void *src)
{
	struct i2c_client *i2c = client;
	int ret = 0, count = 5;
	u64 t1 = 0, t2 = 0;

	while (1) {
		t1 = local_clock();
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, src);
		t2 = local_clock();
		HUSB311_INFO("%s del = %lluus, reg = %02X, len = %d\n",
			    __func__, (t2 - t1) / NSEC_PER_USEC, reg, len);
		if (ret < 0 && count > 1)
			count--;
		else
			break;
		udelay(100);
	}
	return ret;
}

static int husb311_reg_read(struct i2c_client *i2c, u8 reg)
{
	struct husb311_chip *chip = i2c_get_clientdata(i2c);
	u8 val = 0;
	int ret = 0;

#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	ret = husb_regmap_block_read(chip->m_dev, reg, 1, &val);
#else
	ret = husb311_read_device(chip->client, reg, 1, &val);
#endif /* CONFIG_HUSB_REGMAP */
	if (ret < 0) {
		dev_err(chip->dev, "husb311 reg read fail\n");
		return ret;
	}
	return val;
}

static int husb311_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
	struct husb311_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;

#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	ret = husb_regmap_block_write(chip->m_dev, reg, 1, &data);
#else
	ret = husb311_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_HUSB_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "husb311 reg write fail\n");
	return ret;
}

static int husb311_block_read(struct i2c_client *i2c,
			u8 reg, int len, void *dst)
{
	struct husb311_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	ret = husb_regmap_block_read(chip->m_dev, reg, len, dst);
#else
	ret = husb311_read_device(chip->client, reg, len, dst);
#endif /* #if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE) */
	if (ret < 0)
		dev_err(chip->dev, "husb311 block read fail\n");
	return ret;
}

static int husb311_block_write(struct i2c_client *i2c,
			u8 reg, int len, const void *src)
{
	struct husb311_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	ret = husb_regmap_block_write(chip->m_dev, reg, len, src);
#else
	ret = husb311_write_device(chip->client, reg, len, src);
#endif /* #if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE) */
	if (ret < 0)
		dev_err(chip->dev, "husb311 block write fail\n");
	return ret;
}

static int32_t husb311_write_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t data)
{
	int ret;

	/* don't need swap */
	ret = husb311_block_write(client, reg_addr, 2, (uint8_t *)&data);
	return ret;
}

static int32_t husb311_read_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t *data)
{
	int ret;

	/* don't need swap */
	ret = husb311_block_read(client, reg_addr, 2, (uint8_t *)data);
	return ret;
}

static inline int husb311_i2c_write8(
	struct tcpc_device *tcpc, u8 reg, const u8 data)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);

	return husb311_reg_write(chip->client, reg, data);
}

static inline int husb311_i2c_write16(
		struct tcpc_device *tcpc, u8 reg, const u16 data)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);

	return husb311_write_word(chip->client, reg, data);
}

static inline int husb311_i2c_read8(struct tcpc_device *tcpc, u8 reg)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);

	return husb311_reg_read(chip->client, reg);
}

static inline int husb311_i2c_read16(
	struct tcpc_device *tcpc, u8 reg)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);
	u16 data;
	int ret;

	ret = husb311_read_word(chip->client, reg, &data);
	if (ret < 0)
		return ret;
	return data;
}

#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
static struct husb_regmap_fops husb311_regmap_fops = {
	.read_device = husb311_read_device,
	.write_device = husb311_write_device,
};
#endif /* CONFIG_HUSB_REGMAP */

static int husb311_regmap_init(struct husb311_chip *chip)
{
#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	struct husb_regmap_properties *props;
	char name[32];
	int len;

	props = devm_kzalloc(chip->dev, sizeof(*props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	if (chip->chip_id == HUSB311_DID) {
		props->register_num = HUSB311_CHIP_REGMAP_SIZE;
		props->rm = husb311_chip_regmap;
	}

	props->husb_regmap_mode = HUSB_MULTI_BYTE |
				HUSB_IO_PASS_THROUGH | HUSB_DBG_SPECIAL;
	snprintf(name, sizeof(name), "husb311-%02x", chip->client->addr);

	len = strlen(name);
	props->name = kzalloc(len+1, GFP_KERNEL);
	props->aliases = kzalloc(len+1, GFP_KERNEL);

	if ((!props->name) || (!props->aliases))
		return -ENOMEM;

	strlcpy((char *)props->name, name, len+1);
	strlcpy((char *)props->aliases, name, len+1);
	props->io_log_en = 0;

	chip->m_dev = husb_regmap_device_register(props,
			&husb311_regmap_fops, chip->dev, chip->client, chip);
	if (!chip->m_dev) {
		dev_err(chip->dev, "husb311 chip husb_regmap register fail\n");
		return -EINVAL;
	}
#endif
	return 0;
}

static int husb311_regmap_deinit(struct husb311_chip *chip)
{
#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	husb_regmap_device_unregister(chip->m_dev);
#endif
	return 0;
}

static inline int husb311_software_reset(struct tcpc_device *tcpc)
{
	int ret = husb311_i2c_write8(tcpc, HUSB311_REG_SWRESET, 1);
#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_HUSB_REGMAP */

	if (ret < 0)
		return ret;
#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	husb_regmap_cache_reload(chip->m_dev);
#endif /* CONFIG_HUSB_REGMAP */
	usleep_range(1000, 2000);
	return 0;
}

static inline int husb311_command(struct tcpc_device *tcpc, uint8_t cmd)
{
	return husb311_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, cmd);
}

static int husb311_init_vbus_cal(struct tcpc_device *tcpc)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);
	const u8 val_en_test_mode[] = {0x86, 0x62};
	const u8 val_dis_test_mode[] = {0x00, 0x00};
	int ret = 0;
	u8 data = 0;
	s8 cal = 0;

	ret = husb311_block_write(chip->client, HUSB311_REG_UNLOCK_PW_2,
			ARRAY_SIZE(val_en_test_mode), val_en_test_mode);
	if (ret < 0)
		dev_notice(chip->dev, "%s en test mode fail(%d)\n",
				__func__, ret);

	ret = husb311_reg_read(chip->client, HUSB311_REG_EFUSE5);
	if (ret < 0)
		goto out;

	data = ret;
	data = (data & HUSB311_REG_M_VBUS_CAL) >> HUSB311_REG_S_VBUS_CAL;
	cal = (data & BIT(2)) ? (data | GENMASK(7, 3)) : data;
	cal -= 2;
	if (cal < HUSB311_REG_MIN_VBUS_CAL)
		cal = HUSB311_REG_MIN_VBUS_CAL;
	data = (cal << HUSB311_REG_S_VBUS_CAL) | (ret & GENMASK(4, 0));

	ret = husb311_reg_write(chip->client, HUSB311_REG_EFUSE5, data);
out:
	ret = husb311_block_write(chip->client, HUSB311_REG_UNLOCK_PW_2,
			ARRAY_SIZE(val_dis_test_mode), val_dis_test_mode);
	if (ret < 0)
		dev_notice(chip->dev, "%s dis test mode fail(%d)\n",
				__func__, ret);

	return ret;
}

static int husb311_init_alert_mask(struct tcpc_device *tcpc)
{
	uint16_t mask;
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);

	mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;

#if defined(CONFIG_USB_POWER_DELIVERY) || defined(CONFIG_USB_POWER_DELIVERY_MODULE)
	/* Need to handle RX overflow */
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS | TCPC_V10_REG_ALERT_TX_DISCARDED
			| TCPC_V10_REG_ALERT_TX_FAILED
			| TCPC_V10_REG_ALERT_RX_HARD_RST
			| TCPC_V10_REG_ALERT_RX_STATUS
			| TCPC_V10_REG_RX_OVERFLOW;
#endif

	mask |= TCPC_REG_ALERT_FAULT;

	return husb311_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, mask);
}

static int husb311_init_power_status_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask = TCPC_V10_REG_POWER_STATUS_VBUS_PRES;

	return husb311_i2c_write8(tcpc,
			TCPC_V10_REG_POWER_STATUS_MASK, mask);
}

static int husb311_init_fault_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask =
		TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
		TCPC_V10_REG_FAULT_STATUS_VCONN_OC;

	return husb311_i2c_write8(tcpc,
			TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static int husb311_init_husb_mask(struct tcpc_device *tcpc)
{
	uint8_t husb_mask = 0;
#ifdef CONFIG_TCPC_WATCHDOG_EN
	husb_mask |= HUSB311_REG_M_WATCHDOG;
#endif /* CONFIG_TCPC_WATCHDOG_EN */
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	husb_mask |= HUSB311_REG_M_VBUS_80;
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_TYPEC_CAP_RA_DETACH
	if (tcpc->tcpc_flags & TCPC_FLAGS_CHECK_RA_DETACHE)
		husb_mask |= HUSB311_REG_M_RA_DETACH;
#endif /* CONFIG_TYPEC_CAP_RA_DETACH */

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
	if (tcpc->tcpc_flags & TCPC_FLAGS_LPM_WAKEUP_WATCHDOG)
		husb_mask |= HUSB311_REG_M_WAKEUP;
#endif	/* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

	return husb311_i2c_write8(tcpc, HUSB311_REG_HUSB_MASK, husb_mask);
}

static inline void husb311_poll_ctrl(struct husb311_chip *chip)
{
	cancel_delayed_work_sync(&chip->poll_work);

	if (atomic_read(&chip->poll_count) == 0) {
		atomic_inc(&chip->poll_count);
		cpu_idle_poll_ctrl(true);
	}

	schedule_delayed_work(
		&chip->poll_work, msecs_to_jiffies(40));
}

static void husb311_poll_work(struct work_struct *work)
{
	struct husb311_chip *chip = container_of(
		work, struct husb311_chip, poll_work.work);

	if (atomic_dec_and_test(&chip->poll_count))
		cpu_idle_poll_ctrl(false);
}

static irqreturn_t husb311_intr_handler(int irq, void *data)
{
	struct husb311_chip *chip = data;

	pm_wakeup_event(chip->dev, HUSB311_IRQ_WAKE_TIME);

	husb311_poll_ctrl(chip);

	tcpci_lock_typec(chip->tcpc);
	tcpci_alert(chip->tcpc);
	tcpci_unlock_typec(chip->tcpc);

	return IRQ_HANDLED;
}

static int husb311_init_alert(struct tcpc_device *tcpc)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);
	int ret = 0;
	char *name = NULL;

	/* Clear Alert Mask & Status */
	husb311_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, 0);
	husb311_write_word(chip->client, TCPC_V10_REG_ALERT, 0xffff);

	name = devm_kasprintf(chip->dev, GFP_KERNEL, "%s-IRQ",
			      chip->tcpc_desc->name);
	if (!name)
		return -ENOMEM;

	dev_info(chip->dev, "%s name = %s, gpio = %d\n",
			    __func__, chip->tcpc_desc->name, chip->irq_gpio);

	ret = devm_gpio_request(chip->dev, chip->irq_gpio, name);
	if (ret < 0) {
		dev_notice(chip->dev, "%s request GPIO fail(%d)\n",
				      __func__, ret);
		return ret;
	}

	ret = gpio_direction_input(chip->irq_gpio);
	if (ret < 0) {
		dev_notice(chip->dev, "%s set GPIO fail(%d)\n", __func__, ret);
		return ret;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq < 0) {
		dev_notice(chip->dev, "%s gpio to irq fail(%d)",
				      __func__, ret);
		return ret;
	}

	dev_info(chip->dev, "%s IRQ number = %d\n", __func__, chip->irq);

	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					husb311_intr_handler,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					name, chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s request irq fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	device_init_wakeup(chip->dev, true);

	return 0;
}

int husb311_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
	int ret;
	uint16_t mask_t1;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t mask_t2;
#endif

	/* Write 1 clear */
	mask_t1 = (uint16_t) mask;
	if (mask_t1) {
		ret = husb311_i2c_write16(tcpc, TCPC_V10_REG_ALERT, mask_t1);
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	mask_t2 = mask >> 16;
	if (mask_t2) {
		ret = husb311_i2c_write8(tcpc, HUSB311_REG_HUSB_INT, mask_t2);
		if (ret < 0)
			return ret;
	}
#endif

	return 0;
}

static int husb311_set_clock_gating(struct tcpc_device *tcpc, bool en)
{
	int ret = 0;

#ifdef CONFIG_TCPC_CLOCK_GATING
	int i = 0;
	uint8_t clk2 = HUSB311_REG_CLK_DIV_600K_EN
		| HUSB311_REG_CLK_DIV_300K_EN | HUSB311_REG_CLK_CK_300K_EN;
	uint8_t clk3 = HUSB311_REG_CLK_DIV_2P4M_EN;

	if (!en) {
		clk2 |=
			HUSB311_REG_CLK_BCLK2_EN | HUSB311_REG_CLK_BCLK_EN;
		clk3 |=
			HUSB311_REG_CLK_CK_24M_EN | HUSB311_REG_CLK_PCLK_EN;
	}

	if (en) {
		for (i = 0; i < 2; i++)
			ret = husb311_alert_status_clear(tcpc,
				TCPC_REG_ALERT_RX_ALL_MASK);
	}

	if (ret == 0)
		ret = husb311_i2c_write8(tcpc, HUSB311_REG_CLK_CTRL2, clk2);
	if (ret == 0)
		ret = husb311_i2c_write8(tcpc, HUSB311_REG_CLK_CTRL3, clk3);
#endif	/* CONFIG_TCPC_CLOCK_GATING */

	return ret;
}

static inline int husb311_init_cc_params(
			struct tcpc_device *tcpc, uint8_t cc_res)
{
	int rv = 0;

#if defined(CONFIG_USB_POWER_DELIVERY) || defined(CONFIG_USB_POWER_DELIVERY_MODULE)
#ifdef CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC
	uint8_t en, sel;

	if (cc_res == TYPEC_CC_VOLT_SNK_DFT) {	/* 0.55 */
		en = 0;
		sel = 0x81;
	} else {	/* 0.4 & 0.7 */
		en = 1;
		sel = 0x80;
	}

	rv = husb311_i2c_write8(tcpc, HUSB311_REG_BMCIO_RXDZEN, en);
	if (rv == 0)
		rv = husb311_i2c_write8(tcpc, HUSB311_REG_BMCIO_RXDZSEL, sel);
#endif	/* CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC */
#endif	/* CONFIG_USB_POWER_DELIVERY */

	return rv;
}

static int husb311_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	bool retry_discard_old = false;
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);

	HUSB311_INFO("\n");

	if (sw_reset) {
		ret = husb311_software_reset(tcpc);
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_TCPC_I2CRST_EN
	if ((chip->chip_id == HUSB311_DID) && (chip->husb311_version == HUSB311_B)){

	} else {
		husb311_i2c_write8(tcpc,
			HUSB311_REG_I2CRST_CTRL,
			HUSB311_REG_I2CRST_SET(true, 0x0f));
	}
#endif	/* CONFIG_TCPC_I2CRST_EN */

	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	husb311_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL,
		TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));

	/*
	 * CC Detect Debounce : 26.7*val us
	 * Transition window count : spec 12~20us, based on 2.4MHz
	 * DRP Toggle Cycle : 51.2 + 6.4*val ms
	 * DRP Duyt Ctrl : dcSRC: /1024
	 */

	husb311_i2c_write8(tcpc, HUSB311_REG_TTCPC_FILTER, 10);
	husb311_i2c_write8(tcpc, HUSB311_REG_DRP_TOGGLE_CYCLE, 4);
	husb311_i2c_write16(tcpc,
		HUSB311_REG_DRP_DUTY_CTRL, TCPC_NORMAL_RP_DUTY);

	/* Vconn OC */
	husb311_i2c_write8(tcpc, HUSB311_REG_VCONN_CLIMITEN, 1);

	/* RX/TX Clock Gating (Auto Mode)*/
	if (!sw_reset)
		husb311_set_clock_gating(tcpc, true);

	if (!(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD))
		retry_discard_old = true;

	husb311_i2c_write8(tcpc, HUSB311_REG_CONFIG_GPIO0, 0x80);

	/* For BIST, Change Transition Toggle Counter (Noise) from 3 to 7 */
	husb311_i2c_write8(tcpc, HUSB311_REG_PHY_CTRL1,
		HUSB311_REG_PHY_CTRL1_SET(retry_discard_old, 7, 0, 1));

	tcpci_alert_status_clear(tcpc, 0xffffffff);

	husb311_init_vbus_cal(tcpc);
	husb311_init_power_status_mask(tcpc);
	husb311_init_alert_mask(tcpc);
	husb311_init_fault_mask(tcpc);
	husb311_init_husb_mask(tcpc);

	/* CK_300K from 320K, SHIPPING off, AUTOIDLE enable, TIMEOUT = 6.4ms */
	husb311_i2c_write8(tcpc, HUSB311_REG_IDLE_CTRL,
		HUSB311_REG_IDLE_SET(0, 1, 1, 0));
	mdelay(1);

	return 0;
}

static inline int husb311_fault_status_vconn_ov(struct tcpc_device *tcpc)
{
	int ret;

	ret = husb311_i2c_read8(tcpc, HUSB311_REG_BMC_CTRL);
	if (ret < 0)
		return ret;

	ret &= ~HUSB311_REG_DISCHARGE_EN;
	return husb311_i2c_write8(tcpc, HUSB311_REG_BMC_CTRL, ret);
}

int husb311_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
	int ret;

	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OV)
		ret = husb311_fault_status_vconn_ov(tcpc);

	husb311_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
	return 0;
}

int husb311_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
	int ret;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t v2;
#endif

	ret = husb311_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK);
	if (ret < 0)
		return ret;

	*mask = (uint16_t) ret;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = husb311_i2c_read8(tcpc, HUSB311_REG_HUSB_MASK);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*mask |= v2 << 16;
#endif

	return 0;
}

int husb311_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
	int ret;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t v2;
#endif

	ret = husb311_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
	if (ret < 0)
		return ret;

	*alert = (uint16_t) ret;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = husb311_i2c_read8(tcpc, HUSB311_REG_HUSB_INT);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*alert |= v2 << 16;
#endif

	return 0;
}

static int husb311_get_power_status(
		struct tcpc_device *tcpc, uint16_t *pwr_status)
{
	int ret;

	ret = husb311_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
	if (ret < 0)
		return ret;

	*pwr_status = 0;

	if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = husb311_i2c_read8(tcpc, HUSB311_REG_HUSB_STATUS);
	if (ret < 0)
		return ret;

	if (ret & HUSB311_REG_VBUS_80)
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;
#endif
	return 0;
}

int husb311_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{
	int ret;

	ret = husb311_i2c_read8(tcpc, TCPC_V10_REG_FAULT_STATUS);
	if (ret < 0)
		return ret;
	*status = (uint8_t) ret;
	return 0;
}

static int husb311_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	int status, role_ctrl, cc_role;
	bool act_as_sink, act_as_drp;

	status = husb311_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS);
	if (status < 0)
		return status;

	role_ctrl = husb311_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
	if (role_ctrl < 0)
		return role_ctrl;

	if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	}

	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);

	act_as_drp = TCPC_V10_REG_ROLE_CTRL_DRP & role_ctrl;

	if (act_as_drp) {
		act_as_sink = TCPC_V10_REG_CC_STATUS_DRP_RESULT(status);
	} else {
		if (tcpc->typec_polarity)
			cc_role = TCPC_V10_REG_CC_STATUS_CC2(role_ctrl);
		else
			cc_role = TCPC_V10_REG_CC_STATUS_CC1(role_ctrl);
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

	husb311_init_cc_params(tcpc,
		(uint8_t)tcpc->typec_polarity ? *cc2 : *cc1);

	return 0;
}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
static int husb311_enable_vsafe0v_detect(
	struct tcpc_device *tcpc, bool enable)
{
	int ret = husb311_i2c_read8(tcpc, HUSB311_REG_HUSB_MASK);

	if (ret < 0)
		return ret;

	if (enable)
		ret |= HUSB311_REG_M_VBUS_80;
	else
		ret &= ~HUSB311_REG_M_VBUS_80;

	return husb311_i2c_write8(tcpc, HUSB311_REG_HUSB_MASK, (uint8_t) ret);
}
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

static int husb311_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret;
	uint8_t data;
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2;

	HUSB311_INFO("pull = 0x%02X\n", pull);
	pull = TYPEC_CC_PULL_GET_RES(pull);
	if (pull == TYPEC_CC_DRP) {
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(
				1, rp_lvl, TYPEC_CC_RD, TYPEC_CC_RD);

		ret = husb311_i2c_write8(
			tcpc, TCPC_V10_REG_ROLE_CTRL, data);

		if (ret == 0) {
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
			husb311_enable_vsafe0v_detect(tcpc, false);
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
			ret = husb311_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
		}
	} else {
#if defined(CONFIG_USB_POWER_DELIVERY) || defined(CONFIG_USB_POWER_DELIVERY_MODULE)
		if (pull == TYPEC_CC_RD && tcpc->pd_wait_pr_swap_complete)
			husb311_init_cc_params(tcpc, TYPEC_CC_VOLT_SNK_DFT);
#endif	/* CONFIG_USB_POWER_DELIVERY */

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
		ret = husb311_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
	}

	return 0;
}

static int husb311_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	int data;

	data = husb311_init_cc_params(tcpc,
		tcpc->typec_remote_cc[polarity]);
	if (data)
		return data;

	data = husb311_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT;
	data |= polarity ? TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT : 0;

	return husb311_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}

static int husb311_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
	uint16_t duty = low_rp ? TCPC_LOW_RP_DUTY : TCPC_NORMAL_RP_DUTY;

	return husb311_i2c_write16(tcpc, HUSB311_REG_DRP_DUTY_CTRL, duty);
}

static int husb311_set_vconn(struct tcpc_device *tcpc, int enable)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);

	int rv;
	int data;

	data = husb311_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
	data |= enable ? TCPC_V10_REG_POWER_CTRL_VCONN : 0;

	if ((chip->chip_id == HUSB311_DID) && (chip->husb311_version == HUSB311_B)) {
		data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
		pr_info("%s - write 0x%x to HUSB311_POWER_CTRL_VCONNL, off vconn\n",
				__func__, data);
	}

	rv = husb311_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data);
	if (rv < 0)
		return rv;

	return husb311_i2c_write8(tcpc, HUSB311_REG_IDLE_CTRL,
		HUSB311_REG_IDLE_SET(0, 1, enable ? 0 : 1, 0));
}

#ifdef CONFIG_TCPC_LOW_POWER_MODE
static int husb311_is_low_power_mode(struct tcpc_device *tcpc)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;

	rv = husb311_i2c_read8(tcpc, HUSB311_REG_BMC_CTRL);
	if (rv < 0)
		return rv;

	if (chip->chip_id == HUSB311_DID) {
		pr_info("%s - read 0x90=0x%x\n", __func__, rv);
		return ((rv & HUSB311_REG_BMCIO_OSC_EN) == 0);
	}

	return (rv & HUSB311_REG_BMCIO_LPEN) != 0;
}

static int husb311_set_low_power_mode(
		struct tcpc_device *tcpc, bool en, int pull)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);
	int ret = 0;
	uint8_t data;

	ret = husb311_i2c_write8(tcpc, HUSB311_REG_IDLE_CTRL,
		HUSB311_REG_IDLE_SET(0, 1, en ? 0 : 1, 0));
	if (ret < 0)
		return ret;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	husb311_enable_vsafe0v_detect(tcpc, !en);
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
	if (en) {
		data = HUSB311_REG_BMCIO_LPEN;

		if (pull & TYPEC_CC_RP)
			data |= HUSB311_REG_BMCIO_LPRPRD;

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
		data |= HUSB311_REG_BMCIO_BG_EN | HUSB311_REG_VBUS_DET_EN;
#endif
		if (chip->chip_id == HUSB311_DID) {
			data &= ~HUSB311_REG_BMCIO_OSC_EN;
			pr_info("%s - write HUSB311_REG_BMC_CTRL=0x%x\n",
				__func__, data);
		}

	} else {
		data = HUSB311_REG_BMCIO_BG_EN |
			HUSB311_REG_VBUS_DET_EN | HUSB311_REG_BMCIO_OSC_EN;

		if (chip->chip_id == HUSB311_DID) {
			data |= HUSB311_REG_BMCIO_OSC_EN;
			pr_info("%s - write HUSB311_REG_BMC_CTRL=0x%x\n",
				__func__, data);
		}
	}

	return husb311_i2c_write8(tcpc, HUSB311_REG_BMC_CTRL, data);
}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN
int husb311_set_watchdog(struct tcpc_device *tcpc, bool en)
{
	uint8_t data = HUSB311_REG_WATCHDOG_CTRL_SET(en, 7);

	return husb311_i2c_write8(tcpc,
		HUSB311_REG_WATCHDOG_CTRL, data);
}
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#ifdef CONFIG_TCPC_INTRST_EN
static int husb311_set_intrst(struct tcpc_device *tcpc, bool en)
{
	return husb311_i2c_write8(tcpc,
		HUSB311_REG_INTRST_CTRL, HUSB311_REG_INTRST_SET(en, 3));
}
#endif	/* CONFIG_TCPC_INTRST_EN */

static int husb311_tcpc_deinit(struct tcpc_device *tcpc)
{
#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_HUSB_REGMAP */

#ifdef CONFIG_TCPC_SHUTDOWN_CC_DETACH
	husb311_set_cc(tcpc, TYPEC_CC_DRP);
	husb311_set_cc(tcpc, TYPEC_CC_OPEN);

	if (chip->chip_id == HUSB311_DID) {
		tcpci_alert_status_clear(tcpc, 0xffffff);//0x10 0x11 0x98

		husb311_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, 0x0); //0x12 0x13
		husb311_i2c_write8(tcpc, TCPC_V10_REG_POWER_STATUS_MASK, 0x0); //0x14
		husb311_i2c_write8(tcpc, HUSB311_REG_HUSB_MASK, 0x0); //0x99
		husb311_i2c_write8(tcpc, HUSB311_REG_BMC_CTRL, 0x0); //0x90
	} else {
		husb311_i2c_write8(tcpc,
			HUSB311_REG_I2CRST_CTRL,
			HUSB311_REG_I2CRST_SET(true, 4));
	}

	husb311_i2c_write8(tcpc,
		HUSB311_REG_INTRST_CTRL,
		HUSB311_REG_INTRST_SET(true, 0));
#else
	husb311_i2c_write8(tcpc, HUSB311_REG_SWRESET, 1);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */
#if defined(CONFIG_HUSB_REGMAP) || defined(CONFIG_HUSB_REGMAP_MODULE)
	husb_regmap_cache_reload(chip->m_dev);
#endif /* CONFIG_HUSB_REGMAP */

	return 0;
}

#if defined(CONFIG_USB_POWER_DELIVERY) || defined(CONFIG_USB_POWER_DELIVERY_MODULE)
static int husb311_set_msg_header(
	struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
	uint8_t msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(
		data_role, power_role);

	return husb311_i2c_write8(
		tcpc, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int husb311_protocol_reset(struct tcpc_device *tcpc)
{
	husb311_i2c_write8(tcpc, HUSB311_REG_PRL_FSM_RESET, 0);
	mdelay(1);
	husb311_i2c_write8(tcpc, HUSB311_REG_PRL_FSM_RESET, 1);
	return 0;
}

static int husb311_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
	int ret = 0;

	if (enable)
		ret = husb311_set_clock_gating(tcpc, false);

	if (ret == 0)
		ret = husb311_i2c_write8(tcpc, TCPC_V10_REG_RX_DETECT, enable);

	if ((ret == 0) && (!enable)) {
		husb311_protocol_reset(tcpc);
		ret = husb311_set_clock_gating(tcpc, true);
	}

	return ret;
}

/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 start */
static int husb311_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);
	int rv = 0;
	uint8_t type, cnt = 0;
	uint8_t buf[4];
	const uint16_t alert_rx =
		TCPC_V10_REG_ALERT_RX_STATUS|TCPC_V10_REG_RX_OVERFLOW;

	rv = husb311_block_read(chip->client,
			TCPC_V10_REG_RX_BYTE_CNT, 4, buf);
	cnt = buf[0];
	type = buf[1];
	*msg_head = *(uint16_t *)&buf[2];

	if(*msg_head == 0x0) {
		tcpci_init(tcpc, true);
		pr_err("%s: msg_head=0x%x\n", __func__, *msg_head);
		return -1;
	}

	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (rv >= 0 && cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		rv = husb311_block_read(chip->client, TCPC_V10_REG_RX_DATA, cnt,
				(uint8_t *) payload);
	}

	*frame_type = (enum tcpm_transmit_type) type;

	/* Read complete, clear RX status alert bit */
	tcpci_alert_status_clear(tcpc, alert_rx);
	return rv;
}
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 end */

static int husb311_set_bist_carrier_mode(
	struct tcpc_device *tcpc, uint8_t pattern)
{
	/* Don't support this function */
	return 0;
}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
static int husb311_retransmit(struct tcpc_device *tcpc)
{
	return husb311_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
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

static int husb311_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	struct husb311_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	int data_cnt;
	struct tcpc_transmit_packet packet;

	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = sizeof(uint32_t) * PD_HEADER_CNT(header);

		packet.cnt = data_cnt + sizeof(uint16_t);
		packet.msg_header = header;

		if (data_cnt > 0)
			memcpy(packet.data, (uint8_t *) data, data_cnt);

		rv = husb311_block_write(chip->client,
				TCPC_V10_REG_TX_BYTE_CNT,
				packet.cnt+1, (uint8_t *) &packet);
		if (rv < 0)
			return rv;
	}

	rv = husb311_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, type));
	return rv;
}

static int husb311_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	int data;

	data = husb311_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
	data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;

	return husb311_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static struct tcpc_ops husb311_tcpc_ops = {
	.init = husb311_tcpc_init,
	.alert_status_clear = husb311_alert_status_clear,
	.fault_status_clear = husb311_fault_status_clear,
	.get_alert_mask = husb311_get_alert_mask,
	.get_alert_status = husb311_get_alert_status,
	.get_power_status = husb311_get_power_status,
	.get_fault_status = husb311_get_fault_status,
	.get_cc = husb311_get_cc,
	.set_cc = husb311_set_cc,
	.set_polarity = husb311_set_polarity,
	.set_low_rp_duty = husb311_set_low_rp_duty,
	.set_vconn = husb311_set_vconn,
	.deinit = husb311_tcpc_deinit,

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	.is_low_power_mode = husb311_is_low_power_mode,
	.set_low_power_mode = husb311_set_low_power_mode,
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN
	.set_watchdog = husb311_set_watchdog,
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#ifdef CONFIG_TCPC_INTRST_EN
	.set_intrst = husb311_set_intrst,
#endif	/* CONFIG_TCPC_INTRST_EN */

#if defined(CONFIG_USB_POWER_DELIVERY) || defined(CONFIG_USB_POWER_DELIVERY_MODULE)
	.set_msg_header = husb311_set_msg_header,
	.set_rx_enable = husb311_set_rx_enable,
	.protocol_reset = husb311_protocol_reset,
	.get_message = husb311_get_message,
	.transmit = husb311_transmit,
	.set_bist_test_mode = husb311_set_bist_test_mode,
	.set_bist_carrier_mode = husb311_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = husb311_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
};

static int husb_parse_dt(struct husb311_chip *chip, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	pr_info("%s\n", __func__);

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "husb311pd,intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s-%d: no intr_gpio info\n", __func__, __LINE__);
		return ret;
	}
	chip->irq_gpio = ret;
#else
	ret = of_property_read_u32(np,
		"husb311pd,intr_gpio_num", &chip->irq_gpio);
	if (ret < 0)
		pr_err("%s: no intr_gpio info\n", __func__);
#endif
	return ret < 0 ? ret : 0;
}

/*
 * In some platform pr_info may spend too much time on printing debug message.
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

#if defined(CONFIG_PD_DBG_INFO) || defined(CONFIG_PD_DBG_INFO_MODULE)
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
		pr_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_info("pr_info : t2-t1 = %lu\n",
				(unsigned long)nsrem / 1000);
	}
#else
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pr_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_info("t2-t1 = %lu\n",
				(unsigned long)nsrem /  1000);
		PD_BUG_ON(nsrem > 100*1000);
	}
#endif /* CONFIG_PD_DBG_INFO */
}
#endif /* TCPC_ENABLE_ANYMSG */

static int husb311_tcpcdev_init(struct husb311_chip *chip, struct device *dev)
{
	struct tcpc_desc *desc;
	struct device_node *np = dev->of_node;
	u32 val, len;
	const char *name = "default";

	dev_info(dev, "%s\n", __func__);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	if (of_property_read_u32(np, "husb-tcpc,role_def", &val) >= 0) {
		if (val >= TYPEC_ROLE_NR)
			desc->role_def = TYPEC_ROLE_DRP;
		else
			desc->role_def = val;
	} else {
		dev_info(dev, "use default Role DRP\n");
		desc->role_def = TYPEC_ROLE_DRP;
	}

	if (of_property_read_u32(
		np, "husb-tcpc,notifier_supply_num", &val) >= 0) {
		if (val < 0)
			desc->notifier_supply_num = 0;
		else
			desc->notifier_supply_num = val;
	} else
		desc->notifier_supply_num = 0;

	if (of_property_read_u32(np, "husb-tcpc,rp_level", &val) >= 0) {
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
	if (of_property_read_u32(np, "husb-tcpc,vconn_supply", &val) >= 0) {
		if (val >= TCPC_VCONN_SUPPLY_NR)
			desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
		else
			desc->vconn_supply = val;
	} else {
		dev_info(dev, "use default VconnSupply\n");
		desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	if (of_property_read_string(np, "husb-tcpc,name",
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
			desc, &husb311_tcpc_ops, chip);
	if (IS_ERR(chip->tcpc))
		return -EINVAL;

	chip->tcpc->tcpc_flags = TCPC_FLAGS_LPM_WAKEUP_WATCHDOG |
			TCPC_FLAGS_VCONN_SAFE5V_ONLY;

	if (chip->chip_id == HUSB311_DID)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_CHECK_RA_DETACHE;
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 start */
#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	if (chip->chip_id == HUSB311_DID)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif  /* CONFIG_USB_PD_RETRY_CRC_DISCARD */
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 end */

#ifdef CONFIG_USB_PD_REV30
	if (chip->chip_id == HUSB311_DID)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;

	if (chip->tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
		dev_info(dev, "PD_REV30\n");
	else
		dev_info(dev, "PD_REV20\n");
#endif	/* CONFIG_USB_PD_REV30 */
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_ALERT_V10;

	return 0;
}

#define HUSB311_VID	        0x2e99
#define HUSB311_PID	        0x0311

/* hs14 code for SR-AL6528A-945 by shanxinkai at 2022/11/21 start */
/**
* HUSB311C Register detection,Monitor leakage current.
*/
static inline int husb311_cfg_m6_check(struct i2c_client *client)
{
	int ret;
	u8 data = 1;
	u8 version, cfg_mode;
	u8 data_table_cnt = 15;
	u8 data_table[15] = \
		{0x0,0x0,0x0,0x01,0x99,0x2E,0x11,0x03, \
		0x00,0x00,0x24,0x0D,0x94,0x6E,0x00};

	// struct husb311_chip *chip = i2c_get_clientdata(client);

	ret = husb311_read_device(client, HUSB311_REG_CF, 1, &version);

	if (ret < 0) {
		dev_err(&client->dev, "read 0xcf fail(%d)\n", ret);
		return -EIO;
	}

	if (version & BIT(7)) {
		pr_info("%s:chip version [0xCF]=0x%x,HUSB311\n", __func__, version);
		return 0;
	} else {
		pr_info("%s:chip version [0xCF]=0x%x,HUSB311C\n", __func__, version);
	}

	if ((version != 0xFF) && (version != 0x00)
		&& ((version == 0x22) || (version == 0x30))) {
		data = HUSB311_CFG_ENTRY;
		ret = husb311_write_device(client, HUSB311_CFG_MODE, 1, &data);
		if (ret < 0)
			return ret;
		pr_info("%s, HUSB311_CFG_MODE=0x%02x\n", __func__, data);
		usleep_range(100, 200);

		ret = husb311_read_device(client, HUSB311_CFG_MODE, 1, &cfg_mode);

		if (ret < 0) {
			dev_err(&client->dev, "read 0xbf fail(%d)\n", ret);
			return -EIO;
		}

		if(cfg_mode != 0x01)
			return 0;
		usleep_range(100, 200);

		ret = husb311_write_device(client, HUSB311_CFG_START, data_table_cnt, &data_table);

		if (ret < 0) {
			dev_err(&client->dev, "write HUSB311_CFG_START fail(%d)\n", ret);
			return -EIO;
		}
		usleep_range(100, 200);

		data = HUSB311_CFG_EXIT;
		ret = husb311_write_device(client, HUSB311_CFG_MODE, 1, &data);

		if (ret < 0)
			return ret;
		pr_info("%s, HUSB311_CFG_MODE=0x%02x\n", __func__, data);

		usleep_range(1000, 2000);
	}

	return 0;
}
/* hs14 code for SR-AL6528A-945 by shanxinkai at 2022/11/21 end */

static inline int husb311_check_revision(struct i2c_client *client)
{
	u16 vid, pid, did;
	int ret;
	u8 data = 1;
	/* hs14 code for SR-AL6528A-945 by shanxinkai at 2022/11/21 start */
	u8 tmp_reg_cd = 0;
	/* hs14 code for SR-AL6528A-945 by shanxinkai at 2022/11/21 end */

	ret = husb311_read_device(client, TCPC_V10_REG_VID, 2, &vid);
	if (ret < 0) {
		dev_err(&client->dev, "read chip ID fail(%d)\n", ret);
		return -EIO;
	}

	if (vid != HUSB311_VID) {
		pr_info("%s failed, VID=0x%04x\n", __func__, vid);
		return -ENODEV;
	}

	ret = husb311_read_device(client, TCPC_V10_REG_PID, 2, &pid);
	if (ret < 0) {
		dev_err(&client->dev, "read product ID fail(%d)\n", ret);
		return -EIO;
	}

	pr_info("%s: vid=0x%x, pid=0x%x\n", __func__, vid, pid);
	if (pid != HUSB311_PID) {
		pr_info("%s failed, PID=0x%04x\n", __func__, pid);
		return -ENODEV;
	}

	/* hs14 code for SR-AL6528A-945 by shanxinkai at 2022/11/21 start */
	ret = husb311_read_device(client, HUSB311_REG_CD, 1, &tmp_reg_cd);
	if (ret < 0) {
		dev_err(&client->dev, "read 0xcd fail(%d)\n", ret);
	}

	pr_info("%s checking [0xCD]=0x%x\n", __func__, tmp_reg_cd);

	if(tmp_reg_cd & BIT(0)) {
		husb311_cfg_m6_check(client);
	}
	/* hs14 code for SR-AL6528A-945 by shanxinkai at 2022/11/21 end */

	ret = husb311_write_device(client, HUSB311_REG_SWRESET, 1, &data);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);

	ret = husb311_read_device(client, TCPC_V10_REG_DID, 2, &did);
	if (ret < 0) {
		dev_err(&client->dev, "read device ID fail(%d)\n", ret);
		return -EIO;
	}

	return did;
}

/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 start */
enum {
	TYPEC_STATUS_UNKNOWN = 0,
	TYPEC_STATUS_CC1,
	TYPEC_STATUS_CC2,
};

static int typec_port_get_property(struct power_supply *psy, enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	int cc1, cc2;
	struct husb311_chip *chip = container_of(psy->desc, struct husb311_chip, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		ret = husb311_get_cc(chip->tcpc, &cc1, &cc2);
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
/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 end */

/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/08 start */
static int husb311_i2c_remove(struct i2c_client *client);
/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/08 end */
static int husb311_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct husb311_chip *chip;
	int ret = 0, chip_id;
	bool use_dt = client->dev.of_node;
	u8 version;
	/* hs14 code for AL6528A-1013 by lina at 2022/11/30 start */
	int soft_reset_cnt = 10;
	u8 data = 1;
	/* hs14 code for AL6528A-1013 by lina at 2022/11/30 end */
#ifdef HUSB_GPIO_CFG
	gpio_direction_output(HUSB_ATTACH_STATUS, 0);
	gpio_direction_output(HUSB_VCONN_PWR_EN, 0);
	gpio_direction_output(HUSB_OTG_VBUS_OUT_EN, 0);

	gpio_direction_output(HUSB_OTG_VBUS_PATH_EN, 0);
	gpio_direction_output(HUSB_SINK_VBUS_PATH_EN, 0);

	gpio_direction_output(HUSB_OTG_VBUS_DISC_EN, 0);

	gpio_direction_input(HUSB_DC_INPUT);
	gpio_direction_input(HUSB_SINK_PDO_BUTTON);
	gpio_direction_input(HUSB_PRS_BUTTON);
#endif /* HUSB_GPIO_CFG */

	/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/08 start */
	pr_info("%s (%s) probe start\n", __func__, HUSB311_DRV_VERSION);
	if (i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA))
		pr_info("I2C functionality : OK...\n");
	else
		pr_info("I2C functionality check : failuare...\n");

	chip_id = husb311_check_revision(client);
	/* hs14 code for AL6528A-1013 by lina at 2022/11/30 start */
	if (chip_id < 0) {
		do {
			soft_reset_cnt--;
			ret = husb311_write_device(client, HUSB311_REG_SWRESET, 1, &data);
			usleep_range(1000, 2000);
			chip_id = husb311_check_revision(client);
		} while (soft_reset_cnt && (chip_id < 0));

		if (chip_id < 0) {
			pr_err("%s:not match husb311 chiID, remove,soft_reset_cnt=%d\n", __func__, soft_reset_cnt);
			husb311_i2c_remove(client);
			return chip_id;
		} else {
			pr_info("%s:soft_reset_cnt=%d\n", __func__, soft_reset_cnt);
		}
	}
	/* hs14 code for AL6528A-1013 by lina at 2022/11/30 end */
	/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/08 end */

#if TCPC_ENABLE_ANYMSG
	check_printk_performance();
#endif /* TCPC_ENABLE_ANYMSG */

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (use_dt) {
		ret = husb_parse_dt(chip, &client->dev);
		if (ret < 0)
			return ret;
	} else {
		dev_err(&client->dev, "no dts node\n");
		return -ENODEV;
	}
	chip->dev = &client->dev;
	chip->client = client;
	i2c_set_clientdata(client, chip);
	INIT_DELAYED_WORK(&chip->poll_work, husb311_poll_work);
	chip->chip_id = chip_id;
	pr_info("husb311_chipID = 0x%0x\n", chip_id);


	if (chip_id == HUSB311_DID) {
		ret = husb311_read_device(client, HUSB311_REG_CF, 1, &version);
		if (ret < 0) {
			dev_err(&client->dev, "read 0xcf fail(%d)\n", ret);
			return -EIO;
		}
		if (version & BIT(7)) {
			chip->husb311_version = HUSB311_B;
			pr_info("%s:version=0x%x,311B\n", __func__, version);
		} else {
			chip->husb311_version = HUSB311_C;
			pr_info("%s:version=0x%x,311C\n", __func__, version);
		}
	/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/08 start */
	} else {
		pr_err("%s:not match husb311 chiID, remove\n", __func__);
		husb311_i2c_remove(client);
		return -EINVAL;
	}
	/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/08 end */

	ret = husb311_regmap_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "husb311 regmap init fail\n");
		goto err_regmap_init;
	}

	ret = husb311_tcpcdev_init(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "husb311 tcpc dev init fail\n");
		goto err_tcpc_reg;
	}

	ret = husb311_init_alert(chip->tcpc);
	if (ret < 0) {
		pr_err("husb311 init alert fail\n");
		goto err_irq_init;
	}

	tcpc_schedule_init_work(chip->tcpc);

	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 start */
	chip->psd.name = "typec_port";
	chip->psd.properties = typec_port_properties;
	chip->psd.type = POWER_SUPPLY_TYPE_USB_TYPE_C;
	chip->psd.num_properties = ARRAY_SIZE(typec_port_properties);
	chip->psd.get_property = typec_port_get_property;
	chip->typec_psy = devm_power_supply_register(chip->dev, &chip->psd, NULL);
	if (IS_ERR(chip->typec_psy)) {
		pr_err("husb311 failed to register power supply: %ld\n",
			PTR_ERR(chip->typec_psy));
	}
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/08 end */
	pr_info("%s probe OK!\n", __func__);

	/* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 start */
	tcpc_info = HUSB311;
	/* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 start */

	return 0;

err_irq_init:
	tcpc_device_unregister(chip->dev, chip->tcpc);
err_tcpc_reg:
	husb311_regmap_deinit(chip);
err_regmap_init:
	return ret;
}

static int husb311_i2c_remove(struct i2c_client *client)
{
	struct husb311_chip *chip = i2c_get_clientdata(client);

	if (chip) {
		cancel_delayed_work_sync(&chip->poll_work);

		tcpc_device_unregister(chip->dev, chip->tcpc);
		husb311_regmap_deinit(chip);
	}

	return 0;
}

#ifdef CONFIG_PM
static int husb311_i2c_suspend(struct device *dev)
{
	struct husb311_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	if (device_may_wakeup(dev))
		enable_irq_wake(chip->irq);
	disable_irq(chip->irq);

	return 0;
}

static int husb311_i2c_resume(struct device *dev)
{
	struct husb311_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	enable_irq(chip->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(chip->irq);

	return 0;
}

static void husb311_shutdown(struct i2c_client *client)
{
        struct husb311_chip *chip = i2c_get_clientdata(client);

        /* Please reset IC here */
        if (chip != NULL) {
                if (chip->irq)
                        disable_irq(chip->irq);
                tcpm_shutdown(chip->tcpc);
        } else {
                i2c_smbus_write_byte_data(
                        client, HUSB311_REG_SWRESET, 0x01);
        }
}


#ifdef CONFIG_PM_RUNTIME
static int husb311_pm_suspend_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: suspending...\n");
	return 0;
}

static int husb311_pm_resume_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: resuming...\n");
	return 0;
}
#endif /* #ifdef CONFIG_PM_RUNTIME */

static const struct dev_pm_ops husb311_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			husb311_i2c_suspend,
			husb311_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(
		husb311_pm_suspend_runtime,
		husb311_pm_resume_runtime,
		NULL
	)
#endif /* #ifdef CONFIG_PM_RUNTIME */
};
#define HUSB311_PM_OPS	(&husb311_pm_ops)
#else
#define HUSB311_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id husb311_id_table[] = {
	{"husb311", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, husb311_id_table);

static const struct of_device_id husb_match_table[] = {
	{.compatible = "hynetek,husb311",},
	{},
};

static struct i2c_driver husb311_driver = {
	.driver = {
		/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/11 start */
		// .name = "usb_type_c",
		.name = "husb311_type_c",
		/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/11 end */
		.owner = THIS_MODULE,
		.of_match_table = husb_match_table,
		.pm = HUSB311_PM_OPS,
	},
	.probe = husb311_i2c_probe,
	.remove = husb311_i2c_remove,
	.shutdown = husb311_shutdown,
	.id_table = husb311_id_table,
};

module_i2c_driver(husb311_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("HUSB311 TCPC Driver");
MODULE_VERSION(HUSB311_DRV_VERSION);

/**** Release Note ****
 * 2.0.5_G
 * (1) Utilize husb-regmap to reduce I2C accesses
 * (2) Decrease VBUS present threshold (VBUS_CAL) by 60mV (2LSBs)
 *
 * 2.0.4_G
 * (1) Mask vSafe0V IRQ before entering low power mode
 * (2) Disable auto idle mode before entering low power mode
 * (3) Reset Protocol FSM and clear RX alerts twice before clock gating
 *
 * 2.0.3_G
 * (1) Single Rp as Attatched.SRC for Ellisys TD.4.9.4
 *
 * 2.0.2_G
 * (1) Replace wake_lock with wakeup_source
 * (2) Move down the shipping off
 * (3) Add support for NoRp.SRC
 * (4) Reg0x71[7] = 1'b1 to workaround unstable VDD Iq in low power mode
 * (5) Add get_alert_mask of tcpc_ops
 *
 * 2.0.1_G
 * First released PD3.0 Driver
 */
