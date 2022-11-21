
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/gpio.h>
//#include <mms_ts.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/input/mms136_ts.h>
//#include "ARUBA_G1F_CORE53_R30_V07.c"

#define GPIO_TOUCH_INT  		TSP_INT//27
#define GPIO_TSP_SCL    		TSP_SCL//35
#define GPIO_TSP_SDA      		TSP_SDA//40


#define DIR_OUT(a,b) 	gpio_direction_output(a,b)
#define DIR_IN(a)		gpio_direction_input(a)

enum {
	ISP_MODE_FLASH_ERASE	= 0x59F3,
	ISP_MODE_FLASH_WRITE	= 0x62CD,
	ISP_MODE_FLASH_READ	= 0x6AC9,
};

/* each address addresses 4-byte words */
#define ISP_MAX_FW_SIZE		(0x1F00 * 4)
#define ISP_IC_INFO_ADDR	0x1F00

#define ISP_CAL_INFO_ADDR	7936
#define ISP_CAL_DATA_SIZE	32

struct mms_info {
	struct i2c_client		*client;
	struct mms_ts_platform_data	*pdata;
};

struct mms_info *info;
struct i2c_client *mms_client;

extern void ts_power_control(int en);
unsigned char TSP_PanelVersion, TSP_PhoneVersion;
unsigned char TSP_CorePanelVersion,TSP_CorePhoneVersion,TSP_type;


static void hw_reboot(bool bootloader)
{
	//gpio_direction_output(info->pdata->gpio_vdd_en, 0);
	ts_power_control(0);
	DIR_OUT(GPIO_TSP_SDA, bootloader ? 0 : 1);
	DIR_OUT(GPIO_TSP_SCL, bootloader ? 0 : 1);
	DIR_OUT(GPIO_TOUCH_INT, 0);
	msleep(30);
	//gpio_set_value(info->pdata->gpio_vdd_en, 1);
	ts_power_control(1);
	msleep(30);

	if (bootloader) {
		gpio_set_value(GPIO_TSP_SCL, 0);
		gpio_set_value(GPIO_TSP_SDA, 1);
	} else {
		gpio_set_value(GPIO_TOUCH_INT, 1);
		DIR_IN(GPIO_TOUCH_INT);
		DIR_IN(GPIO_TSP_SCL);
		DIR_IN(GPIO_TSP_SDA);
	}
	msleep(40);
}

static void isp_toggle_clk(int start_lvl, int end_lvl,
			   int hold_us)
{
	gpio_set_value(GPIO_TSP_SCL, start_lvl);
	udelay(hold_us);
	gpio_set_value(GPIO_TSP_SCL, end_lvl);
	udelay(hold_us);
}

/* 1 <= cnt <= 32 bits to write */
static void isp_send_bits(u32 data, int cnt)
{
	DIR_OUT(GPIO_TOUCH_INT, 0);
	DIR_OUT(GPIO_TSP_SCL, 0);
	DIR_OUT(GPIO_TSP_SDA, 0);

	/* clock out the bits, msb first */
	while (cnt--) {
		gpio_set_value(GPIO_TSP_SDA, (data >> cnt) & 1);
		udelay(3);
		isp_toggle_clk(1, 0, 3);
	}
}

/* 1 <= cnt <= 32 bits to read */
static u32 isp_recv_bits(int cnt)
{
	u32 data = 0;

	DIR_OUT(GPIO_TOUCH_INT, 0);
	DIR_OUT(GPIO_TSP_SCL, 0);
	gpio_set_value(GPIO_TSP_SDA, 0);
	DIR_IN(GPIO_TSP_SDA);

	/* clock in the bits, msb first */
	while (cnt--) {
		isp_toggle_clk(0, 1, 1);
		data = (data << 1) | (!!gpio_get_value(GPIO_TSP_SDA));
	}

	DIR_OUT(GPIO_TSP_SDA, 0);
	return data;
}

static void isp_enter_mode(u32 mode)
{
	int cnt;
	unsigned long flags;

	local_irq_save(flags);
	DIR_OUT(GPIO_TOUCH_INT, 0);
	DIR_OUT(GPIO_TSP_SCL, 0);
	DIR_OUT(GPIO_TSP_SDA, 1);

	mode &= 0xffff;
	for (cnt = 15; cnt >= 0; cnt--) {
		gpio_set_value(GPIO_TOUCH_INT, (mode >> cnt) & 1);
		udelay(3);
		isp_toggle_clk(1, 0, 3);
	}

	gpio_set_value(GPIO_TOUCH_INT, 0);
	local_irq_restore(flags);
}

