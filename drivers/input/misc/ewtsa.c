/*
 * linux/drivers/input/misc/ewtsa.c
 *
 * Copyright 2010-2011 Panasonic Electronic Devices. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*include files*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/regulator/consumer.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <mach/hardware.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

/* configurable */
#define GYRO_MOUNT_SWAP_XY      0   /* swap X, Y */
#define GYRO_MOUNT_REVERSE_X    0   /* reverse X */
#define GYRO_MOUNT_REVERSE_Y    0   /* reverse Y */
#define GYRO_MOUNT_REVERSE_Z    0   /* reverse Z */
#define GYRO_VER3_ES1           0
#define GYRO_VER3_ES3           0
#define GYRO_VER3_ES4           1
#define GYRO_REGISTER_MAP2      0
#define GYRO_SPI_INTERFACE      0

#define USE_GPIO_DRDY_PIN       0
#define USE_GPIO_DIAG_PIN       0
#define USE_GPIO_WINDOW_PIN     0

/* macro defines */
#define CHIP_ID                 0x68
#define DEVICE_NAME             "ewtsa"
#define EWTSA_ON                1
#define EWTSA_OFF               0
#define WINDOW_PIN              14
#define DRDY_PIN                12
#define DIAG_PIN                11
#define DIAG_ENABLE             0
#define DIAG_DISABLE            1
#define MAX_VALUE               32768

/* ewtsa_delay parameter */
#define DELAY_THRES_MIN         1
#define DELAY_THRES_1           4
#define DELAY_THRES_2           9   /* msec x 90% */
#define DELAY_THRES_3           18
#define DELAY_THRES_4           45
#define DELAY_THRES_5           90
#define DELAY_THRES_6           128
#define DELAY_THRES_MAX         255
#define DELAY_DLPF_0            0
#define DELAY_DLPF_1            1
#define DELAY_DLPF_2            2
#define DELAY_DLPF_3            3
#define DELAY_DLPF_4            4
#define DELAY_DLPF_5            5
#define DELAY_DLPF_6            6
#define DELAY_INTMIN_THRES      9
#define DELAY_INT8K_THRES       79
#define DELAY_THRES_FIFO        150

/* ewtsa_range parameter */
#define DYNRANGE_250            0x00
#define DYNRANGE_500            0x01
#define DYNRANGE_1000           0x02
#define DYNRANGE_2000           0x03

#define DATA_RATE_1             0x01
#define DATA_RATE_5             0x05
#define DATA_RATE_8             0x08
#define DATA_RATE_10            0x0A

/* ewtsa_sleep parameter */
#define SLEEP_OFF               0
#define SLEEP_ON                1
#define SLEEP_ALLRESET          2

/* ewtsa_calib parameter */
#define BOOST_TIME_500MS        0
#define BOOST_TIME_300MS        1
#define BOOST_TIME_200MS        2
#define BOOST_TIME_100MS        3
#define BOOST_FS_48KHZ          0
#define BOOST_FS_16KHZ          1
#define HPF_001HZ              0
#define HPF_002HZ              1
#define MASK_CLIP               0
#define MASK_NON_CLIP           1
#define THRESHOLD_ON            0
#define THRESHOLD_OFF           1
#define LAG_NON                 0
#define LAG_5SMP                1
#define LAG_10SMP               2
#define LAG_20SMP               3
#define W1_THRES_LEVEL_2DPS     0
#define W1_THRES_LEVEL_5DPS     1
#define W1_THRES_LEVEL_10DPS    2
#define W1_THRES_LEVEL_15DPS    3
#define REFERENCE_WAIT          16
#define W1_THRES_CENTER_COEF    25000
#define CALIB_FS_62_5HZ         0
#define CALIB_FS_31_25HZ        1

/* ewtsa_configuration parameter */
#define BYPASS_MODE           0
#define FIFO_MODE             1
#define FIFO_MAX              32
#define FIFO_MIN_DELAY        5
#define MAP_NAME_SIZE         4
#define INTERFACE_NAME_SIZE   3
#define CMD_SIZE_3            3
#define CMD_SIZE_4            4
#define THRES_CENTER_MIN      0
#define THRES_CENTER_MAX      359
#define THRES_CENTER_INIT_X   160
#define THRES_CENTER_INIT_Y   160
#define THRES_CENTER_INIT_Z   160

/* EWTSA_system_restart parameter */
#define RST_REG_INIT_OFF      0x00
#define RST_REG_INIT_RANGE    0x01
#define RST_REG_INIT_DELAY    0x02
#define RST_REG_INIT_ALL      0xFF

/* event mode */
#define EWTSA_POLLING_MODE    0
#define EWTSA_INTERUPT_MODE   1

/* ewtsa register address */
#define REG_SLAD                0x00
#define REG_FIFO_DRDY           0x10
#define REG_FIFO_CTRL1          0x13
#define REG_FIFO_CTRL2          0x14
#define REG_DATA_RATE           0x15
#define REG_FS_DLPF_CTRL        0x16
#define REG_INT_CTRL            0x17
#define REG_CTRL_REG2           0x18
#define REG_CTRL_REG3           0x19
#define REG_INT_STAT            0x1A
#define REG_TEMP_H              0x1B
#define REG_TEMP_L              0x1C
#define REG_OUTX_H              0x1D
#define REG_OUTX_L              0x1E
#define REG_OUTY_H              0x1F
#define REG_OUTY_L              0x20
#define REG_OUTZ_H              0x21
#define REG_OUTZ_L              0x22
#define REG_CTRL_REG5           0x24
#define REG_REFERENCE           0x25
#define REG_STATUS_REG          0x27
#define REG_DIGITAL_CAL         0x29
#define REG_DIAG_CTRL           0x2B
#define REG_FIFO_DM             0x2F
#define REG_DIAG_MONI           0x39
#define REG_SLEEP_CTRL          0x3E
#define REG_W1THX_H             0x46
#define REG_W1THX_L             0x47
#define REG_W1THY_H             0x48
#define REG_W1THY_L             0x49
#define REG_W1THZ_H             0x4A
#define REG_W1THZ_L             0x4B
#define REG_SOC_CNT1            0x4C
#define REG_SOC_CNT2            0x4D
#define REG_SLFSLP_CNT          0x4F
#define REG_SPITYPE             0x60
#define REG_MBURST_ALL          0xFF

/* ewtsa register map2 address */
#define REG2_CTRL_REG1          0x20

/* ewtsa register param */
#define INT_CTRL_WTMON_ENABLE   0x0D
#define INT_CTRL_WTMON_DISABLE  0x00
#define DIGITAL_CAL_ENABLE      0x00
#define DIGITAL_CAL_DISABLE     0x01
#define SLEEP_CTRL_ACTIVATE     0x44
#define SLEEP_CTRL_SLEEP        0x04
#define SLEEP_CTRL_ALLRSTSLEEP  0x00
#define SLEEP_CTRL_LOGIC_RST    0x80
#define SOC_CNT1_BOOST_ON       0x01
#define SOC_CNT1_WIN_DIS        0x70
#define SOC_CNT2_SOCCLK_ON      0x40
#define SOC_CNT2_SOCCLK_OFF     0x00
#define INT_CTRL_INT_ENABLE     0x01
#define INT_CTRL_INT_DISABLE    0x00
#define INT_CTRL_INT_WTMON_EN   0x08
#define CTRL_REG2_REF_MODE      0x10
#define CTRL_REG2_NOR_MODE      0x60
#define CTRL_REG2_HPCF          0x09
#define CTRL_REG5_FIFO_EN       0x53
#define CTRL_REG5_INIT          0x13
#define FIFO_DRDY_DITH_EN       0x02
#define FIFO_DRDY_INIT          0x17
#define CTRL_REG3_WTM_DIS       0x00
#define CTRL_REG3_WTM_EN        0x04
#define STATUS_REG_OVERRUN      0xF0
#define FIFO_CTRL1_CLEAR        0x02

/* ewtsa register map2 param */
#define CTRL_REG1_SLEEP         0x07
#define CTRL_REG1_ACTIVATE      0x0F

/* ewtsa interrupt control */
#define EWSTA_INT_CLEAR         0x00
#define EWSTA_INT_SKIP          0x01

/* EWTSA_set_calibration param */
#define EWTSA_RANGE_0           250
#define EWTSA_RANGE_1           500
#define EWTSA_RANGE_2           1000
#define EWTSA_RANGE_3           2000
#define EWTSA_BOOST_TIME_0      500
#define EWTSA_BOOST_TIME_1      300
#define EWTSA_BOOST_TIME_2      200
#define EWTSA_BOOST_TIME_3      100
#define EWTSA_W1_THRES_LV_0     2
#define EWTSA_W1_THRES_LV_1     5
#define EWTSA_W1_THRES_LV_2     10
#define EWTSA_W1_THRES_LV_3     15

/* init param */
#define SLEEP_INIT_VAL       SLEEP_ALLRESET
#define FIFO_TYPE_INIT_VAL   BYPASS_MODE
#define DELAY_INIT_VAL       DELAY_THRES_1
#define RANGE_INIT_VAL       DYNRANGE_1000
#define DLPF_INIT_VAL        DELAY_DLPF_2
#define CALIB_FUNC_INIT_VAL  EWTSA_ON
#if GYRO_VER3_ES4
#define CALIB_BTIM_INIT_VAL  BOOST_TIME_100MS
#else
#define CALIB_BTIM_INIT_VAL  BOOST_TIME_500MS
#endif
#define CALIB_FS_INIT_VAL    CALIB_FS_62_5HZ
#define CALIB_RST_INIT_VAL   EWTSA_ON
#define CALIB_BFS_INIT_VAL   BOOST_FS_48KHZ
#define CALIB_CLK_INIT_VAL   EWTSA_ON
#define CALIB_HPF_INIT_VAL   HPF_002HZ
#if GYRO_VER3_ES4
#define CALIB_LAG_INIT_VAL   LAG_5SMP
#else
#define CALIB_LAG_INIT_VAL   LAG_10SMP
#endif
#define W1_FUNC_INIT_VAL     THRESHOLD_ON
#define W2_FUNC_INIT_VAL     THRESHOLD_OFF
#if GYRO_VER3_ES4
#define W1_LV_INIT_VAL       W1_THRES_LEVEL_5DPS
#define W1_MASK_INIT_VAL     MASK_CLIP
#else
#define W1_LV_INIT_VAL       W1_THRES_LEVEL_15DPS
#define W1_MASK_INIT_VAL     MASK_NON_CLIP
#endif

/* declarations */
struct ewtsa_t {
    struct i2c_client *client;
    struct input_dev *input_dev;
    struct workqueue_struct *workqueue;
    struct delayed_work input_work;
    struct mutex mlock;
    spinlock_t slock;
    unsigned long delay;
    signed short  fifo_lastdata[4];
    unsigned short w1_thres_x;
    unsigned short w1_thres_y;
    unsigned short w1_thres_z;
    unsigned char event;
    unsigned char range;
    unsigned char sleep;
    unsigned char dlpf;
    unsigned char calib;
    unsigned char calib_fs;
    unsigned char calib_rst;
    unsigned char calib_boost_tm;
    unsigned char calib_boost_fs;
    unsigned char fifo_mode;
    unsigned char fifo_num;
    unsigned char fifo_data_flag;
    unsigned char reset_param;
    unsigned char int_skip;
    unsigned char calib_alway;
    unsigned char hpf;
    unsigned char w1_datamask;
    unsigned char w1_thres_en;
    unsigned char w2_thres_en;
    unsigned char offset_lag;
    unsigned char w1_thres_level;
    unsigned char reserved;
};

static int __init init_ewtsa(void);
static void __exit exit_ewtsa(void);
static ssize_t ewtsa_sleep_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ewtsa_sleep_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t ewtsa_delay_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ewtsa_delay_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t ewtsa_range_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ewtsa_range_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t ewtsa_calib_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ewtsa_calib_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t ewtsa_config_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ewtsa_config_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static int EWTSA_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int EWTSA_remove(struct i2c_client *client);
static int EWTSA_suspend(struct i2c_client *client, pm_message_t mesg);
static int EWTSA_resume(struct i2c_client *client);
static void EWTSA_report_abs(struct ewtsa_t *ewt);
static void EWTSA_dev_poll(struct work_struct *work);
static void EWTSA_input_work_func(struct work_struct *work);
#if USE_GPIO_DRDY_PIN
static irqreturn_t EWTSA_interrupt(int irq, void *dev_id);
#endif
static void EWTSA_system_restart(struct ewtsa_t *ewtsa);
static void EWTSA_set_calibration(struct ewtsa_t *ewtsa);

