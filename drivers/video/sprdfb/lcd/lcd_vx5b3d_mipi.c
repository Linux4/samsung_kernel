#include <linux/bug.h>
#include <linux/delay.h>
#include "../sprdfb_panel.h"
#include <mach/gpio.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <mach/i2c-sprd.h>
#include <linux/regulator/consumer.h>

#define  LCD_DEBUG
#ifdef LCD_DEBUG
#define LCD_PRINT printk
#else
#define LCD_PRINT(...)
#endif

#define MAX_DATA   100
//#define QUICKLOGIC_DEBUG

struct bridge_data {
	u16 addr;
	u32 data;
}__attribute__ ((packed));

static struct i2c_client *g_client = NULL;
struct regulator *lvds_1v8;
struct regulator *lvds_3v3;
struct panel_spec lcd_vx5b3d_mipi_spec;
struct panel_spec *self = &lcd_vx5b3d_mipi_spec;
extern uint32_t sprdfb_panel_reset_all(void);

#define QUICKLOGIC_MIPI_VENDOR_ID_1		0x5
#define QUICKLOGIC_MIPI_VENDOR_ID_2		0x1
#define QUICKLOGIC_MIPI_COMMAND_CSR_WRITE	0x40
#define QUICKLOGIC_MIPI_COMMAND_CSR_OFFSET	0x41

#define VX5B3D_I2C_RETRY_CNT (5)

#define GPIOID_BLIC_EN (214)
#define GPIOID_BRIDGE_EN  (233)
#define GPIOID_BRIDGE_RST  (234)

#define DELAY_CMD	(0xFFFF)

struct bridge_data vx5b3d_init[] = {
	{0x700, 0x6C900040},
	{0x704, 0x0003040A},
	{0x70C, 0x00004604},
	{0x710, 0x054D000B},
	{0x714, 0x00000020},
	{0x718, 0x00000102},
	{0x71C, 0x00A8002F},
	{0x720, 0x00000000},

	{DELAY_CMD, 100000},	/* For pll locking */
	{0x154, 0x00000000},
	{0x154, 0x80000000},
	{DELAY_CMD, 100000},	/* For pll locking */

	{0x700, 0x6C900840},
	{0x70C, 0x00005E56},
	{0x718, 0x00000202},

	{0x154, 0x00000000},
	{0x154, 0x80000000},
	{DELAY_CMD, 100000},	/* For pll locking */

	{0x37C, 0x00001063},
	{0x380, 0x82A86030},
	{0x384, 0x2861408B},
	{0x388, 0x00130285},
	{0x38C, 0x10630009},
	{0x394, 0x400B82A8},
	{0x600, 0x016CC78C},
	{0x604, 0x3FFFFFE0},
	{0x608, 0x00000D8C},
	{0x154, 0x00000000},
	{0x154, 0x80000000},
	{0x120, 0x00000005},
	{0x124, 0x1892C400},
	{0x128, 0x00102806},
	{0x12C, 0x00000062},
	{0x130, 0x00003C18},
	{0x134, 0x00000015},
	{0x138, 0x00FF8000},
	{0x13C, 0x00000000},

	/*PWM  100 % duty ration*/
	{0x114, 0x000c6302},

	/*backlight duty ratio control when device is first bring up.*/
	{0x160, 0x000007F8},
	{0x164, 0x000002A8},
	{0x138, 0x3fff0000},
	{0x15C, 0x00000005},

	{0x140, 0x00010000},
	/*Add for power consumtion*/
	{0x174, 0x00000000},
	/*end*/

	/*
	   slope = 2 / variance = 0x55550022
	   slope register [15,10]
	   */
	{0x404, 0x55550822},

	/*
	   To minimize the text effect
	   this value from 0xa to 0xf
	   */
	{0x418, 0x555502ff},