static void isp_exit_mode()
{
	int i;
	unsigned long flags;

	local_irq_save(flags);
	DIR_OUT(GPIO_TOUCH_INT, 0);
	udelay(3);

	for (i = 0; i < 10; i++)
		isp_toggle_clk(1, 0, 3);
	local_irq_restore(flags);
}

static void flash_set_address(u16 addr)
{
	/* Only 13 bits of addr are valid.
	 * The addr is in bits 13:1 of cmd */
	isp_send_bits((u32)(addr & 0x1fff) << 1, 18);
}

static void flash_erase()
{
	isp_enter_mode(ISP_MODE_FLASH_ERASE);

	DIR_OUT(GPIO_TOUCH_INT, 0);
	DIR_OUT(GPIO_TSP_SCL, 0);
	DIR_OUT(GPIO_TSP_SDA, 1);

	/* 4 clock cycles with different timings for the erase to
	 * get processed, clk is already 0 from above */
	udelay(7);
	isp_toggle_clk(1, 0, 3);
	udelay(7);
	isp_toggle_clk(1, 0, 3);
	usleep_range(25000, 35000);
	isp_toggle_clk(1, 0, 3);
	usleep_range(150, 200);
	isp_toggle_clk(1, 0, 3);

	gpio_set_value(GPIO_TSP_SDA, 0);

	isp_exit_mode();
}

static u32 flash_readl(u16 addr)
{
	int i;
	u32 val;
	unsigned long flags;

	local_irq_save(flags);
	isp_enter_mode(ISP_MODE_FLASH_READ);
	flash_set_address(addr);

	DIR_OUT(GPIO_TSP_SCL, 0);
	DIR_OUT(GPIO_TSP_SDA, 0);
	udelay(40);

	/* data load cycle */
	for (i = 0; i < 6; i++)
		isp_toggle_clk(1, 0, 10);

	val = isp_recv_bits(32);
	isp_exit_mode();
	local_irq_restore(flags);

	return val;
}

static void flash_writel(u16 addr, u32 val)
{
	unsigned long flags;

	local_irq_save(flags);
	isp_enter_mode(ISP_MODE_FLASH_WRITE);
	flash_set_address(addr);
	isp_send_bits(val, 32);

	DIR_OUT(GPIO_TSP_SDA, 1);
	/* 6 clock cycles with different timings for the data to get written
	 * into flash */
	isp_toggle_clk(0, 1, 3);
	isp_toggle_clk(0, 1, 3);
	isp_toggle_clk(0, 1, 6);
	isp_toggle_clk(0, 1, 12);
	isp_toggle_clk(0, 1, 3);
	isp_toggle_clk(0, 1, 3);

	isp_toggle_clk(1, 0, 1);

	DIR_OUT(GPIO_TSP_SDA, 0);
	isp_exit_mode();
	local_irq_restore(flags);
	usleep_range(300, 400);
}

static int fw_write_image(const u8 *data, size_t len)
{
//	struct i2c_client *client = client;
	u16 addr = 0;

	for (addr = 0; addr < (len / 4); addr++, data += 4) {
		u32 val = get_unaligned_le32(data);
		u32 verify_val;
		int retries = 3;

		while (retries--) {
			flash_writel(addr, val);
			verify_val = flash_readl(addr);
			if (val == verify_val)
				break;
//			dev_err(&client->dev,
//				"mismatch @ addr 0x%x: 0x%x != 0x%x\n",
//				addr, verify_val, val);
			printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

			hw_reboot(true);
			continue;
		}
		if (retries < 0)
			return -ENXIO;
	}

	return 0;
}

