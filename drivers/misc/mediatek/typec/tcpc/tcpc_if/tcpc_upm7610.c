// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Unisemipower Inc.
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

#include "../inc/pd_dbg_info.h"
#include "../inc/tcpci.h"
#include "../inc/upm7610.h"

#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* CONFIG_RT_REGMAP */

#include <linux/sched/rt.h>
/* A06 code for SR-AL7160A-01-271 by shixuanxuan at 20240325 start */
#include <linux/power_supply.h>
/* A06 code for SR-AL7160A-01-271 by shixuanxuan at 20240325 end */

/* #define DEBUG_GPIO   66 */
/* A06 code for AL7160A-1630 by shixuanxuan at 20240615 start */
static int upm7610_enter_bist_test_mode(struct tcpc_device *tcpc, bool en);
/* A06 code for AL7160A-1630 by shixuanxuan at 20240615 end */
static int upm7610_set_bist_test_mode(struct tcpc_device *tcpc, bool en);
static int upm7610_set_shutdown_power_mode(struct tcpc_device *tcpc, bool en);

#define UPM7610_DRV_VERSION "1.0.3_UP"

#define UPM7610_IRQ_WAKE_TIME   (500) /* ms */

struct upm7610_chip {
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
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 start */
    struct mutex i2c_rw_lock;
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 end */

    atomic_t poll_count;
    struct delayed_work poll_work;

    int irq_gpio;
    int irq;
    int chip_id;
    int chip_func_sw;
    /* A06 code for SR-AL7160A-01-271 by shixuanxuan at 20240325 start */
    struct power_supply *typec_psy;
    struct power_supply_desc psd;
    /* A06 code for SR-AL7160A-01-271 by shixuanxuan at 20240325 end*/
};

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
RT_REG_DECL(TCPC_V10_REG_TX_BYTE_CNT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_HDR, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TX_DATA, 28, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(UPM7610_REG_CC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(UPM7610_REG_BMC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(UPM7610_REG_VDR_DEF_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(UPM7610_REG_VDR_DEF_ALERT, 1, RT_VOLATILE, {});
RT_REG_DECL(UPM7610_REG_VDR_DEF_ALERT_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(UPM7610_REG_RESET_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(UPM7610_REG_HIDDEN_MODE, 1, RT_VOLATILE, {});
RT_REG_DECL(UPM7610_REG_DEBUG_B9, 1, RT_VOLATILE, {});
RT_REG_DECL(UPM7610_REG_TRIM_R4, 1, RT_VOLATILE, {});
RT_REG_DECL(UPM7610_REG_TRIM_R5, 1, RT_VOLATILE, {});
RT_REG_DECL(UPM7610_REG_TRIM_C6, 1, RT_VOLATILE, {});

static const rt_register_map_t upm7610_chip_regmap[] = {
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
    RT_REG(UPM7610_REG_CC_CTRL),
    RT_REG(UPM7610_REG_BMC_CTRL),
    RT_REG(UPM7610_REG_VDR_DEF_STATUS),
    RT_REG(UPM7610_REG_VDR_DEF_ALERT),
    RT_REG(UPM7610_REG_VDR_DEF_ALERT_MASK),
    RT_REG(UPM7610_REG_RESET_CTRL),
    RT_REG(UPM7610_REG_HIDDEN_MODE),
    RT_REG(UPM7610_REG_DEBUG_B9),
    RT_REG(UPM7610_REG_TRIM_R4),
    RT_REG(UPM7610_REG_TRIM_R5),
    RT_REG(UPM7610_REG_TRIM_C6),
};
#define UPM7610_CHIP_REGMAP_SIZE ARRAY_SIZE(upm7610_chip_regmap)

#endif /* CONFIG_RT_REGMAP */

static int upm7610_read_device(void *client, u32 reg, int len, void *dst)
{
    struct i2c_client *i2c = client;
    int ret = 0, count = 5;
    u64 t1 = 0, t2 = 0;

    while (1) {
        t1 = local_clock();
        ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
        t2 = local_clock();
        if (len == 1) {
            UPM7610_INFO("%s del = %lluus, reg = %02X, len = %d, ret = 0x%02X, dst = 0x%02X\n",
                    __func__, (t2 - t1) / NSEC_PER_USEC, reg, len, ret, *(u8 *)dst);
        } else if (len == 2) {
            UPM7610_INFO("%s del = %lluus, reg = %02X, len = %d, ret = 0x%02X, dst[0] = 0x%02X, dst[1] = 0x%02X\n",
                    __func__, (t2 - t1) / NSEC_PER_USEC, reg, len, ret, *((u8 *)dst), *((u8 *)dst + 1));
        }
        if (ret < 0 && count > 1)
            count--;
        else
            break;
        udelay(100);
    }
    return ret;
}

static int upm7610_write_device(void *client, u32 reg, int len, const void *src)
{
    struct i2c_client *i2c = client;
    int ret = 0, count = 5;
    u64 t1 = 0, t2 = 0;

    while (1) {
        t1 = local_clock();
        ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, src);
        t2 = local_clock();
        UPM7610_INFO("%s del = %lluus, reg = %02X, len = %d\n",
                __func__, (t2 - t1) / NSEC_PER_USEC, reg, len);
        if (ret < 0 && count > 1)
            count--;
        else
            break;
        udelay(100);
    }
    return ret;
}

static int upm7610_reg_read(struct i2c_client *i2c, u8 reg)
{
    struct upm7610_chip *chip = i2c_get_clientdata(i2c);
    u8 val = 0;
    int ret = 0;

#ifdef CONFIG_RT_REGMAP
    ret = rt_regmap_block_read(chip->m_dev, reg, 1, &val);
#else
    ret = upm7610_read_device(chip->client, reg, 1, &val);
#endif /* CONFIG_RT_REGMAP */
    if (ret < 0) {
        dev_err(chip->dev, "upm7610 reg read fail\n");
        return ret;
    }
    return val;
}

static int upm7610_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
    struct upm7610_chip *chip = i2c_get_clientdata(i2c);
    int ret = 0;

#ifdef CONFIG_RT_REGMAP
    ret = rt_regmap_block_write(chip->m_dev, reg, 1, &data);
#else
    ret = upm7610_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
    if (ret < 0)
        dev_err(chip->dev, "upm7610 reg write fail\n");
    return ret;
}

static int upm7610_block_read(struct i2c_client *i2c,
            u8 reg, int len, void *dst)
{
    struct upm7610_chip *chip = i2c_get_clientdata(i2c);
    int ret = 0;
#ifdef CONFIG_RT_REGMAP
    ret = rt_regmap_block_read(chip->m_dev, reg, len, dst);
#else
    ret = upm7610_read_device(chip->client, reg, len, dst);
#endif /* #ifdef CONFIG_RT_REGMAP */
    if (ret < 0)
        dev_err(chip->dev, "upm7610 block read fail\n");
    return ret;
}

static int upm7610_block_write(struct i2c_client *i2c,
            u8 reg, int len, const void *src)
{
    struct upm7610_chip *chip = i2c_get_clientdata(i2c);
    int ret = 0;
#ifdef CONFIG_RT_REGMAP
    ret = rt_regmap_block_write(chip->m_dev, reg, len, src);
#else
    ret = upm7610_write_device(chip->client, reg, len, src);
#endif /* #ifdef CONFIG_RT_REGMAP */
    if (ret < 0)
        dev_err(chip->dev, "upm7610 block write fail\n");
    return ret;
}

static int32_t upm7610_write_word(struct i2c_client *client,
                    uint8_t reg_addr, uint16_t data)
{
    int ret;

    /* don't need swap */
    ret = upm7610_block_write(client, reg_addr, 2, (uint8_t *)&data);
    return ret;
}

static int32_t upm7610_read_word(struct i2c_client *client,
                    uint8_t reg_addr, uint16_t *data)
{
    int ret;

    /* don't need swap */
    ret = upm7610_block_read(client, reg_addr, 2, (uint8_t *)data);
    return ret;
}

static inline int upm7610_i2c_write8(
    struct tcpc_device *tcpc, u8 reg, const u8 data)
{
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);

    return upm7610_reg_write(chip->client, reg, data);
}

static inline int upm7610_i2c_write16(
        struct tcpc_device *tcpc, u8 reg, const u16 data)
{
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);

    return upm7610_write_word(chip->client, reg, data);
}

static inline int upm7610_i2c_read8(struct tcpc_device *tcpc, u8 reg)
{
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);

    return upm7610_reg_read(chip->client, reg);
}

static inline int upm7610_i2c_read16(
    struct tcpc_device *tcpc, u8 reg)
{
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);
    u16 data;
    int ret;

    ret = upm7610_read_word(chip->client, reg, &data);
    if (ret < 0)
        return ret;
    return data;
}

