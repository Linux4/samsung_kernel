#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include <linux/kthread.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/input.h>
#include <linux/signal.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/spi/spi.h>

#if defined(CONFIG_MTK_CLKMGR)
 /* mt_clkmgr.h will be removed after CCF porting is finished. */
#include <mach/mt_clkmgr.h>
#endif /* defined(CONFIG_MTK_CLKMGR) */

//#define PLATFORM_DEVICE
#define SPI_DEVICE

#ifdef SPI_DEVICE
//#define REE_SECSPI_SUPPORT
#define MASTER_CLK
#define DTS_PROBE
#endif

#ifdef REE_SECSPI_SUPPORT

#define TEE_VENDOR_BEANPOD
#define TEE_VENDOR_PINBO
#define TEE_VENDOR_TRUSTONIC
#define GET_ID_RETRY 5

#endif

#ifdef TEE_VENDOR_BEANPOD
#include "fp_func.h"
struct TEEC_UUID cdfinger_uuid = {0x7778c03f, 0xc30c, 0x4dd0,
                                {0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x0c}};
#endif

#ifdef TEE_VENDOR_PINBO
#include <tee_fp.h>
#endif

#ifdef REE_SECSPI_SUPPORT
/* follow soc spi master private data*/
#include <mtk_spi.h>
static struct mt_chip_conf chip_conf={
    .setuptime = 8,
    .holdtime = 8,
    .high_time = 13,
    .low_time = 13,
    .cs_idletime = 6,
    .cpol = 0,
    .cpha = 0,
    .rx_mlsb = 1,
    .tx_mlsb = 1,
    .tx_endian = 0,
    .rx_endian = 0,
    .com_mod = FIFO_TRANSFER,
    .pause = 1,
    .finish_intr = 1,
    .deassert = 0,
    .ulthigh = 0,
    .tckdly = 0,
};

#endif

static u8 cdfinger_debug = 0x07;
static int isInKeyMode = 0; // key mode
static int screen_status = 1; // screen on
static int sign_sync = 0; // for poll
typedef struct key_report{
    int key;
    int value;
}key_report_t;

