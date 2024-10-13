// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/version.h>
#include <linux/hrtimer.h>

#include "../inc/pd_dbg_info.h"
#include "../inc/tcpci.h"
#include "../inc/rt1711h.h"
#include "../inc/cps8851.h"
#include "../inc/tcpci_typec.h"
#include <uapi/linux/sched/types.h>

/* A06 code for SR-AL7160A-01-270 by shixuanxuan at 20240611 start */
#include <linux/power_supply.h>
/* A06 code for SR-AL7160A-01-270 by shixuanxuan at 20240611 end */

#ifdef  CONFIG_RT_REGMAP
#include "rt-regmap.h"
#endif /* CONFIG_RT_REGMAP */

/* #define DEBUG_GPIO	66 */

#define RT1711H_DRV_VERSION	"2.0.6_MTK"

#define RT1711H_IRQ_WAKE_TIME	(500) /* ms */

#define CPS8851_VBUS_PRES_DEB_TIME	50 /* ms */
#define CPS8851_SOFTWARE_TRIM_EN	1
#define CPS8851_OTP_EN				0
#define CPS8851_OTP_RECOVERY_TIME	50 /* ms */
#define CPS8851_I2C_WDT_EN			0
#define CPS8851_I2C_WDT_TOUT		CPS8851_REG_WDT_TO_2S
#define CPS8851_I2C_WDT_KICK_TIME	1500 /* ms */
#define CPS8851_WD_CNT_THRESHOLD	5
#define CPS8851_WD_INTERVAL			500 /* ms */
#define CPS8851_WD_TRY_INTERVAL		200 /* ms */
#define CPS8851_WD_OPEN_INTERVAL	20000 /* ms */

#define RICHTEK_1711_VID	0x29cf
#define RICHTEK_1711_PID	0x1711

#define CPS_8851_VID		0x315c
#define CPS_8851_PID		0x8851

#define CPS_WD_STATE_DRY			0
#define CPS_WD_STATE_WET_PROTECTION	1

struct rt1711_chip {
	struct i2c_client *client;
	struct device *dev;
#ifdef  CONFIG_RT_REGMAP
	struct rt_regmap_device *m_dev;
#endif /* CONFIG_RT_REGMAP */
	struct tcpc_desc *tcpc_desc;
	struct tcpc_device *tcpc;

#if CPS8851_OTP_EN
	struct delayed_work	otp_work;
#endif
#if CPS8851_VBUS_PRES_DEB_TIME
	struct delayed_work	power_change_work;
	bool check_real_vbus;
	bool falling;
#endif
#if CPS8851_I2C_WDT_EN
	bool wdt_en;
	struct hrtimer wdt_timer;
	struct work_struct wdt_work;
#endif
#ifdef CONFIG_WATER_DETECTION
	bool is_wet;
	bool cc_open;
	int wd_state;
	unsigned wd_count;
	ktime_t last_set_cc_toggle_time;
	struct delayed_work	wd_work;
	struct alarm wd_wakeup_timer;
#endif
	struct semaphore suspend_lock;
	struct kthread_worker irq_worker;
	struct kthread_work irq_work;
	struct task_struct *irq_worker_task;
	int irq_gpio;
	int irq;
	int chip_id;
	int chip_pid;
	int chip_vid;
	/* A06 code for SR-AL7160A-01-270 by shixuanxuan at 20240611 start */
	struct power_supply *typec_psy;
	struct power_supply_desc psd;
	/* A06 code for SR-AL7160A-01-270 by shixuanxuan at 20240611 end*/
};

#ifdef  CONFIG_RT_REGMAP
RT_REG_DECL(TCPC_V10_REG_VID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_DID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TYPEC_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PD_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PDIF_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT_MASK, 2, RT_NORMAL, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS_MASK, 1, RT_NORMAL, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS_MASK, 1, RT_NORMAL, {});
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
RT_REG_DECL(TCPC_V10_REG_RX_BYTE_CNT, 4, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DATA, 28, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TRANSMIT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_BYTE_CNT, 31, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_CONFIG_GPIO0, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_PHY_CTRL1, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_CLK_CTRL2, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_CLK_CTRL3, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_PRL_FSM_RESET, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_BMC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_BMCIO_RXDZSEL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_VCONN_CLIMITEN, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_RT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_RT_INT, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_RT_MASK, 1, RT_NORMAL, {});
RT_REG_DECL(RT1711H_REG_IDLE_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_INTRST_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_WATCHDOG_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_I2CRST_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_SWRESET, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_TTCPC_FILTER, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_DRP_TOGGLE_CYCLE, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_DRP_DUTY_CTRL, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(CPS8851_REG_CP_OTSD, 1, RT_VOLATILE, {});
RT_REG_DECL(CPS8851_REG_CP_OTSD_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_BMCIO_RXDZEN, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(CPS8851_REG_PASSWORD, 1, RT_VOLATILE, {});
RT_REG_DECL(CPS8851_REG_PD_OPT_ZTX_SEL, 1, RT_VOLATILE, {});
RT_REG_DECL(CPS8851_REG_PD_OPT_TX_SLEW_SEL, 1, RT_VOLATILE, {});
RT_REG_DECL(CPS8851_REG_PD_OPT_DB_IBIAS_EN, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_UNLOCK_PW_2, 2, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_EFUSE5, 1, RT_VOLATILE, {});

static const rt_register_map_t rt1711_chip_regmap[] = {
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
	RT_REG(TCPC_V10_REG_RX_DATA),
	RT_REG(TCPC_V10_REG_TRANSMIT),
	RT_REG(TCPC_V10_REG_TX_BYTE_CNT),
	RT_REG(RT1711H_REG_CONFIG_GPIO0),
	RT_REG(RT1711H_REG_PHY_CTRL1),
	RT_REG(RT1711H_REG_CLK_CTRL2),
	RT_REG(RT1711H_REG_CLK_CTRL3),
	RT_REG(RT1711H_REG_PRL_FSM_RESET),
	RT_REG(RT1711H_REG_BMC_CTRL),
	RT_REG(RT1711H_REG_BMCIO_RXDZSEL),
	RT_REG(RT1711H_REG_VCONN_CLIMITEN),
	RT_REG(RT1711H_REG_RT_STATUS),
	RT_REG(RT1711H_REG_RT_INT),
	RT_REG(RT1711H_REG_RT_MASK),
	RT_REG(RT1711H_REG_IDLE_CTRL),
	RT_REG(RT1711H_REG_INTRST_CTRL),
	RT_REG(RT1711H_REG_WATCHDOG_CTRL),
	RT_REG(RT1711H_REG_I2CRST_CTRL),
	RT_REG(RT1711H_REG_SWRESET),
	RT_REG(RT1711H_REG_TTCPC_FILTER),
	RT_REG(RT1711H_REG_DRP_TOGGLE_CYCLE),
	RT_REG(RT1711H_REG_DRP_DUTY_CTRL),
	RT_REG(CPS8851_REG_CP_OTSD),
	RT_REG(CPS8851_REG_CP_OTSD_MASK),
	RT_REG(RT1711H_REG_BMCIO_RXDZEN),
	RT_REG(CPS8851_REG_PASSWORD),
	RT_REG(CPS8851_REG_PD_OPT_ZTX_SEL),
	RT_REG(CPS8851_REG_PD_OPT_TX_SLEW_SEL),
	RT_REG(CPS8851_REG_PD_OPT_DB_IBIAS_EN),
	RT_REG(RT1711H_REG_UNLOCK_PW_2),
	RT_REG(RT1711H_REG_EFUSE5),
};
#define RT1711_CHIP_REGMAP_SIZE ARRAY_SIZE(rt1711_chip_regmap)

#endif /* CONFIG_RT_REGMAP */

static inline bool chip_is_cps8851(struct rt1711_chip *chip)
{
	return chip->chip_pid == CPS_8851_PID && chip->chip_vid == CPS_8851_VID;
}

#if CPS8851_I2C_WDT_EN
static void cps8851_start_wdt_timer(struct i2c_client *client)
{
	ktime_t t;

	struct rt1711_chip *chip = NULL;

	if (!client) {
		pr_err("%s, client is null\n", __func__);
		return;
	}

	chip = i2c_get_clientdata(client);
	if (!chip) {
		pr_err("%s, chip is null\n", __func__);
		return;
	}

	if (!chip_is_cps8851(chip))
		return;

	if (!chip->wdt_en) {
		pr_err("%s, wdt disabled\n", __func__);
		return;
	}

	t = ktime_set(CPS8851_I2C_WDT_KICK_TIME / MSEC_PER_SEC,
		(CPS8851_I2C_WDT_KICK_TIME % MSEC_PER_SEC) * NSEC_PER_MSEC);

	hrtimer_start(&chip->wdt_timer, t, HRTIMER_MODE_REL);
}
#endif

static int rt1711_read_device(void *client, u32 reg, int len, void *dst)
{
	struct i2c_client *i2c = client;
	int ret = 0, count = 5;
	u64 t1 = 0, t2 = 0;

	while (1) {
		t1 = local_clock();
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
		t2 = local_clock();
		RT1711_INFO("%s del = %lluus, reg = 0x%02X, len = %d\n",
			    __func__, (t2 - t1) / NSEC_PER_USEC, reg, len);
		if (ret < 0 && count > 1)
			count--;
		else
			break;
		udelay(100);
	}

#if CPS8851_I2C_WDT_EN
	cps8851_start_wdt_timer(i2c);
#endif
	return ret;
}

static int rt1711_write_device(void *client, u32 reg, int len, const void *src)
{
	struct i2c_client *i2c = client;
	int ret = 0, count = 5;
	u64 t1 = 0, t2 = 0;

	while (1) {
		t1 = local_clock();
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, src);
		t2 = local_clock();
		RT1711_INFO("%s del = %lluus, reg = %02X, len = %d\n",
			    __func__, (t2 - t1) / NSEC_PER_USEC, reg, len);
		if (ret < 0 && count > 1)
			count--;
		else
			break;
		udelay(100);
	}

#if CPS8851_I2C_WDT_EN
	cps8851_start_wdt_timer(i2c);
#endif
	return ret;
}

static int rt1711_reg_read(struct i2c_client *i2c, u8 reg)
{
	struct rt1711_chip *chip = i2c_get_clientdata(i2c);
	u8 val = 0;
	int ret = 0;

#ifdef  CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->m_dev, reg, 1, &val);
#else
	ret = rt1711_read_device(chip->client, reg, 1, &val);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0) {
		dev_err(chip->dev, "rt1711 reg read fail\n");
		return ret;
	}
	return val;
}