#ifdef CONFIG_RT_REGMAP
static struct rt_regmap_fops upm7610_regmap_fops = {
    .read_device = upm7610_read_device,
    .write_device = upm7610_write_device,
};
#endif /* CONFIG_RT_REGMAP */

static int upm7610_regmap_init(struct upm7610_chip *chip)
{
#ifdef CONFIG_RT_REGMAP
    struct rt_regmap_properties *props;
    char name[32];
    int len;

    props = devm_kzalloc(chip->dev, sizeof(*props), GFP_KERNEL);
    if (!props)
        return -ENOMEM;

    props->register_num = UPM7610_CHIP_REGMAP_SIZE;
    props->rm = upm7610_chip_regmap;

    props->rt_regmap_mode = RT_MULTI_BYTE |
                RT_IO_PASS_THROUGH | RT_DBG_SPECIAL;
    snprintf(name, sizeof(name), "upm7610-%02x", chip->client->addr);

    len = strlen(name);
    props->name = kzalloc(len+1, GFP_KERNEL);
    props->aliases = kzalloc(len+1, GFP_KERNEL);

    if ((!props->name) || (!props->aliases))
        return -ENOMEM;

    strlcpy((char *)props->name, name, len+1);
    strlcpy((char *)props->aliases, name, len+1);
    props->io_log_en = 0;

    chip->m_dev = rt_regmap_device_register(props,
            &upm7610_regmap_fops, chip->dev, chip->client, chip);
    if (!chip->m_dev) {
        dev_err(chip->dev, "upm7610 chip rt_regmap register fail\n");
        return -EINVAL;
    }
#endif
    return 0;
}

static int upm7610_regmap_deinit(struct upm7610_chip *chip)
{
#ifdef CONFIG_RT_REGMAP
    rt_regmap_device_unregister(chip->m_dev);
#endif
    return 0;
}

static inline int upm7610_software_reset(struct tcpc_device *tcpc)
{
    int ret = upm7610_i2c_write8(tcpc, UPM7610_REG_RESET_CTRL, 1);
#ifdef CONFIG_RT_REGMAP
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_RT_REGMAP */
    if (ret < 0)
        return ret;
#ifdef CONFIG_RT_REGMAP
    rt_regmap_cache_reload(chip->m_dev);
#endif /* CONFIG_RT_REGMAP */
    usleep_range(1000, 2000);
/*A06 code for AL7160A-1244 by shixuanxuan at 20240522 start*/
    upm7610_i2c_write8(tcpc, UPM7610_REG_RESET_CTRL, UPM7610_REG_EXT_MASK_CLEAR);
/*A06 code for AL7160A-1244 by shixuanxuan at 20240522 end*/
    return 0;
}

static inline int upm7610_command(struct tcpc_device *tcpc, uint8_t cmd)
{
    return upm7610_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, cmd);
}

static int upm7610_init_alert_mask(struct tcpc_device *tcpc)
{
    uint16_t mask;
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);

    mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;

#ifdef CONFIG_USB_POWER_DELIVERY
    /* Need to handle RX overflow */
    mask |= TCPC_V10_REG_ALERT_TX_SUCCESS | TCPC_V10_REG_ALERT_TX_DISCARDED
            | TCPC_V10_REG_ALERT_TX_FAILED
            | TCPC_V10_REG_ALERT_RX_HARD_RST
            | TCPC_V10_REG_ALERT_RX_STATUS
            | TCPC_V10_REG_RX_OVERFLOW
            /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 start */
            #ifndef HQ_FACTORY_BUILD
            | TCPC_V10_REG_EXTENDED_STATUS
            #endif
            /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 end */
            | TCPC_V10_REG_VBUS_SINK_DISCONNECT
            | TCPC_V10_REG_ALERT_VENDOR_DEFINED;
#endif

    mask |= TCPC_REG_ALERT_FAULT;

    UPM7610_INFO("mask:0x%x\n", mask);
    return upm7610_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, mask);
}