#define CDFINGER_DBG(fmt, args...) \
    do{ \
        if(cdfinger_debug & 0x01) \
            printk( "[DBG][cdfinger]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
    }while(0)
#define CDFINGER_FUNCTION(fmt, args...) \
    do{ \
        if(cdfinger_debug & 0x02) \
            printk( "[DBG][cdfinger]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
    }while(0)
#define CDFINGER_REG(fmt, args...) \
    do{ \
        if(cdfinger_debug & 0x04) \
            printk( "[DBG][cdfinger]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
    }while(0)
#define CDFINGER_ERR(fmt, args...) \
    do{ \
        printk( "[DBG][cdfinger]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
    }while(0)

#define VERSION                         "cdfinger version 3.3"
#define DEVICE_NAME                     "fpsdev0"
#define SPI_DRV_NAME                    "cdfinger"

#define CDFINGER_IOCTL_MAGIC_NO          0xFB
#define CDFINGER_INIT                    _IOW(CDFINGER_IOCTL_MAGIC_NO, 0, uint8_t)
#define CDFINGER_GETIMAGE                _IOW(CDFINGER_IOCTL_MAGIC_NO, 1, uint8_t)
#define CDFINGER_INITERRUPT_MODE	     _IOW(CDFINGER_IOCTL_MAGIC_NO, 2, uint8_t)
#define CDFINGER_INITERRUPT_KEYMODE      _IOW(CDFINGER_IOCTL_MAGIC_NO, 3, uint8_t)
#define CDFINGER_INITERRUPT_FINGERUPMODE _IOW(CDFINGER_IOCTL_MAGIC_NO, 4, uint8_t)
#define CDFINGER_RELEASE_WAKELOCK        _IO(CDFINGER_IOCTL_MAGIC_NO, 5)
#define CDFINGER_CHECK_INTERRUPT         _IO(CDFINGER_IOCTL_MAGIC_NO, 6)
#define CDFINGER_SET_SPI_SPEED           _IOW(CDFINGER_IOCTL_MAGIC_NO, 7, uint8_t)
#define CDFINGER_REPORT_KEY_LEGACY       _IOW(CDFINGER_IOCTL_MAGIC_NO, 10, uint8_t)
#define CDFINGER_REPORT_KEY              _IOW(CDFINGER_IOCTL_MAGIC_NO, 19, key_report_t)
#define CDFINGER_POWERDOWN               _IO(CDFINGER_IOCTL_MAGIC_NO, 11)
#define CDFINGER_ENABLE_IRQ               _IO(CDFINGER_IOCTL_MAGIC_NO, 12)
#define CDFINGER_DISABLE_IRQ               _IO(CDFINGER_IOCTL_MAGIC_NO, 13)
#define CDFINGER_HW_RESET               _IOW(CDFINGER_IOCTL_MAGIC_NO, 14, uint8_t)
#define CDFINGER_GET_STATUS               _IO(CDFINGER_IOCTL_MAGIC_NO, 15)
#define CDFINGER_SPI_CLK               _IOW(CDFINGER_IOCTL_MAGIC_NO, 16, uint8_t)
#define CDFINGER_INIT_IRQ                   _IO(CDFINGER_IOCTL_MAGIC_NO, 21)
#define CDFINGER_POWER_ON                   _IO(CDFINGER_IOCTL_MAGIC_NO, 22)
#define CDFINGER_POWER_OFF                  _IO(CDFINGER_IOCTL_MAGIC_NO,24)
#define CDFINGER_RELEASE_DEVICE 		 _IO(CDFINGER_IOCTL_MAGIC_NO, 25)
#define CDFINGER_WAKE_LOCK	           _IOW(CDFINGER_IOCTL_MAGIC_NO,26,uint8_t)
#define CDFINGER_ENABLE_CLK				  _IOW(CDFINGER_IOCTL_MAGIC_NO, 30, uint8_t)
#define CDFINGER_POLL_TRIGGER			 _IO(CDFINGER_IOCTL_MAGIC_NO,31)
#define CDFINGER_NEW_KEYMODE		_IOW(CDFINGER_IOCTL_MAGIC_NO, 37, uint8_t)
#define KEY_INTERRUPT                   KEY_F11

/* HS03s code added for SR-AL5625-01-199 by wurui at 20210513 start */
extern int finger_sysfs;
/* HS03s code added for SR-AL5625-01-199 by wurui at 20210513 end */

enum work_mode {
    CDFINGER_MODE_NONE       = 1<<0,
    CDFINGER_INTERRUPT_MODE  = 1<<1,
    CDFINGER_KEY_MODE        = 1<<2,
    CDFINGER_FINGER_UP_MODE  = 1<<3,
    CDFINGER_READ_IMAGE_MODE = 1<<4,
    CDFINGER_MODE_MAX
};

static struct cdfinger_data {
#ifdef PLATFORM_DEVICE
    struct platform_device *pdev;
#endif
#ifdef SPI_DEVICE
    struct spi_device *spi;
#endif
    struct mutex buf_lock;
    unsigned int irq;
    int irq_enabled;

    u32 vdd_ldo_enable;
    u32 vio_ldo_enable;
    u32 config_spi_pin;

    struct pinctrl *fps_pinctrl;
    struct pinctrl_state *fps_reset_high;
    struct pinctrl_state *fps_reset_low;
    struct pinctrl_state *fps_power_on;
    struct pinctrl_state *fps_power_off;
    struct pinctrl_state *fps_vio_on;
    struct pinctrl_state *fps_vio_off;
    struct pinctrl_state *cdfinger_spi_miso;
    struct pinctrl_state *cdfinger_spi_mosi;
    struct pinctrl_state *cdfinger_spi_sck;
    struct pinctrl_state *cdfinger_spi_cs;
    struct pinctrl_state *cdfinger_irq;

    int thread_wakeup;
    int process_interrupt;
    enum work_mode device_mode;
    struct input_dev *cdfinger_inputdev;
#ifdef CONFIG_PM_WAKELOCKS
    struct wakeup_source cdfinger_lock;
#else
    struct wake_lock cdfinger_lock;
#endif
    struct task_struct *cdfinger_thread;
    struct fasync_struct *async_queue;
    uint8_t cdfinger_interrupt;
    struct notifier_block notifier;
}*g_cdfinger;

#ifdef CONFIG_PM_WAKELOCKS
static inline void wakeup_source_prepare(struct wakeup_source *ws, const char *name)
{
    if (ws) {
        memset(ws, 0, sizeof(*ws));
        ws->name = name;
    }
}

static inline void wakeup_source_drop(struct wakeup_source *ws)
{
    if (!ws)
        return;
    __pm_relax(ws);
}

static inline void wakeup_source_init(struct wakeup_source *ws,
                      const char *name)
{
    wakeup_source_prepare(ws, name);
    wakeup_source_add(ws);
}

static inline void wakeup_source_trash(struct wakeup_source *ws)
{
    wakeup_source_remove(ws);
    wakeup_source_drop(ws);
}
#endif

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DECLARE_WAIT_QUEUE_HEAD(cdfinger_waitqueue);

static void cdfinger_reset(int count);

// clk will follow platform... pls check this when you poarting
void mt_spi_enable_master_clk(struct spi_device *spi);
void mt_spi_disable_master_clk(struct spi_device *spi);
static long cdfinger_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static void enable_clk(struct cdfinger_data *cdfinger)
{
#if defined SPI_CLK
    #if (!defined(CONFIG_MT_SPI_FPGA_ENABLE))
    #if defined(CONFIG_MTK_CLKMGR)
            enable_clock(MT_CG_PERI_SPI0, "spi");
    #endif
    #endif
#endif

#if defined MASTER_CLK
    mt_spi_enable_master_clk(cdfinger->spi);
#endif
}

static void disable_clk(struct cdfinger_data *cdfinger)
{
#if defined SPI_CLK
    #if (!defined(CONFIG_MT_SPI_FPGA_ENABLE))
    #if defined(CONFIG_MTK_CLKMGR)
            disable_clock(MT_CG_PERI_SPI0, "spi");
    #endif
    #endif
#endif

#if defined MASTER_CLK
    mt_spi_disable_master_clk(cdfinger->spi);
#endif
}

#ifdef TEE_VENDOR_BEANPOD
static int spi_send_cmd(struct cdfinger_data *cdfinger,  u8 *tx, u8 *rx, u16 spilen)
{
    struct spi_message m;
    struct spi_transfer t = {
        .tx_buf = tx,
        .rx_buf = rx,
        .len = spilen,
        .bits_per_word = 8,
        .speed_hz = 4.8*1000000,
    };

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    return spi_sync(cdfinger->spi, &m);
}
#endif

#ifdef TEE_VENDOR_PINBO
static int spi_send_cmd(struct cdfinger_data *cdfinger,  u8 *tx, u8 *rx, u16 spilen)
{
    (void*)cdfinger;

    return tee_spi_transfer((void*)&chip_conf, sizeof(chip_conf), tx, rx, spilen);
}
#endif

#ifdef TEE_VENDOR_TRUSTONIC
static int spi_send_cmd(struct cdfinger_data *cdfinger,  u8 *tx, u8 *rx, u16 spilen)
{
    struct spi_message m;
    struct spi_transfer t = {
        .tx_buf = tx,
        .rx_buf = rx,
        .len = spilen,
        .bits_per_word = 8;
        .speed_hz = 4.8*1000000;
    };

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    return spi_sync(cdfinger->spi, &m);
}
#endif

#ifdef REE_SECSPI_SUPPORT
static int cdfinger_work_mode(struct cdfinger_data *cdfinger)
{
    int i = 0;
    unsigned char work[4] = {0x18, 0x66, 0x66, 0x66};
    unsigned char read[4] = {0x00};
    int ret = 0;

    for (; i < GET_ID_RETRY; i++)
    {
        ret = spi_send_cmd(cdfinger, work, read, 1);
        if (ret != 0)
        {
            CDFINGER_ERR("send work cmd faild, retry %d", i);
            cdfinger_reset(1);
            continue;
        }
        if (read[0] != 1) {
            CDFINGER_ERR("get work cmd respons faild %d, retry %d", read, i);
            cdfinger_reset(1);
            continue;
        }
        break;
    }

    if (i == GET_ID_RETRY)
        return -1;

    mdelay(1);

    return 0;
}

static int cdfinger_check_id(unsigned char id)
{
    switch ((int)id)
    {
    case 0x98:
    case 0x80:
    case 0x56:
    case 0x88:
    case 0x35:
    case 0x16:
    case 0x64:
    case 0x86:
        return 0;
    default:
        CDFINGER_ERR("check id failed 0x%x", id);
        break;
    }
    return -1;
}

static int cdfinger_spi_setup(struct cdfinger_data *cdfinger)
{
    cdfinger->spi->bits_per_word = 8;
    cdfinger->spi->mode = SPI_MODE_0;
    cdfinger->spi->max_speed_hz    = 8 * 1000 * 1000;

    cdfinger->spi->controller_data = (void *)&chip_conf;
    if(spi_setup(cdfinger->spi) != 0)
    {
        CDFINGER_ERR("%s: spi setup failed!\n", __func__);
        return -1;
    }
    return 0;
}

static int cdfinger_get_id(struct cdfinger_data *cdfinger)
{
    int i = 0;
    unsigned char id_cmd[4] = {0x74, 0x66, 0x66, 0x66};
    unsigned char id_ret[4] = {0x00, 0x00, 0x00, 0x00};
    int ret = 0;

    ret = cdfinger_spi_setup(cdfinger);
    if (ret != 0)
    {
        CDFINGER_ERR("cdfinger_spi_setup faild");
        return -1;
    }

    enable_clk(cdfinger);

    cdfinger_reset(1);

    for (; i < GET_ID_RETRY; i++)
    {
        ret = cdfinger_work_mode(cdfinger);
        if (ret != 0)
        {
            CDFINGER_ERR("work mode faild %d", ret);
            disable_clk(cdfinger);
            return -1;
        }

        ret = spi_send_cmd(cdfinger, id_cmd, id_ret, sizeof(id_cmd));
        if (ret != 0)
        {
            CDFINGER_ERR("send id cmd faild, retry %d", i);
            cdfinger_reset(1);
            continue;
        }

        ret = cdfinger_check_id(id_ret[3]);
        if (ret != 0)
        {
            CDFINGER_ERR("check id faild, retry %d", i);
            cdfinger_reset(1);
            continue;
        }
        break;
    }

    cdfinger_reset(1); //goto idle
    disable_clk(cdfinger);
    if (i == GET_ID_RETRY)
        return -1;

    return 0;
}
#endif

static void cdfinger_disable_irq(struct cdfinger_data *cdfinger)
{
    if(cdfinger->irq_enabled == 1)
    {
        disable_irq_nosync(cdfinger->irq);
        cdfinger->irq_enabled = 0;
        CDFINGER_DBG("irq disable\n");
    }
}

static void cdfinger_enable_irq(struct cdfinger_data *cdfinger)
{
    if(cdfinger->irq_enabled == 0)
    {
        enable_irq(cdfinger->irq);
        cdfinger->irq_enabled =1;
        CDFINGER_DBG("irq enable\n");
    }
}
static int cdfinger_getirq_from_platform(struct cdfinger_data *cdfinger)
{
#ifdef PLATFORM_DEVICE
    struct device *dev = &cdfinger->pdev->dev;
#endif
#ifdef SPI_DEVICE
    struct device *dev = &cdfinger->spi->dev;
#endif
    if(!(dev->of_node)){
        CDFINGER_ERR("of node not exist!\n");
        return -1;
    }

    cdfinger->irq = irq_of_parse_and_map(dev->of_node, 0);
    if(cdfinger->irq < 0)
    {
        CDFINGER_ERR("parse irq failed! irq[%d]\n",cdfinger->irq);
        return -1;
    }
    CDFINGER_DBG("get irq success! irq[%d]\n",cdfinger->irq);
    //pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_irq);
    return 0;
}

static int cdfinger_parse_dts(struct cdfinger_data *cdfinger)
{
    int ret = -1;
#ifdef PLATFORM_DEVICE
    struct device *dev = &cdfinger->pdev->dev;
#endif
#ifdef SPI_DEVICE
    struct device *dev = &cdfinger->spi->dev;
#endif
    dev->of_node = of_find_compatible_node(NULL,NULL,"mediatek,mtk_finger");

    of_property_read_u32(cdfinger->spi->dev.of_node,"vdd_ldo_enable",&cdfinger->vdd_ldo_enable);
    of_property_read_u32(cdfinger->spi->dev.of_node,"vio_ldo_enable",&cdfinger->vio_ldo_enable);
    of_property_read_u32(cdfinger->spi->dev.of_node,"config_spi_pin",&cdfinger->config_spi_pin);

    CDFINGER_DBG("vdd_ldo_enable[%d], vio_ldo_enable[%d], config_spi_pin[%d]\n",
        cdfinger->vdd_ldo_enable, cdfinger->vio_ldo_enable, cdfinger->config_spi_pin);

    cdfinger->fps_pinctrl = devm_pinctrl_get(dev);
    if (IS_ERR(cdfinger->fps_pinctrl)) {
        ret = PTR_ERR(cdfinger->fps_pinctrl);
        CDFINGER_ERR("Cannot find fingerprint cdfinger->fps_pinctrl! ret=%d\n", ret);
        goto parse_err;
    }

    cdfinger->cdfinger_irq = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_irq");
    if (IS_ERR(cdfinger->cdfinger_irq))
    {
        ret = PTR_ERR(cdfinger->cdfinger_irq);
        CDFINGER_ERR("cdfinger->cdfinger_irq ret = %d\n",ret);
        goto parse_err;
    }
    cdfinger->fps_reset_low = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_reset_low");
    if (IS_ERR(cdfinger->fps_reset_low))
    {
        ret = PTR_ERR(cdfinger->fps_reset_low);
        CDFINGER_ERR("cdfinger->fps_reset_low ret = %d\n",ret);
        goto parse_err;
    }
    cdfinger->fps_reset_high = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_reset_high");
    if (IS_ERR(cdfinger->fps_reset_high))
    {
        ret = PTR_ERR(cdfinger->fps_reset_high);
        CDFINGER_ERR("cdfinger->fps_reset_high ret = %d\n",ret);
        goto parse_err;
    }

    if(cdfinger->config_spi_pin == 1)
    {
        cdfinger->cdfinger_spi_miso = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_miso");
        if (IS_ERR(cdfinger->cdfinger_spi_miso))
        {
            ret = PTR_ERR(cdfinger->cdfinger_spi_miso);
            CDFINGER_ERR("cdfinger->cdfinger_spi_miso ret = %d\n",ret);
            goto parse_err;
        }
        cdfinger->cdfinger_spi_mosi = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_mosi");
        if (IS_ERR(cdfinger->cdfinger_spi_mosi))
        {
            ret = PTR_ERR(cdfinger->cdfinger_spi_mosi);
            CDFINGER_ERR("cdfinger->cdfinger_spi_mosi ret = %d\n",ret);
            goto parse_err;
        }
        cdfinger->cdfinger_spi_sck = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_sck");
        if (IS_ERR(cdfinger->cdfinger_spi_sck))
        {
            ret = PTR_ERR(cdfinger->cdfinger_spi_sck);
            CDFINGER_ERR("cdfinger->cdfinger_spi_sck ret = %d\n",ret);
            goto parse_err;
        }
        cdfinger->cdfinger_spi_cs = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_cs");
        if (IS_ERR(cdfinger->cdfinger_spi_cs))
        {
            ret = PTR_ERR(cdfinger->cdfinger_spi_cs);
            CDFINGER_ERR("cdfinger->cdfinger_spi_cs ret = %d\n",ret);
            goto parse_err;
        }
    }

    if(cdfinger->vdd_ldo_enable == 1)
    {
        cdfinger->fps_power_on = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_power_high");
        if (IS_ERR(cdfinger->fps_power_on))
        {
            ret = PTR_ERR(cdfinger->fps_power_on);
            CDFINGER_ERR("cdfinger->fps_power_on ret = %d\n",ret);
            goto parse_err;
        }

        cdfinger->fps_power_off = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_power_low");
        if (IS_ERR(cdfinger->fps_power_off))
        {
            ret = PTR_ERR(cdfinger->fps_power_off);
            CDFINGER_ERR("cdfinger->fps_power_off ret = %d\n",ret);
            goto parse_err;
        }
    }

    if(cdfinger->vio_ldo_enable == 1)
    {
        cdfinger->fps_vio_on = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_vio_high");
        if (IS_ERR(cdfinger->fps_vio_on))
        {
            ret = PTR_ERR(cdfinger->fps_vio_on);
            CDFINGER_ERR("cdfinger->fps_vio_on ret = %d\n",ret);
            goto parse_err;
        }

        cdfinger->fps_vio_off = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_vio_low");
        if (IS_ERR(cdfinger->fps_vio_off))
        {
            ret = PTR_ERR(cdfinger->fps_vio_off);
            CDFINGER_ERR("cdfinger->fps_vio_off ret = %d\n",ret);
            goto parse_err;
        }
    }

    return 0;
parse_err:
    CDFINGER_ERR("parse dts failed!\n");

    return ret;
}

static void cdfinger_power_on(struct cdfinger_data *cdfinger)
{
    if(cdfinger->config_spi_pin == 1)
    {
        pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_miso);
        pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_mosi);
        pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_sck);
        pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_cs);
    }

    if(cdfinger->vdd_ldo_enable == 1)
    {
        pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_power_on);
        CDFINGER_ERR("cdfinger pinctrl power_on\n");
    }
}