static int rt1711_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
	struct rt1711_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;

#ifdef  CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->m_dev, reg, 1, &data);
#else
	ret = rt1711_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "rt1711 reg write fail\n");
	return ret;
}

static int rt1711_block_read(struct i2c_client *i2c,
			u8 reg, int len, void *dst)
{
	struct rt1711_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#ifdef  CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->m_dev, reg, len, dst);
#else
	ret = rt1711_read_device(chip->client, reg, len, dst);
#endif /* #ifdef  CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "rt1711 block read fail\n");
	return ret;
}

static int rt1711_block_write(struct i2c_client *i2c,
			u8 reg, int len, const void *src)
{
	struct rt1711_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#ifdef  CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->m_dev, reg, len, src);
#else
	ret = rt1711_write_device(chip->client, reg, len, src);
#endif /* #ifdef  CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "rt1711 block write fail\n");
	return ret;
}

static int32_t rt1711_write_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t data)
{
	int ret;

	/* don't need swap */
	ret = rt1711_block_write(client, reg_addr, 2, (uint8_t *)&data);
	return ret;
}

static int32_t rt1711_read_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t *data)
{
	int ret;

	/* don't need swap */
	ret = rt1711_block_read(client, reg_addr, 2, (uint8_t *)data);
	return ret;
}

static inline int rt1711_i2c_write8(
	struct tcpc_device *tcpc, u8 reg, const u8 data)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	return rt1711_reg_write(chip->client, reg, data);
}

static inline int rt1711_i2c_write16(
		struct tcpc_device *tcpc, u8 reg, const u16 data)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	return rt1711_write_word(chip->client, reg, data);
}

static inline int rt1711_i2c_read8(struct tcpc_device *tcpc, u8 reg)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	return rt1711_reg_read(chip->client, reg);
}

static inline int rt1711_i2c_read16(
	struct tcpc_device *tcpc, u8 reg)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	u16 data;
	int ret;

	ret = rt1711_read_word(chip->client, reg, &data);
	if (ret < 0)
		return ret;
	return data;
}

#ifdef  CONFIG_RT_REGMAP
static struct rt_regmap_fops rt1711_regmap_fops = {
	.read_device = rt1711_read_device,
	.write_device = rt1711_write_device,
};
#endif /* CONFIG_RT_REGMAP */

static int rt1711_regmap_init(struct rt1711_chip *chip)
{
#ifdef  CONFIG_RT_REGMAP
	struct rt_regmap_properties *props;
	char name[32];
	int len;

	props = devm_kzalloc(chip->dev, sizeof(*props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	props->register_num = RT1711_CHIP_REGMAP_SIZE;
	props->rm = rt1711_chip_regmap;

	props->rt_regmap_mode = RT_MULTI_BYTE |
				RT_IO_PASS_THROUGH | RT_DBG_SPECIAL;
	snprintf(name, sizeof(name), "rt1711-%02x", chip->client->addr);

	len = strlen(name);
	props->name = kzalloc(len+1, GFP_KERNEL);
	props->aliases = kzalloc(len+1, GFP_KERNEL);

	if ((!props->name) || (!props->aliases))
		return -ENOMEM;

	strlcpy((char *)props->name, name, len+1);
	strlcpy((char *)props->aliases, name, len+1);
	props->io_log_en = 0;

	chip->m_dev = rt_regmap_device_register(props,
			&rt1711_regmap_fops, chip->dev, chip->client, chip);
	if (!chip->m_dev) {
		dev_err(chip->dev, "rt1711 chip rt_regmap register fail\n");
		return -EINVAL;
	}
#endif
	return 0;
}

static int rt1711_regmap_deinit(struct rt1711_chip *chip)
{
#ifdef  CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->m_dev);
#endif
	return 0;
}

static inline int rt1711_software_reset(struct tcpc_device *tcpc)
{
	int ret = rt1711_i2c_write8(tcpc, RT1711H_REG_SWRESET, 1);
#ifdef  CONFIG_RT_REGMAP
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_RT_REGMAP */

	if (ret < 0)
		return ret;
#ifdef  CONFIG_RT_REGMAP
	rt_regmap_cache_reload(chip->m_dev);
#endif /* CONFIG_RT_REGMAP */
	usleep_range(1000, 2000);
	return 0;
}

static inline int rt1711_command(struct tcpc_device *tcpc, uint8_t cmd)
{
	return rt1711_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, cmd);
}

static int rt1711_init_vbus_cal(struct tcpc_device *tcpc)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	const u8 val_en_test_mode[] = {0x86, 0x62};
	const u8 val_dis_test_mode[] = {0x00, 0x00};
	int ret = 0;
	u8 data = 0;
	s8 cal = 0;

	ret = rt1711_block_write(chip->client, RT1711H_REG_UNLOCK_PW_2,
			ARRAY_SIZE(val_en_test_mode), val_en_test_mode);
	if (ret < 0)
		dev_notice(chip->dev, "%s en test mode fail(%d)\n",
				__func__, ret);

	ret = rt1711_reg_read(chip->client, RT1711H_REG_EFUSE5);
	if (ret < 0)
		goto out;

	data = ret;
	data = (data & RT1711H_REG_M_VBUS_CAL) >> RT1711H_REG_S_VBUS_CAL;
	cal = (data & BIT(2)) ? (data | GENMASK(7, 3)) : data;
	cal -= 2;
	if (cal < RT1711H_REG_MIN_VBUS_CAL)
		cal = RT1711H_REG_MIN_VBUS_CAL;
	data = (cal << RT1711H_REG_S_VBUS_CAL) | (ret & GENMASK(4, 0));

	ret = rt1711_reg_write(chip->client, RT1711H_REG_EFUSE5, data);
out:
	ret = rt1711_block_write(chip->client, RT1711H_REG_UNLOCK_PW_2,
			ARRAY_SIZE(val_dis_test_mode), val_dis_test_mode);
	if (ret < 0)
		dev_notice(chip->dev, "%s dis test mode fail(%d)\n",
				__func__, ret);

	return ret;
}

