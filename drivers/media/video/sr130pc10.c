/*
 * A V4L2 driver for siliconfile SR130PC10 cameras.
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
 * Create SR130PC10 driver from SR030PC50 driver by
 * Vincent Wan <zswan@marvell.com> for Marvell helan delos project.
 */
#include <linux/init.h>
#include <linux/module.h>



#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>
//#include <mach/camera.h>

#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

#include <media/v4l2-subdev.h>
#include <mach/gpio.h>
//#include <mach/camera.h>

#include "sr130pc10.h"
#include "sr130pc10_regs2.h"

/**++ SYSFS for factory mode : dhee79.lee 20140213 ++**/
//struct class *camera_class; // AT_Command 
struct device *dev_f;
/**-- SYSFS for factory mode : dhee79.lee --**/

MODULE_AUTHOR("Jonathan Corbet <corbet@lwn.net>");
MODULE_DESCRIPTION("A low-level driver for siliconfile SR130PC10 sensors");
MODULE_LICENSE("GPL");

#define to_sr130pc10(sd)		container_of(sd, struct sr130pc10_info, subdev)

#define sr130pc10_WRT_LIST(B, A)	\
	sr130pc10_i2c_wrt_list(B, A, (sizeof(A) / sizeof(A[0])), #A);


static const struct sr130pc10_datafmt sr130pc10_colour_fmts[] = {
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

static int sr130pc10_cam_state;

#define SXGA_WIDTH		1280
#define SXGA_HEIGHT     	960

#define VGA_WIDTH		640
#define VGA_HEIGHT		480

#define CIF_WIDTH		352
#define CIF_HEIGHT		288

#define QVGA_WIDTH		320
#define QVGA_HEIGHT	240

#define QCIF_WIDTH		176
#define QCIF_HEIGHT		144

/*
 * Our nominal (default) frame rate.
 */

int sr130pc10_s_exif_info(struct i2c_client *client);
static int sr130pc10_regs_table_write(struct i2c_client *c, char *name);
static int sr130pc10_t_fps(struct i2c_client *client, int value);

#define SR130PC10_FRAME_RATE 30

#define REG_MIDH	0x1c	/* Manuf. ID high */

#define   CMATRIX_LEN 6

/*Heron Tuning*/
//#define CONFIG_LOAD_FILE

#define FALSE 0
#define TRUE 1

static int gDTP_flag = FALSE;

/*
 * Information we maintain about a known sensor.
 */

struct sr130pc10_sensor sr130pc10 = {
	.timeperframe = {
		.numerator    = 1,
		.denominator  = 30,
	},
	.fps			= 30,
	//.bv			= 0,
	.state			= SR130PC10_STATE_PREVIEW,
	.mode			= SR130PC10_MODE_CAMERA,
	.preview_size		= PREVIEW_SIZE_640_480,
	.capture_size		= CAPTURE_SIZE_640_480,
	.detect			= SENSOR_NOT_DETECTED,
	.focus_mode		= SR130PC10_AF_SET_NORMAL,
	.effect			= EFFECT_OFF,
	.iso			= ISO_AUTO,
	.photometry		= METERING_CENTER,
	.ev			= EV_DEFAULT,
	//.wdr			= SR130PC10_WDR_OFF,
	.contrast		= CONTRAST_DEFAULT,
	.saturation		= SATURATION_DEFAULT,
	.sharpness		= SHARPNESS_DEFAULT,
	.wb			= WB_AUTO,
	//.isc 			= SR130PC10_ISC_STILL_OFF,
	.scene			= SCENE_OFF,
	.aewb			= AWB_AE_UNLOCK,
	//.antishake		= SR130PC10_ANTI_SHAKE_OFF,
	//.flash_capture	= SR130PC10_FLASH_CAPTURE_OFF,
	//.flash_movie		= SR130PC10_FLASH_MOVIE_OFF,
	.quality		= QUALITY_SUPERFINE, 
	//.zoom			= SR130PC10_ZOOM_1P00X,
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
			},
	.cam_mode           =  CAMERA_MODE,
};

extern struct sr130pc10_platform_data sr130pc10_platform_data0;

struct sr130pc10_format_struct;  /* coming later */
struct sr130pc10_info {
	struct sr130pc10_format_struct *fmt;  /* Current format */
	unsigned char sat;		/* Saturation value */
	int hue;			/* Hue value */
	struct v4l2_subdev subdev;
	int model;	/* V4L2_IDENT_xxx* codes from v4l2-chip-ident.h */
	u32 pixfmt;
	struct i2c_client *client;
	struct soc_camera_device icd;

};

static int sr130pc10_write_byte(struct i2c_client *c, unsigned char reg,
		unsigned char value)
{
	int retry = 3, ret;
	
	if (reg == 0xfe)
	{
		msleep(value);  /* Wait for reset to run */
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
 * sr130pc10_i2c_read_multi: Read (I2C) multiple bytes to the camera sensor
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
 * sr130pc10_i2c_read: Read (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @data: data to be read
 *
 * Returns 0 on success, <0 on error
 */
static int sr130pc10_i2c_read( struct i2c_client *client, unsigned char subaddr, unsigned char *data)
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

static int32_t sr130pc10_i2c_write_16bit( struct i2c_client *client, u16 packet)
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

static int sr130pc10_i2c_wrt_list( struct i2c_client *client, const u16 *regs,
	int size, char *name)
{
	int i;
	u8 m_delay = 0;

	u16 temp_packet;


	CAM_DEBUG("%s, size=%d", name, size);
	for (i = 0; i < size; i++) {
		temp_packet = regs[i];

		if ((temp_packet & SR130PC10_DELAY) == SR130PC10_DELAY) {
			m_delay = temp_packet & 0xFF;
			printk("delay = %d", m_delay*10);
			msleep(m_delay*10);/*step is 10msec*/
			continue;
		}

		if (sr130pc10_i2c_write_16bit(client,temp_packet) < 0) {
			printk("fail(0x%x, 0x%x:%d)",
					client->addr, temp_packet, i);
			return -EIO;
		}
		/*udelay(10);*/
	}

	return 0;
}

static int sr130pc10_reg_read_and_check(struct i2c_client *client, 
								unsigned char pagemode, unsigned char addr)
{
	unsigned char val = 0xFF;

	sr130pc10_write_byte(client,0x03,pagemode);//Vincent add here, for p0
	sr130pc10_i2c_read(client, addr, &val);	
	
	printk("-----------sr130pc10_reg_read_check------pagemode:0x%x, reg addr:0x%x, value:0x%x------\n", pagemode, addr, val);

	return val;
}


static int sr130pc10_read(struct i2c_client *c, u16 reg, u16 *value)
{
	int ret;

	ret = i2c_smbus_read_byte_data(c, reg);
	if (ret >= 0)
		*value = (unsigned char) ret;
	return ret;
}

static int sr130pc10_write(struct i2c_client *c, u16 reg,  u16 value)
{
	int ret=0; 

	if(reg == 0xff)
	{
		msleep(value);  /* Delay 100ms */
		return 0;
	}

	ret = i2c_smbus_write_byte_data(c, reg, value);

	return ret;
}

static int sr130pc10_write_regs(struct i2c_client *c, tagCamReg16_t *vals, u32 reg_length, char *name)
{
	int i = 0, ret=0;

#ifdef CONFIG_LOAD_FILE
	printk(KERN_NOTICE "======[Length %d : Line %d]====== \n", reg_length, __LINE__);
	ret = sr130pc10_regs_table_write(c, name);
#else

	for (i = 0; i < reg_length; i++) {
		ret = sr130pc10_write(c, vals[i].value>>8, vals[i].value & 0x00FF);
		if (ret < 0){
			printk(KERN_NOTICE "======[sr130pc10_write_array %d]====== \n", ret );	
			return ret;
		}
	}
#endif
	return ret;
}

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

static char *sr130pc10_regs_table = NULL;

static int sr130pc10_regs_table_size;

static int sr130pc10_regs_table_init(void)
{
	struct file *filp;

	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();

	printk("***** %s %d\n", __func__, __LINE__);

	set_fs(get_ds());

	filp = filp_open("/sdcard/sr130pc10_regs2.h", O_RDONLY, 0);
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

	sr130pc10_regs_table = dp;
	sr130pc10_regs_table_size = l;
	*((sr130pc10_regs_table + sr130pc10_regs_table_size) - 1) = '\0';

	printk("*****Compeleted %s %d\n", __func__, __LINE__);
	return 0;
}

void sr130pc10_regs_table_exit(void)
{
	/* release allocated memory when exit preview */
	if (sr130pc10_regs_table) {
		kfree(sr130pc10_regs_table);
		sr130pc10_regs_table = NULL;
		sr130pc10_regs_table_size = 0;
	}
	else
		printk("*****sr130pc10_regs_table is already null\n");
	
	printk("*****%s done\n", __func__);

}

static int sr130pc10_regs_table_write(struct i2c_client *c, char *name)
{
	char *start, *end, *reg;//, *data;	
	unsigned short value;
	char data_buf[7];

	value = 0;

	printk("*****%s entered.\n", __func__);

	*(data_buf + 6) = '\0';

	start = strstr(sr130pc10_regs_table, name);

	end = strstr(start, "};");

	while (1) {	
		/* Find Address */	
		reg = strstr(start,"0x");		
		if (reg)
			start = (reg + 7);
		if ((reg == NULL) || (reg > end))
			break;

		/* Write Value to Address */	
			memcpy(data_buf, (reg), 6);	
			value = (unsigned short)simple_strtoul(data_buf, NULL, 16); 
			printk("value 0x%04x\n", value);

			{
				if(sr130pc10_write(c, value>>8, value & 0x00FF) < 0 )
				{
					printk("<=PCAM=> %s fail on sensor_write\n", __func__);
				}
			}
	}
	printk(KERN_ERR "***** Writing [%s] Ended\n",name);

	return 0;
}

#endif  /* CONFIG_LOAD_FILE */


static int sr130pc10_detect(struct i2c_client *client)
{
	unsigned char ID = 0xFFFF;

	printk("-----------sr130pc10_detect------client->addr:0x%x------\n", client->addr);
	
	ID = sr130pc10_reg_read_and_check(client, 0x00, 0x04);
	
	if(ID == 0x94) 
	{
		printk(SR130PC10_MOD_NAME"========================================\n");
		printk(SR130PC10_MOD_NAME"   [1.3M CAM] vendor_id ID : 0x%04X\n", ID);
		printk(SR130PC10_MOD_NAME"========================================\n");
	} 
	else 
	{
		printk(SR130PC10_MOD_NAME"-------------------------------------------------\n");
		printk(SR130PC10_MOD_NAME"   [1.3M CAM] sensor detect failure !!\n");
		printk(SR130PC10_MOD_NAME"   ID : 0x%04X[ID should be 0x94]\n", ID);
		printk(SR130PC10_MOD_NAME"-------------------------------------------------\n");
		return -EINVAL;
	}	

	return 0;
}

static void sr130pc10_reset(struct i2c_client *client)
{
	msleep(1);
}

static int sr130pc10_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct sr130pc10_sensor *sensor = &sr130pc10;
	int result =0;
	
#ifdef CONFIG_LOAD_FILE
	result = sr130pc10_regs_table_init();
	if (result > 0)
	{		
		Cam_Printk(KERN_ERR "***** sr130pc10_regs_table_init  FAILED. Check the Filie in MMC\n");
		return result;
	}
	result =0;
#endif
	
	result=sr130pc10_write_regs(c,sr130pc10_Init_Reg,ARRAY_SIZE(sr130pc10_Init_Reg),"sr130pc10_Init_Reg");

	sensor->state 		= SR130PC10_STATE_PREVIEW;
	sensor->mode 		= SR130PC10_MODE_CAMERA;
	sensor->effect		= EFFECT_OFF;
	sensor->iso 		= ISO_AUTO;	
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
	sensor->initial			= SR130PC10_STATE_INITIAL;
	
	Cam_Printk(KERN_NOTICE "===sr130pc10_init===[%s  %d]====== \n", __FUNCTION__, __LINE__);

	return result;
}


/*
 * Store information about the video data format.  The color matrix
 * is deeply tied into the format, so keep the relevant values here.
 * The magic matrix nubmers come from OmniVision.
 */
static struct sr130pc10_format_struct {
	__u8 *desc;
	__u32 pixelformat;
	int bpp;   /* bits per pixel */
} sr130pc10_formats[] = {
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
#define N_SR130PC10_FMTS ARRAY_SIZE(sr130pc10_formats)
/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sr130pc10_win_size {
	int	width;
	int	height;
} sr130pc10_win_sizes[] = {
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
	},
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
	},
	/* CIF */
	{
		.width		= CIF_WIDTH,
		.height		= CIF_HEIGHT,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
	},
	/* QCIF */
	{
		.width		= QCIF_WIDTH,
		.height		= QCIF_HEIGHT,
	},

};

static struct sr130pc10_win_size  sr130pc10_win_sizes_capture[] = {

	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
	},
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
	},
	/* CIF */
	{
		.width		= CIF_WIDTH,
		.height		= CIF_HEIGHT,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
	},
	/* QCIF */
	{
		.width		= QCIF_WIDTH,
		.height		= QCIF_HEIGHT,
	},

};

