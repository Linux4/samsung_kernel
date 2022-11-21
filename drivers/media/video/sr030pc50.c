/*
 * A V4L2 driver for siliconfile SR030PC50 cameras.
 * 
 * Copyright 2006 One Laptop Per Child Association, Inc.  Written
 * by Jonathan Corbet with substantial inspiration from Mark
 * McClelland's ovcamchip code.
 *
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 *jpeg
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
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


#include "sr030pc50.h"

#include "sr030pc50_regs.h"

MODULE_AUTHOR("Jonathan Corbet <corbet@lwn.net>");
MODULE_DESCRIPTION("A low-level driver for siliconfile SR030PC50 sensors");
MODULE_LICENSE("GPL");

#define to_sr030pc50(sd)		container_of(sd, struct sr030pc50_info, subdev)

#define sr030pc50_WRT_LIST(B, A)	\
	sr030pc50_i2c_wrt_list(B, A, (sizeof(A) / sizeof(A[0])), #A);


static const struct sr030pc50_datafmt sr030pc50_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
	//{V4L2_MBUS_FMT_JPEG_1X8, V4L2_COLORSPACE_JPEG},
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


#define VGA_WIDTH		640
#define VGA_HEIGHT		480

#define QVGA_WIDTH		320
#define QVGA_HEIGHT		240

#define QCIF_WIDTH		176
#define QCIF_HEIGHT		144

#define QQVGA_WIDTH		160
#define QQVGA_HEIGHT	120

/*
 * Our nominal (default) frame rate.
 */
#define SR030PC50_FRAME_RATE 30

//#define SR030PC50_I2C_ADDR (0x5A >> 1) 

#define REG_MIDH	0x1c	/* Manuf. ID high */

#define   CMATRIX_LEN 6

/*Hendrix Tuning*/
//#define CONFIG_LOAD_FILE

/*
 * Information we maintain about a known sensor.
 */


/*store the status of touch AF  0 : not touch, 1 : touch AF*/
static int sr030pc50_touch_state;
static int sr030pc50_AE_lock_state;
static int sr030pc50_cam_state;

static int sr030pc50_i2c_wrt_list( struct i2c_client *client, const u16 *regs, int size, char *name);

static int sr030pc50_AE_lock(struct i2c_client *client,s32 value);
static int sr030pc50_set_focus_mode(struct i2c_client *client, s32 value, int step);
static int sr030pc50_set_focus(struct i2c_client *client,s32 value);
static int sr030pc50_set_focus_touch_position(struct i2c_client *client, s32 value);
static int sr030pc50_get_focus_status(struct i2c_client *client, struct v4l2_control *ctrl, s32 value);
void sr030pc50_set_REG_TC_DBG_AutoAlgEnBits(struct i2c_client *client,int bit, int set);
static int sr030pc50_ESD_check(struct i2c_client *client, struct v4l2_control *ctrl);



struct sr030pc50_sensor sr030pc50 = {
	.timeperframe = {
		.numerator    = 1,
		.denominator  = 30,
	},
	.fps			= 30,
	//.bv			= 0,
	.state			= SR030PC50_STATE_PREVIEW,
	.mode			= SR030PC50_MODE_CAMERA,
	.preview_size		= PREVIEW_SIZE_640_480,
	.capture_size		= CAPTURE_SIZE_640_480,
	.detect			= SENSOR_NOT_DETECTED,
	.focus_mode		= SR030PC50_AF_SET_NORMAL,
	.effect			= EFFECT_OFF,
	.iso			= ISO_AUTO,
	.photometry		= METERING_CENTER,
	.ev			= EV_DEFAULT,
	//.wdr			= SR030PC50_WDR_OFF,
	.contrast		= CONTRAST_DEFAULT,
	.saturation		= SATURATION_DEFAULT,
	.sharpness		= SHARPNESS_DEFAULT,
	.wb			= WB_AUTO,
	//.isc 			= SR030PC50_ISC_STILL_OFF,
	.scene			= SCENE_OFF,
	.aewb			= AWB_AE_UNLOCK,
	//.antishake		= SR030PC50_ANTI_SHAKE_OFF,
	//.flash_capture	= SR030PC50_FLASH_CAPTURE_OFF,
	//.flash_movie		= SR030PC50_FLASH_MOVIE_OFF,
	.quality		= QUALITY_SUPERFINE, 
	//.zoom			= SR030PC50_ZOOM_1P00X,
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

extern struct sr030pc50_platform_data sr030pc50_platform_data0;

struct sr030pc50_format_struct;  /* coming later */
struct sr030pc50_info {
	struct sr030pc50_format_struct *fmt;  /* Current format */
	unsigned char sat;		/* Saturation value */
	int hue;			/* Hue value */
	struct v4l2_subdev subdev;
	int model;	/* V4L2_IDENT_xxx* codes from v4l2-chip-ident.h */
	u32 pixfmt;
	struct i2c_client *client;
	struct soc_camera_device icd;

};

#if 1
static int sr030pc50_read(struct i2c_client *c, u16 reg, u16 *value)
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

static int sr030pc50_write(struct i2c_client *c, u16 reg)
{
	u8 data[2];
	int ret = 0;
	data[0] = reg>>8;
	data[1] = reg&0xFF;

	ret = i2c_master_send(c, data, 2);
	return (ret < 0) ? ret: 0;
}

#endif

/*For HENDRIX tunning */
#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

static char *sr030pc50_regs_table = NULL;

static int sr030pc50_regs_table_size;

static int sr030pc50_tuning_enable=0;

static int sr030pc50_regs_table_init(void)
{
	struct file *filp;

	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();

	printk("***** %s %d\n", __func__, __LINE__);

	set_fs(get_ds());

	filp = filp_open("/storage/extSdCard/sr030pc50_regs.h", O_RDONLY, 0);
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

	sr030pc50_regs_table = dp;
	sr030pc50_regs_table_size = l;
	*((sr030pc50_regs_table + sr030pc50_regs_table_size) - 1) = '\0';

	printk("*****Compeleted %s %d\n", __func__, __LINE__);
	return 0;
}

void sr030pc50_regs_table_exit(void)
{
	/* release allocated memory when exit preview */
	if (sr030pc50_regs_table) {
		kfree(sr030pc50_regs_table);
		sr030pc50_regs_table = NULL;
		sr030pc50_regs_table_size = 0;
	}
	else
		printk("*****sr030pc50_regs_table is already null\n");

	printk("*****%s done\n", __func__);

}

static int sr030pc50_regs_table_write(struct i2c_client *c, char *name)
{
	char *start, *end, *reg;
	unsigned short addr;
	unsigned int count = 0;
	int error=0;
	char reg_buf[7];

	printk(KERN_DEBUG "%s start, name = %s\n",__func__, name);

	*(reg_buf + 6) = '\0';

	start = strstr(sr030pc50_regs_table, name);
	end = strstr(start, "};");
	printk(KERN_DEBUG "start = %s, end = %s",start, end);


	while (1) {
		/* Find Address */	
		reg = strstr(start,"0x");
		if ((reg == NULL) || (reg > end))
		{
			break;
		}

		if (reg)
			start = (reg + 8);

		/* Write Value to Address */
		if (reg != NULL) {
			memcpy(reg_buf, reg, 6);
			addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16);
#if 1
			if (((addr&0xff00)>>8) == 0xff)
			{
				msleep(addr&0xff);
				printk("delay 0x%04x,\n", addr&0xff);
			}
			else
#endif
			{
				error = sr030pc50_write(c,addr);
				if( error < 0 )
				{
					printk("<=PCAM=> %s fail on sensor_write\n", __func__);
					break;
				}
			}

		}
		printk(KERN_DEBUG "count = %d, %s end, line = %d\n", count, __func__, __LINE__);
		count++;
	}
	printk(KERN_ERR "***** Writing [%s] Ended\n",name);
	return 0;	


}

/* From Palladio which reads "//#  define  MAX_VALUE 0x0A20 "*/
static short sr030pc50_regs_max_value(char *name)
{

	char *start, *reg;	
	unsigned short value;
	char data_buf[7];

	*(data_buf + 6) = '\0';

	start = strstr(sr030pc50_regs_table, name);

	/* Find Address */	
	reg = strstr(start," 0x");
	if (reg == NULL)
		return 0;

	/* Write Value to Address */
	if (reg != NULL) {
		memcpy(data_buf, (reg + 1), 6);
		value = (unsigned short)simple_strtoul(data_buf, NULL, 16); /*Change To HEX value*/
	}

	printk("sr030pc50_regs_max_value done\n");

	return value;

}
#endif  /* CONFIG_LOAD_FILE */