static int upm7610_rx_alert_mask(struct tcpc_device *tcpc)
{
    int ret;
    uint16_t mask;
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);

    UPM7610_INFO("upm7610: %s\n", __func__);

    ret = upm7610_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK);
    if (ret < 0)
        return ret;

    UPM7610_INFO("get_mask from reg:0x%x\n", ret);
    mask = (uint16_t) ret;

    mask &= ~TCPC_V10_REG_ALERT_RX_STATUS;
    mask &= ~TCPC_V10_REG_RX_OVERFLOW;

    UPM7610_INFO("mask:0x%x\n", mask);
    return upm7610_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, mask);
}

static int upm7610_init_power_status_mask(struct tcpc_device *tcpc)
{
    const uint8_t mask = TCPC_V10_REG_POWER_STATUS_VBUS_PRES;

    return upm7610_i2c_write8(tcpc,
            TCPC_V10_REG_POWER_STATUS_MASK, mask);
}

static int upm7610_init_fault_mask(struct tcpc_device *tcpc)
{
    const uint8_t mask =
        TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
        TCPC_V10_REG_FAULT_STATUS_VCONN_OC;

    return upm7610_i2c_write8(tcpc,
            TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static int upm7610_init_up_mask(struct tcpc_device *tcpc)
{
    uint8_t up_mask = 0;

    UPM7610_INFO("%s\n", __func__);
#ifdef CONFIG_TCPC_WATCHDOG_EN

#endif /* CONFIG_TCPC_WATCHDOG_EN */
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 start */
    #ifndef HQ_FACTORY_BUILD
    up_mask |= UPM7610_REG_VSAFE0V_STATUS_MASK;
    #endif
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 end */
    up_mask |= UPM7610_REG_REF_DISCNT_MASK;
#ifdef CONFIG_TYPEC_CAP_RA_DETACH
    if (tcpc->tcpc_flags & TCPC_FLAGS_CHECK_RA_DETACHE)
        up_mask |= UPM7610_REG_RA_DETACH_MASK;
#endif /* CONFIG_TYPEC_CAP_RA_DETACH */

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG

#endif  /* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

    return upm7610_i2c_write8(tcpc, UPM7610_REG_VDR_DEF_ALERT_MASK, up_mask);
}

static inline void upm7610_poll_ctrl(struct upm7610_chip *chip)
{
    cancel_delayed_work_sync(&chip->poll_work);

    if (atomic_read(&chip->poll_count) == 0) {
        atomic_inc(&chip->poll_count);
        cpu_idle_poll_ctrl(true);
    }

    schedule_delayed_work(
        &chip->poll_work, msecs_to_jiffies(40));
}

static void upm7610_irq_work_handler(struct kthread_work *work)
{
    struct upm7610_chip *chip =
            container_of(work, struct upm7610_chip, irq_work);
    int regval = 0;
    int gpio_val;

    upm7610_poll_ctrl(chip);
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

static void upm7610_poll_work(struct work_struct *work)
{
    struct upm7610_chip *chip = container_of(
        work, struct upm7610_chip, poll_work.work);

    if (atomic_dec_and_test(&chip->poll_count))
        cpu_idle_poll_ctrl(false);
}

static irqreturn_t upm7610_intr_handler(int irq, void *data)
{
    struct upm7610_chip *chip = data;

    __pm_wakeup_event(chip->irq_wake_lock, UPM7610_IRQ_WAKE_TIME);

#ifdef DEBUG_GPIO
    gpio_set_value(DEBUG_GPIO, 0);
#endif
    kthread_queue_work(&chip->irq_worker, &chip->irq_work);
    return IRQ_HANDLED;
}

static int upm7610_init_alert(struct tcpc_device *tcpc)
{
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);
    struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
    int ret;
    char *name;
    int len;

    /* Clear Alert Mask & Status */
    upm7610_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, 0);
    upm7610_write_word(chip->client, TCPC_V10_REG_ALERT, 0xffff);

    len = strlen(chip->tcpc_desc->name);
    name = devm_kzalloc(chip->dev, len+5, GFP_KERNEL);
    if (!name)
        return -ENOMEM;

    snprintf(name, PAGE_SIZE, "%s-IRQ", chip->tcpc_desc->name);

    pr_info("%s name = %s, gpio = %d\n", __func__,
                chip->tcpc_desc->name, chip->irq_gpio);

    ret = devm_gpio_request(chip->dev, chip->irq_gpio, name);
#ifdef DEBUG_GPIO
    gpio_request(DEBUG_GPIO, "debug_latency_pin");
    gpio_direction_output(DEBUG_GPIO, 1);
#endif
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
    kthread_init_work(&chip->irq_work, upm7610_irq_work_handler);

    pr_info("IRQF_NO_THREAD Test\n");
    ret = request_irq(chip->irq, upm7610_intr_handler,
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

int upm7610_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
    int ret;
    uint16_t mask_t1;
    uint8_t mask_t2;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

#endif

    /* Write 1 clear */
    mask_t1 = (uint16_t) mask;
    if (mask_t1) {
        ret = upm7610_i2c_write16(tcpc, TCPC_V10_REG_ALERT, mask_t1);
        if (ret < 0)
            return ret;
    }

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

#endif
    mask_t2 = mask >> 16;
    if (mask_t2) {
        ret = upm7610_i2c_write8(tcpc, UPM7610_REG_VDR_DEF_ALERT, mask_t2);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int upm7610_set_clock_gating(struct tcpc_device *tcpc, bool en)
{
    int ret = 0;

#ifdef CONFIG_TCPC_CLOCK_GATING
    int i = 0;

    if (en) {
        for (i = 0; i < 2; i++)
            ret = upm7610_alert_status_clear(tcpc,
                TCPC_REG_ALERT_RX_ALL_MASK);
    }
#endif  /* CONFIG_TCPC_CLOCK_GATING */

    return ret;
}

static inline int upm7610_init_cc_params(
            struct tcpc_device *tcpc, uint8_t cc_res)
{
    int rv = 0;

#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC
    uint8_t en, sel;
    //struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);

    if (cc_res == TYPEC_CC_VOLT_SNK_DFT) {  /* 0.55 */
        en = 0;
        sel = 0x81;
    } else {    /* 0.4 & 0.7 */
        en = 1;
        sel = 0x80;
    }

#endif  /* CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC */
#endif  /* CONFIG_USB_POWER_DELIVERY */

    return rv;
}

static int upm7610_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
    int ret;
    bool retry_discard_old = false;
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);

    int data = 0;
    uint8_t tmp1;

    UPM7610_INFO("\n");

    if (sw_reset) {
        ret = upm7610_software_reset(tcpc);
        if (ret < 0)
            return ret;
    }
    upm7610_set_shutdown_power_mode(tcpc, false);

#ifdef CONFIG_TCPC_I2CRST_EN

#endif  /* CONFIG_TCPC_I2CRST_EN */

    /* UFP Both RD setting */
    /* DRP = 0, RpVal = 0 (Default), Rd, Rd */
    upm7610_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL,
        TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));

    if (chip->chip_id == UPM7610_DID) {
        upm7610_i2c_write8(tcpc, TCPC_V10_REG_FAULT_CTRL,
            TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV);
        upm7610_command(tcpc, TCPM_CMD_ENABLE_VBUS_DETECT);
        UPM7610_INFO("TCPM_CMD_ENABLE_VBUS_DETECT\n");
    }

    /* RX/TX Clock Gating (Auto Mode)*/
    if (!sw_reset)
        upm7610_set_clock_gating(tcpc, true);

    if (!(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD))
        retry_discard_old = true;


    tcpci_alert_status_clear(tcpc, 0xffffffff);

    upm7610_init_power_status_mask(tcpc);
    upm7610_init_alert_mask(tcpc);
    upm7610_init_fault_mask(tcpc);
    upm7610_init_up_mask(tcpc);

#if 1 // optimization at 1101
    chip->chip_func_sw = 0;

    data = upm7610_i2c_read8(tcpc, UPM7610_REG_BMC_CTRL);
    if (data < 0) {
        pr_err("upm7610: read 0x93 fail, data:0x%x\n", data);
    }
    UPM7610_INFO("upm7610: reg[0x93]:0x%x\n", data);

    if (data != 0xFF) {
        chip->chip_func_sw =  2;
        UPM7610_INFO("upm7610: non pd phy\n");
        return 0;
    }

    upm7610_i2c_write8(tcpc, UPM7610_REG_HIDDEN_MODE, 0x6E);

    data = upm7610_i2c_read8(tcpc, UPM7610_REG_DEBUG_B9);
    if (data < 0) {
        pr_err("upm7610: read 0xB9 fail, data:0x%x\n", data);
    }
    UPM7610_INFO("upm7610: default trim data [0xB9]:0x%x\n", data);

    tmp1 = (uint8_t) (data & 0xF0) >> 4;

    if (tmp1 == 0x01) {
        chip->chip_func_sw =  1;
        upm7610_i2c_write8(tcpc, UPM7610_REG_HIDDEN_MODE, 0x0);
        UPM7610_INFO("upm7610: exit hidden mode\n");
        return 0;
    }

    data = upm7610_i2c_read8(tcpc, UPM7610_REG_TRIM_R5);
    if (data < 0) {
        pr_err("upm7610: read 0xC5 fail, data:0x%x\n", data);
    }
    UPM7610_INFO("upm7610: default trim data [0xC5]:0x%x\n", data);

    tmp1 = (uint8_t) data & 0x07;

    if (tmp1 != 3) {
        tmp1 = 0x03;
    }

    data = (data & 0xF8) | tmp1;
    UPM7610_INFO("upm7610: new trim data [0xC5]:0x%x\n", data);
    //data = 0x33;
    //UPM7610_INFO("upm7610: new trim data [0xC5]:0x%x\n", data);

    upm7610_i2c_write8(tcpc, UPM7610_REG_TRIM_R5, data);

    data = upm7610_i2c_read8(tcpc, UPM7610_REG_TRIM_C6);
    if (data < 0) {
        pr_err("upm7610: read 0xC6 fail, data:0x%x\n", data);
    }
    UPM7610_INFO("upm7610: default trim data [0xC6]:0x%x\n", data);

    tmp1 = (uint8_t) data & 0x03;
    if (tmp1 != 2)
        tmp1 = 0x02;

    data =  (data & 0xFC) | tmp1;
    UPM7610_INFO("upm7610: new trim data [0xC6]:0x%x\n", data);

    upm7610_i2c_write8(tcpc, UPM7610_REG_TRIM_C6, data);

    upm7610_i2c_write8(tcpc, UPM7610_REG_HIDDEN_MODE, 0x0);
#endif
    mdelay(1);

    return 0;
}