static struct device_attribute ewtsa_sleep_attr = {
    .attr = {
         .name = "ewtsa_sleep",
         .mode = S_IRUGO | S_IWUSR,
        },
    .show = ewtsa_sleep_show,
    .store = ewtsa_sleep_store,
};

static struct device_attribute ewtsa_delay_attr = {
    .attr = {
        .name = "ewtsa_delay",
        .mode = S_IRUGO | S_IWUSR,
    },
    .show = ewtsa_delay_show,
    .store = ewtsa_delay_store,
};

static struct device_attribute ewtsa_range_attr = {
    .attr = {
        .name = "ewtsa_range",
        .mode = S_IRUGO | S_IWUSR,
    },
    .show = ewtsa_range_show,
    .store = ewtsa_range_store,
};

static struct device_attribute ewtsa_calib_attr = {
    .attr = {
        .name = "ewtsa_calib",
        .mode = S_IRUGO | S_IWUSR,
    },
    .show = ewtsa_calib_show,
    .store = ewtsa_calib_store,
};

static struct device_attribute ewtsa_config_attr = {
    .attr = {
        .name = "ewtsa_configuration",
        .mode = S_IRUGO | S_IWUSR,
    },
    .show = ewtsa_config_show,
    .store = ewtsa_config_store,
};

static const struct i2c_device_id ewtsa_id[] = {
    {"ewtsa", 0},
    {{0}, 0}
};

MODULE_DEVICE_TABLE(i2c, ewtsa_id);

static struct i2c_driver i2c_ewtsa_driver = {
    .driver = {
           .name = "ewtsa",
           },
    .probe = EWTSA_probe,
    .remove = EWTSA_remove,
    .suspend = EWTSA_suspend,
    .resume  = EWTSA_resume,
    .id_table = ewtsa_id,
};

/* show the sleep mode */
static ssize_t ewtsa_sleep_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct i2c_client *client;
    struct ewtsa_t *ewtsa;
#if !GYRO_VER3_ES1
    int ret;
    unsigned char val;
#endif /* GYRO_VER3_ES1 */

    if (dev == NULL) {
        printk(KERN_INFO "struct device null pointer\n");
        return -EIO;
    }
    client = to_i2c_client(dev);

    if (client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(client);

    /* output buffer check */
    if (buf == NULL) {
        dev_err(&client->dev, "buf pointer null!\n");
        return -EIO;
    }

#if GYRO_VER3_ES1
    return sprintf(buf, "%d\n", ewtsa->sleep);
#else /* GYRO_VER3_ES1 */
    ret = i2c_smbus_read_byte_data(ewtsa->client, REG_SLEEP_CTRL);
    if (ret < 0) {
        dev_err(&ewtsa->client->dev, "REG_SLEEP_CTRL read err(ret = %d)\n", ret);
        return ret;
    }
    val = (unsigned char)ret;

    /* REG_SLEEP_CTRL register value check */
    if ((val & SLEEP_CTRL_ACTIVATE) == SLEEP_CTRL_ACTIVATE) {
        val = SLEEP_OFF;
    }
    else if ((val & SLEEP_CTRL_SLEEP) == SLEEP_CTRL_SLEEP) {
        val = SLEEP_ON;
    }
    else {
        val = SLEEP_ALLRESET;
    }

    return sprintf(buf, "%d\n", val);
#endif /* GYRO_VER3_ES1 */
}

/* Set the sleep mode */
static ssize_t ewtsa_sleep_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    u8 reg;
    struct i2c_client *client;
    struct ewtsa_t *ewtsa;
    unsigned char val;
    unsigned char before_sleep;
    s32 ret;
#if GYRO_VER3_ES1 || USE_GPIO_DRDY_PIN
    int err;
#endif /* GYRO_VER3_ES1 */
    long           chk_param;
    unsigned long  l_flags = 0;
#if GYRO_VER3_ES1
    unsigned char map2_flag = EWTSA_OFF;
#endif /* GYRO_VER3_ES1 */

    if (dev == NULL) {
        printk(KERN_INFO "struct device null pointer\n");
        return -EIO;
    }
    client = to_i2c_client(dev);

    if (client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(client);

    /* input buffer check */
    if (buf == NULL) {
        dev_err(&client->dev, "buf pointer null!\n");
        return -EIO;
    }

    chk_param = simple_strtol(buf, NULL, 10);
    if ((chk_param < SLEEP_OFF) || (chk_param >SLEEP_ALLRESET)) {
        dev_err(&ewtsa->client->dev, "invalid value = %ld\n", chk_param);
        return -EINVAL;
    }
    val = (unsigned char)chk_param;

    dev_info(&ewtsa->client->dev, "set SLEEP = %d\n", val);

#if GYRO_VER3_ES1
    /* if current status is same */
    if (ewtsa->sleep == val) {
        return count;
    }
#else /* GYRO_VER3_ES1 */
    ret = i2c_smbus_read_byte_data(ewtsa->client, REG_SLEEP_CTRL);
    if (ret < 0) {
        dev_err(&ewtsa->client->dev, "REG_SLEEP_CTRL read err(ret = %d)\n", ret);
        return ret;
    }
    reg = (u8)ret;

    /* REG_SLEEP_CTRL register value check */
    if ((reg & SLEEP_CTRL_ACTIVATE) == SLEEP_CTRL_ACTIVATE) {
        reg = SLEEP_OFF;
    }
    else if ((reg & SLEEP_CTRL_SLEEP) == SLEEP_CTRL_SLEEP) {
        reg = SLEEP_ON;
    }
    else {
        reg = SLEEP_ALLRESET;
    }

    /* if current status is same */
    if (reg == val) {
        return count;
    }
#endif /* GYRO_VER3_ES1 */

    before_sleep = ewtsa->sleep;
    switch(val)
    {
        case SLEEP_OFF:
#if GYRO_VER3_ES1
            if (ewtsa->sleep == SLEEP_ON) {
                reg = CTRL_REG1_ACTIVATE;
                map2_flag = EWTSA_ON;
            }
            else {
                reg = SLEEP_CTRL_ACTIVATE;
            }
#else /* GYRO_VER3_ES1 */
            reg = SLEEP_CTRL_ACTIVATE;
#endif /* GYRO_VER3_ES1 */
            break;
        case SLEEP_ON:
            if (ewtsa->sleep == SLEEP_ALLRESET) {
                return count;
            }
            reg = SLEEP_CTRL_SLEEP;
            ewtsa->calib_rst = EWTSA_ON; 
            break;
        default:                                 /* SLEEP_ALLRESET */
            reg = SLEEP_CTRL_ALLRSTSLEEP;
            ewtsa->calib_rst = EWTSA_ON;
            ewtsa->reset_param = RST_REG_INIT_ALL;
            break;
    }

    if (ewtsa->event != EWTSA_POLLING_MODE) {
        /* disable interrupt */
        ret = i2c_smbus_write_byte_data(ewtsa->client, REG_INT_CTRL, INT_CTRL_INT_DISABLE);
        if (ret < 0) 
        {
            dev_err(&ewtsa->client->dev, "REG_INT_CTRL write err(data = 0x%02x, ret = %d)\n", INT_CTRL_INT_DISABLE, ret);
            return ret;
        }
    }

    dev_info(&ewtsa->client->dev, "set reg = 0x%02x\n", reg);
#if GYRO_VER3_ES1
    if (map2_flag == EWTSA_ON) {
        ret = i2c_smbus_write_byte_data(ewtsa->client, REG2_CTRL_REG1, reg);
    }
    else {
        ret = i2c_smbus_write_byte_data(ewtsa->client, REG_SLEEP_CTRL, reg);
    }
#else /* GYRO_VER3_ES1 */
    ret = i2c_smbus_write_byte_data(ewtsa->client, REG_SLEEP_CTRL, reg);
#endif /* GYRO_VER3_ES1 */
    if (ret < 0) {
        dev_err(&ewtsa->client->dev, "REG_SLEEP_CTRL write err(data = 0x%02x, ret = %d)\n", reg, ret);
        return ret;
    }
    ewtsa->sleep = val;

    if (val != SLEEP_OFF) {
        if (ewtsa->event == EWTSA_POLLING_MODE) {
            cancel_delayed_work(&ewtsa->input_work);
        }
    }
    else {
        ret = i2c_smbus_write_byte_data(ewtsa->client, REG_DIAG_CTRL, DIAG_ENABLE);
        if (ret < 0) {
            dev_err(&ewtsa->client->dev, "REG_DIAG_CTRL write err(data = 0x%02x, ret = %d)\n", DIAG_ENABLE, ret);
            return ret;
        }
        if (ewtsa->event == EWTSA_POLLING_MODE) {
            ret = i2c_smbus_write_byte_data(ewtsa->client, REG_INT_CTRL, INT_CTRL_WTMON_DISABLE); /* No INT */
            if (ret < 0) {
                dev_err(&ewtsa->client->dev, "REG_INT_CTRL write err(data = 0x%02x, ret = %d)\n", INT_CTRL_WTMON_DISABLE, ret);
                return ret;
            }
        }

        if (ewtsa->calib_rst == EWTSA_ON) {
            EWTSA_system_restart(ewtsa);
        }
        if (ewtsa->event == EWTSA_POLLING_MODE) {
            /* polling timer start */
            queue_delayed_work(ewtsa->workqueue,
                &ewtsa->input_work,
                msecs_to_jiffies(ewtsa->delay));
        }
    }

    if (ewtsa->event == EWTSA_POLLING_MODE) {
        mutex_lock(&ewtsa->mlock);
        input_report_abs(ewtsa->input_dev, ABS_RY, -1);
        input_sync(ewtsa->input_dev);
        mutex_unlock(&ewtsa->mlock);
    }
    else {
        spin_lock_irqsave(&ewtsa->slock, l_flags);
        input_report_abs(ewtsa->input_dev, ABS_RY, -1);
        input_sync(ewtsa->input_dev);
        spin_unlock_irqrestore(&ewtsa->slock, l_flags);

        if (val == SLEEP_OFF) {
            /* enable interrupt */
#if USE_GPIO_DRDY_PIN
            err = gpio_request(DRDY_PIN, "DRDY");
            err += gpio_direction_input(DRDY_PIN);
            err += request_threaded_irq(gpio_to_irq(DRDY_PIN), NULL, EWTSA_interrupt, IRQF_TRIGGER_RISING, DEVICE_NAME, ewtsa);
            if (err != 0) {
                dev_err(&ewtsa->client->dev, "request_irq err(%d)\n", err);
                return err;
            }
#endif

            ewtsa->fifo_data_flag = EWTSA_OFF;
            memset(ewtsa->fifo_lastdata, 0x00, sizeof(ewtsa->fifo_lastdata));
            ewtsa->int_skip = EWSTA_INT_SKIP;
            if (ewtsa->fifo_mode == BYPASS_MODE) {
                reg = (u8)INT_CTRL_INT_ENABLE;
            }
            else {
                reg = (u8)(INT_CTRL_INT_ENABLE | INT_CTRL_INT_WTMON_EN);
            }
            ret = i2c_smbus_write_byte_data(ewtsa->client, REG_INT_CTRL, reg);
            if (ret < 0) {
                dev_err(&ewtsa->client->dev, "REG_INT_CTRL write err(data = 0x%02x, ret = %d)\n", reg, ret);
                return ret;
            }
        }
        else
        {
            if (before_sleep == SLEEP_OFF) {
#if USE_GPIO_DRDY_PIN
                free_irq(gpio_to_irq(DRDY_PIN), ewtsa);
                gpio_free(DRDY_PIN);
#endif
            }
        }
    }

    return count;
}