/* Find a data format by a pixel code in an array */
static const struct sr130pc10_datafmt *sr130pc10_find_datafmt(
	enum v4l2_mbus_pixelcode code, const struct sr130pc10_datafmt *fmt,
	int n)
{
	int i;
	for (i = 0; i < n; i++)
		if (fmt[i].code == code)
			return fmt + i;

	return NULL;
}

#define N_WIN_SIZES (ARRAY_SIZE(sr130pc10_win_sizes))


static int sr130pc10_querycap(struct i2c_client *c, struct v4l2_capability *argp)
{
	if(!argp){
		printk(KERN_ERR" argp is NULL %s %d \n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}
	strcpy(argp->driver, "sr130pc10");
	strcpy(argp->card, "TD/TTC");
	return 0;
}

static int sr130pc10_enum_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(sr130pc10_colour_fmts))
		return -EINVAL;
	*code = sr130pc10_colour_fmts[index].code;
	return 0;
}

static int sr130pc10_enum_fsizes(struct v4l2_subdev *sd,
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
		if (fsize->index >= ARRAY_SIZE(sr130pc10_win_sizes)) {
			dev_warn(&client->dev,
				"sr130pc10 unsupported size %d!\n", fsize->index);
			return -EINVAL;
		}
		fsize->discrete.height = sr130pc10_win_sizes[fsize->index].height;
		fsize->discrete.width = sr130pc10_win_sizes[fsize->index].width;
		break;

	default:
		dev_err(&client->dev, "sr130pc10 unsupported format!\n");
		return -EINVAL;
	}
	return 0;
}