int upm7610_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
    upm7610_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
    return 0;
}

int upm7610_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
    int ret;
    uint8_t v2;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

#endif

    ret = upm7610_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK);
    if (ret < 0)
        return ret;

    UPM7610_INFO("get_mask from reg:0x%x\n", ret);

    *mask = (uint16_t) ret;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

#endif
    ret = upm7610_i2c_read8(tcpc, UPM7610_REG_VDR_DEF_ALERT_MASK);
    if (ret < 0)
        return ret;

    v2 = (uint8_t) ret;
    *mask |= v2 << 16;

    return 0;
}

int upm7610_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
    int ret;
    uint8_t v2;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

#endif

    ret = upm7610_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
    if (ret < 0)
        return ret;

    *alert = (uint16_t) ret;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

#endif
    ret = upm7610_i2c_read8(tcpc, UPM7610_REG_VDR_DEF_ALERT);
    if (ret < 0)
        return ret;

    v2 = (uint8_t) ret;
    *alert |= v2 << 16;

    return 0;
}

static int upm7610_get_power_status(
        struct tcpc_device *tcpc, uint16_t *pwr_status)
{
    int ret;

    ret = upm7610_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
    if (ret < 0)
        return ret;

    *pwr_status = 0;

    if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
        *pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

#endif
    ret = upm7610_i2c_read8(tcpc, UPM7610_REG_VDR_DEF_STATUS);
    if (ret < 0)
        return ret;

    if (ret & UPM7610_REG_VSAFE0V_STATUS)
        *pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;
    return 0;
}

int upm7610_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{
    int ret;

    ret = upm7610_i2c_read8(tcpc, TCPC_V10_REG_FAULT_STATUS);
    if (ret < 0)
        return ret;
    *status = (uint8_t) ret;
    return 0;
}

static int upm7610_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
    int status, role_ctrl, cc_role;
    bool act_as_sink, act_as_drp;

    status = upm7610_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS);
    if (status < 0)
        return status;

    role_ctrl = upm7610_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
    if (role_ctrl < 0)
        return role_ctrl;

    UPM7610_INFO("status:0x%x, role_ctrl:0x%x\n", status, role_ctrl);

#if 0
    if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
        *cc1 = TYPEC_CC_DRP_TOGGLING;
        *cc2 = TYPEC_CC_DRP_TOGGLING;
        return 0;
    }