/* Show the LPF settings */
static ssize_t ewtsa_delay_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct i2c_client *client;
    struct ewtsa_t *ewtsa;

    if (dev == NULL) {
        printk(KERN_INFO "struct device null pointer\n");
        return -EIO;
    }
    client = to_i2c_client(dev);

    if (client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(client);

    /* output buffer check */
    if (buf == NULL) {
        dev_err(&client->dev, "buf pointer null!\n");
        return -EIO;
    }

    return sprintf(buf, "%ld\n", ewtsa->delay);
}

/* Set the LPF settings*/
static ssize_t ewtsa_delay_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct i2c_client *client;
    struct ewtsa_t *ewtsa;
    unsigned long delay;
    unsigned char val;
    unsigned char number;
    u8            smpl;
    int           err;
    unsigned char reg;
    unsigned char fifo_type;

    if (dev == NULL) {
        printk(KERN_INFO "struct device null pointer\n");
        return -EIO;
    }
    client = to_i2c_client(dev);

    if (client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(client);

    /* input buffer check */
    if (buf == NULL) {
        dev_err(&client->dev, "buf pointer null!\n");
        return -EIO;
    }
    err = strict_strtoul(buf, 0, &delay);
    if (err != 0) {
        dev_err(&ewtsa->client->dev, "strict_strtoul err(ret = %d)\n", err);
        return err;
    }

    if (delay == ewtsa->delay) {
        return count;
    }

    if ((delay < DELAY_THRES_MIN) || (delay > DELAY_THRES_MAX)) {
        dev_err(&ewtsa->client->dev, "param err(ret = %ld)\n", delay);
        return -EINVAL;
    }

    dev_info(&ewtsa->client->dev, "Delay = %ld\n", delay);

    if (ewtsa->sleep == SLEEP_OFF) {
        if (ewtsa->event == EWTSA_POLLING_MODE) {
            cancel_delayed_work(&ewtsa->input_work);
        }
        else {
            /* disable interrupt */
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_INT_CTRL, INT_CTRL_INT_DISABLE);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_INT_CTRL write err(data = 0x%02x, ret = %d)\n", INT_CTRL_INT_DISABLE, err);
                return err;
            }
        }

        if (ewtsa->event == EWTSA_POLLING_MODE) {
            fifo_type = BYPASS_MODE;
        }
        else {
            fifo_type = ewtsa->fifo_mode;
        }

        if (fifo_type == BYPASS_MODE) {
            if (delay < DELAY_THRES_4) {
                if (delay >= DELAY_THRES_3) {
                    val = DELAY_DLPF_3;
                } else {
                    val = DELAY_DLPF_2;
                }
            } else {
                if (delay >= DELAY_THRES_6) {
                    val = DELAY_DLPF_6;
                } else if (delay >= DELAY_THRES_5) {
                    val = DELAY_DLPF_5;
                } else {
                    val = DELAY_DLPF_4;
                }
            }
        }
        else {
            if (delay <= DELAY_THRES_FIFO) {
                val = DELAY_DLPF_1;
            }
            else {
                val = DELAY_DLPF_2;
            }
        }

        reg = (unsigned char)((unsigned char)(ewtsa->range << 3) | val );
        dev_info(&ewtsa->client->dev, "FS_DLPF = %2X\n", (unsigned int)reg);
        err = i2c_smbus_write_byte_data(ewtsa->client, REG_FS_DLPF_CTRL, reg);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_FS_DLPF_CTRL write err(data = 0x%02x, ret = %d)\n", reg, err);
            return err;
        }
        ewtsa->dlpf = val;

        err = i2c_smbus_write_byte_data(ewtsa->client, REG_CTRL_REG5, CTRL_REG5_INIT);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_CTRL_REG5 write err(data = 0x%02x, ret = %d)\n", CTRL_REG5_INIT, err);
            return err;
        }

        if (fifo_type != BYPASS_MODE) {
            reg = FIFO_DRDY_DITH_EN;
        }
        else {
            dev_info(&ewtsa->client->dev, "FIFO_CTR = %2X\n", fifo_type);
            reg = (unsigned char)(fifo_type << 5);
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_FIFO_CTRL2, reg);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_FIFO_CTRL2 write err(data = 0x%02x, ret = %d)\n", reg, err);
                return err;
            }
            reg = FIFO_DRDY_INIT;
        }

        err = i2c_smbus_write_byte_data(ewtsa->client, REG_FIFO_DRDY, reg);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_FIFO_DRDY write err(data = 0x%02x, ret = %d)\n", reg, err);
            return err;
        }

        if (ewtsa->event == EWTSA_POLLING_MODE) {
            smpl = DATA_RATE_1 - 1; /* SMPL = 0 */
            dev_info(&ewtsa->client->dev, "SMPL+1 = %2X\n", smpl);
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_DATA_RATE, smpl);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_DATA_RATE write err(data = 0x%02x, ret = %d)\n", smpl, err);
                return err;
            }
            ewtsa->delay = delay;
            ewtsa->reset_param &= (unsigned char)~RST_REG_INIT_DELAY;
            EWTSA_system_restart(ewtsa);

            queue_delayed_work(ewtsa->workqueue,
                &ewtsa->input_work,
                msecs_to_jiffies(ewtsa->delay));
        }
        else {
            if (fifo_type == BYPASS_MODE) {
                if (delay <= DELAY_THRES_2) {
                    smpl = DELAY_INTMIN_THRES;
                }
                else {
                    smpl = (unsigned char)(delay - 1);
                }
                ewtsa->fifo_num = 1;
            }
            else {
                if (delay <= DELAY_THRES_FIFO) {
                    if (delay > FIFO_MIN_DELAY) {
                        number = (unsigned char)(delay / DATA_RATE_5);
                        if ((delay % DATA_RATE_5) != 0) {
                            number++;
                        }
                    }
                    else {
                        number = 2;
                    }
                    smpl = DATA_RATE_5 - 1; /* SMPL = 4 */
                }
                else {
                    number = (unsigned char)(delay / DATA_RATE_10);
                    if ((delay % DATA_RATE_10) != 0) {
                        number++;
                    }
                    smpl = DATA_RATE_10 - 1; /* SMPL = 9 */
                }

                reg = (unsigned char)(fifo_type << 5);
                reg |= number;
                err = i2c_smbus_write_byte_data(ewtsa->client, REG_FIFO_CTRL2, reg);
                if (err < 0) {
                    dev_err(&ewtsa->client->dev, "REG_FIFO_CTRL2 write err(data = 0x%02x, ret = %d)\n", reg, err);
                    return err;
                }
                ewtsa->fifo_num = number;

                err = i2c_smbus_write_byte_data(ewtsa->client, REG_CTRL_REG3, CTRL_REG3_WTM_EN);
                if (err < 0) {
                    dev_err(&ewtsa->client->dev, "REG_CTRL_REG3 write err(data = 0x%02x, ret = %d)\n", CTRL_REG3_WTM_EN, err);
                    return err;
                }

                err = i2c_smbus_write_byte_data(ewtsa->client, REG_CTRL_REG5, CTRL_REG5_FIFO_EN);
                if (err < 0) {
                    dev_err(&ewtsa->client->dev, "REG_CTRL_REG5 write err(data = 0x%02x, ret = %d)\n", CTRL_REG5_FIFO_EN, err);
                    return err;
                }
            }
            dev_info(&ewtsa->client->dev, "SMPL+1 = %2X\n", smpl);
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_DATA_RATE, smpl);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_DATA_RATE write err(data = 0x%02x, ret = %d)\n", smpl, err);
                return err;
            }
            ewtsa->delay = delay;
            ewtsa->reset_param &= (unsigned char)~RST_REG_INIT_DELAY;
            EWTSA_system_restart(ewtsa);

            /* enable interrupt */
            ewtsa->fifo_data_flag = EWTSA_OFF;
            memset(ewtsa->fifo_lastdata, 0x00, sizeof(ewtsa->fifo_lastdata));
            ewtsa->int_skip = EWSTA_INT_SKIP;
            if (fifo_type == BYPASS_MODE) {
                reg = (u8)INT_CTRL_INT_ENABLE;
            }
            else {
                reg = (u8)(INT_CTRL_INT_ENABLE | INT_CTRL_INT_WTMON_EN);
            }
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_INT_CTRL, reg);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_INT_CTRL write err(data = 0x%02x, ret = %d)\n", reg, err);
                return err;
            }
        }
    }
    else {
        ewtsa->delay = delay;
        ewtsa->calib_rst = EWTSA_ON;
        ewtsa->reset_param |= RST_REG_INIT_DELAY;
    }

    return count;
}

/* Show the FS settings */
static ssize_t ewtsa_range_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct i2c_client *client;
    struct ewtsa_t *ewtsa ;

    if (dev == NULL) {
        printk(KERN_INFO "struct device null pointer\n");
        return -EIO;
    }
    client = to_i2c_client(dev);

    if (client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(client);

    /* output buffer check */
    if (buf == NULL) {
        dev_err(&client->dev, "buf pointer null!\n");
        return -EIO;
    }

    return sprintf(buf, "%d\n", ewtsa->range);
}

/* Set the FS settings*/
static ssize_t ewtsa_range_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct i2c_client *client;
    struct ewtsa_t *ewtsa;
    unsigned char val;
    s32           err;
    unsigned char rng;
    u8            reg;
    long          chk_param;

    if (dev == NULL) {
        printk(KERN_INFO "struct device null pointer\n");
        return -EIO;
    }
    client = to_i2c_client(dev);

    if (client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(client);

    /* input buffer check */
    if (buf == NULL) {
        dev_err(&client->dev, "buf pointer null!\n");
        return -EIO;
    }
    chk_param = simple_strtol(buf, NULL, 10);
    if ((chk_param < DYNRANGE_250) || (chk_param > DYNRANGE_2000)) {
        dev_info(&ewtsa->client->dev, "invalid value = %ld\n", chk_param);
        return -EINVAL;
    }
    val = (unsigned char)chk_param;

    if (val == ewtsa->range) {
        return count;
    }

    dev_info(&ewtsa->client->dev, "FS = %d\n", (int)val);
    if (ewtsa->sleep == SLEEP_OFF) {
        if (ewtsa->event != EWTSA_POLLING_MODE) {
            /* disable interrupt */
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_INT_CTRL, INT_CTRL_INT_DISABLE);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_INT_CTRL write err(data = 0x%02x, ret = %d)\n", INT_CTRL_INT_DISABLE, err);
                return err;
            }
        }
        rng = (unsigned char)((unsigned char)(val << 3) | ewtsa->dlpf);
        err = i2c_smbus_write_byte_data(ewtsa->client, REG_FS_DLPF_CTRL, rng);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_FS_DLPF_CTRL write err(data = 0x%02x, ret = %d)\n", rng, err);
            return err;
        }
        ewtsa->range = val;
        ewtsa->reset_param &= (unsigned char)~RST_REG_INIT_RANGE;
        if (ewtsa->event == EWTSA_POLLING_MODE) {
            cancel_delayed_work(&ewtsa->input_work);
            EWTSA_system_restart(ewtsa);
            queue_delayed_work(ewtsa->workqueue,
                &ewtsa->input_work,
                msecs_to_jiffies(ewtsa->delay));
        }
        else {
            EWTSA_system_restart(ewtsa);

            /* enable interrupt */
            ewtsa->fifo_data_flag = EWTSA_OFF;
            memset(ewtsa->fifo_lastdata, 0x00, sizeof(ewtsa->fifo_lastdata));
            ewtsa->int_skip = EWSTA_INT_SKIP;
            if (ewtsa->fifo_mode == BYPASS_MODE) {
                reg = (u8)INT_CTRL_INT_ENABLE;
            }
            else {
                reg = (u8)(INT_CTRL_INT_ENABLE | INT_CTRL_INT_WTMON_EN);
            }
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_INT_CTRL, reg);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_INT_CTRL write err(data = 0x%02x, ret = %d)\n", reg, err);
                return err;
            }
        }
    }
    else {
        ewtsa->range = val;
        ewtsa->calib_rst = EWTSA_ON;
        ewtsa->reset_param |= RST_REG_INIT_RANGE;
    }

    return count;
}