static int sr130pc10_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct sr130pc10_datafmt *fmt;
	int i;

	fmt = sr130pc10_find_datafmt(mf->code, sr130pc10_colour_fmts,
				   ARRAY_SIZE(sr130pc10_colour_fmts));
	if (!fmt) {
		dev_err(&client->dev, "sr130pc10 unsupported color format!\n");
		return -EINVAL;
	}

	mf->field = V4L2_FIELD_NONE;

	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
		/* enum the supported sizes*/
		for (i = 0; i < ARRAY_SIZE(sr130pc10_win_sizes); i++)
			if (mf->width == sr130pc10_win_sizes[i].width
				&& mf->height == sr130pc10_win_sizes[i].height)
				break;

		if (i >= ARRAY_SIZE(sr130pc10_win_sizes)) {
			dev_err(&client->dev, "sr130pc10 unsupported window"
				"size, w%d, h%d!\n", mf->width, mf->height);
			return -EINVAL;
		}
		mf->colorspace = V4L2_COLORSPACE_JPEG;
		break;

	default:
		dev_err(&client->dev, "sr130pc10 doesn't support code"
				"%d\n", mf->code);
		break;
	}
	return 0;
}


/*
 * Set a format.
 */

static int sr130pc10_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	const struct sr130pc10_datafmt *fmt;
	struct sr130pc10_sensor *sensor = &sr130pc10;

	printk("[DHL]sr130pc10_s_fmt..!!! \n");
	printk("[DHL]mf->code : [%d] \n",mf->code);
	printk("[DHL]mf->width : [%d] \n",mf->width);

	fmt =sr130pc10_find_datafmt(mf->code,sr130pc10_colour_fmts,
				   ARRAY_SIZE(sr130pc10_colour_fmts));
	if (!fmt) {
		dev_err(&c->dev, "sr130pc10 unsupported color format!\n");
		return -EINVAL;
	}

	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
		sensor->pix.pixelformat = V4L2_PIX_FMT_YUV422P;
		if(sr130pc10_cam_state == SR130PC10_STATE_PREVIEW){		
			switch (mf->width)
			{							
				case QCIF_WIDTH:
						sensor->preview_size=PREVIEW_SIZE_176_144;
						sr130pc10_write_regs(c,sr130pc10_preview_176x144,ARRAY_SIZE(sr130pc10_preview_176x144),"sr130pc10_preview_176x144");
						Cam_Printk(KERN_ERR"choose qcif(176x144) setting \n");
					break;

				case QVGA_WIDTH:
						sensor->preview_size=PREVIEW_SIZE_320_240;
						sr130pc10_write_regs(c,sr130pc10_preview_320x240,ARRAY_SIZE(sr130pc10_preview_320x240),"sr130pc10_preview_320x240");			
						Cam_Printk(KERN_ERR"choose qvga(320x240) setting \n");
					break;

				case CIF_WIDTH:
						sensor->preview_size=PREVIEW_SIZE_352_288;
						sr130pc10_write_regs(c,sr130pc10_preview_352x288,ARRAY_SIZE(sr130pc10_preview_352x288),"sr130pc10_preview_352x288");			
						Cam_Printk(KERN_ERR"choose cif(352x288) setting \n");
					break;

				case VGA_WIDTH:
						sensor->preview_size=PREVIEW_SIZE_640_480;
						sr130pc10_write_regs(c,sr130pc10_preview_640x480,ARRAY_SIZE(sr130pc10_preview_640x480),"sr130pc10_preview_640x480");
						Cam_Printk(KERN_ERR"choose VGA(640x480) setting \n");
						sr130pc10_write_regs(c,sr130pc10_Preview_Mode,ARRAY_SIZE(sr130pc10_Preview_Mode),"sr130pc10_Preview_Mode");	
						Cam_Printk(KERN_ERR"Preview Mode setting \n");
						break;

				case SXGA_WIDTH:
						sr130pc10_write_regs(c,sr130pc10_Capture_1280X960,ARRAY_SIZE(sr130pc10_Capture_1280X960),"sr130pc10_Capture_1280X960");
						Cam_Printk(KERN_ERR"choose SXGA(1280x960) setting \n");
						break;
						
				default:
					printk("\n unsupported size for preview! %s %d w=%d h=%d\n", __FUNCTION__, __LINE__, mf->width, mf->height);
					goto out;
					break;
			    }
			}
			else if (sr130pc10_cam_state == SR130PC10_STATE_CAPTURE){	
				switch (mf->width){	
					case SXGA_WIDTH:
							sr130pc10_write_regs(c,sr130pc10_Capture_1280X960,ARRAY_SIZE(sr130pc10_Capture_1280X960),"sr130pc10_Capture_1280X960");
							Cam_Printk(KERN_ERR"choose SXGA(1280x960) setting \n");
							break;
							
					case VGA_WIDTH:
							sr130pc10_write_regs(c,sr130pc10_Capture_640X480,ARRAY_SIZE(sr130pc10_Capture_640X480),"sr130pc10_Capture_640X480");
							Cam_Printk(KERN_ERR"choose VGA(640x480) setting \n");
							break;

					default:
						printk("\n unsupported size for preview! %s %d w=%d h=%d\n", __FUNCTION__, __LINE__, mf->width, mf->height);
						goto out;
						break;
				}
			}

			
			break;	

			default:
				printk("\n unsupported format! %s %d\n", __FUNCTION__, __LINE__);
				break;
		}