#endif

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

    upm7610_init_cc_params(tcpc,
        (uint8_t)tcpc->typec_polarity ? *cc2 : *cc1);

    return 0;
}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

static int upm7610_set_cc(struct tcpc_device *tcpc, int pull)
{
    int ret;
    uint8_t data;
    int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2;

    UPM7610_INFO("pull = 0x%02X\n", pull);
    pull = TYPEC_CC_PULL_GET_RES(pull);
    if (pull == TYPEC_CC_DRP) {
        data = TCPC_V10_REG_ROLE_CTRL_RES_SET(
                1, rp_lvl, TYPEC_CC_RD, TYPEC_CC_RD);

        ret = upm7610_i2c_write8(
            tcpc, TCPC_V10_REG_ROLE_CTRL, data);

        if (ret == 0) {
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
            ret = upm7610_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
        }
    } else {
#ifdef CONFIG_USB_POWER_DELIVERY
        if (pull == TYPEC_CC_RD && tcpc->pd_wait_pr_swap_complete) {
            upm7610_init_cc_params(tcpc, TYPEC_CC_VOLT_SNK_DFT);
        }
#endif  /* CONFIG_USB_POWER_DELIVERY */

        pull1 = pull2 = pull;

        if (pull == TYPEC_CC_RP && tcpc->typec_is_attached_src) {
            if (tcpc->typec_polarity)
                pull1 = TYPEC_CC_OPEN;
            else
                pull2 = TYPEC_CC_OPEN;
        }
        data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull1, pull2);
        ret = upm7610_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
        UPM7610_INFO("data:%d, rp_lvl:%d, pull1:%d, pull2:%d\n",
                data, rp_lvl, pull1, pull2);
    }

    return 0;
}

static int upm7610_set_polarity(struct tcpc_device *tcpc, int polarity)
{
    int data;

    data = upm7610_init_cc_params(tcpc,
        tcpc->typec_remote_cc[polarity]);
    if (data)
        return data;

    data = upm7610_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
    if (data < 0)
        return data;

    data &= ~TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT;
    data |= polarity ? TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT : 0;

    return upm7610_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}

static int upm7610_set_vconn(struct tcpc_device *tcpc, int enable)
{
    int rv;
    int data;

    data = upm7610_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
    if (data < 0)
        return data;

    data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
    data |= enable ? TCPC_V10_REG_POWER_CTRL_VCONN : 0;

    rv = upm7610_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data);
    if (rv < 0)
        return rv;

    return 0;
}

#ifdef CONFIG_TCPC_LOW_POWER_MODE
#if 0
static int upm7610_is_low_power_mode(struct tcpc_device *tcpc)
{
    int rv = upm7610_i2c_read8(tcpc, UPM7610_REG_CC_CTRL);

    if (rv < 0)
        return rv;

    return (rv & UPM7610_REG_DISABLED_REQ) != 0;
}

static int upm7610_set_low_power_mode(
        struct tcpc_device *tcpc, bool en, int pull)
{
    int data = 0;

    data = upm7610_i2c_read8(tcpc, UPM7610_REG_CC_CTRL);
    if (data < 0) {
        pr_err("%s: read CC_CTRL fail, data:%d\n", __func__, data);
        return data;
    }

    UPM7610_INFO("r_data:0x%x", data);

    if (en) {
        data |= UPM7610_REG_DISABLED_REQ;
    } else {
        data &= ~UPM7610_REG_DISABLED_REQ;
    }

    UPM7610_INFO("w_data:0x%x", data);

    return upm7610_i2c_write8(tcpc, UPM7610_REG_CC_CTRL, data);
}
#endif
#endif  /* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN

#endif  /* CONFIG_TCPC_WATCHDOG_EN */

#ifdef CONFIG_TCPC_INTRST_EN

#endif  /* CONFIG_TCPC_INTRST_EN */

static int upm7610_set_shutdown_power_mode(struct tcpc_device *tcpc, bool en)
{
    int data = 0;

    data = upm7610_i2c_read8(tcpc, UPM7610_REG_CC_CTRL);
    if (data < 0) {
        pr_err("%s: read CC_CTRL fail, data:%d\n", __func__, data);
        return data;
    }

    UPM7610_INFO("upm7610: cc_ctrl:0x%x\n", data);

    if (en) {
        data |= UPM7610_REG_DISABLED_REQ;
        UPM7610_INFO("upm7610: enter shutdown mode, cc_ctrl:0x%x\n", data);
    } else {
        data &= ~UPM7610_REG_DISABLED_REQ;
        UPM7610_INFO("upm7610: exit shutdown mode, cc_ctrl:0x%x\n", data);
    }

    return upm7610_i2c_write8(tcpc, UPM7610_REG_CC_CTRL, data);
}