static void cdfinger_power_off(struct cdfinger_data *cdfinger)
{
    if(cdfinger->config_spi_pin == 1)
    {
        pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_miso);
        pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_mosi);
        pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_sck);
        pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_cs);
    }

    if(cdfinger->vdd_ldo_enable == 1)
    {
        pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_power_off);
        CDFINGER_ERR("cdfinger pinctrl power_off\n");
    }
}

static void cdfinger_reset(int count)
{
    struct cdfinger_data *cdfinger = g_cdfinger;
    pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_reset_low);
    mdelay(count);
    pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_reset_high);
    mdelay(count);
}

static void cdfinger_ws_timeout(struct cdfinger_data *cdfinger)
{
#ifdef CONFIG_PM_WAKELOCKS
    __pm_wakeup_event(&cdfinger->cdfinger_lock, jiffies_to_msecs(1*HZ));
#else
    wake_lock(&cdfinger->cdfinger_lock,1*HZ);
#endif
}

static void cdfinger_wake_lock(struct cdfinger_data *cdfinger, int arg)
{
    static int onoff = 0;

    CDFINGER_FUNCTION("enter\n");
    if(arg == 1 && onoff == 0)
    {
#ifdef CONFIG_PM_WAKELOCKS
        __pm_stay_awake(&cdfinger->cdfinger_lock);
#else
        wake_lock(&cdfinger->cdfinger_lock);
#endif
        onoff = 1;
    }
    if (arg == 0 && onoff == 1)
    {
#ifdef CONFIG_PM_WAKELOCKS
        __pm_relax(&cdfinger->cdfinger_lock);
#else
        wake_unlock(&cdfinger->cdfinger_lock);
#endif
        onoff = 0;
    }
    CDFINGER_FUNCTION("exit\n");
}

