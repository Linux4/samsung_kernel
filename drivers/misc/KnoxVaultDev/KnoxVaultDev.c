#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
// #include <linux/ipc_logging.h>

// #define READ_i2c_SLAVE_FROM_KERNEL
// #define READ_i2c_SLAVE_BY_INTERRUPT

// #define K250A_I2C_DRV_STR   "sec,k250a-i2c"   // kept same as dts
#define K250A_I2C_DRV_STR   "sec_k250a"   // kept same as dts
#define K250A_I2C_DEV_ID    "k250a-i2c"   // driver name

// Max device count for this driver
#define DEV_COUNT            1
// k250a device class
#define CLASS_NAME          "k250a-i2c_class"

//  K250A character device name, this will be in /dev/
#define K250A_CHAR_DEV_NAME     "k250a-i2c"

#define MAX_BUFFER_SIZE                 (512)

// Retry count for normal write
#define NO_RETRY                (1)
#define WAKEUP_SRC_TIMEOUT      (2000)

/* command response timeout */
#define NCI_CMD_RSP_TIMEOUT     (2000)  // 2s
/* Time to wait before retrying i2c writes */
#define WRITE_RETRY_WAIT_TIME_USEC      (1000)

// Linux kernel5.10
// #define NUM_OF_IPC_LOG_PAGES    (2)
#define PKT_MAX_LEN             (4) // no of max bytes to print for cmd/resp

/*
#define GET_IPCLOG_MAX_PKT_LEN(c)	((c > PKT_MAX_LEN) ? PKT_MAX_LEN : c)

#define K250ALOG_IPC(k250a_dev, log_to_dmesg, x...)	\
do { \
	ipc_log_string(k250a_dev->ipcl, x); \
	if (log_to_dmesg) { \
		if (k250a_dev->k250a_device) \
			dev_err((k250a_dev->k250a_device), x); \
		else \
			pr_err(x); \
	} \
} while (0)
*/

// Interface specific parameters
struct i2c_dev {
	struct i2c_client *client;
	/* IRQ parameters */
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
	bool irq_enabled;
	spinlock_t irq_enabled_lock;
	/* K250A_IRQ wake-up state */
	bool irq_wake_up;
#endif
};

/* Device specific structure */
struct k250a_dev {
	wait_queue_head_t read_wq;
	struct mutex read_mutex;
	struct mutex write_mutex;
	uint8_t *read_kbuf;
	uint8_t *write_kbuf;
	struct mutex dev_ref_mutex;
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
	unsigned int dev_ref_count;
#endif
	struct class *k250a_class;
	struct device *k250a_device;
	struct cdev c_dev;
	dev_t devno;

	struct i2c_dev i2c_dev;

	/* read buffer*/
	size_t kbuflen;
	u8 *kbuf;
	// void *ipcl;
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
	int (*k250a_enable_intr)(struct k250a_dev *dev);
	int (*k250a_disable_intr)(struct k250a_dev *dev);
#endif
};

uint8_t read_kbuf_k250a[MAX_BUFFER_SIZE] = {0};
uint8_t write_kbuf_k250a[MAX_BUFFER_SIZE] = {0};

#if defined(READ_i2c_SLAVE_FROM_KERNEL)
static struct k250a_dev *g_k250a_dev = NULL;
#endif

#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
static int i2c_disable_irq(struct k250a_dev *dev)
{
	unsigned long flags;

        if (!dev) {
            pr_err("[%s]l=%d: dev is NULL\n", __FUNCTION__, __LINE__);
            return -ENODEV;
        }

	spin_lock_irqsave(&dev->i2c_dev.irq_enabled_lock, flags);
	if (dev->i2c_dev.irq_enabled) {
		disable_irq_nosync(dev->i2c_dev.client->irq);
		dev->i2c_dev.irq_enabled = false;
	}
	spin_unlock_irqrestore(&dev->i2c_dev.irq_enabled_lock, flags);

	return 0;
}

static int i2c_enable_irq(struct k250a_dev *dev)
{
	unsigned long flags;

        if (!dev) {
            pr_err("[%s]l=%d: dev is NULL\n", __FUNCTION__, __LINE__);
            return -ENODEV;
        }

	spin_lock_irqsave(&dev->i2c_dev.irq_enabled_lock, flags);
	if (!dev->i2c_dev.irq_enabled) {
		dev->i2c_dev.irq_enabled = true;
		enable_irq(dev->i2c_dev.client->irq);
	}
	spin_unlock_irqrestore(&dev->i2c_dev.irq_enabled_lock, flags);

	return 0;
}
#endif

#if defined(READ_i2c_SLAVE_FROM_KERNEL)
#define MAX_FILE_DATA_SIZE 256

#define CHIP_INFO_SIZE              (44)
#define KEY_FLAG_INFO_SIZE          (4)
#define PARTITION_INFO_SIZE         (4)
#define IMG_VERSION_SIZE            (4)
#define IMG_DATE_SIZE               (12)
#define VERSION_INFO_SIZE           (IMG_VERSION_SIZE + IMG_DATE_SIZE)

typedef struct img_ver_s {
	uint8_t version[IMG_VERSION_SIZE];
	uint8_t build_date[IMG_DATE_SIZE];
} img_ver_t;

typedef struct {
	uint8_t chip[CHIP_INFO_SIZE];
	uint8_t key_flag[KEY_FLAG_INFO_SIZE];
	uint8_t partition[PARTITION_INFO_SIZE];
	img_ver_t bootloader;
	img_ver_t core;
	img_ver_t crypto;
} __attribute__((packed)) ese_info_t;

// main
unsigned char info[MAX_FILE_DATA_SIZE] = {0x00, };
unsigned int info_size = MAX_FILE_DATA_SIZE;
// boot_firmware
unsigned char txdata_boot_firmware[] = {0x12, 0x00, 0x04, 0x00, 0xfc, 0x00, 0x00, 0xea};
unsigned int txdata_size_boot_firmware = 8;
unsigned char rxdata_boot_firmware[MAX_FILE_DATA_SIZE] = {0x0, };
unsigned int rxdata_size_boot_firmware = 6;
// get_info
unsigned char txdata_get_info[] = {0x12, 0x00, 0x05, 0x00, 0xb1, 0x3b, 0x00, 0x84, 0x19};
unsigned int txdata_size_get_info = 9;
unsigned char rxdata_get_info[MAX_FILE_DATA_SIZE] = {0x0, };
unsigned int rxdata_size_get_info = 0x8A;

