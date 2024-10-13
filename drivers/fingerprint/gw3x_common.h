#ifndef __GF_SPI_DRIVER_H
#define __GF_SPI_DRIVER_H

#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/cdev.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <net/sock.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/of_platform.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef ENABLE_SENSORS_FPRINT_SECURE
#include <linux/pm_runtime.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#endif

#include <linux/pinctrl/consumer.h>
#include "../pinctrl/core.h"
#include <linux/version.h>
#include <linux/uaccess.h>
#include "fingerprint_common.h"

/*
 * This feature is temporary for exynos AP only.
 * It's for control GPIO config on enabled TZ before enable GPIO protection.
 * If it's still defined this feature after enable GPIO protection,
 * it will be happened kernel panic
 * So it should be un-defined after enable GPIO protection
 */
#undef DISABLED_GPIO_PROTECTION

#define SPI_CLK_DIV_MTK 12

#define GF_IOC_MAGIC	'g'

#define GF_GW32J_CHIP_ID		0x00220e
#define GF_GW32N_CHIP_ID		0x002215
#define GF_GW36H_CHIP_ID		0x002504
#define GF_GW36C_CHIP_ID		0x002502
#define GF_GW36T1_CHIP_ID		0x002507 /* FAB : SMIC */
#define GF_GW36T2_CHIP_ID		0x002510 /* FAB : SILTRRA */
#define GF_GW36T3_CHIP_ID		0x002508 /* FAB : CANSEMI */
#define GF_GW39B_CHIP_ID		0x02010502
#define GF_GW39U_CHIP_ID		0x02010505
#define GF_GW36T1_SHIFT_CHIP_ID	0x004a0f
#define GF_GW36T2_SHIFT_CHIP_ID	0x004a21
#define GF_GW36T3_SHIFT_CHIP_ID	0x004a11

#define gw3x_SPI_BAUD_RATE 9600000
#define TANSFER_MAX_LEN (512*1024)
#define SPI_TRANSFER_DELAY 5

enum gf_netlink_cmd {
	GF_NETLINK_TEST = 0,
	GF_NETLINK_IRQ = 1,
	GF_NETLINK_SCREEN_OFF,
	GF_NETLINK_SCREEN_ON
};

struct gf_ioc_transfer {
	u8 cmd;    /* spi read = 0, spi  write = 1 */
	u8 reserved;
	u16 addr;
	u32 len;
	u8 *buf;
};

struct gf_ioc_transfer_raw {
	u32 len;
	u8 *read_buf;
	u8 *write_buf;
	uint32_t high_time;
	uint32_t bits_per_word;
};

struct gf_ioc_transfer_32 {
	u8 cmd;    /* spi read = 0, spi  write = 1 */
	u8 reserved;
	u16 addr;
	u32 len;
	u32 buf;
};

struct gf_ioc_transfer_raw_32 {
	u32 len;
	u32 read_buf;
	u32 write_buf;
	uint32_t high_time;
	uint32_t bits_per_word;
};

/* define commands */
#define GF_IOC_INIT             _IOR(GF_IOC_MAGIC, 0, u8)
#define GF_IOC_EXIT             _IO(GF_IOC_MAGIC, 1)
#define GF_IOC_RESET            _IO(GF_IOC_MAGIC, 2)
#define GF_IOC_ENABLE_IRQ       _IO(GF_IOC_MAGIC, 3)
#define GF_IOC_DISABLE_IRQ      _IO(GF_IOC_MAGIC, 4)
#define GF_IOC_ENABLE_SPI_CLK   _IOW(GF_IOC_MAGIC, 5, uint32_t)
#define GF_IOC_DISABLE_SPI_CLK  _IO(GF_IOC_MAGIC, 6)
#define GF_IOC_ENABLE_POWER     _IO(GF_IOC_MAGIC, 7)
#define GF_IOC_DISABLE_POWER    _IO(GF_IOC_MAGIC, 8)
#define GF_IOC_ENTER_SLEEP_MODE _IO(GF_IOC_MAGIC, 10)
#define GF_IOC_GET_FW_INFO      _IOR(GF_IOC_MAGIC, 11, u8)
#define GF_IOC_REMOVE           _IO(GF_IOC_MAGIC, 12)

