#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include "k250.h"
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/of_clk.h>

extern int secchip_enable_clk(struct i2c_adapter *adap);
extern int secchip_disable_clk(struct i2c_adapter *adap);
static int gpio_power;
static volatile uint8_t client_open_flag = 0;
struct i2c_client *i2c_k250;
#define MAX_FILE_DATA_SIZE 256

//read chip start
typedef struct {
    dev_t devt;							/* Device ID */
    struct cdev cdev;					/* cdev */
    struct class *class;				/* class */
    struct device *device;
    int major;							/* Major device id */
    int minor;							/* Minor device id */
    unsigned int device_count;			/* device count */
    u8 i2c_store_data[512];				/* store data(r/w) */
    struct i2c_client *client;			/* i2c client */
} dev_info;

int template_i2c_client_open(struct inode *inode, struct file *file)
{
    int ret = 0;
    dev_info *m_dev_info;
    PRINT_ERR("%s \n", __func__);

    //通过结构体成员变量地址inode->i_cdev 以及 cdev类型，找到结构体dev_info
    m_dev_info = container_of(inode->i_cdev, dev_info, cdev);
    if (IS_ERR(m_dev_info)) {
        ret = PTR_ERR(m_dev_info);
        PRINT_ERR("%s pointer err, err code %d\n", __func__, ret);
        return ret;
    } else {
        PRINT_ERR("%s m_dev_info is not null\n", __func__);
        //将这个结构体变量存入file->private_data中，在read/write时可以使用
        file->private_data = m_dev_info;
    }

    gpio_set_value(gpio_power, 1);
    ret = secchip_enable_clk(i2c_k250->adapter);
    PRINT_ERR("%s secchip_enable_clk ret=%d\n", __func__, ret);

    msleep(20);

    client_open_flag = 1;
    PRINT_ERR("template_i2c_client_open exit\n");
    return ret;
}

int template_i2c_client_release(struct inode *inode, struct file *file)
{
    int ret = 0;
    PRINT_ERR("%s \n", __func__);

    ret = secchip_disable_clk(i2c_k250->adapter);
    gpio_set_value(gpio_power, 0);

    client_open_flag = 0;
    PRINT_ERR("template_i2c_client_close exit gpio_set_value ret=%d\n", ret);
    return ret;
}

ssize_t template_i2c_client_read(struct file *file, char *buf, size_t len, loff_t *offset)
{
    int ret = 0;
    PRINT_ERR("template_i2c_client_read ret=%d\n", ret);
    return ret;
}

ssize_t template_i2c_client_write(struct file *file, const char *buf, size_t len, loff_t *offset)
{
    int ret = 0;
    PRINT_ERR("template_i2c_client_write ret=%d\n", ret);
    return ret;
}

static struct file_operations template_i2c_client_fops =
{
    .owner = THIS_MODULE,
    .open = template_i2c_client_open,
    .release = template_i2c_client_release,
    .read = template_i2c_client_read,
    .write = template_i2c_client_write,
};