static int upm7610_tcpc_deinit(struct tcpc_device *tcpc)
{
#ifdef CONFIG_RT_REGMAP
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_RT_REGMAP */

#ifdef CONFIG_TCPC_SHUTDOWN_CC_DETACH
    upm7610_set_cc(tcpc, TYPEC_CC_DRP);
    upm7610_set_cc(tcpc, TYPEC_CC_OPEN);
    mdelay(100);
    upm7610_set_cc(tcpc, TYPEC_CC_RD);
    mdelay(10);
#else
    upm7610_i2c_write8(tcpc, UPM7610_REG_RESET_CTRL, 1);
#endif  /* CONFIG_TCPC_SHUTDOWN_CC_DETACH */
#ifdef CONFIG_RT_REGMAP
    rt_regmap_cache_reload(chip->m_dev);
#endif /* CONFIG_RT_REGMAP */
    upm7610_set_shutdown_power_mode(tcpc, true);

    return 0;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static int upm7610_set_msg_header(
    struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
    uint8_t msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(
        data_role, power_role);

    return upm7610_i2c_write8(
        tcpc, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int upm7610_protocol_reset(struct tcpc_device *tcpc)
{

    return 0;
}

static int upm7610_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
    int ret = 0;

    if (enable)
        ret = upm7610_set_clock_gating(tcpc, false);

    if (ret == 0)
        ret = upm7610_i2c_write8(tcpc, TCPC_V10_REG_RX_DETECT, enable);

    if ((ret == 0) && (!enable)) {
        upm7610_protocol_reset(tcpc);
        ret = upm7610_set_clock_gating(tcpc, true);
    }

    return ret;
}

static int upm7610_get_message(struct tcpc_device *tcpc, uint32_t *payload,
            uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);
    int rv;
    uint8_t type, cnt = 0;
    uint8_t buf[4];
    const uint16_t alert_rx =
        TCPC_V10_REG_ALERT_RX_STATUS|TCPC_V10_REG_RX_OVERFLOW;
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 start */
    uint16_t msg_head_tmp = 0;

    mutex_lock(&chip->i2c_rw_lock);
    rv = upm7610_i2c_read8(tcpc, TCPC_V10_REG_RX_BYTE_CNT);
    if (rv == 0x1F) {
        upm7610_enter_bist_test_mode(tcpc, true);
    }

    rv = upm7610_block_read(chip->client,
            TCPC_V10_REG_RX_BYTE_CNT, 4, buf);
    cnt = buf[0];
    type = buf[1];
    *msg_head = *(uint16_t *)&buf[2];
    msg_head_tmp = *msg_head & 0xF01F;

    if (msg_head_tmp == 0x7003) {
        upm7610_set_bist_test_mode(tcpc, true);
    } else {
        upm7610_enter_bist_test_mode(tcpc, false);
    }
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 end*/

    /* TCPC 1.0 ==> no need to subtract the size of msg_head */
    if (rv >= 0 && cnt > 3) {
        cnt -= 3; /* MSG_HDR */
        rv = upm7610_block_read(chip->client, TCPC_V10_REG_RX_DATA, cnt,
                (uint8_t *) payload);
    }

    *frame_type = (enum tcpm_transmit_type) type;

    /* Read complete, clear RX status alert bit */
    if (*msg_head != 0x77a3)
        tcpci_alert_status_clear(tcpc, alert_rx);
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 start */
    mutex_unlock(&chip->i2c_rw_lock);
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 end*/
    /*mdelay(1); */
    return rv;
}

static int upm7610_set_bist_carrier_mode(
    struct tcpc_device *tcpc, uint8_t pattern)
{
    /* Don't support this function */
    return 0;
}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
static int upm7610_retransmit(struct tcpc_device *tcpc)
{
    return upm7610_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
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

static int upm7610_transmit(struct tcpc_device *tcpc,
    enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);
    int rv;
    int data_cnt;
    struct tcpc_transmit_packet packet;

    if (type < TCPC_TX_HARD_RESET) {
        data_cnt = sizeof(uint32_t) * PD_HEADER_CNT(header);

        packet.cnt = data_cnt + sizeof(uint16_t);
        packet.msg_header = header;

        if (data_cnt > 0)
            memcpy(packet.data, (uint8_t *) data, data_cnt);

        rv = upm7610_block_write(chip->client,
                TCPC_V10_REG_TX_BYTE_CNT,
                packet.cnt+1, (uint8_t *) &packet);
        if (rv < 0)
            return rv;
    }

    rv = upm7610_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
            TCPC_V10_REG_TRANSMIT_SET(
            tcpc->pd_retry_count, type));
    return rv;
}
/* A06 code for AL7160A-1630 by shixuanxuan at 20240615 start */
static int upm7610_enter_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
    int data;

    data = upm7610_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
    if (data < 0) {
        return data;
    }

    data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
    data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;

    return upm7610_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
/* A06 code for AL7160A-1630 by shixuanxuan at 20240615 end */

static int upm7610_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
    struct upm7610_chip *chip = tcpc_get_dev_data(tcpc);
    int data;

    UPM7610_INFO("upm7610: %s\n", __func__);
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 start */
    data = upm7610_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
    if (data < 0) {
        return data;
    }

    data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
    data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;

    upm7610_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);

    if (en) {
        upm7610_rx_alert_mask(tcpc);
    } else {
        if (chip->chip_func_sw == 0) {
            upm7610_tcpc_init(tcpc, true);
            upm7610_set_cc(tcpc,TYPEC_CC_DRP);
        }
        upm7610_i2c_write16(tcpc, TCPC_V10_REG_ALERT, 0x0404);
        upm7610_i2c_write16(tcpc, TCPC_V10_REG_ALERT, 0x0404);
        upm7610_init_alert_mask(tcpc);
    }

    return 0;
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 end*/
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static struct tcpc_ops upm7610_tcpc_ops = {
    .init = upm7610_tcpc_init,
    .alert_status_clear = upm7610_alert_status_clear,
    .fault_status_clear = upm7610_fault_status_clear,
    .get_alert_mask = upm7610_get_alert_mask,
    .get_alert_status = upm7610_get_alert_status,
    .get_power_status = upm7610_get_power_status,
    .get_fault_status = upm7610_get_fault_status,
    .get_cc = upm7610_get_cc,
    .set_cc = upm7610_set_cc,
    .set_polarity = upm7610_set_polarity,
    //.set_low_rp_duty = upm7610_set_low_rp_duty,
    .set_vconn = upm7610_set_vconn,
    .deinit = upm7610_tcpc_deinit,
    .init_alert_mask = upm7610_init_alert_mask,

#ifdef CONFIG_TCPC_LOW_POWER_MODE
    //.is_low_power_mode = upm7610_is_low_power_mode,
    //.set_low_power_mode = upm7610_set_low_power_mode,
#endif  /* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN
    //.set_watchdog = upm7610_set_watchdog,
#endif  /* CONFIG_TCPC_WATCHDOG_EN */

#ifdef CONFIG_TCPC_INTRST_EN
    //.set_intrst = upm7610_set_intrst,
#endif  /* CONFIG_TCPC_INTRST_EN */

#ifdef CONFIG_USB_POWER_DELIVERY
    .set_msg_header = upm7610_set_msg_header,
    .set_rx_enable = upm7610_set_rx_enable,
    .protocol_reset = upm7610_protocol_reset,
    .get_message = upm7610_get_message,
    .transmit = upm7610_transmit,
    .set_bist_test_mode = upm7610_set_bist_test_mode,
    .set_bist_carrier_mode = upm7610_set_bist_carrier_mode,
#endif  /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
    .retransmit = upm7610_retransmit,
#endif  /* CONFIG_USB_PD_RETRY_CRC_DISCARD */
};