/**
 * sr030pc50_i2c_read_multi: Read (I2C) multiple bytes to the camera sensor
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
 * sr030pc50_i2c_read: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @data: data to be read
 *
 * Returns 0 on success, <0 on error
 */
static int sr030pc50_i2c_read( struct i2c_client *client, unsigned char subaddr, unsigned char *data)
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
 * sr030pc50_i2c_write_multi: Write (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static int sr030pc50_i2c_write_multi( struct i2c_client *client, unsigned short addr, unsigned int w_data)
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


static int32_t sr030pc50_i2c_write_16bit( struct i2c_client *client, u16 packet)
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

static int sr030pc50_i2c_wrt_list( struct i2c_client *client, const u16 *regs,
		int size, char *name)
{
	// tuning test
	//#ifdef CONFIG_LOAD_FILE
	//	sr030pc50_write_regs_from_sd(name);
	//#else

	int i, ret;
	u8 m_delay = 0;

	u16 temp_packet;

#ifdef CONFIG_LOAD_FILE
	if (sr030pc50_tuning_enable==1){
		printk(KERN_NOTICE "======[Length %d : Line %d]====== \n", size, __LINE__);
		ret = sr030pc50_regs_table_write(client, name);
	}
	else
	{
#endif

		CAM_DEBUG("%s, size=%d", name, size);
		for (i = 0; i < size; i++) {
			temp_packet = regs[i];

			if ((temp_packet & SR030PC50_DELAY) == SR030PC50_DELAY) {
				m_delay = temp_packet & 0xFF;
				printk("delay = %d", m_delay*10);
				msleep(m_delay*10);/*step is 10msec*/
				continue;
			}

			if (sr030pc50_i2c_write_16bit(client,temp_packet) < 0) {
				printk("fail(0x%x, 0x%x:%d)",
						client->addr, temp_packet, i);
				return -EIO;
			}
			/*udelay(10);*/
		}
#ifdef CONFIG_LOAD_FILE
	}
#endif
	return 0;
}



static int sr030pc50_detect(struct i2c_client *client)
{
	//int ret;
	//struct sr030pc50_sensor *sensor = &sr030pc50;
	unsigned char ID = 0xFFFF;

	sr030pc50_i2c_read(client, 0x04, &ID);

	if(ID == 0xB8)
	{
		printk(SR030PC50_MOD_NAME"========================================\n");
		printk(SR030PC50_MOD_NAME"   [VGA CAM] vendor_id ID : 0x%04X\n", ID);
		printk(SR030PC50_MOD_NAME"========================================\n");
	}
	else
	{
		printk(SR030PC50_MOD_NAME"-------------------------------------------------\n");
		printk(SR030PC50_MOD_NAME"   [VGA CAM] sensor detect failure !!\n");
		printk(SR030PC50_MOD_NAME"   ID : 0x%04X[ID should be 0xB8]\n", ID);
		printk(SR030PC50_MOD_NAME"-------------------------------------------------\n");
		return -EINVAL;
	}

	return 0;
}





static void sr030pc50_reset(struct i2c_client *client)
{
	msleep(1);
}

static int sr030pc50_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct sr030pc50_sensor *sensor = &sr030pc50;
	int result =0;

	Cam_Printk(KERN_NOTICE "===sr030pc50_init entered===[%s  %d]====== \n", __FUNCTION__, __LINE__);

#ifdef CONFIG_LOAD_FILE
	result = sr030pc50_regs_table_init();
	if (result > 0)
	{
		Cam_Printk(KERN_ERR "***** sr030pc50_regs_table_init  FAILED. Check the Filie in MMC\n");
		sr030pc50_tuning_enable=0;
		//return result;
	}
	else{
		sr030pc50_tuning_enable=1;
	}
	result =0;
#endif

	sr030pc50_WRT_LIST(c,sr030pc50_Init_Reg);
	Cam_Printk(KERN_NOTICE "**********sr030pc50_Init_Reg*********[%s  %d]*******\n", __FUNCTION__, __LINE__);



	sensor->state		= SR030PC50_STATE_PREVIEW;
	sensor->mode		= SR030PC50_MODE_CAMERA;
	sensor->effect		= EFFECT_OFF;
	sensor->iso		= ISO_AUTO;
	sensor->photometry	= METERING_CENTER;
	sensor->ev		= EV_DEFAULT;
	sensor->contrast	= CONTRAST_DEFAULT;
	sensor->saturation	= SATURATION_DEFAULT;
	sensor->sharpness	= SHARPNESS_DEFAULT;
	sensor->wb		= WB_AUTO;
	sensor->scene		= SCENE_OFF;
	sensor->quality		= QUALITY_SUPERFINE;
	sensor->fps			= FPS_auto;
	sensor->pix.width		=VGA_WIDTH;
	sensor->pix.height		=VGA_HEIGHT;
	sensor->pix.pixelformat = V4L2_PIX_FMT_YUV420;
	sensor->initial			= SR030PC50_STATE_INITIAL;

	Cam_Printk(KERN_NOTICE "===sr030pc50_init complete===[%s  %d]====== \n", __FUNCTION__, __LINE__);

	return result;
}


/*
 * Store information about the video data format.  The color matrix
 * is deeply tied into the format, so keep the relevant values here.
 * The magic matrix nubmers come from OmniVision.
 */
