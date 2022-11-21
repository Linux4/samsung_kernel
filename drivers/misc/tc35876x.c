/*
 * Tc35876x MIPI to Parallel Bridge Chip
 *
 *
 * Copyright (C) 2006, Marvell Corporation.
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/suspend.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <mach/cputype.h>
#include <mach/mfp-mmp2.h>
#include <mach/gpio.h>
#include <mach/regs-mpmu.h>
#include <mach/tc35876x.h>
#include <mach/pxa988.h>
#include <mach/pxa168fb.h>

#ifdef CONFIG_MACH_LT02
#include <mach/mfp-pxa986-lt02.h>
#elif defined(CONFIG_MACH_COCOA7)
#include <mach/mfp-pxa986-cocoa7.h>
#else
#include <mach/mfp-mmp2.h>
#endif

/* Unique ID allocation */
static struct i2c_client *g_client;

static DEFINE_MUTEX(lock);

#ifdef Vx5B3D_MIPI_MERGE
 /* QL merge code here */
#define QL_I2C_READ_AFTER_WRITE_DEBUG

#define Vx5D3B_MIPI_VENDOR_ID_1 0x5
#define Vx5D3B_MIPI_VENDOR_ID_2 0x1
#define Vx5D3B_MIPI_COMMAND_CSR_WRITE 0x40
#define Vx5D3B_MIPI_COMMAND_CSR_OFFSET 0x41

static char ql_csr_wr_payload[9] = {	Vx5D3B_MIPI_VENDOR_ID_1,
					Vx5D3B_MIPI_VENDOR_ID_2,
					Vx5D3B_MIPI_COMMAND_CSR_WRITE,
					0x0, 0x0,/* address 16bits */
					0x0, 0x0, 0x0, 0x0 };/* data max 32bits */

static struct dsi_cmd_desc ql_generic_csr_wr_cmd[] = {
	{ DSI_DI_DCS_GEN_LWRITE,1,0,
		sizeof(ql_csr_wr_payload), ql_csr_wr_payload }
};

int vx5d3b_mipi_write(struct pxa168fb_info *fbi, u32 address, u32 value, u32 data_size)
{
	u32 regval = 0;
	struct pxa168fb_info *fbi_mipi;

	mutex_lock(&lock);

	/*printk("W[0x%x]=0x%x\n", address, value);*/
	fbi_mipi = fbi_global;

	ql_csr_wr_payload[3] = (u8)(address & 0xff);		/* Address LS */
	ql_csr_wr_payload[4] = (u8)((address >> 8) & 0xff);	/* Address MS */
	ql_csr_wr_payload[5] = (u8) value;			/* value LSB */
	if (data_size > 1)
		ql_csr_wr_payload[6] = (u8)((value >> 8) & 0xff);

	if (data_size > 2) {
		ql_csr_wr_payload[7] = (u8)((value >> 16) & 0xff);
		ql_csr_wr_payload[8] = (u8)((value >> 24) & 0xff);
	}

	dsi_cmd_array_tx(fbi_mipi,ql_generic_csr_wr_cmd ,ARRAY_SIZE(ql_generic_csr_wr_cmd));

	mutex_unlock(&lock);

	udelay(1);

#ifdef QL_MIPI_READ_AFTER_WRITE_DEBUG
	if (ql_mipi_read(mfd, address, &regval, data_size))
		return -1;

	if (regval != value)
		pr_info("\nMismatch: [0x%x]->0x%x r0x%x\n",\
			address, value, regval);
#endif
#if 0/*def QL_I2C_READ_AFTER_WRITE_DEBUG*/
	if (address != 0x154) {
	 if (vx5d3b_i2c_read(address, &regval, data_size))
		 return -1;

	 if (regval != value) {
		 printk("\nMismatch: [0x%x]->0x%x "
			 "r0x%x\n", address, value, regval);
		 //return -1;
		}
	}
	if (address != 0x154)
	vx5d3b_i2c_release();

	udelay(1000);
#endif
 
	 return 0;
 }
 
 
#endif