static int i2c_read_k250a(struct k250a_dev *k250a_dev, u8 *write_buf, u32 write_buf_len,
                        u8 *read_buf, u32 read_buf_len, int max_retry_cnt);

static int i2c_write_k250a(struct k250a_dev *k250a_dev, u8 *write_buf, u32 count,
                            int max_retry_cnt);

static void print_buffer(const char *buf_tag, uint8_t *buffer, uint32_t buffer_size)
{
    char tmp_buffer[49] = { 0 };
    char *tmp_p = tmp_buffer;
    uint32_t i = 0;
    pr_err("[%s]l=%d: %s : \n", __FUNCTION__, __LINE__, buf_tag);

    for (i = 0; i < buffer_size; i ++) {
        if (i != 0 && (i % 16) == 0) {
            pr_err("[%s]l=%d: %s\n", __FUNCTION__, __LINE__, tmp_buffer);
            tmp_p = tmp_buffer;
        }
        sprintf(tmp_p, " %02x", buffer[i]);
        tmp_p += 3;
    }
    pr_err("[%s]l=%d: %s\n", __FUNCTION__, __LINE__, tmp_buffer);
}

ssize_t k250a_i2c_dev_read_k250a_i2c_node(u8 *read_buf, u32 read_buf_len)
{
    int ret = 0;

    if (!read_buf || !read_buf_len) {
        pr_err("[%s]l=%d: read_buf is NULL, read_buf_len=%u\n",
            __FUNCTION__, __LINE__, read_buf_len);
        return -1;
    }

    mutex_lock(&g_k250a_dev->read_mutex);
    ret = i2c_read_k250a(g_k250a_dev, NULL, txdata_size_boot_firmware,
                    read_buf, read_buf_len, NO_RETRY);
    mutex_unlock(&g_k250a_dev->read_mutex);

    return ret;
}

ssize_t k250a_i2c_dev_write_k250a_i2c_node(u8 *write_buf, u32 count)
{
    int ret = 0;

    if (!write_buf || !count) {
        pr_err("[%s]l=%d: write_buf is NULL, read_buf_len=%u\n",
            __FUNCTION__, __LINE__, count);
        return -1;
    }

    mutex_lock(&g_k250a_dev->write_mutex);
    ret = i2c_write_k250a(g_k250a_dev, write_buf, count, NO_RETRY);
    mutex_unlock(&g_k250a_dev->write_mutex);

    return ret;
}

#define SLAVE_ADDRESS   0x21
// #define SLAVE_ADDRESS   0x23
#define MAX_POLLING_COUNT   100
static int receive_data_with_polling(unsigned char *data, unsigned int data_size)
{
    unsigned int data_len = 0;
    unsigned int rec_len = 0;
    unsigned int poll_cnt = MAX_POLLING_COUNT;

    do {
        usleep_range(2 * WRITE_RETRY_WAIT_TIME_USEC,
                    2 * WRITE_RETRY_WAIT_TIME_USEC + 100);
        data_len = k250a_i2c_dev_read_k250a_i2c_node(data, 1);
        if (data_len != 1) {
            pr_err("[%s]l=%d: mismatch receive size %u, %u\n",
                __FUNCTION__, __LINE__, 1, data_len);
            // return -1;
        }
    } while (data[0] == 0x0 && poll_cnt > 0);

    if (poll_cnt == 0) {
        pr_err("[%s]l=%d: response timeout\n", __FUNCTION__, __LINE__);
        // return -1;
    }

    if (data[0] != SLAVE_ADDRESS) {
        pr_err("[%s]l=%d: invalid slave address : 0x%02x\n",
            __FUNCTION__, __LINE__, data[0]);
        return -1;
    }

    data_len += k250a_i2c_dev_read_k250a_i2c_node(data + 1, 2);
    if (data_len != 3) {
        pr_err("[%s]l=%d: mismatch receive size %u, %u\n",
            __FUNCTION__, __LINE__, 3, data_len);
        // return -1;
    }

    rec_len = data[2] + 1;
    data_len += k250a_i2c_dev_read_k250a_i2c_node(data + 3, rec_len);
    if (data_len != 3 + rec_len) {
        pr_err("[%s]l=%d: mismatch receive size %u, %u\n",
            __FUNCTION__, __LINE__, 3 + rec_len, data_len);
        // return -1;
    }

    return data_len;
}

int boot_firmware(void)
{
    int ret_size = 0;

    // send txdata_boot_firmware
    ret_size = k250a_i2c_dev_write_k250a_i2c_node(txdata_boot_firmware, txdata_size_boot_firmware);
    if (ret_size != txdata_size_boot_firmware) {
        pr_err("[%s]l=%d: mismatch send size : %u, %d\n",
            __FUNCTION__, __LINE__, txdata_size_boot_firmware, ret_size);
    }
    ret_size = receive_data_with_polling(rxdata_boot_firmware, rxdata_size_boot_firmware);
    if (ret_size != rxdata_size_boot_firmware) {
        pr_err("[%s]l=%d: failed to star_bootFirmware (receive data fail)\n",
            __FUNCTION__, __LINE__);
        // return -1;
    }
    // parse the rxdata_boot_firmware
    if (rxdata_boot_firmware[3] != 0x90 && rxdata_boot_firmware[4] != 0x00) {
        pr_err("[%s]l=%d: failed to star_bootFirmware (FW booting fail), 0x%x 0x%x\n",
            __FUNCTION__, __LINE__, rxdata_boot_firmware[3], rxdata_boot_firmware[4]);
        // return -1;
    } else {
        pr_err("[%s]l=%d: star_bootFirmware (FW booting success)\n", __FUNCTION__, __LINE__);
    }

    return ret_size;
}