/* Show the SELF O C settings */
static ssize_t ewtsa_calib_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct i2c_client *client;
    struct ewtsa_t *ewtsa;

    if (dev == NULL) {
        printk(KERN_INFO "struct device null pointer\n");
        return -EIO;
    }
    client = to_i2c_client(dev);

    if (client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(client);

    /* output buffer check */
    if (buf == NULL) {
        dev_err(&client->dev, "buf pointer null!\n");
        return -EIO;
    }

    return sprintf(buf, "%d\n", ewtsa->calib);
}

/* Set the SELF O C settings*/
static ssize_t ewtsa_calib_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct i2c_client *client;
    struct ewtsa_t *ewtsa;
    unsigned char val;
    s32           err;
    u8            reg;
    long          chk_param;

    if (dev == NULL) {
        printk(KERN_INFO "struct device null pointer\n");
        return -EIO;
    }
    client = to_i2c_client(dev);

    if (client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(client);

    /* input buffer check */
    if (buf == NULL) {
        dev_err(&client->dev, "buf pointer null!\n");
        return -EIO;
    }
    chk_param = simple_strtol(buf, NULL, 10);
    if ((chk_param != EWTSA_ON) && (chk_param != EWTSA_OFF)) {
        dev_info(&ewtsa->client->dev, "invalid value = %ld\n", chk_param);
        return -EINVAL;
    }
    val = (unsigned char)chk_param;

    dev_info(&ewtsa->client->dev, "SELF_O_C = %d(now = %d)\n", (int)val, ewtsa->calib);

    if (ewtsa->calib == val) {
        return count;
    }

    ewtsa->calib = val;
    if (ewtsa->sleep == SLEEP_OFF) {
        if (ewtsa->event == EWTSA_POLLING_MODE) {
            cancel_delayed_work(&ewtsa->input_work);
            EWTSA_system_restart(ewtsa);
            queue_delayed_work(ewtsa->workqueue,
                &ewtsa->input_work,
                msecs_to_jiffies(ewtsa->delay));
        }
        else {
            /* disable interrupt */
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_INT_CTRL, INT_CTRL_INT_DISABLE);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_INT_CTRL write err(data = 0x%02x, ret = %d)\n", INT_CTRL_INT_DISABLE, err);
                return err;
            }

            EWTSA_system_restart(ewtsa);

            /* enable interrupt */
            ewtsa->fifo_data_flag = EWTSA_OFF;
            memset(ewtsa->fifo_lastdata, 0x00, sizeof(ewtsa->fifo_lastdata));
            ewtsa->int_skip = EWSTA_INT_SKIP;
            if (ewtsa->fifo_mode == BYPASS_MODE) {
                reg = (u8)INT_CTRL_INT_ENABLE;
            }
            else {
                reg = (u8)(INT_CTRL_INT_ENABLE | INT_CTRL_INT_WTMON_EN);
            }
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_INT_CTRL, reg);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_INT_CTRL write err(data = 0x%02x, ret = %d)\n", reg, err);
                return err;
            }
        }
    }
    else {
        ewtsa->calib_rst = EWTSA_ON;
    }

    return count;
}

/* Show the gyro configurations */
static ssize_t ewtsa_config_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct i2c_client *l_client;
    struct ewtsa_t *ewtsa;
    int            ret;
    ssize_t        ret_len = 0;

    if (dev == NULL) {
        printk(KERN_INFO "struct device null pointer\n");
        return -EIO;
    }
    l_client = to_i2c_client(dev);

    if (l_client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(l_client);

    /* output buffer check */
    if (buf == NULL) {
        dev_err(&ewtsa->client->dev, "buf pointer null!\n");
        return -EIO;
    }

#if GYRO_REGISTER_MAP2
    ret = sprintf(buf+ret_len, "MAP2;");
#else /* GYRO_REGISTER_MAP2 */
    ret = sprintf(buf+ret_len, "MAP1;");
#endif /* GYRO_REGISTER_MAP2 */
    if (ret >= 0) {
        ret_len += ret;
    }

#if GYRO_SPI_INTERFACE
    ret = sprintf(buf+ret_len, "SPI;");
#else /* GYRO_SPI_INTERFACE */
    ret = sprintf(buf+ret_len, "I2C;");
#endif /* GYRO_SPI_INTERFACE */
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "SLP=%d;", ewtsa->sleep);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "DLY=%ld;", ewtsa->delay);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "RNG=%d;", ewtsa->range);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "SOC=%d;", ewtsa->calib);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "SBT=%d;", ewtsa->calib_boost_tm);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "HPF=%d;", ewtsa->hpf);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "SFR=%d;", ewtsa->calib_fs);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "EVE=%d;", ewtsa->event);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "FIFO=%d;", ewtsa->fifo_mode);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "SAA=%d;", ewtsa->calib_alway);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "W1CX=%d;", ewtsa->w1_thres_x);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "W1CY=%d;", ewtsa->w1_thres_y);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "W1CZ=%d;", ewtsa->w1_thres_z);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "BFR=%d;", ewtsa->calib_boost_fs);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "OTL=%d;", ewtsa->offset_lag);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "W1L=%d;", ewtsa->w1_thres_level);
    if (ret >= 0) {
        ret_len += ret;
    }

    ret = sprintf(buf+ret_len, "W1DM=%d;", ewtsa->w1_datamask);
    if (ret >= 0) {
        ret_len += ret;
    }

    return ret_len;
}

/* Set the gyro configurations */
static ssize_t ewtsa_config_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct i2c_client *l_client;
    struct ewtsa_t *ewtsa;
    char val_char[4] = {0};
    char sleep_val[4] = {0};
    unsigned char cmd[5] = {0};
    unsigned char cmd_size = 0;
    unsigned char val_size = 0;
    unsigned char sleep_flag = EWTSA_OFF;
    unsigned char sleep_mode = SLEEP_ALLRESET;
    unsigned char fifo_tmp;
    unsigned char event_tmp;
    int           loop;
    int           size_err;
    unsigned long val = 0;
    size_t        chk_cnt = 0;
    ssize_t       sleep_ret;

    if (dev == NULL) {
        printk(KERN_INFO "struct device null pointer\n");
        return -EIO;
    }
    l_client = to_i2c_client(dev);

    if (l_client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(l_client);

    /* input buffer check */
    if (buf == NULL) {
        dev_err(&ewtsa->client->dev, "buf pointer null!\n");
        return -EIO;
    }

    fifo_tmp = ewtsa->fifo_mode;
    event_tmp = ewtsa->event;

    if (strncmp(buf, "MAP", 3) == 0) {
        /* get register map information */
#if GYRO_REGISTER_MAP2
        if (strncmp(buf, "MAP2", MAP_NAME_SIZE) != 0) {
            dev_err(&ewtsa->client->dev, "register map unmatch!\n");
            return -EINVAL;
        }
#else /* GYRO_REGISTER_MAP2 */
        if (strncmp(buf, "MAP1", MAP_NAME_SIZE) != 0) {
            dev_err(&ewtsa->client->dev, "register map unmatch!\n");
            return -EINVAL;
        }
#endif /* GYRO_REGISTER_MAP2 */
        buf += MAP_NAME_SIZE + 1;
        chk_cnt += MAP_NAME_SIZE + 1;

        /* get communication interface */
#if GYRO_SPI_INTERFACE
        if (strncmp(buf, "SPI", INTERFACE_NAME_SIZE) != 0) {
            dev_err(&ewtsa->client->dev, "communication interface unmatch!\n");
            return -EINVAL;
        }
#else /* GYRO_SPI_INTERFACE */
        if (strncmp(buf, "I2C", INTERFACE_NAME_SIZE) != 0) {
            dev_err(&ewtsa->client->dev, "communication interface unmatch!\n");
            return -EINVAL;
        }
#endif /* GYRO_SPI_INTERFACE */
        buf += INTERFACE_NAME_SIZE + 1;
        chk_cnt += INTERFACE_NAME_SIZE + 1;
    }

    while (chk_cnt < count) {
        memset(cmd, 0x00, sizeof(cmd));
        memset(val_char, 0x00, sizeof(val_char));
        size_err = 0;

        for (loop = 0; (chk_cnt + loop) < count; loop++) {
            if (*(buf + loop) == '=') {
                break;
            }

            if (*(buf + loop) == ';') {
                dev_err(&ewtsa->client->dev, "command format err\n");
                return -EINVAL;
            }
        }

        if ((chk_cnt + loop) >= count) {
            break;
        }

        cmd_size = (unsigned char)loop;
        if (cmd_size == 0) {
            dev_err(&ewtsa->client->dev, "command nothing\n");
            return -EINVAL;
        }

        /* command size over? */
        if (loop > (sizeof(cmd) - 1)) {
            size_err = EWTSA_ON;
        }
        else {
            memcpy(cmd, buf, cmd_size);
        }
        buf += cmd_size + 1;
        chk_cnt += cmd_size + 1;

        for (loop = 0; (chk_cnt + loop) < count; loop++) {
            if (*(buf + loop) == ';') {
                break;
            }
        }

        if ((chk_cnt + loop) >= count) {
            break;
        }

        val_size = (unsigned char)loop;
        if (val_size == 0) {
            dev_err(&ewtsa->client->dev, "value nothing(cmd = %s)\n", cmd);
            return -EINVAL;
        }

        if (size_err != 0) {
            /* if command size is over, value ignore and next command check. */
            buf += val_size + 1;
            chk_cnt += val_size + 1;
            continue;
        }
        else if (loop > (sizeof(val_char) - 1)) {
            /* if value size is over, return format err. */
            size_err = EWTSA_ON;
        }
        else {
            memcpy(val_char, buf, val_size);
            size_err = strict_strtoul(val_char, 10, &val);
        }

        if (size_err != 0) {
            dev_err(&ewtsa->client->dev, "value format err(cmd = %s)\n", cmd);
            return -EINVAL;
        }
        buf += val_size + 1;
        chk_cnt += val_size + 1;

        if (cmd_size == CMD_SIZE_3) {
            if (strncmp((char*)cmd, "SLP", CMD_SIZE_3) == 0) {
                sleep_flag = EWTSA_ON;
                memcpy(sleep_val, val_char, sizeof(sleep_val));
                if (val == 0) {
                    sleep_mode = SLEEP_OFF;
                }
            }
            else if (strncmp((char*)cmd, "DLY", CMD_SIZE_3) == 0) {
                if ((val < DELAY_THRES_MIN) || (val > DELAY_THRES_MAX)) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }

                ewtsa->delay = val;
                ewtsa->calib_rst = EWTSA_ON;
                ewtsa->reset_param |= RST_REG_INIT_DELAY;
            }
            else if (strncmp((char*)cmd, "RNG", CMD_SIZE_3) == 0) {
                if (val > DYNRANGE_2000) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->range = (unsigned char)val;
                ewtsa->calib_rst = EWTSA_ON;
                ewtsa->reset_param |= RST_REG_INIT_RANGE;
            }
            else if (strncmp((char*)cmd, "SOC", CMD_SIZE_3) == 0) {
                if ((val != EWTSA_ON) && (val != EWTSA_OFF)) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->calib = (unsigned char)val;
                ewtsa->calib_rst = EWTSA_ON;
            }
            else if (strncmp((char*)cmd, "SBT", CMD_SIZE_3) == 0) {
                if (val > BOOST_TIME_100MS) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->calib_boost_tm = (unsigned char)val;
            }
            else if (strncmp((char*)cmd, "HPF", CMD_SIZE_3) == 0) {
                if ((val != HPF_001HZ) && (val != HPF_002HZ)) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->hpf = (unsigned char)val;
                ewtsa->calib_rst = EWTSA_ON;
            }
            else if (strncmp((char*)cmd, "SFR", CMD_SIZE_3) == 0) {
                if ((val != CALIB_FS_62_5HZ) && (val != CALIB_FS_31_25HZ)) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->calib_fs = (unsigned char)val;
                ewtsa->calib_rst = EWTSA_ON;
            }
            else if (strncmp((char*)cmd, "EVE", CMD_SIZE_3) == 0) {
                if ((val != EWTSA_POLLING_MODE) && (val != EWTSA_INTERUPT_MODE)) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
#if USE_GPIO_DRDY_PIN
                event_tmp = (unsigned char)val;
#else
                event_tmp = (unsigned char)EWTSA_POLLING_MODE;
#endif
                ewtsa->calib_rst = EWTSA_ON;
                ewtsa->reset_param = RST_REG_INIT_ALL;
            }
            else if (strncmp((char*)cmd, "SAA", CMD_SIZE_3) == 0) {
                if ((val != EWTSA_ON) && (val != EWTSA_OFF)) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->calib_alway = (unsigned char)val;
                ewtsa->calib_rst = EWTSA_ON;
            }
            else if (strncmp((char*)cmd, "BFR", CMD_SIZE_3) == 0) {
                if ((val != BOOST_FS_48KHZ) && (val != BOOST_FS_16KHZ)) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->calib_boost_fs = (unsigned char)val;
            }
            else if (strncmp((char*)cmd, "OTL", CMD_SIZE_3) == 0) {
                if (val > LAG_20SMP) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->offset_lag = (unsigned char)val;
                ewtsa->calib_rst = EWTSA_ON;
            }
            else if (strncmp((char*)cmd, "W1L", CMD_SIZE_3) == 0) {
                if (val > W1_THRES_LEVEL_15DPS) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->w1_thres_level = (unsigned char)val;
                ewtsa->calib_rst = EWTSA_ON;
            }
            else {
                dev_info(&ewtsa->client->dev, "unknown command(%s)\n", cmd);
            }
        }
        else if (cmd_size == CMD_SIZE_4) {
            if (strncmp((char*)cmd, "FIFO", CMD_SIZE_4) == 0) {
                if ((val != BYPASS_MODE) && (val != FIFO_MODE)) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                fifo_tmp = (unsigned char)val;
                ewtsa->calib_rst = EWTSA_ON;
                ewtsa->reset_param = RST_REG_INIT_ALL;
            }
            else if (strncmp((char*)cmd, "W1CX", CMD_SIZE_4) == 0) {
                if (val > THRES_CENTER_MAX) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->w1_thres_x = (unsigned short)val;
                ewtsa->calib_rst = EWTSA_ON;
            }
            else if (strncmp((char*)cmd, "W1CY", CMD_SIZE_4) == 0) {
                if (val > THRES_CENTER_MAX) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->w1_thres_y = (unsigned short)val;
                ewtsa->calib_rst = EWTSA_ON;
            }
            else if (strncmp((char*)cmd, "W1CZ", CMD_SIZE_4) == 0) {
                if (val > THRES_CENTER_MAX) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->w1_thres_z = (unsigned short)val;
                ewtsa->calib_rst = EWTSA_ON;
            }
            else if (strncmp((char*)cmd, "W1DM", CMD_SIZE_4) == 0) {
                if ((val != MASK_CLIP) && (val != MASK_NON_CLIP)) {
                    dev_err(&ewtsa->client->dev, "invalid value = %ld\n", val);
                    return -EINVAL;
                }
                ewtsa->w1_datamask = (unsigned char)val;
                ewtsa->calib_rst = EWTSA_ON;
            }
            else {
                dev_info(&ewtsa->client->dev, "unknown command(%s)\n", cmd);
            }
        }
        else {
            dev_info(&ewtsa->client->dev, "unknown command(%s)\n", cmd);
        }
    }

    if (sleep_flag == EWTSA_ON) {
        if (sleep_mode == SLEEP_OFF) {
            /* all reset sleep mode */
            val_char[0] = '2';
            val_char[1] = '\0';
            sleep_ret = ewtsa_sleep_store(dev, attr, val_char, 1);
            if (sleep_ret != 1) {
                dev_err(&ewtsa->client->dev, "ewtsa_sleep_store err(%d)\n", sleep_ret);
                return sleep_ret;
            }
            ewtsa->event = event_tmp;
            ewtsa->fifo_mode = fifo_tmp;
        }
        sleep_ret = ewtsa_sleep_store(dev, attr, sleep_val, sizeof(sleep_val));
        if (sleep_ret != sizeof(sleep_val)) {
            dev_err(&ewtsa->client->dev, "ewtsa_sleep_store err(%d)\n", sleep_ret);
            return sleep_ret;
        }
        ewtsa->event = event_tmp;
        ewtsa->fifo_mode = fifo_tmp;
    }
    else {
        if (ewtsa->calib_rst == EWTSA_ON) {
            sleep_mode = ewtsa->sleep;

            /* all reset sleep mode */
            val_char[0] = '2';
            val_char[1] = '\0';
            sleep_ret = ewtsa_sleep_store(dev, attr, val_char, 1);
            if (sleep_ret != 1) {
                dev_err(&ewtsa->client->dev, "ewtsa_sleep_store err(%d)\n", sleep_ret);
                return sleep_ret;
            }

            ewtsa->event = event_tmp;
            ewtsa->fifo_mode = fifo_tmp;

            if (sleep_mode == SLEEP_OFF) {
                /* sleep off mode */
                val_char[0] = '0';
                val_char[1] = '\0';
                sleep_ret = ewtsa_sleep_store(dev, attr, val_char, 1);
                if (sleep_ret != 1) {
                    dev_err(&ewtsa->client->dev, "ewtsa_sleep_store err(%d)\n", sleep_ret);
                    return sleep_ret;
                }
            }
        }
    }

    return count;
}