static dev_info *create_char_device(struct platform_device *pdev)
{
    int ret = 0;
    dev_info *device_info;

    //开辟内存
    device_info = devm_kzalloc(&pdev->dev, sizeof(dev_info), GFP_KERNEL);
    device_info->device_count = 1;

    //1.没有定义设备号，动态分配设备号
    ret = alloc_chrdev_region(&device_info->devt, 0, device_info->device_count, KV_DRIVER_NAME);
    if (ret) {
        PRINT_INF("%s failed to allocate char device region, error code %d\n", __func__, ret);
        goto alloc_chrdev_fial;
    }

    //获取字符设备主设备，从设备号
    device_info->major = MAJOR(device_info->devt);
    device_info->minor = MINOR(device_info->devt);
 
    //2.初始化字符设备
    cdev_init(&device_info->cdev, &template_i2c_client_fops);

    //3.添加字符设备
    ret = cdev_add(&device_info->cdev, device_info->devt, device_info->device_count);
    if (ret) {
        PRINT_INF("%s cdev_add failed\n", __func__);
        goto cdev_add_fial;
    }

    //创建class
    device_info->class = class_create(THIS_MODULE, KV_DRIVER_NAME);
    if (IS_ERR(device_info->class)) {
        ret = PTR_ERR(device_info->class);
        goto class_create_fail;
    }

    //创建节点
    device_info->device = device_create(device_info->class, &pdev->dev, device_info->devt, NULL, KV_DRIVER_NAME);
    if (IS_ERR(device_info->device)) {
        ret = PTR_ERR(device_info->device);
        goto device_create_fail;
    }

    return device_info;

device_create_fail:
    class_destroy(device_info->class);

class_create_fail:
    cdev_del(&device_info->cdev);

cdev_add_fial:
    unregister_chrdev_region(device_info->devt, device_info->device_count);

alloc_chrdev_fial:

    return NULL;

}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^通用读写 START
//从指定起始寄存器开始，连续写入count个值
static int linux_common_i2c_write(struct i2c_client *client, uint8_t *reg_addr, uint8_t *txbuf, int count)
{
    int ret = -1;
    uint8_t buf[MAX_FILE_DATA_SIZE + 1] = {0};
    struct i2c_msg msg[1];

    unsigned char rxdata[MAX_FILE_DATA_SIZE] = {0};
    memcpy(rxdata, txbuf, MAX_FILE_DATA_SIZE);

    if(count > MAX_FILE_DATA_SIZE) {
        count = MAX_FILE_DATA_SIZE;
        PRINT_ERR("block write over size!!!\n");
    }
    // buf[0] = *reg_addr;
    memcpy(buf, txbuf, count);

    msg[0].addr = client->addr;
    msg[0].flags = 0;//write
    msg[0].len = count;
    msg[0].buf = buf;

    ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
    if (ARRAY_SIZE(msg) != ret) {
        PRINT_ERR("linux_common_i2c_write failed. ret=%d\n", ret);
        ret = -1;
    } else {
        ret = 0;
    }
    return ret;
}

static int linux_common_i2c_read(struct i2c_client *client, uint8_t *reg_addr, uint8_t *rxbuf, int count)
{
    int ret = -1;
    // int ii = 0;
    struct i2c_msg msg[1];

    msg[0].addr = client->addr;
    msg[0].flags = I2C_M_RD;//write
    msg[0].len = count;
    msg[0].buf = reg_addr;

    ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));

    if (ARRAY_SIZE(msg) != ret) {
        PRINT_ERR("linux_common_i2c_read failed. ret=%d\n", ret);
        ret = -1;
        PRINT_ERR("msg[0].addr=0x%04X, msg[0].flags=0x%04X, msg[0].len=0x%04X, msg[0].buf[0]=0x%02X\n",
                  msg[0].addr,
                  msg[0].flags,
                  msg[0].len,
                  msg[0].buf[0]);
    } else {
        ret = count;
    }

    return ret;
}

#define CHIP_INFO_SIZE				(44)
#define KEY_FLAG_INFO_SIZE			(4)
#define PARTITION_INFO_SIZE			(4)
#define IMG_VERSION_SIZE			(4)
#define IMG_DATE_SIZE				(12)
#define VERSION_INFO_SIZE			(IMG_VERSION_SIZE + IMG_DATE_SIZE)
#define ADD_SEQUENCE(x) \
do {\
    (x) = (((x) + 1) % 2); \
} while(0);

#define SELFTEST_CMD_TEST_CHIP_SN	(0x01)
#define	SELFTEST_CMD_TIMER			(0x02)
#define SELFTEST_CMD_PROVISION_AK	(0x04)
#define	SELFTEST_CMD_NVM_CHECKSUM	(0x08)
#define SELFTEST_CMD_NVM_ACCESS		(0x10)
#define	SELFTEST_CMD_RAM_ACCESS		(0x20)
#define SELFTEST_CMD_ENC_CHECK		(0x40)
#define	SELFTEST_CMD_TEST_ALL		(0x7F)

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