static int rt1711_init_alert_mask(struct tcpc_device *tcpc)
{
	uint16_t mask;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;

#ifdef  CONFIG_USB_POWER_DELIVERY
	/* Need to handle RX overflow */
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS | TCPC_V10_REG_ALERT_TX_DISCARDED
			| TCPC_V10_REG_ALERT_TX_FAILED
			| TCPC_V10_REG_ALERT_RX_HARD_RST
			| TCPC_V10_REG_ALERT_RX_STATUS
			| TCPC_V10_REG_RX_OVERFLOW;
#endif

	mask |= TCPC_REG_ALERT_FAULT;

	return rt1711_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, mask);
}

static int rt1711_init_power_status_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask = TCPC_V10_REG_POWER_STATUS_VBUS_PRES;

	return rt1711_i2c_write8(tcpc,
			TCPC_V10_REG_POWER_STATUS_MASK, mask);
}

static int rt1711_init_fault_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask =
		TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
		TCPC_V10_REG_FAULT_STATUS_VCONN_OC;

	return rt1711_i2c_write8(tcpc,
			TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static int rt1711_init_rt_mask(struct tcpc_device *tcpc)
{
	uint8_t rt_mask = 0;
#ifdef CONFIG_TCPC_WATCHDOG_EN
	rt_mask |= RT1711H_REG_M_WATCHDOG;
#endif /* CONFIG_TCPC_WATCHDOG_EN */
	rt_mask |= RT1711H_REG_M_VBUS_80;

#ifdef CONFIG_TYPEC_CAP_RA_DETACH
	if (tcpc->tcpc_flags & TCPC_FLAGS_CHECK_RA_DETACHE)
		rt_mask |= RT1711H_REG_M_RA_DETACH;
#endif /* CONFIG_TYPEC_CAP_RA_DETACH */

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
	if (tcpc->tcpc_flags & TCPC_FLAGS_LPM_WAKEUP_WATCHDOG)
		rt_mask |= RT1711H_REG_M_WAKEUP;
#endif	/* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

#if CPS8851_OTP_EN
	rt1711_i2c_write8(tcpc, CPS8851_REG_CP_OTSD_MASK,
		CPS8851_REG_M_OTSD_STATE);
#endif

	return rt1711_i2c_write8(tcpc, RT1711H_REG_RT_MASK, rt_mask);
}

static int cps8851_init_mask(struct tcpc_device *tcpc)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	if (!chip_is_cps8851(chip))
		return 0;

	rt1711_init_alert_mask(tcpc);
	rt1711_init_power_status_mask(tcpc);
	rt1711_init_fault_mask(tcpc);
	rt1711_init_rt_mask(tcpc);

	return 0;
}

static void rt1711_irq_work_handler(struct kthread_work *work)
{
	struct rt1711_chip *chip =
			container_of(work, struct rt1711_chip, irq_work);
	int regval = 0;
	int gpio_val;

	/* make sure I2C bus had resumed */
	down(&chip->suspend_lock);

	tcpci_lock_typec(chip->tcpc);

#ifdef DEBUG_GPIO
	gpio_set_value(DEBUG_GPIO, 1);
#endif

	do {
		regval = tcpci_alert(chip->tcpc);
		if (regval)
			break;
		gpio_val = gpio_get_value(chip->irq_gpio);
	} while (gpio_val == 0);

	tcpci_unlock_typec(chip->tcpc);
	up(&chip->suspend_lock);

#ifdef DEBUG_GPIO
	gpio_set_value(DEBUG_GPIO, 1);
#endif
}

static irqreturn_t rt1711_intr_handler(int irq, void *data)
{
	struct rt1711_chip *chip = data;

	pm_wakeup_event(chip->dev, RT1711H_IRQ_WAKE_TIME);

#ifdef DEBUG_GPIO
	gpio_set_value(DEBUG_GPIO, 0);
#endif
	kthread_queue_work(&chip->irq_worker, &chip->irq_work);

	return IRQ_HANDLED;
}

static int rt1711_init_alert(struct tcpc_device *tcpc)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret = 0;
	char *name = NULL;

	/* Clear Alert Mask & Status */
	rt1711_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, 0);
	rt1711_write_word(chip->client, TCPC_V10_REG_ALERT, 0xffff);

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

	ret = gpio_to_irq(chip->irq_gpio);
	if (ret < 0) {
		dev_notice(chip->dev, "%s gpio to irq fail(%d)",
				      __func__, ret);
		return ret;
	}
	chip->irq = ret;

	dev_info(chip->dev, "%s IRQ number = %d\n", __func__, chip->irq);

	kthread_init_worker(&chip->irq_worker);
	chip->irq_worker_task = kthread_run(kthread_worker_fn,
			&chip->irq_worker, "%s", chip->tcpc_desc->name);

	if (IS_ERR(chip->irq_worker_task)) {
		pr_err("Error: Could not create tcpc task\n");
		return -EINVAL;
	}

	sched_setscheduler(chip->irq_worker_task, SCHED_FIFO, &param);
	kthread_init_work(&chip->irq_work, rt1711_irq_work_handler);

	pr_info("IRQF_NO_THREAD Test\n");
	ret = request_irq(chip->irq, rt1711_intr_handler,
		IRQF_TRIGGER_FALLING | IRQF_NO_THREAD, name, chip);
	if (ret < 0) {
		pr_err("Error: failed to request irq%d (gpio = %d, ret = %d)\n",
			chip->irq, chip->irq_gpio, ret);
		return ret;
	}

	device_init_wakeup(chip->dev, true);
	enable_irq_wake(chip->irq);

	return 0;
}