/* calibration & register settings */
static void EWTSA_system_restart(struct ewtsa_t *ewtsa)
{
    unsigned char  val;
    unsigned char  smpl;
    unsigned char  number;
    unsigned char  fifo_type;
    s32            err;
#if GYRO_VER3_ES1
    signed long    ret;
    signed long    timeout;
#endif

    if (ewtsa == NULL) {
         printk(KERN_INFO "EWTSA_system_restart:ewtsa null pointer\n");
        return;
    }

    err = i2c_smbus_write_byte_data(ewtsa->client, REG_DIGITAL_CAL, DIGITAL_CAL_DISABLE);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_DIGITAL_CAL write err(data = 0x%02x, ret = %d)\n", DIGITAL_CAL_DISABLE, err);
        return;
    }

#if GYRO_VER3_ES1
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_REFERENCE, 0x00);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_REFERENCE write err(data = 0x00, ret = %d)\n", err);
        return;
    }

    val = (unsigned char)(CTRL_REG2_REF_MODE | CTRL_REG2_HPCF);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_CTRL_REG2, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_CTRL_REG2 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    err = i2c_smbus_read_byte_data(ewtsa->client, REG_REFERENCE);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_REFERENCE read err(ret = %d)\n", err);
        return;
    }

    val = SOC_CNT2_SOCCLK_ON;
    val |= (unsigned char)(CALIB_FS_62_5HZ << 7);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SOC_CNT2, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SOC_CNT2 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    /* Wait reference mode setting */
    ret = (signed long)((REFERENCE_WAIT * HZ)/1000);
    ret += (signed long)(1000 / HZ);
    while (ret > 0) {
        timeout = ret;
        set_current_state(TASK_UNINTERRUPTIBLE);
        ret = schedule_timeout(timeout);
    }

    val = SOC_CNT2_SOCCLK_OFF;
    val |= (unsigned char)(CALIB_FS_62_5HZ << 7);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SOC_CNT2, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SOC_CNT2 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    val = (unsigned char)(CTRL_REG2_NOR_MODE | CTRL_REG2_HPCF);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_CTRL_REG2, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_CTRL_REG2 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }
#endif /* GYRO_VER3_ES1 */

    if (ewtsa->event == EWTSA_POLLING_MODE) {
        fifo_type = BYPASS_MODE;
    }
    else {
        fifo_type = ewtsa->fifo_mode;
    }

    if ((ewtsa->reset_param & RST_REG_INIT_ALL) != RST_REG_INIT_OFF) {
        if (fifo_type == BYPASS_MODE) {
            if (ewtsa->delay < DELAY_THRES_4) {
                if (ewtsa->delay >= DELAY_THRES_3) {
                    ewtsa->dlpf = DELAY_DLPF_3;
                } else {
                    ewtsa->dlpf = DELAY_DLPF_2;
                }
            } else {
                if (ewtsa->delay >= DELAY_THRES_6) {
                    ewtsa->dlpf = DELAY_DLPF_6;
                } else if (ewtsa->delay >= DELAY_THRES_5) {
                    ewtsa->dlpf = DELAY_DLPF_5;
                } else {
                    ewtsa->dlpf = DELAY_DLPF_4;
                }
            }
        }
        else {
            if (ewtsa->delay <= DELAY_THRES_FIFO) {
                ewtsa->dlpf = DELAY_DLPF_1;
            }
            else {
                ewtsa->dlpf = DELAY_DLPF_2;
            }
        }
        val = (unsigned char)((unsigned char)(ewtsa->range << 3) | ewtsa->dlpf);

        err = i2c_smbus_write_byte_data(ewtsa->client, REG_FS_DLPF_CTRL, val);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_FS_DLPF_CTRL write err(data = 0x%02x, ret = %d)\n", val, err);
            return;
        }
    }

    if ((ewtsa->reset_param & RST_REG_INIT_DELAY) != RST_REG_INIT_OFF) {
        err = i2c_smbus_write_byte_data(ewtsa->client, REG_CTRL_REG5, CTRL_REG5_INIT);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_CTRL_REG5 write err(data = 0x%02x, ret = %d)\n", CTRL_REG5_INIT, err);
            return;
        }

        if (fifo_type != BYPASS_MODE) {
            val = FIFO_DRDY_DITH_EN;
        }
        else {
            val = (unsigned char)(fifo_type << 5);
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_FIFO_CTRL2, val);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_FIFO_CTRL2 write err(data = 0x%02x, ret = %d)\n", val, err);
                return;
            }
            val = FIFO_DRDY_INIT;
        }

        err = i2c_smbus_write_byte_data(ewtsa->client, REG_FIFO_DRDY, val);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_FIFO_DRDY write err(data = 0x%02x, ret = %d)\n", val, err);
            return;
        }

        if (fifo_type == BYPASS_MODE) {
            if (ewtsa->event == EWTSA_POLLING_MODE) {
                smpl = DATA_RATE_1 - 1; /* SMPL = 0 */
            }
            else {
                if (ewtsa->delay <= DELAY_THRES_2) {
                    smpl =  DELAY_INTMIN_THRES;
                }
                else {
                    smpl = (unsigned char)(ewtsa->delay - 1);
                }
            }
            ewtsa->fifo_num = 1;
        }
        else {
            if (ewtsa->delay <= DELAY_THRES_FIFO) {
                if (ewtsa->delay > FIFO_MIN_DELAY) {
                    number = (unsigned char)(ewtsa->delay / DATA_RATE_5);
                    if ((ewtsa->delay % DATA_RATE_5) != 0) {
                        number++;
                    }
                }
                else {
                    number = 2;
                }
                smpl = DATA_RATE_5 - 1; /* SMPL = 4 */
            }
            else {
                number = (unsigned char)(ewtsa->delay / DATA_RATE_10);
                if ((ewtsa->delay % DATA_RATE_10) != 0) {
                    number++;
                }
                smpl = DATA_RATE_10 - 1; /* SMPL = 9 */
            }

            val = (unsigned char)(fifo_type << 5);
            val |= number;
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_FIFO_CTRL2, val);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_FIFO_CTRL2 write err(data = 0x%02x, ret = %d)\n", val, err);
                return;
            }
            ewtsa->fifo_num = number;

            val = CTRL_REG3_WTM_EN;
            err = i2c_smbus_write_byte_data(ewtsa->client, REG_CTRL_REG3, val);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_CTRL_REG3 write err(data = 0x%02x, ret = %d)\n", val, err);
                return;
            }

            err = i2c_smbus_write_byte_data(ewtsa->client, REG_CTRL_REG5, CTRL_REG5_FIFO_EN);
            if (err < 0) {
                dev_err(&ewtsa->client->dev, "REG_CTRL_REG5 write err(data = 0x%02x, ret = %d)\n", CTRL_REG5_FIFO_EN, err);
                return;
            }
        }
        err = i2c_smbus_write_byte_data(ewtsa->client, REG_DATA_RATE, smpl);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_DATA_RATE write err(data = 0x%02x, ret = %d)\n", smpl, err);
            return;
        }
    }
    ewtsa->reset_param = RST_REG_INIT_OFF;

    if (ewtsa->calib == EWTSA_ON) {
        EWTSA_set_calibration(ewtsa);
    }
    else {
        err = i2c_smbus_write_byte_data(ewtsa->client, REG_SLEEP_CTRL, SLEEP_CTRL_SLEEP);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_SLEEP_CTRL write err(data = 0x%02x, ret = %d)\n", SLEEP_CTRL_SLEEP, err);
            return;
        }