char __hex2ascii(char hex)
{
    if (hex < 0x0A) hex += 0x30;
    else hex += 0x37;

    return (hex);
}

void __getHWInfo(uint8_t *chip_info, uint8_t *hw_info)
{
    uint32_t i = 0;

    for (i = 0; i < 0x05; i++) {
        hw_info[i + 2] = __hex2ascii(chip_info[0x04 + i]);
    }
    hw_info[11] = __hex2ascii(chip_info[0x2A]);
}

unsigned char __cal_lrc(unsigned char *data, unsigned char data_size)
{
    int i = 0;
    unsigned char lrc = 0x0;
    for (i=0; i < (data_size - 1); i++)
    {
        lrc ^= data[i];
    }
    return lrc;
}

static int receive_data_with_polling(struct i2c_client *client, unsigned char *data, unsigned int data_size)
{
#define SLAVE_ADDRESS 0x21
#define MAX_POLLING_COUNT 30
    // unsigned char base_arr[] = {0x23};
    int ii = 0;
    unsigned int data_len = 0, rec_len = 0;
    unsigned int poll_cnt = MAX_POLLING_COUNT;

    do {
        // msleep(2 * 1000);
        //wxh read
        // data_len = linux_common_i2c_read(client, data, data, 1);
        data_len = linux_common_i2c_read(client, data, data, 1);
        //data_len = ese_hal_receive(data, 1);
        if (data_len != 1) {
            // i++;
            // rxdata[0] = i;
            PRINT_ERR("mismatch receive size %u, %u 0x%02X", 1, data_len, data[0]);
            poll_cnt--;

            // return -1;
        }
    } while (data[0] == 0x0 && poll_cnt > 0);
        for (ii = 0;ii<5 ;ii++){
        PRINT_ERR("wxh read 1 return value : rxdata[%d] = %02X", ii, data[ii]);
    }
    if (poll_cnt == 0) {
        PRINT_ERR("response timeout");
        return -1;
    }

    if (data[0] != SLAVE_ADDRESS) {
        PRINT_ERR("invalid slave address : 0x%02x", data[0]);
        return -1;
    }
    //wxh read
    data_len = linux_common_i2c_read(client, data + 1, data + 1, 2);
        for (ii = 0;ii<5 ;ii++){
        PRINT_ERR("wxh read 2 return value : rxdata[%d] = %02X", ii, data[ii]);
    }
    rec_len = data[2] + 1;
    //wxh read
    for(ii=0;ii<rec_len;ii++){
    // msleep(50);
    data_len = linux_common_i2c_read(client, data + 3 +ii, data + 3 +ii, 1);
    }
    return data_size;
}

int boot_firmware(struct i2c_client *client)
{
    unsigned char txdata[] = {0x12, 0x00, 0x04, 0x00, 0xfc, 0x00, 0x00, 0xea};
    unsigned char base_arr[] = {0x23};
    unsigned int txdata_size = 8;
    unsigned char rxdata[MAX_FILE_DATA_SIZE] = {0x0, };
    unsigned int rxdata_size = 6;
    int ret_size = 0;
    int ii = 0;
    // msleep(2 * 1000);
    //wxh write
    linux_common_i2c_write(client, base_arr, txdata, txdata_size);
    //ret_size = ese_hal_send(txdata, txdata_size);
    if (ret_size != txdata_size) {
        PRINT_ERR("mismatch send size : %u, %d", txdata_size, ret_size);
    }

    ret_size = receive_data_with_polling(client, rxdata, rxdata_size);
    if (ret_size != rxdata_size) {
        PRINT_ERR("failed to star_bootFirmware (receive data fail)");
        return -1;
    }

    if (rxdata[3] != 0x90 && rxdata[4] != 0x00) {
        PRINT_ERR("failed to star_bootFirmware (FW booting fail)");
        PRINT_ERR("return value : %x%x", rxdata[3], rxdata[4]);
        return -1;
    }
        for (ii = 0;ii<10 ;ii++){
        PRINT_ERR("wxh boot_firmware return value : rxdata[%d] = %02X", ii, rxdata[ii]);
    }
    return 0;
}