static int fw_download(const u8 *data, size_t len)
{
//	struct i2c_client *client = info->client;
	u32 val;
	int ret = 0;
	int i;
	int addr = ISP_CAL_INFO_ADDR; 
	u32 *buf = kzalloc(ISP_CAL_DATA_SIZE * 4, GFP_KERNEL);
	struct i2c_adapter *adapter = to_i2c_adapter(mms_client->dev.parent);

	if (len % 4) {
//		dev_err(&client->dev,
//			"fw image size (%d) must be a multiple of 4 bytes\n",
//			len);
		printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

		ret = -EINVAL;
		goto out;
	} else if (len > ISP_MAX_FW_SIZE) {
//		dev_err(&client->dev,
//			"fw image is too big, %d > %d\n", len, ISP_MAX_FW_SIZE);
		printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

		ret = -EINVAL;
		goto out;
	}

//	dev_info(&client->dev, "fw download start\n");

	i2c_lock_adapter(adapter);
//	info->pdata->mux_fw_flash(true);

	//gpio_direction_output(info->pdata->gpio_vdd_en, 0);
	ts_power_control(0);
	DIR_OUT(GPIO_TSP_SDA, 0);
	DIR_OUT(GPIO_TSP_SCL, 0);
	DIR_OUT(GPIO_TOUCH_INT, 0);
	ts_power_control(1);
	hw_reboot(true);

//	dev_info(&client->dev, "calibration data backup\n");
	printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

	for (i = 0; i < ISP_CAL_DATA_SIZE; i++) {
		buf[i] = flash_readl(addr);
//		dev_info(&client->dev, "cal data : 0x%02x\n", buf[i]);
	}

	val = flash_readl(ISP_IC_INFO_ADDR);
//	dev_info(&client->dev, "IC info: 0x%02x (%x)\n", val & 0xff, val);
	printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);


//	dev_info(&client->dev, "fw erase...\n");
	flash_erase();

//	dev_info(&client->dev, "fw write...\n");
	/* XXX: what does this do?! */
	printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

	flash_writel(ISP_IC_INFO_ADDR, 0xffffff00 | (val & 0xff));
	usleep_range(1000, 1500);
	ret = fw_write_image(data, len);
	if (ret)
		goto out;
	usleep_range(1000, 1500);

	for (i = 0; i < ISP_CAL_DATA_SIZE; i++) {
		flash_writel(addr, buf[i]);
	}

	hw_reboot(false);
	usleep_range(1000, 1500);
//	dev_info(&client->dev, "fw download done...\n");
	printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

//	dev_info(&client->dev, "restoring data\n");
	ret = 0;

out:
	if (ret != 0) ;
//		dev_err(&client->dev, "fw download failed...\n");

	hw_reboot(false);
	kfree(buf);

//	info->pdata->mux_fw_flash(false);
	i2c_unlock_adapter(adapter);

	return ret;
}

int mms_flash_fw(struct i2c_client *client)
{
	int ret;
	const struct firmware *fw;
//	const char *fw_name = "mms_ts.bin";
	const char *fw_name = "melfas/arubaTD/ARUBA_G1M_CORE53_R20_V09.fw";
//	struct mms_info *info;
	printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

	mms_client = client;
//	info = kzalloc(sizeof(struct mms_info), GFP_KERNEL);

//	info->client = client;
//	info->pdata = pdata;
//	ret += request_firmware(&(fw_mbin[0]),"tsp_melfas/aruba_duos/BOOT.fw", &_client->dev);


	ret = request_firmware(&fw, fw_name, &client->dev); 
	if (ret) {
		//dev_err(&client->dev, "error requesting firmware\n");		
		printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);
		goto out;
	}
	printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

	ret = fw_download(fw->data, fw->size);
	printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

out:
//	kfree(info);
	release_firmware(fw);

	return ret;
}
#ifdef CONFIG_MACH_ARUBA_TD	
int mms_flash_fw2(struct i2c_client *client,const char* fw_name)
{
	int ret;
	const struct firmware *fw;

	printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

	mms_client = client;


#if 1
	ret = request_firmware(&fw, fw_name, &client->dev); 
	if (ret) {
		//dev_err(&client->dev, "error requesting firmware\n");		
		printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);
		goto out;
	}
	printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

	ret = fw_download(fw->data, fw->size);
	printk(KERN_ERR "[TSP] %s ( %d)\n", __func__, __LINE__);

out:
//	kfree(info);
	release_firmware(fw);
#endif 

//	ret = fw_download(MELFAS_binary, 31744);

	return ret;
}
#endif