out:
	return 0;
}

/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 */
static int sr130pc10_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	
	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.numerator = 1;
	cp->timeperframe.denominator = SR130PC10_FRAME_RATE;
	
	return 0;
}

static int sr130pc10_s_parm(struct i2c_client *c, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sr130pc10_t_saturation(struct i2c_client *client, int value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;
	s32 old_value = (s32)sensor->saturation;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case SATURATION_MINUS_2:
			sr130pc10_write_regs(client,sr130pc10_saturation_m2,ARRAY_SIZE(sr130pc10_saturation_m2),"sr130pc10_saturation_m2");	
			break;

		case SATURATION_MINUS_1:
			sr130pc10_write_regs(client,sr130pc10_saturation_m1,ARRAY_SIZE(sr130pc10_saturation_m2),"sr130pc10_saturation_m2");	
			break;		

		case SATURATION_DEFAULT:
			sr130pc10_write_regs(client,sr130pc10_saturation_default,ARRAY_SIZE(sr130pc10_saturation_default),"sr130pc10_saturation_default");	
			break;	

		case SATURATION_PLUS_1:
			sr130pc10_write_regs(client,sr130pc10_saturation_p1,ARRAY_SIZE(sr130pc10_saturation_p1),"sr130pc10_saturation_p1");	
			break;		

		case SATURATION_PLUS_2:
			sr130pc10_write_regs(client,sr130pc10_saturation_p2,ARRAY_SIZE(sr130pc10_saturation_p2),"sr130pc10_saturation_p2");	
			break;	

		default:
			printk(SR130PC10_MOD_NAME "Saturation value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->saturation = value;
	Cam_Printk(KERN_NOTICE "%s success [Saturation e:%d]\n",__func__, sensor->saturation);
	return 0;
}

static int sr130pc10_q_saturation(struct i2c_client *client, __s32 *value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;

	Cam_Printk(KERN_NOTICE "sr130pc10_q_saturation is called...\n"); 
	value = sensor->saturation;
	return 0;
}


static int sr130pc10_t_brightness(struct i2c_client *client, int value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;
	s32 old_value = (s32)sensor->ev;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case EV_MINUS_4:
			sr130pc10_write_regs(client,sr130pc10_brightness_M4,ARRAY_SIZE(sr130pc10_brightness_M4),"sr130pc10_brightness_M4");	
			break;

		case EV_MINUS_3:
			sr130pc10_write_regs(client,sr130pc10_brightness_M3,ARRAY_SIZE(sr130pc10_brightness_M3),"sr130pc10_brightness_M3");	
			break;		

		case EV_MINUS_2:
			sr130pc10_write_regs(client,sr130pc10_brightness_M2,ARRAY_SIZE(sr130pc10_brightness_M2),"sr130pc10_brightness_M2");	
			break;	

		case EV_MINUS_1:
			sr130pc10_write_regs(client,sr130pc10_brightness_M1,ARRAY_SIZE(sr130pc10_brightness_M1),"sr130pc10_brightness_M4");	
			break;	
		
		case EV_DEFAULT:
			sr130pc10_write_regs(client,sr130pc10_brightness_default,ARRAY_SIZE(sr130pc10_brightness_default),"sr130pc10_brightness_default");	
			break;

		case EV_PLUS_1:
			sr130pc10_write_regs(client,sr130pc10_brightness_P1,ARRAY_SIZE(sr130pc10_brightness_P1),"sr130pc10_brightness_P1");	
			break;

		case EV_PLUS_2:
			sr130pc10_write_regs(client,sr130pc10_brightness_P2,ARRAY_SIZE(sr130pc10_brightness_P2),"sr130pc10_brightness_P2");	
			break;

		case EV_PLUS_3:
			sr130pc10_write_regs(client,sr130pc10_brightness_P3,ARRAY_SIZE(sr130pc10_brightness_P3),"sr130pc10_brightness_P3");	
			break;

		case EV_PLUS_4:
			sr130pc10_write_regs(client,sr130pc10_brightness_P4,ARRAY_SIZE(sr130pc10_brightness_P4),"sr130pc10_brightness_P4");	
			break;

		default:
			printk(SR130PC10_MOD_NAME "Brightness value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->ev = value;
	Cam_Printk(KERN_NOTICE "%s success [Brightness:%d]\n",__func__, sensor->ev);
	return 0;
}

static int sr130pc10_q_brightness(struct i2c_client *client, __s32 *value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;

	Cam_Printk(KERN_NOTICE "sr130pc10_q_brightness is called...\n"); 
	value = sensor->ev;
	return 0;
}

static int sr130pc10_t_contrast(struct i2c_client *client, int value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;
	s32 old_value = (s32)sensor->contrast;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case CONTRAST_MINUS_2:
			sr130pc10_write_regs(client,sr130pc10_contrast_m2,ARRAY_SIZE(sr130pc10_contrast_m2),"sr130pc10_contrast_m2");	
			break;

		case CONTRAST_MINUS_1:
			sr130pc10_write_regs(client,sr130pc10_contrast_m1,ARRAY_SIZE(sr130pc10_contrast_m1),"sr130pc10_contrast_m1");	
			break;		

		case CONTRAST_DEFAULT:
			sr130pc10_write_regs(client,sr130pc10_contrast_default,ARRAY_SIZE(sr130pc10_contrast_default),"sr130pc10_contrast_default");	
			break;	

		case CONTRAST_PLUS_1:
			sr130pc10_write_regs(client,sr130pc10_contrast_p1,ARRAY_SIZE(sr130pc10_contrast_p1),"sr130pc10_contrast_p1");	
			break;		

		case CONTRAST_PLUS_2:
			sr130pc10_write_regs(client,sr130pc10_contrast_p2,ARRAY_SIZE(sr130pc10_contrast_p2),"sr130pc10_contrast_p2");	
			break;	

		default:
			printk(SR130PC10_MOD_NAME "contrast value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->contrast = value;
	Cam_Printk(KERN_NOTICE "%s success [Contrast e:%d]\n",__func__, sensor->contrast);
	return 0;
}

static int sr130pc10_q_contrast(struct i2c_client *client, __s32 *value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;

	Cam_Printk(KERN_NOTICE "sr130pc10_q_contrast is called...\n"); 
	value = sensor->contrast;
	return 0;
}

static int sr130pc10_t_whitebalance(struct i2c_client *client, int value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;
	s32 old_value = (s32)sensor->wb;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case WB_AUTO:
			sr130pc10_write_regs(client,sr130pc10_WB_Auto,ARRAY_SIZE(sr130pc10_WB_Auto),"sr130pc10_WB_Auto");	
			break;

		case WB_DAYLIGHT:
			sr130pc10_write_regs(client,sr130pc10_WB_Daylight,ARRAY_SIZE(sr130pc10_WB_Daylight),"sr130pc10_WB_Daylight");	
			break;		

		case WB_CLOUDY:
			sr130pc10_write_regs(client,sr130pc10_WB_Cloudy,ARRAY_SIZE(sr130pc10_WB_Cloudy),"sr130pc10_WB_Cloudy");	
			break;	

		case WB_FLUORESCENT:
			sr130pc10_write_regs(client,sr130pc10_WB_Fluorescent,ARRAY_SIZE(sr130pc10_WB_Fluorescent),"sr130pc10_WB_Fluorescent");	
			break;	
		
		case WB_INCANDESCENT:
			sr130pc10_write_regs(client,sr130pc10_WB_Incandescent,ARRAY_SIZE(sr130pc10_WB_Incandescent),"sr130pc10_WB_Incandescent");	
			break;

		default:
			printk(SR130PC10_MOD_NAME "White Balance value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->wb = value;
	Cam_Printk(KERN_NOTICE "%s success [White Balance e:%d]\n",__func__, sensor->wb);
	return 0;
}

static int sr130pc10_q_whitebalance(struct i2c_client *client, __s32 *value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;

	Cam_Printk(KERN_NOTICE "sr130pc10_get_whitebalance is called...\n"); 
	value = sensor->wb;
	return 0;
}

static int sr130pc10_t_effect(struct i2c_client *client, int value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;
	s32 old_value = (s32)sensor->effect;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case EFFECT_OFF:
			sr130pc10_write_regs(client,sr130pc10_Effect_Normal,ARRAY_SIZE(sr130pc10_Effect_Normal),"sr130pc10_Effect_Normal");	
			break;

		case EFFECT_MONO:
			sr130pc10_write_regs(client,sr130pc10_Effect_Gray,ARRAY_SIZE(sr130pc10_Effect_Gray),"sr130pc10_Effect_Gray");	
			break;		

		case EFFECT_SEPIA:
			sr130pc10_write_regs(client,sr130pc10_Effect_Sepia,ARRAY_SIZE(sr130pc10_Effect_Sepia),"sr130pc10_Effect_Sepia");	
			break;	

		case EFFECT_NEGATIVE:
			sr130pc10_write_regs(client,sr130pc10_Effect_Negative,ARRAY_SIZE(sr130pc10_Effect_Negative),"sr130pc10_Effect_Negative");	
			break;	
			
		default:
			printk(SR130PC10_MOD_NAME "Sketch value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->effect = value;
	Cam_Printk(KERN_NOTICE "%s success [Effect e:%d]\n",__func__, sensor->effect);
	return 0;
}

static int sr130pc10_q_effect(struct i2c_client *client, __s32 *value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;

	Cam_Printk(KERN_NOTICE "sr130pc10_q_effect is called...\n"); 
	value = sensor->effect;
	return 0;
}

static int sr130pc10_t_fps(struct i2c_client *client, int value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;
	s32 old_value = (s32)sensor->fps;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	if(old_value == value){
		Cam_Printk(KERN_NOTICE "%s new value is same as existing value\n", __func__);
		return 0;
	}

	switch(value)
	{

	case FPS_auto:
		sr130pc10_write_regs(client,sr130pc10_Auto_fps,ARRAY_SIZE(sr130pc10_Auto_fps),"sr130pc10_Auto_fps");
		break;

	case FPS_7:
		sr130pc10_write_regs(client,sr130pc10_7fps,ARRAY_SIZE(sr130pc10_7fps),"sr130pc10_7fps");	
		break;

	case FPS_10:
		sr130pc10_write_regs(client,sr130pc10_10fps,ARRAY_SIZE(sr130pc10_10fps),"sr130pc10_10fps");	
		break;

	case FPS_15:
		sr130pc10_write_regs(client,sr130pc10_15fps,ARRAY_SIZE(sr130pc10_15fps),"sr130pc10_15fps");	
		break;		

	case FPS_30:
		printk(KERN_NOTICE "######################################\n");
		printk(KERN_NOTICE "[DHL] FPS Change 30 -> 15 !!!!! \n");
		printk(KERN_NOTICE "######################################\n");
		sr130pc10_write_regs(client,sr130pc10_15fps,ARRAY_SIZE(sr130pc10_15fps),"sr130pc10_15fps");	
		break;	
		
	default:
			printk(KERN_NOTICE "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->fps = value;
	Cam_Printk(KERN_NOTICE "%s success [FPS e:%d]\n",__func__, sensor->fps);
	return 0;
}

static int sr130pc10_q_fps(struct i2c_client *client, __s32 *value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;

	Cam_Printk(KERN_NOTICE "sr130pc10_q_fps is called...\n"); 
	value = sensor->fps;
	return 0;
}

static int sr130pc10_ESD_check(struct i2c_client *client, __s32 *value)
{	
	Cam_Printk(KERN_NOTICE "sr130pc10_ESD_check() \r\n");

	*value = ESD_NONE;

	return 0;

}

static int sr130pc10_t_dtp_on(struct i2c_client *client)
{
	Cam_Printk(KERN_NOTICE "sr130pc10_t_dtp_on is called...\n"); 

	gDTP_flag = TRUE;
	
	//sr130pc10_write_regs(client,sr130pc10_DTP_On,ARRAY_SIZE(sr130pc10_DTP_On),"sr130pc10_DTP_On");	
	return 0;
}

static int sr130pc10_t_dtp_stop(struct i2c_client *client)
{
	Cam_Printk(KERN_NOTICE "sr130pc10_t_dtp_stop is called...\n"); 
	sr130pc10_write_regs(client,sr130pc10_DTP_Off,ARRAY_SIZE(sr130pc10_DTP_Off),"sr130pc10_DTP_Off");	

	return 0;
}

static int sr130pc10_g_exif_info(struct i2c_client *client,struct v4l2_exif_info *exif_info)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;

	Cam_Printk(KERN_NOTICE "sr130pc10_g_exif_info is called...\n"); 
	*exif_info = sensor->exif_info;
	return 0;
}

static int sr130pc10_set_mode(struct i2c_client *client, int value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;
	
	sensor->mode = value;
	Cam_Printk(KERN_NOTICE, "sr130pc10_set_mode is called... mode = %d\n", sensor->mode);
	return 0;
}


static int sr130pc10_preview_size(struct i2c_client *client, int value)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;

	if(sensor->mode != SR130PC10_MODE_CAMCORDER)
	{
		switch (value) 
		{
			Cam_Printk(KERN_NOTICE "CAMERA MODE..\n"); 

			case SR130PC10_PREVIEW_SIZE_176_144:
				sr130pc10_write_regs(client,sr130pc10_preview_176x144,ARRAY_SIZE(sr130pc10_preview_176x144),"sr130pc10_preview_176x144");	
				break;
			case SR130PC10_PREVIEW_SIZE_320_240:
				sr130pc10_write_regs(client,sr130pc10_preview_320x240,ARRAY_SIZE(sr130pc10_preview_320x240),"sr130pc10_preview_320x240");	
				break;
			case SR130PC10_PREVIEW_SIZE_352_288:
				sr130pc10_write_regs(client,sr130pc10_preview_352x288,ARRAY_SIZE(sr130pc10_preview_352x288),"sr130pc10_preview_352x288");	
				break;
			default:
				printk(SR130PC10_MOD_NAME "Preview Resolution is not supported! : %d\n",value);
				return 0;
		}
	} 
	else 
	{
		Cam_Printk(KERN_NOTICE "CAMCORDER MODE..\n"); 

		switch (value) 
		{
			Cam_Printk(KERN_NOTICE "CAMERA MODE..\n"); 

			case SR130PC10_PREVIEW_SIZE_176_144:
				sr130pc10_write_regs(client,sr130pc10_preview_176x144,ARRAY_SIZE(sr130pc10_preview_176x144),"sr130pc10_preview_176x144");	
				break;
			case SR130PC10_PREVIEW_SIZE_320_240:
				sr130pc10_write_regs(client,sr130pc10_preview_320x240,ARRAY_SIZE(sr130pc10_preview_320x240),"sr130pc10_preview_320x240");	
				break;
			case SR130PC10_PREVIEW_SIZE_352_288:
				sr130pc10_write_regs(client,sr130pc10_preview_352x288,ARRAY_SIZE(sr130pc10_preview_352x288),"sr130pc10_preview_352x288");	
				break;
			default:
				printk(SR130PC10_MOD_NAME "Preview Resolution is not supported! : %d\n",value);
				return 0;
		}
	}
	return 0;
}


/* Get chip identification */
static int sr130pc10_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *id)
{
	struct sr130pc10_info *priv = to_sr130pc10(sd);

	id->ident = priv->model;
	id->revision = 0x0;//priv->revision;

	return 0;
}


static int sr130pc10_set_still_status(void)
{
	Cam_Printk(KERN_NOTICE "[DHL]sr130pc10_set_still_status.. \n");

	sr130pc10_cam_state = SR130PC10_STATE_CAPTURE;	

	return 0;
}

static int sr130pc10_set_preview_status(int value)
{
	Cam_Printk(KERN_NOTICE "[DHL]sr130pc10_set_preview_status.. \n");

	Cam_Printk(KERN_NOTICE "sr130pc10_set_preview_status value : 0x%x \n", value);
	Cam_Printk(KERN_NOTICE "sr130pc10_set_preview_status value : %d \n", value);
	
	sr130pc10_cam_state = SR130PC10_STATE_PREVIEW;

	return 0;
}

int sr130pc10_streamon(struct i2c_client *client)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;

	Cam_Printk(KERN_NOTICE "sr130pc10_streamon is called...\n");  

	if(sr130pc10_cam_state == SR130PC10_STATE_CAPTURE){
		sr130pc10_s_exif_info(client);
	}
	
	if(gDTP_flag == TRUE){
		sr130pc10_write_regs(client,sr130pc10_DTP_On,ARRAY_SIZE(sr130pc10_DTP_On),"sr130pc10_DTP_On");	
		gDTP_flag=FALSE;
	}
	
	return 0;
}

static int sr130pc10_streamoff(struct i2c_client *client)
{
	/* What's wrong with this sensor, it has no stream off function, oh!,Vincent.Wan */
	Cam_Printk(KERN_NOTICE " sr130pc10_sensor_stop_stream");
	return 0;
}
static int set_stream(struct i2c_client *client, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sr130pc10_streamon(client);
		if (ret < 0)
			goto out;
	} else {
		ret = sr130pc10_streamoff(client);
	}
out:
	return ret;
}

static int sr130pc10_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	ret = set_stream(client, enable);
	if (ret < 0)
		dev_err(&client->dev, "sr130pc10 set stream error\n");
	return ret;
}

static int sr130pc10_video_probe(struct soc_camera_device *icd,
			      struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sr130pc10_info *priv = to_sr130pc10(sd);
	u8 modelhi, modello;
	int ret, i;

	/*
	 * Make sure it's an sr130pc10
	 */
	//for(i =0;i<3;i++)
	{
		ret = sr130pc10_detect(client);
		if (!ret) {
			Cam_Printk(KERN_NOTICE "=========siliconfile sr130pc10 sensor detected==========\n");
			goto out;
		}
		
	}

	priv->model = V4L2_IDENT_SR130PC10;
out:
	return ret;
}

int sr130pc10_s_exif_info(struct i2c_client *client)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;
	u32 exposure_time=0;
	u16 exposure_data1=0;
	u16 exposure_data2=0;
	u16 exposure_data3=0;
	u32 iso_gain= 0;
	u16 iso_a_gain= 0;
	u32 iso_value= 0;

	Cam_Printk(KERN_NOTICE "[DHL] EXIF Info.. \r\n");
	Cam_Printk(KERN_NOTICE "s5k4ecgx_s_Exposure() \r\n");
	sr130pc10_write(client,0x03, 0x20);
	sr130pc10_read(client, 0x80, &exposure_data1);	
	sr130pc10_read(client, 0x81, &exposure_data2);	
	sr130pc10_read(client, 0x82, &exposure_data3);	

	exposure_time = 24000000 / ((exposure_data3<<3)|(exposure_data2<<11)|(exposure_data1<<19));

	Cam_Printk(KERN_NOTICE "[DHL]shutter_speed : %d \r\n",exposure_time);

	sensor->exif_info.exposure_time.inumerator=1;
	sensor->exif_info.exposure_time.denominal=exposure_time;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_s_ISO() \r\n");
	sr130pc10_write(client,0x03, 0x20);
	sr130pc10_read(client, 0xb0, &iso_a_gain);	

	iso_gain = 10000*(iso_a_gain/32)+5000;

	if(iso_gain < 12100)
		iso_value = 50;
	else if(iso_gain < 20000) 
		iso_value = 100;
	else if(iso_gain < 36800) 
		iso_value = 200;	
	else if(iso_gain < 69375) 
		iso_value = 400;
	else if(iso_gain < 138750) 
		iso_value = 800;
	else 
		iso_value = 1600;
	sensor->exif_info.iso_speed_rationg=iso_value;
	return 0;
}

static int sr130pc10_s_thumbnail_size(struct i2c_client *client, struct v4l2_pix_format *thumbnail)
{
	struct sr130pc10_sensor *sensor = &sr130pc10;
	struct v4l2_pix_format* pix = &sensor->thumbnail;
	pix->width= thumbnail->width;
	pix->height= thumbnail->height;
	int retval = 0;
	
	Cam_Printk(KERN_NOTICE "sr130pc10_s_thumbnail_size is called...(Width %d Height %d)\n",pix->width,pix->height);

	return retval;
}

static int sr130pc10_g_register(struct v4l2_subdev *sd,  struct v4l2_dbg_register * reg)
{
	
	Cam_Printk(KERN_NOTICE "sr130pc10_g_register() \r\n");

	switch (reg->reg) {
		case V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL:
		{
			struct sr130pc10_sensor *sensor = &sr130pc10;
			printk( "[DHL]V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL.. \n");
			reg->val = (__s64)sensor->exif_info.exposure_time.denominal;
			break;
		}
		case V4L2_CID_GET_EXIF_ISO_SPEED:
		{
			struct sr130pc10_sensor *sensor = &sr130pc10;
			printk( "[DHL]V4L2_CID_GET_EXIF_ISO_SPEED.. \n");
			reg->val = (__s64)sensor->exif_info.iso_speed_rationg;
			break;
		}
		case V4L2_CID_GET_EXIF_FLASH:
		{
			struct sr130pc10_sensor *sensor = &sr130pc10;
			printk( "[DHL]V4L2_CID_GET_EXIF_FLASH.. \n");
			reg->val = 0;
			break;
		}
		case V4L2_CID_GET_FLASH_STATUS:
			printk( "[DHL]V4L2_CID_GET_FLASH_STATUS.. \n");
			reg->val = 0;
			break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sr130pc10_s_register(struct v4l2_subdev *sd,  struct v4l2_dbg_register * reg)
{	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr130pc10_sensor *sensor = &sr130pc10;
	int retval = 0;

	Cam_Printk(KERN_NOTICE "ioctl_s_ctrl is called...(%d)\n", reg->reg);

	switch (reg->reg) 
	{ 
		case V4L2_CID_BRIGHTNESS:
			retval = sr130pc10_t_brightness(client,reg->val);
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			retval = sr130pc10_t_whitebalance(client,reg->val);
			break;
		case V4L2_CID_EFFECT:
			retval = sr130pc10_t_effect(client,reg->val);
			break;
		case V4L2_CID_CONTRAST:
			retval = sr130pc10_t_contrast(client,reg->val);
			break;
		case V4L2_CID_SATURATION:
			retval = sr130pc10_t_saturation(client,reg->val);
			break;	
		case V4L2_CID_FPS:
			retval = sr130pc10_t_fps(client,reg->val);
			break;
		case V4L2_CID_CAMERA_CHECK_DATALINE:
			retval = sr130pc10_t_dtp_on(client);
			break;	
		case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
			retval = sr130pc10_t_dtp_stop(client); 
			break;			
		case V4L2_CID_CAMERA_PREVIEW_SIZE:
			retval = sr130pc10_preview_size(client,reg->val); 
			break;
		case V4L2_CID_SET_STILL_STATUS:
			retval = sr130pc10_set_still_status();
			break;
		case V4L2_CID_SET_PREVIEW_STATUS:
			retval = sr130pc10_set_preview_status(reg->val);
			break;		
				
		default:
			Cam_Printk(SR130PC10_MOD_NAME "[id]Invalid value is ordered!!!\n");
			break;
	}
	return retval;
}

static struct i2c_device_id sr130pc10_idtable[] = {
	{ "sr130pc10", 1 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, sr130pc10_idtable);


static struct v4l2_subdev_core_ops sr130pc10_core_ops = {
//	.g_ctrl			= sr130pc10_g_ctrl,
//	.s_ctrl			= sr130pc10_s_ctrl,
	.init				= sr130pc10_init,
	.g_chip_ident		= sr130pc10_g_chip_ident,
	.g_register		= sr130pc10_g_register,
	.s_register		= sr130pc10_s_register,

};

static struct v4l2_subdev_video_ops sr130pc10_video_ops = {
	.s_stream		= sr130pc10_s_stream,
	.s_mbus_fmt		= sr130pc10_s_fmt,
	.try_mbus_fmt		= sr130pc10_try_fmt,
	.enum_mbus_fmt		= sr130pc10_enum_fmt,
	.enum_mbus_fsizes	= sr130pc10_enum_fsizes,
	.enum_framesizes		= sr130pc10_enum_fsizes,	
	.g_parm				= sr130pc10_g_parm,
	.s_parm				= sr130pc10_s_parm,
	//.g_mbus_config	= sr130pc10_g_mbus_config,
};

static struct v4l2_subdev_ops sr130pc10_subdev_ops = {
	.core			= &sr130pc10_core_ops,
	.video			= &sr130pc10_video_ops,
};

/*
 * i2c_driver function
 */


static int sr130pc10_command(struct i2c_client *client, unsigned int cmd, void *arg)
{


	switch (cmd) { 
		case VIDIOC_DBG_G_CHIP_IDENT:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_DBG_G_CHIP_IDENT\n");
			return v4l2_chip_ident_i2c_client(client, arg, V4L2_IDENT_SR130PC10, 0);		
		case VIDIOC_INT_RESET:
			sr130pc10_reset(client);
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_INT_RESET\n");
			return 0;
		case VIDIOC_QUERYCAP:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_QUERYCAP\n");
			return sr130pc10_querycap(client, (struct v4l2_capability *) arg);
		case VIDIOC_ENUM_FRAMESIZES:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_ENUM_FRAMESIZES\n");
			return sr130pc10_enum_fsizes(client, (struct v4l2_frmsizeenum *) arg);
		case VIDIOC_TRY_FMT:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_TRY_FMT\n");
			return sr130pc10_try_fmt(client, (struct v4l2_format *) arg);
		case VIDIOC_S_FMT:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_S_FMT\n");
			return sr130pc10_s_fmt(client, (struct v4l2_format *) arg);
		case VIDIOC_DBG_G_REGISTER:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_DBG_G_REGISTER\n");
			return sr130pc10_g_register(client, (struct v4l2_dbg_register *) arg);
		case VIDIOC_DBG_S_REGISTER:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_DBG_S_REGISTER\n");
			return sr130pc10_s_register(client, (struct v4l2_dbg_register *) arg);
		case VIDIOC_S_PARM:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_S_PARM\n");
			return sr130pc10_s_parm(client, (struct v4l2_streamparm *) arg);
		case VIDIOC_G_PARM:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_G_PARM\n");
			return sr130pc10_g_parm(client, (struct v4l2_streamparm *) arg);
		case VIDIOC_STREAMON:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_STREAMON\n");
			return sr130pc10_streamon(client);
		case VIDIOC_STREAMOFF:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_STREAMOFF\n");
			return sr130pc10_streamoff(client);
		case VIDIOC_G_EXIF:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_G_EXIF\n");
			return sr130pc10_g_exif_info(client, (struct v4l2_exif_info *) arg);
		case VIDIOC_S_THUMBNAIL:
			Cam_Printk(KERN_NOTICE " sr130pc10_command : VIDIOC_S_THUMBNAIL\n");
			return sr130pc10_s_thumbnail_size(client, (struct v4l2_pix_format *) arg);
	}
	return -EINVAL;
}

 ssize_t front_camera_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("front_camera_type: SOC\n");
	return sprintf(buf, "SOC");
}
 ssize_t front_camerafw_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("front_camerafw_type: SR130PC10\n");
	return sprintf(buf, "SR130PC10 N");
}

static DEVICE_ATTR(front_type, S_IRUGO | S_IXOTH,  front_camera_type, NULL);
static DEVICE_ATTR(front_camfw, S_IRUGO | S_IXOTH,  front_camerafw_type, NULL);
/*-- AT_Command[Flash mode] --*/

static int sr130pc10_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct sr130pc10_info *priv;
	struct soc_camera_device *icd	= client->dev.platform_data;
	struct soc_camera_link *icl;
	int ret;

	printk("------------sr130pc10_probe--------------\n");

	if (!icd) {
		dev_err(&client->dev, "Missing soc-camera data!\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(struct sr130pc10_info), GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "Failed to allocate private data!\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &sr130pc10_subdev_ops);

	/*++ AT_Command : dhee79.lee@samsung.com_20140213 ++*/
	// Camera Class already registered at the SR352 in DEGAS model
	dev_f= device_create(camera_class, NULL, 0, "%s", "front");	
	if (device_create_file(dev_f, &dev_attr_front_type) < 0)
	 printk("Failed to create device file(%s)!\n", dev_attr_front_type.attr.name);
	if (device_create_file(dev_f, &dev_attr_front_camfw) < 0)
	 printk("Failed to create device file(%s)!\n", dev_attr_front_camfw.attr.name);
	/*-- AT_Command --*/

	ret = sr130pc10_video_probe(icd, client);
	if (ret < 0) {
		kfree(priv);
	}
	
	printk("------------sr130pc10_probe---return --ret = %d---------\n", ret);
	return ret;
}

static int sr130pc10_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_driver sr130pc10_driver = {
	.driver = {
		.name	= "sr130pc10",
	},
	.id_table       = sr130pc10_idtable,	
	.command	= sr130pc10_command,
	.probe		= sr130pc10_probe,
	.remove		= sr130pc10_remove,
};

/*
 * Module initialization
 */
static int __init sr130pc10_mod_init(void)
{
	int ret =0;
	Cam_Printk(KERN_NOTICE "siliconfile sr130pc10 sensor driver, at your service\n");
	ret = i2c_add_driver(&sr130pc10_driver);
	Cam_Printk(KERN_NOTICE "siliconfile sr130pc10 :%d \n ",ret);
	return ret;
}

static void __exit sr130pc10_mod_exit(void)
{
	i2c_del_driver(&sr130pc10_driver);
}

module_init(sr130pc10_mod_init);
module_exit(sr130pc10_mod_exit);