	/*
	   Disable brightnes issue Caused by IBC
	   read 4 bytes from address 0x410 to 0x413
	   0x15E50300 is read value for 0x410 register
	   0x5E50300= 0x15E50300 & 0xefffffff
	   */
	{0x410, 0x05E50300},
	/*...end*/
	{0x20C, 0x00000124},
	{0x21C, 0x00000000},
	{0x224, 0x00000007},
	{0x228, 0x00050001},
	{0x22C, 0x0000FF03},
	{0x230, 0x00000001},
	{0x234, 0xCA033E10},
	{0x238, 0x00000060},
	{0x23C, 0x82E86030},
	{0x244, 0x001E0285},
	{0x258, 0x00060062},
	/*vee strenght initialization*/
	{0x400, 0x00000000},
	{0x158, 0x00000000},
	{0x158, 0x00000001},
};

static DEFINE_MUTEX(lock);
#define QUICKLOGIC_MIPI_VENDOR_ID_1		0x5
#define QUICKLOGIC_MIPI_VENDOR_ID_2		0x1
#define QUICKLOGIC_MIPI_COMMAND_CSR_WRITE	0x40
#define QUICKLOGIC_MIPI_COMMAND_CSR_OFFSET	0x41

#define QUICKLOGIC_IIC_READ_CONTROL 0x0E
#define QUICKLOGIC_IIC_WRITE_CONTROL 0x0A
#define QUICKLOGIC_BYTE_I2C_RELEASE (0x0u)
#define QUICKLOGIC_IIC_RELEASE  {\
	QUICKLOGIC_BYTE_I2C_RELEASE, \
}

#define QL_MIPI_PANEL_CMD_SIZE 255
#define QL_VX_LCD_VC 0			/* dcs read/write */
#define CONTROL_BYTE_DCS       (0x08u)

#define DTYPE_DCS_WRITE		0x05	/* short write, 0 parameter */
#define DTYPE_DCS_WRITE1	0x15	/* short write, 1 parameter */
#define DTYPE_DCS_READ		0x06	/* read */
#define DTYPE_DCS_LWRITE	0x39	/* long write *//* generic read/write */
#define DTYPE_GEN_WRITE		0x03	/* short write, 0 parameter */
#define DTYPE_GEN_WRITE1	0x13	/* short write, 1 parameter */
#define DTYPE_GEN_WRITE2	0x23	/* short write, 2 parameter */
#define DTYPE_GEN_LWRITE	0x29	/* long write */
#define DTYPE_GEN_READ		0x04	/* long read, 0 parameter */
#define DTYPE_GEN_READ1		0x14	/* long read, 1 parameter */
#define DTYPE_GEN_READ2		0x24	/* long read, 2 parameter */


#define I2CID_BRIDGE                3
#define I2C_NT50366_ADDR_WRITE      0x9E /* NT50366C 100_1111 0 */
#define I2C_NT51017_ADDR_WRITE      0xD0 /* NT51017 110_1000 0 */
#define I2C_BRIDGE_ADDR_WRITE0      0x58 /* 0101_100 0 ADDR=Low */
#define I2C_BRIDGE_ADDR_WRITE1      0x5A /* 0101_101 0 ADDR=High */
#define I2C_BRIDGE_ADDR             I2C_BRIDGE_ADDR_WRITE0

#define CONTROL_BYTE_GEN       (0x09u)
/**************************************************
 *		QUICKLOGIC MACRO FOR I2C			*
 ***************************************************/
#define GEN_QL_CSR_OFFSET_LENGTH  {\
	CONTROL_BYTE_GEN, \
	0x29,  /* Data ID */\
	0x05,  /* Vendor Id 1 */\
	0x01,  /* Vendor Id 2 */\
	0x41,  /* Vendor Unique Command */\
	0x00,  /* Address LS */\
	0x00,  /* Address MS */\
	0x00,  /* Length LS */\
	0x00,  /* Length MS */\
}

#define GEN_QL_CSR_WRITE  {\
	CONTROL_BYTE_GEN, \
	0x29,  /* Data ID */\
	0x05,  /* Vendor Id 1 */\
	0x01,  /* Vendor Id 2 */\
	0x40,  /* Vendor Unique Command */\
	0x00,  /* Address LS */\
	0x00,  /* Address MS */\
	0x00,  /* data LS */\
	0x00, \
	0x00, \
	0x00,  /* data MS */\
}