int get_info(struct i2c_client *client, unsigned int *seq, unsigned char *info, unsigned int *info_size)
{
    unsigned char txdata[] = {0x12, 0xFF, 0x05, 0x00, 0xb1, 0x3b, 0x00, 0x84, 0xFF};
    unsigned char base_arr[] = {0x23};
    unsigned int txdata_size = 9;
    unsigned char rxdata[MAX_FILE_DATA_SIZE] = {0x0, };
    unsigned int rxdata_size = 0x8A;
    int ret_size = 0;
    int ii = 0;
    txdata[1] = ((*seq) << 6) & 0xFF;
    txdata[sizeof(txdata)-1] = __cal_lrc(txdata, sizeof(txdata));
    PRINT_ERR("wxhgetinfo1");
    // msleep(2 * 1000);
    //wxh write
    linux_common_i2c_write(client, base_arr, txdata, txdata_size);
    //ret_size = ese_hal_send(txdata, txdata_size);
    if (ret_size != txdata_size) {
        PRINT_ERR("mismatch send size : %u, %d", txdata_size, ret_size);
    }
    PRINT_ERR("wxhgetinfo2");
    ret_size = receive_data_with_polling(client, rxdata, rxdata_size);
    if (ret_size != rxdata_size) {
        PRINT_ERR("failed to get_info (receive data fail)");
        return -1;
    }
    PRINT_ERR("wxhgetinfo3");

    if (rxdata[0x87] != 0x90 && rxdata[0x88] != 0x00) {
        PRINT_ERR("failed to get_info (FW get information fail)");
        PRINT_ERR("return value : %x%x", rxdata[0x87], rxdata[0x88]);
        ADD_SEQUENCE(*seq);
        return -1;
    }

    if (memcpy(info, rxdata + 3, rxdata_size - 6) < 0) {
        PRINT_ERR("failed to copy rx data");
        ADD_SEQUENCE(*seq);
        return -1;
    }
    *info_size = rxdata_size - 6;
    ADD_SEQUENCE(*seq);
    for (ii=0;ii<140 ;ii++){
        PRINT_ERR("wxh return value : rxdata[%d] = %02X", ii, rxdata[ii]);
    }
    return 0;
}