int rt1711_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
	int ret;
	uint16_t mask_t1;
	uint8_t mask_t2;

	mask_t1 = mask;
	if (mask_t1) {
		ret = rt1711_i2c_write16(tcpc, TCPC_V10_REG_ALERT, mask_t1);
		if (ret < 0)
			return ret;
	}

	mask_t2 = mask >> 16;
	if (mask_t2) {
		ret = rt1711_i2c_write8(tcpc, RT1711H_REG_RT_INT, mask_t2);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int rt1711h_set_clock_gating(struct tcpc_device *tcpc, bool en)
{
	int ret = 0;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

#ifdef CONFIG_TCPC_CLOCK_GATING
	int i = 0;
	uint8_t clk2 = RT1711H_REG_CLK_DIV_600K_EN
		| RT1711H_REG_CLK_DIV_300K_EN | RT1711H_REG_CLK_CK_300K_EN;
	uint8_t clk3 = RT1711H_REG_CLK_DIV_2P4M_EN;

	if (!en) {
		clk2 |=
			RT1711H_REG_CLK_BCLK2_EN | RT1711H_REG_CLK_BCLK_EN;
		clk3 |=
			RT1711H_REG_CLK_CK_24M_EN | RT1711H_REG_CLK_PCLK_EN;
	}

	if (en) {
		for (i = 0; i < 2; i++)
			ret = rt1711_alert_status_clear(tcpc,
				TCPC_REG_ALERT_RX_ALL_MASK);
	}

	if (!chip_is_cps8851(chip)) {
		if (ret == 0)
			ret = rt1711_i2c_write8(tcpc, RT1711H_REG_CLK_CTRL2, clk2);
		if (ret == 0)
			ret = rt1711_i2c_write8(tcpc, RT1711H_REG_CLK_CTRL3, clk3);
	}
#endif	/* CONFIG_TCPC_CLOCK_GATING */

	return ret;
}

static inline int rt1711h_init_cc_params(
			struct tcpc_device *tcpc, uint8_t cc_res)
{
	int rv = 0;

#ifdef  CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC
	uint8_t en, sel;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	if (cc_res == TYPEC_CC_VOLT_SNK_DFT) {	/* 0.55 */
		en = 0;
		sel = 0x81;
	} else if (chip->chip_id >= RT1715_DID_D) {	/* 0.35 & 0.75 */
		en = 1;
		sel = 0x81;
	} else {	/* 0.4 & 0.7 */
		en = 1;
		sel = 0x80;
	}

	rv = rt1711_i2c_write8(tcpc, RT1711H_REG_BMCIO_RXDZEN, en);
	if (rv == 0)
		rv = rt1711_i2c_write8(tcpc, RT1711H_REG_BMCIO_RXDZSEL, sel);
#endif	/* CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC */
#endif	/* CONFIG_USB_POWER_DELIVERY */

	return rv;
}

#if CPS8851_SOFTWARE_TRIM_EN
static int cps8851_trim(struct tcpc_device *tcpc)
{
	int ret;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	if (!chip_is_cps8851(chip))
		return 0;

	ret = rt1711_i2c_write8(tcpc, CPS8851_REG_PASSWORD, 0x51);
	/* Modify TRIM values. */
	ret += rt1711_i2c_write8(tcpc, CPS8851_REG_PD_OPT_ZTX_SEL, 0x09);
	ret += rt1711_i2c_write8(tcpc, CPS8851_REG_PASSWORD, 0x00);

	if (ret < 0) {
		dev_err(&tcpc->dev, "CPS8851 update trim value fail\n");
		return -EIO;
	}

	return ret;
}
#endif /* CPS8851_SOFTWARE_TRIM_EN */

#if CPS8851_I2C_WDT_EN
static void cps8851_init_i2c_wdt(struct tcpc_device *tcpc)
{
	int data;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	if (!chip_is_cps8851(chip))
		return;

	data = rt1711_i2c_read8(tcpc, RT1711H_REG_IDLE_CTRL);
	if (data < 0)
		return;
	data |= (CPS8851_I2C_WDT_TOUT << CPS8851_REG_WATCHDOG_TO_SHIFT);
	rt1711_i2c_write8(tcpc, RT1711H_REG_IDLE_CTRL, data);

	data = rt1711_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return;
	data |= TCPC_V10_REG_TCPC_CTRL_EN_WDT;
	chip->wdt_en = true;
	rt1711_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif

static int rt1711_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	bool retry_discard_old = false;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	RT1711_INFO("\n");

	if (sw_reset) {
		ret = rt1711_software_reset(tcpc);
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_TCPC_I2CRST_EN
	rt1711_i2c_write8(tcpc,
		RT1711H_REG_I2CRST_CTRL,
		RT1711H_REG_I2CRST_SET(true, 0x0f));
#endif	/* CONFIG_TCPC_I2CRST_EN */

	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	rt1711_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL,
		TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));

	if (chip->chip_id == RT1711H_DID_A) {
		rt1711_i2c_write8(tcpc, TCPC_V10_REG_FAULT_CTRL,
			TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV);
	}

	/*
	 * CC Detect Debounce : 26.7*val us
	 * Transition window count : spec 12~20us, based on 2.4MHz
	 * DRP Toggle Cycle : 51.2 + 6.4*val ms
	 * DRP Duty Ctrl : dcSRC / 1024
	 */

	rt1711_i2c_write8(tcpc, RT1711H_REG_TTCPC_FILTER, 10);
	rt1711_i2c_write8(tcpc, RT1711H_REG_DRP_TOGGLE_CYCLE, 4);
	rt1711_i2c_write16(tcpc,
		RT1711H_REG_DRP_DUTY_CTRL, TCPC_NORMAL_RP_DUTY);

	/* RX/TX Clock Gating (Auto Mode)*/
	if (!sw_reset)
		rt1711h_set_clock_gating(tcpc, true);

	if (!(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD))
		retry_discard_old = true;

	rt1711_i2c_write8(tcpc, RT1711H_REG_CONFIG_GPIO0, 0x80);

	/* For BIST, Change Transition Toggle Counter (Noise) from 3 to 7 */
	if (!chip_is_cps8851(chip)) {
		rt1711_i2c_write8(tcpc, RT1711H_REG_PHY_CTRL1,
			RT1711H_REG_PHY_CTRL1_SET(retry_discard_old, 7, 0, 1));
	}

	tcpci_alert_status_clear(tcpc, 0xffffffff);

	rt1711_init_vbus_cal(tcpc);
	rt1711_init_power_status_mask(tcpc);
	rt1711_init_alert_mask(tcpc);
	rt1711_init_fault_mask(tcpc);
	rt1711_init_rt_mask(tcpc);

	/* CK_300K from 320K, SHIPPING off, AUTOIDLE enable, TIMEOUT = 6.4ms */
	rt1711_i2c_write8(tcpc, RT1711H_REG_IDLE_CTRL,
		CPS8851_REG_IDLE_SET(0, 1, 1, 0, CPS8851_I2C_WDT_TOUT));
	mdelay(1);

#if CPS8851_SOFTWARE_TRIM_EN
	cps8851_trim(tcpc);
#endif

#if CPS8851_I2C_WDT_EN
	cps8851_init_i2c_wdt(tcpc);
#endif
#ifdef CONFIG_WATER_DETECTION
	chip->wd_state = CPS_WD_STATE_DRY;
	chip->wd_count = 0;
	chip->is_wet = false;
#endif
	return 0;
}

static inline int rt1711_fault_status_vconn_ov(struct tcpc_device *tcpc)
{
	int ret;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	if (chip_is_cps8851(chip))
		return 0;

	ret = rt1711_i2c_read8(tcpc, RT1711H_REG_BMC_CTRL);
	if (ret < 0)
		return ret;

	ret &= ~RT1711H_REG_DISCHARGE_EN;
	return rt1711_i2c_write8(tcpc, RT1711H_REG_BMC_CTRL, ret);
}

int rt1711_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
	int ret;

	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OV)
		ret = rt1711_fault_status_vconn_ov(tcpc);

	rt1711_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
	return 0;
}

int rt1711_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
	int ret;
	uint8_t v2;

	ret = rt1711_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK);
	if (ret < 0)
		return ret;

	*mask = (uint16_t) ret;

	ret = rt1711_i2c_read8(tcpc, RT1711H_REG_RT_MASK);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*mask |= v2 << 16;

	return 0;
}

int rt1711_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
	int ret;
	uint8_t v2;
#if CPS8851_OTP_EN
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
#endif

	ret = rt1711_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
	if (ret < 0)
		return ret;

	*alert = (uint16_t) ret;

	ret = rt1711_i2c_read8(tcpc, RT1711H_REG_RT_INT);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*alert |= v2 << 16;

#if CPS8851_OTP_EN
	ret = rt1711_i2c_read8(tcpc, CPS8851_REG_CP_OTSD);
	if (ret) {
		rt1711_i2c_write8(tcpc, CPS8851_REG_CP_OTSD_MASK, 0);
		cancel_delayed_work(&chip->otp_work);
		schedule_delayed_work(&chip->otp_work,
			msecs_to_jiffies(CPS8851_OTP_RECOVERY_TIME));
	}
#endif

	return 0;
}

static int cps8851_get_power_status(
		struct tcpc_device *tcpc, uint16_t *pwr_status)
{
	int ret;

#if CPS8851_VBUS_PRES_DEB_TIME
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	bool falling = false;
#endif

	ret = rt1711_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
	if (ret < 0)
		return ret;

	*pwr_status = 0;

	if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

#if CPS8851_VBUS_PRES_DEB_TIME
	if (tcpc->vbus_level == TCPC_VBUS_VALID &&
		!(ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES))
		falling = true;
#endif

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = rt1711_i2c_read8(tcpc, CPS8851_REG_CP_STATUS);
#if CPS8851_VBUS_PRES_DEB_TIME
	if (ret < 0)
		goto out;
#else
	if (ret < 0)
		return ret;
#endif

	if (ret & CPS8851_REG_VBUS_80) {
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;
#if CPS8851_VBUS_PRES_DEB_TIME
		falling = false;
#endif
	}

#endif
	ret = 0;

#if CPS8851_VBUS_PRES_DEB_TIME
out:
	if (chip->check_real_vbus) {
		chip->falling = false;
		return ret;
	}

	if (falling && !chip->falling) {
		dev_info(chip->dev, "sched deferred power_change\n");
		cancel_delayed_work(&chip->power_change_work);
		schedule_delayed_work(&chip->power_change_work,
			msecs_to_jiffies(CPS8851_VBUS_PRES_DEB_TIME));
	} else if ((*pwr_status & (TCPC_REG_POWER_STATUS_VBUS_PRES |
		TCPC_REG_POWER_STATUS_EXT_VSAFE0V)) && chip->falling) {
		dev_info(chip->dev, "cancel deferred power_change\n");
		cancel_delayed_work(&chip->power_change_work);
	}

	if (falling) {
		dev_info(chip->dev, "fake vbus_pres\n");
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;
	}

	chip->falling = falling;
#endif
	return ret;
}

static int rt1711_get_power_status(
		struct tcpc_device *tcpc, uint16_t *pwr_status)
{
	int ret;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	if (chip_is_cps8851(chip))
		return cps8851_get_power_status(tcpc, pwr_status);