int Quicklogic_i2c_read32(u16 reg, u32 *pval)
{
	int ret;
	int status;
	u8 address[3], data[4];

	if (g_client == NULL)	/* No global client pointer? */
		return -EIO;

	mutex_lock(&lock);
	memset(data, 0, 4);
	address[0] = (reg >> 8) & 0xff;
	address[1] = reg & 0xff;
	address[2] = 0xE;

	ret = i2c_master_send(g_client, address, 3);
	if (ret < 0) {
		status = -EIO;
	}
	ret = i2c_master_recv(g_client, data, 4);
	if (ret >= 0) {
		status = 0;
		*pval = data[0] | (data[1] << 8) | (data[2] << 16)
			| (data[3] << 24);
	} else
		status = -EIO;

	mutex_unlock(&lock);

	return status;
}
EXPORT_SYMBOL(Quicklogic_i2c_read32);

int Quicklogic_i2c_read(u32 addr, u32 *val, u32 data_size)
{
	u32 data;
	char buf[] = GEN_QL_CSR_OFFSET_LENGTH;
	char rx[10];
	int ret = -1;
	int write_size;

	if (g_client == NULL)	/* No global client pointer? */
		return -EIO;

	mutex_lock(&lock);

	buf[5] = addr & 0xff;
	buf[6] = (addr >> 8) & 0xff;
	buf[7] = data_size & 0xff;
	buf[8] = (data_size >> 8) & 0xff;

	write_size = 9;

	if ((ret = i2c_master_send( g_client,
					(char*)(&buf[0]),
					write_size )) != write_size) {
		printk(KERN_ERR
				"%s: i2c_master_send failed (%d)!\n", __func__, ret);
		mutex_unlock(&lock);

		return -EIO;
	}

	/*generic read request 0x24 to send generic read command */
	write_size = 4;

	buf[0] = CONTROL_BYTE_GEN;
	buf[1] = 0x24;  /* Data ID */
	buf[2] = 0x05;  /* Vendor Id 1 */
	buf[3] = 0x01;  /* Vendor Id 2 */

	if ((ret = i2c_master_send(g_client,
					(char*)(&buf[0]),
					write_size )) != write_size) {

		pr_info("i2c_master_send failed (%d)!\n", ret);
		mutex_unlock(&lock);
		return -EIO;
	}

	/*return number of bytes or error*/
	if ((ret = i2c_master_recv(g_client,
					(char*)(&rx[0]),
					data_size )) != data_size) {

		pr_info("i2c_master_recv failed (%d)!\n", ret);
		mutex_unlock(&lock);
		return -EIO;
	}

	data = rx[0];
	if (data_size > 1)
		data |= (rx[1] << 8);
	if (data_size > 2)
		data |= (rx[2] << 16) | (rx[3] << 24);

	*val = data;

	mutex_unlock(&lock);

	pr_info("QX_read value0x%x=0x%x\n",addr,data);

	return 0;

}
EXPORT_SYMBOL(Quicklogic_i2c_read);

int Quicklogic_i2c_write32(u16 reg, u32 val)
{
	int status = 0;
	/*int write_size;*/
	char buf[] = GEN_QL_CSR_WRITE;

	if (g_client == NULL)	/* No global client pointer? */
		return -EIO;

	mutex_lock(&lock);

	buf[5] = (uint8_t)reg;  /* Address LS */
	buf[6] = (uint8_t)(reg >> 8);	/* Address MS */

	buf[7] = val & 0xff;
	buf[8] = (val >> 8) & 0xff;
	buf[9] = (val >> 16) & 0xff;
	buf[10] =(val >> 24) & 0xff;
	status = i2c_master_send(g_client, (char *)(&buf[0]), 11);

	if (status >= 0)
		status = 0;
	else {
		pr_info("%s, [0x%04x] -EIO\n", __func__, reg);
		status = -EIO;
	}

	udelay(10);
	mutex_unlock(&lock);

	return status;

}
EXPORT_SYMBOL(Quicklogic_i2c_write32);

int Quicklogic_i2c_release(void)
{
	int write_size;
	int ret = 0;
	char buf[] = QUICKLOGIC_IIC_RELEASE;

	if (g_client == NULL)	/* No global client pointer? */
		return -1;

	mutex_lock(&lock);

	write_size = 1;

	if ((ret = i2c_master_send( g_client,
					(char*)(&buf[0]),
					write_size )) != write_size)
		pr_info("i2c_master_send failed (%d)!\n", ret);

	mutex_unlock(&lock);

	return ret;
}
EXPORT_SYMBOL(Quicklogic_i2c_release);