static int cdfinger_mode_init(struct cdfinger_data *cdfinger, uint8_t arg, enum work_mode mode)
{
    CDFINGER_DBG("mode=0x%x\n", mode);
    cdfinger->process_interrupt = 1;
    cdfinger->device_mode = mode;

    return 0;
}

/* HS03S code added for DEVAL5625-51 by shibinbin at 20210510 start */
static int cdfinger_free_gpio(struct cdfinger_data *cdfinger)
{
    int ret = 0;

    CDFINGER_FUNCTION("enter\n");
    if(cdfinger->fps_pinctrl)
    {
        devm_pinctrl_put(cdfinger->fps_pinctrl);
        cdfinger->fps_pinctrl = NULL;
    }
    /* HS03s code added for SR-AL5625-01-199 by wurui at 20210513 start */
    finger_sysfs = 0;
    /* HS03s code added for SR-AL5625-01-199 by wurui at 20210513 end */
    CDFINGER_FUNCTION("exit\n");

    return ret;
}
/* HS03S code added for DEVAL5625-51 by shibinbin at 20210510 end */

static unsigned int cdfinger_poll(struct file *filp, struct poll_table_struct *wait)
{
    int mask = 0;
    poll_wait(filp, &cdfinger_waitqueue, wait);
    if (sign_sync == 1)
    {
        mask |= POLLIN|POLLPRI;
    } else if (sign_sync == 2)
    {
        mask |= POLLOUT;
    }
    sign_sync = 0;
    CDFINGER_DBG("mask %u\n",mask);
    return mask;
}