int get_info(unsigned char *info, unsigned int *info_size)
{
    int ret_size = 0;

    ret_size = k250a_i2c_dev_write_k250a_i2c_node(txdata_get_info, txdata_size_get_info);
    if (ret_size != txdata_size_get_info) {
        pr_err("[%s]l=%d: mismatch send size : %u, %d\n",
            __FUNCTION__, __LINE__, txdata_size_get_info, ret_size);
    }

    ret_size = receive_data_with_polling(rxdata_get_info, rxdata_size_get_info);
    if (ret_size != rxdata_size_get_info) {
        pr_err("[%s]l=%d: failed to get_info (receive data fail)\n", __FUNCTION__, __LINE__);
        // return -1;
    }

    if (rxdata_get_info[0x87] != 0x90 && rxdata_get_info[0x88] != 0x00) {
        pr_err("[%s]l=%d: failed to get_info (FW get information fail), return value : 0x%x 0x%x\n",
            __FUNCTION__, __LINE__, rxdata_get_info[0x87], rxdata_get_info[0x88]);
        // return -1;
    }

    if (memcpy(info, rxdata_get_info + 3, rxdata_size_get_info - 6) < 0) {
        pr_err("[%s]l=%d: failed to copy rx data\n", __FUNCTION__, __LINE__);
        // return -1;
    }
    *info_size = rxdata_size_get_info - 6;

	return 0;
}
#endif  // READ_i2c_SLAVE_FROM_KERNEL

static ssize_t show_k250a_i2c_node(struct device *dev,
					struct device_attribute *attr, char *buf)
{
    if (!dev || !attr || !buf) {
        pr_err("[%s]l=%d: dev, attr, buf is NULL\n", __FUNCTION__, __LINE__);
        return -1;
    }
#if defined(READ_i2c_SLAVE_FROM_KERNEL)
    memcpy(buf, info, info_size);
#endif
    return 0;
}

static ssize_t store_k250a_i2c_node(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
#if defined(READ_i2c_SLAVE_FROM_KERNEL)
    ese_info_t *version = NULL;
    char date[13] = { 0 };
#endif
    if (!dev || !attr || !buf || !len) {
        pr_err("[%s]l=%d: dev, attr, buf is NULL, len=%d\n",
            __FUNCTION__, __LINE__, len);
        return -1;
    }
#if defined(READ_i2c_SLAVE_FROM_KERNEL)
    if (buf[0] == '1') {
        // boot_firmware
        if (boot_firmware() < 0) {
            pr_err("[%s]l=%d: boot_firmware fail\n", __FUNCTION__, __LINE__);
            // return -1;
        }
        pr_err("[%s]l=%d: Boot Firmware Success\n", __FUNCTION__, __LINE__);
        // get_info
        if (get_info(info, &info_size) < 0) {
            pr_err("[%s]l=%d: get_info fail\n", __FUNCTION__, __LINE__);
            // return -1;
        }
        pr_err("[%s]l=%d: get fw information success\n", __FUNCTION__, __LINE__);

        version = (ese_info_t *)info;

        print_buffer("chip", version->chip, CHIP_INFO_SIZE);
        print_buffer("keyflag", version->key_flag, KEY_FLAG_INFO_SIZE);
        print_buffer("partition", version->partition, PARTITION_INFO_SIZE);
        print_buffer("BL", version->bootloader.version, IMG_VERSION_SIZE);
        date[IMG_DATE_SIZE] = '\0';
        memcpy(date, version->bootloader.build_date, IMG_DATE_SIZE);
        pr_err("[%s]l=%d: BL build date : %s\n", __FUNCTION__, __LINE__, date);
        print_buffer("CORE", version->core.version, IMG_VERSION_SIZE);
        memcpy(date, version->core.build_date, IMG_DATE_SIZE);
        pr_err("[%s]l=%d: CORE build date : %s\n", __FUNCTION__, __LINE__, date);
        print_buffer("CRYPTO", version->crypto.version, IMG_VERSION_SIZE);
        memcpy(date, version->crypto.build_date, IMG_DATE_SIZE);
        pr_err("[%s]l=%d: CRYPTO build date : %s\n", __FUNCTION__, __LINE__, date);
    }
#endif
    pr_err("[%s]l=%d: buf[0]=%d, len=%d\n",
        __FUNCTION__, __LINE__, buf[0], len);

    return len;
}

// /sys/class/k250a-i2c_class/k250a-i2c/k250a_i2c_node
static DEVICE_ATTR(k250a_i2c_node, S_IWUSR|S_IRUSR,
				show_k250a_i2c_node, store_k250a_i2c_node);

static long k250a_dev_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	struct k250a_dev *k250a_dev = pfile->private_data;

        if (!k250a_dev || !pfile) {
            pr_err("[%s]l=%d: k250a_dev, pfile is NULL\n", __FUNCTION__, __LINE__);
            return -ENODEV;
        }

	pr_err("[%s]l=%d: cmd=%x, arg=%zx\n", __FUNCTION__, __LINE__, cmd, arg);

	return ret;
}

int k250a_dev_open(struct inode *inode, struct file *filp)
{
	struct k250a_dev *k250a_dev = container_of(inode->i_cdev,
					struct k250a_dev, c_dev);

        if (!k250a_dev || !filp) {
            pr_err("[%s]l=%d: k250a_dev, filp is NULL\n", __FUNCTION__, __LINE__);
            return -ENODEV;
        }

	pr_err("[%s]l=%d: %d, %d\n", __FUNCTION__, __LINE__, imajor(inode), iminor(inode));

	/* Set flag to block freezer fake signal if not set already.
	 * Without this Signal being set, Driver is trying to do a read
	 * which is causing the delay in moving to Hibernate Mode.
	 */
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
	if (!(current->flags & PF_NOFREEZE)) {
		current->flags |= PF_NOFREEZE;
		pr_err("[%s]l=%d: current->flags=0x%x.\n", __FUNCTION__, __LINE__, current->flags);
	}
#endif
	mutex_lock(&k250a_dev->dev_ref_mutex);

	filp->private_data = k250a_dev;
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
	if (k250a_dev->dev_ref_count == 0) {
		k250a_dev->k250a_enable_intr(k250a_dev);
	}
	k250a_dev->dev_ref_count = k250a_dev->dev_ref_count + 1;
#endif
	mutex_unlock(&k250a_dev->dev_ref_mutex);

	return 0;
}