	ret = rt1711_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
	if (ret < 0)
		return ret;

	*pwr_status = 0;

	if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

	ret = rt1711_i2c_read8(tcpc, RT1711H_REG_RT_STATUS);
	if (ret < 0)
		return ret;

	if (ret & RT1711H_REG_VBUS_80)
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;

	return 0;
}


int rt1711_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{
	int ret;

	ret = rt1711_i2c_read8(tcpc, TCPC_V10_REG_FAULT_STATUS);
	if (ret < 0)
		return ret;
	*status = (uint8_t) ret;
	return 0;
}

static int rt1711_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	int status, role_ctrl, cc_role;
	bool act_as_sink, act_as_drp;

	status = rt1711_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS);
	if (status < 0)
		return status;

	role_ctrl = rt1711_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
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

	rt1711h_init_cc_params(tcpc,
		(uint8_t)tcpc->typec_polarity ? *cc2 : *cc1);

	return 0;
}

static int rt1711_enable_vsafe0v_detect(
	struct tcpc_device *tcpc, bool enable)
{
	int ret = rt1711_i2c_read8(tcpc, RT1711H_REG_RT_MASK);

	if (ret < 0)
		return ret;

	if (enable)
		ret |= RT1711H_REG_M_VBUS_80;
	else
		ret &= ~RT1711H_REG_M_VBUS_80;

	return rt1711_i2c_write8(tcpc, RT1711H_REG_RT_MASK, (uint8_t) ret);
}

static inline int swap_rp_rd(int cc)
{
	if (cc == CC_RD)
		cc = CC_RP;
	else if (cc == CC_RP)
		cc = CC_RD;

	return cc;
}

static int rt1711_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret;
	uint8_t data;
	int role_ctrl;
	int cc1, cc2;
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	RT1711_INFO("pull = 0x%02X\n", pull);
	pull = TYPEC_CC_PULL_GET_RES(pull);
	if (pull == TYPEC_CC_DRP) {
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(
				1, rp_lvl, TYPEC_CC_RD, TYPEC_CC_RD);

#ifdef CONFIG_WATER_DETECTION
			if (chip->wd_count) {
				if (ktime_ms_delta(ktime_get(),
					chip->last_set_cc_toggle_time) > CPS8851_WD_INTERVAL) {
					chip->wd_count = 0;
				} else if (chip->wd_count >= CPS8851_WD_CNT_THRESHOLD){
					chip->is_wet = true;
					schedule_delayed_work(&chip->wd_work, 0);
				}
			}
			++chip->wd_count;
			chip->last_set_cc_toggle_time = ktime_get();
#endif
		ret = rt1711_i2c_write8(
			tcpc, TCPC_V10_REG_ROLE_CTRL, data);

		if (ret == 0) {
			rt1711_enable_vsafe0v_detect(tcpc, false);
			ret = rt1711_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
		}
	} else {
#ifdef  CONFIG_USB_POWER_DELIVERY
		if (pull == TYPEC_CC_RD && tcpc->pd_wait_pr_swap_complete)
			rt1711h_init_cc_params(tcpc, TYPEC_CC_VOLT_SNK_DFT);
#endif	/* CONFIG_USB_POWER_DELIVERY */

		pull1 = pull2 = pull;

		if (pull == TYPEC_CC_RP && tcpc->typec_is_attached_src) {
			if (tcpc->typec_polarity)
				pull1 = TYPEC_CC_OPEN;
			else
				pull2 = TYPEC_CC_OPEN;
		}
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull1, pull2);

		/*
		 * Swap Rp/Rd to trigger a CC change alert when setting both CCs open.
		 */
		if (chip_is_cps8851(chip)) {
			role_ctrl = rt1711_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);

			if (role_ctrl >= 0 && !(role_ctrl & TCPC_V10_REG_ROLE_CTRL_DRP)) {
				cc1 = role_ctrl & 0x03;
				cc2 = (role_ctrl >> 2) & 0x03;
				if ((cc1 != CC_OPEN || cc2 != CC_OPEN) &&
					(pull1 == TYPEC_CC_OPEN && pull2 == TYPEC_CC_OPEN)) {
					cc1 = swap_rp_rd(cc1);
					cc2 = swap_rp_rd(cc2);
					role_ctrl &= ~0xf;
					role_ctrl |= TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, cc1, cc2);
					rt1711_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, role_ctrl);
					mdelay(1);
				}
			}
		}

		ret = rt1711_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
	}

	return 0;
}

static int rt1711_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	int data;

	if (polarity >= 0 && polarity < ARRAY_SIZE(tcpc->typec_remote_cc)) {
		data = rt1711h_init_cc_params(tcpc,
			tcpc->typec_remote_cc[polarity]);
		if (data)
			return data;
	}

	data = rt1711_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT;
	data |= polarity ? TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT : 0;

	return rt1711_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}

static int rt1711_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
	uint16_t duty = low_rp ? TCPC_LOW_RP_DUTY : TCPC_NORMAL_RP_DUTY;

	return rt1711_i2c_write16(tcpc, RT1711H_REG_DRP_DUTY_CTRL, duty);
}

static int rt1711_set_vconn(struct tcpc_device *tcpc, int enable)
{
	int rv;
	int data;

	data = rt1711_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
	data |= enable ? TCPC_V10_REG_POWER_CTRL_VCONN : 0;

	rv = rt1711_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data);
	if (rv < 0)
		return rv;

	return rt1711_i2c_write8(tcpc, RT1711H_REG_IDLE_CTRL,
		CPS8851_REG_IDLE_SET(0, 1, enable ? 0 : 1, 0, CPS8851_I2C_WDT_TOUT));
}

#ifdef CONFIG_TCPC_LOW_POWER_MODE
static int rt1711_is_low_power_mode(struct tcpc_device *tcpc)
{
	int rv = rt1711_i2c_read8(tcpc, RT1711H_REG_BMC_CTRL);

	if (rv < 0)
		return rv;

	return (rv & RT1711H_REG_BMCIO_LPEN) != 0;
}

static int rt1711_set_low_power_mode(
		struct tcpc_device *tcpc, bool en, int pull)
{
	int ret = 0;
	uint8_t data;

	ret = rt1711_i2c_write8(tcpc, RT1711H_REG_IDLE_CTRL,
		CPS8851_REG_IDLE_SET(0, 1, en ? 0 : 1, 0, CPS8851_I2C_WDT_TOUT));
	if (ret < 0)
		return ret;
	rt1711_enable_vsafe0v_detect(tcpc, !en);
	if (en) {
		data = RT1711H_REG_BMCIO_LPEN;

		if (pull & TYPEC_CC_RP)
			data |= RT1711H_REG_BMCIO_LPRPRD;

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
		data |= RT1711H_REG_BMCIO_BG_EN | RT1711H_REG_VBUS_DET_EN;
#endif
	} else {
		data = RT1711H_REG_BMCIO_BG_EN |
			RT1711H_REG_VBUS_DET_EN | RT1711H_REG_BMCIO_OSC_EN;
	}

	return rt1711_i2c_write8(tcpc, RT1711H_REG_BMC_CTRL, data);
}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN
int rt1711h_set_watchdog(struct tcpc_device *tcpc, bool en)
{
	uint8_t data = RT1711H_REG_WATCHDOG_CTRL_SET(en, 7);

	return rt1711_i2c_write8(tcpc,
		RT1711H_REG_WATCHDOG_CTRL, data);
}
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#ifdef CONFIG_TCPC_INTRST_EN
int rt1711h_set_intrst(struct tcpc_device *tcpc, bool en)
{
	return rt1711_i2c_write8(tcpc,
		RT1711H_REG_INTRST_CTRL, RT1711H_REG_INTRST_SET(en, 3));
}
#endif	/* CONFIG_TCPC_INTRST_EN */

static int rt1711_tcpc_deinit(struct tcpc_device *tcpc)
{
#ifdef  CONFIG_RT_REGMAP
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_RT_REGMAP */

#ifdef CONFIG_TCPC_SHUTDOWN_CC_DETACH
	rt1711_set_cc(tcpc, TYPEC_CC_DRP);
	rt1711_set_cc(tcpc, TYPEC_CC_OPEN);

	rt1711_i2c_write8(tcpc,
		RT1711H_REG_I2CRST_CTRL,
		RT1711H_REG_I2CRST_SET(true, 4));

	rt1711_i2c_write8(tcpc,
		RT1711H_REG_INTRST_CTRL,
		RT1711H_REG_INTRST_SET(true, 0));
#else
	rt1711_i2c_write8(tcpc, RT1711H_REG_SWRESET, 1);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */
#ifdef  CONFIG_RT_REGMAP
	rt_regmap_cache_reload(chip->m_dev);
#endif /* CONFIG_RT_REGMAP */

	return 0;
}