static int cdfinger_open(struct inode *inode, struct file *file)
{
    CDFINGER_FUNCTION("enter\n");
    file->private_data = g_cdfinger;
    CDFINGER_FUNCTION("exit\n");

    return 0;
}

static ssize_t cdfinger_write(struct file *file, const char *buff, size_t count, loff_t * ppos)
{
    return 0;
}

static int cdfinger_async_fasync(int fd, struct file *filp, int mode)
{
    struct cdfinger_data *cdfinger = g_cdfinger;

    CDFINGER_FUNCTION("enter\n");
    return fasync_helper(fd, filp, mode, &cdfinger->async_queue);
}

static ssize_t cdfinger_read(struct file *file, char *buff, size_t count, loff_t * ppos)
{
    return 0;
}

static int cdfinger_release(struct inode *inode, struct file *file)
{
    file->private_data = NULL;

    return 0;
}

static const struct file_operations cdfinger_fops = {
    .owner = THIS_MODULE,
    .open = cdfinger_open,
    .write = cdfinger_write,
    .read = cdfinger_read,
    .release = cdfinger_release,
    .fasync = cdfinger_async_fasync,
    .unlocked_ioctl = cdfinger_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = cdfinger_ioctl,
#endif
    .poll = cdfinger_poll,
};

static struct miscdevice cdfinger_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &cdfinger_fops,
};