int k250a_dev_close(struct inode *inode, struct file *filp)
{
    struct k250a_dev *k250a_dev = container_of(inode->i_cdev,
        struct k250a_dev, c_dev);

    if (!k250a_dev || !filp) {
        pr_err("[%s]l=%d: k250a_dev, filp is NULL\n", __FUNCTION__, __LINE__);
        return -ENODEV;
    }

    pr_err("[%s]l=%d: %d, %d\n", __FUNCTION__, __LINE__, imajor(inode), iminor(inode));
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
    /* unset the flag to restore to previous state */
    if (current->flags & PF_NOFREEZE) {
        current->flags &= ~PF_NOFREEZE;
        pr_err("[%s]l=%d: current->flags=0x%x.\n", __FUNCTION__, __LINE__, current->flags);
    }
#endif
    mutex_lock(&k250a_dev->dev_ref_mutex);

#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
    if (k250a_dev->dev_ref_count == 1) {
        k250a_dev->k250a_disable_intr(k250a_dev);
    }

    if (k250a_dev->dev_ref_count > 0) {
        k250a_dev->dev_ref_count = k250a_dev->dev_ref_count - 1;
    }
#endif
    filp->private_data = NULL;
    mutex_unlock(&k250a_dev->dev_ref_mutex);

    return 0;
}

static void k250a_misc_unregister(struct k250a_dev *k250a_dev, int count)
{
    pr_err("[%s]l=%d: entry\n", __FUNCTION__, __LINE__);

    if (!k250a_dev) {
        pr_err("[%s]l=%d: k250a_dev is NULL\n", __FUNCTION__, __LINE__);
        return;
    }

    kfree(k250a_dev->kbuf);
    device_destroy(k250a_dev->k250a_class, k250a_dev->devno);
    cdev_del(&k250a_dev->c_dev);
    class_destroy(k250a_dev->k250a_class);
    unregister_chrdev_region(k250a_dev->devno, count);
    /*
    if (k250a_dev->ipcl) {
        ipc_log_context_destroy(k250a_dev->ipcl);
    }
    */
}

static int k250a_misc_register(struct k250a_dev *k250a_dev,
		      const struct file_operations *k250a_fops, int count,
		      char *devname, char *classname)
{
    int ret = 0;

    if (!k250a_dev || !k250a_fops || !devname || !classname) {
        pr_err("[%s]l=%d: k250a_dev, k250a_fops, devname, classname is NULL\n",
            __FUNCTION__, __LINE__);
        return -ENODEV;
    }

    ret = alloc_chrdev_region(&k250a_dev->devno, 0, count, devname);
    if (ret < 0) {
        pr_err("[%s]l=%d: failed to alloc chrdev region ret=%d\n",
            __FUNCTION__, __LINE__, ret);
        return ret;
    }
    // sysFS-/sys/class/k250a-i2c_class
    k250a_dev->k250a_class = class_create(THIS_MODULE, classname);
    if (IS_ERR(k250a_dev->k250a_class)) {
        ret = PTR_ERR(k250a_dev->k250a_class);
        pr_err("[%s]l=%d: failed to register device class ret=%d\n",
            __FUNCTION__, __LINE__, ret);
        unregister_chrdev_region(k250a_dev->devno, count);
        return ret;
    }
    cdev_init(&k250a_dev->c_dev, k250a_fops);
    ret = cdev_add(&k250a_dev->c_dev, k250a_dev->devno, count);
    if (ret < 0) {
        pr_err("[%s]l=%d: failed to add cdev ret=%d\n", __FUNCTION__, __LINE__, ret);
        class_destroy(k250a_dev->k250a_class);
        unregister_chrdev_region(k250a_dev->devno, count);
        return ret;
    }
    // devFS-/dev/k250a-i2c
    k250a_dev->k250a_device = device_create(k250a_dev->k250a_class, NULL,
        k250a_dev->devno, k250a_dev, devname);
    if (IS_ERR(k250a_dev->k250a_device)) {
        ret = PTR_ERR(k250a_dev->k250a_device);
        pr_err("[%s]l=%d: failed to create the device ret=%d\n",
            __FUNCTION__, __LINE__, ret);
        cdev_del(&k250a_dev->c_dev);
        class_destroy(k250a_dev->k250a_class);
        unregister_chrdev_region(k250a_dev->devno, count);
        return ret;
    }
    // sysFS-/sys/class/k250a-i2c_class/k250a-i2c/k250a_i2c_node
    ret = sysfs_create_file(&(k250a_dev->k250a_device->kobj),
                                &dev_attr_k250a_i2c_node.attr);
    if (ret) {
        pr_err("[%s]l=%d: fail to create k250a-i2c sys file, ret=%d\n",
            __FUNCTION__, __LINE__, ret);
        device_destroy(k250a_dev->k250a_class, k250a_dev->devno);
        unregister_chrdev_region(k250a_dev->devno, count);
        return ret;
    }
    /*
    k250a_dev->ipcl = ipc_log_context_create(NUM_OF_IPC_LOG_PAGES,
        dev_name(k250a_dev->k250a_device), 0);
    */

    k250a_dev->kbuflen = MAX_BUFFER_SIZE;
    k250a_dev->kbuf = kzalloc(MAX_BUFFER_SIZE, GFP_KERNEL | GFP_DMA);
    if (!k250a_dev->kbuf) {
        k250a_misc_unregister(k250a_dev, count);
        return -ENOMEM;
    }

    return 0;
}

#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
static irqreturn_t i2c_irq_handler(int irq, void *dev_id)
{
	struct k250a_dev *k250a_dev = dev_id;
	struct i2c_dev *i2c_dev = &k250a_dev->i2c_dev;

        if (!k250a_dev || !i2c_dev) {
            pr_err("[%s]l=%d: k250a_dev, i2c_dev is NULL\n", __FUNCTION__, __LINE__);
            return -ENODEV;
        }

        if (device_may_wakeup(&i2c_dev->client->dev)) {
            pm_wakeup_event(&i2c_dev->client->dev, WAKEUP_SRC_TIMEOUT);
        }

	i2c_disable_irq(k250a_dev);
	wake_up(&k250a_dev->read_wq);

	return IRQ_HANDLED;
}
#endif