#if GYRO_VER3_ES1
        err = i2c_smbus_write_byte_data(ewtsa->client, REG2_CTRL_REG1, CTRL_REG1_ACTIVATE);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG2_CTRL_REG1 write err(data = 0x%02x, ret = %d)\n", CTRL_REG1_ACTIVATE, err);
            return;
        }
#else /* GYRO_VER3_ES1 */
        err = i2c_smbus_write_byte_data(ewtsa->client, REG_SLEEP_CTRL, SLEEP_CTRL_ACTIVATE);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_SLEEP_CTRL write err(data = 0x%02x, ret = %d)\n", SLEEP_CTRL_ACTIVATE, err);
            return;
        }
#endif /* GYRO_VER3_ES1 */

        /* FIFO clear */
        err = i2c_smbus_write_byte_data(ewtsa->client, REG_FIFO_CTRL1, FIFO_CTRL1_CLEAR);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_FIFO_CTRL1 write err(data = 0x02, ret = %d)\n", err);
            return;
        }
    }

    ewtsa->calib_rst = EWTSA_OFF;
}

static void EWTSA_set_calibration(struct ewtsa_t *ewtsa)
{
    unsigned char  val;
    s32            err;
    signed long    ret;
    signed long    l_timeout;
    unsigned long  center;
    static const unsigned short ewtsa_boost_time[4] = {
        EWTSA_BOOST_TIME_0, EWTSA_BOOST_TIME_1, EWTSA_BOOST_TIME_2, EWTSA_BOOST_TIME_3
    };
    static const unsigned char ewtsa_w1_threshold_level[4] = {
        EWTSA_W1_THRES_LV_0, EWTSA_W1_THRES_LV_1, EWTSA_W1_THRES_LV_2, EWTSA_W1_THRES_LV_3
    };
    static const unsigned short ewtsa_dynamic_range[4] = {
        EWTSA_RANGE_0, EWTSA_RANGE_1, EWTSA_RANGE_2, EWTSA_RANGE_3
    };

    if (ewtsa == NULL) {
         printk(KERN_INFO "EWTSA_system_restart:ewtsa null pointer\n");
        return;
    }

    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SOC_CNT1, SOC_CNT1_WIN_DIS | SOC_CNT1_BOOST_ON);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SOC_CNT1 write err(data = 0x%02x, ret = %d)\n", SOC_CNT1_WIN_DIS | SOC_CNT1_BOOST_ON, err);
        return;
    }

    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SLEEP_CTRL, SLEEP_CTRL_SLEEP);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SLEEP_CTRL write err(data = 0x%02x, ret = %d)\n", SLEEP_CTRL_SLEEP, err);
        return;
    }

#if GYRO_VER3_ES1
    err = i2c_smbus_write_byte_data(ewtsa->client, REG2_CTRL_REG1, CTRL_REG1_ACTIVATE);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG2_CTRL_REG1 write err(data = 0x%02x, ret = %d)\n", CTRL_REG1_ACTIVATE, err);
        return;
    }
#else /* GYRO_VER3_ES1 */
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SLEEP_CTRL, SLEEP_CTRL_ACTIVATE);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SLEEP_CTRL write err(data = 0x%02x, ret = %d)\n", SLEEP_CTRL_ACTIVATE, err);
        return;
    }
#endif /* GYRO_VER3_ES1 */

    if (ewtsa_w1_threshold_level[ewtsa->w1_thres_level] > ewtsa->w1_thres_x) {
        center = ewtsa_w1_threshold_level[ewtsa->w1_thres_level] + 1;
    }
    else {
        center = ewtsa->w1_thres_x;
    }
    center *= W1_THRES_CENTER_COEF;
    center /= ewtsa_dynamic_range[ewtsa->range];
    val = (unsigned char)((0xFF00 & center) >> 8);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_REFERENCE, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_REFERENCE write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    val = (unsigned char)(CTRL_REG2_REF_MODE | CTRL_REG2_HPCF);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_CTRL_REG2, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_CTRL_REG2 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    val = SOC_CNT2_SOCCLK_ON;
    val |= (unsigned char)(CALIB_FS_62_5HZ << 7);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SOC_CNT2, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SOC_CNT2 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    /* Wait reference mode setting */
    ret = (signed long)((REFERENCE_WAIT * HZ)/1000);
    ret += (signed long)(1000 / HZ);
    while (ret > 0) {
        l_timeout = ret;
        set_current_state(TASK_UNINTERRUPTIBLE);
        ret = schedule_timeout(l_timeout);
    }

    val = SOC_CNT2_SOCCLK_OFF;
    val |= (unsigned char)(CALIB_FS_62_5HZ << 7);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SOC_CNT2, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SOC_CNT2 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    val = (unsigned char)(CTRL_REG2_NOR_MODE | CTRL_REG2_HPCF);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_CTRL_REG2, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_CTRL_REG2 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }
#if GYRO_VER3_ES3 || GYRO_VER3_ES4 //2012.01.25 for ES3 & ES4
    val = (unsigned char)(0x40 | SOC_CNT1_BOOST_ON);
#else
    val = (unsigned char)((unsigned char)(ewtsa->w1_datamask << 6) | SOC_CNT1_BOOST_ON);
#endif
    val |= (unsigned char)(ewtsa->w2_thres_en << 5);
    val |= (unsigned char)(ewtsa->w1_thres_en << 4);
    val |= (unsigned char)(ewtsa->calib_boost_tm << 2);
    val |= (unsigned char)(ewtsa->calib_boost_fs << 1);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SOC_CNT1, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SOC_CNT1 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    val = (unsigned char)(SOC_CNT2_SOCCLK_ON | ewtsa->w1_thres_level);
    val |= (unsigned char)(ewtsa->calib_fs << 7);
    val |= (unsigned char)(ewtsa->offset_lag << 4);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SOC_CNT2, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SOC_CNT2 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    val = (unsigned char)(ewtsa->hpf << 7);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SLFSLP_CNT, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SLFSLP_CNT write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    if (ewtsa_w1_threshold_level[ewtsa->w1_thres_level] > ewtsa->w1_thres_x) {
        center = ewtsa_w1_threshold_level[ewtsa->w1_thres_level] + 1;
    }
    else {
        center = ewtsa->w1_thres_x;
    }
    center *= W1_THRES_CENTER_COEF;
    center /= ewtsa_dynamic_range[ewtsa->range];
    val = (unsigned char)((0xFF00 & center) >> 8);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_W1THX_H, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_W1THX_H write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    val = (unsigned char)(0xFF & center);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_W1THX_L, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_W1THX_L write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    if (ewtsa_w1_threshold_level[ewtsa->w1_thres_level] > ewtsa->w1_thres_y) {
        center = ewtsa_w1_threshold_level[ewtsa->w1_thres_level] + 1;
    }
    else {
        center = ewtsa->w1_thres_y;
    }
    center *= W1_THRES_CENTER_COEF;
    center /= ewtsa_dynamic_range[ewtsa->range];
    val = (unsigned char)((0xFF00 & center) >> 8);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_W1THY_H, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_W1THY_H write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    val = (unsigned char)(0xFF & center);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_W1THY_L, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_W1THY_L write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    if (ewtsa_w1_threshold_level[ewtsa->w1_thres_level] > ewtsa->w1_thres_z) {
        center = ewtsa_w1_threshold_level[ewtsa->w1_thres_level] + 1;
    }
    else {
        center = ewtsa->w1_thres_z;
    }
    center *= W1_THRES_CENTER_COEF;
    center /= ewtsa_dynamic_range[ewtsa->range];
    val = (unsigned char)((0xFF00 & center) >> 8);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_W1THZ_H, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_W1THZ_H write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    val = (unsigned char)(0xFF & center);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_W1THZ_L, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_W1THZ_L write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    err = i2c_smbus_write_byte_data(ewtsa->client, REG_DIGITAL_CAL, DIGITAL_CAL_ENABLE);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_DIGITAL_CAL write err(data = 0x%02x, ret = %d)\n", DIGITAL_CAL_ENABLE, err);
        return;
    }

    /* Wait calibration boost */
    ret = (signed long)((ewtsa_boost_time[ewtsa->calib_boost_tm] * HZ)/1000);
    ret += (signed long)(1000 / HZ);
    while (ret > 0) {
        l_timeout = ret;
        set_current_state(TASK_UNINTERRUPTIBLE);
        ret = schedule_timeout(l_timeout);
    }

    /* boost stop */
    val = (unsigned char)(ewtsa->w1_datamask << 6);
    val |= (unsigned char)(ewtsa->w2_thres_en << 5);
    val |= (unsigned char)(ewtsa->w1_thres_en << 4);
    val |= (unsigned char)(ewtsa->calib_boost_tm << 2);
    val |= (unsigned char)(ewtsa->calib_boost_fs << 1);
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_SOC_CNT1, val);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_SOC_CNT1 write err(data = 0x%02x, ret = %d)\n", val, err);
        return;
    }

    if ((ewtsa->calib_alway != EWTSA_ON) ||
        ((ewtsa->w1_thres_en == THRESHOLD_OFF) && (ewtsa->w2_thres_en == THRESHOLD_OFF))) {
        val = (unsigned char)(SOC_CNT2_SOCCLK_OFF | ewtsa->w1_thres_level);
        err = i2c_smbus_write_byte_data(ewtsa->client, REG_SOC_CNT2, val);
        if (err < 0) {
            dev_err(&ewtsa->client->dev, "REG_SOC_CNT2 write err(data = 0x%02x, ret = %d)\n", val, err);
            return;
        }
    }

    /* FIFO clear */
    err = i2c_smbus_write_byte_data(ewtsa->client, REG_FIFO_CTRL1, FIFO_CTRL1_CLEAR);
    if (err < 0) {
        dev_err(&ewtsa->client->dev, "REG_FIFO_CTRL1 write err(data = 0x02, ret = %d)\n", err);
        return;
    }
    return;
}