int Quicklogic_i2cTomipi_write(int dtype, int vit_com, u8 data_size, u8 *ptr )
{
	char buf[QL_MIPI_PANEL_CMD_SIZE];
	int write_size;
	int i;
	int ret = -1;
	int dtype_int;


	if (g_client == NULL)	/* No global client pointer? */
		return -1;

	if (data_size > (QL_MIPI_PANEL_CMD_SIZE - 10)) {
		printk("data size (%d) too big, adjust the QL_MIPI_PANEL_CMD_SIZE!\n", data_size);
		return ret;
	}
	dtype_int = dtype;

	mutex_lock(&lock);

	switch (dtype) {
	case DTYPE_GEN_WRITE:
	case DTYPE_GEN_LWRITE:
	case DTYPE_GEN_WRITE1:
	case DTYPE_GEN_WRITE2:
		buf[0] = CONTROL_BYTE_GEN;
		dtype_int = (data_size == 1) ? DTYPE_GEN_WRITE1 :
			(data_size == 2) ? DTYPE_GEN_WRITE2 : DTYPE_GEN_LWRITE;
		break;
	case DTYPE_DCS_WRITE:
	case DTYPE_DCS_LWRITE:
	case DTYPE_DCS_WRITE1:
		buf[0] =  CONTROL_BYTE_DCS;
		dtype_int = (data_size == 1) ? DTYPE_DCS_WRITE :
			(data_size == 2) ? DTYPE_DCS_WRITE1 : DTYPE_DCS_LWRITE;
		break;
	default:
		printk("Error unknown data type (0x%x).\n", dtype);
		break;
	}

	buf[1] = ((vit_com & 0x3) << 6) | dtype_int ;  /*vc,  Data ID *//* Copy data */

	/* Copy data */
	for(i = 0; i < data_size; i++)
		buf[2+i] = *ptr++;

	write_size = data_size + 2;

	if ((ret = i2c_master_send(g_client,(char*)(&buf[0]),write_size )) != write_size) {
		printk("Quicklogic_i2c_write32 failed : %d\n", ret);

		mutex_unlock(&lock);

		return -EIO;
	}

	mutex_unlock(&lock);

	return 0;
}
EXPORT_SYMBOL(Quicklogic_i2cTomipi_write);

int Quicklogic_i2cTomipi_read(int dtype, int vit_com, u8 data_size, u8 reg , u8 *pval)
{
	char buf[QL_MIPI_PANEL_CMD_SIZE];
	char rx[10];
	int write_size; int ret = -1; int dtype_int;

	if (data_size > (QL_MIPI_PANEL_CMD_SIZE-10)) {
		printk("data size (%d) too big, adjust the QL_MIPI_PANEL_CMD_SIZE!\n", data_size);
		return ret;
	}

	dtype_int = dtype;

	mutex_lock(&lock);

	switch (dtype) {

	case DTYPE_GEN_READ1:  /*0x14*/
	case DTYPE_GEN_READ2:  /*0x24*/
		buf[0] = CONTROL_BYTE_GEN;
		break;

	case DTYPE_DCS_READ:  /*0x6*/
		buf[0] =  CONTROL_BYTE_DCS;
		break;

	default:
		printk("Error unknown data type (0x%x).\n", dtype);
		break;
	}

	buf[1] = ((vit_com & 0x3) << 6) | dtype_int ;  /*vc,  Data ID */

	switch (dtype) {

	case DTYPE_GEN_READ2: /* short read, 2 parameter */

		write_size = 4;
		buf[2] = reg;
		buf[3] = 0;
		break;

	default:
		buf[2] = reg;
		break;
	}

	write_size = 3;

	if ((ret = i2c_master_send(g_client,(char*)(&buf[0]),
					write_size )) != write_size) {

		printk("%s: i2c_master_send failed (%d)!\n", __func__, ret);

		mutex_unlock(&lock);
		return -EIO;
	}

	if ((ret = i2c_master_recv(g_client, (char*)(&rx[0]),
					data_size )) != data_size) {

		printk("%s: i2c_master_recv failed (%d)!\n", __func__, ret);

		mutex_unlock(&lock);
		return -EIO;
	}

	if (ret < 0)
		printk("Quicklogic_i2c_read32 failed : %d\n", ret);

	*pval  = rx[0];

	if (data_size > 1)
		*pval  |= (rx[1] << 8);
	if (data_size > 2)
		*pval  |= (rx[2] << 16) | (rx[3] << 24);

	printk("QuickBridge reg=[0x%x]..return=[0x%x]\n",reg,*pval);

	mutex_unlock(&lock);

	return 0;
}
EXPORT_SYMBOL(Quicklogic_i2cTomipi_read);