#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
static int i2c_read(struct k250a_dev *k250a_dev, char *buf, size_t count, int timeout)
{
    int ret = 0;
    struct i2c_dev *i2c_dev = &k250a_dev->i2c_dev;
    uint16_t i = 0;
    // uint16_t disp_len = GET_IPCLOG_MAX_PKT_LEN(count);
    uint16_t disp_len = count;

    pr_err("[%s]l=%d: reading %zu bytes.\n", __FUNCTION__, __LINE__, count);

    if (timeout > NCI_CMD_RSP_TIMEOUT) {
        timeout = NCI_CMD_RSP_TIMEOUT;
    }

    if (count > MAX_BUFFER_SIZE) {
        count = MAX_BUFFER_SIZE;
    }
    if (!k250a_dev || !buf) {
        pr_err("[%s]l=%d: k250a_dev, buf is NULL\n", __FUNCTION__, __LINE__);
        return -ENODEV;
    }

    while (1) {
        ret = 0;
        if (!i2c_dev->irq_enabled) {
            i2c_dev->irq_enabled = true;
            enable_irq(i2c_dev->client->irq);
        }
        if (timeout) {
            ret = wait_event_interruptible_timeout(k250a_dev->read_wq,
                    !i2c_dev->irq_enabled, msecs_to_jiffies(timeout));

            if (ret <= 0) {
                pr_err("[%s]l=%d: timeout/error in read\n", __FUNCTION__, __LINE__);
                goto err;
            }
        } else {
            ret = wait_event_interruptible(k250a_dev->read_wq,
                !i2c_dev->irq_enabled);
            if (ret) {
                pr_err("[%s]l=%d: error wakeup of read wq\n", __FUNCTION__, __LINE__);
                ret = -EINTR;
                goto err;
            }
        }
        i2c_disable_irq(k250a_dev);

        pr_err("[%s]l=%d: spurious interrupt detected\n", __FUNCTION__, __LINE__);
    }

    memset(buf, 0x00, count);
    /* Read data */
    ret = i2c_master_recv(k250a_dev->i2c_dev.client, buf, count);
    // K250ALOG_IPC(k250a_dev, true, "[%s]l=%d of %d bytes, ret=%d", __FUNCTION__, __LINE__, count, ret);
    pr_info("[%s]l=%d: %d bytes, ret=%d\n", __FUNCTION__, __LINE__, count, ret);

    if (ret <= 0) {
        pr_err("[%s]l=%d: ret=%d\n", __FUNCTION__, __LINE__, ret);
        goto err;
    }

    for (i = 0; i < disp_len; i++) {
        // K250ALOG_IPC(k250a_dev, true, " %02x", buf[i]);
        pr_info(" %02x\n", buf[i]);
    }

err:
    return ret;
}
#endif