/* Read the angular data from the registers */
static void EWTSA_report_abs(struct ewtsa_t *ewt)
{
    unsigned short x = 0, y = 0, z = 0, temp = 0;
    s16 send_x, send_y, send_z, send_temp;
    unsigned char reg = REG_MBURST_ALL;
    unsigned char reg2[32][8];
    int err;
    struct i2c_msg msg[2];
#if GYRO_MOUNT_SWAP_XY
    s16 tmp;
#endif /* GYRO_MOUNT_SWAP_XY */
    unsigned char diag_stat = 0;
    unsigned char loop;
    unsigned long l_flags = 0;
    signed long cal_tmp[4] = {0};
    unsigned char get_num = 0;
    unsigned char read_num = 0;

    if (ewt == NULL) {
        printk(KERN_INFO "struct ewtsa_t null pointer\n");
        return;
    }

    if (ewt->event == EWTSA_POLLING_MODE) {
        mutex_lock(&ewt->mlock);
    }
    else {
        if (ewt->fifo_mode == FIFO_MODE) {
            err = i2c_smbus_read_byte_data(ewt->client, REG_FIFO_DM);
            if (err < 0) {
                dev_err(&ewt->client->dev, "REG_FIFO_DM read err(%d)\n", err);
                err = 0;
            }
            read_num = (unsigned char)err;
            if (((unsigned char)(read_num & 0x20) >> 5) == 1) {
                /* data nothing */
                err = i2c_smbus_read_byte_data(ewt->client, REG_INT_STAT);
                if (err < 0) {
                    dev_err(&ewt->client->dev, "REG_INT_STAT read err(%d)\n", err);
                }
                return;
            }
            read_num &= 0x1F;

            if (ewt->fifo_num > read_num) {
                err = i2c_smbus_read_byte_data(ewt->client, REG_INT_STAT);
                if (err < 0) {
                    dev_err(&ewt->client->dev, "REG_INT_STAT read err(%d)\n", err);
                }
                return;
            }
        }
    }

    msg[0].addr = ewt->client->addr;
    msg[0].flags = 0;
    msg[0].len = 1;
    msg[0].buf = &reg;

    msg[1].addr = ewt->client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = (unsigned short)(8 * ewt->fifo_num);
    msg[1].buf = &reg2[0][0];

    err = i2c_transfer(ewt->client->adapter, msg, 2);
    if (err < 0) {
        dev_err(&ewt->client->dev, "REG_MBURST_ALL read err(%d)\n", err);
        memset(reg2, 0x00, sizeof(reg2));
    }

    for (loop = 0; loop < ewt->fifo_num; loop++) {
        x = (unsigned short)(0xFF & reg2[loop][3]);
        x |= (unsigned short)(0xFF00 & (unsigned short)(reg2[loop][2] << 8));
        y = (unsigned short)(0xFF & reg2[loop][5]);
        y |= (unsigned short)(0xFF00 & (unsigned short)(reg2[loop][4] << 8));
        z = (unsigned short)(0xFF & reg2[loop][7]);
        z |= (unsigned short)(0xFF00 & (unsigned short)(reg2[loop][6] << 8));
        temp = (unsigned short)(0xFF & reg2[loop][1]);
        temp |= (unsigned short)(0xFF00 & (unsigned short)(reg2[loop][0] << 8));

        cal_tmp[0] += (signed short)x;
        cal_tmp[1] += (signed short)y;
        cal_tmp[2] += (signed short)z;
        cal_tmp[3] += (signed short)temp;
        get_num++;
    }

    err = i2c_smbus_read_byte_data(ewt->client, REG_STATUS_REG);
    if (err < 0) {
        dev_err(&ewt->client->dev, "REG_STATUS_REG read err(%d)\n", err);
        err = 0;
    }
    reg = (unsigned char)err;
    if ((reg & STATUS_REG_OVERRUN) != 0) {
        err = i2c_smbus_write_byte_data(ewt->client, REG_FIFO_CTRL1, FIFO_CTRL1_CLEAR);
        if (err < 0) {
            dev_err(&ewt->client->dev, "REG_FIFO_CTRL1 read err(%d)\n", err);
        }
    }

    if (ewt->int_skip == EWSTA_INT_SKIP) {
        ewt->int_skip = EWSTA_INT_CLEAR;
        err = i2c_smbus_read_byte_data(ewt->client, REG_INT_STAT);
        if (err < 0) {
            dev_err(&ewt->client->dev, "REG_INT_STAT read err(%d)\n", err);
        }
        return;
    }

    if (get_num != 0) {
        if (ewt->fifo_data_flag == EWTSA_ON) {
            cal_tmp[0] += ewt->fifo_lastdata[0];
            cal_tmp[1] += ewt->fifo_lastdata[1];
            cal_tmp[2] += ewt->fifo_lastdata[2];
            cal_tmp[3] += ewt->fifo_lastdata[3];
            get_num++;
        }

        /* fifo mode only */
        if (get_num > 1) {
            ewt->fifo_lastdata[0] = (signed short)x;
            ewt->fifo_lastdata[1] = (signed short)y;
            ewt->fifo_lastdata[2] = (signed short)z;
            ewt->fifo_lastdata[3] = (signed short)temp;
            ewt->fifo_data_flag = EWTSA_ON;
        }

        send_x = (signed short)(cal_tmp[0] / get_num);
        send_y = (signed short)(cal_tmp[1] / get_num);
        send_z = (signed short)(cal_tmp[2] / get_num);
        send_temp = (signed short)(cal_tmp[3] / get_num);
    }
    else {
        ewt->fifo_lastdata[0] = 0;
        ewt->fifo_lastdata[1] = 0;
        ewt->fifo_lastdata[2] = 0;
        ewt->fifo_lastdata[3] = 0;
        ewt->fifo_data_flag = EWTSA_OFF;

        send_x = 0;
        send_y = 0;
        send_z = 0;
        send_temp = 0;
        err = i2c_smbus_write_byte_data(ewt->client, REG_FIFO_CTRL1, FIFO_CTRL1_CLEAR);
        if (err < 0) {
            dev_err(&ewt->client->dev, "REG_FIFO_CTRL1 read err(%d)\n", err);
        }
    }

#if GYRO_MOUNT_SWAP_XY
    tmp = send_x;
    send_x = send_y;
    send_y = tmp;
#endif /* GYRO_MOUNT_SWAP_XY */
#if GYRO_MOUNT_REVERSE_X
    send_x *= -1;
#endif /* GYRO_MOUNT_REVERSE_X */
#if GYRO_MOUNT_REVERSE_Y
    send_y *= -1;
#endif /* GYRO_MOUNT_REVERSE_Y */
#if GYRO_MOUNT_REVERSE_Z
    send_z *= -1;
#endif /* GYRO_MOUNT_REVERSE_Z */

    if (ewt->event != EWTSA_POLLING_MODE) {
        err = i2c_smbus_read_byte_data(ewt->client, REG_INT_STAT);
        if (err < 0) {
            dev_err(&ewt->client->dev, "REG_INT_STAT read err(%d)\n", err);
            err = 0;
        }
        diag_stat = (unsigned char)err;
        spin_lock_irqsave(&ewt->slock, l_flags);
    }

    input_report_abs(ewt->input_dev, ABS_X, send_x); 
    input_report_abs(ewt->input_dev, ABS_Y, send_y);
    input_report_abs(ewt->input_dev, ABS_Z, send_z);
    input_report_abs(ewt->input_dev, ABS_RX, send_temp);

#if USE_GPIO_DIAG_PIN
    if (ewt->event == EWTSA_POLLING_MODE) {
        input_report_abs(ewt->input_dev, ABS_RY, gpio_get_value(DIAG_PIN));
    }
    else {
        diag_stat = (unsigned char)((unsigned char)(diag_stat & 0x04) >> 2);
        input_report_abs(ewt->input_dev, ABS_RY, diag_stat);
    }
#else
    diag_stat = (unsigned char)((unsigned char)(diag_stat & 0x04) >> 2);
    input_report_abs(ewt->input_dev, ABS_RY, diag_stat);
#endif
    input_sync(ewt->input_dev);

    if (ewt->event == EWTSA_POLLING_MODE) {
        mutex_unlock(&ewt->mlock);
    }
    else {
        spin_unlock_irqrestore(&ewt->slock, l_flags);
    }
}

static void EWTSA_dev_poll(struct work_struct *work)
{
    struct ewtsa_t *ewt;

    if (work == NULL) {
        printk(KERN_INFO "struct work_struct null pointer\n");
        return;
    }
    ewt = container_of(work, struct ewtsa_t, input_work.work);

    if (ewt->sleep == SLEEP_OFF) {
      EWTSA_report_abs(ewt);
    }
}

/* Interrupt handler */
#if USE_GPIO_DRDY_PIN
static irqreturn_t EWTSA_interrupt(int irq, void *dev_id)
{
    struct ewtsa_t *ewtsa = dev_id;

    if (ewtsa == NULL) {
        printk(KERN_INFO "struct ewtsa_t null pointer\n");
        return IRQ_RETVAL(1);
    }

    if (irq == gpio_to_irq(DRDY_PIN)) {
        if (ewtsa->sleep != SLEEP_OFF) {
            return IRQ_RETVAL(1);
        }
        EWTSA_report_abs(ewtsa);
    }
    return IRQ_RETVAL(1);
}
#endif

static void EWTSA_input_work_func(struct work_struct *work)
{
    struct ewtsa_t *ewt;
    volatile unsigned long start_jiff = jiffies, delay_jiff, next_jiff;

    if (work == NULL) {
        printk(KERN_INFO "struct work_struct null pointer\n");
        return;
    }
    ewt = container_of(work, struct ewtsa_t, input_work.work);

    if (ewt->sleep == SLEEP_OFF) {
        EWTSA_dev_poll(work);

        /* adjust polling interval */
        delay_jiff = jiffies;
        if (start_jiff > delay_jiff) {               /* jiffies overflow */
            delay_jiff += 0xFFFFFFFFUL - start_jiff;
        }
        else {
            delay_jiff -= start_jiff;
        }
        next_jiff = msecs_to_jiffies(ewt->delay);
        /* for delay_jiff > next_jiff */
        delay_jiff %= next_jiff;
        next_jiff -= delay_jiff;

        queue_delayed_work(ewt->workqueue,
            &ewt->input_work,
            next_jiff);
    }
}

/* Suspend function to register device */
static int EWTSA_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct ewtsa_t *ewt;
    s32             ret;

    if (client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewt = i2c_get_clientdata(client);

    if (ewt->event == EWTSA_POLLING_MODE) {
        cancel_delayed_work(&ewt->input_work);
    }
    else {
        /* disable interrupt */
        ret = i2c_smbus_write_byte_data(ewt->client, REG_INT_CTRL, INT_CTRL_INT_DISABLE);
        if (ret < 0) {
            dev_err(&ewt->client->dev, "REG_INT_CTRL write err(ret = %d)\n", ret);
        }

        if (ewt->sleep == SLEEP_OFF) {
#if USE_GPIO_DRDY_PIN
            free_irq(gpio_to_irq(DRDY_PIN), ewt);
            /* keep sleep status for resume  */
#endif
        }
        else {
            ewt->sleep = SLEEP_ALLRESET;
        }
    }
#if USE_GPIO_WINDOW_PIN
    gpio_free(WINDOW_PIN);
#endif
#if USE_GPIO_DRDY_PIN
    gpio_free(DRDY_PIN);
#endif
#if USE_GPIO_DIAG_PIN
    gpio_free(DIAG_PIN);
#endif
    ewt->calib_rst = EWTSA_ON;
    ewt->reset_param = RST_REG_INIT_ALL;
    ret = i2c_smbus_write_byte_data(ewt->client, REG_SLEEP_CTRL, SLEEP_CTRL_ALLRSTSLEEP);
    if (ret < 0) {
        dev_err(&ewt->client->dev, "REG_SLEEP_CTRL write err(ret = %d)\n", ret);
    }
    return 0;
}

/* Resume function to register device */
static int EWTSA_resume(struct i2c_client *client)
{
    struct ewtsa_t *ewt;
    int             ret;

    if (client == NULL) {
        printk(KERN_INFO "struct i2c_client null pointer\n");
        return -EIO;
    }
    ewt = i2c_get_clientdata(client);

#if USE_GPIO_WINDOW_PIN
    ret = gpio_request(WINDOW_PIN, "WINDOW");
    ret += gpio_direction_input(WINDOW_PIN);
#else
    ret = 0;
#endif
#if USE_GPIO_DIAG_PIN
    ret += gpio_request(DIAG_PIN, "DIAG");
    ret += gpio_direction_input(DIAG_PIN);
#endif
    if ((ewt->sleep != SLEEP_OFF) || (ret != 0)) {
        return 0;
    }

#if GYRO_VER3_ES1
    ret = i2c_smbus_write_byte_data(ewt->client, REG2_CTRL_REG1, CTRL_REG1_ACTIVATE);
#else /* GYRO_VER3_ES1 */
    ret = i2c_smbus_write_byte_data(ewt->client, REG_SLEEP_CTRL, SLEEP_CTRL_ACTIVATE);
#endif /* GYRO_VER3_ES1 */

    if (ret >= 0) {
        EWTSA_system_restart(ewt);

        if (ewt->event == EWTSA_POLLING_MODE) {
            queue_delayed_work(ewt->workqueue,
                &ewt->input_work,
                msecs_to_jiffies(ewt->delay));
        }
        else {
            /* enable interrupt */
#if USE_GPIO_DRDY_PIN
            ret = gpio_request(DRDY_PIN, "DRDY");
            ret += gpio_direction_input(DRDY_PIN);
            ret += request_threaded_irq(gpio_to_irq(DRDY_PIN), NULL, EWTSA_interrupt, IRQF_TRIGGER_RISING, DEVICE_NAME, ewt);

            if (ret == 0) {
                ewt->int_skip = EWSTA_INT_SKIP;
                if (ewt->fifo_mode == BYPASS_MODE) {
                    ret = i2c_smbus_write_byte_data(ewt->client, REG_INT_CTRL, INT_CTRL_INT_ENABLE);
                }
                else {
                    ret = i2c_smbus_write_byte_data(ewt->client, REG_INT_CTRL, INT_CTRL_INT_ENABLE | INT_CTRL_INT_WTMON_EN);
                }
                if (ret < 0) {
                    dev_err(&ewt->client->dev, "REG_INT_CTRL write err(ret = %d)\n", ret);
                }
            }
#endif
        }
    }
    else {
        dev_err(&ewt->client->dev, "REG_SLEEP_CTRL write err(ret = %d)\n", ret);
    }

    return 0;
}

