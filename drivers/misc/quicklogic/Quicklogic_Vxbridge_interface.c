/*
 * QUICKLOGIC MIPI to RGB&LVDS&MIPI Bridge Chip
 *
 *
 * Copyright (C) 2013, Samsung Corporation.
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html
 */
#include <linux/errno.h> 
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
#include <linux/gpio.h>

#include <mach/cputype.h>
#if defined(CONFIG_MACH_CS05)
#include <mach/mfp-pxa1088-cs05.h>
#elif defined(CONFIG_MACH_DEGAS)
#include <mach/mfp-pxa1088-degas.h>
#elif defined(CONFIG_MACH_GOYA)
#include <mach/mfp-pxa986-goya.h>
#else
#endif
#include <mach/regs-mpmu.h>
#include <mach/pxa988.h>
#include <mach/pxa168fb.h>
#include <mach/Quicklogic_Vxbridge_interface.h>
//#include <onboard/lcd_hx8369_param.h>

/* Unique ID allocation */
static struct i2c_client *g_client;

static DEFINE_MUTEX(lock);


#undef CONFIG_PROC_FS


static struct dsi_cmd_desc quicklogic_generic_csr_wr_cmd[] = {
	{ DSI_DI_GENERIC_LWRITE, 1, 0, sizeof( quicklogic_csr_wr_payload ), quicklogic_csr_wr_payload }
};

int Quicklogic_mipi_write(struct pxa168fb_info *fbi, u32 address, u32 value, u32 data_size)
{

	mutex_lock(&lock);

	quicklogic_csr_wr_payload[3] = (u8)(address & 0xff);	/* Address LS */
	quicklogic_csr_wr_payload[4] = (u8)((address >> 8) & 0xff);	/* Address MS */
	quicklogic_csr_wr_payload[5] = (u8) value;			/* value LSB */

	if (data_size > 1)
		quicklogic_csr_wr_payload[6] = (u8)((value >> 8) & 0xff);

	if (data_size > 2) {
		quicklogic_csr_wr_payload[7] = (u8)((value >> 16) & 0xff);
		quicklogic_csr_wr_payload[8] = (u8)((value >> 24) & 0xff);
	}

	dsi_cmd_array_tx(fbi,quicklogic_generic_csr_wr_cmd ,ARRAY_SIZE(quicklogic_generic_csr_wr_cmd));

	mutex_unlock(&lock);

	udelay(1);

	return 0;
}

EXPORT_SYMBOL(Quicklogic_mipi_write);


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

	if ((ret = i2c_master_send( g_client,
				(char*)(&buf[0]),
				write_size )) != write_size) {

		pr_info("i2c_master_send failed (%d)!\n", ret);
		mutex_unlock(&lock);
		return -EIO;
	}

	/*return number of bytes or error*/
	if ((ret = i2c_master_recv( g_client,
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
	else
		status = -EIO;

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

	if ((ret = i2c_master_send( g_client,(char*)(&buf[0]),write_size )) != write_size) {
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

	if ((ret = i2c_master_send( g_client,(char*)(&buf[0]),
						write_size )) != write_size) {

		printk("%s: i2c_master_send failed (%d)!\n", __func__, ret);

		mutex_unlock(&lock);
		return -EIO;
	}

	if ((ret = i2c_master_recv( g_client, (char*)(&rx[0]),
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

#ifdef	CONFIG_PROC_FS
#define	QUICKLOGIC_PROC_FILE	"driver/vx_bridge"
static struct proc_dir_entry *quicklogic_proc_file;
static unsigned long index;

static ssize_t quicklogic_proc_read(struct file *filp,
				  char *buffer, size_t length,
				  loff_t *offset)
{
	u32 reg_val;
	int ret;

	if (index < 0)
		return 0;

	ret = Quicklogic_i2c_read32((u16)index, &reg_val);
	if (ret < 0)
		printk(KERN_INFO "QuickBridge read error!\n");
	else
		printk(KERN_INFO "register 0x%lx: 0x%x\n", index, reg_val);

	return 0;
}

static ssize_t quicklogic_proc_write(struct file *filp,
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
		Quicklogic_i2c_write32(index, reg_val);
	}

	return len;
}

static const struct file_operations quicklogic_proc_ops = {
	.read = quicklogic_proc_read,
	.write = quicklogic_proc_write,
};

static void create_quicklogic_proc_file(void)
{
	quicklogic_proc_file =
	    create_proc_entry(QUICKLOGIC_PROC_FILE, 0644, NULL);
	if (quicklogic_proc_file)
		quicklogic_proc_file->proc_fops = &quicklogic_proc_ops;
	else
		printk(KERN_INFO "proc file create failed!\n");
}

static void remove_quciklogic_proc_file(void)
{
	remove_proc_entry(QUICKLOGIC_PROC_FILE, NULL);
}

#endif

static int __devinit quickbridge_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	g_client = client;

	printk("quickbridge_probe\n");
#ifdef	CONFIG_PROC_FS
	create_quicklogic_proc_file();
#endif

	return 0;
}

static int quickbridge_remove(struct i2c_client *client)
{
#ifdef	CONFIG_PROC_FS
	remove_quciklogic_proc_file();
#endif

	return 0;
}

static const struct i2c_device_id quicklogci_id[] = {
	{"QuickBridge", 0},
	{}
};

static struct i2c_driver quickbridge_driver = {
	.driver = {
		   .name = "QuickBridge",
		   },
	.id_table = quicklogci_id,
	.probe = quickbridge_probe,
	.remove = quickbridge_remove,
};

static int __init quickbridge_init(void)
{
	return i2c_add_driver(&quickbridge_driver);
}

static void __exit quickbridge_exit(void)
{
	i2c_del_driver(&quickbridge_driver);
}

subsys_initcall(quickbridge_init);
module_exit(quickbridge_exit);

MODULE_DESCRIPTION("Quickbridge Driver");
MODULE_LICENSE("GPL");