int vx5d3b_i2c_read(u32 addr, u32 *val, u32 data_size) 
{
	u32 data;
	char buf[] = GEN_QL_CSR_OFFSET_LENGTH;
	char rx[10];
	int ret = -1;
	int write_size;

	if (g_client == NULL)	/* No global client pointer? */
	return -1;

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
		return -1;
	}

	/*generic read request 0x24 to send generic read command */
	write_size = 4;

	buf[0] = CONTROL_BYTE_GEN;
	buf[1] = 0x24;  /* Data ID */
	buf[2] = 0x05;  /* Vendor Id 1 */
	buf[3] = 0x01;  /* Vendor Id 2 */

	if ((ret = i2c_master_send( g_client,
				(char*)(&buf[0]),
				write_size )) != write_size) {

		pr_info("i2c_master_send failed (%d)!\n", ret);
		mutex_unlock(&lock);
		return -1;
	}

	/*return number of bytes or error*/
	if ((ret = i2c_master_recv( g_client,
					(char*)(&rx[0]),
					data_size )) != data_size) {

		pr_info("i2c_master_recv failed (%d)!\n", ret);
		mutex_unlock(&lock);
		return -1;
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

int vx5d3b_i2c_release(void)
{
	int write_size;
	int ret = 0;
	char buf[] = Vx5D3B_I2C_RELEASE;

	mutex_lock(&lock);

	write_size = 1;

	if ((ret = i2c_master_send( g_client,
					(char*)(&buf[0]),
					write_size )) != write_size)
		pr_info("i2c_master_send failed (%d)!\n", ret);

	mutex_unlock(&lock);

	return ret;
}

int tc35876x_read32(u16 reg, u32 *pval)
{
	int ret;
	int status;
	u8 address[2], data[4];

	if (g_client == NULL)	/* No global client pointer? */
		return -1;

	mutex_lock(&lock);
	memset(data, 0, 4);
	address[0] = (reg >> 8) & 0xff;
	address[1] = reg & 0xff;

	ret = i2c_master_send(g_client, address, 2);
	if (ret < 0) {
		status = -EIO;
		goto out_unlock;
	}
	ret = i2c_master_recv(g_client, data, 4);
	if (ret >= 0) {
		status = 0;
		*pval = data[0] | (data[1] << 8) | (data[2] << 16)
		    | (data[3] << 24);
	} else
		status = -EIO;

out_unlock:
	mutex_unlock(&lock);

	return status;
}
EXPORT_SYMBOL(tc35876x_read32);

int tc35876x_read16(u16 reg, u16 *pval)
{
	int ret;
	int status;
	u8 address[2], data[4];

	if (g_client == NULL)	/* No global client pointer? */
		return -1;

	mutex_lock(&lock);
	address[0] = (reg >> 8) & 0xff;
	address[1] = reg & 0xff;

	ret = i2c_master_send(g_client, address, 2);
	if (ret < 0) {
		status = -EIO;
		goto out_unlock;
	}

	ret = i2c_master_recv(g_client, data, 2);
	if (ret >= 0) {
		status = 0;
		*pval = data[0] | (data[1] << 8);
	} else
		status = -EIO;
out_unlock:
	mutex_unlock(&lock);

	return status;
}
EXPORT_SYMBOL(tc35876x_read16);

int tc35876x_write32(u16 reg, u32 val)
{
	int status = 0;

	int write_size;
	char buf[] = GEN_QL_CSR_WRITE;

	if (g_client == NULL)	/* No global client pointer? */
	return -1;

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
	else
		status = -EIO;

	udelay(10);
	mutex_unlock(&lock);

/* For I2C of tc35876x

	 {
		int ret;
		u8 data[6];

		if (g_client == NULL)
			return -1;

		mutex_lock(&lock);
		data[0] = (reg >> 8) & 0xff;
		data[1] = reg & 0xff;
		data[2] = val & 0xff;
		data[3] = (val >> 8) & 0xff;
		data[4] = (val >> 16) & 0xff;
		data[5] = (val >> 24) & 0xff;
		ret = i2c_master_send(g_client, data, 6);
		if (ret >= 0)
			status = 0;
		else
			status = -EIO;
		mutex_unlock(&lock);
	}
*/
	return status;
}
EXPORT_SYMBOL(tc35876x_write32);

int tc35876x_write16(u16 reg, u16 val)
{
	int ret;
	int status;
	u8 data[4];

	if (g_client == NULL)	/* No global client pointer? */
		return -1;

	mutex_lock(&lock);
	data[0] = (reg >> 8) & 0xff;
	data[1] = reg & 0xff;
	data[2] = val & 0xff;
	data[3] = (val >> 8) & 0xff;

	ret = i2c_master_send(g_client, data, 4);
	if (ret >= 0)
		status = 0;
	else
		status = -EIO;
	mutex_unlock(&lock);

	return status;
}
EXPORT_SYMBOL(tc35876x_write16);

#ifdef	CONFIG_PROC_FS
#define TC35876x_REG_NUM		(0x5a4)
#define	TC358762_PROC_FILE	"driver/tc35876x"
static struct proc_dir_entry *tc35876x_proc_file;
static unsigned long index;

static ssize_t tc35876x_proc_read(struct file *filp,
				  char *buffer, size_t length,
				  loff_t *offset)
{
	u32 reg_val;
	int ret;

	if ((index <= 0) || (index > TC35876x_REG_NUM))
		return 0;

	ret = tc35876x_read32(index, &reg_val);
	if (ret < 0)
		printk(KERN_INFO "tc35876x read error!\n");
	else
		printk(KERN_INFO "register 0x%lx: 0x%x\n", index, reg_val);
	return 0;
}

static ssize_t tc35876x_proc_write(struct file *filp,
				   const char *buff, size_t len,
				   loff_t *off)
{
	unsigned long reg_val;
	char messages[256], vol[256];
	int ret;

	if (len > 256)
		len = 256;

	if (copy_from_user(messages, buff, len))
		return -EFAULT;

	if ('-' == messages[0]) {
		/* set the register index */
		memcpy(vol, messages + 1, len - 1);
		ret = kstrtoul(vol, 16, &index);
		if (ret < 0)
			return ret;
	} else {
		/* set the register value */
		ret = kstrtoul(messages, 16, &reg_val);
		if (ret < 0)
			return ret;
		tc35876x_write32(index, reg_val);
	}

	return len;
}

static const struct file_operations tc35876x_proc_ops = {
	.read = tc35876x_proc_read,
	.write = tc35876x_proc_write,
};

static void create_tc35876x_proc_file(void)
{
	tc35876x_proc_file = create_proc_entry(TC358762_PROC_FILE, 0644, NULL);

	if (tc35876x_proc_file)
		tc35876x_proc_file->proc_fops = &tc35876x_proc_ops;
	else
		printk(KERN_INFO "proc file create failed!\n");
}

static void remove_tc35876x_proc_file(void)
{
	remove_proc_entry(TC358762_PROC_FILE, NULL);
}

#endif

static int __devinit tc35876x_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct tc35876x_platform_data *pdata;

	g_client = client;
	pdata = client->dev.platform_data;
	pdata->platform_init();


#ifdef	CONFIG_PROC_FS
	create_tc35876x_proc_file();
#endif

	return 0;
}

static int tc35876x_remove(struct i2c_client *client)
{
#ifdef	CONFIG_PROC_FS
	remove_tc35876x_proc_file();
#endif

	return 0;
}

static const struct i2c_device_id tc35876x_id[] = {
	{"tc35876x", 0},
	{}
};

static struct i2c_driver tc35876x_driver = {
	.driver = {
		   .name = "tc35876x",
		   },
	.id_table = tc35876x_id,
	.probe = tc35876x_probe,
	.remove = tc35876x_remove,
};

static int __init tc35876x_init(void)
{
	return i2c_add_driver(&tc35876x_driver);
}

static void __exit tc35876x_exit(void)
{
	i2c_del_driver(&tc35876x_driver);
}

subsys_initcall(tc35876x_init);
module_exit(tc35876x_exit);

MODULE_DESCRIPTION("Tc35876x Driver");
MODULE_LICENSE("GPL");
