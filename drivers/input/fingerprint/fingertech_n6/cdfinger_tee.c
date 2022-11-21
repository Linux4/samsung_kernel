#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/spi/spi.h>
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
#include <linux/spi/spidev.h>
#include <linux/semaphore.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/input.h>
#include <linux/signal.h>
#include <linux/spi/spi.h>
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
#include <linux/cdev.h>

#ifdef CONFIG_MTK_CLKMGR
#include "mach/mt_clkmgr.h"
#endif

#include <linux/platform_data/spi-mt65xx.h>
struct spi_device *global_spidev =NULL; // add for compatible
struct mtk_spi {
	void __iomem *base;
	void __iomem *peri_regs;
	u32 state;
	int pad_num;
	u32 *pad_sel;
	struct clk *parent_clk, *sel_clk, *spi_clk;
	struct spi_transfer *cur_transfer;
	u32 xfer_len;
	struct scatterlist *tx_sgl, *rx_sgl;
	u32 tx_sgl_len, rx_sgl_len;
	const struct mtk_spi_compatible *dev_comp;
	u32 dram_8gb_offset;
};

void mt_spi_enable_master_clk(struct spi_device *spi);
void mt_spi_disable_master_clk(struct spi_device *spi);
static void cdfinger_async_Report(void);
static irqreturn_t cdfinger_interrupt_handler(unsigned irq, void *arg);
//#ifdef CONFIG_SPI_MT65XX
//extern void mt_spi_enable_master_clk(struct spi_device *spidev);
//extern void mt_spi_disable_master_clk(struct spi_device *spidev);
//#endif

//#define COMPAT_VENDOR

//#include "mt_spi_hal.h"
//#include <mt_spi.h>

#ifdef COMPAT_VENDOR
//#include "tee_client_api.h"
//#include <../../../misc/mediatek/tkcore/include/linux/tee_fp.h>
#include "fp_func.h"
struct TEEC_UUID cdfinger_uuid = {0x7778c03f, 0xc30c, 0x4dd0,
								{0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x0c}};
#endif

#ifdef PINBO_TEE_COMPATIBLE
#include <tee_fp.h>
#endif