static irqreturn_t cdfinger_interrupt_handler(unsigned irq, void *arg)
{
    struct cdfinger_data *cdfinger = (struct cdfinger_data *)arg;

    cdfinger->cdfinger_interrupt = 1;
    if (cdfinger->process_interrupt == 1)
    {
        cdfinger->thread_wakeup = 1;
        wake_up_interruptible(&waiter);
    }

    return IRQ_HANDLED;
}

static void cdfinger_async_Report(void)
{
    struct cdfinger_data *cdfinger = g_cdfinger;

    CDFINGER_FUNCTION("enter\n");
    kill_fasync(&cdfinger->async_queue, SIGIO, POLL_IN);
    CDFINGER_FUNCTION("exit\n");
}

static int cdfinger_thread_func(void *arg)
{
    struct cdfinger_data *cdfinger = (struct cdfinger_data *)arg;

    do {
        wait_event_interruptible(waiter, cdfinger->thread_wakeup != 0);
        CDFINGER_DBG("cdfinger:%s,thread wakeup\n",__func__);
        cdfinger->thread_wakeup = 0;

        if (cdfinger->device_mode == CDFINGER_INTERRUPT_MODE) {
            cdfinger_ws_timeout(cdfinger);
            cdfinger->process_interrupt = 0;
            sign_sync = 1;
            wake_up_interruptible(&cdfinger_waitqueue);
            cdfinger_async_Report();
            continue;
        }

    }while(!kthread_should_stop());

    CDFINGER_ERR("thread exit\n");
    return -1;
}

static int init_irq(){
    struct cdfinger_data *cdfinger = g_cdfinger;
    int ret = 0;
    ret =request_threaded_irq (cdfinger->irq, (irq_handler_t)cdfinger_interrupt_handler, NULL,
            IRQF_TRIGGER_RISING | IRQF_ONESHOT, "cdfinger-irq", cdfinger);
    if(ret){
        CDFINGER_ERR("request_irq error\n");
        return ret;
    }
    enable_irq_wake(cdfinger->irq);
    cdfinger->irq_enabled = 1;

    cdfinger->cdfinger_thread = kthread_run(cdfinger_thread_func, cdfinger, "cdfinger_thread");
    ret = IS_ERR(cdfinger->cdfinger_thread);
    if (ret) {
        CDFINGER_ERR("kthread_run is failed\n");
    }
        return ret;
}