/* for SPI REE transfer */
#ifndef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
#define GF_IOC_TRANSFER_CMD     _IOWR(GF_IOC_MAGIC, 15, \
		struct gf_ioc_transfer_32)
#else
#define GF_IOC_TRANSFER_CMD     _IOWR(GF_IOC_MAGIC, 15, \
		struct gf_ioc_transfer)
#endif
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
#define GF_IOC_TRANSFER_RAW_CMD _IOWR(GF_IOC_MAGIC, 16, \
		struct gf_ioc_transfer_raw_32)
#else
#define GF_IOC_TRANSFER_RAW_CMD _IOWR(GF_IOC_MAGIC, 16, \
		struct gf_ioc_transfer_raw)
#endif
#else
#define GF_IOC_SET_SENSOR_TYPE _IOW(GF_IOC_MAGIC, 18, unsigned int)
#endif
#define GF_IOC_POWER_CONTROL _IOW(GF_IOC_MAGIC, 19, unsigned int)
#define GF_IOC_SPEEDUP			_IOW(GF_IOC_MAGIC, 20, unsigned int)
#define GF_IOC_SET_LOCKSCREEN	_IOW(GF_IOC_MAGIC, 21, unsigned int)
#define GF_IOC_GET_ORIENT		_IOR(GF_IOC_MAGIC, 22, unsigned int)

#define GF_IOC_MAXNR    23  /* THIS MACRO IS NOT USED NOW... */

struct gf_device {
	dev_t devno;
	struct cdev cdev;
	struct device *dev;
	struct device *fp_device;
	struct class *class;
	struct spi_device *spi;
	int device_count;

	spinlock_t spi_lock;
	struct list_head device_entry;
	struct mutex release_lock;
	struct sock *nl_sk;
	u8 buf_status;
	struct notifier_block notifier;
	u8 irq_enabled;
	u8 sig_count;
	u8 system_status;

	u32 pwr_gpio;
	u32 reset_gpio;
	int prev_bits_per_word;
	u32 irq_gpio;
	u32 irq;
	u8  need_update;
	/* for netlink use */
	int pid;

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	u8 *spi_buffer;
	u8 *tx_buf;
	u8 *rx_buf;
	struct mutex buf_lock;
#endif
	unsigned int orient;
	int sensortype;
	int reset_count;
	int interrupt_count;
	bool ldo_onoff;
	bool tz_mode;
	const char *chipid;
	struct wakeup_source *wake_lock;
	const char *btp_vdd;
	const char *position;
	struct regulator *regulator_3p3;
	struct pinctrl *p;
	struct pinctrl_state *pins_poweron;
	struct pinctrl_state *pins_poweroff;
	struct spi_clk_setting *clk_setting;
	struct boosting_config *boosting;
	struct debug_logger *logger;
};


int gw3x_get_gpio_dts_info(struct device *dev, struct gf_device *gf_dev);
void gw3x_cleanup_info(struct gf_device *gf_dev);
void gw3x_hw_power_enable(struct gf_device *gf_dev, u8 onoff);
void gw3x_hw_reset(struct gf_device *gf_dev, u8 delay);

#ifndef ENABLE_SENSORS_FPRINT_SECURE
void gw3x_spi_setup_conf(struct gf_device *gf_dev, u32 bits);
int gw3x_spi_read_bytes(struct gf_device *gf_dev, u16 addr,
		u32 data_len, u8 *rx_buf);
int gw3x_spi_write_bytes(struct gf_device *gf_dev, u16 addr,
		u32 data_len, u8 *tx_buf);
int gw3x_spi_read_byte(struct gf_device *gf_dev, u16 addr, u8 *value);
int gw3x_spi_write_byte(struct gf_device *gf_dev, u16 addr, u8 value);
int gw3x_ioctl_transfer_raw_cmd(struct gf_device *gf_dev,
		unsigned long arg, unsigned int bufsiz);
int gw3x_init_buffer(struct gf_device *gf_dev);
int gw3x_free_buffer(struct gf_device *gf_dev);
#endif /* !ENABLE_SENSORS_FPRINT_SECURE */

extern int fingerprint_register(struct device *dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
extern void fingerprint_unregister(struct device *dev,
	struct device_attribute *attributes[]);

#ifdef CONFIG_BATTERY_SAMSUNG
extern unsigned int lpcharge;
#endif

#endif	/* __GF_SPI_DRIVER_H */