#define CDFINGER_COMPIT_MORE_FINGER
#ifndef TBASE_TEE_COMPATIBLE
#define  TBASE_TEE_COMPATIBLE
#endif
/*
static struct mtk_chip_config spi_conf={
	.cs_pol = 0,
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.sample_sel = 0,
};
*/
extern bool sunwave_fp_exist ;
extern struct spi_device *spi_fingerprint;
extern int fpc_or_chipone_exist;
extern bool egis_fp_exist;
extern bool fingtech_fp_exist;

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
#define CDFINGER_PROBE(fmt, args...) \
    do{ \
		printk( "[PROBE][cdfinger]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
    }while(0)


    
#define DTS_PROBE    
#define HAS_RESET_PIN

#define VERSION                         "cdfinger version 3.2"
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
#define	CDFINGER_GETID			 		 _IO(CDFINGER_IOCTL_MAGIC_NO,9)
#define	CDFINGER_SETID						_IOW(CDFINGER_IOCTL_MAGIC_NO,8,uint8_t)
//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#ifdef USE_INPUT_DEVICE
#define CDFINGER_REPORT_KEY_LEGACY              _IOW(CDFINGER_IOCTL_MAGIC_NO, 10, uint8_t)
#define CDFINGER_REPORT_KEY              _IOW(CDFINGER_IOCTL_MAGIC_NO, 19, key_report_t)
#endif
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#define CDFINGER_POWERDOWN               _IO(CDFINGER_IOCTL_MAGIC_NO, 11)
#define CDFINGER_ENABLE_IRQ               _IO(CDFINGER_IOCTL_MAGIC_NO, 12)
#define CDFINGER_DISABLE_IRQ               _IO(CDFINGER_IOCTL_MAGIC_NO, 13)
#define CDFINGER_HW_RESET               _IOW(CDFINGER_IOCTL_MAGIC_NO, 14, uint8_t)
#define CDFINGER_GET_STATUS               _IO(CDFINGER_IOCTL_MAGIC_NO, 15)
#define CDFINGER_SPI_CLK               _IOW(CDFINGER_IOCTL_MAGIC_NO, 16, uint8_t)
#define CDFINGER_POWER_ON 				_IO(CDFINGER_IOCTL_MAGIC_NO, 22)
#define CDFINGER_RESET 				_IO(CDFINGER_IOCTL_MAGIC_NO, 23)
#define CDFINGER_RELEASE_DEVICE				_IO(CDFINGER_IOCTL_MAGIC_NO,25)
#define CDFINGER_WAKE_LOCK	           _IOW(CDFINGER_IOCTL_MAGIC_NO,26,uint8_t)
#define CDFINGER_SENSOR_TEST             _IO(CDFINGER_IOCTL_MAGIC_NO,27)
#define CDFINGER_ENABLE_CLK				  _IOW(CDFINGER_IOCTL_MAGIC_NO, 30, uint8_t)
#define CDFINGER_POLL_TRIGGER			 _IO(CDFINGER_IOCTL_MAGIC_NO,31)
#define CDFINGER_NEW_KEYMODE		_IOW(CDFINGER_IOCTL_MAGIC_NO, 37, uint8_t)
#define KEY_INTERRUPT                   KEY_F11

enum work_mode {
	CDFINGER_MODE_NONE       = 1<<0,
	CDFINGER_INTERRUPT_MODE  = 1<<1,
	CDFINGER_KEY_MODE        = 1<<2,
	CDFINGER_FINGER_UP_MODE  = 1<<3,
	CDFINGER_READ_IMAGE_MODE = 1<<4,
	CDFINGER_MODE_MAX
};

static struct cdfinger_data {
	struct spi_device *spi;
	struct miscdevice *miscdev;
	struct mutex buf_lock;
	unsigned int irq;
    unsigned int irq_gpio;
	int irq_enabled;
	u32 chip_id;

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
	int key_report;
	enum work_mode device_mode;
//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#ifdef USE_INPUT_DEVICE
	struct input_dev *cdfinger_inputdev;
#endif
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#ifdef CONFIG_PM_WAKELOCKS
	struct wakeup_source *cdfinger_lock;
#else
	struct wake_lock cdfinger_lock;
#endif
	struct task_struct *cdfinger_thread;
	struct fasync_struct *async_queue;
	uint8_t cdfinger_interrupt;
	struct notifier_block notifier;
	struct mt_spi_t * cdfinger_ms;
	struct mtk_chip_config mtk_spi_config;
	int res_init;
}*g_cdfinger;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DECLARE_WAIT_QUEUE_HEAD(cdfinger_waitqueue);
//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
static int fingerprint_adm = -1;
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
// clk will follow platform... pls check this when you poarting
//static void cdfinger_enable_clk(void)
/*static void cdfinger_enable_clk(struct cdfinger_data *cdfinger,u8 bonoff)

{

	struct mtk_spi *mdata = NULL;
	int ret = -1;
	static int count = 0;
	mdata = spi_master_get_devdata(cdfinger->spi->master);

	if (bonoff && (count == 0)) {
		ret = clk_prepare_enable(mdata->spi_clk);
		if (ret < 0) {
			CDFINGER_DBG("failed to enable spi_clk (%d)\n", ret);
		}else {
			count = 1;
			CDFINGER_DBG("sucess to enable spi_clk-- (%d)\n", ret);
		}
	} else if ((count > 0) && (bonoff == 0)) {
		clk_disable_unprepare(mdata->spi_clk);
		CDFINGER_DBG("disable spi_clk\n");
		count = 0;
	}
	
}
*/
//static void cdfinger_disable_clk(struct cdfinger_data *cdfinger)
//{
	/*
	#ifdef CDFINGER_EN_SPI_CLK
	struct mtk_spi *mdata = NULL;	
	mdata = spi_master_get_devdata(g_cdfinger->spi->master);
	clk_disable_unprepare(mdata->spi_clk);
	 //mtk_spi_clk_ctl(g_cdfinger->spi, 0);
	 #else
	 mt_spi_disable_clk(g_cdfinger->cdfinger_ms);
	 #endif
	 */
	 	//struct mtk_spi *mdata = NULL;	
	 //	mdata = spi_master_get_devdata(cdfinger->spi->master);
	 	//clk_disable_unprepare(mdata->spi_clk);
		//CDFINGER_DBG("--disable spi clk\n");



//}


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
    int err;
	struct device_node *node_eint = NULL;

    node_eint = of_find_compatible_node(NULL, NULL, "mediatek,fpsensor_fp_eint_n6");
    if (node_eint == NULL) {
        err = -EINVAL;
        printk("fps980 mediatek,fpsensor_fp_eint cannot find node_eint rc = %d.\n",err);
        return 0;
    }

    cdfinger->irq_gpio = of_get_named_gpio(node_eint, "int-gpios", 0);
    printk(" fps980 irq_gpio=%d",cdfinger->irq_gpio);   

    err = devm_gpio_request(&cdfinger->spi->dev, cdfinger->irq_gpio, "fps980,gpio_irq");
    if (err) {
        printk( KERN_ERR " fps980 failed to request gpio %d\n",  cdfinger->irq_gpio);
        return 0;
    }
			  
    cdfinger->irq = gpio_to_irq(cdfinger->irq_gpio);

	return 0;
}