#if defined(READ_i2c_SLAVE_FROM_KERNEL)
static int i2c_read_k250a(struct k250a_dev *k250a_dev, u8 *write_buf, u32 write_buf_len,
                        u8 *read_buf, u32 read_buf_len, int max_retry_cnt)
{
    int ret = 0;
    int i = 0;
    struct i2c_msg msg_list[2];
    struct i2c_msg *msg = NULL;
    int msg_num = 0;

    /* must have data when read */
    if (!k250a_dev || !k250a_dev->i2c_dev.client || !read_buf || !read_buf_len
        || (read_buf_len >= MAX_BUFFER_SIZE) || (write_buf_len >= MAX_BUFFER_SIZE)) {
        pr_err("[%s]l=%d: k250a_dev, read_buf is NULL, write_buf_len=%d, read_buf_len=%d\n",
            __FUNCTION__, __LINE__, write_buf_len, read_buf_len);
        return -EINVAL;
    }

    memset(&msg_list[0], 0, sizeof(struct i2c_msg));
    memset(&msg_list[1], 0, sizeof(struct i2c_msg));
    // memset(k250a_dev->read_kbuf, 0, MAX_BUFFER_SIZE);  // clean read_kbuf
    // memset(read_kbuf_k250a, 0, MAX_BUFFER_SIZE);
    // memset(k250a_dev->write_kbuf, 0, MAX_BUFFER_SIZE);  // clean write_kbuf
    // memset(write_kbuf_k250a, 0, MAX_BUFFER_SIZE);
    // memcpy(write_kbuf_k250a, write_buf, write_buf_len);
    msg_list[0].addr = k250a_dev->i2c_dev.client->addr;
    msg_list[0].flags = 0;
    msg_list[0].len = write_buf_len;
    msg_list[0].buf = write_kbuf_k250a;
    msg_list[1].addr = k250a_dev->i2c_dev.client->addr;
    msg_list[1].flags = I2C_M_RD;
    msg_list[1].len = read_buf_len;
    msg_list[1].buf = read_kbuf_k250a;
    if (write_buf && write_buf_len) {
        msg = &msg_list[0];
        msg_num = 2;
    } else {
        msg = &msg_list[1];
        msg_num = 1;
    }

    pr_info("[%s]l=%d: i2c addr=0x%x\n",
        __FUNCTION__, __LINE__, k250a_dev->i2c_dev.client->addr);

    for (i = 0; i < max_retry_cnt; i++) {
        ret = i2c_transfer(k250a_dev->i2c_dev.client->adapter, msg, msg_num);
        if (ret < 0) {
            pr_err("[%s]l=%d: i2c_transfer(read) fail, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
            usleep_range(WRITE_RETRY_WAIT_TIME_USEC,
                    WRITE_RETRY_WAIT_TIME_USEC + 100);
        } else {
            // memcpy(read_buf, read_kbuf_k250a, read_buf_len);
            pr_err("[%s]l=%d: i2c_transfer(read) success, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
            break;
        }
    }

    return ret;
}

static int i2c_write_k250a(struct k250a_dev *k250a_dev, u8 *write_buf, u32 count,
                            int max_retry_cnt)
{
    int ret = 0;
    int i = 0;
    struct i2c_msg msgs;

    if (!k250a_dev || !k250a_dev->i2c_dev.client || !write_buf || !count
        || (count >= MAX_BUFFER_SIZE)) {
        pr_err("[%s]l=%d: k250a_dev, write_buf is NULL, count=%d\n",
            __FUNCTION__, __LINE__, count);
        return -EINVAL;
    }

    memset(&msgs, 0, sizeof(struct i2c_msg));
    // memset(k250a_dev->write_kbuf, 0, MAX_BUFFER_SIZE);  // clean write_kbuf
    // memset(write_kbuf_k250a, 0, MAX_BUFFER_SIZE);
    // memcpy(k250a_dev->write_kbuf, write_buf, count);
    // memcpy(write_kbuf_k250a, write_buf, count);

    pr_info("[%s]l=%d: writing %zu bytes.\n", __FUNCTION__, __LINE__, count);

    for (i = 0; i < count; i++) {
        // K250ALOG_IPC(k250a_dev, true, " 0x%02x", write_kbuf_k250a[i]);
        pr_info(" 0x%02x\n", write_kbuf_k250a[i]);
    }

    msgs.addr = k250a_dev->i2c_dev.client->addr;
    msgs.flags = 0;
    msgs.len = count;
    msgs.buf = write_kbuf_k250a;

    pr_info("[%s]l=%d: i2c addr=0x%x\n",
        __FUNCTION__, __LINE__, k250a_dev->i2c_dev.client->addr);

    for (i = 0; i < max_retry_cnt; i++) {
        ret = i2c_transfer(k250a_dev->i2c_dev.client->adapter, &msgs, 1);
        if (ret < 0) {
            pr_err("[%s]l=%d: i2c_transfer(write) fail, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
            usleep_range(WRITE_RETRY_WAIT_TIME_USEC,
                        WRITE_RETRY_WAIT_TIME_USEC + 100);
        } else {
            break;
        }
    }
    return ret;
}
#endif

#if !defined(READ_i2c_SLAVE_FROM_KERNEL)
static int i2c_write(struct k250a_dev *k250a_dev, const char *buf, size_t count,
	      int max_retry_cnt)
{
    int ret = -EINVAL;
    int retry_cnt = 0;
    uint16_t i = 0;
    // uint16_t disp_len = GET_IPCLOG_MAX_PKT_LEN(count);
    uint16_t disp_len = count;

    if (count > MAX_BUFFER_SIZE) {
        count = MAX_BUFFER_SIZE;
    }

    if (!k250a_dev || !buf) {
        pr_err("[%s]l=%d: k250a_dev, buf is NULL\n", __FUNCTION__, __LINE__);
        return -ENODEV;
    }

    pr_info("[%s]l=%d: writing %zu bytes.\n", __FUNCTION__, __LINE__, count);
    // debug
    pr_info("[%s]l=%d: i2c addr=0x%x\n", __FUNCTION__, __LINE__, k250a_dev->i2c_dev.client->addr);
    // debug
    // K250ALOG_IPC(k250a_dev, true, "[%s]l=%d sending %d B", __FUNCTION__, __LINE__, count);
    pr_info("[%s]l=%d sending %d B\n", __FUNCTION__, __LINE__, count);

    for (i = 0; i < disp_len; i++) {
        // K250ALOG_IPC(k250a_dev, true, " 0x%02x", buf[i]);
        pr_info(" 0x%02x\n", buf[i]);
    }

    for (retry_cnt = 1; retry_cnt <= max_retry_cnt; retry_cnt++) {
        ret = i2c_master_send(k250a_dev->i2c_dev.client, buf, count);
        // K250ALOG_IPC(k250a_dev, true, "[%s]l=%d ret=%d", __FUNCTION__, __LINE__, ret);
        pr_info("[%s]l=%d ret=%d\n", __FUNCTION__, __LINE__, ret);
        if (ret <= 0) {
            pr_err("[%s]l=%d: write failed ret=%d, Maybe in Standby Mode - Retry(%d)\n",
                __FUNCTION__, __LINE__, ret, retry_cnt);
            usleep_range(WRITE_RETRY_WAIT_TIME_USEC,
                    WRITE_RETRY_WAIT_TIME_USEC + 100);
        } else if (ret != count) {
            pr_err("[%s]l=%d: failed to write ret=%d\n", __FUNCTION__, __LINE__, ret);
            ret = -EIO;
        } else if (ret == count)
            break;
    }
    return ret;
}
#endif

ssize_t k250a_i2c_dev_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *offset)
{
    int ret = 0;
#if defined(READ_i2c_SLAVE_FROM_KERNEL)
    int i = 0;
#else
#if !defined(READ_i2c_SLAVE_BY_INTERRUPT)
    int i = 0;
#endif  // READ_i2c_SLAVE_BY_INTERRUPT
#endif  // READ_i2c_SLAVE_FROM_KERNEL
    struct k250a_dev *k250a_dev = (struct k250a_dev *)filp->private_data;

    if (!filp || !buf || !offset) {
        pr_err("[%s]l=%d: filp, buf, offset is NULL\n", __FUNCTION__, __LINE__);
        return -ENODEV;
    }

    mutex_lock(&k250a_dev->read_mutex);
    // memset(k250a_dev->read_kbuf, 0, MAX_BUFFER_SIZE);  // clean read_kbuf
    memset(read_kbuf_k250a, 0, MAX_BUFFER_SIZE);
#if defined(READ_i2c_SLAVE_FROM_KERNEL)
    // ret = i2c_read_k250a(k250a_dev, NULL, count, k250a_dev->read_kbuf, count, NO_RETRY);
    ret = i2c_read_k250a(g_k250a_dev, NULL, count, read_kbuf_k250a, count, NO_RETRY);
#else
    // if (filp->f_flags & O_NONBLOCK) {
    // ret = i2c_master_recv(k250a_dev->i2c_dev.client, k250a_dev->read_kbuf, count);
    // debug
    pr_info("[%s]l=%d: i2c addr=0x%x\n", __FUNCTION__, __LINE__, k250a_dev->i2c_dev.client->addr);
    // debug
    ret = i2c_master_recv(k250a_dev->i2c_dev.client, read_kbuf_k250a, count);
    pr_info("[%s]l=%d: NONBLOCK read count=%d, ret=%d\n", __FUNCTION__, __LINE__, count, ret);
    // } else {
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
        ret = i2c_read(k250a_dev, k250a_dev->read_kbuf, count, 0);
#else
        /*
        ret = i2c_master_recv(k250a_dev->i2c_dev.client, k250a_dev->read_kbuf, count);
        if (ret <= 0) {
            pr_err("[%s]l=%d: ret=%d\n", __FUNCTION__, __LINE__, ret);
        }
        */
#endif  // READ_i2c_SLAVE_BY_INTERRUPT
    // }
#endif  // READ_i2c_SLAVE_FROM_KERNEL

    for (i = 0; i < count; i++) {
        // K250ALOG_IPC(k250a_dev, true, " 0x%02x", read_kbuf_k250a[i]);
        pr_info(" 0x%02x\n", read_kbuf_k250a[i]);
    }

    if (ret > 0) {
        // if (copy_to_user(buf, k250a_dev->read_kbuf, ret)) {
        if (copy_to_user(buf, read_kbuf_k250a, ret)) {
            pr_err("[%s]l=%d: failed to copy to user space\n", __FUNCTION__, __LINE__);
            ret = -EFAULT;
        }
    }
    mutex_unlock(&k250a_dev->read_mutex);
    return ret;
}

ssize_t k250a_i2c_dev_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *offset)
{
    int ret = 0;
    struct k250a_dev *k250a_dev = (struct k250a_dev *)filp->private_data;

    if (count > MAX_BUFFER_SIZE) {
        count = MAX_BUFFER_SIZE;
    }

    if (!filp || !buf) {
        pr_err("[%s]l=%d: filp, buf is NULL\n", __FUNCTION__, __LINE__);
        return -ENODEV;
    }

    if (!offset || !k250a_dev) {
        pr_err("[%s]l=%d: offset, k250a_dev is NULL\n", __FUNCTION__, __LINE__);
        return -ENODEV;
    }

    mutex_lock(&k250a_dev->write_mutex);
    // memset(k250a_dev->write_kbuf, 0, MAX_BUFFER_SIZE);  // clean write_kbuf
    memset(write_kbuf_k250a, 0, MAX_BUFFER_SIZE);
    // if (copy_from_user(k250a_dev->write_kbuf, buf, count)) {
    if (copy_from_user(write_kbuf_k250a, buf, count)) {
        pr_err("[%s]l=%d: failed to copy from user space\n", __FUNCTION__, __LINE__);
        mutex_unlock(&k250a_dev->write_mutex);
        return -EFAULT;
    }
#if defined(READ_i2c_SLAVE_FROM_KERNEL)
    // ret = i2c_write_k250a(k250a_dev, k250a_dev->write_kbuf, count, NO_RETRY);
    ret = i2c_write_k250a(g_k250a_dev, write_kbuf_k250a, count, NO_RETRY);
#else
    // ret = i2c_write(k250a_dev, k250a_dev->write_kbuf, count, NO_RETRY);
    ret = i2c_write(k250a_dev, write_kbuf_k250a, count, NO_RETRY);
#endif
    mutex_unlock(&k250a_dev->write_mutex);

    return ret;
}

static const struct file_operations k250a_i2c_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = k250a_i2c_dev_read,
	.write = k250a_i2c_dev_write,
	.open = k250a_dev_open,
	.release = k250a_dev_close,
	.unlocked_ioctl = k250a_dev_ioctl,
};

