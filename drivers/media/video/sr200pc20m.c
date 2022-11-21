/*
 * A V4L2 driver for siliconfile SR200PC20M cameras.
 * 
 * Copyright 2006 One Laptop Per Child Association, Inc.  Written
 * by Jonathan Corbet with substantial inspiration from Mark
 * McClelland's ovcamchip code.
 *
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 *jpeg
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
 * 
 * Create SR200PC20M driver from SR030PC50 driver by
 * Vincent Wan <zswan@marvell.com> for Marvell PXA986 harrison project, 2013/02/17.
 */
#include <linux/init.h>
#include <linux/module.h>

#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>
#include <mach/camera.h>

#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

#include <media/v4l2-subdev.h>
#include <mach/gpio.h>
#include <mach/camera.h>

#include "sr200pc20m.h"

#include "sr200pc20m_regs_50hz.h"
//#include "sr200pc20m_regs2.h"



MODULE_AUTHOR("Jonathan Corbet <corbet@lwn.net>");
MODULE_DESCRIPTION("A low-level driver for siliconfile SR200PC20M sensors");
MODULE_LICENSE("GPL");

struct device *cam_dev_rear;

#define to_sr200pc20m(sd)		container_of(sd, struct sr200pc20m_info, subdev)

#define sr200pc20m_WRT_LIST(B, A)	\
	sr200pc20m_i2c_wrt_list(B, A, (sizeof(A) / sizeof(A[0])), #A);


static const struct sr200pc20m_datafmt sr200pc20m_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
};

#define CAM_DEBUG

#ifdef CAM_DEBUG
#define Cam_Printk(msg...) printk(msg)
#else
#define Cam_Printk
#endif

/*
 * Basic window sizes.  These probably belong somewhere more globally
 * useful.
 */
 #define SVGA_WIDTH		800
 #define SVGA_HEIGHT	600

#define VGA_WIDTH		640
#define VGA_HEIGHT		480

#define QVGA_WIDTH		320
#define QVGA_HEIGHT	240

#define QQVGA_WIDTH	160
#define QQVGA_HEIGHT	120

/*
 * Our nominal (default) frame rate.
 */
#define SR200PC20M_FRAME_RATE 30

//#define SR200PC20M_I2C_ADDR (0x5A >> 1) 

#define REG_MIDH	0x1c	/* Manuf. ID high */

#define   CMATRIX_LEN 6

/*Heron Tuning*/
// #define CONFIG_LOAD_FILE

/*
 * Information we maintain about a known sensor.
 */


/*store the status of touch AF  0 : not touch, 1 : touch AF*/
static int sr200pc20m_touch_state;
static int sr200pc20m_AE_lock_state;
static int sr200pc20m_cam_state;

static int sr200pc20m_AE_lock(struct i2c_client *client,s32 value);
static int sr200pc20m_set_focus_mode(struct i2c_client *client, s32 value, int step);
static int sr200pc20m_set_focus(struct i2c_client *client,s32 value);
static int sr200pc20m_set_focus_touch_position(struct i2c_client *client, s32 value);
static int sr200pc20m_get_focus_status(struct i2c_client *client, struct v4l2_control *ctrl, s32 value);
void sr200pc20m_set_REG_TC_DBG_AutoAlgEnBits(struct i2c_client *client,int bit, int set); 


struct sr200pc20m_sensor sr200pc20m = {
	.timeperframe = {
		.numerator    = 1,
		.denominator  = 30,
	},
	.fps			= 30,
	//.bv			= 0,
	.state			= SR200PC20M_STATE_PREVIEW,
	.mode			= SR200PC20M_MODE_CAMERA,
	.preview_size		= PREVIEW_SIZE_640_480,
	.capture_size		= CAPTURE_SIZE_640_480,
	.detect			= SENSOR_NOT_DETECTED,
	.focus_mode		= SR200PC20M_AF_SET_NORMAL,
	.effect			= EFFECT_OFF,
	.iso			= ISO_AUTO,
	.photometry		= METERING_CENTER,
	.ev			= EV_DEFAULT,
	//.wdr			= SR200PC20M_WDR_OFF,
	.contrast		= CONTRAST_DEFAULT,
	.saturation		= SATURATION_DEFAULT,
	.sharpness		= SHARPNESS_DEFAULT,
	.wb			= WB_AUTO,
	//.isc 			= SR200PC20M_ISC_STILL_OFF,
	.scene			= SCENE_OFF,
	.aewb			= AWB_AE_UNLOCK,
	//.antishake		= SR200PC20M_ANTI_SHAKE_OFF,
	//.flash_capture	= SR200PC20M_FLASH_CAPTURE_OFF,
	//.flash_movie		= SR200PC20M_FLASH_MOVIE_OFF,
	.quality		= QUALITY_SUPERFINE, 
	//.zoom			= SR200PC20M_ZOOM_1P00X,
	.thumb_offset		= 0,
	.yuv_offset		= 0,
	.jpeg_main_size		= 0,
	.jpeg_main_offset	= 0,
	.jpeg_thumb_size	= 0,
	.jpeg_thumb_offset	= 0,
	.jpeg_postview_offset	= 0, 
	.jpeg_capture_w		= JPEG_CAPTURE_WIDTH,
	.jpeg_capture_h		= JPEG_CAPTURE_HEIGHT,
	.check_dataline		= 0,
	.exif_info={
		.exposure_time.denominal =0,
		.exposure_time.inumerator =0,
		.iso_speed_rationg =0,
			}
};

extern struct sr200pc20m_platform_data sr200pc20m_platform_data0;

struct sr200pc20m_format_struct;  /* coming later */
struct sr200pc20m_info {
	struct sr200pc20m_format_struct *fmt;  /* Current format */
	unsigned char sat;		/* Saturation value */
	int hue;			/* Hue value */
	struct v4l2_subdev subdev;
	int model;	/* V4L2_IDENT_xxx* codes from v4l2-chip-ident.h */
	u32 pixfmt;
	struct i2c_client *client;
	struct soc_camera_device icd;

};

#if 0
static int sr200pc20m_read(struct i2c_client *c, u16 reg, u16 *value)
{
	u8 data[2];
	u8 address[2];
	int ret = 0;
	address[0] = reg>>8;
	address[1] = reg;
	ret = i2c_master_send(c, address, 2);
	if(ret < 0){
		printk(KERN_ERR" kassey send %s %d ret = %d\n", __func__, __LINE__, ret);
		goto out;
	}
	ret = i2c_master_recv(c, data, 2);
	if(ret < 0){
		printk(KERN_ERR" kassey recv %s %d ret = %d\n", __func__, __LINE__, ret);
		goto out;
	}
	//*value = data[0] | data[1]<<8;
	*value = data[0]<<8 | data[1] ;
out:
	return (ret < 0) ? ret: 0;
}

static int sr200pc20m_write(struct i2c_client *c, u16 reg,  u16 val)
{
	u8 data[4];
	int ret = 0;
	data[0] = reg>>8;
	data[1] = reg;
	data[2]=  val>>8;
	data[3] = val;
	
	
	if (reg == 0xffff)
	{
	msleep(val);  /* Wait for reset to run */
	return 0;
	}
	
	ret = i2c_master_send(c, data, 4);
	return (ret < 0) ? ret: 0;
}
#endif

static int sr200pc20m_write_byte(struct i2c_client *c, unsigned char reg,
		unsigned char value)
{
	int retry = 3, ret;
	
	if (reg == 0xfe)
	{
		mdelay(value);  /* Wait for reset to run */
		return 0;
	}
	
to_retry:
	ret = i2c_smbus_write_byte_data(c, reg, value);
	if (ret < 0) {
			printk("<##############################>ret : %d , retry: %d \n", ret, retry);
			if (retry > 0) {
					retry --;
					goto to_retry;
				}
			}
	return ret;
}

/**
 * sr200pc20m_i2c_read_multi: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 * @r_data: buffer where data is read
 * @r_len: number of bytes to read
 *
 * Returns 0 on success, <0 on error
 */

/**
 * sr200pc20m_i2c_read: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @data: data to be read
 *
 * Returns 0 on success, <0 on error
 */
static int sr200pc20m_i2c_read( struct i2c_client *client, unsigned char subaddr, unsigned char *data)
{
	unsigned char buf[1];
	struct i2c_msg msg = {client->addr, 0, 1, buf};

	int err = 0;
	buf[0] = subaddr;

	if (!client->adapter)
		return -EIO;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;

	msg.flags = I2C_M_RD;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
		return -EIO;
	/*
	 * Data comes in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */

	*data = buf[0];

	return err;
}

/**
 * sr200pc20m_i2c_write_multi: Write (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static int sr200pc20m_i2c_write_multi( struct i2c_client *client, unsigned short addr, unsigned int w_data)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (addr >> 8);
	buf[1] = (u8) (w_data & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = buf;


	Cam_Printk("I2C CHIP ID=0x%x, DATA 0x%x 0x%x\n",
			client->addr, buf[0], buf[1]);


	do {
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc == 1)
			return 0;
		retry_count++;
		Cam_Printk("retry_count %d\n", retry_count);
		msleep(3);

	} while (retry_count <= 5);

	return 0;
}


static int32_t sr200pc20m_i2c_write_16bit( struct i2c_client *client, u16 packet)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (packet >> 8);
	buf[1] = (u8) (packet & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

#if defined(CAM_I2C_DEBUG)
	printk("I2C CHIP ID=0x%x, DATA 0x%x 0x%x\n",
			client->addr, buf[0], buf[1]);
#endif

	do {
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc == 1)
			return 0;
		retry_count++;
		printk("i2c transfer failed, retrying %x err:%d\n",
		       packet, rc);
		msleep(3);

	} while (retry_count <= 5);

	return 0;
}

static int sr200pc20m_reg_read_and_check(struct i2c_client *client,
								unsigned char pagemode, unsigned char addr)
{
	unsigned char val = 0xFF;

	sr200pc20m_write_byte(client,0x03,pagemode);//Vincent add here, for p0
	sr200pc20m_i2c_read(client, addr, &val);

	printk("-----------sr200pc20m_reg_read_check------pagemode:0x%x, reg addr:0x%x, value:0x%x------\n", pagemode, addr, val);

	return val;
}

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

static char *sr200pc20m_regs_table = NULL;
static int sr200pc20m_regs_table_size;

static int sr200pc20m_regs_table_init(void)
{
	struct file *filp;

	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();

	printk("***** %s %d\n", __func__, __LINE__);

	set_fs(get_ds());

	filp = filp_open("/sdcard/sr200pc20m_regs_50hz.h", O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk("***** file open error\n");
		return 1;
	}
	else
		printk(KERN_ERR "***** File is opened \n");


	l = filp->f_path.dentry->d_inode->i_size;

	printk("l = %ld\n", l);

	dp = kmalloc(l, GFP_KERNEL);
	if (dp == NULL) {
		printk("*****Out of Memory\n");
		filp_close(filp, current->files);
		return 1;
	}
 
	pos = 0;

	memset(dp, 0, l);

	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	if (ret != l) {
		printk("*****Failed to read file ret = %d\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		return 1;
	}

	filp_close(filp, current->files);
	set_fs(fs);

	sr200pc20m_regs_table = dp;
	sr200pc20m_regs_table_size = l;
	*((sr200pc20m_regs_table + sr200pc20m_regs_table_size) - 1) = '\0';

	printk("*****Compeleted %s %d\n", __func__, __LINE__);
	return 0;
}

void sr200pc20m_regs_table_exit(void)
{
	/* release allocated memory when exit preview */
	if (sr200pc20m_regs_table) {
		kfree(sr200pc20m_regs_table);
		sr200pc20m_regs_table = NULL;
		sr200pc20m_regs_table_size = 0;
	}
	else
		printk("*****sr200pc20m_regs_table is already null\n");

	printk("*****%s done\n", __func__);
}