/* Probe function to register device */
static int EWTSA_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    struct ewtsa_t *ewtsa;

    if (client == NULL) {
        printk(KERN_INFO "EWTSA_probe:client null pointer\n");
        return -EIO;
    }

    ewtsa = kzalloc(sizeof(*ewtsa), GFP_KERNEL);
    if (ewtsa == NULL) {
        dev_err(&client->dev, "unable to allocate memory!\n");
        return -ENOMEM;
    }

    ewtsa->workqueue = create_workqueue("ewtsa_workqueue");
    if (ewtsa->workqueue == NULL) {
        dev_err(&client->dev, "Unable to create workqueue!\n");
        kfree(ewtsa);
        return -ENOMEM;
    }

    ewtsa->client = client;
    i2c_set_clientdata(client, ewtsa);

    ewtsa->sleep          = SLEEP_INIT_VAL;      /* sleep mode            */
    ewtsa->delay          = DELAY_INIT_VAL;      /* information interval  */
    ewtsa->range          = RANGE_INIT_VAL;      /* dynamic range         */
    ewtsa->dlpf           = DLPF_INIT_VAL;       /* dlpf                  */
    ewtsa->calib          = CALIB_FUNC_INIT_VAL; /* calibration function  */
    ewtsa->calib_boost_tm = CALIB_BTIM_INIT_VAL; /* boost time[ms]        */
    ewtsa->calib_fs       = CALIB_FS_INIT_VAL;   /* offset renewal rate   */
    ewtsa->calib_rst      = CALIB_RST_INIT_VAL;  /* calib restart flag    */
    ewtsa->fifo_mode      = FIFO_TYPE_INIT_VAL;  /* fifo type             */
    ewtsa->reset_param    = RST_REG_INIT_ALL;    /* register setting flag */
#if USE_GPIO_DRDY_PIN
    ewtsa->event          = EWTSA_INTERUPT_MODE; /* data detect method    */
#else
    ewtsa->event          = EWTSA_POLLING_MODE; /* data detect method    */
#endif
    ewtsa->calib_boost_fs = CALIB_BFS_INIT_VAL;  /* calibration boost fs  */
    ewtsa->int_skip       = EWSTA_INT_CLEAR;     /* first data flag       */
    ewtsa->calib_alway    = CALIB_CLK_INIT_VAL;  /* socclk flag           */
    ewtsa->hpf            = CALIB_HPF_INIT_VAL;  /* hpf                   */
    ewtsa->w1_thres_x     = THRES_CENTER_INIT_X; /* threshold center val  */
    ewtsa->w1_thres_y     = THRES_CENTER_INIT_Y; /* threshold center val  */
    ewtsa->w1_thres_z     = THRES_CENTER_INIT_Z; /* threshold center val  */
    ewtsa->w1_datamask    = W1_MASK_INIT_VAL;    /* window1 datamask set  */
    ewtsa->w1_thres_en    = W1_FUNC_INIT_VAL;    /* window1 function flag */
    ewtsa->w2_thres_en    = W2_FUNC_INIT_VAL;    /* window2 function flag */
    ewtsa->offset_lag     = CALIB_LAG_INIT_VAL;  /* offset renewal lag    */
    ewtsa->w1_thres_level = W1_LV_INIT_VAL;      /* window1 threshold lv  */
    ewtsa->fifo_data_flag = EWTSA_OFF;           /* last data flag        */
    ewtsa->fifo_num       = 0;                   /* fifo number           */
    memset(ewtsa->fifo_lastdata, 0x00, sizeof(ewtsa->fifo_lastdata));

    /*create device file in sysfs as user interface */
    ret = device_create_file(&client->dev, &ewtsa_sleep_attr);
    if (ret != 0) {
        dev_err(&client->dev, "create device(sleep_attr) file failed!\n");
        destroy_workqueue(ewtsa->workqueue);
        kfree(ewtsa);
        return ret;
    }
    ret = device_create_file(&client->dev, &ewtsa_delay_attr);
    if (ret != 0) {
        dev_err(&client->dev, "create device(delay_attr) file failed!\n");
        device_remove_file(&client->dev, &ewtsa_sleep_attr);
        destroy_workqueue(ewtsa->workqueue);
        kfree(ewtsa);
        return ret;
    }
    ret = device_create_file(&client->dev, &ewtsa_range_attr);
    if (ret != 0) {
        dev_err(&client->dev, "create device(range_attr) file failed!\n");
        device_remove_file(&client->dev, &ewtsa_delay_attr);
        device_remove_file(&client->dev, &ewtsa_sleep_attr);
        destroy_workqueue(ewtsa->workqueue);
        kfree(ewtsa);
        return ret;
    }
    ret = device_create_file(&client->dev, &ewtsa_calib_attr);
    if (ret != 0) {
        dev_err(&client->dev, "create device(calib_attr) file failed!\n");
        device_remove_file(&client->dev, &ewtsa_range_attr);
        device_remove_file(&client->dev, &ewtsa_delay_attr);
        device_remove_file(&client->dev, &ewtsa_sleep_attr);
        destroy_workqueue(ewtsa->workqueue);
        kfree(ewtsa);
        return ret;
    }
    ret = device_create_file(&client->dev, &ewtsa_config_attr);
    if (ret != 0) {
        dev_err(&client->dev, "create device(calib_attr) file failed!\n");
        device_remove_file(&client->dev, &ewtsa_calib_attr);
        device_remove_file(&client->dev, &ewtsa_range_attr);
        device_remove_file(&client->dev, &ewtsa_delay_attr);
        device_remove_file(&client->dev, &ewtsa_sleep_attr);
        destroy_workqueue(ewtsa->workqueue);
        kfree(ewtsa);
        return ret;
    }

    /*input poll device register */
    ewtsa->input_dev = input_allocate_device();
    if (ewtsa->input_dev == NULL) {
        dev_err(&client->dev, "alloc input device failed!\n");
        device_remove_file(&client->dev, &ewtsa_config_attr);
        device_remove_file(&client->dev, &ewtsa_calib_attr);
        device_remove_file(&client->dev, &ewtsa_range_attr);
        device_remove_file(&client->dev, &ewtsa_delay_attr);
        device_remove_file(&client->dev, &ewtsa_sleep_attr);
        destroy_workqueue(ewtsa->workqueue);
        kfree(ewtsa);
        return -ENOMEM;
    }
    ewtsa->input_dev->name = DEVICE_NAME;
    ewtsa->input_dev->id.bustype = BUS_I2C;
    ewtsa->input_dev->evbit[0] = BIT_MASK(EV_ABS);

    mutex_init(&ewtsa->mlock);
    spin_lock_init(&ewtsa->slock);

    input_set_abs_params(ewtsa->input_dev, ABS_X, -MAX_VALUE, MAX_VALUE, 0, 0);
    input_set_abs_params(ewtsa->input_dev, ABS_Y, -MAX_VALUE, MAX_VALUE, 0, 0);
    input_set_abs_params(ewtsa->input_dev, ABS_Z, -MAX_VALUE, MAX_VALUE, 0, 0);
    input_set_abs_params(ewtsa->input_dev, ABS_RX, -200, 200, 0, 0);
    input_set_abs_params(ewtsa->input_dev, ABS_RY, -200, 200, 0, 0);

    ret = input_register_device(ewtsa->input_dev);
    if (ret != 0) {
        dev_err(&client->dev, "register poll device failed!\n");
        input_free_device(ewtsa->input_dev);
        device_remove_file(&client->dev, &ewtsa_config_attr);
        device_remove_file(&client->dev, &ewtsa_calib_attr);
        device_remove_file(&client->dev, &ewtsa_range_attr);
        device_remove_file(&client->dev, &ewtsa_delay_attr);
        device_remove_file(&client->dev, &ewtsa_sleep_attr);
        destroy_workqueue(ewtsa->workqueue);
        kfree(ewtsa);
        return ret;
    }

    /* GPIO configuration */
    ret = i2c_smbus_write_byte_data(ewtsa->client, REG_SLEEP_CTRL, SLEEP_CTRL_ALLRSTSLEEP);
    if (ret < 0) {
        dev_err(&ewtsa->client->dev, "REG_SLEEP_CTRL write err(data = 0x00, ret = %d)\n", ret);
    }
#if USE_GPIO_WINDOW_PIN
    ret = gpio_request(WINDOW_PIN, "WINDOW");
    ret += gpio_direction_input(WINDOW_PIN);
#else
    ret = 0;
#endif
#if USE_GPIO_DIAG_PIN
    ret += gpio_request(DIAG_PIN, "DIAG");
    ret += gpio_direction_input(DIAG_PIN);
#endif
    if (ret != 0) {
        dev_err(&client->dev, "gpio_request failed!\n");
#if USE_GPIO_WINDOW_PIN
        gpio_free(WINDOW_PIN);
#endif
#if USE_GPIO_DIAG_PIN
        gpio_free(DIAG_PIN);
#endif
        input_unregister_device(ewtsa->input_dev);
        input_free_device(ewtsa->input_dev);
        device_remove_file(&client->dev, &ewtsa_config_attr);
        device_remove_file(&client->dev, &ewtsa_calib_attr);
        device_remove_file(&client->dev, &ewtsa_range_attr);
        device_remove_file(&client->dev, &ewtsa_delay_attr);
        device_remove_file(&client->dev, &ewtsa_sleep_attr);
        destroy_workqueue(ewtsa->workqueue);
        kfree(ewtsa);
        return ret;
    }

    INIT_DELAYED_WORK(&ewtsa->input_work, EWTSA_input_work_func);

    dev_info(&client->dev, "ewtsa device is probed successfully.\n");
    return 0;
}

/* Remove function to unregister the device */
static int EWTSA_remove(struct i2c_client *client)
{
    struct ewtsa_t *ewtsa;

    if (client == NULL) {
        printk(KERN_INFO "client pointer null!\n");
        return -EIO;
    }
    ewtsa = i2c_get_clientdata(client);

    i2c_smbus_write_byte_data(ewtsa->client, REG_SLEEP_CTRL, SLEEP_CTRL_ALLRSTSLEEP);
#if USE_GPIO_DRDY_PIN
    if (ewtsa->sleep == SLEEP_OFF) {
        free_irq(gpio_to_irq(DRDY_PIN), ewtsa);
        gpio_free(DRDY_PIN);
    }
#endif
#if USE_GPIO_WINDOW_PIN
    gpio_free(WINDOW_PIN);
#endif
#if USE_GPIO_DIAG_PIN
    gpio_free(DIAG_PIN);
#endif

    input_unregister_device(ewtsa->input_dev);
    input_free_device(ewtsa->input_dev);
    destroy_workqueue(ewtsa->workqueue);
    device_remove_file(&client->dev, &ewtsa_sleep_attr);
    device_remove_file(&client->dev, &ewtsa_delay_attr);
    device_remove_file(&client->dev, &ewtsa_range_attr);
    device_remove_file(&client->dev, &ewtsa_calib_attr);
    device_remove_file(&client->dev, &ewtsa_config_attr);
    kfree(ewtsa);
    dev_info(&client->dev, "ewtsa device is removed successfully.\n");
    return 0;
}


static int __init init_ewtsa(void)
{
     return i2c_add_driver(&i2c_ewtsa_driver);
}

static void __exit exit_ewtsa(void)
{
    i2c_del_driver(&i2c_ewtsa_driver);
}

module_init(init_ewtsa);
module_exit(exit_ewtsa);

MODULE_AUTHOR("Panasonic Electronic Devices");
MODULE_DESCRIPTION("Gyro sensor driver");
MODULE_LICENSE("GPL");