static long cdfinger_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct cdfinger_data *cdfinger = filp->private_data;
    int ret = 0;

    CDFINGER_FUNCTION("enter\n");
    if(cdfinger == NULL)
    {
        CDFINGER_ERR("%s: fingerprint please open device first!\n", __func__);
        return -EIO;
    }

    mutex_lock(&cdfinger->buf_lock);
	/* HS03s code added for SR-AL5625-01-199 by wurui at 20210513 start */
    finger_sysfs = 0x03;
    /* HS03s code added for SR-AL5625-01-199 by wurui at 20210513 end */
    switch (cmd) {
        case CDFINGER_INIT:
            ret = cdfinger_parse_dts(cdfinger);
            if(ret)
            {
                CDFINGER_ERR("%s: parse dts failed!\n", __func__);
                kfree(cdfinger);
                cdfinger = NULL;
            } else {
                cdfinger_power_on(cdfinger);
            }
            break;
        case CDFINGER_POWER_ON:
            cdfinger_power_on(cdfinger);
            break;
        case CDFINGER_INIT_IRQ:
            ret = cdfinger_getirq_from_platform(cdfinger);
            if(ret == 0)
                ret = init_irq();
            break;
        case CDFINGER_INITERRUPT_MODE:
            sign_sync = 0;
            isInKeyMode = 1;  // not key mode
            cdfinger_reset(2);
            ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_INTERRUPT_MODE);
            break;
        case CDFINGER_NEW_KEYMODE:
            isInKeyMode = 0;
            ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_INTERRUPT_MODE);
            break;
        case CDFINGER_INITERRUPT_FINGERUPMODE:
            ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_FINGER_UP_MODE);
            break;
        case CDFINGER_INITERRUPT_KEYMODE:
            ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_KEY_MODE);
            break;
        case CDFINGER_WAKE_LOCK:
            cdfinger_wake_lock(cdfinger,arg);
            break;
        case CDFINGER_REPORT_KEY:
            break;
        case CDFINGER_REPORT_KEY_LEGACY:
            break;
        case CDFINGER_POWERDOWN:
            break;
        case CDFINGER_POWER_OFF:
            cdfinger_power_off(cdfinger);
            break;
        case CDFINGER_ENABLE_IRQ:
            cdfinger_enable_irq(cdfinger);
            break;
        case CDFINGER_DISABLE_IRQ:
            cdfinger_disable_irq(cdfinger);
            break;
        case CDFINGER_SPI_CLK:
        case CDFINGER_ENABLE_CLK:
            if (arg == 1)
                enable_clk(cdfinger);
            else if (arg == 0)
                disable_clk(cdfinger);
            break;
        case CDFINGER_HW_RESET:
            cdfinger_reset(arg);
            break;
        case CDFINGER_RELEASE_DEVICE:
            cdfinger_free_gpio(cdfinger);
            break;
        case CDFINGER_GET_STATUS:
            ret = screen_status;
            break;
        case CDFINGER_POLL_TRIGGER:
            sign_sync = 2;
            wake_up_interruptible(&cdfinger_waitqueue);
            ret = 0;
            break;
        default:
            ret = -ENOTTY;
            break;
    }
    mutex_unlock(&cdfinger->buf_lock);
    CDFINGER_FUNCTION("exit\n");

    return ret;
}

static int cdfinger_fb_notifier_callback(struct notifier_block* self,
                                        unsigned long event, void* data)
{
    struct fb_event* evdata = data;
    unsigned int blank;
    int retval = 0;

    if (event != FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
        return 0;
    }
    blank = *(int*)evdata->data;
    switch (blank) {
        case FB_BLANK_UNBLANK:
            mutex_lock(&g_cdfinger->buf_lock);
            screen_status = 1;
            if (isInKeyMode == 0) {
                sign_sync = 1;
                wake_up_interruptible(&cdfinger_waitqueue);
                cdfinger_async_Report();
            }
            mutex_unlock(&g_cdfinger->buf_lock);
            break;

        case FB_BLANK_POWERDOWN:
            mutex_lock(&g_cdfinger->buf_lock);
            screen_status = 0;
            if (isInKeyMode == 0) {
                sign_sync = 1;
                wake_up_interruptible(&cdfinger_waitqueue);
                cdfinger_async_Report();
            }
            mutex_unlock(&g_cdfinger->buf_lock);
            break;
        default:
            break;
    }

    return retval;
}

static void cdfinger_wakesource_init(struct cdfinger_data *cdfinger)
{
#ifdef CONFIG_PM_WAKELOCKS
    wakeup_source_init(&cdfinger->cdfinger_lock, "cdfinger wakelock");
#else
    wake_lock_init(&cdfinger->cdfinger_lock, WAKE_LOCK_SUSPEND, "cdfinger wakelock");
#endif
}

static void cdfinger_wakesource_destory(struct cdfinger_data *cdfinger)
{
#ifdef CONFIG_PM_WAKELOCKS
    wakeup_source_trash(&cdfinger->cdfinger_lock);
#else
    wake_lock_destroy(&cdfinger->cdfinger_lock);
#endif
}