static int sr200pc20m_regs_table_write(struct i2c_client *c, char *name)
{
	char *start, *end, *reg;//, *data;
	unsigned short addr, value;
	char reg_buf[5], data_buf[3];
	addr = value = 0;

	printk("*****%s entered.\n", __func__);

	*(reg_buf + 4) = '\0';
	*(data_buf + 2) = '\0';

	start = strstr(sr200pc20m_regs_table, name);

	end = strstr(start, "};");

	while (1) {
		/* Find Address */
		reg = strstr(start,"0x");
		if (reg)
			start = (reg + 6);
		if ((reg == NULL) || (reg > end))
			break;

		/* Write Value to Address */
		memcpy(reg_buf, (reg + 0), 4);
		memcpy(data_buf, (reg + 4), 2);
		addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16);
		value = (unsigned short)simple_strtoul(data_buf, NULL, 16);
		printk("addr 0x%02x, value 0x%02x\n", addr, value);

		if (addr == 0xff)
		{
			msleep(value);
			printk("delay 0x%02x, value 0x%02x\n", addr, value);
		}
		else
		{
			if( sr200pc20m_write_byte(c,addr, value) < 0 )
			{
				printk("<=PCAM=> %s fail on sensor_write\n", __func__);
			}
		}
	}
	printk(KERN_ERR "***** Writing [%s] Ended\n",name);

	return 0;
}

/* From Palladio which reads "//#  define  MAX_VALUE 0x0A20 "*/
static short sr200pc20m_regs_max_value(char *name)
{

	char *start, *reg;
	unsigned short value;
	char data_buf[7];

	*(data_buf + 6) = '\0';

	start = strstr(sr200pc20m_regs_table, name);

	/* Find Address */
	reg = strstr(start," 0x");
	if (reg == NULL)
		return 0;

	/* Write Value to Address */
	if (reg != NULL) {
		memcpy(data_buf, (reg + 1), 6);
		value = (unsigned short)simple_strtoul(data_buf, NULL, 16); /*Change To HEX value*/
	}

	printk("sr200pc20m_regs_max_value done\n");

	return value;

}

#endif // CONFIG_LOAD_FILE

static int sr200pc20m_i2c_wrt_list( struct i2c_client *client, const u16 *regs,
	int size, char *name)
{
#ifdef CONFIG_LOAD_FILE
	// sr200pc20m_write_regs_from_sd(name);
	sr200pc20m_regs_table_write(client, name);
#else

	int i;
	u8 m_delay = 0;

	u16 temp_packet;


	CAM_DEBUG("%s, size=%d", name, size);
	for (i = 0; i < size; i++) {
		temp_packet = regs[i];

		if ((temp_packet & SR200PC20M_DELAY) == SR200PC20M_DELAY) {
			m_delay = temp_packet & 0xFF;
			printk("delay = %d", m_delay*10);
			msleep(m_delay*10);/*step is 10msec*/
			continue;
		}

		if (sr200pc20m_i2c_write_16bit(client,temp_packet) < 0) {
			printk("fail(0x%x, 0x%x:%d)",
					client->addr, temp_packet, i);
			return -EIO;
		}
		/*udelay(10);*/
	}
#endif

	return 0;
}

static int sr200pc20m_detect(struct i2c_client *client)
{
	unsigned char ID = 0xFFFF;
	int i;

	printk("-----------sr200pc20m_detect------client->addr:0x%x------\n", client->addr);

	//return 0;//for rev0.0 board, no need tune main sensor.by Vincent Wan.
		
	//sr200pc20m_write_byte(client,0x03,0x00);//Vincent add here, for p0
	//sr200pc20m_i2c_read(client, 0x04, &ID);	
	ID = sr200pc20m_reg_read_and_check(client, 0x00, 0x04);
	
	if(ID == 0xb4) 
	{
		printk(SR200PC20M_MOD_NAME"========================================\n");
		printk(SR200PC20M_MOD_NAME"   [VGA CAM] vendor_id ID : 0x%04X\n", ID);
		printk(SR200PC20M_MOD_NAME"========================================\n");
	} 
	else 
	{
		printk(SR200PC20M_MOD_NAME"-------------------------------------------------\n");
		printk(SR200PC20M_MOD_NAME"   [VGA CAM] sensor detect failure !!\n");
		printk(SR200PC20M_MOD_NAME"   ID : 0x%04X[ID should be 0xb4]\n", ID);
		printk(SR200PC20M_MOD_NAME"-------------------------------------------------\n");
		return -EINVAL;
	}	

	return 0;
}

static void sr200pc20m_reset(struct i2c_client *client)
{
	msleep(1);
}

static int sr200pc20m_init(struct v4l2_subdev *sd, u32 val)
{

	//return 0;//for rev0.0 board, no need tune main sensor.by Vincent Wan.
		
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	int result =0;
	
#ifdef CONFIG_LOAD_FILE
	result = sr200pc20m_regs_table_init();
	if (result > 0)
	{		
		Cam_Printk(KERN_ERR "***** sr200pc20m_regs_table_init  FAILED. Check the Filie in MMC\n");
		return result;
	}
	result =0;
#endif

	Cam_Printk(KERN_ERR "********************************************* Before sr200pc20m_WRT_LIST\n");
	sr200pc20m_detect(c);
	sr200pc20m_WRT_LIST(c,sr200pc20m_Init_Reg);
	Cam_Printk(KERN_ERR "************************************ After sr200pc20m_WRT_LIST\n");

	sensor->state			= SR200PC20M_STATE_PREVIEW;
	sensor->mode		= SR200PC20M_MODE_CAMERA;
	sensor->effect		= EFFECT_OFF;
	sensor->iso			= ISO_AUTO;
	sensor->photometry	= METERING_CENTER;
	sensor->ev		= EV_DEFAULT;
	sensor->contrast	= CONTRAST_DEFAULT;
	sensor->saturation	= SATURATION_DEFAULT;
	sensor->sharpness	= SHARPNESS_DEFAULT;
	sensor->wb		= WB_AUTO;
	sensor->scene		= SCENE_OFF;
	sensor->quality		= QUALITY_SUPERFINE;
	sensor->fps			= FPS_auto;
	sensor->pix.width		=SVGA_WIDTH;
	sensor->pix.height		=SVGA_HEIGHT;
	sensor->pix.pixelformat = V4L2_PIX_FMT_YUV420;
	sensor->initial			= SR200PC20M_STATE_INITIAL;
	sensor->wide_capture	= false;
	sensor->preview_ratio	= NORMAL_PREVIEW_RATIO;

	Cam_Printk(KERN_NOTICE "===sr200pc20m_init===[%s  %d]====== \n", __FUNCTION__, __LINE__);
#if 0
	Cam_Printk(KERN_NOTICE "===sr200pc20m_init===start read check!!!!====== \n");

	sr200pc20m_reg_read_and_check(c, 0x10, 0x60);
	sr200pc20m_reg_read_and_check(c, 0x10, 0x61);
	sr200pc20m_reg_read_and_check(c, 0x10, 0x62);
	sr200pc20m_reg_read_and_check(c, 0x10, 0x63);
	sr200pc20m_reg_read_and_check(c, 0x10, 0x64);
	sr200pc20m_reg_read_and_check(c, 0x10, 0x66);
	sr200pc20m_reg_read_and_check(c, 0x10, 0x67);
#endif
	return result;
}


/*
 * Store information about the video data format.  The color matrix
 * is deeply tied into the format, so keep the relevant values here.
 * The magic matrix nubmers come from OmniVision.
 */
static struct sr200pc20m_format_struct {
	__u8 *desc;
	__u32 pixelformat;
	int bpp;   /* bits per pixel */
} sr200pc20m_formats[] = {
	{
		.desc		= "YUYV 4:2:2",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.bpp		= 16,
	},
	{
		.desc		= "YUYV422 planar",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.bpp		= 16,
	},
	{
		.desc           = "YUYV 4:2:0",
		.pixelformat    = V4L2_PIX_FMT_YUV420,
		.bpp            = 12,
	},
	{
		.desc           = "JFIF JPEG",
		.pixelformat    = V4L2_PIX_FMT_JPEG,
		.bpp            = 16,
	},
};
#define N_SR200PC20M_FMTS ARRAY_SIZE(sr200pc20m_formats)
/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sr200pc20m_win_size {
	int	width;
	int	height;
};
static struct sr200pc20m_win_size sr200pc20m_win_sizes[] = {
	/* 768x576, Normal Preview */
	{
		.width		= 768,
		.height		= 576,
	},
	/* 768x432, Wide Preview */
	{
		.width		= 768,
		.height		= 432,
	},
	/* 704x576, VT Preview */
	{
		.width		= 704,
		.height		= 576,
	},
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
	},
	/* QQVGA */
	{
		.width		= QQVGA_WIDTH,
		.height		= QQVGA_HEIGHT,
	},

};