#ifdef  CONFIG_USB_POWER_DELIVERY
static int rt1711_set_msg_header(
	struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
	uint8_t msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(
		data_role, power_role);

	return rt1711_i2c_write8(
		tcpc, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int rt1711_protocol_reset(struct tcpc_device *tcpc)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	if (!chip_is_cps8851(chip)) {
		rt1711_i2c_write8(tcpc, RT1711H_REG_PRL_FSM_RESET, 0);
		mdelay(1);
		rt1711_i2c_write8(tcpc, RT1711H_REG_PRL_FSM_RESET, 1);
	}

	return 0;
}

static int rt1711_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
	int ret = 0;

	if (enable)
		ret = rt1711h_set_clock_gating(tcpc, false);

	if (ret == 0)
		ret = rt1711_i2c_write8(tcpc, TCPC_V10_REG_RX_DETECT, enable);

	if ((ret == 0) && (!enable)) {
		rt1711_protocol_reset(tcpc);
		ret = rt1711h_set_clock_gating(tcpc, true);
	}

	return ret;
}

static int rt1711_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int rv = 0;
	uint8_t cnt = 0, buf[4];

	rv = rt1711_block_read(chip->client, TCPC_V10_REG_RX_BYTE_CNT, 4, buf);
	if (rv < 0)
		return rv;

	cnt = buf[0];
	*frame_type = buf[1];
	*msg_head = le16_to_cpu(*(uint16_t *)&buf[2]);

	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		rv = rt1711_block_read(chip->client, TCPC_V10_REG_RX_DATA, cnt,
				       payload);
	}

	/* Read complete, clear RX status alert bit */
	if (chip_is_cps8851(chip))
		tcpci_alert_status_clear(tcpc, TCPC_V10_REG_ALERT_RX_STATUS |
										TCPC_V10_REG_RX_OVERFLOW);

	return rv;
}

static int rt1711_set_bist_carrier_mode(
	struct tcpc_device *tcpc, uint8_t pattern)
{
	/* Don't support this function */
	return 0;
}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
static int rt1711_retransmit(struct tcpc_device *tcpc)
{
	return rt1711_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
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

static int rt1711_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	int data_cnt;
	struct tcpc_transmit_packet packet;

	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = sizeof(uint32_t) * PD_HEADER_CNT(header);

		packet.cnt = data_cnt + sizeof(uint16_t);
		packet.msg_header = header;

		if (data_cnt > 0)
			memcpy(packet.data, (uint8_t *) data, data_cnt);

		rv = rt1711_block_write(chip->client,
				TCPC_V10_REG_TX_BYTE_CNT,
				packet.cnt+1, (uint8_t *) &packet);
		if (rv < 0)
			return rv;
	}

	rv = rt1711_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, type));

	if (type == TCPC_TX_HARD_RESET)
		cps8851_init_mask(tcpc);

#ifdef PD_DYNAMIC_SENDER_RESPONSE
	tcpc->t[0] = local_clock();
#endif
	return rv;
}

static int rt1711_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	int data;

	data = rt1711_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
	data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;

	return rt1711_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_WATER_DETECTION
static int cps8851_is_water_detected(struct tcpc_device *tcpc)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	return chip->is_wet;
}

static int cps8851_set_water_protection(struct tcpc_device *tcpc, bool en)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	if (en) {
		chip->wd_state = CPS_WD_STATE_WET_PROTECTION;
		chip->cc_open = false;
	} else {
		chip->wd_state = CPS_WD_STATE_DRY;
		chip->is_wet = false;
	}

	return 0;
}
#endif /* CONFIG_WATER_DETECTION */

static struct tcpc_ops rt1711_tcpc_ops = {
	.init = rt1711_tcpc_init,
	.init_alert_mask = cps8851_init_mask,
	.alert_status_clear = rt1711_alert_status_clear,
	.fault_status_clear = rt1711_fault_status_clear,
	.get_alert_mask = rt1711_get_alert_mask,
	.get_alert_status = rt1711_get_alert_status,
	.get_power_status = rt1711_get_power_status,
	.get_fault_status = rt1711_get_fault_status,
	.get_cc = rt1711_get_cc,
	.set_cc = rt1711_set_cc,
	.set_polarity = rt1711_set_polarity,
	.set_low_rp_duty = rt1711_set_low_rp_duty,
	.set_vconn = rt1711_set_vconn,
	.deinit = rt1711_tcpc_deinit,

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	.is_low_power_mode = rt1711_is_low_power_mode,
	.set_low_power_mode = rt1711_set_low_power_mode,
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN
	.set_watchdog = rt1711h_set_watchdog,
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#ifdef CONFIG_TCPC_INTRST_EN
	.set_intrst = rt1711h_set_intrst,
#endif	/* CONFIG_TCPC_INTRST_EN */

#ifdef  CONFIG_USB_POWER_DELIVERY
	.set_msg_header = rt1711_set_msg_header,
	.set_rx_enable = rt1711_set_rx_enable,
	.protocol_reset = rt1711_protocol_reset,
	.get_message = rt1711_get_message,
	.transmit = rt1711_transmit,
	.set_bist_test_mode = rt1711_set_bist_test_mode,
	.set_bist_carrier_mode = rt1711_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = rt1711_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_WATER_DETECTION
	.is_water_detected = cps8851_is_water_detected,
	.set_water_protection = cps8851_set_water_protection,
#endif /* CONFIG_WATER_DETECTION */
};

static int rt_parse_dt(struct rt1711_chip *chip, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	pr_info("%s\n", __func__);

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "cps8851,intr_gpio", 0);
	if (ret < 0)
		pr_err("%s no intr_gpio info\n", __func__);
	else
		chip->irq_gpio = ret;
#else
	ret = of_property_read_u32(np,
		"cps8851,intr_gpio_num", &chip->irq_gpio);
	if (ret < 0)
		pr_err("%s no intr_gpio info\n", __func__);
#endif /* !CONFIG_MTK_GPIO || CONFIG_MTK_GPIOLIB_STAND */
	return ret < 0 ? ret : 0;
}

/*
 * In some platform pr_info may spend too much time on printing debug message.
 * So we use this function to test the printk performance.
 * If your platform cannot not pass this check function, please config
 * PD_DBG_INFO, this will provide the threaded debug message for you.
 */