/*static int cdfinger_get_node_cfg(struct cdfinger_data *cdfinger)
{

   	int ret = -1;	
#ifdef CDFINGER_COMPIT_MORE_FINGER
	if(cdfinger->spi == NULL)
	{
		CDFINGER_ERR("spi is NULL !\n");
		goto node_err;
	}
#ifdef DTS_PROBE
	cdfinger->spi->dev.of_node = of_find_compatible_node(NULL,NULL,"ontim,fingerprint");
	printk("cdfinger DTS_PROBE open!\n");
//	CDFINGER_DBG("get irq success! irq[%d]\n",cdfinger->irq);
	//cdfinger->spi->dev.of_node = of_find_compatible_node(NULL,NULL,"mediatek,mtk_finger");
#endif

	cdfinger->vdd_ldo_enable = 1;
	cdfinger->vio_ldo_enable = 0;
	cdfinger->config_spi_pin = 1;
	
	of_property_read_u32(cdfinger->spi->dev.of_node,"vdd_ldo_enable",&cdfinger->vdd_ldo_enable);
	of_property_read_u32(cdfinger->spi->dev.of_node,"vio_ldo_enable",&cdfinger->vio_ldo_enable);
	of_property_read_u32(cdfinger->spi->dev.of_node,"config_spi_pin",&cdfinger->config_spi_pin);


	CDFINGER_PROBE("vdd_ldo_enable[%d], vio_ldo_enable[%d], config_spi_pin[%d]\n",
		cdfinger->vdd_ldo_enable, cdfinger->vio_ldo_enable, cdfinger->config_spi_pin);

	cdfinger->fps_pinctrl = devm_pinctrl_get(&cdfinger->spi->dev);
	if (IS_ERR(cdfinger->fps_pinctrl)) {
		ret = PTR_ERR(cdfinger->fps_pinctrl);
		CDFINGER_ERR("Cannot find fingerprint cdfinger->fps_pinctrl! ret=%d\n", ret);
		goto node_err;
	}
#endif 

    return 0;

node_err:
	CDFINGER_ERR("node cfg failed!\n");

   return ret;
}*/


static int cdfinger_parse_dts(struct cdfinger_data *cdfinger)
{
	int ret;
	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;

	printk(KERN_ERR "fps980 %s, from dts pinctrl\n", __func__);

	node = of_find_compatible_node(NULL, NULL, "mediatek,finger-fp");
	if (node) {
		pdev = of_find_device_by_node(node);
		if (pdev) {
			cdfinger->fps_pinctrl = devm_pinctrl_get(&pdev->dev);
			if (IS_ERR(cdfinger->fps_pinctrl)) {
				ret = PTR_ERR(cdfinger->fps_pinctrl);
				printk(KERN_ERR "fps980 %s can't find fingerprint pinctrl\n", __func__);
				return ret;
			}

			cdfinger->fps_reset_high = pinctrl_lookup_state(cdfinger->fps_pinctrl, "rst-high");
			if (IS_ERR(cdfinger->fps_reset_high)) {
				ret = PTR_ERR(cdfinger->fps_reset_high);
				printk(KERN_ERR "fps980 %s can't find fingerprint pinctrl reset_high\n", __func__);
				return ret;
			}
			cdfinger->fps_reset_low = pinctrl_lookup_state(cdfinger->fps_pinctrl, "rst-low");
			if (IS_ERR(cdfinger->fps_reset_low)) {
				ret = PTR_ERR(cdfinger->fps_reset_low);
				printk(KERN_ERR "fps980 %s can't find fingerprint pinctrl reset_low\n", __func__);
				return ret;
			}
			printk(KERN_ERR "fps980 %s, get pinctrl success!\n", __func__);
		} else {
			printk(KERN_ERR "fps980 %s platform device is null\n", __func__);
		}
	} else {
		printk(KERN_ERR "fps980 %s device node is null\n", __func__);
	}

	return 0;	
}