static struct sr200pc20m_win_size  sr200pc20m_win_sizes_jpeg[] = {

	/* SVGA */
	{
		.width		= SVGA_WIDTH,
		.height		= SVGA_HEIGHT,
	},
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
	},
	/* QQVGA */
	{
		.width		= QQVGA_WIDTH,
		.height		= QQVGA_HEIGHT,
	},

};

/* Find a data format by a pixel code in an array */
static const struct sr200pc20m_datafmt *sr200pc20m_find_datafmt(
	enum v4l2_mbus_pixelcode code, const struct sr200pc20m_datafmt *fmt,
	int n)
{
	int i;
	for (i = 0; i < n; i++)
		if (fmt[i].code == code)
			return fmt + i;

	return NULL;
}

#define N_WIN_SIZES (ARRAY_SIZE(sr200pc20m_win_sizes))
/*
 * Store a set of start/stop values into the camera.
 */
static int sr200pc20m_set_hw(struct i2c_client *client, int hstart, int hstop,
		int vstart, int vstop)
{
//	int ret;
//	unsigned char v;
	/*
	 * Horizontal: 11 bits, top 8 live in hstart and hstop.  Bottom 3 of
	 * hstart are in href[2:0], bottom 3 of hstop in href[5:3].  There is
	 * a mystery "edge offset" value in the top two bits of href.
	 */
	return 0;
}

static int sr200pc20m_querycap(struct i2c_client *c, struct v4l2_capability *argp)
{
	if(!argp){
		printk(KERN_ERR" argp is NULL %s %d \n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}
	strcpy(argp->driver, "sr200pc20m");
	strcpy(argp->card, "TD/TTC");
	return 0;
}

static int sr200pc20m_enum_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(sr200pc20m_colour_fmts))
		return -EINVAL;
	*code = sr200pc20m_colour_fmts[index].code;
	return 0;
}

static int sr200pc20m_enum_fsizes(struct v4l2_subdev *sd,
				struct v4l2_frmsizeenum *fsize)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!fsize)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	/* abuse pixel_format, in fact, it is xlate->code*/
	switch (fsize->pixel_format) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
		if (fsize->index >= ARRAY_SIZE(sr200pc20m_win_sizes)) {
			dev_warn(&client->dev,
				"sr200pc20m unsupported size %d!\n", fsize->index);
			return -EINVAL;
		}
		fsize->discrete.height = sr200pc20m_win_sizes[fsize->index].height;
		fsize->discrete.width = sr200pc20m_win_sizes[fsize->index].width;
		break;
#if 0
	case V4L2_MBUS_FMT_JPEG_1X8:
		if (fsize->index >= ARRAY_SIZE(sr200pc20m_win_sizes_jpeg)) {
			dev_warn(&client->dev,
				"sr200pc20m unsupported jpeg size %d!\n",
				fsize->index);
			return -EINVAL;
		}
		fsize->discrete.height =
			sr200pc20m_win_sizes_jpeg[fsize->index].height;
		fsize->discrete.width =
			sr200pc20m_win_sizes_jpeg[fsize->index].width;
		break;
#endif
	default:
		dev_err(&client->dev, "sr200pc20m unsupported format!\n");
		return -EINVAL;
	}
	return 0;
}

static int sr200pc20m_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
 //static int sr200pc20m_try_fmt(struct i2c_client *c, struct v4l2_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct sr200pc20m_datafmt *fmt;
	int i;

	fmt = sr200pc20m_find_datafmt(mf->code, sr200pc20m_colour_fmts,
				   ARRAY_SIZE(sr200pc20m_colour_fmts));
	if (!fmt) {
		dev_err(&client->dev, "sr200pc20m unsupported color format!\n");
		return -EINVAL;
	}

	mf->field = V4L2_FIELD_NONE;

	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
		if (sr200pc20m_cam_state == SR200PC20M_STATE_CAPTURE) {
		mf->colorspace = V4L2_COLORSPACE_JPEG;
			break;
		}
		/* enum the supported sizes*/
		for (i = 0; i < ARRAY_SIZE(sr200pc20m_win_sizes); i++)
			if (mf->width == sr200pc20m_win_sizes[i].width
				&& mf->height == sr200pc20m_win_sizes[i].height)
				break;

		if (i >= ARRAY_SIZE(sr200pc20m_win_sizes)) {
			dev_err(&client->dev, "sr200pc20m unsupported window"
				"size, w%d, h%d!\n", mf->width, mf->height);
			return -EINVAL;
		}
		mf->colorspace = V4L2_COLORSPACE_JPEG;
		break;
#if 0
	case V4L2_MBUS_FMT_JPEG_1X8:
		/* enum the supported sizes for JPEG*/
		for (i = 0; i < ARRAY_SIZE(sr200pc20m_win_sizes_jpeg); i++)
			if (mf->width == sr200pc20m_win_sizes_jpeg[i].width &&
				mf->height == sr200pc20m_win_sizes_jpeg[i].height)
				break;

		if (i >= ARRAY_SIZE(sr200pc20m_win_sizes_jpeg)) {
			dev_err(&client->dev, "sr200pc20m unsupported jpeg size!\n");
			return -EINVAL;
		}
		mf->colorspace = V4L2_COLORSPACE_JPEG;
		break;
#endif

	default:
		dev_err(&client->dev, "sr200pc20m doesn't support code"
				"%d\n", mf->code);
		break;
	}
	return 0;
}


/*
 * Set a format.
 */

static int sr200pc20m_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	int ret = 0;
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	const struct sr200pc20m_datafmt *fmt;
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	
//	struct regval_list *pregs = NULL;
//	struct regval_list *pregs_default = NULL;

	printk("[DHL]sr200pc20m_s_fmt..!!! \n");
	printk("[DHL]mf->code : [%d] \n",mf->code);
	printk("[DHL]mf->width : [%d] \n",mf->width);



	fmt =sr200pc20m_find_datafmt(mf->code,sr200pc20m_colour_fmts,
				   ARRAY_SIZE(sr200pc20m_colour_fmts));
	if (!fmt) {
		dev_err(&c->dev, "sr200pc20m unsupported color format!\n");
		return -EINVAL;
	}

	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
			sensor->pix.pixelformat = V4L2_PIX_FMT_YUV422P;
			//sr200pc20m_cam_state = SR200PC20M_STATE_PREVIEW;
			if(sr200pc20m_cam_state == SR200PC20M_STATE_PREVIEW)
			{
				switch (mf->width)
				{
					/* for YUV preview */
					case 768:
						if(mf->height == 432) {
							Cam_Printk(KERN_ERR"choose Wide-Preview setting \n");
							sensor->preview_ratio = WIDE_PREVIEW_RATIO;

						}
						else {
							Cam_Printk(KERN_ERR"choose Normal Preview setting \n");
							sensor->preview_ratio = NORMAL_PREVIEW_RATIO;
						}
						break;
					case 704:
						if(mf->height == 576) {
							Cam_Printk(KERN_ERR"choose VT-Preview setting \n");
							sensor->preview_ratio = VT_PREVIEW_RATIO;
						}
						break;
					default:
						printk("\n unsupported size for preview! %s %d w=%d h=%d\n", __FUNCTION__, __LINE__, mf->width, mf->height);
						sensor->preview_ratio = NORMAL_PREVIEW_RATIO;
						goto out;
						break;
				}
			}

			Cam_Printk(KERN_NOTICE "!!!!Set  FPS to 30fps!!!!\n");
			//sr200pc20m_write_regs(c, regs_sr200pc20m_30_FPS, ARRAY_SIZE(regs_sr200pc20m_30_FPS),"regs_sr200pc20m_30_FPS");


			if (sr200pc20m_cam_state == SR200PC20M_STATE_CAPTURE) {
				if(mf->width == 1600 && mf->height == 900) {
					Cam_Printk(KERN_ERR"choose Wide capture size\n");
					sensor->wide_capture = true;
				}
				else {
					Cam_Printk(KERN_ERR"choose Normal capture size\n");
					sensor->wide_capture = false;
				}
				//sr200pc20m_write_regs(c, regs_sr200pc20m_Capture_Start, ARRAY_SIZE(regs_sr200pc20m_Capture_Start),"regs_sr200pc20m_Capture_Start");
				Cam_Printk(KERN_NOTICE "Start to yuv format Capture \n");
			}


			break;
			
		default:
			printk("\n unsupported format! %s %d\n", __FUNCTION__, __LINE__);
			break;
	}

out:
	return ret;
}

/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 */
static int sr200pc20m_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
//	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_captureparm *cp = &parms->parm.capture;
//	u16 clkrc;
//	int ret;

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	//ret = sr200pc20m_read(c, REG_CLKRC, &clkrc);
	//if (ret < 0)
	//	return ret;
	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.numerator = 1;
	cp->timeperframe.denominator = SR200PC20M_FRAME_RATE;
	//if ((clkrc & CLK_EXT) == 0 && (clkrc & CLK_SCALE) > 1)
	//	cp->timeperframe.denominator /= (clkrc & CLK_SCALE);
	return 0;
}

static int sr200pc20m_s_parm(struct i2c_client *c, struct v4l2_streamparm *parms)
{
	return 0;
}

/*
 * Code for dealing with controls.
 */

/*
 * Hue also requires messing with the color matrix.  It also requires
 * trig functions, which tend not to be well supported in the kernel.
 * So here is a simple table of sine values, 0-90 degrees, in steps
 * of five degrees.  Values are multiplied by 1000.
 *
 * The following naive approximate trig functions require an argument
 * carefully limited to -180 <= theta <= 180.
 */
#define SIN_STEP 5
static const int sr200pc20m_sin_table[] = {
	0,	 87,   173,   258,   342,   422,
	499,	573,   642,   707,   766,   819,
	866,	906,   939,   965,   984,   996,
	1000
};

static int sr200pc20m_sine(int theta)
{
	int chs = 1;
	int sine;

	if (theta < 0) {
		theta = -theta;
		chs = -1;
	}
	if (theta <= 90)
		sine = sr200pc20m_sin_table[theta/SIN_STEP];
	else {
		theta -= 90;
		sine = 1000 - sr200pc20m_sin_table[theta/SIN_STEP];
	}
	return sine*chs;
}