int selfTest(struct i2c_client *client, unsigned int *seq, unsigned char test_cmd, unsigned char *selfTestRes, unsigned int *selfTestRes_size)
{
    unsigned char txdata[] = {0x12, 0xFF, 0x04, 0x00, 0xf1, 0xFF, 0x00, 0xFF};	// 0xFF is dynamic data
    unsigned char base_arr[] = {0x23};
    unsigned int txdata_size = 8;
    unsigned char rxdata[MAX_FILE_DATA_SIZE] = {0x0, };
    unsigned int rxdata_size = 0x2E;
    unsigned int wtxResponse_size = 8;
    unsigned int wtxResponse_cnt = 0;
    unsigned int wtxTime = 0;
    int ii = 0;
    int ret_size = 0;
    if (test_cmd > 0x7F || test_cmd < 0x01) {
        PRINT_ERR("Wrong selfTest command");
        return -1;
    }
    if ((*seq) != 0 && (*seq) != 1) {
        PRINT_ERR("Wrong sequnce format");
        return -1;
    }
    txdata[5] = (test_cmd) & 0xFF;
    txdata[1] = ((*seq) << 6) & 0xFF;
    txdata[sizeof(txdata)-1] = __cal_lrc(txdata, sizeof(txdata));

    wtxResponse_cnt += (test_cmd & SELFTEST_CMD_NVM_CHECKSUM) != 0 ? 1 : 0;
    wtxResponse_cnt += (test_cmd & SELFTEST_CMD_NVM_ACCESS) != 0 ? 1 : 0;
    wtxResponse_cnt += (test_cmd & SELFTEST_CMD_RAM_ACCESS) != 0 ? 1 : 0;
    wtxResponse_cnt += (test_cmd & SELFTEST_CMD_ENC_CHECK) != 0 ? 1 : 0;
    // msleep(2 * 1000);
    //wxh write
    linux_common_i2c_write(client, base_arr, txdata, txdata_size);
    //ret_size = ese_hal_send(txdata, txdata_size);
    if (ret_size != txdata_size) {
        PRINT_ERR("mismatch send size : %u, %d", txdata_size, ret_size);
    }
    while(wtxResponse_cnt > 0) {
        ret_size = receive_data_with_polling(client, rxdata, wtxResponse_size);
    if (ret_size != wtxResponse_size) {
            PRINT_ERR("failed to get wtxReqeust data");
            // return -1;
        }
        PRINT_ERR("wxh11 selftest result rxdata[1]=0x%02X\n", rxdata[1]);
        if (rxdata[1] != 0xc3) {
            PRINT_ERR("wxh22 selftest result rxdata[1]=0x%02X\n", rxdata[1]);
            PRINT_ERR("failed to wtxReqeust");
            // return -1;
        }

        wtxTime = (rxdata[5] << 8) | rxdata[6];
        PRINT_ERR("wxh05 wtxTime = %d %d %02x", wtxTime, wtxResponse_cnt, rxdata[1]);
        // msleep(wtxTime * 1000);
        // msleep(5 * 1000);
        wtxResponse_cnt--;
    }
        for(ii = 0; ii < 40; ii++) {
            PRINT_ERR("wxh2 selftest result rxdata[%d]=0x%02X %c\n",ii, rxdata[ii], rxdata[ii]);
        }
        ret_size = receive_data_with_polling(client, rxdata, rxdata_size);
        for(ii = 0; ii < 40; ii++) {
            PRINT_ERR("wxh3 selftest result rxdata[%d]=0x%02X %c\n",ii, rxdata[ii], rxdata[ii]);
        }

    if (ret_size != rxdata_size) {
        PRINT_ERR("failed to selfTest (receive data fail)");
        return -1;
    }
    if (rxdata[0x28] != 0x90 && rxdata[0x29] != 0x00) {
        PRINT_ERR("failed to self_test");
        PRINT_ERR("return value : %x%x", rxdata[0x28], rxdata[0x29]);
        ADD_SEQUENCE(*seq);
        return -1;
    }
    if (memcpy(selfTestRes, rxdata + 3, rxdata_size - 6) < 0) {
        PRINT_ERR("failed to copy rx data");
        return -1;
    }
        *selfTestRes_size = rxdata_size - 6;
        ADD_SEQUENCE(*seq);

        for(ii = 0; ii < 40; ii++) {
            PRINT_ERR("wxh4 selftest result rxdata[%d]=0x%02X %c\n",ii, selfTestRes[ii], selfTestRes[ii]);
        }
        return 0;
}
//read chip end

int k250_suspend(struct device *dev)
{
	if(client_open_flag == 1){
		secchip_disable_clk(i2c_k250->adapter);
		PRINT_ERR("secchip suspend disable clk");
	}
	return 0;
}

int k250_resume(struct device *dev)
{
	if(client_open_flag == 1){
		secchip_enable_clk(i2c_k250->adapter);
		PRINT_ERR("secchip resume enable clk");
	}
	return 0;
}

static const struct dev_pm_ops k250_pm_ops =
{
    .suspend = k250_suspend,
    .resume = k250_resume,
};