int vx5b3d_mipi_write32(struct panel_spec *self, u16 addr, u32 data)
{
	int i;
	static u8 packet[] = {
		0x09, 0x00,		/* data length */
		QUICKLOGIC_MIPI_VENDOR_ID_1,
		QUICKLOGIC_MIPI_VENDOR_ID_2,
		QUICKLOGIC_MIPI_COMMAND_CSR_WRITE,
		0x0, 0x0,		/* address 16bits */
		0x0, 0x0, 0x0, 0x0	/* data max 32bits */
	};
	mipi_dcs_write_t mipi_gen_write=
		self->info.mipi->ops->mipi_gen_write;

	for (i = 0; i < 2; i++)
		packet[5 + i] = (u8)(addr >> (8 * i) & 0xff);

	for (i = 0; i < 4; i++)
		packet[7 + i] = (u8)((data >> (8 * i)) & 0xff);

#ifdef VX5B3D_DEBUG
	   for (i = 0; i < ARRAY_SIZE(packet); i++)
		   printf("packet[%d] = 0x%02x\n", i, packet[i]);
#endif

	mipi_gen_write(packet, ARRAY_SIZE(packet));

	udelay(1);

	return 0;
}

int vx5b3d_mipi_write32_array(struct panel_spec *self,
		struct bridge_data *b_data, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (b_data[i].addr == DELAY_CMD) {
			udelay(b_data[i].data);
			continue;
		}
		vx5b3d_mipi_write32(self, b_data[i].addr, b_data[i].data);
	}

	return 0;
}

int vx5b3d_i2c_write32(u16 addr, u32 data)
{
	int ret, i;
	int retry_cnt = VX5B3D_I2C_RETRY_CNT;

	do {
		ret = Quicklogic_i2c_write32(addr, data);
	} while (--retry_cnt && ret);

	if (!retry_cnt)
		pr_err("%s, [%d, addr:0x%04x, data:0x%08x] failed!!\n",
				__func__, i, addr, data);

	return ret;
}

int vx5b3d_i2c_write32_array(struct bridge_data *b_data, int len)
{
	int i, ret;

	for (i = 0; i < len; i++) {
		if (b_data[i].addr == DELAY_CMD) {
			udelay(b_data[i].data);
			continue;
		}
		ret = vx5b3d_i2c_write32(b_data[i].addr, b_data[i].data);
	}

	return ret;
}

static u32 vx5b3d_i2c_read32(u16 addr)
{
	u32 buf;
	Quicklogic_i2c_read(addr, &buf, 4);

	return buf;
}

static u32 vx5b3d_compare(struct bridge_data *b_data, int len)
{
	int i;
	u32 rd_data;

	for (i = 0; i < len; i++) {
		if (b_data[i].addr == DELAY_CMD) {
			continue;
		}
		rd_data = vx5b3d_i2c_read32(b_data[i].addr);
		if (rd_data != b_data[i].data)
			pr_err("[0x%04x] rd_data:0x%x, data:0x%x - not match!!\n",
					b_data[i].addr, rd_data, b_data[i].data);
	}
}