#ifdef PLATFORM_DEVICE
static int cdfinger_probe(struct platform_device *pdev)
#endif
#ifdef SPI_DEVICE
static int cdfinger_probe(struct spi_device *spi)
#endif
{
    struct cdfinger_data *cdfinger = NULL;
    int status = -ENODEV;
    CDFINGER_DBG("enter\n");

    cdfinger = kzalloc(sizeof(struct cdfinger_data), GFP_KERNEL);
    if (!cdfinger) {
        CDFINGER_ERR("alloc cdfinger failed!\n");
        return -ENOMEM;;
    }

    g_cdfinger = cdfinger;
#ifdef PLATFORM_DEVICE
    cdfinger->pdev = pdev;
#endif
#ifdef SPI_DEVICE
    cdfinger->spi = spi;
#endif

#ifdef REE_SECSPI_SUPPORT
    status = cdfinger_get_id(cdfinger);
    if (status == 0) {
        CDFINGER_DBG("get id success\n");
#ifdef TEE_VENDOR_BEANPOD
        memcpy(&uuid_fp, &cdfinger_uuid, sizeof(struct TEEC_UUID));
#endif
    } else {
        CDFINGER_ERR("get id success err\n");
        status = -1;
        goto free_cdfinger;
    }
#endif
    mutex_init(&cdfinger->buf_lock);
    cdfinger_wakesource_init(cdfinger);

    status = misc_register(&cdfinger_dev);
    if (status < 0) {
        CDFINGER_ERR("%s: cdev register failed!\n", __func__);
        goto free_lock;
    }

    cdfinger->notifier.notifier_call = cdfinger_fb_notifier_callback;
    fb_register_client(&cdfinger->notifier);

    CDFINGER_DBG("exit\n");

    return 0;

free_lock:
    cdfinger_wakesource_destory(cdfinger);
    mutex_destroy(&cdfinger->buf_lock);
    kfree(cdfinger);
    cdfinger = NULL;

    return -1;
}

#ifdef PLATFORM_DEVICE
static const struct of_device_id cdfinger_of_match[] = {
    { .compatible = "cdfinger,fingerprint_tee", },
    {},
};

static const struct platform_device_id cdfinger_id[] = {
    {"cdfinger_fp", 0},
    {}
};

struct platform_device cdfinger_tee_device = {
    .name = "cdfinger_fp",
};


static struct platform_driver cdfinger_driver = {
    .driver = {
        .name = "cdfinger_fp",
        .of_match_table = cdfinger_of_match,
    },
    .id_table	= cdfinger_id,
    .probe = cdfinger_probe,
};
#endif

#ifdef SPI_DEVICE
static int cdfinger_suspend (struct device *dev)
{
    return 0;
}

static int cdfinger_resume (struct device *dev)
{
    return 0;
}
static const struct dev_pm_ops cdfinger_pm = {
    .suspend = cdfinger_suspend,
    .resume = cdfinger_resume
};
struct of_device_id cdfinger_of_match[] = {
    { .compatible = "cdfinger,fingerprint_tee", },
    {},
};
MODULE_DEVICE_TABLE(of, cdfinger_of_match);

static const struct spi_device_id cdfinger_id[] = {
    {SPI_DRV_NAME, 0},
    {}
};
MODULE_DEVICE_TABLE(spi, cdfinger_id);

static struct spi_driver cdfinger_driver = {
    .driver = {
        .name = SPI_DRV_NAME,
        .bus = &spi_bus_type,
        .owner = THIS_MODULE,
        .pm = &cdfinger_pm,
        .of_match_table = of_match_ptr(cdfinger_of_match),
    },
    .id_table = cdfinger_id,
    .probe = cdfinger_probe,
    .remove = NULL,
};

#ifndef DTS_PROBE
static struct spi_board_info spi_board_cdfinger[] __initdata = {
    [0] = {
        .modalias = "cdfinger",
        .bus_num = 0,
        .chip_select = 0,
        .mode = SPI_MODE_0,
        .max_speed_hz = 6000000,
        .controller_data = &chip_conf,
    },
};
#endif
#endif

static int __init cdfinger_fp_init(void)
{
#ifdef PLATFORM_DEVICE
    int ret;
#endif
    CDFINGER_ERR("cdfinger_fp_init\n");
#ifdef PLATFORM_DEVICE
    ret = platform_device_register(&cdfinger_tee_device);
    if(ret){
        CDFINGER_ERR("device register failed!\n");
    }
    platform_driver_register(&cdfinger_driver);
#endif

#ifdef SPI_DEVICE
#ifndef DTS_PROBE
    spi_register_board_info(spi_board_cdfinger, ARRAY_SIZE(spi_board_cdfinger));
#endif
    return spi_register_driver(&cdfinger_driver);
#endif
    return 0;
}

static void __exit cdfinger_fp_exit(void)
{
#ifdef PLATFORM_DEVICE
    platform_driver_unregister(&cdfinger_driver);
#endif
#ifdef SPI_DEVICE
    spi_unregister_driver(&cdfinger_driver);
#endif
}

module_init(cdfinger_fp_init);
module_exit(cdfinger_fp_exit);

MODULE_DESCRIPTION("cdfinger tee Driver");
MODULE_AUTHOR("shuaitao@cdfinger.com");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cdfinger");