#ifdef COMPAT_VENDOR //0
static int spi_send_cmd(struct cdfinger_data *cdfinger,  u8 *tx, u8 *rx, u16 spilen)
{
	struct spi_message m;
	struct spi_transfer t = {
		.tx_buf = tx,
		.rx_buf = rx,
		.len = spilen,
	};

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return spi_sync(cdfinger->spi, &m);
}
#endif
#if 0
static void cdfinger_power_off(struct cdfinger_data *cdfinger)
{

	if(cdfinger->vdd_ldo_enable == 1)
	{
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_power_off);
	}

	if(cdfinger->vio_ldo_enable == 1)
	{
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_vio_off);
	}
}

#endif

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
	}

	if(cdfinger->vio_ldo_enable == 1)
	{
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_vio_on);
	}
}


static void cdfinger_reset(int count)
{
#ifdef HAS_RESET_PIN
	struct cdfinger_data *cdfinger = g_cdfinger;
	pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_reset_low);
	mdelay(count);
	pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_reset_high);
	mdelay(count);
#endif
}


static void cdfinger_release_wakelock(struct cdfinger_data *cdfinger)
{
	CDFINGER_FUNCTION("enter\n");
#ifdef CONFIG_PM_WAKELOCKS
	__pm_relax(cdfinger->cdfinger_lock);
#else
	wake_unlock(&cdfinger->cdfinger_lock);
#endif
	CDFINGER_FUNCTION("exit\n");
}

static int cdfinger_mode_init(struct cdfinger_data *cdfinger, uint8_t arg, enum work_mode mode)
{
	CDFINGER_DBG("mode=0x%x\n", mode);
	cdfinger->process_interrupt = 1;
	cdfinger->device_mode = mode;
	cdfinger->key_report = 0;

	return 0;
}

static void cdfinger_wake_lock(struct cdfinger_data *cdfinger,int arg)
{
	CDFINGER_DBG("cdfinger_wake_lock enter----------\n");
	if(arg)
	{
#ifdef CONFIG_PM_WAKELOCKS
		__pm_stay_awake(cdfinger->cdfinger_lock);
#else
		wake_lock(&cdfinger->cdfinger_lock);
#endif
	}
	else
	{
#ifdef CONFIG_PM_WAKELOCKS
		__pm_relax(cdfinger->cdfinger_lock);
		__pm_wakeup_event(cdfinger->cdfinger_lock, jiffies_to_msecs(3*HZ));
#else
		wake_unlock(&cdfinger->cdfinger_lock);
		wake_lock_timeout(&cdfinger->cdfinger_lock,3*HZ);
#endif
	}
}

//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#ifdef USE_INPUT_DEVICE
int cdfinger_report_key(struct cdfinger_data *cdfinger, unsigned long arg)
{
	key_report_t report;
	if (copy_from_user(&report, (key_report_t *)arg, sizeof(key_report_t)))
	{
		CDFINGER_ERR("%s err\n", __func__);
		return -1;
	}
	switch(report.key)
	{
	case KEY_UP:
		report.key=KEY_VOLUMEDOWN;
		break;
	case KEY_DOWN:
		report.key=KEY_VOLUMEUP;
		break;
	case KEY_RIGHT:
		report.key=KEY_PAGEUP;
		break;
	case KEY_LEFT:
		report.key=KEY_PAGEDOWN;
		break;
	default:
		break;
	}

	CDFINGER_FUNCTION("enter\n");
	input_report_key(cdfinger->cdfinger_inputdev, report.key, !!report.value);
	input_sync(cdfinger->cdfinger_inputdev);
	CDFINGER_FUNCTION("exit\n");

	return 0;
}

static int cdfinger_report_key_legacy(struct cdfinger_data *cdfinger, uint8_t arg)
{
	CDFINGER_FUNCTION("enter\n");
	input_report_key(cdfinger->cdfinger_inputdev, KEY_INTERRUPT, !!arg);
	input_sync(cdfinger->cdfinger_inputdev);
	CDFINGER_FUNCTION("exit\n");

	return 0;
}
#endif
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup

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

static int cdfinger_thread_func(void *arg)
{
	struct cdfinger_data *cdfinger = (struct cdfinger_data *)arg;

	do {
		wait_event_interruptible(waiter, cdfinger->thread_wakeup != 0);
		CDFINGER_DBG("cdfinger:%s,thread wakeup\n",__func__);
		cdfinger->thread_wakeup = 0;
#ifdef CONFIG_PM_WAKELOCKS
		__pm_wakeup_event(cdfinger->cdfinger_lock, jiffies_to_msecs(3*HZ));
#else
		wake_lock_timeout(&cdfinger->cdfinger_lock,3*HZ);
#endif

		if (cdfinger->device_mode == CDFINGER_INTERRUPT_MODE) {
			cdfinger->process_interrupt = 0;
			sign_sync = 1;
			wake_up_interruptible(&cdfinger_waitqueue);
			cdfinger_async_Report();
			continue;
		} 
//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#ifdef USE_INPUT_DEVICE
else if ((cdfinger->device_mode == CDFINGER_KEY_MODE) && (cdfinger->key_report == 0)) {

			input_report_key(cdfinger->cdfinger_inputdev, KEY_INTERRUPT, 1);
			input_sync(cdfinger->cdfinger_inputdev);
			cdfinger->key_report = 1;
		}
#endif
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
	}while(!kthread_should_stop());

	CDFINGER_ERR("thread exit\n");
	return -1;
}