static int32_t vx5b3d_power(int on)
{
	static int probe = 0;

	if (!probe) {
		if (gpio_request(GPIOID_BRIDGE_EN, "BRIDGE_EN")) {
			printk("[%s] GPIO%d request failed!\n", "BRIDGE_EN", GPIOID_BRIDGE_EN);
			return 0;
		}

		if (gpio_request(GPIOID_BRIDGE_RST, "BRIDGE_RST")){
			printk("[%s] GPIO%d request failed!\n", "BRIDGE_RST", GPIOID_BRIDGE_RST);
			return 0;
		}

		if (gpio_request(GPIOID_BLIC_EN, "BLIC_EN")) {
			printk("[%s] GPIO%d request failed!\n", "BLIC_EN", GPIOID_BLIC_EN);
			return 0;
		}

		lvds_1v8 = regulator_get(NULL,"vddsim1");
		if (IS_ERR(lvds_1v8)) {
			lvds_1v8 = NULL;
			pr_info("%s regulator 1v8 get failed!!\n", "vddsim1");
		} else {
			regulator_set_voltage(lvds_1v8, 1800000, 1800000);
			regulator_enable(lvds_1v8);
		}

		lvds_3v3 = regulator_get(NULL,"vddwifipa");
		if (IS_ERR(lvds_3v3)) {
			lvds_3v3 = NULL;
			pr_info("%s regulator 3v3 get failed!!\n", "vddwifipa");
		} else {
			regulator_set_voltage(lvds_3v3, 3300000, 3300000);
			regulator_enable(lvds_3v3);
		}
		probe = 1;
	}

	pr_info("%s, %s\n", __func__, on ? "on" : "off");
	if (on) {
		gpio_set_value(GPIOID_BRIDGE_EN, 1);
		regulator_enable(lvds_1v8);
		regulator_enable(lvds_3v3);
		udelay(20);
		gpio_set_value(GPIOID_BRIDGE_RST, 1);
		udelay(20);
		gpio_set_value(GPIOID_BLIC_EN, 1);
	} else {
		gpio_set_value(GPIOID_BRIDGE_RST, 0);
		udelay(20);
		regulator_disable(lvds_3v3);
		regulator_disable(lvds_1v8);
		gpio_set_value(GPIOID_BRIDGE_EN, 0);
		gpio_set_value(GPIOID_BLIC_EN, 0);
		msleep(350);
	}
}

static int32_t vx5b3d_mipi_init(struct panel_spec *self)
{
	uint32_t handle;
	int ret;

	mipi_set_lp_mode_t mipi_set_lp_mode =
		self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_video_mode_t mipi_set_video_mode =
		self->info.mipi->ops->mipi_set_video_mode;

	if (!g_client)
		return 0;

	pr_info("%s +\n", __func__);

	/* step 1 : All DSI into LP11*/

	mipi_set_lp_mode();
	vx5b3d_power(1);

	/* step 3 : Delay*/
	mdelay(10);
#ifdef  QUICKLOGIC_DEBUG
	pr_info("[QUICKLOGIC] : VX5B3D INIT - START\n");
	pr_info("[QUICKLOGIC] : VX5B3D INIT - Waiting...(10sec)\n");
	mdelay(10 * 1000);

	pr_info("[QUICKLOGIC] : START VX5B3D INIT - Waiting Done!!\n");
#else
	//vx5b3d_mipi_write32_array(self, vx5b3d_init, ARRAY_SIZE(vx5b3d_init));
	vx5b3d_i2c_write32_array(vx5b3d_init, ARRAY_SIZE(vx5b3d_init));
#endif
	/* step 5 : Start the DSI video stream */
	mipi_set_video_mode();
	mdelay(5);

	pr_info("[QUICKLOGIC] : VX5B3D INIT - DONE\n");
	//vx5b3d_compare(vx5b3d_init, ARRAY_SIZE(vx5b3d_init));

	return 0;
}