static int sr200pc20m_cosine(int theta)
{
	theta = 90 - theta;
	if (theta > 180)
		theta -= 360;
	else if (theta < -180)
		theta += 360;
	return sr200pc20m_sine(theta);
}

static int sr200pc20m_calc_cmatrix(struct sr200pc20m_info *info,
		int matrix[CMATRIX_LEN])
{
	return 0;
}

static int sr200pc20m_t_saturation(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	s32 old_value = (s32)sensor->saturation;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case SATURATION_MINUS_2:
			//sr200pc20m_write_regs(client, regs_sr200pc20m_saturation_level_0, ARRAY_SIZE(regs_sr200pc20m_saturation_level_0),"regs_sr200pc20m_saturation_level_0");			
			break;

		case SATURATION_MINUS_1:
			//sr200pc20m_write_regs(client, regs_sr200pc20m_saturation_level_1, ARRAY_SIZE(regs_sr200pc20m_saturation_level_1),"regs_sr200pc20m_saturation_level_1");			
			break;		

		case SATURATION_DEFAULT:
			//sr200pc20m_write_regs(client, regs_sr200pc20m_saturation_level_2, ARRAY_SIZE(regs_sr200pc20m_saturation_level_2),"regs_sr200pc20m_saturation_level_2");			
			break;	

		case SATURATION_PLUS_1:
			//sr200pc20m_write_regs(client, regs_sr200pc20m_saturation_level_3, ARRAY_SIZE(regs_sr200pc20m_saturation_level_3),"regs_sr200pc20m_saturation_level_3");			
			break;		

		case SATURATION_PLUS_2:
			//sr200pc20m_write_regs(client, regs_sr200pc20m_saturation_level_4, ARRAY_SIZE(regs_sr200pc20m_saturation_level_4),"regs_sr200pc20m_saturation_level_4");			
			break;	

		default:
			printk(SR200PC20M_MOD_NAME "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->saturation = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->saturation);
	return 0;
}

static int sr200pc20m_q_saturation(struct i2c_client *client, __s32 *value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_q_saturation is called...\n"); 
	value = sensor->saturation;
	return 0;
}


static int sr200pc20m_t_hue(struct i2c_client *client, int value)
{
	return 0;
}


static int sr200pc20m_q_hue(struct i2c_client *client, __s32 *value)
{
	return 0;
}


/*
 * Some weird registers seem to store values in a sign/magnitude format!
 */
static unsigned char sr200pc20m_sm_to_abs(unsigned char v)
{
	if ((v & 0x80) == 0)
		return v + 128;
	else
		return 128 - (v & 0x7f);
}


static unsigned char sr200pc20m_abs_to_sm(unsigned char v)
{
	if (v > 127)
		return v & 0x7f;
	else
		return (128 - v) | 0x80;
}

//static int sr200pc20m_q_brightness(struct i2c_client *client, __s32 *value)
static int sr200pc20m_q_brightness(struct i2c_client *client, int value)
{
	// struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_get_scene is called...\n");
	// value = sensor->ev;

	switch(value)
	{
		case EV_MINUS_4:
			sr200pc20m_WRT_LIST(client, sr200pc20m_bright_m4);
			Cam_Printk(KERN_NOTICE "[Set Brightness] Minus 4 !!!\n");
			break;

		case EV_MINUS_3:
			sr200pc20m_WRT_LIST(client, sr200pc20m_bright_m3);
			Cam_Printk(KERN_NOTICE "[Set Brightness] Minus 3 !!!\n");
			break;

		case EV_MINUS_2:
			sr200pc20m_WRT_LIST(client, sr200pc20m_bright_m2);
			Cam_Printk(KERN_NOTICE "[Set Brightness] Minus 2 !!!\n");
			break;

		case EV_MINUS_1:
			sr200pc20m_WRT_LIST(client, sr200pc20m_bright_m1);
			Cam_Printk(KERN_NOTICE "[Set Brightness] Minus 1 !!!\n");
			break;

		case EV_DEFAULT:
			sr200pc20m_WRT_LIST(client, sr200pc20m_bright_default);
			Cam_Printk(KERN_NOTICE "[Set Brightness] Default !!!\n");
			break;

		case EV_PLUS_1:
			sr200pc20m_WRT_LIST(client, sr200pc20m_bright_p1);
			Cam_Printk(KERN_NOTICE "[Set Brightness] Plus 1 !!!\n");
			break;

		case EV_PLUS_2:
			sr200pc20m_WRT_LIST(client, sr200pc20m_bright_p2);
			Cam_Printk(KERN_NOTICE "[Set Brightness] Plus 2 !!!\n");
			break;

		case EV_PLUS_3:
			sr200pc20m_WRT_LIST(client, sr200pc20m_bright_p3);
			Cam_Printk(KERN_NOTICE "[Set Brightness] Plus 3 !!!\n");
			break;

		case EV_PLUS_4:
			sr200pc20m_WRT_LIST(client, sr200pc20m_bright_p4);
			Cam_Printk(KERN_NOTICE "[Set Brightness] Plus 4 !!!\n");
			break;

		default:
			printk(SR200PC20M_MOD_NAME "quality value is not supported!!!\n");
		return -EINVAL;
	}

	return 0;
}

static int sr200pc20m_t_contrast(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	s32 old_value = (s32)sensor->contrast;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case CONTRAST_MINUS_2:
			break;

		case CONTRAST_MINUS_1:
			break;		

		case CONTRAST_DEFAULT:
			break;	

		case CONTRAST_PLUS_1:
			break;		

		case CONTRAST_PLUS_2:
			break;	

		default:
			printk(SR200PC20M_MOD_NAME "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->contrast = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->quality);
	return 0;
}

static int sr200pc20m_q_contrast(struct i2c_client *client, __s32 *value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_q_quality is called...\n"); 
	value = sensor->contrast;
	return 0;
}

static int sr200pc20m_q_hflip(struct i2c_client *client, __s32 *value)
{
	return 0;
}


static int sr200pc20m_t_hflip(struct i2c_client *client, int value)
{
	return 0;
}



static int sr200pc20m_q_vflip(struct i2c_client *client, __s32 *value)
{
	return 0;
}


static int sr200pc20m_t_vflip(struct i2c_client *client, int value)
{
	return 0;
}

static int sr200pc20m_set_nightshot(struct i2c_client *client)
{
	unsigned char a, b, c;
	int expTime;
	int expMax;

	sr200pc20m_write_byte(client,0x03,0x00);
	sr200pc20m_write_byte(client,0x11,0x90); // Camcorder -> Camera, Skip frame
	sr200pc20m_write_byte(client,0x03,0x20);
	sr200pc20m_write_byte(client,0x10,0x0c); // AE Off

	a = sr200pc20m_reg_read_and_check(client, 0x20, 0x80);
	b = sr200pc20m_reg_read_and_check(client, 0x20, 0x81);
	c = sr200pc20m_reg_read_and_check(client, 0x20, 0x82);

	expTime = ((a<<16)+(b<<8)+(c));

	a = sr200pc20m_reg_read_and_check(client, 0x20, 0x88);
	b = sr200pc20m_reg_read_and_check(client, 0x20, 0x89);
	c = sr200pc20m_reg_read_and_check(client, 0x20, 0x8a);

	expMax = ((a<<16)+(b<<8)+(c));

	if(expTime < expMax) { // Normal Condition
		sr200pc20m_WRT_LIST(client, sr200pc20m_NightshotNomal);
		Cam_Printk(KERN_NOTICE "sr200pc20m_set_nightshot() : Night shot Normal: expTime = %d, expMax = %d\n", expTime, expMax);
	}else{
		sr200pc20m_WRT_LIST(client, sr200pc20m_NightshotDark);
		Cam_Printk(KERN_NOTICE "sr200pc20m_set_nightshot() : Night shot Dark: expTime = %d, expMax = %d\n", expTime, expMax);
	}
	return 0;
}

static int sr200pc20m_t_scene(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	s32 old_value = (s32)sensor->scene;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	if(value != SCENE_OFF){
		Cam_Printk(KERN_NOTICE "regs_sr200pc20m_scene_off");
	}
	switch(value)
	{
		case SCENE_OFF:
			sr200pc20m_WRT_LIST(client, sr200pc20m_SceneOff);
			Cam_Printk(KERN_NOTICE "[Set Scene mode]  OFF setting !!!\n");
			break;
		case SCENE_PORTRAIT:
			break;
		case SCENE_LANDSCAPE:
			sr200pc20m_WRT_LIST(client, sr200pc20m_Landscape);
			Cam_Printk(KERN_NOTICE "[Set Scene mode]  Landscape setting !!!\n");
			break;
		case SCENE_SPORTS:
			break;
		case SCENE_PARTY:
			sr200pc20m_WRT_LIST(client, sr200pc20m_Party);
			Cam_Printk(KERN_NOTICE "[Set Scene mode]  Party setting !!!\n");
			break;
		case SCENE_BEACH:
			break;
		case SCENE_SUNSET:
			sr200pc20m_WRT_LIST(client, sr200pc20m_Sunset);
			Cam_Printk(KERN_NOTICE "[Set Scene mode]  Sunset setting !!!\n");
			break;
		case SCENE_DAWN:
			sr200pc20m_WRT_LIST(client, sr200pc20m_Dawn);
			Cam_Printk(KERN_NOTICE "[Set Scene mode]  Dawn setting !!!\n");
			break;
		case SCENE_FALL:
			sr200pc20m_WRT_LIST(client, sr200pc20m_Fall);
			Cam_Printk(KERN_NOTICE "[Set Scene mode]  Fall setting !!!\n");
			break;
		case SCENE_NIGHT:
			sr200pc20m_set_nightshot(client);
			Cam_Printk(KERN_NOTICE "[Set Scene mode]  Nightshot setting !!!\n");
			break;
		case SCENE_BACKLIGHT:
			sr200pc20m_WRT_LIST(client, sr200pc20m_Backlight);
			Cam_Printk(KERN_NOTICE "[Set Scene mode]  Backlight setting !!!\n");
			break;
		case SCENE_FIRE:
			break;
		case SCENE_TEXT:
			break;
		case SCENE_CANDLE:
			sr200pc20m_WRT_LIST(client, sr200pc20m_Candle);
			Cam_Printk(KERN_NOTICE "[Set Scene mode]  Candle setting !!!\n");
			break;
		default:
			printk(SR200PC20M_MOD_NAME "Scene value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->scene = value;
	Cam_Printk(KERN_NOTICE "%s success [scene:%d]\n",__func__, sensor->scene);
	return 0;
}

static int sr200pc20m_q_scene(struct i2c_client *client, __s32 *value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_get_scene is called...\n"); 
	value = sensor->scene;
	return 0;
}

static int sr200pc20m_t_whitebalance(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	s32 old_value = (s32)sensor->wb;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case WB_AUTO:
			sr200pc20m_WRT_LIST(client, sr200pc20m_wb_auto);
			Cam_Printk(KERN_NOTICE "[Set whitebalance] WB_AUTO setting !!!\n");
		break;

		case WB_DAYLIGHT:
			sr200pc20m_WRT_LIST(client, sr200pc20m_wb_sunny);
			Cam_Printk(KERN_NOTICE "[Set whitebalance] WB_DAYLIGHT setting !!!\n");
			break;

		case WB_CLOUDY:
			sr200pc20m_WRT_LIST(client, sr200pc20m_wb_cloudy);
			Cam_Printk(KERN_NOTICE "[Set whitebalance] WB_CLOUDY setting !!!\n");
			break;

		case WB_FLUORESCENT:
			sr200pc20m_WRT_LIST(client, sr200pc20m_wb_fluorescent);
			Cam_Printk(KERN_NOTICE "[Set whitebalance] WB_FLUORESCENT setting !!!\n");
			break;

		case WB_INCANDESCENT:
			sr200pc20m_WRT_LIST(client, sr200pc20m_wb_incandescent);
			Cam_Printk(KERN_NOTICE "[Set whitebalance] WB_INCANDESCENT setting !!!\n");
			break;

		default:
			printk(SR200PC20M_MOD_NAME "White Balance value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->wb = value;
	Cam_Printk(KERN_NOTICE "%s success [White Balance e:%d]\n",__func__, sensor->wb);
	return 0;
}

static int sr200pc20m_q_whitebalance(struct i2c_client *client, __s32 *value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_get_whitebalance is called...\n"); 
	value = sensor->wb;
	return 0;
}

static int sr200pc20m_t_effect(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	s32 old_value = (s32)sensor->effect;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case EFFECT_OFF:
			sr200pc20m_WRT_LIST(client,sr200pc20m_effect_none);
			Cam_Printk(KERN_NOTICE "[Set effect] OFF setting !!!\n");
			break;

		case EFFECT_MONO:
			sr200pc20m_WRT_LIST(client,sr200pc20m_effect_gray);
			Cam_Printk(KERN_NOTICE "[Set effect] MONO setting !!!\n");
			break;

		case EFFECT_SEPIA:
			sr200pc20m_WRT_LIST(client,sr200pc20m_effect_sepia);
			Cam_Printk(KERN_NOTICE "[Set effect] SEPIA setting !!!\n");
			break;

		case EFFECT_NEGATIVE:
			sr200pc20m_WRT_LIST(client,sr200pc20m_effect_negative);
			Cam_Printk(KERN_NOTICE "[Set effect] NEGATIVE setting !!!\n");
			break;

		case EFFECT_AQUA:
			break;

		case EFFECT_SKETCH:
			break;

		default:
			printk(SR200PC20M_MOD_NAME "Sketch value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->effect = value;
	Cam_Printk(KERN_NOTICE "%s success [Effect e:%d]\n",__func__, sensor->effect);
	return 0;
}

static int sr200pc20m_q_effect(struct i2c_client *client, __s32 *value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_get_whitebalance is called...\n"); 
	value = sensor->effect;
	return 0;
}

static int sr200pc20m_t_ISO(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	s32 old_value = (s32)sensor->iso;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case ISO_AUTO:
			break;

		case ISO_50:
			break;		

		case ISO_100:
			break;	

		case ISO_200:
			break;	
		
		case ISO_400:
			break;

		default:
			printk(SR200PC20M_MOD_NAME "ISO value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->iso = value;
	Cam_Printk(KERN_NOTICE "%s success [ISO e:%d]\n",__func__, sensor->iso);
	return 0;
}

static int sr200pc20m_q_ISO(struct i2c_client *client, __s32 *value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_q_ISO is called...\n"); 
	value = sensor->iso;
	return 0;
}

static int sr200pc20m_t_photometry(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	s32 old_value = (s32)sensor->photometry;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case METERING_MATRIX:
			sr200pc20m_WRT_LIST(client,sr200pc20m_metering_matrix);
			Cam_Printk(KERN_NOTICE "[Set Metering] Matrix setting !!!\n");
			break;

		case METERING_SPOT:
			sr200pc20m_WRT_LIST(client,sr200pc20m_metering_spot);
			Cam_Printk(KERN_NOTICE "[Set Metering] Spot setting !!!\n");
			break;

		case METERING_CENTER:
			sr200pc20m_WRT_LIST(client,sr200pc20m_metering_center);
			Cam_Printk(KERN_NOTICE "[Set Metering] Center setting !!!\n");
			break;

		default:
			printk(SR200PC20M_MOD_NAME "ISO value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->photometry = value;
	Cam_Printk(KERN_NOTICE "%s success [PHOTOMERTY e:%d]\n",__func__, sensor->photometry);
	return 0;
}

static int sr200pc20m_q_photometry(struct i2c_client *client, __s32 *value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_q_photometry is called...\n"); 
	value = sensor->photometry;
	return 0;
}

static int sr200pc20m_t_quality(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	s32 old_value = (s32)sensor->quality;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case QUALITY_SUPERFINE:
			break;

		case QUALITY_FINE:
			break;		

		case QUALITY_NORMAL:
			break;	

		default:
			printk(SR200PC20M_MOD_NAME "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->quality = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->quality);
	return 0;
}

static int sr200pc20m_q_quality(struct i2c_client *client, __s32 *value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_q_quality is called...\n"); 
	value = sensor->quality;
	return 0;
}


static int sr200pc20m_t_sharpness(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	s32 old_value = (s32)sensor->sharpness;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	
	switch(value)
	{
		case SHARPNESS_MINUS_2:
			break;

		case SHARPNESS_MINUS_1:
			break;		

		case SHARPNESS_DEFAULT:
			break;	

		case SHARPNESS_PLUS_1:
			break;		

		case SHARPNESS_PLUS_2:
			break;	

		default:
			printk(SR200PC20M_MOD_NAME "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->sharpness = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->saturation);
	return 0;
}

static int sr200pc20m_q_sharpness(struct i2c_client *client, __s32 *value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_q_sharpness is called...\n"); 
	value = sensor->sharpness;
	return 0;
}

static int sr200pc20m_t_fps(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	s32 old_value = (s32)sensor->fps;

	//tagCamReg32_t    *pregs = NULL;
	//u32 regs_length = 0;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	if(old_value == value){
		Cam_Printk(KERN_NOTICE "%s new value is same as existing value\n", __func__);
		return 0;
	}

	switch(value)
	{

		case FPS_auto:
			break;
			
		case FPS_7:
			break;

		case FPS_10:
			break;

		case FPS_12:
			break;

		case FPS_15:
			break;		

		case FPS_20:
			break;			

		case FPS_25:
			break;	
			
		case FPS_30:
			break;	

		default:
			printk(KERN_NOTICE "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->fps = value;
	Cam_Printk(KERN_NOTICE "%s success [FPS e:%d]\n",__func__, sensor->fps);
	return 0;
}

static int sr200pc20m_q_fps(struct i2c_client *client, __s32 *value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_q_fps is called...\n"); 
	value = sensor->fps;
	return 0;
}

static int sr200pc20m_g_frame_time(struct i2c_client *client,__s32 *value)
{
	//struct sr200pc20m_sensor *sensor = &sr200pc20m;
		u16 frame_time =0;
		u16 temp1 = 0;
		int err;
	
		Cam_Printk(KERN_NOTICE "[DHL:Test] sr200pc20m_g_frame_time() \r\n");
	

	
		return 0;
}

static int sr200pc20m_mode_switch_check(struct i2c_client *client,__s32 *value)
{
	//struct sr200pc20m_sensor *sensor = &sr200pc20m;
		u16 frame_time =0;
		u16 temp1 = 0;
		int err;
	
		Cam_Printk(KERN_NOTICE "[DHL:Test] sr200pc20m_mode_switch_check() \r\n");
	

	
		return 0;
}


static int sr200pc20m_get_frame_time(struct i2c_client *client)
{
	//struct sr200pc20m_sensor *sensor = &sr200pc20m;
	u16 frame_time =0;
	u16 temp1 = 0;
	int err;

	Cam_Printk(KERN_NOTICE "[DHL:Test] sr200pc20m_g_frame_time() \r\n");

	return frame_time;
}

static int sr200pc20m_g_lightness_check(struct i2c_client *client,__s32 *value)
{
	//struct sr200pc20m_sensor *sensor = &sr200pc20m;
	//u16 frame_time =0;
	u16 temp1,temp2,temp3;
	int err;

	Cam_Printk(KERN_NOTICE "sr200pc20m_g_frame_check() \r\n");

	return 0;

}

static int sr200pc20m_ESD_check(struct i2c_client *client, __s32 *value)
{
	Cam_Printk(KERN_NOTICE "sr200pc20m_ESD_check() \r\n");

	//struct sr200pc20m_sensor *sensor = &sr200pc20m;
	unsigned char esd_value1 = 0;
	unsigned char esd_value2 = 0;
	unsigned char esd_value3 = 0;

	esd_value1 = sr200pc20m_reg_read_and_check(client, 0x00, 0x0b);
	esd_value2 = sr200pc20m_reg_read_and_check(client, 0x00, 0x0c);
	esd_value3 = sr200pc20m_reg_read_and_check(client, 0x00, 0x0d);

	if(esd_value1 == 0xaa && esd_value2 == 0xaa && esd_value3 == 0xaa){
		Cam_Printk(KERN_ERR "sr200pc20m_ESD_check() : Camera state ESD_NONE\n");
	*value = ESD_NONE;
	}
	else{
		Cam_Printk(KERN_ERR "sr200pc20m_ESD_check() : esd_value1 = %x, esd_value2 = %x, esd_value3 = %x\n", esd_value1, esd_value2, esd_value3);
		*value = ESD_ERROR;
	}
	return 0;
}
static int sr200pc20m_t_dtp_on(struct i2c_client *client)
{
	Cam_Printk(KERN_NOTICE "sr200pc20m_t_dtp_stop is called...\n");

	return 0;
}

static int sr200pc20m_t_dtp_stop(struct i2c_client *client)
{
	Cam_Printk(KERN_NOTICE "sr200pc20m_t_dtp_stop is called...\n"); 

	return 0;
}

static int sr200pc20m_g_exif_info(struct i2c_client *client,struct v4l2_exif_info *exif_info)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "sr200pc20m_g_exif_info is called...\n"); 
	*exif_info = sensor->exif_info;
	return 0;
}

static int sr200pc20m_set_mode(struct i2c_client *client, int value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	
	sensor->mode = value;
	Cam_Printk(KERN_NOTICE, "sr200pc20m_set_mode is called... mode = %d\n", sensor->mode);
	return 0;
}


static int sr200pc20m_preview_size(struct i2c_client *client, int value)
	{
		struct sr200pc20m_sensor *sensor = &sr200pc20m;
	
		if(sensor->mode != SR200PC20M_MODE_CAMCORDER)
		{
			switch (value) 
			{
				Cam_Printk(KERN_NOTICE "CAMERA MODE..\n"); 



				case SR200PC20M_PREVIEW_SIZE_320_240:
					break;
				case SR200PC20M_PREVIEW_SIZE_640_480:
					break;

				default:
					// When running in image capture mode, the call comes here.
					// Set the default video resolution - SR200PC20M_PREVIEW_VGA
					printk(SR200PC20M_MOD_NAME "Preview Resolution is not supported! : %d\n",value);
					return 0;
			}
		} 
		else 
		{
			Cam_Printk(KERN_NOTICE "CAMCORDER MODE..\n"); 

			switch (value) {

			case SR200PC20M_CAMCORDER_SIZE_320_240:
				break;
			case SR200PC20M_CAMCORDER_SIZE_640_480:
				break;

			default:
				// When running in image capture mode, the call comes here.
				// Set the default video resolution - SR200PC20M_PREVIEW_VGA
				printk(SR200PC20M_MOD_NAME "Preview Resolution is not supported! : %d\n",value);
				return 0;
		}
	}
	return 0;
}

static int sr200pc20m_set_still_status(void)
{
	Cam_Printk(KERN_NOTICE "[DHL]sr200pc20m_set_still_status.. \n");

	sr200pc20m_cam_state = SR200PC20M_STATE_CAPTURE;

	return 0;
}

static int sr200pc20m_set_preview_status(int value)
{
	Cam_Printk(KERN_NOTICE "[DHL]sr200pc20m_set_preview_status.. \n");

	if(0 == value){
		Cam_Printk(KERN_NOTICE "sr200pc20m_set_preview_status : SR200PC20M_STATE_PREVIEW cam state\n");
	sr200pc20m_cam_state = SR200PC20M_STATE_PREVIEW;
	}
	else if(1 == value){
		Cam_Printk(KERN_NOTICE "sr200pc20m_set_preview_status : SR200PC20M_STATE_CAMCORDER cam state\n");
		sr200pc20m_cam_state = SR200PC20M_STATE_CAMCORDER;
	}
	else{
		Cam_Printk(KERN_ERR "sr200pc20m_set_preview_status : Unknown cam state\n");
	}

	return 0;
}

static int sr200pc20m_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	int retval = 0;

	Cam_Printk(KERN_NOTICE "ioctl_s_ctrl is called...(%d)\n", ctrl->id);

	switch (ctrl->id) 
	{ 
		case V4L2_CID_ISO:
			retval = sr200pc20m_t_ISO(client,ctrl->value);
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			retval = sr200pc20m_t_whitebalance(client,ctrl->value);
			break;
		case V4L2_CID_EFFECT:
			retval = sr200pc20m_t_effect(client,ctrl->value);
			break;
		case V4L2_CID_CONTRAST:
			retval = sr200pc20m_t_contrast(client,ctrl->value);
			break;
		case V4L2_CID_SATURATION:
			retval = sr200pc20m_t_saturation(client,ctrl->value);
			break;
		case V4L2_CID_SHARPNESS:
			retval = sr200pc20m_t_sharpness(client,ctrl->value);
			break;			
		case V4L2_CID_SCENE:
			retval = sr200pc20m_t_scene(client,ctrl->value);
			break;
		case V4L2_CID_PHOTOMETRY:
			retval = sr200pc20m_t_photometry(client,ctrl->value);
			break;
		case V4L2_CID_QUALITY:
			retval = sr200pc20m_t_quality(client,ctrl->value);
			break;
		case V4L2_CID_FPS:
			retval = sr200pc20m_t_fps(client,ctrl->value);
			break;
		case V4L2_CID_CAMERA_CHECK_DATALINE:
			retval = sr200pc20m_t_dtp_on(client);
			break;
		case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
			retval = sr200pc20m_t_dtp_stop(client);
			break;
		case V4L2_CID_SELECT_MODE:
			retval = sr200pc20m_set_mode(client,ctrl->value);
			break;
		case V4L2_CID_CAMERA_PREVIEW_SIZE:
			retval = sr200pc20m_preview_size(client,ctrl->value);
			break;
		case V4L2_CID_FOCUS_MODE_STEP1:
			//retval = sr200pc20m_set_focus_mode(client,ctrl->value, SET_FOCUS_STEP1);
			break;
		case V4L2_CID_FOCUS_MODE_STEP2:
			//retval = sr200pc20m_set_focus_mode(client,ctrl->value, SET_FOCUS_STEP2);
			break;
		case V4L2_CID_FOCUS_MODE_STEP3:
			//retval = sr200pc20m_set_focus_mode(client,ctrl->value, SET_FOCUS_STEP3);
			break;
		case V4L2_CID_FOCUS_MODE:
			//retval = sr200pc20m_set_focus_mode(client,ctrl->value, SET_FOCUS_ALL);
			break;
		case V4L2_CID_AF:
			//retval = sr200pc20m_set_focus(client,ctrl->value);
			break;
		case V4L2_CID_CAMERA_OBJECT_POSITION_X:
			sensor->position.x = ctrl->value;
			retval = 0;
			break;
		case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
			sensor->position.y = ctrl->value;
			retval = 0;
			break;
		case V4L2_CID_AF_POSITION_START:
			retval = sr200pc20m_set_focus_touch_position(client,ctrl->value);
			break;
		case V4L2_CID_AF_POSITION_STOP:
			retval = sr200pc20m_set_focus_mode(client,ctrl->value, SET_FOCUS_ALL);
			break;
		case V4L2_CID_AE_LOCK:
			retval = sr200pc20m_AE_lock(client,ctrl->value);
			break;
		case V4L2_CID_SET_STILL_STATUS:
			retval = sr200pc20m_set_still_status();
			break;
		case V4L2_CID_SET_PREVIEW_STATUS:
			retval = sr200pc20m_set_preview_status(ctrl->value);
			break;

		default:
			Cam_Printk(SR200PC20M_MOD_NAME "[id]Invalid value is ordered!!!\n");
			break;
	}
	return retval;
}

/* Get chip identification */
static int sr200pc20m_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *id)
{
	struct sr200pc20m_info *priv = to_sr200pc20m(sd);

	id->ident = priv->model;
	id->revision = 0x0;//priv->revision;

	return 0;
}

static int sr200pc20m_set_bus_param(struct soc_camera_device *icd,
				    unsigned long flags)
{
	return 0;
}

camera_light_status_type sr200pc20m_check_illuminance_status(struct i2c_client *client);
int sr200pc20m_s_exif_info(struct i2c_client *client);

int sr200pc20m_streamon(struct i2c_client *client)
{

	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	struct v4l2_pix_format* pix = &sensor->pix;
	volatile int checkvalue, timeout = 5;
	camera_light_status_type illuminance=CAMERA_SENSOR_LIGHT_STATUS_NORMAL;

	Cam_Printk(KERN_NOTICE "sr200pc20m_streamon is called...\n");

	//if(pix->pixelformat == V4L2_PIX_FMT_JPEG)
	if(sr200pc20m_cam_state == SR200PC20M_STATE_CAPTURE)
	{
		Cam_Printk(KERN_NOTICE "capture: set stream-on setting\n");
		if(sensor->wide_capture == true) {
			sr200pc20m_WRT_LIST(client,sr200pc20m_Capture_wide);
		}
		else {
			sr200pc20m_WRT_LIST(client,sr200pc20m_Capture);
		}

	}
	else if(sr200pc20m_cam_state == SR200PC20M_STATE_CAMCORDER){
		Cam_Printk(KERN_NOTICE "camcorder: set stream-on setting\n");
		sr200pc20m_WRT_LIST(client,sr200pc20m_recording_60Hz_common);
	}
	else
	{
		switch(sensor->preview_ratio)
		{
			case NORMAL_PREVIEW_RATIO:
				sr200pc20m_WRT_LIST(client,sr200pc20m_preview);
				break;
			case WIDE_PREVIEW_RATIO:
				sr200pc20m_WRT_LIST(client,sr200pc20m_preview_wide);
				break;
			case VT_PREVIEW_RATIO:
				sr200pc20m_WRT_LIST(client,sr200pc20m_preview_vt);
				break;
			default:
				sr200pc20m_WRT_LIST(client,sr200pc20m_preview);
				break;
		}
		Cam_Printk(KERN_ERR "[start Preview] case : %d\n", sensor->preview_ratio);
		//return - EINVAL;
	}

			return 0;
}


static int sr200pc20m_streamoff(struct i2c_client *client)
{

	/* What's wrong with this sensor, it has no stream off function, oh!,Vincent.Wan */
	Cam_Printk(KERN_NOTICE " sr200pc20m_sensor_stop_stream E");
	sr200pc20m_WRT_LIST(client,sr200pc20m_stream_stop);
	Cam_Printk(KERN_NOTICE " sr200pc20m_sensor_stop_stream X");
	return 0;
}
static int set_stream(struct i2c_client *client, int enable)
{
	int ret = 0;
	int st = 0;

	if (enable) {
		ret = sr200pc20m_streamon(client);
		if (ret < 0)
			goto out;
	} else {
		ret = sr200pc20m_streamoff(client);
	}
out:
	return ret;
}

static int sr200pc20m_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	ret = set_stream(client, enable);
	if (ret < 0)
		dev_err(&client->dev, "sr200pc20m set stream error\n");
	return ret;
}

static int sr200pc20m_video_probe(struct soc_camera_device *icd,
			      struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sr200pc20m_info *priv = to_sr200pc20m(sd);
	u8 modelhi, modello;
	int ret, i;

	/*
	 * Make sure it's an sr200pc20m
	 */
	for(i =0;i<3;i++)
	{
		ret = sr200pc20m_detect(client);
		if (!ret) {
			Cam_Printk(KERN_NOTICE "=========siliconfile sr200pc20m sensor detected==========\n");
			goto out;
		}
		
	}

	priv->model = V4L2_IDENT_SR200PC20M;
out:
	return ret;
}

static int sr200pc20m_get_lux(struct i2c_client *client, int* lux)
{
	u16 lux_msb = 0;
	u16 lux_lsb = 0;
	int cur_lux = 0;


	printk("get_lux : %d lux\n", cur_lux);

	return cur_lux; //this value is under 0x0032 in low light condition 
}

static int sr200pc20m_get_exposure_time(struct i2c_client *client)
{
	unsigned char a, b, c;
	int exposureTime = 0;
	int fps = 0;

	a = sr200pc20m_reg_read_and_check(client, 0x20, 0x80);
	b = sr200pc20m_reg_read_and_check(client, 0x20, 0x81);
	c = sr200pc20m_reg_read_and_check(client, 0x20, 0x82);

	exposureTime = ((a<<19)+(b<<11)+(c<<3));
	fps = 24000000/exposureTime; // PCLK(48MHz) (OPCLK = 48,000,000 / 2 = 24,000,000)
	
	Cam_Printk(KERN_NOTICE "sr200pc20m_get_exposure_time() : exposure time = %d, fps = %d\n", exposureTime, fps);
	
	return fps;
}

static int sr200pc20m_get_iso(struct i2c_client *client)
{
	int regData;

	// Read data
	regData = sr200pc20m_reg_read_and_check(client, 0x20, 0xb0);

	Cam_Printk(KERN_NOTICE "sr200pc20m_get_iso() : regData = %d\n", regData);

	return regData;
}

camera_light_status_type sr200pc20m_check_illuminance_status(struct i2c_client *client)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	u32 lightStatus = 0;
	u16 lightStatus_low_word = 0;
	u16 lightStatus_high_word= 0;	
	int err;
	u32 luxcheck_low = 0x0032;

	Cam_Printk(KERN_NOTICE "sr200pc20m_check_illuminance_status() \r\n");

	

	return 0;
}

int sr200pc20m_s_exif_info(struct i2c_client *client)
{

	Cam_Printk(KERN_NOTICE "[DHL] EXIF Info.. \r\n");

	return 0;
}

static int sr200pc20m_s_thumbnail_size(struct i2c_client *client, struct v4l2_pix_format *thumbnail)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	struct v4l2_pix_format* pix = &sensor->thumbnail;
	pix->width= thumbnail->width;
	pix->height= thumbnail->height;
	int retval = 0;

	
	Cam_Printk(KERN_NOTICE "sr200pc20m_s_thumbnail_size is called...(Width %d Height %d)\n",pix->width,pix->height);

	return retval;
}


static int sr200pc20m_AE_AWB_Status(struct i2c_client *client,int *LuxCheck)
{
	u16 AE_Check =0;
	u16 AWB_Check=0;
	u32 lightStatus = CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
	u16 lightStatus_low_word = 0;
	u16 lightStatus_high_word= 0;	
	int err;
	u32 luxcheck_high = 0xFFFE;
	u32 luxcheck_low = 0x0080;	

	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	//illuminance = sr200pc20m_check_illuminance_status(client);

	Cam_Printk(KERN_NOTICE "sr200pc20m_check_illuminance_status() \r\n");

	Cam_Printk(KERN_NOTICE "*LuxCheck %d .. \n",*LuxCheck);
	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
//static int sr200pc20m_g_register(struct i2c_client *client, struct v4l2_dbg_register * reg)
static int sr200pc20m_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register * reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	int retval = 0;

	Cam_Printk(KERN_NOTICE "sr200pc20m_g_register is called...(%d)\n", reg->reg);

	switch(reg->reg)
	{
		case V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL:
			reg->val = sr200pc20m_get_exposure_time(client);
			break;
		case V4L2_CID_GET_EXIF_ISO_SPEED:
			reg->val = sr200pc20m_get_iso(client);
			break;
		case V4L2_CID_ESD_CHECK:
			sr200pc20m_ESD_check(client, &retval);
			break;
		default:
			Cam_Printk(SR200PC20M_MOD_NAME "[id]Invalid value is ordered!!!\n");
			break;
	}

	return retval;//sr200pc20m_read(client, (unsigned char)reg->reg, (unsigned char *)&(reg->val));
}

//static int sr200pc20m_s_register(struct i2c_client *client, struct v4l2_dbg_register * reg) // Previous code
static int sr200pc20m_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register * reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	int retval = 0;

	Cam_Printk(KERN_NOTICE "sr200pc20m_s_register is called...(%d)\n", reg->reg);

	switch (reg->reg)
	{
		case V4L2_CID_ISO:
			retval = sr200pc20m_t_ISO(client,reg->val);
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			retval = sr200pc20m_t_whitebalance(client,reg->val);
			break;
		case V4L2_CID_EFFECT:
			retval = sr200pc20m_t_effect(client,reg->val);
			break;
		case V4L2_CID_CONTRAST:
			retval = sr200pc20m_t_contrast(client,reg->val);
			break;
		case V4L2_CID_SATURATION:
			retval = sr200pc20m_t_saturation(client,reg->val);
			break;
		case V4L2_CID_SHARPNESS:
			retval = sr200pc20m_t_sharpness(client,reg->val);
			break;
		case V4L2_CID_SCENE:
			retval = sr200pc20m_t_scene(client,reg->val);
			break;
		case V4L2_CID_PHOTOMETRY:
			retval = sr200pc20m_t_photometry(client,reg->val);
			break;
		case V4L2_CID_QUALITY:
			retval = sr200pc20m_t_quality(client,reg->val);
			break;
		case V4L2_CID_FPS:
			retval = sr200pc20m_t_fps(client,reg->val);
			break;
		case V4L2_CID_CAMERA_CHECK_DATALINE:
			retval = sr200pc20m_t_dtp_on(client);
			break;
		case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
			retval = sr200pc20m_t_dtp_stop(client);
			break;
		case V4L2_CID_SELECT_MODE:
			retval = sr200pc20m_set_mode(client,reg->val);
			break;
		case V4L2_CID_CAMERA_PREVIEW_SIZE:
			retval = sr200pc20m_preview_size(client,reg->val);
			break;
		case V4L2_CID_FOCUS_MODE_STEP1:
			retval = sr200pc20m_set_focus_mode(client,reg->val, SET_FOCUS_STEP1);
			break;
		case V4L2_CID_FOCUS_MODE_STEP2:
			retval = sr200pc20m_set_focus_mode(client,reg->val, SET_FOCUS_STEP2);
			break;
		case V4L2_CID_FOCUS_MODE_STEP3:
			retval = sr200pc20m_set_focus_mode(client,reg->val, SET_FOCUS_STEP3);
			break;
		case V4L2_CID_FOCUS_MODE:
			retval = sr200pc20m_set_focus_mode(client,reg->val, SET_FOCUS_ALL);
			break;
		case V4L2_CID_AF:
			retval = sr200pc20m_set_focus(client,reg->val);
			break;
		case V4L2_CID_CAMERA_OBJECT_POSITION_X:
			sensor->position.x = reg->val;
			retval = 0;
			break;
		case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
			sensor->position.y = reg->val;
			retval = 0;
			break;
		case V4L2_CID_AF_POSITION_START:
			retval = sr200pc20m_set_focus_touch_position(client,reg->val);
			break;
		case V4L2_CID_AF_POSITION_STOP:
			retval = sr200pc20m_set_focus_mode(client,reg->val, SET_FOCUS_ALL);
			break;
		case V4L2_CID_AE_LOCK:
			retval = sr200pc20m_AE_lock(client,reg->val);
			break;
		case V4L2_CID_SET_STILL_STATUS:
			retval = sr200pc20m_set_still_status();
			break;
		case V4L2_CID_SET_PREVIEW_STATUS:
			retval = sr200pc20m_set_preview_status(reg->val);
			break;
		case V4L2_CID_BRIGHTNESS:
			retval = sr200pc20m_q_brightness(client,reg->val);
			break;

		default:
			Cam_Printk(SR200PC20M_MOD_NAME "[id : %d]Invalid value is ordered!!!\n", reg->reg);
			break;
	}
	return retval;
}
#endif

static struct i2c_device_id sr200pc20m_idtable[] = {
	{ "samsung_mainsensor", 1 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, sr200pc20m_idtable);

/**************************************************************************/
/*                 AF Focus Mopde                                         */
/**************************************************************************/

static int sr200pc20m_AE_lock(struct i2c_client *client,s32 value)
{       
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

    Cam_Printk(KERN_NOTICE "AE set value = %d\n", value);

	switch(value)
	{
		case AWB_AE_LOCK :

			break;
		case AWB_AE_UNLOCK :

			break;

		default:
			Cam_Printk(KERN_NOTICE "[AE]Invalid value is ordered!!! : %d\n",value);
			goto focus_fail;
	}

	sensor->aewb = value;
	return 0;

	focus_fail:
	Cam_Printk(KERN_NOTICE "sr200pc20m_AE_lock is failed!!!\n");
	return -EINVAL;
}

static int sr200pc20m_set_focus_mode(struct i2c_client *client, s32 value, int step)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;
	u16 frame_delay = 0;

	Cam_Printk(KERN_NOTICE "AF set value = %d\n", value);
	
	switch( step )
	{
		case SET_FOCUS_STEP1:

			break;
			
		case SET_FOCUS_STEP2:

			break;
			
		case SET_FOCUS_STEP3:

			break;
			
		case SET_FOCUS_ALL:

			break;
	}
	return 0;


	Cam_Printk(KERN_NOTICE "sr200pc20m_set_focus is failed!!!\n");
	return -EINVAL;
}


static int sr200pc20m_set_focus(struct i2c_client *client,s32 value)
{       
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

//	int cnt = 0;

	//u8 readdata = 0x00;
	//u8 status = 0x00;

	Cam_Printk(KERN_NOTICE "sr200pc20m_set_focus_status is called...[%d]\n",value);



	switch(value) 
	{
		case SR200PC20M_AF_START :

			break;
			
		case SR200PC20M_AF_STOP :

			break;
			
		case SR200PC20M_AF_STOP_STEP_1 :

			break;
			
		case SR200PC20M_AF_STOP_STEP_2 :

			break;
			
		case SR200PC20M_AF_STOP_STEP_3 :

			break;

		default:
			Cam_Printk(KERN_NOTICE "[AF]Invalid value is ordered!!!  : %d\n",value);
			goto focus_status_fail;
	}

	return 0;

	focus_status_fail:
		Cam_Printk(KERN_NOTICE "sr200pc20m_set_focus_status is failed!!!\n");
		return -EINVAL;
}

/* 640x480 Window Size */
#define INNER_WINDOW_WIDTH              143
#define INNER_WINDOW_HEIGHT             143
#define OUTER_WINDOW_WIDTH              320
#define OUTER_WINDOW_HEIGHT             266

static int sr200pc20m_set_focus_touch_position(struct i2c_client *client, s32 value)
{
	int err, i=0;
//	int ret;
	u16 read_value = 0;
	u16 touch_x, touch_y;
	u16 outter_x, outter_y;
	u16 inner_x, inner_y;
	u32 width, height;
	u16 outter_window_width, outter_window_height;
	u16 inner_window_width, inner_window_height;

	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	Cam_Printk(KERN_NOTICE "value : %d\n", value);



	sr200pc20m_touch_state = true;
	return 0;
}

static int sr200pc20m_get_focus_status(struct i2c_client *client, struct v4l2_control *ctrl, s32 value)
{
	struct sr200pc20m_sensor *sensor = &sr200pc20m;

	u16 status= 0x00;


	Cam_Printk(KERN_NOTICE, "result = %d (1.progress, 2.success, 3.fail)\n", ctrl->value);
	return 0;
  
}


void sr200pc20m_set_REG_TC_DBG_AutoAlgEnBits(struct i2c_client *client,int bit, int set) 
{
	//struct sr200pc20m_sensor *sensor = &sr200pc20m;
	int REG_TC_DBG_AutoAlgEnBits = 0; 

	return;
}

static int sr200pc20m_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_1_LANE;

	return 0;
}

static struct v4l2_subdev_core_ops sr200pc20m_core_ops = {
	//.g_ctrl			= sr200pc20m_g_ctrl,
	.s_ctrl			= sr200pc20m_s_ctrl,
	.init				= sr200pc20m_init,
	.g_chip_ident		= sr200pc20m_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= sr200pc20m_g_register,
	.s_register		= sr200pc20m_s_register,
#endif

};

static struct v4l2_subdev_video_ops sr200pc20m_video_ops = {
	.s_stream		= sr200pc20m_s_stream,
	.s_mbus_fmt		= sr200pc20m_s_fmt,
	.try_mbus_fmt		= sr200pc20m_try_fmt,
	.enum_mbus_fmt		= sr200pc20m_enum_fmt,
	.enum_mbus_fsizes	= sr200pc20m_enum_fsizes,
	.g_parm				= sr200pc20m_g_parm,
	.s_parm				= sr200pc20m_s_parm,
	.g_mbus_config	= sr200pc20m_g_mbus_config,
};

static struct v4l2_subdev_ops sr200pc20m_subdev_ops = {
	.core			= &sr200pc20m_core_ops,
	.video			= &sr200pc20m_video_ops,
};

/*
 * i2c_driver function
 */


static int sr200pc20m_command(struct i2c_client *client, unsigned int cmd, void *arg)
{


	switch (cmd) { 
		case VIDIOC_DBG_G_CHIP_IDENT:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_DBG_G_CHIP_IDENT\n");
			return v4l2_chip_ident_i2c_client(client, arg, V4L2_IDENT_SR200PC20M, 0);		
		case VIDIOC_INT_RESET:
			sr200pc20m_reset(client);
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_INT_RESET\n");
			return 0;
		case VIDIOC_QUERYCAP:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_QUERYCAP\n");
			return sr200pc20m_querycap(client, (struct v4l2_capability *) arg);
#if 0			
		case VIDIOC_ENUM_FMT:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_ENUM_FMT\n");
			return sr200pc20m_enum_fmt(client, index, (struct v4l2_fmtdesc *) arg);
#endif
		case VIDIOC_ENUM_FRAMESIZES:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_ENUM_FRAMESIZES\n");
			return sr200pc20m_enum_fsizes(client, (struct v4l2_frmsizeenum *) arg);
		case VIDIOC_TRY_FMT:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_TRY_FMT\n");
			return sr200pc20m_try_fmt(client, (struct v4l2_format *) arg);
		case VIDIOC_S_FMT:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_S_FMT\n");
			return sr200pc20m_s_fmt(client, (struct v4l2_format *) arg);
#if 0			
		case VIDIOC_QUERYCTRL:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_QUERYCTRL\n");
			return sr200pc20m_queryctrl(client, (struct v4l2_queryctrl *) arg);
#endif
		case VIDIOC_S_CTRL:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_S_CTRL\n");
			return sr200pc20m_s_ctrl(client, (struct v4l2_control *) arg);
		case VIDIOC_S_PARM:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_S_PARM\n");
			return sr200pc20m_s_parm(client, (struct v4l2_streamparm *) arg);
		case VIDIOC_G_PARM:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_G_PARM\n");
			return sr200pc20m_g_parm(client, (struct v4l2_streamparm *) arg);
#if 0		
		case VIDIOC_S_INPUT:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_S_INPUT\n");
			return sr200pc20m_s_input(client, (int *) arg);
#endif
		case VIDIOC_STREAMON:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_STREAMON\n");
			return sr200pc20m_streamon(client);
		case VIDIOC_STREAMOFF:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_STREAMOFF\n");
			return sr200pc20m_streamoff(client);
#ifdef CONFIG_VIDEO_ADV_DEBUG
		case VIDIOC_DBG_G_REGISTER:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_DBG_G_REGISTER\n");
			return sr200pc20m_g_register(client, (struct v4l2_dbg_register *) arg);
		case VIDIOC_DBG_S_REGISTER:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_DBG_S_REGISTER\n");
			return sr200pc20m_s_register(client, (struct v4l2_dbg_register *) arg);
#endif
		case VIDIOC_G_EXIF:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_G_EXIF\n");
			return sr200pc20m_g_exif_info(client, (struct v4l2_exif_info *) arg);
		case VIDIOC_S_THUMBNAIL:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_S_THUMBNAIL\n");
			return sr200pc20m_s_thumbnail_size(client, (struct v4l2_pix_format *) arg);
		case VIDIOC_AE_AWB_STATUS:
			Cam_Printk(KERN_NOTICE " sr200pc20m_command : VIDIOC_AE_AWB_STATUS\n");
			return sr200pc20m_AE_AWB_Status(client, (int *)arg);
	}
	return -EINVAL;
}

static ssize_t rear_camera_fw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", "SR200PC20M_SR200PC20M\n");
}

static ssize_t rear_camera_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char cam_type[] = "SF_SR200PC20M_NONE\n";

	return sprintf(buf, "%s", cam_type);
}

static struct device_attribute dev_attr_camtype_rear =
__ATTR(rear_camtype, S_IRUGO, rear_camera_type_show, NULL);

static struct device_attribute dev_attr_camfw_rear =
__ATTR(rear_camfw, S_IRUGO, rear_camera_fw_show, NULL);

static int __devinit sr200pc20m_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct sr200pc20m_info *priv;
	struct soc_camera_device *icd	= client->dev.platform_data;
	struct soc_camera_link *icl;
	int ret;

	printk("------------sr200pc20m_probe--------------\n");

	if (!icd) {
		dev_err(&client->dev, "Missing soc-camera data!\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(struct sr200pc20m_info), GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "Failed to allocate private data!\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &sr200pc20m_subdev_ops);

	cam_dev_rear =	device_create(camera_class, NULL,
			MKDEV(CAM_MAJOR, 0), NULL, "rear");

	if (IS_ERR(cam_dev_rear)) {
		dev_err(&client->dev, "Failed to create deivce (cam_dev_rear)\n");
		goto SKIP_SYSFS;
	}

	if (device_create_file(cam_dev_rear, &dev_attr_camtype_rear) < 0) {
		dev_err(&client->dev, "Failed to create device file: %s\n",
				dev_attr_camtype_rear.attr.name);
		goto SKIP_SYSFS;
	}

	if (device_create_file(cam_dev_rear, &dev_attr_camfw_rear) < 0) {
		dev_err(&client->dev, "Failed to create device file: %s\n",
				dev_attr_camfw_rear.attr.name);
		goto SKIP_SYSFS;
	}

SKIP_SYSFS:

	ret = sr200pc20m_video_probe(icd, client);
	if (ret < 0) {
		kfree(priv);
	}

	printk("------------sr200pc20m_probe---return --ret = %d---------\n", ret);
	return ret;
}

static int sr200pc20m_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_driver sr200pc20m_driver = {
	.driver = {
		.name	= "samsung_mainsensor",
	},
	.id_table       = sr200pc20m_idtable,	
	.command	= sr200pc20m_command,
	.probe		= sr200pc20m_probe,
	.remove		= sr200pc20m_remove,
};

/*
 * Module initialization
 */
static int __init sr200pc20m_mod_init(void)
{
	int ret =0;
	Cam_Printk(KERN_NOTICE "siliconfile sr200pc20m sensor driver, at your service\n");
	ret = i2c_add_driver(&sr200pc20m_driver);
	Cam_Printk(KERN_NOTICE "siliconfile sr200pc20m :%d \n ",ret);
	return ret;
}

static void __exit sr200pc20m_mod_exit(void)
{
	i2c_del_driver(&sr200pc20m_driver);
}

module_init(sr200pc20m_mod_init);
module_exit(sr200pc20m_mod_exit);