static int k250a_i2c_dev_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    struct k250a_dev *k250a_dev = NULL;
    struct i2c_dev *i2c_dev = NULL;

    if (!client) {
        pr_err("[%s]l=%d: client is NULL\n", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }

    pr_err("[%s]l=%d: start\n", __FUNCTION__, __LINE__);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("[%s]l=%d: need I2C_FUNC_I2C\n", __FUNCTION__, __LINE__);
        ret = -ENODEV;
        goto err;
    }
    k250a_dev = kzalloc(sizeof(struct k250a_dev), GFP_KERNEL);
    if (k250a_dev == NULL) {
        ret = -ENOMEM;
        goto err;
    }
    k250a_dev->read_kbuf = kzalloc(MAX_BUFFER_SIZE, GFP_DMA | GFP_KERNEL);
    if (!k250a_dev->read_kbuf) {
        ret = -ENOMEM;
        goto err_free_k250a_dev;
    }
    k250a_dev->write_kbuf = kzalloc(MAX_BUFFER_SIZE, GFP_DMA | GFP_KERNEL);
    if (!k250a_dev->write_kbuf) {
        ret = -ENOMEM;
        goto err_free_read_kbuf;
    }
    k250a_dev->i2c_dev.client = client;
    i2c_dev = &k250a_dev->i2c_dev;
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
    k250a_dev->k250a_enable_intr = i2c_enable_irq;
    k250a_dev->k250a_disable_intr = i2c_disable_irq;
#endif

    /* init mutex and queues */
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
    init_waitqueue_head(&k250a_dev->read_wq);
#endif
    mutex_init(&k250a_dev->read_mutex);
    mutex_init(&k250a_dev->write_mutex);
    mutex_init(&k250a_dev->dev_ref_mutex);
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
    spin_lock_init(&i2c_dev->irq_enabled_lock);
#endif
    ret = k250a_misc_register(k250a_dev, &k250a_i2c_dev_fops, DEV_COUNT,
        K250A_CHAR_DEV_NAME, CLASS_NAME);
    if (ret) {
        pr_err("[%s]l=%d: k250a_misc_register failed\n", __FUNCTION__, __LINE__);
        goto err_mutex_destroy;
    }
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
    /* interrupt initializations */
    pr_err("[%s]l=%d: requesting IRQ %d\n", __FUNCTION__, __LINE__, client->irq);

    i2c_dev->irq_enabled = true;
    ret = request_irq(client->irq, i2c_irq_handler,
        IRQF_TRIGGER_HIGH, client->name, k250a_dev);
    if (ret) {
        pr_err("[%s]l=%d: request_irq failed\n", __FUNCTION__, __LINE__);
        goto err_k250a_misc_unregister;
    }
    i2c_disable_irq(k250a_dev);