static long cdfinger_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct cdfinger_data *cdfinger = filp->private_data;
	int ret = 0;

	CDFINGER_FUNCTION("enter,cmd=%x，\n",cmd);
	if(cdfinger == NULL)
	{
		CDFINGER_ERR("%s: fingerprint please open device first!\n", __func__);
		return -EIO;
	}

	mutex_lock(&cdfinger->buf_lock);
	switch (cmd) {
		case CDFINGER_INIT:
            CDFINGER_FUNCTION("cdfinger_ioctl init\n");
#ifndef TBASE_TEE_COMPATIBLE
			if (cdfinger->res_init == 1) {
				CDFINGER_ERR("res has init\n");
				break;
			}
			if(cdfinger_parse_dts(cdfinger))
			{
				CDFINGER_ERR("%s: parse dts failed!\n", __func__);
				break;
			}
			cdfinger_power_on(cdfinger);
			cdfinger_reset(50);
			ret = cdfinger_getirq_from_platform(cdfinger);
			if (ret != 0) {
				CDFINGER_ERR("get irq error\n");
				break;
			}
			ret = request_threaded_irq(cdfinger->irq, (irq_handler_t)cdfinger_interrupt_handler, NULL,
							IRQF_TRIGGER_RISING | IRQF_ONESHOT, "cdfinger-irq", cdfinger);
			if(ret != 0){
				CDFINGER_ERR("request_irq error\n");
				break;
			}

			cdfinger->cdfinger_thread = kthread_run(cdfinger_thread_func, cdfinger, "cdfinger_thread");
			if (IS_ERR(cdfinger->cdfinger_thread)) {
				CDFINGER_ERR("kthread_run is failed\n");
				ret = -1;
				break;
			}
			enable_irq_wake(cdfinger->irq);
			cdfinger->irq_enabled = 1;
			cdfinger->res_init = 1;
#endif

			break;
		case CDFINGER_GETIMAGE:
			break;
		case CDFINGER_RELEASE_DEVICE:
			if(cdfinger->fps_pinctrl != NULL){
	           		devm_pinctrl_put(cdfinger->fps_pinctrl);
				cdfinger->fps_pinctrl = NULL;
			}
			cdfinger_disable_irq(cdfinger);
			if(cdfinger->irq){
				free_irq(cdfinger->irq,cdfinger);
				cdfinger->irq_enabled = 0;
			}
			misc_deregister(cdfinger->miscdev);
//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#ifdef USE_INPUT_DEVICE
			input_unregister_device(cdfinger->cdfinger_inputdev);
			input_free_device(cdfinger->cdfinger_inputdev);
			cdfinger->cdfinger_inputdev = NULL;
#endif
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
			//devm_pinctrl_put(cdfinger->spi->dev);
			//cdfinger_power_off(cdfinger);
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
		case CDFINGER_RELEASE_WAKELOCK:
			cdfinger_release_wakelock(cdfinger);
			break;
		case CDFINGER_INITERRUPT_KEYMODE:
			ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_KEY_MODE);
			break;
		case CDFINGER_CHECK_INTERRUPT:
			break;
		case CDFINGER_SET_SPI_SPEED:
			break;
		case CDFINGER_WAKE_LOCK:
			cdfinger_wake_lock(cdfinger,arg);
			break;
		case CDFINGER_SENSOR_TEST:
			CDFINGER_FUNCTION("cdfinger sensor\n");
			break;
//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#ifdef USE_INPUT_DEVICE
		case CDFINGER_REPORT_KEY:
			ret = cdfinger_report_key(cdfinger, arg);
			break;
		case CDFINGER_REPORT_KEY_LEGACY:
			ret = cdfinger_report_key_legacy(cdfinger, arg);
			break;