static struct sr030pc50_format_struct {
	__u8 *desc;
	__u32 pixelformat;
	int bpp;   /* bits per pixel */
} sr030pc50_formats[] = {
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
#define N_SR030PC50_FMTS ARRAY_SIZE(sr030pc50_formats)
/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sr030pc50_win_size {
	int	width;
	int	height;
} sr030pc50_win_sizes[] = {
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

static struct sr030pc50_win_size  sr030pc50_win_sizes_jpeg[] = {

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
static const struct sr030pc50_datafmt *sr030pc50_find_datafmt(
		enum v4l2_mbus_pixelcode code, const struct sr030pc50_datafmt *fmt,
		int n)
{
	int i;
	for (i = 0; i < n; i++)
		if (fmt[i].code == code)
			return fmt + i;

	return NULL;
}

#define N_WIN_SIZES (ARRAY_SIZE(sr030pc50_win_sizes))
/*
 * Store a set of start/stop values into the camera.
 */
static int sr030pc50_set_hw(struct i2c_client *client, int hstart, int hstop,
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

static int sr030pc50_querycap(struct i2c_client *c, struct v4l2_capability *argp)
{
	if(!argp){
		printk(KERN_ERR" argp is NULL %s %d \n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}
	strcpy(argp->driver, "sr030pc50");
	strcpy(argp->card, "TD/TTC");
	return 0;
}

static int sr030pc50_enum_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(sr030pc50_colour_fmts))
		return -EINVAL;
	*code = sr030pc50_colour_fmts[index].code;
	return 0;
}

static int sr030pc50_enum_fsizes(struct v4l2_subdev *sd,
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
			if (fsize->index >= ARRAY_SIZE(sr030pc50_win_sizes)) {
				dev_warn(&client->dev,
						"sr030pc50 unsupported size %d!\n", fsize->index);
				return -EINVAL;
			}
			fsize->discrete.height = sr030pc50_win_sizes[fsize->index].height;
			fsize->discrete.width = sr030pc50_win_sizes[fsize->index].width;
			break;
#if 0
		case V4L2_MBUS_FMT_JPEG_1X8:
			if (fsize->index >= ARRAY_SIZE(sr030pc50_win_sizes_jpeg)) {
				dev_warn(&client->dev,
						"sr030pc50 unsupported jpeg size %d!\n",
						fsize->index);
				return -EINVAL;
			}
			fsize->discrete.height =
				sr030pc50_win_sizes_jpeg[fsize->index].height;
			fsize->discrete.width =
				sr030pc50_win_sizes_jpeg[fsize->index].width;
			break;
#endif
		default:
			dev_err(&client->dev, "sr030pc50 unsupported format!\n");
			return -EINVAL;
	}
	return 0;
}

static int sr030pc50_try_fmt(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *mf)
//static int sr030pc50_try_fmt(struct i2c_client *c, struct v4l2_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct sr030pc50_datafmt *fmt;
	int i;

	fmt = sr030pc50_find_datafmt(mf->code, sr030pc50_colour_fmts,
			ARRAY_SIZE(sr030pc50_colour_fmts));
	if (!fmt) {
		dev_err(&client->dev, "sr030pc50 unsupported color format!\n");
		return -EINVAL;
	}

	mf->field = V4L2_FIELD_NONE;

	switch (mf->code) {
		case V4L2_MBUS_FMT_UYVY8_2X8:
		case V4L2_MBUS_FMT_VYUY8_2X8:
			/* enum the supported sizes*/
			for (i = 0; i < ARRAY_SIZE(sr030pc50_win_sizes); i++)
				if (mf->width == sr030pc50_win_sizes[i].width
						&& mf->height == sr030pc50_win_sizes[i].height)
					break;

			if (i >= ARRAY_SIZE(sr030pc50_win_sizes)) {
				dev_err(&client->dev, "sr030pc50 unsupported window"
						"size, w%d, h%d!\n", mf->width, mf->height);
				return -EINVAL;
			}
			mf->colorspace = V4L2_COLORSPACE_JPEG;
			break;
#if 0
		case V4L2_MBUS_FMT_JPEG_1X8:
			/* enum the supported sizes for JPEG*/
			for (i = 0; i < ARRAY_SIZE(sr030pc50_win_sizes_jpeg); i++)
				if (mf->width == sr030pc50_win_sizes_jpeg[i].width &&
						mf->height == sr030pc50_win_sizes_jpeg[i].height)
					break;

			if (i >= ARRAY_SIZE(sr030pc50_win_sizes_jpeg)) {
				dev_err(&client->dev, "sr030pc50 unsupported jpeg size!\n");
				return -EINVAL;
			}
			mf->colorspace = V4L2_COLORSPACE_JPEG;
			break;
#endif

		default:
			dev_err(&client->dev, "sr030pc50 doesn't support code"
					"%d\n", mf->code);
			break;
	}
	return 0;
}


/*
 * Set a format.
 */

static int sr030pc50_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct sr030pc50_datafmt *fmt;
	struct sr030pc50_sensor *sensor = &sr030pc50;

	//	struct regval_list *pregs = NULL;
	//	struct regval_list *pregs_default = NULL;

	printk("[DHL]sr030pc50_s_fmt..!!! \n");
	printk("[DHL]mf->code : [%d] \n",mf->code);
	printk("[DHL]mf->width : [%d] \n",mf->width);



	fmt =sr030pc50_find_datafmt(mf->code,sr030pc50_colour_fmts,
			ARRAY_SIZE(sr030pc50_colour_fmts));
	if (!fmt) {
		dev_err(&client->dev, "sr030pc50 unsupported color format!\n");
		return -EINVAL;
	}

	switch (mf->code) {
		case V4L2_MBUS_FMT_UYVY8_2X8:
		case V4L2_MBUS_FMT_VYUY8_2X8:
			sensor->pix.pixelformat = V4L2_PIX_FMT_YUV422P;
			if(sr030pc50_cam_state == SR030PC50_STATE_PREVIEW)
			{
				switch (mf->width)
				{
					/* for YUV preview */

					case VGA_WIDTH:
						sr030pc50_WRT_LIST(client,sr030pc50_640x480);
						Cam_Printk(KERN_ERR"choose vga Preview setting \n");
						break;

					case QVGA_WIDTH:
						sr030pc50_WRT_LIST(client,sr030pc50_320x240);
						Cam_Printk(KERN_ERR"choose qvga Preview setting \n");
						break;

					case QCIF_WIDTH:
						sr030pc50_WRT_LIST(client,sr030pc50_176x144);
						Cam_Printk(KERN_ERR"choose qcif Preview setting \n");
						break;

					default:
						printk("\n unsupported size for preview! %s %d w=%d h=%d\n", __FUNCTION__, __LINE__, mf->width, mf->height);
						goto out;
						break;
				}
			//	sr030pc50_WRT_LIST(client, sr030pc50_Init_Reg);
			}
			else if (sr030pc50_cam_state == SR030PC50_STATE_CAPTURE)
			{
				Cam_Printk(KERN_NOTICE "Start to yuv format Capture \n");
				//sr030pc50_WRT_LIST(client, sr030pc50_Capture);
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
static int sr030pc50_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	//	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_captureparm *cp = &parms->parm.capture;
	//	u16 clkrc;
	//	int ret;

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	//ret = sr030pc50_read(c, REG_CLKRC, &clkrc);
	//if (ret < 0)
	//	return ret;
	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.numerator = 1;
	cp->timeperframe.denominator = SR030PC50_FRAME_RATE;
	//if ((clkrc & CLK_EXT) == 0 && (clkrc & CLK_SCALE) > 1)
	//	cp->timeperframe.denominator /= (clkrc & CLK_SCALE);
	return 0;
}

static int sr030pc50_s_parm(struct i2c_client *c, struct v4l2_streamparm *parms)
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
static const int sr030pc50_sin_table[] = {
	0,	 87,   173,   258,   342,   422,
	499,	573,   642,   707,   766,   819,
	866,	906,   939,   965,   984,   996,
	1000
};

static int sr030pc50_sine(int theta)
{
	int chs = 1;
	int sine;

	if (theta < 0) {
		theta = -theta;
		chs = -1;
	}
	if (theta <= 90)
		sine = sr030pc50_sin_table[theta/SIN_STEP];
	else {
		theta -= 90;
		sine = 1000 - sr030pc50_sin_table[theta/SIN_STEP];
	}
	return sine*chs;
}

static int sr030pc50_cosine(int theta)
{
	theta = 90 - theta;
	if (theta > 180)
		theta -= 360;
	else if (theta < -180)
		theta += 360;
	return sr030pc50_sine(theta);
}

static int sr030pc50_calc_cmatrix(struct sr030pc50_info *info,
		int matrix[CMATRIX_LEN])
{
	return 0;
}

static int sr030pc50_t_saturation(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
	s32 old_value = (s32)sensor->saturation;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case SATURATION_MINUS_2:
			//sr030pc50_write_regs(client, regs_sr030pc50_saturation_level_0, ARRAY_SIZE(regs_sr030pc50_saturation_level_0),"regs_sr030pc50_saturation_level_0");			
			break;

		case SATURATION_MINUS_1:
			//sr030pc50_write_regs(client, regs_sr030pc50_saturation_level_1, ARRAY_SIZE(regs_sr030pc50_saturation_level_1),"regs_sr030pc50_saturation_level_1");			
			break;		

		case SATURATION_DEFAULT:
			//sr030pc50_write_regs(client, regs_sr030pc50_saturation_level_2, ARRAY_SIZE(regs_sr030pc50_saturation_level_2),"regs_sr030pc50_saturation_level_2");			
			break;	

		case SATURATION_PLUS_1:
			//sr030pc50_write_regs(client, regs_sr030pc50_saturation_level_3, ARRAY_SIZE(regs_sr030pc50_saturation_level_3),"regs_sr030pc50_saturation_level_3");			
			break;		

		case SATURATION_PLUS_2:
			//sr030pc50_write_regs(client, regs_sr030pc50_saturation_level_4, ARRAY_SIZE(regs_sr030pc50_saturation_level_4),"regs_sr030pc50_saturation_level_4");			
			break;	

		default:
			printk(SR030PC50_MOD_NAME "quality value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->saturation = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->saturation);
	return 0;
}

static int sr030pc50_q_saturation(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_q_saturation is called...\n"); 
	value = sensor->saturation;
	return 0;
}


static int sr030pc50_t_hue(struct i2c_client *client, int value)
{
	return 0;
}


static int sr030pc50_q_hue(struct i2c_client *client, __s32 *value)
{
	return 0;
}


/*
 * Some weird registers seem to store values in a sign/magnitude format!
 */
static unsigned char sr030pc50_sm_to_abs(unsigned char v)
{
	if ((v & 0x80) == 0)
		return v + 128;
	else
		return 128 - (v & 0x7f);
}


static unsigned char sr030pc50_abs_to_sm(unsigned char v)
{
	if (v > 127)
		return v & 0x7f;
	else
		return (128 - v) | 0x80;
}

static int sr030pc50_t_brightness(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
	s32 old_value = (s32)sensor->ev;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case EV_MINUS_4:
			sr030pc50_WRT_LIST(client,sr030pc50_brightness_M4);
			Cam_Printk(KERN_NOTICE "**********sr030pc50_brightness_M4*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
			break;

		case EV_MINUS_3:
			sr030pc50_WRT_LIST(client,sr030pc50_brightness_M3);
			Cam_Printk(KERN_NOTICE "**********sr030pc50_brightness_M3*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
			break;

		case EV_MINUS_2:
			sr030pc50_WRT_LIST(client,sr030pc50_brightness_M2);
			Cam_Printk(KERN_NOTICE "**********sr030pc50_brightness_M2*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
			break;

		case EV_MINUS_1:
			sr030pc50_WRT_LIST(client,sr030pc50_brightness_M1);
			Cam_Printk(KERN_NOTICE "**********sr030pc50_brightness_M1*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
			break;

		case EV_DEFAULT:
			sr030pc50_WRT_LIST(client,sr030pc50_brightness_default);
			Cam_Printk(KERN_NOTICE "**********sr030pc50_brightness_default*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
			break;

		case EV_PLUS_1:
			sr030pc50_WRT_LIST(client,sr030pc50_brightness_P1);
			Cam_Printk(KERN_NOTICE "**********sr030pc50_brightness_P1*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
			break;

		case EV_PLUS_2:
			sr030pc50_WRT_LIST(client,sr030pc50_brightness_P2);
			Cam_Printk(KERN_NOTICE "**********sr030pc50_brightness_P2*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
			break;

		case EV_PLUS_3:
			sr030pc50_WRT_LIST(client,sr030pc50_brightness_P3);
			Cam_Printk(KERN_NOTICE "**********sr030pc50_brightness_P3*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
			break;

		case EV_PLUS_4:
			sr030pc50_WRT_LIST(client,sr030pc50_brightness_P4);
			Cam_Printk(KERN_NOTICE "**********sr030pc50_brightness_P4*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
			break;

		default:
			printk(SR030PC50_MOD_NAME "Scene value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->ev = value;
	Cam_Printk(KERN_NOTICE "%s success [scene:%d]\n",__func__, sensor->scene);
	return 0;
}

static int sr030pc50_q_brightness(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_get_scene is called...\n"); 
	value = sensor->ev;
	return 0;
}

static int sr030pc50_t_contrast(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
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
			printk(SR030PC50_MOD_NAME "quality value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->contrast = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->quality);
	return 0;
}

static int sr030pc50_q_contrast(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_q_quality is called...\n"); 
	value = sensor->contrast;
	return 0;
}

static int sr030pc50_q_hflip(struct i2c_client *client, __s32 *value)
{
	return 0;
}


static int sr030pc50_t_hflip(struct i2c_client *client, int value)
{
	return 0;
}



static int sr030pc50_q_vflip(struct i2c_client *client, __s32 *value)
{
	return 0;
}


static int sr030pc50_t_vflip(struct i2c_client *client, int value)
{
	return 0;
}

static int sr030pc50_t_scene(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
	s32 old_value = (s32)sensor->scene;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	if(value != SCENE_OFF){
		Cam_Printk(KERN_NOTICE "regs_sr030pc50_scene_off");
	}
	switch(value)
	{
		case SCENE_OFF:
			break;
		case SCENE_PORTRAIT:
			break;		
		case SCENE_LANDSCAPE:
			break;	
		case SCENE_SPORTS:
			break;			
		case SCENE_PARTY:
			break;
		case SCENE_BEACH:
			break;
		case SCENE_SUNSET:
			break;
		case SCENE_DAWN:
			break;
		case SCENE_FALL:
			break;
		case SCENE_NIGHT:
			break;
		case SCENE_BACKLIGHT:
			break;
		case SCENE_FIRE:
			break;
		case SCENE_TEXT:
			break;
		case SCENE_CANDLE:
			break;
		default:
			printk(SR030PC50_MOD_NAME "Scene value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->scene = value;
	Cam_Printk(KERN_NOTICE "%s success [scene:%d]\n",__func__, sensor->scene);
	return 0;
}

static int sr030pc50_q_scene(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_get_scene is called...\n"); 
	value = sensor->scene;
	return 0;
}

static int sr030pc50_t_whitebalance(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
	s32 old_value = (s32)sensor->wb;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case WB_AUTO:
			break;

		case WB_DAYLIGHT:
			break;

		case WB_CLOUDY:
			break;

		case WB_FLUORESCENT:
			break;

		case WB_INCANDESCENT:
			break;

		default:
			printk(SR030PC50_MOD_NAME "White Balance value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->wb = value;
	Cam_Printk(KERN_NOTICE "%s success [White Balance e:%d]\n",__func__, sensor->wb);
	return 0;
}

static int sr030pc50_q_whitebalance(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_get_whitebalance is called...\n"); 
	value = sensor->wb;
	return 0;
}

static int sr030pc50_t_effect(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
	s32 old_value = (s32)sensor->effect;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case EFFECT_OFF:
			break;

		case EFFECT_MONO:
			break;

		case EFFECT_SEPIA:
			break;

		case EFFECT_NEGATIVE:
			break;

		case EFFECT_AQUA:
			break;

		case EFFECT_SKETCH:
			break;

		default:
			printk(SR030PC50_MOD_NAME "Sketch value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->effect = value;
	Cam_Printk(KERN_NOTICE "%s success [Effect e:%d]\n",__func__, sensor->effect);
	return 0;
}

static int sr030pc50_q_effect(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_get_whitebalance is called...\n"); 
	value = sensor->effect;
	return 0;
}

static int sr030pc50_t_ISO(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
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
			printk(SR030PC50_MOD_NAME "ISO value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->iso = value;
	Cam_Printk(KERN_NOTICE "%s success [ISO e:%d]\n",__func__, sensor->iso);
	return 0;
}

static int sr030pc50_q_ISO(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_q_ISO is called...\n"); 
	value = sensor->iso;
	return 0;
}

static int sr030pc50_t_photometry(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
	s32 old_value = (s32)sensor->photometry;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case METERING_MATRIX:
			break;

		case METERING_SPOT:
			break;		

		case METERING_CENTER:
			break;	

		default:
			printk(SR030PC50_MOD_NAME "ISO value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->photometry = value;
	Cam_Printk(KERN_NOTICE "%s success [PHOTOMERTY e:%d]\n",__func__, sensor->photometry);
	return 0;
}

static int sr030pc50_q_photometry(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_q_photometry is called...\n"); 
	value = sensor->photometry;
	return 0;
}

static int sr030pc50_t_quality(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
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
			printk(SR030PC50_MOD_NAME "quality value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->quality = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->quality);
	return 0;
}

static int sr030pc50_q_quality(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_q_quality is called...\n"); 
	value = sensor->quality;
	return 0;
}


static int sr030pc50_t_sharpness(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
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
			printk(SR030PC50_MOD_NAME "quality value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->sharpness = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->saturation);
	return 0;
}

static int sr030pc50_q_sharpness(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_q_sharpness is called...\n"); 
	value = sensor->sharpness;
	return 0;
}

static int sr030pc50_t_fps(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
	s32 old_value = (s32)sensor->fps;

	//tagCamReg32_t    *pregs = NULL;
	//u32 regs_length = 0;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
#if 0
	if(old_value == value){
		Cam_Printk(KERN_NOTICE "%s new value is same as existing value\n", __func__);
		return 0;
	}
#endif
	switch(value)
	{
		case FPS_auto:
		case FPS_7:
		case FPS_10:
		case FPS_12:
			Cam_Printk(KERN_NOTICE "FPS_Auto\n");
			sr030pc50_WRT_LIST(client,sr030pc50_fps_Auto);
			break;

		case FPS_15:
			Cam_Printk(KERN_NOTICE "FPS_15\n");
			sr030pc50_WRT_LIST(client,sr030pc50_15_fps_60Hz);
			break;

		case FPS_20:
		case FPS_25:
		case FPS_30:
			Cam_Printk(KERN_NOTICE "FPS_24\n");
			sr030pc50_WRT_LIST(client,sr030pc50_24_fps_60Hz);
			break;

		default:
			printk(KERN_NOTICE "quality value is not supported!!!\n");
			return -EINVAL;
	}

	sensor->fps = value;
	Cam_Printk(KERN_NOTICE "%s success [FPS e:%d]\n",__func__, sensor->fps);
	return 0;
}

static int sr030pc50_q_fps(struct i2c_client *client, __s32 *value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_q_fps is called...\n"); 
	value = sensor->fps;
	return 0;
}

static int sr030pc50_g_frame_time(struct i2c_client *client,__s32 *value)
{
	//struct sr030pc50_sensor *sensor = &sr030pc50;
	//u16 frame_time =0;
	//u16 temp1 = 0;
	//int err;

	Cam_Printk(KERN_NOTICE "[DHL:Test] sr030pc50_g_frame_time() \r\n");

	return 0;
}

static int sr030pc50_mode_switch_check(struct i2c_client *client,__s32 *value)
{
	//struct sr030pc50_sensor *sensor = &sr030pc50;
	u16 frame_time =0;
	u16 temp1 = 0;
	int err;

	Cam_Printk(KERN_NOTICE "[DHL:Test] sr030pc50_mode_switch_check() \r\n");



	return 0;
}


static int sr030pc50_get_frame_time(struct i2c_client *client)
{
	//struct sr030pc50_sensor *sensor = &sr030pc50;
	u16 frame_time =0;
	u16 temp1 = 0;
	int err;

	Cam_Printk(KERN_NOTICE "[DHL:Test] sr030pc50_g_frame_time() \r\n");

	return frame_time;
}

static int sr030pc50_g_lightness_check(struct i2c_client *client,__s32 *value)
{
	//struct sr030pc50_sensor *sensor = &sr030pc50;
	//u16 frame_time =0;
	u16 temp1,temp2,temp3;
	int err;

	Cam_Printk(KERN_NOTICE "sr030pc50_g_frame_check() \r\n");

	return 0;

}

static int sr030pc50_ESD_check(struct i2c_client *client, struct v4l2_control *ctrl)
{
	//struct sr030pc50_sensor *sensor = &sr030pc50;
	u16 esd_value1 = 0;
	u16 esd_value2 = 0;
	u16 esd_value3 = 0;
	int err;

	Cam_Printk(KERN_NOTICE "sr030pc50_ESD_check() \r\n");

	err=sr030pc50_i2c_write_16bit(client, 0x0300);

	err=sr030pc50_i2c_read(client,0x0b, &esd_value1);
	err=sr030pc50_i2c_read(client,0x0c, &esd_value2);
	err=sr030pc50_i2c_read(client,0x0d, &esd_value3);

	Cam_Printk(KERN_NOTICE "[DHL]esd_value1 : %d \r\n",esd_value1);
	Cam_Printk(KERN_NOTICE "[DHL]esd_value2 : %d \r\n",esd_value2);
	Cam_Printk(KERN_NOTICE "[DHL]esd_value3 : %d \r\n",esd_value3);

	if((esd_value1 != 0xaa)||(esd_value2 != 0xaa)||(esd_value3 != 0xaa))
	{
		Cam_Printk(KERN_NOTICE "ESD Error is Happened..() \r\n");
		ctrl->value = ESD_ERROR;
		return 0;
	}

	Cam_Printk(KERN_NOTICE "sr030pc50_ESD_check is OK..() \r\n");

	ctrl->value = ESD_NONE;
	return 0;

}
static int sr030pc50_t_dtp_on(struct i2c_client *client)
{
	Cam_Printk(KERN_NOTICE "sr030pc50_t_dtp_stop is called...\n"); 

	return 0;
}

static int sr030pc50_t_dtp_stop(struct i2c_client *client)
{
	Cam_Printk(KERN_NOTICE "sr030pc50_t_dtp_stop is called...\n"); 

	return 0;
}

static int sr030pc50_g_exif_info(struct i2c_client *client,struct v4l2_exif_info *exif_info)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "sr030pc50_g_exif_info is called...\n"); 
	*exif_info = sensor->exif_info;
	return 0;
}

static int sr030pc50_set_mode(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	sensor->mode = value;
	Cam_Printk(KERN_NOTICE, "sr030pc50_set_mode is called... mode = %d\n", sensor->mode);
	return 0;
}


static int sr030pc50_preview_size(struct i2c_client *client, int value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE, "sr030pc50_preview_size is called... mode = %d\n", sensor->mode);

	if(sensor->mode != SR030PC50_MODE_CAMCORDER)
	{
		switch (value)
		{
			Cam_Printk(KERN_NOTICE "CAMERA MODE..\n");

			case SR030PC50_PREVIEW_SIZE_176_144:
				sr030pc50_WRT_LIST(client,sr030pc50_176x144);
				Cam_Printk(KERN_ERR"choose qvga Preview setting \n");
				break;

			case SR030PC50_PREVIEW_SIZE_320_240:
				sr030pc50_WRT_LIST(client,sr030pc50_320x240);
				Cam_Printk(KERN_ERR"choose qvga Preview setting \n");
				break;

			case SR030PC50_PREVIEW_SIZE_640_480:
				sr030pc50_WRT_LIST(client,sr030pc50_640x480);
				Cam_Printk(KERN_ERR"choose vga Preview setting \n");
				break;

			default:
			// When running in image capture mode, the call comes here.
			// Set the default video resolution - SR030PC50_PREVIEW_VGA
			printk(SR030PC50_MOD_NAME "Preview Resolution is not supported! : %d\n",value);
			return 0;
		}
	}
	else
	{
		Cam_Printk(KERN_NOTICE "CAMCORDER MODE..\n");

		switch (value) {

			case SR030PC50_PREVIEW_SIZE_176_144:
				sr030pc50_WRT_LIST(client,sr030pc50_176x144);
				Cam_Printk(KERN_ERR"choose qvga Preview setting \n");
				break;

			case SR030PC50_PREVIEW_SIZE_320_240:
				sr030pc50_WRT_LIST(client,sr030pc50_320x240);
				Cam_Printk(KERN_ERR"choose qvga Preview setting \n");
				break;

			case SR030PC50_PREVIEW_SIZE_640_480:
				sr030pc50_WRT_LIST(client,sr030pc50_640x480);
				Cam_Printk(KERN_ERR"choose vga Preview setting \n");
				break;


			default:
				// When running in image capture mode, the call comes here.
				// Set the default video resolution - SR030PC50_PREVIEW_VGA
				printk(SR030PC50_MOD_NAME "Preview Resolution is not supported! : %d\n",value);
				return 0;
		}
	}
	return 0;
}

static const struct v4l2_queryctrl sr030pc50_controls[] = {
	{
		.id		= V4L2_CID_PRIVATE_GET_MIPI_PHY,
		.type		= V4L2_CTRL_TYPE_CTRL_CLASS,
		.name		= "get mipi phy config"
	},
};


static int sr030pc50_set_still_status(void)
{
	Cam_Printk(KERN_NOTICE "[DHL]sr030pc50_set_still_status.. \n");

	sr030pc50_cam_state = SR030PC50_STATE_CAPTURE;	

	return 0;
}

static int sr030pc50_set_preview_status(void)
{
	Cam_Printk(KERN_NOTICE "[DHL]sr030pc50_set_preview_status.. \n");

	sr030pc50_cam_state = SR030PC50_STATE_PREVIEW;	

	return 0;
}

static int sr030pc50_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int retval = 0;

	Cam_Printk(KERN_NOTICE "ioctl_g_ctrl is called...(%d)\n", ctrl->id);

	switch (ctrl->id) {
		case V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL:
		{
			struct sr030pc50_sensor *sensor = &sr030pc50;
			printk( "[DHL]V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL.. \n");
			ctrl->value = (__s32)sensor->exif_info.exposure_time.denominal;
			break;
		}
		case V4L2_CID_GET_EXIF_ISO_SPEED:
		{
			struct sr030pc50_sensor *sensor = &sr030pc50;
			printk( "[DHL]V4L2_CID_GET_EXIF_ISO_SPEED.. \n");
			ctrl->value = (__s32)sensor->exif_info.iso_speed_rationg;
			break;
		}
		case V4L2_CID_ESD_CHECK:
		{
			struct v4l2_control ESDCtrl;
			printk( "[DHL]V4L2_CID_ESD_CHECK.. \n");
			sr030pc50_ESD_check(client, &ESDCtrl);
			ctrl->value = (__s64)ESDCtrl.value;
			break;
		}
		default:
			return -EINVAL;
	}

	return retval;
}
static int sr030pc50_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
//	struct sr030pc50_sensor *sensor = &sr030pc50;
	int retval = 0;

	Cam_Printk(KERN_NOTICE "sr030pc50_s_ctrl is called...(%d, %s)\n", ctrl->id, ctrl->id);

	switch (ctrl->id)
	{
		case V4L2_CID_ISO:
			Cam_Printk(KERN_NOTICE "V4L2_CID_ISO");
			retval = sr030pc50_t_ISO(client,ctrl->value);
			break;
		case V4L2_CID_BRIGHTNESS:
			Cam_Printk(KERN_NOTICE "V4L2_CID_BRIGHTNESS");
			retval = sr030pc50_t_brightness(client,ctrl->value);
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			Cam_Printk(KERN_NOTICE "V4L2_CID_DO_WHITE_BALANCE");
			retval = sr030pc50_t_whitebalance(client,ctrl->value);
			break;
		case V4L2_CID_EFFECT:
			Cam_Printk(KERN_NOTICE "V4L2_CID_EFFECT");
			retval = sr030pc50_t_effect(client,ctrl->value);
			break;
		case V4L2_CID_CONTRAST:
			Cam_Printk(KERN_NOTICE "V4L2_CID_CONTRAST");
			retval = sr030pc50_t_contrast(client,ctrl->value);
			break;
		case V4L2_CID_SATURATION:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SATURATION");
			retval = sr030pc50_t_saturation(client,ctrl->value);
			break;
		case V4L2_CID_SHARPNESS:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SHARPNESS");
			retval = sr030pc50_t_sharpness(client,ctrl->value);
			break;
		case V4L2_CID_SCENE:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SCENE");
			retval = sr030pc50_t_scene(client,ctrl->value);
			break;
		case V4L2_CID_PHOTOMETRY:
			Cam_Printk(KERN_NOTICE "V4L2_CID_PHOTOMETRY");
			retval = sr030pc50_t_photometry(client,ctrl->value);
			break;
#if 0
		case V4L2_CID_QUALITY:
			Cam_Printk(KERN_NOTICE "V4L2_CID_QUALITY");
			retval = sr030pc50_t_quality(client,ctrl->value);
			break;
#endif
		case V4L2_CID_FPS:
			Cam_Printk(KERN_NOTICE "V4L2_CID_FPS");
			retval = sr030pc50_t_fps(client,ctrl->value);
			break;
#if 0
		case V4L2_CID_CAMERA_CHECK_DATALINE:
			retval = sr030pc50_t_dtp_on(client);
			break;
		case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
			retval = sr030pc50_t_dtp_stop(client);
			break;
#endif
		case V4L2_CID_SELECT_MODE:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SELECT_MODE");
			retval = sr030pc50_set_mode(client,ctrl->value);
			break;
#if 0

		case V4L2_CID_CAMERA_PREVIEW_SIZE:
			retval = sr030pc50_preview_size(client,ctrl->value);
			break;
		case V4L2_CID_FOCUS_MODE_STEP1:
			retval = sr030pc50_set_focus_mode(client,ctrl->value, SET_FOCUS_STEP1);
			break;
		case V4L2_CID_FOCUS_MODE_STEP2:
			retval = sr030pc50_set_focus_mode(client,ctrl->value, SET_FOCUS_STEP2);
			break;
		case V4L2_CID_FOCUS_MODE_STEP3:
			retval = sr030pc50_set_focus_mode(client,ctrl->value, SET_FOCUS_STEP3);
			break;
		case V4L2_CID_FOCUS_MODE:
			retval = sr030pc50_set_focus_mode(client,ctrl->value, SET_FOCUS_ALL);
			break;
		case V4L2_CID_AF:
			retval = sr030pc50_set_focus(client,ctrl->value);
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
			retval = sr030pc50_set_focus_touch_position(client,ctrl->value);
			break;
		case V4L2_CID_AF_POSITION_STOP:
			retval = sr030pc50_set_focus_mode(client,ctrl->value, SET_FOCUS_ALL);
			break;
		case V4L2_CID_AE_LOCK:
			retval = sr030pc50_AE_lock(client,ctrl->value);
			break;
#endif
		case V4L2_CID_SET_STILL_STATUS:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SET_STILL_STATUS");
			retval = sr030pc50_set_still_status();
			break;
		case V4L2_CID_SET_PREVIEW_STATUS:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SET_PREVIEW_STATUS");
			retval = sr030pc50_set_preview_status();
			break;

		default:
			Cam_Printk(SR030PC50_MOD_NAME "[id]Invalid value is ordered!!!\n");
			break;
	}
	return retval;
}

/* Get chip identification */
static int sr030pc50_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *id)
{
	struct sr030pc50_info *priv = to_sr030pc50(sd);

	id->ident = priv->model;
	id->revision = 0x0;//priv->revision;

	return 0;
}

static int sr030pc50_set_bus_param(struct soc_camera_device *icd,
		unsigned long flags)
{
	return 0;
}

camera_light_status_type sr030pc50_check_illuminance_status(struct i2c_client *client);
int sr030pc50_s_exif_info(struct i2c_client *client);

int sr030pc50_streamon(struct i2c_client *client)
{

	struct sr030pc50_sensor *sensor = &sr030pc50;
	struct v4l2_pix_format* pix = &sensor->pix;
	volatile int checkvalue, timeout = 5;
//		camera_light_status_type illuminance=CAMERA_SENSOR_LIGHT_STATUS_NORMAL;

	Cam_Printk(KERN_NOTICE "sr030pc50_streamon is called...\n");  

	//if(pix->pixelformat == V4L2_PIX_FMT_JPEG)
	if(sr030pc50_cam_state == SR030PC50_STATE_CAPTURE)
	{
		msleep(200);
		Cam_Printk(KERN_NOTICE "capture: set stream-on setting\n");
		sr030pc50_WRT_LIST(client,sr030pc50_start_stream);
		sr030pc50_s_exif_info(client);
		sensor->state = SR030PC50_STATE_CAPTURE;
	}
	else
	{
		sr030pc50_WRT_LIST(client,sr030pc50_start_stream);
		Cam_Printk(KERN_NOTICE "**********sr030pc50_start_stream*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
	}

	return 0;
}


static int sr030pc50_streamoff(struct i2c_client *client)
{

	/* What's wrong with this sensor, it has no stream off function, oh!,Vincent.Wan */
	Cam_Printk(KERN_NOTICE " sr030pc50_sensor_stop_stream E");
	sr030pc50_WRT_LIST(client,sr030pc50_stop_stream);
	Cam_Printk(KERN_NOTICE "**********sr030pc50_stop_stream*********[%s  %d]*******\n", __FUNCTION__, __LINE__);
	Cam_Printk(KERN_NOTICE " sr030pc50_sensor_stop_stream X");
	return 0;
}
static int set_stream(struct i2c_client *client, int enable)
{
	int ret = 0;
	int st = 0;

	if (enable) {
		ret = sr030pc50_streamon(client);
		if (ret < 0)
			goto out;
	} else {
		ret = sr030pc50_streamoff(client);
	}
out:
	return ret;
}

static int sr030pc50_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	ret = set_stream(client, enable);
	if (ret < 0)
		dev_err(&client->dev, "sr030pc50 set stream error\n");
	return ret;
}

static int sr030pc50_video_probe(struct soc_camera_device *icd,
		struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sr030pc50_info *priv = to_sr030pc50(sd);
	u8 modelhi, modello;
	int ret, i;

	pr_info("=====================================================================================\n");
	/*
	 * Make sure it's an sr030pc50
	 */
	for(i =0;i<3;i++)
	{
		ret = sr030pc50_detect(client);
		if (!ret) {
			pr_info("=========siliconfile sr030pc50 sensor detected==========\n");
			goto out;
		}

	}

	priv->model = V4L2_IDENT_SR030PC50;
out:
	return ret;
}

static int sr030pc50_get_lux(struct i2c_client *client, int* lux)
{
	u16 lux_msb = 0;
	u16 lux_lsb = 0;
	int cur_lux = 0;


	printk("get_lux : %d lux\n", cur_lux);

	return cur_lux; //this value is under 0x0032 in low light condition 
}


camera_light_status_type sr030pc50_check_illuminance_status(struct i2c_client *client)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
	u32 lightStatus = 0;
	u16 lightStatus_low_word = 0;
	u16 lightStatus_high_word= 0;	
	int err;
	u32 luxcheck_low = 0x0032;

	Cam_Printk(KERN_NOTICE "sr030pc50_check_illuminance_status() \r\n");



	return 0;
}

int sr030pc50_s_exif_info(struct i2c_client *client)
{
	Cam_Printk(KERN_NOTICE "[DHL] EXIF Info.. \r\n");

	struct sr030pc50_sensor *sensor = &sr030pc50;
	u32 regData=0;
	u32 gainValue=0;
	u32 iso=0;
	u32 expA=0, expB=0, expC=0;
	u32 exp_time=0;
	int err;

	err=sr030pc50_i2c_write_16bit(client, 0x0320);

	err=sr030pc50_i2c_read(client,0x80, &expA);
	err=sr030pc50_i2c_read(client,0x81, &expB);
	err=sr030pc50_i2c_read(client,0x82, &expC);

	exp_time =((expA << 19) + (expB << 11) + (expC << 3)); // / 12;
	Cam_Printk(KERN_NOTICE "[DHL]exposure_time : %d \r\n",exp_time);
	exp_time = 12000000/exp_time;
	Cam_Printk(KERN_NOTICE "[DHL]exposure_time : %d \r\n",exp_time);

	sensor->exif_info.exposure_time.inumerator=1;
	sensor->exif_info.exposure_time.denominal=exp_time;

	err=sr030pc50_i2c_write_16bit(client,0x0320);
	err=sr030pc50_i2c_read(client,0xb0,&regData);

	gainValue = (regData*100/16);
	Cam_Printk(KERN_NOTICE "sr030pc50_s_ISO() %d\r\n",gainValue);

	// calculate ISO
	if(gainValue < 112)
		iso=50;
	else if(gainValue < 176)
		iso=100;
	else if(gainValue < 343)
		iso=200;
	else if(gainValue < 687)
		iso=400;
	else if(gainValue < 1381)
		iso=800;
	else
		iso=1600;
	
	Cam_Printk(KERN_NOTICE "sr030pc50_s_ISO() %d\r\n",iso);

	sensor->exif_info.iso_speed_rationg=iso;

	return 0;
}

static int sr030pc50_s_thumbnail_size(struct i2c_client *client, struct v4l2_pix_format *thumbnail)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
	struct v4l2_pix_format* pix = &sensor->thumbnail;
	pix->width= thumbnail->width;
	pix->height= thumbnail->height;
	int retval = 0;


	Cam_Printk(KERN_NOTICE "sr030pc50_s_thumbnail_size is called...(Width %d Height %d)\n",pix->width,pix->height);

	return retval;
}


static int sr030pc50_AE_AWB_Status(struct i2c_client *client,int *LuxCheck)
{
	u16 AE_Check =0;
	u16 AWB_Check=0;
	u32 lightStatus = CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
	u16 lightStatus_low_word = 0;
	u16 lightStatus_high_word= 0;	
	int err;
	u32 luxcheck_high = 0xFFFE;
	u32 luxcheck_low = 0x0080;	

	struct sr030pc50_sensor *sensor = &sr030pc50;

	//illuminance = sr030pc50_check_illuminance_status(client);

	Cam_Printk(KERN_NOTICE "sr030pc50_check_illuminance_status() \r\n");

	Cam_Printk(KERN_NOTICE "*LuxCheck %d .. \n",*LuxCheck);
	return 0;
}

//#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sr030pc50_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register * reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int retval = 0;

	Cam_Printk(KERN_NOTICE "ioctl_g_register is called...(%d)\n", reg->reg);


	switch (reg->reg) {
		case V4L2_CID_AF_STATUS:
		{
			printk( "[DHL]V4L2_CID_AF_STATUS.. \n");
			break;
		}
		case V4L2_CID_AF_2nd_STATUS:
		{
			printk( "[DHL]V4L2_CID_AF_2nd_STATUS.. \n");
			break;
		}
		case V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL:
		{
			printk( "[DHL]V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL.. \n");
			struct sr030pc50_sensor *sensor = &sr030pc50;
			reg->val = (__s64)sensor->exif_info.exposure_time.denominal;
			break;
		}
		case V4L2_CID_GET_EXIF_ISO_SPEED:
		{
			printk( "[DHL]V4L2_CID_GET_EXIF_ISO_SPEED.. \n");
			struct sr030pc50_sensor *sensor = &sr030pc50;
			reg->val = (__s64)sensor->exif_info.iso_speed_rationg;
			break;
		}
		case V4L2_CID_GET_EXIF_FLASH:
		{
			printk( "[DHL]V4L2_CID_GET_EXIF_FLASH.. \n");
			break;
		}
		case V4L2_CID_GET_FLASH_STATUS:
			printk( "[DHL]V4L2_CID_GET_FLASH_STATUS.. \n");
			break;
		case V4L2_CID_ESD_CHECK:
		{
			printk( "[DHL]V4L2_CID_ESD_CHECK.. \n");
			struct v4l2_control ESDCtrl;
			sr030pc50_ESD_check(client, &ESDCtrl);
			reg->val = (__s64)ESDCtrl.value;
			break;
		}

	default:
		Cam_Printk(KERN_NOTICE "ioctl_g_register!! quality value is not supported!!!\n");
		return -EINVAL;
}

return retval;
}

static int sr030pc50_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register * reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr030pc50_sensor *sensor = &sr030pc50;
	int retval = 0;

	Cam_Printk(KERN_NOTICE "ioctl_s_register is called...(reg = %d, val = %d)\n", reg->reg, reg->val);

	switch (reg->reg)
	{
		case V4L2_CID_ISO:
			Cam_Printk(KERN_NOTICE "V4L2_CID_ISO");
			retval = sr030pc50_t_ISO(client,reg->val);
			break;
		case V4L2_CID_BRIGHTNESS:
			Cam_Printk(KERN_NOTICE "V4L2_CID_BRIGHTNESS");
			retval = sr030pc50_t_brightness(client,reg->val);
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			Cam_Printk(KERN_NOTICE "V4L2_CID_DO_WHITE_BALANCE");
			retval = sr030pc50_t_whitebalance(client,reg->val);
			break;
		case V4L2_CID_EFFECT:
			Cam_Printk(KERN_NOTICE "V4L2_CID_EFFECT");
			retval = sr030pc50_t_effect(client,reg->val);
			break;
		case V4L2_CID_CONTRAST:
			Cam_Printk(KERN_NOTICE "V4L2_CID_CONTRAST");
			retval = sr030pc50_t_contrast(client,reg->val);
			break;
		case V4L2_CID_SATURATION:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SATURATION");
			retval = sr030pc50_t_saturation(client,reg->val);
			break;
		case V4L2_CID_SHARPNESS:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SHARPNESS");
			retval = sr030pc50_t_sharpness(client,reg->val);
			break;
		case V4L2_CID_SCENE:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SCENE");
			retval = sr030pc50_t_scene(client,reg->val);
			break;
		case V4L2_CID_PHOTOMETRY:
			Cam_Printk(KERN_NOTICE "V4L2_CID_PHOTOMETRY");
			retval = sr030pc50_t_photometry(client,reg->val);
			break;
		case V4L2_CID_QUALITY:
			Cam_Printk(KERN_NOTICE "V4L2_CID_QUALITY");
			retval = sr030pc50_t_quality(client,reg->val);
			break;
		case V4L2_CID_FPS:
			Cam_Printk(KERN_NOTICE "V4L2_CID_FPS");
			retval = sr030pc50_t_fps(client,reg->val);
			break;
		case V4L2_CID_SELECT_MODE:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SELECT_MODE");
			retval = sr030pc50_set_mode(client,reg->val);
			break;
		case V4L2_CID_CAMERA_PREVIEW_SIZE:
			Cam_Printk(KERN_NOTICE "V4L2_CID_CAMERA_PREVIEW_SIZE");
			retval = sr030pc50_preview_size(client,reg->val);
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
			break;
		case V4L2_CID_AF_POSITION_STOP:
			break;
		case V4L2_CID_AE_LOCK:
			break;
		case V4L2_CID_SET_STILL_STATUS:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SET_STILL_STATUS");
			retval = sr030pc50_set_still_status();
			break;
		case V4L2_CID_SET_PREVIEW_STATUS:
			Cam_Printk(KERN_NOTICE "V4L2_CID_SET_PREVIEW_STATUS");
			retval = sr030pc50_set_preview_status();
			break;
		case V4L2_CID_SET_FLASH_STATUS:
			break;
		case V4L2_CID_SET_FLASH_MODE:
			break;

		default:
			break;
	}
	return retval;
}
//#endif

static struct i2c_device_id sr030pc50_idtable[] = {
	{ "sr030pc50", 1 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, sr030pc50_idtable);

/**************************************************************************/
/*                 AF Focus Mopde                                         */
/**************************************************************************/

static int sr030pc50_AE_lock(struct i2c_client *client,s32 value)
{       
	struct sr030pc50_sensor *sensor = &sr030pc50;

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
	Cam_Printk(KERN_NOTICE "sr030pc50_AE_lock is failed!!!\n");
	return -EINVAL;
}

static int sr030pc50_set_focus_mode(struct i2c_client *client, s32 value, int step)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;
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


	Cam_Printk(KERN_NOTICE "sr030pc50_set_focus is failed!!!\n");
	return -EINVAL;
}


static int sr030pc50_set_focus(struct i2c_client *client,s32 value)
{       
	struct sr030pc50_sensor *sensor = &sr030pc50;

	//	int cnt = 0;

	//u8 readdata = 0x00;
	//u8 status = 0x00;

	Cam_Printk(KERN_NOTICE "sr030pc50_set_focus_status is called...[%d]\n",value);



	switch(value)
	{
		case SR030PC50_AF_START :

			break;

		case SR030PC50_AF_STOP :

			break;

		case SR030PC50_AF_STOP_STEP_1 :

			break;

		case SR030PC50_AF_STOP_STEP_2 :

			break;

		case SR030PC50_AF_STOP_STEP_3 :

			break;

		default:
			Cam_Printk(KERN_NOTICE "[AF]Invalid value is ordered!!!  : %d\n",value);
			goto focus_status_fail;
	}

	return 0;

focus_status_fail:
	Cam_Printk(KERN_NOTICE "sr030pc50_set_focus_status is failed!!!\n");
	return -EINVAL;
}

/* 640x480 Window Size */
#define INNER_WINDOW_WIDTH              143
#define INNER_WINDOW_HEIGHT             143
#define OUTER_WINDOW_WIDTH              320
#define OUTER_WINDOW_HEIGHT             266

static int sr030pc50_set_focus_touch_position(struct i2c_client *client, s32 value)
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

	struct sr030pc50_sensor *sensor = &sr030pc50;

	Cam_Printk(KERN_NOTICE "value : %d\n", value);



	sr030pc50_touch_state = true;
	return 0;
}

static int sr030pc50_get_focus_status(struct i2c_client *client, struct v4l2_control *ctrl, s32 value)
{
	struct sr030pc50_sensor *sensor = &sr030pc50;

	u16 status= 0x00;


	Cam_Printk(KERN_NOTICE, "result = %d (1.progress, 2.success, 3.fail)\n", ctrl->value);
	return 0;

}


void sr030pc50_set_REG_TC_DBG_AutoAlgEnBits(struct i2c_client *client,int bit, int set)
{
	//struct sr030pc50_sensor *sensor = &sr030pc50;
	int REG_TC_DBG_AutoAlgEnBits = 0;

	return;
}

static int sr030pc50_g_mbus_config(struct v4l2_subdev *sd,
		struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_1_LANE;/*lyh: mipi with 1 data lane*/

	return 0;
}


static struct v4l2_subdev_core_ops sr030pc50_core_ops = {
	//.g_ctrl			= sr030pc50_g_ctrl,
	.s_ctrl			= sr030pc50_s_ctrl,
	.init				= sr030pc50_init,
	.g_chip_ident		= sr030pc50_g_chip_ident,
//#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= sr030pc50_g_register,
	.s_register		= sr030pc50_s_register,
//#endif

};

static struct v4l2_subdev_video_ops sr030pc50_video_ops = {
	.s_stream		= sr030pc50_s_stream,
	.s_mbus_fmt		= sr030pc50_s_fmt,
	.try_mbus_fmt		= sr030pc50_try_fmt,
	.enum_mbus_fmt		= sr030pc50_enum_fmt,
	.enum_mbus_fsizes	= sr030pc50_enum_fsizes,
	.g_parm				= sr030pc50_g_parm,
	.s_parm				= sr030pc50_s_parm,
	.g_mbus_config	= sr030pc50_g_mbus_config,
	//.cropcap		= sr030pc50_cropcap,
	//.g_crop			= sr030pc50_g_crop,
};

static struct v4l2_subdev_ops sr030pc50_subdev_ops = {
	.core			= &sr030pc50_core_ops,
	.video			= &sr030pc50_video_ops,
};

/*
 * i2c_driver function
 */


static int sr030pc50_command(struct i2c_client *client, unsigned int cmd, void *arg)
{


	switch (cmd) { 
		case VIDIOC_DBG_G_CHIP_IDENT:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_DBG_G_CHIP_IDENT\n");
			return v4l2_chip_ident_i2c_client(client, arg, V4L2_IDENT_SR030PC50, 0);		
		case VIDIOC_INT_RESET:
			sr030pc50_reset(client);
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_INT_RESET\n");
			return 0;
		case VIDIOC_QUERYCAP:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_QUERYCAP\n");
			return sr030pc50_querycap(client, (struct v4l2_capability *) arg);
#if 0			
		case VIDIOC_ENUM_FMT:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_ENUM_FMT\n");
			return sr030pc50_enum_fmt(client, index, (struct v4l2_fmtdesc *) arg);
#endif
		case VIDIOC_ENUM_FRAMESIZES:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_ENUM_FRAMESIZES\n");
			return sr030pc50_enum_fsizes(client, (struct v4l2_frmsizeenum *) arg);
		case VIDIOC_TRY_FMT:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_TRY_FMT\n");
			return sr030pc50_try_fmt(client, (struct v4l2_format *) arg);
		case VIDIOC_S_FMT:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_S_FMT\n");
			return sr030pc50_s_fmt(client, (struct v4l2_format *) arg);
#if 0			
		case VIDIOC_QUERYCTRL:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_QUERYCTRL\n");
			return sr030pc50_queryctrl(client, (struct v4l2_queryctrl *) arg);
#endif
		case VIDIOC_S_CTRL:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_S_CTRL\n");
			return sr030pc50_s_register(client, (struct v4l2_control *) arg);
		case VIDIOC_G_CTRL:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_G_CTRL\n");
			return sr030pc50_g_register(client, (struct v4l2_control *) arg);
		case VIDIOC_S_PARM:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_S_PARM\n");
			return sr030pc50_s_parm(client, (struct v4l2_streamparm *) arg);
		case VIDIOC_G_PARM:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_G_PARM\n");
			return sr030pc50_g_parm(client, (struct v4l2_streamparm *) arg);
#if 0		
		case VIDIOC_S_INPUT:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_S_INPUT\n");
			return sr030pc50_s_input(client, (int *) arg);
#endif
		case VIDIOC_STREAMON:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_STREAMON\n");
			return sr030pc50_streamon(client);
		case VIDIOC_STREAMOFF:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_STREAMOFF\n");
			return sr030pc50_streamoff(client);
#ifdef CONFIG_VIDEO_ADV_DEBUG
		case VIDIOC_DBG_G_REGISTER:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_DBG_G_REGISTER\n");
			return sr030pc50_g_register(client, (struct v4l2_dbg_register *) arg);
		case VIDIOC_DBG_S_REGISTER:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_DBG_S_REGISTER\n");
			return sr030pc50_s_register(client, (struct v4l2_dbg_register *) arg);
#endif
		case VIDIOC_G_EXIF:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_G_EXIF\n");
			return sr030pc50_g_exif_info(client, (struct v4l2_exif_info *) arg);
		case VIDIOC_S_THUMBNAIL:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_S_THUMBNAIL\n");
			return sr030pc50_s_thumbnail_size(client, (struct v4l2_pix_format *) arg);
		case VIDIOC_AE_AWB_STATUS:
			Cam_Printk(KERN_NOTICE " sr030pc50_command : VIDIOC_AE_AWB_STATUS\n");
			return sr030pc50_AE_AWB_Status(client, (int *)arg);
	}
	return -EINVAL;
}

static int sr030pc50_probe(struct i2c_client *client,
		const struct i2c_device_id *did)
{
	struct sr030pc50_info *priv;
	struct soc_camera_device *icd	= client->dev.platform_data;
	struct soc_camera_link *icl;
	int ret;

	printk("------------sr030pc50_probe--------------\n");

	if (!icd) {
		dev_err(&client->dev, "Missing soc-camera data!\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(struct sr030pc50_info), GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "Failed to allocate private data!\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &sr030pc50_subdev_ops);

	//icd->ops = &sr030pc50_ops;

	ret = sr030pc50_video_probe(icd, client);
	if (ret < 0) {
		//icd->ops = NULL;
		kfree(priv);
	}

	printk("------------sr030pc50_probe---return --ret = %d---------\n", ret);
	return ret;
}

static int sr030pc50_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_driver sr030pc50_driver = {
	.driver = {
		.name	= "sr030pc50",
	},
	.id_table       = sr030pc50_idtable,	
	.command	= sr030pc50_command,
	.probe		= sr030pc50_probe,
	.remove		= sr030pc50_remove,
};

/*
 * Module initialization
 */
static int __init sr030pc50_mod_init(void)
{
	int ret =0;
	Cam_Printk(KERN_NOTICE "siliconfile sr030pc50 sensor driver, at your service\n");
	ret = i2c_add_driver(&sr030pc50_driver);
	Cam_Printk(KERN_NOTICE "siliconfile sr030pc50 :%d \n ",ret);
	return ret;
	//return i2c_add_driver(&sr030pc50_driver);
}

static void __exit sr030pc50_mod_exit(void)
{
	i2c_del_driver(&sr030pc50_driver);
}

module_init(sr030pc50_mod_init);
//module_init(sr030pc50_mod_init);
module_exit(sr030pc50_mod_exit);