static int k250_parse_dt(struct device *dev, int status)
{
    int ret = -1;
    struct device_node *np = dev->of_node;

    gpio_power = of_get_named_gpio(np, "sec_en", 0);
    PRINT_ERR("wxh-1of_get_named_gpio %d\n", gpio_power);
    if(gpio_power < 0) {
        PRINT_ERR("wxh-of_get_named_gpio %d\n", gpio_power);
        return -1;
    }

    ret = gpio_request(gpio_power, "sec_en");
    PRINT_ERR("wxh-1gpio_request %d\n", ret);
    if (ret) {
        PRINT_ERR("wxh-gpio_request %d\n", ret);
        return ret;
    }

    ret = gpio_direction_output(gpio_power,1);
    PRINT_ERR("wxh-1gpio_direction_output %d\n", ret);
    if (ret) {
        PRINT_ERR("wxh-gpio_direction_output %d\n", ret);
        return ret;
    }
    if(status){
        gpio_set_value(gpio_power, 1);
    }else{
        gpio_set_value(gpio_power, 0);
    }
    ret = gpio_get_value(gpio_power);
    PRINT_ERR("wxh-gpio_get_value reset ret = %d\n", ret);
    return 0;
}

static int k250a_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    PRINT_ERR("Entry : %s\n", __func__);
    i2c_k250 = client;
    PRINT_ERR("Exit : %s\n", __func__);
    return 0;
}

static int k250a_remove(struct i2c_client *client)
{
    PRINT_ERR("Entry : %s\n", __func__);

    PRINT_ERR("Exit : %s\n", __func__);
    return 0;
}

static int Knoxvault_probe(struct platform_device *pdev)
{
    int ret = 0;
    dev_info *device_info;
    device_info = create_char_device(pdev);
    PRINT_ERR("create_char_devicen");
    ENTER;
    ret = k250_parse_dt(&pdev->dev, 0);//yasin: power, irq, regs
    PRINT_ERR("k250_parse_dt- 4\n");
    if (ret) {
        PRINT_ERR("k250_parse_dt failed\n");
        ret = -ENODEV;
        goto failed_parse_dt;
    }

    PRINT_INF("wxh-probe success\n");
    return 0;

    PRINT_ERR("wxh- 11\n");
failed_parse_dt:
    PRINT_ERR("wxh-probe failed\n");
    return ret;
}

static int Knoxvault_remove(struct platform_device *pdev)
{
    int ret = 0;
    PRINT_INF("wxh-Knoxvault_remove success %d\n", ret);
    ENTER;
    return 0;
}

static struct of_device_id Knoxvault_of_match_table[] = {
    {.compatible = "sec,Knoxvault"},
    { },
};

static struct platform_driver Knoxvault_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = KV_DRIVER_NAME,
        .of_match_table = Knoxvault_of_match_table,
    },
    .probe = Knoxvault_probe,
    .remove = Knoxvault_remove,
};

static const struct i2c_device_id k250a_id[] = {
	{"k250a", 0},
	{}
};

static const struct of_device_id k250a_match_table[] = {
	{ .compatible = "sec_k250a",},
	{},
};

static struct i2c_driver k250a_driver = {
	.id_table = k250a_id,
	.probe = k250a_probe,
	.remove = k250a_remove,
	.driver = {
		.name = "k250a",
		.owner = THIS_MODULE,
		.of_match_table = k250a_match_table,
		.pm = &k250_pm_ops,
	},
};

static int __init Knoxvault_module_init(void)
{
    ENTER;
    platform_driver_register(&Knoxvault_i2c_driver);
    i2c_add_driver(&k250a_driver);
    PRINT_INF("Knoxvault_i2c_driver\n");
    return 0;
}

static void __exit Knoxvault_module_exit(void)
{
    ENTER;
    i2c_del_driver(&k250a_driver);
    platform_driver_unregister(&Knoxvault_i2c_driver);
}

rootfs_initcall(Knoxvault_module_init);
module_exit(Knoxvault_module_exit);
MODULE_AUTHOR("wuxihua");
MODULE_DESCRIPTION("Driver for k250");
MODULE_ALIAS("k250 driver");
MODULE_LICENSE("GPL");