#endif
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
		case CDFINGER_POWERDOWN:
			break;
		case CDFINGER_POWER_ON:
			CDFINGER_DBG("cmd=CDFINGER_POWER_ON\n");
			cdfinger_power_on(cdfinger);
		break;
		case CDFINGER_ENABLE_IRQ:
			cdfinger_enable_irq(cdfinger);
			break;
		case CDFINGER_DISABLE_IRQ:
			cdfinger_disable_irq(cdfinger);
			break;
		case CDFINGER_ENABLE_CLK:
		case CDFINGER_SPI_CLK:
			if (arg == 1)
				mt_spi_enable_master_clk(cdfinger->spi);

			else if (arg == 0)
				mt_spi_disable_master_clk(cdfinger->spi);

			break;
		case CDFINGER_HW_RESET:
			cdfinger_reset(arg);
			break;
		case CDFINGER_SETID:
			cdfinger->chip_id = arg;
			CDFINGER_DBG("set cdfinger chip id 0x%x\n",cdfinger->chip_id);
			break;
		case CDFINGER_GETID:
			ret = cdfinger->chip_id;
			CDFINGER_DBG("get cdfinger chip id 0x%x\n",ret);
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

static void cdfinger_async_Report(void)
{
	struct cdfinger_data *cdfinger = g_cdfinger;

	CDFINGER_FUNCTION("enter\n");
	kill_fasync(&cdfinger->async_queue, SIGIO, POLL_IN);
	CDFINGER_FUNCTION("exit\n");
}


static irqreturn_t cdfinger_interrupt_handler(unsigned irq, void *arg)
{
	struct cdfinger_data *cdfinger = (struct cdfinger_data *)arg;
printk("fps980 cdfinger %s 111\n",__func__);
	cdfinger->cdfinger_interrupt = 1;
	if (cdfinger->process_interrupt == 1)
	{
		cdfinger->thread_wakeup = 1;
		wake_up_interruptible(&waiter);
	}
printk("fps980 cdfinger %s 222\n",__func__);
	return IRQ_HANDLED;
}

//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#ifdef USE_INPUT_DEVICE
static int cdfinger_create_inputdev(struct cdfinger_data *cdfinger)
{
	cdfinger->cdfinger_inputdev = input_allocate_device();
	if (!cdfinger->cdfinger_inputdev) {
		CDFINGER_ERR("cdfinger->cdfinger_inputdev create faile!\n");
		return -ENOMEM;
	}
	__set_bit(EV_KEY, cdfinger->cdfinger_inputdev->evbit);
	__set_bit(KEY_INTERRUPT, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F1, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F2, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F3, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F4, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_VOLUMEUP, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_VOLUMEDOWN, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_PAGEUP, cdfinger->cdfinger_inputdev->keybit);
    __set_bit(KEY_PAGEDOWN, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_UP, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_LEFT, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_RIGHT, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_DOWN, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_ENTER, cdfinger->cdfinger_inputdev->keybit);

	cdfinger->cdfinger_inputdev->id.bustype = BUS_HOST;
	cdfinger->cdfinger_inputdev->name = "cdfinger_inputdev";
	if (input_register_device(cdfinger->cdfinger_inputdev)) {
		CDFINGER_ERR("register inputdev failed\n");
		input_free_device(cdfinger->cdfinger_inputdev);
		return -1;
	}

	return 0;
}
#endif
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup

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
			CDFINGER_DBG("sunlin==FB_BLANK_UNBLANK==\n");
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
			CDFINGER_DBG("sunlin==FB_BLANK_POWERDOWN==\n");
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

#if defined(TBASE_TEE_COMPATIBLE) 
static int tee_spi_transfer(const char *txbuf, char *rxbuf, int len)
{
    struct spi_transfer t;
    struct spi_message m;
    memset(&t, 0, sizeof(t));
    spi_message_init(&m);
    t.tx_buf = txbuf;
    t.rx_buf = rxbuf;
    t.bits_per_word = 8;
    t.len = len;
    t.speed_hz = 1*1000000;
    spi_message_add_tail(&t, &m);
    return spi_sync(global_spidev, &m);
}
#endif

//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
static ssize_t adm_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	if (fingerprint_adm == 0){
		 return snprintf(buf, 2, "1");
	}else{
		 return snprintf(buf, 2, "0");
	}
}

static struct device_attribute adm_on_off_attr = {
	.attr = {
		.name = "adm",
		.mode = S_IRUSR,
	},
	.show = adm_show,
};
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup

static int cdfinger_probe(struct platform_device *pldev)
{
	struct cdfinger_data *cdfinger = NULL;
    struct device *dev = &pldev->dev;
	int status = -ENODEV;
#if defined(COMPAT_VENDOR) || defined(PINBO_TEE_COMPATIBLE) || defined(TBASE_TEE_COMPATIBLE) 
	int TestCnt;
	uint8_t chipid_tx[4] = {0x74, 0x66, 0x66, 0x66};
    uint8_t chipid_rx[4] = {0};
	u8 reset = 0x0c;
    u8 start = 0x18;
    u8 read;
#endif

//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
	struct class    *fingerprint_adm_class;
	struct device *fingerprint_adm_dev;
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup

	CDFINGER_PROBE("enter.......\n");
    printk("fps980 %s enter\n",__func__);
	cdfinger = kzalloc(sizeof(struct cdfinger_data), GFP_KERNEL);
	if (!cdfinger) {
		CDFINGER_ERR("alloc cdfinger failed!\n");
		return -ENOMEM;;
	}

	g_cdfinger = cdfinger;
	cdfinger->spi = spi_fingerprint;
    global_spidev = spi_fingerprint;
/*
	cdfinger->spi->bits_per_word = 8;
	cdfinger->spi->mode = SPI_MODE_0;
	cdfinger->spi->max_speed_hz    = 10 * 1000 * 1000;
	memcpy (&cdfinger->mtk_spi_config, &spi_conf, sizeof (struct mtk_chip_config));
	cdfinger->spi->controller_data = (void *)&cdfinger->mtk_spi_config;
	if(spi_setup(cdfinger->spi) != 0)
	{
		CDFINGER_ERR("%s: spi setup failed!\n", __func__);
		goto free_cdfinger;
	}
*/
	dev_set_drvdata(dev, cdfinger);
    cdfinger_parse_dts(cdfinger);

#ifdef HAS_RESET_PIN
	cdfinger_reset(50);
#endif

#if defined(TBASE_TEE_COMPATIBLE)  //1
	mt_spi_enable_master_clk(cdfinger->spi);
    chipid_tx[0]= 0x74;
    for(TestCnt = 0;TestCnt < 5;TestCnt++){
		status = tee_spi_transfer(&reset, &read, 1);
        mdelay(1);
        status = tee_spi_transfer(&reset, &read, 1);
        mdelay(1);
        status = tee_spi_transfer(&start, &read, 1);
        mdelay(1);
        CDFINGER_PROBE("work_mode_switch, status = %d\n",status);
        status = tee_spi_transfer(chipid_tx, chipid_rx, 4);
        printk("fps980 %s tee_spi_transfer status = %d \n",__func__,status);
		if (status == 0) {
			if (chipid_rx[3] == 0x80 || chipid_rx[3] == 0x98 || chipid_rx[3] == 0x56 || chipid_rx[3] == 0x88){
				CDFINGER_PROBE("get id success(%x)\n",chipid_rx[3]);
                fingerprint_adm = 0; //oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
                fingtech_fp_exist = true;
				break;
			} else {
				CDFINGER_PROBE("get id failed(%x) count(%d)\n",chipid_rx[3],TestCnt);
				status = -1;
				cdfinger_reset(10);
				if(TestCnt == 4){
					CDFINGER_ERR("cdfinger: check id failed! status=%d\n",status);
					mt_spi_disable_master_clk(global_spidev);
					goto free_cdfinger;
				}
			}
		} else {
			CDFINGER_ERR("spi invoke err()\n");
			status = -1;
			if(TestCnt == 4){
                mt_spi_disable_master_clk(global_spidev);
				goto free_cdfinger;
			}
		}
	}
	cdfinger_reset(1);
    mt_spi_disable_master_clk(global_spidev);
#endif

	mutex_init(&cdfinger->buf_lock);
#ifdef CONFIG_PM_WAKELOCKS
	//wakeup_source_init(&cdfinger->cdfinger_lock, "cdfinger wakelock");
	cdfinger->cdfinger_lock = wakeup_source_create("cdfinger wakelock"); 
        wakeup_source_add(cdfinger->cdfinger_lock);
#else
	wake_lock_init(&cdfinger->cdfinger_lock, WAKE_LOCK_SUSPEND, "cdfinger wakelock");
#endif
	status = misc_register(&cdfinger_dev);
	if (status < 0) {
		CDFINGER_ERR("%s: cdev register failed!\n", __func__);
		goto free_lock;
	}
	cdfinger->miscdev = &cdfinger_dev;
//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#ifdef USE_INPUT_DEVICE
	if(cdfinger_create_inputdev(cdfinger) < 0)
	{
		CDFINGER_ERR("%s: inputdev register failed!\n", __func__);
		goto free_device;
	}
#endif
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
#ifdef TBASE_TEE_COMPATIBLE
	if(cdfinger_getirq_from_platform(cdfinger)!=0)
		goto free_device;
	status = request_threaded_irq(cdfinger->irq, (irq_handler_t)cdfinger_interrupt_handler, NULL,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT, "cdfinger-irq", cdfinger);
	if(status){
		CDFINGER_ERR("request_irq error\n");
		goto free_device;
	}

	enable_irq_wake(cdfinger->irq);
//	cdfinger_enable_irq(cdfinger);
	cdfinger->irq_enabled = 1;
	cdfinger->cdfinger_thread = kthread_run(cdfinger_thread_func, cdfinger, "cdfinger_thread");
	if (IS_ERR(cdfinger->cdfinger_thread)) {
		CDFINGER_ERR("kthread_run is failed\n");
		goto free_device;
	}
#endif 
	cdfinger->notifier.notifier_call = cdfinger_fb_notifier_callback;
    fb_register_client(&cdfinger->notifier);

//+oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
	fingerprint_adm_class = class_create(THIS_MODULE, "fingerprint");
	if (IS_ERR(fingerprint_adm_class)){
		printk(KERN_ERR "fpc  class_create failed \n");
	}

	fingerprint_adm_dev = device_create(fingerprint_adm_class, NULL, 0, NULL, "fingerprint");

	if (device_create_file(fingerprint_adm_dev, &adm_on_off_attr) < 0){
		printk(KERN_ERR "Failed to create device file(%s)!\n", adm_on_off_attr.attr.name);
	}
//-oa 6648421,gjx.wt,add,20200916,fingertech fingerprint bringup
    printk("fps980 %s success\n",__func__);
	CDFINGER_PROBE("exit\n");

	return 0;
#if 0
free_irq:
	free_irq(cdfinger->irq, cdfinger);
free_work:
	input_unregister_device(cdfinger->cdfinger_inputdev);
	cdfinger->cdfinger_inputdev = NULL;
	input_free_device(cdfinger->cdfinger_inputdev);
#endif 
free_device:
	misc_deregister(&cdfinger_dev);
free_lock:
#ifdef CONFIG_PM_WAKELOCKS
	//wakeup_source_trash(&cdfinger->cdfinger_lock);
	wakeup_source_remove(cdfinger->cdfinger_lock);
#else
	wake_lock_destroy(&cdfinger->cdfinger_lock);
#endif
	mutex_destroy(&cdfinger->buf_lock);
free_cdfinger:
    devm_pinctrl_put(cdfinger->fps_pinctrl);
	kfree(cdfinger);
	cdfinger = NULL;

	return -1;
}



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
	{ .compatible = "mediatek,finger-fp", },
	{},
};
MODULE_DEVICE_TABLE(of, cdfinger_of_match);