static uint32_t vx5b3d_readid(struct panel_spec *self)
{
	int32_t j =0;
	uint8_t read_data[4] = {0};
	int32_t read_rtn = 0;
	uint8_t param[2] = {0};
	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	pr_info("%s\n", __func__);

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(j = 0; j < 4; j++){
		param[0] = 0x01;
		param[1] = 0x00;
		mipi_force_write(0x04, param, 2);
		read_rtn = mipi_force_read(0xda,1,&read_data[0]);
		LCD_PRINT("lcd_vx5b3d_mipi read id 0xda value is 0x%x!\n",read_data[0]);
		read_rtn = mipi_force_read(0xdb,1,&read_data[1]);
		LCD_PRINT("lcd_vx5b3d_mipi read id 0xdb value is 0x%x!\n",read_data[1]);
		read_rtn = mipi_force_read(0xdc,1,&read_data[2]);
		LCD_PRINT("lcd_vx5b3d_mipi read id 0xdc value is 0x%x!\n",read_data[2]);

		//	if((0x55 == read_data[0])&&(0xbc == read_data[1])&&(0x90 == read_data[2])){
		printk("lcd_vx5b3d_mipi read id success!\n");
		return 0x5B3D;
		//		}
	}

	mipi_eotp_set(0,0);

	return 0;
}


static int32_t vx5b3d_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	pr_info("%s\n", __func__);

	vx5b3d_power(0);

	return 0;
}

static struct panel_operations lcd_vx5b3d_mipi_operations = {
	.panel_init = vx5b3d_mipi_init,
	.panel_readid = vx5b3d_readid,
	.panel_enter_sleep = vx5b3d_enter_sleep,
};

static struct timing_rgb lcd_vx5b3d_mipi_timing = {
	.hfp = 123,
	.hbp = 100,
	.hsync = 120,
	.vfp = 13,
	.vbp = 10,
	.vsync = 10,
};


static struct info_mipi lcd_vx5b3d_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 4,
	.phy_feq = 312*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_vx5b3d_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_vx5b3d_mipi_spec = {
	.width = 1024,
	.height = 600,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.suspend_mode = SEND_SLEEP_CMD,
	.info = {
		.mipi = &lcd_vx5b3d_mipi_info
	},
	.ops = &lcd_vx5b3d_mipi_operations,
};
struct panel_cfg lcd_vx5b3d_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x5B3D,
	.lcd_name = "lcd_vx5b3d_mipi",
	.panel = &lcd_vx5b3d_mipi_spec,
};

//temp code
static int __init lcd_vx5b3d_mipi_init(void)
{
	pr_info("%s\n", __func__);

	return sprdfb_panel_register(&lcd_vx5b3d_mipi);
}
subsys_initcall(lcd_vx5b3d_mipi_init);

static int __init quicklogic_probe(struct i2c_client *client,
		const struct i2c_device_id * id)
{
	g_client = client;
	sprd_i2c_ctl_chg_clk(3, 400000); // up h/w i2c 1 400k
	vx5b3d_power(1);

	return 0;
}
static int __exit quicklogic_remove(struct i2c_client *client)
{
	pr_info("%s\n", __func__);

	return 0;
}


#define QUICKLOGIC_DEV_NAME ("bridge")
static struct i2c_device_id quicklogic_id_table[] = {
	{ QUICKLOGIC_DEV_NAME, 0 },
	{},
};

MODULE_DEVICE_TABLE(i2c, quicklogic_id_table);

#ifdef CONFIG_HAS_EARLYSUSPEND
static const struct dev_pm_ops quicklogic_pm_ops = {
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id quicklogic_of_match[] = {
	{ .compatible = "quicklogic,i2c", },
	{ }
};
#endif

static struct i2c_driver quicklogic_i2c_driver = {
	.id_table	= quicklogic_id_table,
	.probe		= quicklogic_probe,
	.remove		= __exit_p(quicklogic_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= QUICKLOGIC_DEV_NAME,
#ifdef CONFIG_HAS_EARLYSUSPEND
		.pm	= &quicklogic_pm_ops,
#endif

#ifdef CONFIG_OF
		.of_match_table = quicklogic_of_match,
#endif
	},
};

static int __init quicklogic_init(void)
{
	pr_info("%s, add quicklogic i2c\n", __func__);
	return i2c_add_driver(&quicklogic_i2c_driver);
}

static void __exit quicklogic_exit(void)
{
	pr_info("%s, delete quicklogic i2c\n", __func__);
	i2c_del_driver(&quicklogic_i2c_driver);
}

module_init(quicklogic_init);
module_exit(quicklogic_exit);

MODULE_DESCRIPTION("QuickLogic Bridge Driver");
MODULE_LICENSE("GPL");