static int rt_parse_dt(struct upm7610_chip *chip, struct device *dev)
{
    struct device_node *np = NULL;
    int ret = 0;

    pr_info("%s\n", __func__);

    np = dev->of_node;

    ret = of_get_named_gpio(np, "upm7610pd,intr_gpio", 0);
    if (ret < 0) {
        pr_err("%s upm7610 get  intr_gpio info fail\n", __func__);
        return ret;
    }
    chip->irq_gpio = ret;

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

static int upm7610_tcpcdev_init(struct upm7610_chip *chip, struct device *dev)
{
    struct tcpc_desc *desc;
    struct device_node *np = dev->of_node;
    u32 val, len;
    const char *name = "default";

    dev_info(dev, "%s\n", __func__);

    desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
    if (!desc)
        return -ENOMEM;
    if (of_property_read_u32(np, "rt-tcpc,role_def", &val) >= 0) {
        if (val >= TYPEC_ROLE_NR)
            desc->role_def = TYPEC_ROLE_DRP;
        else
            desc->role_def = val;
    } else {
        dev_info(dev, "use default Role DRP\n");
        desc->role_def = TYPEC_ROLE_DRP;
    }

    if (of_property_read_u32(
        np, "rt-tcpc,notifier_supply_num", &val) >= 0) {
        if (val < 0)
            desc->notifier_supply_num = 0;
        else
            desc->notifier_supply_num = val;
    } else
        desc->notifier_supply_num = 0;

    if (of_property_read_u32(np, "rt-tcpc,rp_level", &val) >= 0) {
        switch (val) {
        case TYPEC_RP_DFT:
        case TYPEC_RP_1_5:
        case TYPEC_RP_3_0:
            desc->rp_lvl = val;
            break;
        default:
            break;
        }
    }

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
    if (of_property_read_u32(np, "rt-tcpc,vconn_supply", &val) >= 0) {
        if (val >= TCPC_VCONN_SUPPLY_NR)
            desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
        else
            desc->vconn_supply = val;
    } else {
        dev_info(dev, "use default VconnSupply\n");
        desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
    }
#endif  /* CONFIG_TCPC_VCONN_SUPPLY_MODE */

    if (of_property_read_string(np, "rt-tcpc,name",
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
            desc, &upm7610_tcpc_ops, chip);
    if (IS_ERR_OR_NULL(chip->tcpc))
        return -EINVAL;

#ifdef CONFIG_USB_PD_DISABLE_PE
    chip->tcpc->disable_pe =
            of_property_read_bool(np, "rt-tcpc,disable_pe");
#endif  /* CONFIG_USB_PD_DISABLE_PE */

    chip->tcpc->tcpc_flags = TCPC_FLAGS_VCONN_SAFE5V_ONLY
            | TCPC_FLAGS_CHECK_RA_DETACHE
            | TCPC_FLAGS_PD_REV30
            | TCPC_FLAGS_RETRY_CRC_DISCARD;

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD

#endif  /* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_USB_PD_REV30

    if (chip->tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
        dev_info(dev, "PD_REV30\n");
    else
        dev_info(dev, "PD_REV20\n");
#endif  /* CONFIG_USB_PD_REV30 */
    chip->tcpc->tcpc_flags |= TCPC_FLAGS_ALERT_V10;

    return 0;
}

#define UNISEMI_VID            0x362f
#define UNISEMI_7610_PID    0x7610

static inline int upm7610_check_revision(struct i2c_client *client)
{
    u16 vid, pid, did;
    int ret;
    u8 data = 1;

    ret = upm7610_read_device(client, UPM7610_REG_BMC_CTRL, 1, &data);
    if (ret < 0) {
        dev_err(&client->dev, "read chip ID fail\n");
        return -EIO;
    }

    pr_err("upm7610: reg[0x93]:0x%x\n", data);
    if (data != 0xFF) {
        UPM7610_INFO("upm7610: non pd phy\n");
        return -EIO;
    }

    ret = upm7610_read_device(client, TCPC_V10_REG_VID, 2, &vid);
    if (ret < 0) {
        dev_err(&client->dev, "read chip ID fail\n");
        return -EIO;
    }
    pr_err("upm7610 read vid:0x%x\n", vid);
    if (vid != UNISEMI_VID) {
        pr_info("%s failed, VID=0x%04x\n", __func__, vid);
        return -ENODEV;
    }

    ret = upm7610_read_device(client, TCPC_V10_REG_PID, 2, &pid);
    if (ret < 0) {
        dev_err(&client->dev, "read product ID fail\n");
        return -EIO;
    }
    pr_err("upm7610 read pid:0x%x\n", pid);
    if (pid != UNISEMI_7610_PID) {
        pr_info("%s failed, PID=0x%04x\n", __func__, pid);
        return -ENODEV;
    }

    ret = upm7610_write_device(client, UPM7610_REG_RESET_CTRL, 1, &data);
    if (ret < 0)
        return ret;
    /*A06 code for AL7160A-1244 by shixuanxuan at 20240522 start*/
    data = UPM7610_REG_EXT_MASK_CLEAR;
    ret = upm7610_write_device(client, UPM7610_REG_RESET_CTRL, 1, &data);
    if (ret < 0) {
        pr_err("%s write reset ctrl fail\n",__func__);
        return ret;
    }
    /*A06 code for AL7160A-1244 by shixuanxuan at 20240522 end*/

    usleep_range(1000, 2000);

    ret = upm7610_read_device(client, TCPC_V10_REG_DID, 2, &did);
    if (ret < 0) {
        dev_err(&client->dev, "read device ID fail\n");
        return -EIO;
    }
    pr_err("upm7610 read did:0x%x\n", did);
    return did;
}

/* A06 code for SR-AL7160A-01-271 by shixuanxuan at 20230325 start */
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
    struct upm7610_chip *chip = container_of(psy->desc, struct upm7610_chip, psd);

    switch (psp) {
    case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
        ret = upm7610_get_cc(chip->tcpc, &cc1, &cc2);
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
/* A06 code for SR-AL7160A-01-271 by shixuanxuan at 20230325 end */

static int upm7610_i2c_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    struct upm7610_chip *chip;
    int ret = 0, chip_id;
    bool use_dt = client->dev.of_node;

    pr_info("%s (%s)\n", __func__, UPM7610_DRV_VERSION);
    if (i2c_check_functionality(client->adapter,
            I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA))
        pr_info("upm7610 I2C functionality : OK...\n");
    else
        pr_info("upm7610 I2C functionality check : failuare...\n");

    chip_id = upm7610_check_revision(client);
    if (chip_id < 0) {
        dev_err(&client->dev, " get chip id fail, chip_id = %d\n", chip_id);
        return chip_id;
    }
#if TCPC_ENABLE_ANYMSG
    check_printk_performance();
#endif /* TCPC_ENABLE_ANYMSG */

    chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
    if (!chip) {
        dev_err(&client->dev, " chip devm_kzalloc fail\n");
        return -ENOMEM;
    }

    if (use_dt) {
        ret = rt_parse_dt(chip, &client->dev);
        if (ret < 0) {
            dev_err(&client->dev, " parse dts fail\n");
            return ret;
        }
    } else {
        dev_err(&client->dev, "no dts node\n");
        return -ENODEV;
    }
    chip->dev = &client->dev;
    chip->client = client;
    sema_init(&chip->io_lock, 1);
    sema_init(&chip->suspend_lock, 1);
    i2c_set_clientdata(client, chip);
    INIT_DELAYED_WORK(&chip->poll_work, upm7610_poll_work);
    chip->irq_wake_lock =
        wakeup_source_register(chip->dev, "upm7610_irq_wake_lock");

    chip->chip_id = chip_id;
    pr_info("upm7610_chipID = 0x%0x\n", chip_id);
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 start */
    mutex_init(&chip->i2c_rw_lock);
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240615 end*/

    ret = upm7610_regmap_init(chip);
    if (ret < 0) {
        dev_err(chip->dev, "upm7610 regmap init fail\n");
        goto err_regmap_init;
    }

    ret = upm7610_tcpcdev_init(chip, &client->dev);
    if (ret < 0) {
        dev_err(&client->dev, "upm7610 tcpc dev init fail\n");
        goto err_tcpc_reg;
    }

    ret = upm7610_init_alert(chip->tcpc);
    if (ret < 0) {
        pr_err("upm7610 init alert fail\n");
        goto err_irq_init;
    }

    tcpc_schedule_init_work(chip->tcpc);
    /* A06 code for SR-AL7160A-01-271 by shixuanxuan at 20230325 start */
    chip->psd.name = "typec_port";
    chip->psd.properties = typec_port_properties;
    chip->psd.type = POWER_SUPPLY_TYPE_USB_TYPE_C;
    chip->psd.num_properties = ARRAY_SIZE(typec_port_properties);
    chip->psd.get_property = typec_port_get_property;
    chip->typec_psy = devm_power_supply_register(chip->dev, &chip->psd, NULL);
    tcpc_info = UPM7610;
    if (IS_ERR(chip->typec_psy)) {
        pr_err("et7304 failed to register power supply: %ld\n",
        PTR_ERR(chip->typec_psy));
    }
    /* A06 code for SR-AL7160A-01-271 by shixuanxuan at 20230325 end */
    pr_info("%s probe OK!\n", __func__);
    return 0;

err_irq_init:
    tcpc_device_unregister(chip->dev, chip->tcpc);
err_tcpc_reg:
    upm7610_regmap_deinit(chip);
err_regmap_init:
    wakeup_source_unregister(chip->irq_wake_lock);
    return ret;
}

static int upm7610_i2c_remove(struct i2c_client *client)
{
    struct upm7610_chip *chip = i2c_get_clientdata(client);

    if (chip) {
        cancel_delayed_work_sync(&chip->poll_work);

        tcpc_device_unregister(chip->dev, chip->tcpc);
        upm7610_regmap_deinit(chip);
    }

    return 0;
}

#ifdef CONFIG_PM
static int upm7610_i2c_suspend(struct device *dev)
{
    struct upm7610_chip *chip;
    struct i2c_client *client = to_i2c_client(dev);

    if (client) {
        chip = i2c_get_clientdata(client);
        if (chip)
            down(&chip->suspend_lock);
    }

    return 0;
}

static int upm7610_i2c_resume(struct device *dev)
{
    struct upm7610_chip *chip;
    struct i2c_client *client = to_i2c_client(dev);

    if (client) {
        chip = i2c_get_clientdata(client);
        if (chip)
            up(&chip->suspend_lock);
    }

    return 0;
}

static void upm7610_shutdown(struct i2c_client *client)
{
    struct upm7610_chip *chip = i2c_get_clientdata(client);

    /* Please reset IC here */
    if (chip != NULL) {
        if (chip->irq)
            disable_irq(chip->irq);
        tcpm_shutdown(chip->tcpc);
    } else {
        i2c_smbus_write_byte_data(
            client, UPM7610_REG_RESET_CTRL, 0x01);
        i2c_smbus_write_byte_data(
            client, UPM7610_REG_CC_CTRL, 0x44);
    }
}

#ifdef CONFIG_PM_RUNTIME
static int upm7610_pm_suspend_runtime(struct device *device)
{
    dev_dbg(device, "pm_runtime: suspending...\n");
    return 0;
}

static int upm7610_pm_resume_runtime(struct device *device)
{
    dev_dbg(device, "pm_runtime: resuming...\n");
    return 0;
}
#endif /* #ifdef CONFIG_PM_RUNTIME */

static const struct dev_pm_ops upm7610_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(
            upm7610_i2c_suspend,
            upm7610_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
    SET_RUNTIME_PM_OPS(
        upm7610_pm_suspend_runtime,
        upm7610_pm_resume_runtime,
        NULL
    )
#endif /* #ifdef CONFIG_PM_RUNTIME */
};
#define UPM7610_PM_OPS  (&upm7610_pm_ops)
#else
#define UPM7610_PM_OPS  (NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id upm7610_id_table[] = {
    {"upm7610", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, upm7610_id_table);

static const struct of_device_id up_match_table[] = {
    {.compatible = "up,upm7610",},
    {},
};

static struct i2c_driver upm7610_driver = {
    .driver = {
        .name = "upm7610",
        .owner = THIS_MODULE,
        .of_match_table = up_match_table,
        .pm = UPM7610_PM_OPS,
    },
    .probe = upm7610_i2c_probe,
    .remove = upm7610_i2c_remove,
    .shutdown = upm7610_shutdown,
    .id_table = upm7610_id_table,
};

static int __init upm7610_init(void)
{
    struct device_node *np;

    pr_info("%s (%s): initializing...\n", __func__, UPM7610_DRV_VERSION);
    np = of_find_node_by_name(NULL, "usb_type_c");
    if (np != NULL)
        pr_info("usb_type_c node found...\n");
    else
        pr_info("usb_type_c node not found...\n");

    return i2c_add_driver(&upm7610_driver);
}
subsys_initcall(upm7610_init);

static void __exit upm7610_exit(void)
{
    i2c_del_driver(&upm7610_driver);
}
module_exit(upm7610_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DuLai <lai.du@unisemipower.com>");
MODULE_DESCRIPTION("UPM7610 TCPC Driver");
MODULE_VERSION(UPM7610_DRV_VERSION);