static struct platform_driver cdfinger_driver = {
	.driver = {
		.name = "fps980",
		.owner = THIS_MODULE,
		.pm = &cdfinger_pm,
		.of_match_table = of_match_ptr(cdfinger_of_match),
	},
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
		.controller_data = &spi_conf,
	},
};
#endif

static int __init cdfinger_fingerprint_init(void)
{
    int status;

    printk("fps980 %s enter\n",__func__);
    if (sunwave_fp_exist) {
        printk(KERN_ERR "%s sunwave sensor has been detected, so exit FPC sensor detect.\n",__func__);
        return -EINVAL;
    }

    if (egis_fp_exist) {
        printk(KERN_ERR "%s egis sensor has been detected, so exit FPC sensor detect.\n",__func__);
        return -EINVAL;
    }
	
    if(fpc_or_chipone_exist != -1){
        printk(KERN_ERR "%s, chipone sensor has been detected.\n", __func__);
        return -EINVAL;
    }

    if(spi_fingerprint == NULL)
        pr_notice("%s Line:%d spi device is NULL,cannot spi transfer\n", __func__, __LINE__);
    else {
        status = platform_driver_register(&cdfinger_driver);
	    if (status !=0) {
		    printk("%s, fps980 fingerprint init failed.\n", __func__);
        }
    }

    printk("fps980 %s exit\n",__func__);
    return 0;
}

//static void cdfinger_spi_exit(void
static void __exit cdfinger_fingerprint_exit(void)
{
    printk("fps980 %s\n",__func__);
    platform_driver_unregister(&cdfinger_driver);
}
late_initcall(cdfinger_fingerprint_init);
module_exit(cdfinger_fingerprint_exit);

MODULE_DESCRIPTION("cdfinger tee Driver");
MODULE_AUTHOR("shuaitao@cdfinger.com");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cdfinger");