#endif
#if defined(READ_i2c_SLAVE_FROM_KERNEL)
    g_k250a_dev = k250a_dev;
#endif
    i2c_set_clientdata(client, k250a_dev);
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
    device_init_wakeup(&client->dev, true);
    i2c_dev->irq_wake_up = false;
#endif

    pr_err("[%s]l=%d: success\n", __FUNCTION__, __LINE__);

    return 0;

#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
err_k250a_misc_unregister:
    k250a_misc_unregister(k250a_dev, DEV_COUNT);
#endif
err_mutex_destroy:
    mutex_destroy(&k250a_dev->dev_ref_mutex);
    mutex_destroy(&k250a_dev->read_mutex);
    mutex_destroy(&k250a_dev->write_mutex);

err_free_read_kbuf:
    kfree(k250a_dev->read_kbuf);
err_free_k250a_dev:
    kfree(k250a_dev);
err:
    pr_err("[%s]l=%d: failed\n", __FUNCTION__, __LINE__);

    return ret;
}

static int k250a_i2c_dev_remove(struct i2c_client *client)
{
	int ret = 0;
	struct k250a_dev *k250a_dev = NULL;

    pr_err("[%s]l=%d: remove device\n", __FUNCTION__, __LINE__);
	k250a_dev = i2c_get_clientdata(client);
	if (!k250a_dev) {
		pr_err("[%s]l=%d: device doesn't exist anymore\n", __FUNCTION__, __LINE__);
		return -ENODEV;
	}
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
	if (k250a_dev->dev_ref_count > 0) {
		pr_err("[%s]l=%d: device already in use\n", __FUNCTION__, __LINE__);
		return -EBUSY;
	}
	usleep_range(10000, 10100);

	device_init_wakeup(&client->dev, false);
	free_irq(client->irq, k250a_dev);
#endif
	k250a_misc_unregister(k250a_dev, DEV_COUNT);
	mutex_destroy(&k250a_dev->dev_ref_mutex);
	mutex_destroy(&k250a_dev->read_mutex);
	mutex_destroy(&k250a_dev->write_mutex);

	kfree(k250a_dev->read_kbuf);
	kfree(k250a_dev->write_kbuf);
	kfree(k250a_dev);
	return ret;
}

static int k250a_i2c_dev_suspend(struct device *device)
{
    struct i2c_client *client = to_i2c_client(device);
    struct k250a_dev *k250a_dev = i2c_get_clientdata(client);
    struct i2c_dev *i2c_dev = NULL;

    if (!device || !k250a_dev) {
        pr_err("[%s]l=%d: device doesn't exist anymore\n", __FUNCTION__, __LINE__);
        return -ENODEV;
    }

    i2c_dev = &k250a_dev->i2c_dev;

#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
    // K250ALOG_IPC(k250a_dev, true, "[%s]l=%d: irq_enabled=%d", __FUNCTION__, __LINE__, i2c_dev->irq_enabled);
    pr_info("[%s]l=%d: irq_enabled=%d\n", __FUNCTION__, __LINE__, i2c_dev->irq_enabled);

    if (device_may_wakeup(&client->dev) && i2c_dev->irq_enabled) {
        if (!enable_irq_wake(client->irq)) {
            i2c_dev->irq_wake_up = true;
        }
    }
#endif
    return 0;
}

static int k250a_i2c_dev_resume(struct device *device)
{
    struct i2c_client *client = to_i2c_client(device);
    struct k250a_dev *k250a_dev = i2c_get_clientdata(client);
    struct i2c_dev *i2c_dev = NULL;

    if (!k250a_dev) {
        pr_err("[%s]l=%d: device doesn't exist anymore\n", __FUNCTION__, __LINE__);
        return -ENODEV;
    }

    i2c_dev = &k250a_dev->i2c_dev;
#if defined(READ_i2c_SLAVE_BY_INTERRUPT)
    // K250ALOG_IPC(k250a_dev, true, "[%s]l=%d: irq_wake_up=%d", __FUNCTION__, __LINE__, i2c_dev->irq_wake_up);
    pr_info("[%s]l=%d: irq_wake_up=%d\n", __FUNCTION__, __LINE__, i2c_dev->irq_wake_up);

    if (device_may_wakeup(&client->dev) && i2c_dev->irq_wake_up) {
        if (!disable_irq_wake(client->irq)) {
            i2c_dev->irq_wake_up = false;
        }
    }
#endif
    return 0;
}

static const struct i2c_device_id k250a_i2c_dev_id[] = {
	{K250A_I2C_DEV_ID, 0},
	{}
};

static const struct of_device_id k250a_i2c_dev_match_table[] = {
	{.compatible = K250A_I2C_DRV_STR,},
	{}
};

static const struct dev_pm_ops k250a_i2c_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(k250a_i2c_dev_suspend, k250a_i2c_dev_resume)
};

static struct i2c_driver k250a_i2c_dev_driver = {
	.id_table = k250a_i2c_dev_id,
	.probe = k250a_i2c_dev_probe,
	.remove = k250a_i2c_dev_remove,
	.driver = {
		.name = K250A_I2C_DEV_ID,
		.pm = &k250a_i2c_dev_pm_ops,
		.of_match_table = k250a_i2c_dev_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

MODULE_DEVICE_TABLE(of, k250a_i2c_dev_match_table);

static int __init k250a_i2c_dev_init(void)
{
	int ret = 0;

        pr_err("[%s]l=%d: init\n", __FUNCTION__, __LINE__);
	ret = i2c_add_driver(&k250a_i2c_dev_driver);
        if (ret != 0) {
            pr_err("K250A I2C add driver error ret %d\n", ret);
        }
	return ret;
}

static void __exit k250a_i2c_dev_exit(void)
{
	pr_err("[%s]l=%d: exit\n", __FUNCTION__, __LINE__);
	i2c_del_driver(&k250a_i2c_dev_driver);
}

module_exit(k250a_i2c_dev_exit);
late_initcall(k250a_i2c_dev_init);

MODULE_LICENSE("GPL");