#ifdef TCPC_ENABLE_ANYMSG
static void check_printk_performance(void)
{
	int i;
	u64 t1, t2;
	u32 nsrem;

#ifdef  CONFIG_PD_DBG_INFO
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

static int rt1711_tcpcdev_init(struct rt1711_chip *chip, struct device *dev)
{
	struct tcpc_desc *desc;
	struct device_node *np = dev->of_node;
	u32 val, len;
	const char *name = "default";

	dev_info(dev, "%s\n", __func__);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	if (of_property_read_u32(np, "cps-tcpc,role_def", &val) >= 0) {
		if (val >= TYPEC_ROLE_NR)
			desc->role_def = TYPEC_ROLE_DRP;
		else
			desc->role_def = val;
	} else {
		dev_info(dev, "use default Role DRP\n");
		desc->role_def = TYPEC_ROLE_DRP;
	}

	if (of_property_read_u32(np, "cps-tcpc,rp_level", &val) >= 0) {
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
	if (of_property_read_u32(np, "cps-tcpc,vconn_supply", &val) >= 0) {
		if (val >= TCPC_VCONN_SUPPLY_NR)
			desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
		else
			desc->vconn_supply = val;
	} else {
		dev_info(dev, "use default VconnSupply\n");
		desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	if (of_property_read_string(np, "cps-tcpc,name",
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
			desc, &rt1711_tcpc_ops, chip);
	if (IS_ERR_OR_NULL(chip->tcpc))
		return -EINVAL;

#ifdef CONFIG_USB_PD_DISABLE_PE
	chip->tcpc->disable_pe =
			of_property_read_bool(np, "rt-tcpc,disable_pe");
#endif	/* CONFIG_USB_PD_DISABLE_PE */

	chip->tcpc->tcpc_flags = TCPC_FLAGS_LPM_WAKEUP_WATCHDOG |
			TCPC_FLAGS_VCONN_SAFE5V_ONLY;

	if (chip->chip_id > RT1711H_DID_B)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_CHECK_RA_DETACHE;

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	if (chip->chip_id > RT1715_DID_D)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif  /* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_USB_PD_REV30
	if (chip->chip_id >= RT1715_DID_D)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;

	if (chip->tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
		dev_info(dev, "PD_REV30\n");
	else
		dev_info(dev, "PD_REV20\n");
#endif	/* CONFIG_USB_PD_REV30 */
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_ALERT_V10;

#ifdef CONFIG_WATER_DETECTION
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_WATER_DETECTION;
#endif

	return 0;
}

static bool id_in_table(u16 id, const u16 *table, size_t size)
{
	int i;

	for (i = 0; i < size; ++i)
		if (id == table[i])
			return true;
	return false;
}

static const u16 supported_vids[] = {
	RICHTEK_1711_VID,
	CPS_8851_VID,
};

static const u16 supported_pids[] = {
	RICHTEK_1711_PID,
	CPS_8851_PID,
};

static inline int rt1711h_check_revision(struct i2c_client *client,
			int *chip_vid, int *chip_pid, int *chip_did)
{
	u16 vid, pid, did;
	int ret;
	u8 data = 1;

	ret = rt1711_read_device(client, TCPC_V10_REG_VID, 2, &vid);
	if (ret < 0) {
		dev_err(&client->dev, "read chip ID fail\n");
		return -EIO;
	}

	if (!id_in_table(vid, supported_vids, ARRAY_SIZE(supported_vids))) {
		pr_info("%s failed, VID=0x%04x\n", __func__, vid);
		return -ENODEV;
	}

	ret = rt1711_read_device(client, TCPC_V10_REG_PID, 2, &pid);
	if (ret < 0) {
		dev_err(&client->dev, "read product ID fail\n");
		return -EIO;
	}

	if (!id_in_table(pid, supported_pids, ARRAY_SIZE(supported_pids))) {
		pr_info("%s failed, PID=0x%04x\n", __func__, pid);
		return -ENODEV;
	}

	ret = rt1711_write_device(client, RT1711H_REG_SWRESET, 1, &data);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);

	ret = rt1711_read_device(client, TCPC_V10_REG_DID, 2, &did);
	if (ret < 0) {
		dev_err(&client->dev, "read device ID fail\n");
		return -EIO;
	}

	if (chip_vid)
		*chip_vid = vid;

	if (chip_pid)
		*chip_pid = pid;

	if (chip_did)
		*chip_did = did;

	return ret;
}

#if CPS8851_OTP_EN
static void cps8851_otp_work(struct work_struct *work)
{
	struct rt1711_chip *chip = container_of(
		work, struct rt1711_chip, otp_work.work);

	tcpci_lock_typec(chip->tcpc);
	tcpci_set_vconn(chip->tcpc, 0);
	rt1711_i2c_write8(chip->tcpc, CPS8851_REG_CP_OTSD, CPS8851_REG_OTSD_STATE);
	rt1711_i2c_write8(chip->tcpc, CPS8851_REG_CP_OTSD_MASK,
		CPS8851_REG_M_OTSD_STATE);
	tcpci_unlock_typec(chip->tcpc);
}
#endif

#if CPS8851_VBUS_PRES_DEB_TIME
static void cps8851_power_change_work(struct work_struct *work)
{
	struct rt1711_chip *chip = container_of(
		work, struct rt1711_chip, power_change_work.work);

	dev_info(chip->dev, "deferred power_change\n");
	tcpci_lock_typec(chip->tcpc);
	chip->check_real_vbus = true;
	tcpci_alert_power_status_changed(chip->tcpc);
	chip->check_real_vbus = false;
	tcpci_unlock_typec(chip->tcpc);
}
#endif

#if CPS8851_I2C_WDT_EN
static enum hrtimer_restart cps8851_watchdog_timer(struct hrtimer *timer)
{
	struct rt1711_chip *chip = NULL;

	if (!timer) {
		pr_err("%s timer is null\n", __func__);
		return HRTIMER_NORESTART;
	}

	chip = container_of(timer, struct rt1711_chip, wdt_timer);
	if(!chip) {
		pr_err("%s chip is null\n", __func__);
		return HRTIMER_NORESTART;
	}

	pm_wakeup_event(chip->dev, 200);
	schedule_work(&chip->wdt_work);

	return HRTIMER_NORESTART;
}

static void cps8851_watchdog_work(struct work_struct *work)
{
	struct rt1711_chip *chip = container_of(work, struct rt1711_chip,
						wdt_work);

	down(&chip->suspend_lock);
	tcpci_lock_typec(chip->tcpc);
	rt1711_i2c_read8(chip->tcpc, TCPC_V10_REG_POWER_STATUS);
	tcpci_unlock_typec(chip->tcpc);
	up(&chip->suspend_lock);
}
#endif /* CPS8851_I2C_WDT_EN */

#ifdef CONFIG_WATER_DETECTION
static enum alarmtimer_restart cps8851_wd_wakeup(struct alarm *alarm,
						ktime_t now)
{
	struct rt1711_chip *chip =
		container_of(alarm, struct rt1711_chip, wd_wakeup_timer);

	pm_wakeup_event(chip->dev, 500);
	schedule_delayed_work(&chip->wd_work, 0);
	return ALARMTIMER_NORESTART;
}

static void cps8851_wd_work(struct work_struct *work)
{
	struct rt1711_chip *chip = container_of(
		work, struct rt1711_chip, wd_work.work);
	u32 delay = 0;
	int cc1, cc2;
	int ret;

	down(&chip->suspend_lock);
	tcpci_lock_typec(chip->tcpc);
	dev_info(chip->dev, "%s wd_state = %d, cnt=%d\n", __func__, chip->wd_state,
		chip->wd_count);
	switch (chip->wd_state)
	{
	case CPS_WD_STATE_DRY:
		if (chip->is_wet) {
			tcpc_typec_handle_wd(chip->tcpc, true);
			delay = CPS8851_WD_OPEN_INTERVAL;
		}
		break;
	case CPS_WD_STATE_WET_PROTECTION:
		if (chip->cc_open) {
			/* Try to toggle the CC. */
			chip->cc_open = false;
			delay = CPS8851_WD_TRY_INTERVAL;
			rt1711_set_cc(chip->tcpc, TYPEC_CC_DRP);
			break;
		}

		ret = rt1711_get_cc(chip->tcpc, &cc1, &cc2);
		if (ret < 0) {
			delay = CPS8851_WD_TRY_INTERVAL;
			break;
		}

		if (cc1 == TYPEC_CC_DRP_TOGGLING) {
			/* CC toggling indicates that the port is dry. */
			chip->is_wet = 0;
			tcpc_typec_handle_wd(chip->tcpc, false);
			break;
		} else {
			/* Since the port remains wet, keep the CC open to prevent rusting */
			chip->cc_open = true;
			rt1711_set_cc(chip->tcpc, TYPEC_CC_OPEN);
			delay = CPS8851_WD_OPEN_INTERVAL;
		}

		break;
	default:
		break;
	}

	chip->wd_count = 0;
	tcpci_unlock_typec(chip->tcpc);
	up(&chip->suspend_lock);

	if (!delay)
		return;

	alarm_start_relative(&chip->wd_wakeup_timer,
				     ktime_set(delay / MSEC_PER_SEC, (delay % MSEC_PER_SEC) * USEC_PER_SEC));
}
#endif /* CONFIG_WATER_DETECTION */

/* A06 code for SR-AL7160A-01-270 by shixuanxuan at 20240611 start */
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
    struct rt1711_chip *chip = container_of(psy->desc, struct rt1711_chip, psd);

    switch (psp) {
    case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
        ret = rt1711_get_cc(chip->tcpc, &cc1, &cc2);
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
/* A06 code for SR-AL7160A-01-270 by shixuanxuan at 20240611 end */

static int rt1711_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct rt1711_chip *chip;
	int ret = 0;
	int chip_vid, chip_pid, chip_did;
	bool use_dt = client->dev.of_node;

	pr_info("%s SXX3 (%s)\n", __func__, RT1711H_DRV_VERSION);
	if (i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA))
		pr_info("I2C functionality : OK...\n");
	else
		pr_info("I2C functionality check : failuare...\n");

	ret = rt1711h_check_revision(client, &chip_vid, &chip_pid, &chip_did);
	if (ret < 0)
		return ret;

#ifdef TCPC_ENABLE_ANYMSG
	check_printk_performance();
#endif /* TCPC_ENABLE_ANYMSG */

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (use_dt) {
		ret = rt_parse_dt(chip, &client->dev);
		if (ret < 0)
			return ret;
	} else {
		dev_err(&client->dev, "no dts node\n");
		return -ENODEV;
	}
	chip->dev = &client->dev;
	chip->client = client;
	sema_init(&chip->suspend_lock, 1);
	i2c_set_clientdata(client, chip);

#if CPS8851_OTP_EN
	INIT_DELAYED_WORK(&chip->otp_work, cps8851_otp_work);
#endif
#if CPS8851_VBUS_PRES_DEB_TIME
	INIT_DELAYED_WORK(&chip->power_change_work, cps8851_power_change_work);
#endif
#ifdef CONFIG_WATER_DETECTION
	INIT_DELAYED_WORK(&chip->wd_work, cps8851_wd_work);
	alarm_init(&chip->wd_wakeup_timer, ALARM_REALTIME, cps8851_wd_wakeup);
#endif
#if CPS8851_I2C_WDT_EN
	INIT_WORK(&chip->wdt_work, cps8851_watchdog_work);
	hrtimer_init(&chip->wdt_timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->wdt_timer.function = cps8851_watchdog_timer;
#endif
	chip->chip_vid = chip_vid;
	chip->chip_pid = chip_pid;
	chip->chip_id = chip_did;
	pr_info("rt1711h_chipID = 0x%0x\n", chip_did);

	ret = rt1711_regmap_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "rt1711 regmap init fail\n");
		goto err_regmap_init;
	}

	ret = rt1711_tcpcdev_init(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "rt1711 tcpc dev init fail\n");
		goto err_tcpc_reg;
	}

	ret = rt1711_init_alert(chip->tcpc);
	if (ret < 0) {
		pr_err("rt1711 init alert fail\n");
		goto err_irq_init;
	}

	/* A06 code for SR-AL7160A-01-270 by shixuanxuan at 20240611 start */
	chip->psd.name = "typec_port";
	chip->psd.properties = typec_port_properties;
	chip->psd.type = POWER_SUPPLY_TYPE_USB_TYPE_C;
	chip->psd.num_properties = ARRAY_SIZE(typec_port_properties);
	chip->psd.get_property = typec_port_get_property;
	chip->typec_psy = devm_power_supply_register(chip->dev, &chip->psd, NULL);
	tcpc_info = CPS8851;
	if (IS_ERR(chip->typec_psy)) {
		pr_err("failed to register power supply: %ld\n",
		PTR_ERR(chip->typec_psy));
	}
	/* A06 code for SR-AL7160A-01-270 by shixuanxuan at 20240611 end */

	pr_info("%s probe OK!\n", __func__);
	return 0;

err_irq_init:
	tcpc_device_unregister(chip->dev, chip->tcpc);
err_tcpc_reg:
	rt1711_regmap_deinit(chip);
err_regmap_init:
	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
static void rt1711_i2c_remove(struct i2c_client *client)
#else
static int rt1711_i2c_remove(struct i2c_client *client)
#endif
{
	struct rt1711_chip *chip = i2c_get_clientdata(client);

	if (chip) {
#if CPS8851_OTP_EN
		cancel_delayed_work_sync(&chip->otp_work);
#endif
#if CPS8851_VBUS_PRES_DEB_TIME
		cancel_delayed_work_sync(&chip->power_change_work);
#endif
#ifdef CONFIG_WATER_DETECTION
		cancel_delayed_work_sync(&chip->wd_work);
		alarm_cancel(&chip->wd_wakeup_timer);
#endif
#if CPS8851_I2C_WDT_EN
		hrtimer_cancel(&chip->wdt_timer);
		cancel_work_sync(&chip->wdt_work);
#endif
		tcpc_device_unregister(chip->dev, chip->tcpc);
		rt1711_regmap_deinit(chip);
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,1,0))
	return 0;
#endif
}

#ifdef  CONFIG_PM
#ifdef  CONFIG_PM_SLEEP
static int rt1711_i2c_suspend(struct device *dev)
{
	struct rt1711_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			down(&chip->suspend_lock);
	}

	return 0;
}

static int rt1711_i2c_resume(struct device *dev)
{
	struct rt1711_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			up(&chip->suspend_lock);
	}

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static void rt1711_shutdown(struct i2c_client *client)
{
	struct rt1711_chip *chip = i2c_get_clientdata(client);

	/* Please reset IC here */
	if (chip != NULL) {
		if (chip->irq)
			disable_irq(chip->irq);
		tcpm_shutdown(chip->tcpc);
		if (chip_is_cps8851(chip))
			mdelay(25);
	} else {
		i2c_smbus_write_byte_data(
			client, RT1711H_REG_SWRESET, 0x01);
	}
}

#ifdef  CONFIG_PM_RUNTIME
static int rt1711_pm_suspend_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: suspending...\n");
	return 0;
}

static int rt1711_pm_resume_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: resuming...\n");
	return 0;
}
#endif /* CONFIG_PM_RUNTIME */

static const struct dev_pm_ops rt1711_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			rt1711_i2c_suspend,
			rt1711_i2c_resume)
#ifdef  CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(
		rt1711_pm_suspend_runtime,
		rt1711_pm_resume_runtime,
		NULL
	)
#endif /* CONFIG_PM_RUNTIME */
};
#define RT1711_PM_OPS	(&rt1711_pm_ops)
#else
#define RT1711_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id rt1711_id_table[] = {
	{"rt1711h", 0},
	{"rt1715", 0},
	{"rt1716", 0},
	{"cps8851", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, rt1711_id_table);

static const struct of_device_id rt_match_table[] = {
	{.compatible = "richtek,rt1711h",},
	{.compatible = "richtek,rt1715",},
	{.compatible = "richtek,rt1716",},
	{.compatible = "cps,cps8851",},
	{},
};

static struct i2c_driver rt1711_driver = {
	.driver = {
		.name = "rt1711h",
		.owner = THIS_MODULE,
		.of_match_table = rt_match_table,
		.pm = RT1711_PM_OPS,
	},
	.probe = rt1711_i2c_probe,
	.remove = rt1711_i2c_remove,
	.shutdown = rt1711_shutdown,
	.id_table = rt1711_id_table,
};

static int __init rt1711_init(void)
{
	struct device_node *np;

	pr_info("%s (%s)\n", __func__, RT1711H_DRV_VERSION);
	np = of_find_node_by_name(NULL, "rt1711h");
	pr_info("%s rt1711h node %s\n", __func__,
		np == NULL ? "not found" : "found");

	return i2c_add_driver(&rt1711_driver);
}
subsys_initcall(rt1711_init);

static void __exit rt1711_exit(void)
{
	i2c_del_driver(&rt1711_driver);
}
module_exit(rt1711_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("RT1711 TCPC Driver");
MODULE_VERSION(RT1711H_DRV_VERSION);

/**** Release Note ****
 * 2.0.6_MTK
 * (1) Revert Vconn OC to shutdown mode
 * (2) Revise IRQ handling
 *
 * 2.0.5_MTK
 * (1) Utilize rt-regmap to reduce I2C accesses
 * (2) Decrease VBUS present threshold (VBUS_CAL) by 60mV (2LSBs)
 *
 * 2.0.4_MTK
 * (1) Mask vSafe0V IRQ before entering low power mode
 * (2) Disable auto idle mode before entering low power mode
 * (3) Reset Protocol FSM and clear RX alerts twice before clock gating
 *
 * 2.0.3_MTK
 * (1) Single Rp as Attatched.SRC for Ellisys TD.4.9.4
 *
 * 2.0.2_MTK
 * (1) Replace wake_lock with wakeup_source
 * (2) Move down the shipping off
 * (3) Add support for NoRp.SRC
 * (4) Reg0x71[7] = 1'b1 to workaround unstable VDD Iq in low power mode
 * (5) Add get_alert_mask of tcpc_ops
 *
 * 2.0.1_MTK
 * First released PD3.0 Driver on MTK platform
 */
