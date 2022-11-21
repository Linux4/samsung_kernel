/*
 * A V4L2 driver for SYS.LSI S5K4ECGX cameras.
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
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>
#include <mach/camera.h>
#include <linux/gpio.h>

#include "s5k4ecgx_regs.h"
#include "s5k4ecgx.h"

MODULE_AUTHOR("Jonathan Corbet <corbet@lwn.net>");
MODULE_DESCRIPTION("A low-level driver for SYS.LSI S5K4ECGX sensors");
MODULE_LICENSE("GPL");

#define to_s5k4ecgx(sd)		container_of(sd, struct s5k4ecgx_info, subdev)

static const struct s5k4ecgx_datafmt s5k4ecgx_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
	//{V4L2_MBUS_FMT_JPEG_1X8, V4L2_COLORSPACE_JPEG},
};


#define CAM_DEBUG 

#ifdef CAM_DEBUG 
#define Cam_Printk(msg...) printk(msg)	
#else
#define Cam_Printk
#endif

#define FLASH_PULSE_VALUE 15

#define FALSE 0
#define TRUE 1
static int gflash_status = FALSE;
static int need_flash = FALSE;
static int gflash_exif = FALSE;
/*
 * Basic window sizes.  These probably belong somewhere more globally
 * useful.
 */
#define WQXGA_WIDTH		2560
#define WQXGA_HEIGHT	1920

#define Wide4M_WIDTH	2560
#define Wide4M_HEIGHT	1536

#define QXGA_WIDTH		2048
#define QXGA_HEIGHT	1536

#define Wide2_4M_WIDTH	2048
#define Wide2_4M_HEIGHT	1232

#define UXGA_WIDTH		1600
#define UXGA_HEIGHT     	1200

#define Wide1_5M_WIDTH	1600
#define Wide1_5M_HEIGHT	 960

#define SXGA_WIDTH		1280
#define SXGA_HEIGHT     	960
#define HD_WIDTH		1280
#define HD_HEIGHT		720
#define XGA_WIDTH		1024
#define XGA_HEIGHT     	768
#define D1_WIDTH		720
#define D1_HEIGHT      	480

#define Wide4K_WIDTH	800
#define Wide4K_HEIGHT	480

#define WIDTH_704		704
#define HEIGHT_576		576

#define VGA_WIDTH		640
#define VGA_HEIGHT		480
#define QVGA_WIDTH		320
#define QVGA_HEIGHT	240
#define CIF_WIDTH		352
#define CIF_HEIGHT		288
#define QCIF_WIDTH		176
#define QCIF_HEIGHT		144
#define ROTATED_QCIF_WIDTH	144
#define ROTATED_QCIF_HEIGHT	176
#define THUMB_WIDTH	160
#define THUMB_HEIGHT	120
/*
 * Our nominal (default) frame rate.
 */
#define S5K4ECGX_FRAME_RATE 30

//#define S5K4ECGX_I2C_ADDR (0x5A >> 1) 

#define REG_MIDH	0x1c	/* Manuf. ID high */

#define   CMATRIX_LEN 6

/*Heron Tuning*/
//#define CONFIG_LOAD_FILE

/*
 * Information we maintain about a known sensor.
 */


static int s5k4ecgx_AE_lock_state = FALSE;
static int s5k4ecgx_AWB_lock_state = FALSE;
static int s5k4ecgx_cam_state;

static void s5k4ecgx_AE_lock(struct i2c_client *client,s32 value);
static void s5k4ecgx_AWB_lock(struct i2c_client *client,s32 value);
static int s5k4ecgx_AE_AWB_lock(struct i2c_client *client,s32 value);
static int s5k4ecgx_set_focus_mode(struct i2c_client *client, s32 value, int step);
static int s5k4ecgx_set_focus(struct i2c_client *client,s32 value);
static int s5k4ecgx_set_focus_touch_position(struct i2c_client *client, s32 value);
static int s5k4ecgx_get_focus_status(struct i2c_client *client, struct v4l2_control *ctrl, s32 value);
void s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(struct i2c_client *client,int bit, int set); 
camera_light_status_type s5k4ecgx_check_illuminance_status(struct i2c_client *client);
camera_ae_stable_type s5k4ecgx_check_ae_status(struct i2c_client *client);
int s5k4ecgx_s_exif_info(struct i2c_client *client);
static void s5k4ecgx_set_flash(struct i2c_client *client, int mode);
static int s5k4ecgx_get_flash_status(struct i2c_client *client);
static int s5k4ecgx_set_flash_mode(struct i2c_client *client, int mode);


struct s5k4ecgx_sensor s5k4ecgx = {
	.timeperframe = {
		.numerator    = 1,
		.denominator  = 30,
	},
	.fps			= 30,
	//.bv			= 0,
	.state			= S5K4ECGX_STATE_PREVIEW,
	.mode			= S5K4ECGX_MODE_CAMERA,
	.preview_size		= PREVIEW_SIZE_640_480,
	.capture_size		= CAPTURE_SIZE_2048_1536,
	.detect			= SENSOR_NOT_DETECTED,
	.focus_mode		= S5K4ECGX_AF_SET_NORMAL,
	.focus_type		= AUTO_FOCUS_MODE, 
	.effect			= EFFECT_OFF,
	.iso			= ISO_AUTO,
	.photometry		= METERING_CENTER,
	.ev			= EV_DEFAULT,
	//.wdr			= S5K4ECGX_WDR_OFF,
	.contrast		= CONTRAST_DEFAULT,
	.saturation		= SATURATION_DEFAULT,
	.sharpness		= SHARPNESS_DEFAULT,
	.wb			= WB_AUTO,
	//.isc 			= S5K4ECGX_ISC_STILL_OFF,
	.scene			= SCENE_OFF,
	.ae			= AE_UNLOCK,
	.awb			= AWB_UNLOCK,
	//.antishake		= S5K4ECGX_ANTI_SHAKE_OFF,
	//.flash_capture	= S5K4ECGX_FLASH_CAPTURE_OFF,
	//.flash_movie		= S5K4ECGX_FLASH_MOVIE_OFF,
	.quality		= QUALITY_SUPERFINE, 
	//.zoom			= S5K4ECGX_ZOOM_1P00X,
	.thumb_offset		= 0,
	.yuv_offset		= 0,
	.main_flash_mode  = MAIN_FLASH_OFF,
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

extern struct s5k4ecgx_platform_data s5k4ecgx_platform_data0;

struct s5k4ecgx_format_struct;  /* coming later */
struct s5k4ecgx_info {
	struct s5k4ecgx_format_struct *fmt;  /* Current format */
	unsigned char sat;		/* Saturation value */
	int hue;			/* Hue value */
	struct v4l2_subdev subdev;
	int model;	/* V4L2_IDENT_xxx* codes from v4l2-chip-ident.h */
	u32 pixfmt;
	struct i2c_client *client;
	struct soc_camera_device icd;

};

static int s5k4ecgx_read(struct i2c_client *c, u16 reg, u16 *value)
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

static int s5k4ecgx_write(struct i2c_client *c, u16 reg,  u16 val)
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

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

static char *s5k4ecgx_regs_table = NULL;

static int s5k4ecgx_regs_table_size;

static int s5k4ecgx_regs_table_init(void)
{
	struct file *filp;

	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();

	printk("***** %s %d\n", __func__, __LINE__);

	set_fs(get_ds());

	filp = filp_open("/storage/extSdCard/s5k4ecgx_regs.h", O_RDONLY, 0);
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

	s5k4ecgx_regs_table = dp;
	s5k4ecgx_regs_table_size = l;
	*((s5k4ecgx_regs_table + s5k4ecgx_regs_table_size) - 1) = '\0';

	printk("*****Compeleted %s %d\n", __func__, __LINE__);
	return 0;
}

void s5k4ecgx_regs_table_exit(void)
{
	/* release allocated memory when exit preview */
	if (s5k4ecgx_regs_table) {
		kfree(s5k4ecgx_regs_table);
		s5k4ecgx_regs_table = NULL;
		s5k4ecgx_regs_table_size = 0;
	}
	else
		printk("*****s5k4ecgx_regs_table is already null\n");
	
	printk("*****%s done\n", __func__);

}

static int s5k4ecgx_regs_table_write(struct i2c_client *c, char *name)
{
	char *start, *end, *reg;//, *data;	
	unsigned short addr, value;
	char reg_buf[7], data_buf[7];

	addr = value = 0;

	printk("*****%s entered.\n", __func__);

	*(reg_buf + 6) = '\0';
	*(data_buf + 6) = '\0';

	start = strstr(s5k4ecgx_regs_table, name);

	end = strstr(start, "};");

	while (1) {	
		/* Find Address */	
		reg = strstr(start,"{0x");		
		if (reg)
			start = (reg + 16);
		if ((reg == NULL) || (reg > end))
			break;

		/* Write Value to Address */	
		if (reg != NULL) {
			memcpy(reg_buf, (reg + 1), 6);	
			memcpy(data_buf, (reg + 9), 6);	
			addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16); 
			value = (unsigned short)simple_strtoul(data_buf, NULL, 16); 
//			printk("addr 0x%04x, value 0x%04x\n", addr, value);

			if (addr == 0xffff)
			{
				msleep(value);
				printk("delay 0x%04x, value 0x%04x\n", addr, value);
			}	
			else
			{
				if( s5k4ecgx_write(c,addr, value) < 0 )
				{
					printk("<=PCAM=> %s fail on sensor_write\n", __func__);
				}
			}
		}
		else
			printk(KERN_ERR " EXCEPTION! reg value : %c  addr : 0x%x,  value : 0x%x\n", *reg, addr, value);
	}
	printk(KERN_ERR "***** Writing [%s] Ended\n",name);

	return 0;
}

/* From Palladio which reads "//#  define  MAX_VALUE 0x0A20 "*/
static short s5k4ecgx_regs_max_value(char *name)
{

	char *start, *reg;	
	unsigned short value;
	char data_buf[7];
	
	*(data_buf + 6) = '\0';

	start = strstr(s5k4ecgx_regs_table, name);	

	/* Find Address */	
	reg = strstr(start," 0x");		
	if (reg == NULL)
		return 0;

	/* Write Value to Address */	
	if (reg != NULL) {
		memcpy(data_buf, (reg + 1), 6);	
		value = (unsigned short)simple_strtoul(data_buf, NULL, 16); /*Change To HEX value*/
	}

	printk("s5k4ecgx_regs_max_value done\n");

	return value;

}

#endif  /* CONFIG_LOAD_FILE */


static int s5k4ecgx_detect(struct i2c_client *client)
{
	//int ret;
	//struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	u16 ID = 0xFFFF;
	s5k4ecgx_write(client,0x002C, 0x7000);
	s5k4ecgx_write(client,0x002E, 0x01A6);
	s5k4ecgx_read(client, 0x0F12, &ID);	
	
	if(ID == 0x0011) 
	{
		printk(S5K4ECGX_MOD_NAME"========================================\n");
		printk(S5K4ECGX_MOD_NAME"   [5MEGA CAM] vendor_id ID : 0x%04X\n", ID);
		printk(S5K4ECGX_MOD_NAME"========================================\n");
	} 
	else 
	{
		printk(S5K4ECGX_MOD_NAME"-------------------------------------------------\n");
		printk(S5K4ECGX_MOD_NAME"   [5MEGA CAM] sensor detect failure !!\n");
		printk(S5K4ECGX_MOD_NAME"   ID : 0x%04X[ID should be 0x0011]\n", ID);
		printk(S5K4ECGX_MOD_NAME"-------------------------------------------------\n");
		return -EINVAL;
	}	
	
	return 0;
}

#if 0
/*
 * Write a list of register settings; ff/ff stops the process.
 */
static int s5k4ecgx_write_array(struct i2c_client *c, tagCamReg32_t *vals, u32 reg_length)
{
	int i = 0;
//	u16 iic_data[3000] = {0};
//	u16 iic_length = 2;
	
//	iic_data[0]=0x0F;
//	iic_data[1]=0x12;

//	while((vals->reg_num != 0xFFFF) || (vals->value != 0xFFFF)) 
	for(i=0;i<reg_length;i++){
#if 0
		if( vals->reg_num == 0x0F12 )
		{	
			iic_data[iic_length] = vals->value >>8;
			iic_data[iic_length+1] = vals->value & 0x00FF;
			iic_length = iic_length+2;
		}
		else
		{
			if(iic_length !=2)
			{
			i2c_master_send(c, iic_data, iic_length);
			iic_length =2;
			}
#endif
			int ret = s5k4ecgx_write(c, vals[i].addr, vals[i].value);
			if (ret < 0)
			{
				printk(KERN_NOTICE "======[s5k4ecgx_write_array %d]====== \n", ret );	
	return ret;
}
#if 0
		}
#endif
		//vals++;

	}
/*
	while (vals->reg_num1 != 0xff || vals->reg_num2 != 0xff) {		
		int ret = s5k4ecgx_write(c, vals->reg_num1,vals->reg_num2, vals->value1,vals->value2);
		if (ret < 0)
			return ret;
		vals++;
	}
 */
	return 0;
}
#endif
#if 1
static int s5k4ecgx_write_regs(struct i2c_client *c, tagCamReg32_t *vals, u32 reg_length, char *name)
{
	int i = 0, ret=0;

#ifdef CONFIG_LOAD_FILE
	printk(KERN_NOTICE "======[Length %d : Line %d]====== \n", reg_length, __LINE__);
	ret = s5k4ecgx_regs_table_write(c, name);
#else
	u8 iic_data[3000] = {0};
	u16 iic_length = 2;

//	while((vals->reg_num != 0xFFFF) || (vals->value != 0xFFFF)) 
	for(i=0;i<reg_length;i++){
		if( vals[i].addr == 0x0F12 )
		{	
			iic_data[iic_length] = vals[i].value >>8;
			iic_data[iic_length+1] = vals[i].value & 0x00FF;
				
			iic_length = iic_length+2;
			
			if(i==(reg_length-1))
			{
				iic_data[0]=0x0F;
				iic_data[1]=0x12;
				i2c_master_send(c, iic_data, iic_length);
				iic_length =2;
			}
		}
		else
		{
			if(iic_length !=2)
			{
				iic_data[0]=0x0F;
				iic_data[1]=0x12;
			i2c_master_send(c, iic_data, iic_length);
			iic_length =2;
			}
			ret = s5k4ecgx_write(c, vals[i].addr, vals[i].value);
			if (ret < 0)
			{
				printk(KERN_NOTICE "======[s5k4ecgx_write_array %d]====== \n", ret );	
				return ret;
			}
		}
		//vals++;

	}
/*
	while (vals->reg_num1 != 0xff || vals->reg_num2 != 0xff) {		
		int ret = s5k4ecgx_write(c, vals->reg_num1,vals->reg_num2, vals->value1,vals->value2);
		if (ret < 0)
			return ret;
		vals++;
	}
*/
#endif /*CONFIG_LOAD_FILE*/

	return 0;
}
#endif
#if 0
static int s5k4ecgx_read_regs(struct i2c_client *c, tagCamReg32_t *vals, u32 reg_length, char *name)
{
	int i = 0, ret=0;
	u16 regval;
	
	for(i=0;i<reg_length;i++){
		
			ret = s5k4ecgx_read(c, vals[i].addr, &regval);
			if (ret < 0)
			{
				printk(KERN_NOTICE "======[s5k4ecgx_read_regs %d]====== \n", ret );	
				return ret;
			} else
				printk(KERN_NOTICE "===s5k4ecgx_read_regs===reg: 0x%x, val =0x%x ====== \n", vals[i].addr,  regval);	
	}

	return 0;
}
#endif

static void s5k4ecgx_reset(struct i2c_client *client)
{
	msleep(1);
}
#if 0
static int s5k4ecgx_init(struct i2c_client *client)
{

	s5k4ecgx_write_regs(client, regs_s5k4ecgx_initialize, ARRAY_SIZE(regs_s5k4ecgx_initialize),"regs_s5k4ecgx_initialize");

	return 0;
}
#endif
static int s5k4ecgx_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int result =0;
	
#ifdef CONFIG_LOAD_FILE
	result = s5k4ecgx_regs_table_init();
	if (result > 0)
	{		
		Cam_Printk(KERN_ERR "***** s5k4ecgx_regs_table_init  FAILED. Check the Filie in MMC\n");
		return result;
	}
	result =0;
#endif
	
	result=s5k4ecgx_write_regs(c,regs_s5k4ecgx_init_arm,ARRAY_SIZE(regs_s5k4ecgx_init_arm),"regs_s5k4ecgx_init_arm");
	msleep(10);
	result=s5k4ecgx_write_regs(c,regs_s5k4ecgx_initialize,ARRAY_SIZE(regs_s5k4ecgx_initialize),"regs_s5k4ecgx_initialize");


	sensor->state 		= S5K4ECGX_STATE_PREVIEW;
	sensor->mode 		= S5K4ECGX_MODE_CAMERA;
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
	sensor->initial			= S5K4ECGX_STATE_INITIAL;
	sensor->flash_mode      =S5K4ECGX_FLASH_OFF;
	
	gflash_status = FALSE;
	need_flash = FALSE;
	
	Cam_Printk(KERN_NOTICE "===s5k4ecgx_init===[%s  %d]====== \n", __FUNCTION__, __LINE__);
	
	return result;
}


/*
 * Store information about the video data format.  The color matrix
 * is deeply tied into the format, so keep the relevant values here.
 * The magic matrix nubmers come from OmniVision.
 */
static struct s5k4ecgx_format_struct {
	__u8 *desc;
	__u32 pixelformat;
	int bpp;   /* bits per pixel */
} s5k4ecgx_formats[] = {
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
#define N_S5K4ECGX_FMTS ARRAY_SIZE(s5k4ecgx_formats)
/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct s5k4ecgx_win_size {
	int	width;
	int	height;
} s5k4ecgx_win_sizes[] = {
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
	/* 720*480 */
	{
		.width		= D1_WIDTH,
		.height		= D1_HEIGHT,
	},
	/* 704*576 */
	{
		.width		= WIDTH_704,
		.height		= HEIGHT_576,
	},
	/* 800*480 */
	{
		.width		= Wide4K_WIDTH,
		.height		= Wide4K_HEIGHT,
	},
	/* ROTATED_QCIF For VT */
	{
		.width		= ROTATED_QCIF_WIDTH,
		.height		= ROTATED_QCIF_HEIGHT,
	},
	/* THUMB For Shot to Shot*/
	{
		.width		= THUMB_WIDTH,
		.height		= THUMB_HEIGHT,
	},
	/* For YUV Capture : Nevo-TD */
	/* QXGA */
	{
		.width          = QXGA_WIDTH,
		.height         = QXGA_HEIGHT,
	},
	/* UXGA */
	{
		.width          = UXGA_WIDTH,
		.height         = UXGA_HEIGHT,
	},
	/* SXGA */
	{
		.width          = SXGA_WIDTH,
		.height         = SXGA_HEIGHT,
	}, 
	/* WQXGA */
	{
		.width		= WQXGA_WIDTH,
		.height		= WQXGA_HEIGHT,
	},
	/* 4M Wide */
	{
		.width		= Wide4M_WIDTH,
		.height		= Wide4M_HEIGHT,
	},
	/* 2.4M Wide */
	{
		.width		= Wide2_4M_WIDTH,
		.height		= Wide2_4M_HEIGHT,
	},
	/* 1.5M Wide */
	{
		.width		= Wide1_5M_WIDTH,
		.height		= Wide1_5M_HEIGHT,
	},
	/* 4K Wide */
	{
		.width		= Wide4K_WIDTH,
		.height		= Wide4K_HEIGHT,
	},		
	/* HD */
	{
		.width    = HD_WIDTH,
		.height   = HD_HEIGHT,
	}, 	
};

static struct s5k4ecgx_win_size  s5k4ecgx_win_sizes_jpeg[] = {
	/* QXGA */
	{
		.width          = QXGA_WIDTH,
		.height         = QXGA_HEIGHT,
	},
	/* UXGA */
	{
		.width          = UXGA_WIDTH,
		.height         = UXGA_HEIGHT,
	},
	/* SXGA */
	{
		.width          = SXGA_WIDTH,
		.height         = SXGA_HEIGHT,
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
	/* WQXGA */
	{
		.width		= WQXGA_WIDTH,
		.height		= WQXGA_HEIGHT,
	},
	/* 4M Wide */
	{
		.width		= Wide4M_WIDTH,
		.height		= Wide4M_HEIGHT,
	},
	/* 2.4M Wide */
	{
		.width		= Wide2_4M_WIDTH,
		.height		= Wide2_4M_HEIGHT,
	},
	/* 1.5M Wide */
	{
		.width		= Wide1_5M_WIDTH,
		.height		= Wide1_5M_HEIGHT,
	},
	/* 4K Wide */
	{
		.width		= Wide4K_WIDTH,
		.height		= Wide4K_HEIGHT,
	},
};

/* Find a data format by a pixel code in an array */
static const struct s5k4ecgx_datafmt *s5k4ecgx_find_datafmt(
	enum v4l2_mbus_pixelcode code, const struct s5k4ecgx_datafmt *fmt,
	int n)
{
	int i;
	for (i = 0; i < n; i++)
		if (fmt[i].code == code)
			return fmt + i;

	return NULL;
}

#define N_WIN_SIZES (ARRAY_SIZE(s5k4ecgx_win_sizes))
/*
 * Store a set of start/stop values into the camera.
 */
static int s5k4ecgx_set_hw(struct i2c_client *client, int hstart, int hstop,
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

static int s5k4ecgx_querycap(struct i2c_client *c, struct v4l2_capability *argp)
{
	if(!argp){
		printk(KERN_ERR" argp is NULL %s %d \n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}
	strcpy(argp->driver, "s5k4ecgx");
	strcpy(argp->card, "TD/TTC");
	return 0;
}

static int s5k4ecgx_enum_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(s5k4ecgx_colour_fmts))
		return -EINVAL;
	*code = s5k4ecgx_colour_fmts[index].code;
	return 0;
}

static int s5k4ecgx_enum_fsizes(struct v4l2_subdev *sd,
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
		if (fsize->index >= ARRAY_SIZE(s5k4ecgx_win_sizes)) {
			dev_warn(&client->dev,
				"s5k4ecgx unsupported size %d!\n", fsize->index);
			return -EINVAL;
		}
		fsize->discrete.height = s5k4ecgx_win_sizes[fsize->index].height;
		fsize->discrete.width = s5k4ecgx_win_sizes[fsize->index].width;
		break;
#if 0
	case V4L2_MBUS_FMT_JPEG_1X8:
		if (fsize->index >= ARRAY_SIZE(s5k4ecgx_win_sizes_jpeg)) {
			dev_warn(&client->dev,
				"s5k4ecgx unsupported jpeg size %d!\n",
				fsize->index);
			return -EINVAL;
		}
		fsize->discrete.height =
			s5k4ecgx_win_sizes_jpeg[fsize->index].height;
		fsize->discrete.width =
			s5k4ecgx_win_sizes_jpeg[fsize->index].width;
		break;
#endif
	default:
		dev_err(&client->dev, "s5k4ecgx unsupported format!\n");
		return -EINVAL;
	}
	return 0;
}

static int s5k4ecgx_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
 //static int s5k4ecgx_try_fmt(struct i2c_client *c, struct v4l2_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct s5k4ecgx_datafmt *fmt;
	int i;

	fmt = s5k4ecgx_find_datafmt(mf->code, s5k4ecgx_colour_fmts,
				   ARRAY_SIZE(s5k4ecgx_colour_fmts));
	if (!fmt) {
		dev_err(&client->dev, "s5k4ecgx unsupported color format!\n");
		return -EINVAL;
	}

	mf->field = V4L2_FIELD_NONE;

	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
		/* enum the supported sizes*/
		for (i = 0; i < ARRAY_SIZE(s5k4ecgx_win_sizes); i++)
			if (mf->width == s5k4ecgx_win_sizes[i].width
				&& mf->height == s5k4ecgx_win_sizes[i].height)
				break;

		if (i >= ARRAY_SIZE(s5k4ecgx_win_sizes)) {
			dev_err(&client->dev, "s5k4ecgx unsupported window"
				"size, w%d, h%d!\n", mf->width, mf->height);
			return -EINVAL;
		}
		mf->colorspace = V4L2_COLORSPACE_JPEG;
		break;
#if 0
	case V4L2_MBUS_FMT_JPEG_1X8:
		/* enum the supported sizes for JPEG*/
		for (i = 0; i < ARRAY_SIZE(s5k4ecgx_win_sizes_jpeg); i++)
			if (mf->width == s5k4ecgx_win_sizes_jpeg[i].width &&
				mf->height == s5k4ecgx_win_sizes_jpeg[i].height)
				break;

		if (i >= ARRAY_SIZE(s5k4ecgx_win_sizes_jpeg)) {
			dev_err(&client->dev, "s5k4ecgx unsupported jpeg size!\n");
			return -EINVAL;
		}
		mf->colorspace = V4L2_COLORSPACE_JPEG;
		break;
#endif

	default:
		dev_err(&client->dev, "s5k4ecgx doesn't support code"
				"%d\n", mf->code);
		break;
	}
	return 0;
}


/*
 * Set a format.
 */

static int s5k4ecgx_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	int ret = 0;
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	const struct s5k4ecgx_datafmt *fmt;
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	
	printk("[DHL]s5k4ecgx_s_fmt..!!! \n");
	printk("[DHL]mf->code : [%d] \n",mf->code);
	printk("[DHL]mf->width : [%d] \n",mf->width);

	fmt =s5k4ecgx_find_datafmt(mf->code,s5k4ecgx_colour_fmts,
				   ARRAY_SIZE(s5k4ecgx_colour_fmts));
	if (!fmt) {
		dev_err(&c->dev, "s5k4ecgx unsupported color format!\n");
		return -EINVAL;
	}
	
	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
			sensor->pix.pixelformat = V4L2_PIX_FMT_YUV422P;
			if(s5k4ecgx_cam_state == S5K4ECGX_STATE_PREVIEW)
			{
			    	switch (mf->width)
				{
						case ROTATED_QCIF_WIDTH:
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_144_176, ARRAY_SIZE(regs_s5k4ecgx_preview_144_176),"regs_s5k4ecgx_preview_144_176");		
							Cam_Printk(KERN_ERR"choose rotated(144x176) qcif Preview setting \n");
							break;
							
						case QCIF_WIDTH:
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_176_144, ARRAY_SIZE(regs_s5k4ecgx_preview_176_144),"regs_s5k4ecgx_preview_176_144");					
							Cam_Printk(KERN_ERR"choose qcif(176x144) Preview setting \n");
							break;

						case QVGA_WIDTH:
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_320_240, ARRAY_SIZE(regs_s5k4ecgx_preview_320_240),"regs_s5k4ecgx_preview_320_240");					
							Cam_Printk(KERN_ERR"choose qvga(320x240) Preview setting \n");
							break;

						case CIF_WIDTH:
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_320_240, ARRAY_SIZE(regs_s5k4ecgx_preview_352_288),"regs_s5k4ecgx_preview_352_288");					
							Cam_Printk(KERN_ERR"choose qvga(320x240) Preview setting \n");
							break;

						case WIDTH_704:
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_704_576, ARRAY_SIZE(regs_s5k4ecgx_preview_704_576),"regs_s5k4ecgx_preview_704_576");					
							Cam_Printk(KERN_ERR"choose qvga(704x576) Preview setting \n");
							break;

						case D1_WIDTH:
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_720_480, ARRAY_SIZE(regs_s5k4ecgx_preview_720_480),"regs_s5k4ecgx_preview_720_480");					
							Cam_Printk(KERN_ERR"choose d1(720x480) Preview setting \n");
							break;

						case VGA_WIDTH:
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_640_480, ARRAY_SIZE(regs_s5k4ecgx_preview_640_480),"regs_s5k4ecgx_preview_640_480");					
							Cam_Printk(KERN_ERR"choose vga(640x480) Preview setting \n");
							break;

						case Wide4K_WIDTH:
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_800_480, ARRAY_SIZE(regs_s5k4ecgx_preview_800_480),"regs_s5k4ecgx_preview_800_480");					
							Cam_Printk(KERN_ERR"choose WVGA(800x480) Preview setting \n");
							break;

						case HD_WIDTH:
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_1280_720, ARRAY_SIZE(regs_s5k4ecgx_preview_1280_720),"regs_s5k4ecgx_preview_1280_720");					
							Cam_Printk(KERN_ERR"choose WVGA(1280x720) Preview setting \n");
							break;

						default:
							printk("\n unsupported size for preview! %s %d w=%d h=%d\n", __FUNCTION__, __LINE__, mf->width, mf->height);
							goto out;
							break;
			    	}
			}
			else if(s5k4ecgx_cam_state == S5K4ECGX_STATE_CAPTURE)
			{
				switch (mf->width) 			
				{			
					case VGA_WIDTH: // VGA Capture
						s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_640_480, ARRAY_SIZE(regs_s5k4ecgx_capture_640_480),"regs_s5k4ecgx_capture_640_480");					
						Cam_Printk(KERN_ERR"choose vga(640x480) jpeg setting \n");
						break;
						
					case Wide4K_WIDTH: // 4K Wide Capture
						s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_800_480, ARRAY_SIZE(regs_s5k4ecgx_capture_800_480),"regs_s5k4ecgx_capture_800_480");					
						Cam_Printk(KERN_ERR"choose 4K(800x480) Wide jpeg setting \n");
						break;

					case QXGA_WIDTH:
						if((mf->height)==QXGA_HEIGHT){// 3M Capture
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_2048_1536, ARRAY_SIZE(regs_s5k4ecgx_capture_2048_1536),"regs_s5k4ecgx_capture_2048_1536"); 	
							Cam_Printk(KERN_ERR"choose 3M(2048x1536) jpeg setting \n");
						}
						else if((mf->height)==Wide2_4M_HEIGHT){//2.4M  Wide Capture
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_2048_1232, ARRAY_SIZE(regs_s5k4ecgx_capture_2048_1232),"regs_s5k4ecgx_capture_2048_1232"); 	
							Cam_Printk(KERN_ERR"choose 2.4M(2048x1232) Wide jpeg setting \n");
						}
						break;
						
					case UXGA_WIDTH: // 2M Capture
						if((mf->height)==UXGA_HEIGHT){// 1.5M Capture
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_1600_1200, ARRAY_SIZE(regs_s5k4ecgx_capture_1600_1200),"regs_s5k4ecgx_capture_1600_1200");					
							Cam_Printk(KERN_ERR"choose 2M(1600x1200) jpeg setting \n");
						}else if((mf->height)==Wide1_5M_HEIGHT){// 1.5M Capture
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_1600_960, ARRAY_SIZE(regs_s5k4ecgx_capture_1600_960),"regs_s5k4ecgx_capture_1600_960");					
							Cam_Printk(KERN_ERR"choose 1.5M(1600x960) jpeg setting \n");
						}
						break;
						
					case WQXGA_WIDTH: 
						if((mf->height)==WQXGA_HEIGHT){// 5M Capture
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_2560_1920, ARRAY_SIZE(regs_s5k4ecgx_capture_2560_1920),"regs_s5k4ecgx_capture_2560_1920"); 	
							Cam_Printk(KERN_ERR"choose 5M(2560x1920) jpeg setting \n");
						}
						else if((mf->height)==Wide4M_HEIGHT){//4M Wide Capture
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_2560_1536, ARRAY_SIZE(regs_s5k4ecgx_capture_2560_1536),"regs_s5k4ecgx_capture_2560_1536"); 	
							Cam_Printk(KERN_ERR"choose 4M(2560x1536) Wide jpeg setting \n");
						}
						break;
	
					default:
						printk("\n unsupported size for capture! %s %d w=%d h=%d\n", __FUNCTION__, __LINE__, mf->width, mf->height);
						goto out;
						break;
				}

				Cam_Printk(KERN_NOTICE "Start to Capture \n");

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
static int s5k4ecgx_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
//	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_captureparm *cp = &parms->parm.capture;
//	u16 clkrc;
//	int ret;

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	//ret = s5k4ecgx_read(c, REG_CLKRC, &clkrc);
	//if (ret < 0)
	//	return ret;
	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.numerator = 1;
	cp->timeperframe.denominator = S5K4ECGX_FRAME_RATE;
	//if ((clkrc & CLK_EXT) == 0 && (clkrc & CLK_SCALE) > 1)
	//	cp->timeperframe.denominator /= (clkrc & CLK_SCALE);
	return 0;
}

static int s5k4ecgx_s_parm(struct i2c_client *c, struct v4l2_streamparm *parms)
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
static const int s5k4ecgx_sin_table[] = {
	0,	 87,   173,   258,   342,   422,
	499,	573,   642,   707,   766,   819,
	866,	906,   939,   965,   984,   996,
	1000
};

static int s5k4ecgx_sine(int theta)
{
	int chs = 1;
	int sine;

	if (theta < 0) {
		theta = -theta;
		chs = -1;
	}
	if (theta <= 90)
		sine = s5k4ecgx_sin_table[theta/SIN_STEP];
	else {
		theta -= 90;
		sine = 1000 - s5k4ecgx_sin_table[theta/SIN_STEP];
	}
	return sine*chs;
}

static int s5k4ecgx_cosine(int theta)
{
	theta = 90 - theta;
	if (theta > 180)
		theta -= 360;
	else if (theta < -180)
		theta += 360;
	return s5k4ecgx_sine(theta);
}

static int s5k4ecgx_calc_cmatrix(struct s5k4ecgx_info *info,
		int matrix[CMATRIX_LEN])
{
	return 0;
}

static int s5k4ecgx_t_saturation(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->saturation;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case SATURATION_MINUS_2:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_saturation_level_0, ARRAY_SIZE(regs_s5k4ecgx_saturation_level_0),"regs_s5k4ecgx_saturation_level_0");			
			break;

		case SATURATION_MINUS_1:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_saturation_level_1, ARRAY_SIZE(regs_s5k4ecgx_saturation_level_1),"regs_s5k4ecgx_saturation_level_1");			
			break;		

		case SATURATION_DEFAULT:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_saturation_level_2, ARRAY_SIZE(regs_s5k4ecgx_saturation_level_2),"regs_s5k4ecgx_saturation_level_2");			
			break;	

		case SATURATION_PLUS_1:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_saturation_level_3, ARRAY_SIZE(regs_s5k4ecgx_saturation_level_3),"regs_s5k4ecgx_saturation_level_3");			
			break;		

		case SATURATION_PLUS_2:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_saturation_level_4, ARRAY_SIZE(regs_s5k4ecgx_saturation_level_4),"regs_s5k4ecgx_saturation_level_4");			
			break;	

		default:
			printk(S5K4ECGX_MOD_NAME "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->saturation = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->saturation);
	return 0;
}

static int s5k4ecgx_q_saturation(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_q_saturation is called...\n"); 
	value = sensor->saturation;
	return 0;
}


static int s5k4ecgx_t_hue(struct i2c_client *client, int value)
{
	return 0;
}


static int s5k4ecgx_q_hue(struct i2c_client *client, __s32 *value)
{
	return 0;
}


/*
 * Some weird registers seem to store values in a sign/magnitude format!
 */
static unsigned char s5k4ecgx_sm_to_abs(unsigned char v)
{
	if ((v & 0x80) == 0)
		return v + 128;
	else
		return 128 - (v & 0x7f);
}


static unsigned char s5k4ecgx_abs_to_sm(unsigned char v)
{
	if (v > 127)
		return v & 0x7f;
	else
		return (128 - v) | 0x80;
}

static int s5k4ecgx_t_brightness(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->ev;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case EV_MINUS_4:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_brightness_level_0, ARRAY_SIZE(regs_s5k4ecgx_brightness_level_0),"regs_s5k4ecgx_brightness_level_0");
			break;

		case EV_MINUS_3:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_brightness_level_1, ARRAY_SIZE(regs_s5k4ecgx_brightness_level_1),"regs_s5k4ecgx_brightness_level_1");
			break;		

		case EV_MINUS_2:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_brightness_level_2, ARRAY_SIZE(regs_s5k4ecgx_brightness_level_2),"regs_s5k4ecgx_brightness_level_2");
			break;	

		case EV_MINUS_1:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_brightness_level_3, ARRAY_SIZE(regs_s5k4ecgx_brightness_level_3),"regs_s5k4ecgx_brightness_level_3");
			break;	
		
		case EV_DEFAULT:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_brightness_level_4, ARRAY_SIZE(regs_s5k4ecgx_brightness_level_4),"regs_s5k4ecgx_brightness_level_4");
			break;

		case EV_PLUS_1:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_brightness_level_5, ARRAY_SIZE(regs_s5k4ecgx_brightness_level_5),"regs_s5k4ecgx_brightness_level_5");
			break;

		case EV_PLUS_2:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_brightness_level_6, ARRAY_SIZE(regs_s5k4ecgx_brightness_level_6),"regs_s5k4ecgx_brightness_level_6");
			break;

		case EV_PLUS_3:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_brightness_level_7, ARRAY_SIZE(regs_s5k4ecgx_brightness_level_7),"regs_s5k4ecgx_brightness_level_7");
			break;

		case EV_PLUS_4:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_brightness_level_8, ARRAY_SIZE(regs_s5k4ecgx_brightness_level_8),"regs_s5k4ecgx_brightness_level_8");
			break;

		default:
			printk(S5K4ECGX_MOD_NAME "Scene value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->ev = value;
	Cam_Printk(KERN_NOTICE "%s success [scene:%d]\n",__func__, sensor->scene);
	return 0;
}

static int s5k4ecgx_q_brightness(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_get_scene is called...\n"); 
	value = sensor->ev;
	return 0;
}

static int s5k4ecgx_t_contrast(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->contrast;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case CONTRAST_MINUS_2:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_contrast_level_0, ARRAY_SIZE(regs_s5k4ecgx_contrast_level_0),"regs_s5k4ecgx_contrast_level_0");			
			break;

		case CONTRAST_MINUS_1:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_contrast_level_1, ARRAY_SIZE(regs_s5k4ecgx_contrast_level_1),"regs_s5k4ecgx_contrast_level_1");			
			break;		

		case CONTRAST_DEFAULT:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_contrast_level_2, ARRAY_SIZE(regs_s5k4ecgx_contrast_level_2),"regs_s5k4ecgx_contrast_level_2");			
			break;	

		case CONTRAST_PLUS_1:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_contrast_level_3, ARRAY_SIZE(regs_s5k4ecgx_contrast_level_3),"regs_s5k4ecgx_contrast_level_3");			
			break;		

		case CONTRAST_PLUS_2:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_contrast_level_4, ARRAY_SIZE(regs_s5k4ecgx_contrast_level_4),"regs_s5k4ecgx_contrast_level_4");			
			break;	

		default:
			printk(S5K4ECGX_MOD_NAME "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->contrast = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->quality);
	return 0;
}

static int s5k4ecgx_q_contrast(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_q_quality is called...\n"); 
	value = sensor->contrast;
	return 0;
}

static int s5k4ecgx_q_hflip(struct i2c_client *client, __s32 *value)
{
	return 0;
}


static int s5k4ecgx_t_hflip(struct i2c_client *client, int value)
{
	return 0;
}



static int s5k4ecgx_q_vflip(struct i2c_client *client, __s32 *value)
{
	return 0;
}


static int s5k4ecgx_t_vflip(struct i2c_client *client, int value)
{
	return 0;
}

static int s5k4ecgx_t_scene(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->scene;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	if(value != SCENE_OFF){
		s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_off,ARRAY_SIZE(regs_s5k4ecgx_scene_off),"regs_s5k4ecgx_scene_off");		
		Cam_Printk(KERN_NOTICE "regs_s5k4ecgx_scene_off");
	}
	s5k4ecgx_write_regs(client,regs_s5k4ecgx_effect_off,ARRAY_SIZE(regs_s5k4ecgx_effect_off),"regs_s5k4ecgx_effect_off");		
	switch(value)
	{
		case SCENE_OFF:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_off,ARRAY_SIZE(regs_s5k4ecgx_scene_off),"regs_s5k4ecgx_scene_off");		
			break;
		case SCENE_PORTRAIT:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_portrait,ARRAY_SIZE(regs_s5k4ecgx_scene_portrait),"regs_s5k4ecgx_scene_portrait");		
			break;		
		case SCENE_LANDSCAPE:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_landscape,ARRAY_SIZE(regs_s5k4ecgx_scene_landscape),"regs_s5k4ecgx_scene_landscape");		
			break;	
		case SCENE_SPORTS:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_sports,ARRAY_SIZE(regs_s5k4ecgx_scene_sports),"regs_s5k4ecgx_scene_sports");		
			break;			
		case SCENE_PARTY:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,5,0);
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_party,ARRAY_SIZE(regs_s5k4ecgx_scene_party),"regs_s5k4ecgx_scene_party");		
			break;
		case SCENE_BEACH:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,5,0);
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_beach ,ARRAY_SIZE(regs_s5k4ecgx_scene_beach),"regs_s5k4ecgx_scene_beach");		
			break;
		case SCENE_SUNSET:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_sunset,ARRAY_SIZE(regs_s5k4ecgx_scene_sunset),"regs_s5k4ecgx_scene_sunset");		
			break;
		case SCENE_DAWN:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_dawn,ARRAY_SIZE(regs_s5k4ecgx_scene_dawn),"regs_s5k4ecgx_scene_dawn");		
			break;
		case SCENE_FALL:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_fall,ARRAY_SIZE(regs_s5k4ecgx_scene_fall),"regs_s5k4ecgx_scene_fall");		
			break;
		case SCENE_NIGHT:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_night,ARRAY_SIZE(regs_s5k4ecgx_scene_night),"regs_s5k4ecgx_scene_night");		
			break;
		case SCENE_BACKLIGHT:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_backlight,ARRAY_SIZE(regs_s5k4ecgx_scene_backlight),"regs_s5k4ecgx_scene_backlight");		
			break;
		case SCENE_FIRE:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_fire,ARRAY_SIZE(regs_s5k4ecgx_scene_fire),"regs_s5k4ecgx_scene_fire");		
			break;
		case SCENE_TEXT:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_text,ARRAY_SIZE(regs_s5k4ecgx_scene_text),"regs_s5k4ecgx_scene_text");		
			break;
		case SCENE_CANDLE:
			s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_canddle,ARRAY_SIZE(regs_s5k4ecgx_scene_canddle),"regs_s5k4ecgx_scene_canddle");		
			break;
		default:
			printk(S5K4ECGX_MOD_NAME "Scene value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->scene = value;
	Cam_Printk(KERN_NOTICE "%s success [scene:%d]\n",__func__, sensor->scene);
	return 0;
}

static int s5k4ecgx_q_scene(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_get_scene is called...\n"); 
	value = sensor->scene;
	return 0;
}

static int s5k4ecgx_t_whitebalance(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->wb;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case WB_AUTO:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,3,1);
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_whitebalance_auto, ARRAY_SIZE(regs_s5k4ecgx_whitebalance_auto),"regs_s5k4ecgx_whitebalance_auto");			
			break;

		case WB_DAYLIGHT:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,3,0);
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_whitebalance_daylight, ARRAY_SIZE(regs_s5k4ecgx_whitebalance_daylight),"regs_s5k4ecgx_whitebalance_daylight");			
			break;		

		case WB_CLOUDY:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,3,0);
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_whitebalance_cloudy, ARRAY_SIZE(regs_s5k4ecgx_whitebalance_cloudy),"regs_s5k4ecgx_whitebalance_cloudy");			
			break;	

		case WB_FLUORESCENT:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,3,0);
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_whitebalance_fluorescent, ARRAY_SIZE(regs_s5k4ecgx_whitebalance_fluorescent),"regs_s5k4ecgx_whitebalance_fluorescent");			
			break;	
		
		case WB_INCANDESCENT:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,3,0);
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_whitebalance_incandescent, ARRAY_SIZE(regs_s5k4ecgx_whitebalance_incandescent),"regs_s5k4ecgx_whitebalance_incandescent");			
			break;

		default:
			printk(S5K4ECGX_MOD_NAME "White Balance value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->wb = value;
	Cam_Printk(KERN_NOTICE "%s success [White Balance e:%d]\n",__func__, sensor->wb);
	return 0;
}

static int s5k4ecgx_q_whitebalance(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_get_whitebalance is called...\n"); 
	value = sensor->wb;
	return 0;
}

static int s5k4ecgx_t_effect(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->effect;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case EFFECT_OFF:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_effect_off, ARRAY_SIZE(regs_s5k4ecgx_effect_off),"regs_s5k4ecgx_effect_off");			
			break;

		case EFFECT_MONO:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_effect_mono, ARRAY_SIZE(regs_s5k4ecgx_effect_mono),"regs_s5k4ecgx_effect_mono");			
			break;		

		case EFFECT_SEPIA:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_effect_sepia, ARRAY_SIZE(regs_s5k4ecgx_effect_sepia),"regs_s5k4ecgx_effect_sepia");			
			break;	

		case EFFECT_NEGATIVE:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_effect_negative, ARRAY_SIZE(regs_s5k4ecgx_effect_negative),"regs_s5k4ecgx_effect_negative");			
			break;	
		
		case EFFECT_AQUA:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_effect_aqua, ARRAY_SIZE(regs_s5k4ecgx_effect_aqua),"regs_s5k4ecgx_effect_aqua");			
			break;

		case EFFECT_SKETCH:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_effect_sketch, ARRAY_SIZE(regs_s5k4ecgx_effect_sketch),"regs_s5k4ecgx_effect_sketch");			
			break;

		default:
			printk(S5K4ECGX_MOD_NAME "Sketch value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->effect = value;
	Cam_Printk(KERN_NOTICE "%s success [Effect e:%d]\n",__func__, sensor->effect);
	return 0;
}

static int s5k4ecgx_q_effect(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_get_whitebalance is called...\n"); 
	value = sensor->effect;
	return 0;
}

static int s5k4ecgx_t_ISO(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->iso;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case ISO_AUTO:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,5,1);
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_ISO_auto, ARRAY_SIZE(regs_s5k4ecgx_ISO_auto),"regs_s5k4ecgx_ISO_auto");			
			break;

		case ISO_50:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,5,0);
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_ISO_50, ARRAY_SIZE(regs_s5k4ecgx_ISO_50),"regs_s5k4ecgx_ISO_50");			
			break;		

		case ISO_100:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,5,0);
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_ISO_100, ARRAY_SIZE(regs_s5k4ecgx_ISO_100),"regs_s5k4ecgx_ISO_100");			
			break;	

		case ISO_200:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,5,0);
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_ISO_200, ARRAY_SIZE(regs_s5k4ecgx_ISO_200),"regs_s5k4ecgx_ISO_200");			
			break;	
		
		case ISO_400:
			s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,5,0);
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_ISO_400, ARRAY_SIZE(regs_s5k4ecgx_ISO_400),"regs_s5k4ecgx_ISO_400");			
			break;

		default:
			printk(S5K4ECGX_MOD_NAME "ISO value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->iso = value;
	Cam_Printk(KERN_NOTICE "%s success [ISO e:%d]\n",__func__, sensor->iso);
	return 0;
}

static int s5k4ecgx_q_ISO(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_q_ISO is called...\n"); 
	value = sensor->iso;
	return 0;
}

static int s5k4ecgx_t_photometry(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->photometry;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case METERING_MATRIX:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_metering_matrix, ARRAY_SIZE(regs_s5k4ecgx_metering_matrix),"regs_s5k4ecgx_metering_matrix");			
			break;

		case METERING_SPOT:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_metering_spot, ARRAY_SIZE(regs_s5k4ecgx_metering_spot),"regs_s5k4ecgx_metering_spot");			
			break;		

		case METERING_CENTER:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_metering_centerweighted, ARRAY_SIZE(regs_s5k4ecgx_metering_centerweighted),"regs_s5k4ecgx_metering_centerweighted");			
			break;	

		default:
			printk(S5K4ECGX_MOD_NAME "ISO value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->photometry = value;
	Cam_Printk(KERN_NOTICE "%s success [PHOTOMERTY e:%d]\n",__func__, sensor->photometry);
	return 0;
}

static int s5k4ecgx_q_photometry(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_q_photometry is called...\n"); 
	value = sensor->photometry;
	return 0;
}

static int s5k4ecgx_t_quality(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->quality;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value)
	{
		case QUALITY_SUPERFINE:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_quality_superfine, ARRAY_SIZE(regs_s5k4ecgx_quality_superfine),"regs_s5k4ecgx_quality_superfine");
			break;

		case QUALITY_FINE:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_quality_fine, ARRAY_SIZE(regs_s5k4ecgx_quality_fine),"regs_s5k4ecgx_quality_fine");
			break;		

		case QUALITY_NORMAL:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_quality_normal, ARRAY_SIZE(regs_s5k4ecgx_quality_normal),"regs_s5k4ecgx_quality_normal");
			break;	

		default:
			printk(S5K4ECGX_MOD_NAME "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->quality = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->quality);
	return 0;
}

static int s5k4ecgx_q_quality(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_q_quality is called...\n"); 
	value = sensor->quality;
	return 0;
}


static int s5k4ecgx_t_sharpness(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->sharpness;

	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	
	switch(value)
	{
		case SHARPNESS_MINUS_2:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_sharpness_level_0, ARRAY_SIZE(regs_s5k4ecgx_sharpness_level_0),"regs_s5k4ecgx_sharpness_level_0");			
			break;

		case SHARPNESS_MINUS_1:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_sharpness_level_1, ARRAY_SIZE(regs_s5k4ecgx_sharpness_level_1),"regs_s5k4ecgx_sharpness_level_1");			
			break;		

		case SHARPNESS_DEFAULT:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_sharpness_level_2, ARRAY_SIZE(regs_s5k4ecgx_sharpness_level_2),"regs_s5k4ecgx_sharpness_level_2");			
			break;	

		case SHARPNESS_PLUS_1:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_sharpness_level_3, ARRAY_SIZE(regs_s5k4ecgx_sharpness_level_3),"regs_s5k4ecgx_sharpness_level_3");			
			break;		

		case SHARPNESS_PLUS_2:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_sharpness_level_4, ARRAY_SIZE(regs_s5k4ecgx_sharpness_level_4),"regs_s5k4ecgx_sharpness_level_4");			
			break;	

		default:
			printk(S5K4ECGX_MOD_NAME "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->sharpness = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->saturation);
	return 0;
}

static int s5k4ecgx_q_sharpness(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_q_sharpness is called...\n"); 
	value = sensor->sharpness;
	return 0;
}

static int s5k4ecgx_t_fps(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
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
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_auto_FPS, ARRAY_SIZE(regs_s5k4ecgx_auto_FPS),"regs_s5k4ecgx_auto_FPS");			
			break;
			
		case FPS_7:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_7_FPS, ARRAY_SIZE(regs_s5k4ecgx_7_FPS),"regs_s5k4ecgx_7_FPS");			
			break;

		case FPS_10:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_10_FPS, ARRAY_SIZE(regs_s5k4ecgx_10_FPS),"regs_s5k4ecgx_10_FPS");			
			break;

		case FPS_12:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_12_FPS, ARRAY_SIZE(regs_s5k4ecgx_12_FPS),"regs_s5k4ecgx_12_FPS");			
			break;

		case FPS_15:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_15_FPS, ARRAY_SIZE(regs_s5k4ecgx_15_FPS),"regs_s5k4ecgx_15_FPS");					
			break;		

		case FPS_20:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_20_FPS, ARRAY_SIZE(regs_s5k4ecgx_20_FPS),"regs_s5k4ecgx_20_FPS");					
			break;			

		case FPS_25:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_25_FPS, ARRAY_SIZE(regs_s5k4ecgx_25_FPS),"regs_s5k4ecgx_25_FPS");					
			break;	
			
		case FPS_30:
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_30_FPS, ARRAY_SIZE(regs_s5k4ecgx_30_FPS),"regs_s5k4ecgx_30_FPS");				
			break;	

		default:
			printk(KERN_NOTICE "quality value is not supported!!!\n");
		return -EINVAL;
	}

	sensor->fps = value;
	Cam_Printk(KERN_NOTICE "%s success [FPS e:%d]\n",__func__, sensor->fps);
	return 0;
}

static int s5k4ecgx_q_fps(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_q_fps is called...\n"); 
	value = sensor->fps;
	return 0;
}

static int s5k4ecgx_g_frame_time(struct i2c_client *client,__s32 *value)
{
	//struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
		u16 frame_time =0;
		u16 temp1 = 0;
		int err;
	
		Cam_Printk(KERN_NOTICE "[DHL:Test] s5k4ecgx_g_frame_time() \r\n");
	
		err = s5k4ecgx_write(client,0xFCFC, 0xD000);
		err = s5k4ecgx_write(client,0x002C, 0x7000);
	
		err = s5k4ecgx_write(client, 0x002E, 0x2128); 
		err = s5k4ecgx_read (client, 0x0F12, &temp1);
		
		frame_time = temp1/400;
	
		*value = frame_time;
	
		Cam_Printk(KERN_NOTICE "%s success [Frame Time :%d ]\n",__func__,frame_time);
	
		return 0;
}

static int s5k4ecgx_mode_switch_check(struct i2c_client *client,__s32 *value)
{
	//struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
		u16 frame_time =0;
		u16 temp1 = 0;
		int err;
	
		//Cam_Printk(KERN_NOTICE "[DHL:Test] s5k4ecgx_mode_switch_check() \r\n");
	
		err = s5k4ecgx_write(client,0xFCFC, 0xD000);
		err = s5k4ecgx_write(client,0x002C, 0x7000);
	
		err = s5k4ecgx_write(client, 0x002E, 0x215f); 
		err = s5k4ecgx_read (client, 0x0F12, &temp1);
	
		*value = temp1;
	
		//Cam_Printk(KERN_NOTICE "%s --------s5k4ecgx_mode_switch_check [ %d ]\n",__func__,temp1);
	
		return 0;
}


static int s5k4ecgx_get_frame_time(struct i2c_client *client)
{
	//struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	u16 frame_time =0;
	u16 temp1 = 0;
	int err;

	Cam_Printk(KERN_NOTICE "[DHL:Test] s5k4ecgx_g_frame_time() \r\n");

	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);

	err = s5k4ecgx_write(client, 0x002E, 0x2128); 
	err = s5k4ecgx_read (client, 0x0F12, &temp1);
	
	frame_time = temp1/400;
	
	Cam_Printk(KERN_NOTICE "%s success [Frame Time :%d ]\n",__func__,frame_time);

	return frame_time;
}

static int s5k4ecgx_g_lightness_check(struct i2c_client *client,__s32 *value)
{
	//struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	//u16 frame_time =0;
	u16 temp1,temp2,temp3;
	int err;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_g_frame_check() \r\n");

	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);

	err = s5k4ecgx_write(client, 0x002E, 0x2128); 
	err = s5k4ecgx_read (client, 0x0F12, &temp1);
	
	Cam_Printk(KERN_NOTICE "[DHL:Test] temp1 : %d \r\n",temp1);
	Cam_Printk(KERN_NOTICE "[DHL:Test] temp1 : 0x%x \r\n",temp1);

	temp2 = temp1/400;

	Cam_Printk(KERN_NOTICE "[DHL:Test] temp2 : %d \r\n",temp2);
	Cam_Printk(KERN_NOTICE "[DHL:Test] temp2 : 0x%x \r\n",temp2);	

	temp3 = 1000/temp2;

	Cam_Printk(KERN_NOTICE "[DHL:Test] temp3 : %d \r\n",temp3);
	Cam_Printk(KERN_NOTICE "[DHL:Test] temp3 : 0x%x \r\n",temp3);

	if (temp3 <= 10)
	{
		Cam_Printk(KERN_NOTICE "Lightness_Low.. \r\n");
		*value = Lightness_Low;
	}
	else 
	{
		Cam_Printk(KERN_NOTICE "Lightness_Normal.. \r\n");
		*value = Lightness_Normal;
	}
	Cam_Printk(KERN_NOTICE "%s success [Frame Time :%d ]\n",__func__,&value);

	return 0;

}

static int s5k4ecgx_ESD_check(struct i2c_client *client,  struct v4l2_control *ctrl)
{
	//struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	u16 esd_value1 = 0;
	u16 esd_value2 = 0;
	int err;
	
	Cam_Printk(KERN_NOTICE "s5k4ecgx_ESD_check() \r\n");

	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);
	err = s5k4ecgx_write(client,0x002E, 0x1002); 
	err = s5k4ecgx_read (client,0x0F12, &esd_value1);	

	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);
	err = s5k4ecgx_write(client,0x002E, 0x01A8); 
	err = s5k4ecgx_read (client,0x0F12, &esd_value2);	

	Cam_Printk(KERN_NOTICE "[DHL]esd_value1 : %d \r\n",esd_value1);
	Cam_Printk(KERN_NOTICE "[DHL]esd_value2 : %d \r\n",esd_value2);	

	if((esd_value1 != 0x0000)||(esd_value2 != 0xAAAA))
	{
		Cam_Printk(KERN_NOTICE "ESD Error is Happened..() \r\n");
		ctrl->value = ESD_ERROR;
		return 0;
	}

	Cam_Printk(KERN_NOTICE "s5k4ecgx_ESD_check is OK..() \r\n");
	ctrl->value = ESD_NONE;

	return 0;

}
static int s5k4ecgx_t_dtp_on(struct i2c_client *client)
{
	Cam_Printk(KERN_NOTICE "s5k4ecgx_t_dtp_stop is called...\n"); 
	s5k4ecgx_write_regs(client,regs_s5k4ecgx_DTP_on,ARRAY_SIZE(regs_s5k4ecgx_DTP_on),"regs_s5k4ecgx_DTP_on");

	return 0;
}

static int s5k4ecgx_t_dtp_stop(struct i2c_client *client)
{
	Cam_Printk(KERN_NOTICE "s5k4ecgx_t_dtp_stop is called...\n"); 
	s5k4ecgx_write_regs(client,regs_s5k4ecgx_DTP_off,ARRAY_SIZE(regs_s5k4ecgx_DTP_off),"regs_s5k4ecgx_DTP_off");

	return 0;
}

static int s5k4ecgx_g_exif_info(struct i2c_client *client,struct v4l2_exif_info *exif_info)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_g_exif_info is called...\n"); 
	*exif_info = sensor->exif_info;
	return 0;
}

static int s5k4ecgx_set_mode(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	
	sensor->mode = value;
	Cam_Printk(KERN_NOTICE, "s5k4ecgx_set_mode is called... mode = %d\n", sensor->mode);
	return 0;
}


static int s5k4ecgx_preview_size(struct i2c_client *client, int value)
	{
		struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	
		if(sensor->mode != S5K4ECGX_MODE_CAMCORDER)
		{
			switch (value) 
			{
				Cam_Printk(KERN_NOTICE "CAMERA MODE..\n"); 

				case S5K4ECGX_PREVIEW_SIZE_144_176:	
					s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_144_176,ARRAY_SIZE(regs_s5k4ecgx_preview_144_176),"regs_s5k4ecgx_preview_144_176");			
					break;
				case S5K4ECGX_PREVIEW_SIZE_176_144:
					s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_176_144,ARRAY_SIZE(regs_s5k4ecgx_preview_176_144),"regs_s5k4ecgx_preview_176_144");			
					break;
				case S5K4ECGX_PREVIEW_SIZE_320_240:
					s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_320_240,ARRAY_SIZE(regs_s5k4ecgx_preview_320_240),"regs_s5k4ecgx_preview_320_240");			
					break;
				case S5K4ECGX_PREVIEW_SIZE_640_480:
					s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_640_480,ARRAY_SIZE(regs_s5k4ecgx_preview_640_480),"regs_s5k4ecgx_preview_640_480");			
					break;
				case S5K4ECGX_PREVIEW_SIZE_720_480:
					s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_720_480,ARRAY_SIZE(regs_s5k4ecgx_preview_720_480),"regs_s5k4ecgx_preview_720_480");			
					break;
				case S5K4ECGX_PREVIEW_SIZE_800_480:
					s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_800_480,ARRAY_SIZE(regs_s5k4ecgx_preview_800_480),"regs_s5k4ecgx_preview_800_480");			
					break;
				default:
					// When running in image capture mode, the call comes here.
					// Set the default video resolution - S5K4ECGX_PREVIEW_VGA
					printk(S5K4ECGX_MOD_NAME "Preview Resolution is not supported! : %d\n",value);
					return 0;
			}
		} 
		else 
		{
			Cam_Printk(KERN_NOTICE "CAMCORDER MODE..\n"); 

			switch (value) {
			case S5K4ECGX_CAMCORDER_SIZE_176_144:
				s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_176_144,ARRAY_SIZE(regs_s5k4ecgx_preview_176_144),"regs_s5k4ecgx_preview_176_144");			
				break;
			case S5K4ECGX_CAMCORDER_SIZE_320_240:
				s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_320_240,ARRAY_SIZE(regs_s5k4ecgx_preview_320_240),"regs_s5k4ecgx_preview_320_240");			
				break;
			case S5K4ECGX_CAMCORDER_SIZE_640_480:
				s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_640_480,ARRAY_SIZE(regs_s5k4ecgx_preview_640_480),"regs_s5k4ecgx_preview_640_480");			
				break;
			case S5K4ECGX_CAMCORDER_SIZE_720_480:
				s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_720_480,ARRAY_SIZE(regs_s5k4ecgx_preview_720_480),"regs_s5k4ecgx_preview_720_480");			
				break;
			case S5K4ECGX_CAMCORDER_SIZE_800_480:
				s5k4ecgx_write_regs(client,regs_s5k4ecgx_preview_800_480,ARRAY_SIZE(regs_s5k4ecgx_preview_800_480),"regs_s5k4ecgx_preview_800_480");			
				break;
			default:
				// When running in image capture mode, the call comes here.
				// Set the default video resolution - S5K4ECGX_PREVIEW_VGA
				printk(S5K4ECGX_MOD_NAME "Preview Resolution is not supported! : %d\n",value);
				return 0;
		}
	}
	return 0;
}

static const struct v4l2_queryctrl s5k4ecgx_controls[] = {
	{
		.id		= V4L2_CID_PRIVATE_GET_MIPI_PHY,
		.type		= V4L2_CTRL_TYPE_CTRL_CLASS,
		.name		= "get mipi phy config"
	},
};
#if 0
static struct s5k4ecgx_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct i2c_client *c, __s32 *value);
	int (*tweak)(struct i2c_client *c, int value);
} s5k4ecgx_controls[] =
{
	#if 0
	{
		.qc = {
			.id = V4L2_CID_BRIGHTNESS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Brightness",
			.minimum = 0,
			.maximum = 255,
			.step = 1,
			.default_value = 0x80,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_brightness,
		.query = s5k4ecgx_q_brightness,
	},
	{
		.qc = {
			.id = V4L2_CID_CONTRAST,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Contrast",
			.minimum = CONTRAST_MINUS_2,
			.maximum = CONTRAST_PLUS_2,
			.step = 1,
			.default_value = CONTRAST_DEFAULT,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_contrast,
		.query = s5k4ecgx_q_contrast,
	},
	{
		.qc = {
			.id = V4L2_CID_SATURATION,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Saturation",
			.minimum = SHARPNESS_MINUS_2,
			.maximum = SHARPNESS_PLUS_2,
			.step = 1,
			.default_value = SHARPNESS_DEFAULT,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_saturation,
		.query = s5k4ecgx_q_saturation,
	},
	{
		.qc = {
			.id = V4L2_CID_HUE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "HUE",
			.minimum = -180,
			.maximum = 180,
			.step = 5,
			.default_value = 0,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_hue,
		.query = s5k4ecgx_q_hue,
	},
	{
		.qc = {
			.id = V4L2_CID_VFLIP,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "Vertical flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = s5k4ecgx_t_vflip,
		.query = s5k4ecgx_q_vflip,
	},
	{
		.qc = {
			.id = V4L2_CID_HFLIP,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "Horizontal mirror",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = s5k4ecgx_t_hflip,
		.query = s5k4ecgx_q_hflip,
	},
	{
		.qc = {
			.id = V4L2_CID_SCENE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Scene",
			.minimum = SCENE_OFF,
			.maximum = SCENE_NIGHT_OFF,
			.step = 1,
			.default_value = SCENE_OFF,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_scene,
		.query = s5k4ecgx_q_scene,
	},
	{
		.qc = {
			.id = V4L2_CID_DO_WHITE_BALANCE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "White balance",
			.minimum = WB_AUTO,
			.maximum = WB_INCANDESCENT,
			.step = 1,
			.default_value = WB_AUTO,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_whitebalance,
		.query = s5k4ecgx_q_whitebalance,
	},
	{
		.qc = {
			.id = V4L2_CID_EFFECT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Effect",
			.minimum = EFFECT_OFF,
			.maximum = EFFECT_SKETCH,
			.step = 1,
			.default_value = EFFECT_OFF,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_effect,
		.query = s5k4ecgx_q_effect,
	},
	{
		.qc = {
			.id = V4L2_CID_ISO,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "ISO",
			.minimum = ISO_AUTO,
			.maximum = ISO_400,
			.step = 1,
			.default_value = ISO_AUTO,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_ISO,
		.query = s5k4ecgx_q_ISO,
	},
	{
		.qc = {
			.id = V4L2_CID_PHOTOMETRY,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Photometry",
			.minimum = METERING_MATRIX,
			.maximum = METERING_CENTER,
			.step = 1,
			.default_value = METERING_MATRIX,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_photometry,
		.query = s5k4ecgx_q_photometry,
	},
	{
		.qc = {
			.id = V4L2_CID_QUALITY,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Quality",
			.minimum = QUALITY_SUPERFINE,
			.maximum = QUALITY_NORMAL,
			.step = 1,
			.default_value = QUALITY_SUPERFINE,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_quality,
		.query = s5k4ecgx_q_quality,
	},
	{
		.qc = {
			.id = V4L2_CID_SHARPNESS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Sharpness",
			.minimum = SHARPNESS_MINUS_2,
			.maximum = SHARPNESS_PLUS_2,
			.step = 1,
			.default_value = SHARPNESS_DEFAULT,
			.flags = V4L2_CTRL_FLAG_SLIDER
		},
		.tweak = s5k4ecgx_t_sharpness,
		.query = s5k4ecgx_q_sharpness,
	},
	#endif
};
#define N_CONTROLS (ARRAY_SIZE(s5k4ecgx_controls))
#endif

static int s5k4ecgx_set_still_status(void)
{
	Cam_Printk(KERN_NOTICE "[DHL]s5k4ecgx_set_still_status.. \n");

	s5k4ecgx_cam_state = S5K4ECGX_STATE_CAPTURE;	

	return 0;
}

static int s5k4ecgx_set_preview_status(void)
{
	Cam_Printk(KERN_NOTICE "[DHL]s5k4ecgx_set_preview_status.. \n");

	s5k4ecgx_cam_state = S5K4ECGX_STATE_PREVIEW;	

	return 0;
}

/* Get chip identification */
static int s5k4ecgx_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *id)
{
	struct s5k4ecgx_info *priv = to_s5k4ecgx(sd);

	id->ident = priv->model;
	id->revision = 0x0;//priv->revision;

	return 0;
}

static int s5k4ecgx_set_bus_param(struct soc_camera_device *icd,
				    unsigned long flags)
{
	return 0;
}

/*++ Flash Mode : dhee79.lee@samsung.com ++*/

static int s5k4ecgx_set_flash_mode(struct i2c_client *client, int mode)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;	
	camera_light_status_type LightStatus1 = CAMERA_SENSOR_LIGHT_STATUS_INVALID;
	
	CAM_DEBUG(" s5k4ecgx_set_flash_mode...\n");

	if (mode) 
	{		/* start AF */
		int need_flash = 0;
		CAM_DEBUG(" Flash Turn ON...\n");
		
		if(sensor->flash_mode == S5K4ECGX_FLASH_ON)
			need_flash = 1;
		else if(sensor->flash_mode == S5K4ECGX_FLASH_AUTO)
		{
			LightStatus1 = s5k4ecgx_check_illuminance_status(client);
			if(LightStatus1 == CAMERA_SENSOR_LIGHT_STATUS_LOW)
				need_flash = 1;
		}
		if(need_flash == 1)
			s5k4ecgx_set_flash(client, MOVIE_FLASH);
	} 
	else 
	{		
		CAM_DEBUG(" Flash Turn OFF...\n");
		s5k4ecgx_set_flash(client, FLASH_OFF);
	}
	return 0;
}

static int s5k4ecgx_get_flash_status(struct i2c_client *client)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int flash_status = 0;
	camera_light_status_type LightStatus2 = CAMERA_SENSOR_LIGHT_STATUS_INVALID;

	CAM_DEBUG(" s5k4ecgx_get_flash_status...\n");

	LightStatus2 = s5k4ecgx_check_illuminance_status(client);

	if(LightStatus2 == CAMERA_SENSOR_LIGHT_STATUS_LOW)
	{ 
		if ((sensor->flash_mode == S5K4ECGX_FLASH_AUTO) || (sensor->flash_mode == S5K4ECGX_FLASH_ON)) 
		{
			flash_status = 1;
		}
	}

	CAM_DEBUG(" %d", flash_status);

	return flash_status;
}

static void s5k4ecgx_set_flash(struct i2c_client *client, int mode)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int i = 0;

	CAM_DEBUG(" %d", mode);
	Cam_Printk(KERN_NOTICE "s5k4ecgx_set_flash : %d...\n", mode);  

	if (mode == MOVIE_FLASH)
	{
		Cam_Printk(" MOVIE FLASH ON");
		gpio_direction_output(camera_flash_en, 1); 
		gpio_direction_output(camera_flash_set, 0);

		gflash_status = TRUE;
	} 
	else if (mode == CAPTURE_FLASH) 
	{
		Cam_Printk(" CAPTURE FLASH ON");
		for(i=0;i<FLASH_PULSE_VALUE;i++)
		{
			gpio_direction_output(camera_flash_en, 1);
			gpio_direction_output(camera_flash_en, 0);
		}
		gpio_direction_output(camera_flash_en, 1);
		gpio_direction_output(camera_flash_set, 0);
		gflash_status = TRUE;
	}
	else if (mode == PRE_CAPTURE_FLASH) 
	{
		Cam_Printk(" Pre-CAPTURE FLASH ON");
		gpio_direction_output(camera_flash_en, 1);
		gpio_direction_output(camera_flash_set, 1);
		gflash_status = TRUE;
	}
	else 
	{
		if( gflash_status == TRUE )
		{
			gflash_status = FALSE;
			CAM_DEBUG(" FLASH OFF");
			gpio_direction_output(camera_flash_en, 0);
			gpio_direction_output(camera_flash_set, 0);
		}
	}

}

static void s5k4ecgx_main_flash_mode(struct i2c_client *client, int mode)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int i = 0;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_main_flash_mode : %d\n", mode);  

	switch(mode)
	{
		case MAIN_FLASH_ON :	
			s5k4ecgx_write_regs(client, reg_s5k4ecgx_Main_Flash_On, ARRAY_SIZE(reg_s5k4ecgx_Main_Flash_On),"reg_s5k4ecgx_Main_Flash_On");			
			Cam_Printk(KERN_NOTICE "MAIN_FLASH_ON \n");
			sensor->main_flash_mode = true;
			break;
			
		case MAIN_FLASH_OFF :
			if(sensor->main_flash_mode == true)
			{		
				s5k4ecgx_write_regs(client, reg_s5k4ecgx_Main_Flash_Off, ARRAY_SIZE(reg_s5k4ecgx_Main_Flash_Off),"reg_s5k4ecgx_Main_Flash_Off");			
				Cam_Printk(KERN_NOTICE "MAIN_FLASH_OFF \n");
				sensor->main_flash_mode = false;
			}
			else
				Cam_Printk(KERN_NOTICE "No need to Main Flash OFF.. \n");
			break;
	}

	sensor->main_flash_mode = mode;
}
/*-- Flash Mode : dhee79.lee@samsung.com --*/

int s5k4ecgx_streamon(struct i2c_client *client)
{

	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	struct v4l2_pix_format* pix = &sensor->pix;
	volatile int checkvalue, timeout = 30;
	camera_light_status_type illuminance=CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
	int err = 0;
	u16 frame_time = 0;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_streamon is called...\n");  

	if(s5k4ecgx_cam_state == S5K4ECGX_STATE_CAPTURE)
	{
		frame_time = s5k4ecgx_get_frame_time(client);
		if(need_flash == TRUE)	
		{
			Cam_Printk(KERN_NOTICE "The Capture Process in Flash On..!! AE & AWB Unlock.. \n");
			s5k4ecgx_AE_lock(client, AE_UNLOCK);
			s5k4ecgx_AWB_lock(client, AWB_UNLOCK);

			Cam_Printk(KERN_NOTICE "Main Flash ON...\n");  
			s5k4ecgx_set_flash(client, CAPTURE_FLASH);
 			s5k4ecgx_main_flash_mode(client, MAIN_FLASH_ON);
			msleep(200);
		}
		else{
			msleep(frame_time);
		}
		
		s5k4ecgx_write_regs(client, regs_s5k4ecgx_Capture_Start, ARRAY_SIZE(regs_s5k4ecgx_Capture_Start),"regs_s5k4ecgx_Capture_Start"); 
 		s5k4ecgx_s_exif_info(client);
 		sensor->state = S5K4ECGX_STATE_CAPTURE;
	
		while((checkvalue != 0x0100) && (timeout-- > 0)) {
			msleep(10);
			s5k4ecgx_mode_switch_check(client, &checkvalue);

			if((checkvalue != 0x0100) && (timeout == 4)) {
				Cam_Printk(KERN_NOTICE "s5k4ecgx polling failed, and reset the capture mode...\n"); 
				//s5k4ecgx_write_regs(client, regs_s5k4ecgx_Capture_Start, ARRAY_SIZE(regs_s5k4ecgx_Capture_Start),"regs_s5k4ecgx_Capture_Start"); 
			}
		}
 	}
 	else
	{
		if(sensor->main_flash_mode == TRUE)
		{
		Cam_Printk(KERN_NOTICE "[DHL]Flash OFF...!!!!! \n");  
 			s5k4ecgx_main_flash_mode(client, MAIN_FLASH_OFF);
		s5k4ecgx_set_flash(client, FLASH_OFF);
			need_flash = FALSE;
		}

		if(sensor->state == S5K4ECGX_STATE_CAPTURE)
		{
			Cam_Printk(KERN_NOTICE "The Capture  is finished..!! AE & AWB Unlock.. \n");
			s5k4ecgx_AE_lock(client, AE_UNLOCK);
			s5k4ecgx_AWB_lock(client, AWB_UNLOCK);
			sensor->focus_type = AUTO_FOCUS_MODE;
		}
		
		err = s5k4ecgx_t_whitebalance(client, sensor->wb);
		if((sensor->scene == SCENE_SUNSET) ||(sensor->scene == SCENE_DAWN) ||(sensor->scene == SCENE_CANDLE))
			err = s5k4ecgx_t_scene(client, sensor->scene); 	

		s5k4ecgx_write_regs(client, regs_s5k4ecgx_Preview_Return, ARRAY_SIZE(regs_s5k4ecgx_Preview_Return),"regs_s5k4ecgx_Preview_Return"); 	
		Cam_Printk(KERN_NOTICE "Return to Preview..\n");
 		Cam_Printk(KERN_NOTICE "[start Preview] !!!\n");
 	}
 	
	return 0;
}


static int s5k4ecgx_streamoff(struct i2c_client *client)
{

	/* What's wrong with this sensor, it has no stream off function, oh!,Vincent.Wan */
	return 0;
}

static int s5k4ecgx_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int retval = 0;
	
	switch (ctrl->id) {
/*		
		case V4L2_CID_GET_AF_STATUS:
		{
			int iAFResult = 0;
			printk( "[DHL]V4L2_CID_GET_AF_STATUS.. \n");
			iAFResult = isx012_get_sensor_af_status();
			ctrl->value = iAFResult;			
			break;
		}
*/		
		case V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL:
		{
			struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
			printk( "[DHL]V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL.. \n");
			ctrl->value = (__s32)sensor->exif_info.exposure_time.denominal;
			break;
		}
		case V4L2_CID_GET_EXIF_ISO_SPEED:
		{
			struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
			printk( "[DHL]V4L2_CID_GET_EXIF_ISO_SPEED.. \n");
			ctrl->value = (__s32)sensor->exif_info.iso_speed_rationg;
			break;
		}
		case V4L2_CID_GET_EXIF_FLASH:
		{
			struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
			printk( "[DHL]V4L2_CID_GET_EXIF_FLASH.. \n");
			ctrl->value = gflash_exif;
			break;
		}
		case V4L2_CID_GET_FLASH_STATUS:
			printk( "[DHL]V4L2_CID_GET_FLASH_STATUS.. \n");
			ctrl->value = s5k4ecgx_get_flash_status(client);
			break;
	default:
		return -EINVAL;
	}

	return retval;
}

static int s5k4ecgx_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int retval = 0;

	Cam_Printk(KERN_NOTICE "ioctl_s_ctrl is called...(%d)\n", ctrl->id);

	switch (ctrl->id) 
	{ 
		case V4L2_CID_ISO:
			retval = s5k4ecgx_t_ISO(client,ctrl->value);
			break;
		case V4L2_CID_BRIGHTNESS:
			retval = s5k4ecgx_t_brightness(client,ctrl->value);
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			retval = s5k4ecgx_t_whitebalance(client,ctrl->value);
			break;
		case V4L2_CID_EFFECT:
			retval = s5k4ecgx_t_effect(client,ctrl->value);
			break;
		case V4L2_CID_CONTRAST:
			retval = s5k4ecgx_t_contrast(client,ctrl->value);
			break;
		case V4L2_CID_SATURATION:
			retval = s5k4ecgx_t_saturation(client,ctrl->value);
			break;
		case V4L2_CID_SHARPNESS:
			retval = s5k4ecgx_t_sharpness(client,ctrl->value);
			break;			
		case V4L2_CID_SCENE:
			retval = s5k4ecgx_t_scene(client,ctrl->value);
			break;
		case V4L2_CID_PHOTOMETRY:
			retval = s5k4ecgx_t_photometry(client,ctrl->value);
			break;
		case V4L2_CID_QUALITY:
			retval = s5k4ecgx_t_quality(client,ctrl->value);
			break;
		case V4L2_CID_FPS:
			retval = s5k4ecgx_t_fps(client,ctrl->value);
			break;
		case V4L2_CID_CAMERA_CHECK_DATALINE:
			retval = s5k4ecgx_t_dtp_on(client);
			break;	
		case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
			retval = s5k4ecgx_t_dtp_stop(client); 
			break;			
		case V4L2_CID_SELECT_MODE:
			retval = s5k4ecgx_set_mode(client,ctrl->value);
			break;
		case V4L2_CID_CAMERA_PREVIEW_SIZE:
			retval = s5k4ecgx_preview_size(client,ctrl->value); 
			break;
		case V4L2_CID_FOCUS_MODE_STEP1:
			retval = s5k4ecgx_set_focus_mode(client,ctrl->value, SET_FOCUS_STEP1);
			break;
		case V4L2_CID_FOCUS_MODE_STEP2:
			retval = s5k4ecgx_set_focus_mode(client,ctrl->value, SET_FOCUS_STEP2);
			break;
		case V4L2_CID_FOCUS_MODE_STEP3:
			retval = s5k4ecgx_set_focus_mode(client,ctrl->value, SET_FOCUS_STEP3);
			break;
		case V4L2_CID_FOCUS_MODE:
			retval = s5k4ecgx_set_focus_mode(client,ctrl->value, SET_FOCUS_ALL);
			break;
		case V4L2_CID_AF:
			retval = s5k4ecgx_set_focus(client,ctrl->value);
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
			retval = s5k4ecgx_set_focus_touch_position(client,ctrl->value);
			break;
		case V4L2_CID_AF_POSITION_STOP:
			retval = s5k4ecgx_set_focus_mode(client,ctrl->value, SET_FOCUS_ALL);
			break;
		case V4L2_CID_AE_LOCK:
			s5k4ecgx_AE_lock(client,ctrl->value);			
			break;
		case V4L2_CID_SET_STILL_STATUS:
			retval = s5k4ecgx_set_still_status();
			break;
		case V4L2_CID_SET_PREVIEW_STATUS:
			retval = s5k4ecgx_set_preview_status();
			break;		
		case V4L2_CID_SET_FLASH_STATUS:
			printk( "[DHL]V4L2_CID_SET_FLASH_STATUS.. \n");			
			s5k4ecgx_set_flash_mode(client, ctrl->value);
			break;
		case V4L2_CID_SET_FLASH_MODE:
			printk( "[DHL]V4L2_CID_SET_FLASH_MODE.. \n");
			sensor->flash_mode = ctrl->value;
			break;
				
		default:
			Cam_Printk(S5K4ECGX_MOD_NAME "[id]Invalid value is ordered!!!\n");
			break;
	}
	return retval;
}
static int set_stream(struct i2c_client *client, int enable)
{
	int ret = 0;
	int st = 0;

	if (enable) {
		ret = s5k4ecgx_streamon(client);
		if (ret < 0)
			goto out;
	} else {
		ret = s5k4ecgx_streamoff(client);
	}
out:
	return ret;
}

static int s5k4ecgx_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	ret = set_stream(client, enable);
	if (ret < 0)
		dev_err(&client->dev, "s5k4ecgx set stream error\n");
	return ret;
}

static int s5k4ecgx_video_probe(struct soc_camera_device *icd,
			      struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k4ecgx_info *priv = to_s5k4ecgx(sd);
	u8 modelhi, modello;
	int ret, i;

	/*
	 * Make sure it's an s5k4ecgx
	 */
	for(i =0;i<3;i++)
	{
		ret = s5k4ecgx_detect(client);
		if (!ret) {
			Cam_Printk(KERN_NOTICE "=========SYS.LSI s5k4ecgx sensor detected==========\n");
			goto out;
		}
		
	}

	priv->model = V4L2_IDENT_S5K4ECGX;
out:
	return ret;
}

static int s5k4ecgx_get_lux(struct i2c_client *client, int* lux)
{
	u16 lux_msb = 0;
	u16 lux_lsb = 0;
	int cur_lux = 0;

	s5k4ecgx_write(client, 0x002C, 0x7000);
	s5k4ecgx_write(client, 0x002E, 0x2C18); //for EVT 1.1
	s5k4ecgx_read(client, 0x0F12, &lux_lsb);
	s5k4ecgx_read(client, 0x0F12, &lux_msb);

	cur_lux = ((lux_msb<<16) |lux_lsb);
	*lux = cur_lux;

	printk("get_lux : %d lux\n", cur_lux);

	return cur_lux; //this value is under 0x0032 in low light condition 
}

camera_ae_stable_type s5k4ecgx_check_ae_status(struct i2c_client *client)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	u32 AE_Stable_Value = 0x0001;
	int err;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_check_ae_status() \r\n");
	camera_ae_stable_type AE_Status = CAMERA_SENSOR_AE_INVALID;


	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);

	err = s5k4ecgx_write(client, 0x002E, 0x2C74); 
	err = s5k4ecgx_read (client, 0x0F12, &AE_Status);		

	if(err < 0){
		printk(KERN_NOTICE "s5k4ecgx_check_ae_status - Failed to read a AE status \r\n");
		return -EIO;
	}

	if( AE_Status == AE_Stable_Value)
	{	
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_ae_status - CAMERA_SENSOR_AE_STABLE \r\n");
		AE_Status = CAMERA_SENSOR_AE_STABLE; 
	}	
	else
	{
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_ae_status - CAMERA_SENSOR_AE_UNSTABLE \r\n");
		AE_Status =  CAMERA_SENSOR_AE_UNSTABLE;
	}
	

	return AE_Status;
}

camera_light_status_type s5k4ecgx_check_illuminance_status(struct i2c_client *client)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	u32 lightStatus = 0;
	u16 lightStatus_low_word = 0;
	u16 lightStatus_high_word= 0;	
	int err;
	u32 luxcheck_low = 0x0032;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status() \r\n");
	camera_light_status_type LightStatus = CAMERA_SENSOR_LIGHT_STATUS_INVALID;


	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);

	err = s5k4ecgx_write(client, 0x002E, 0x2C18); 
	err = s5k4ecgx_read (client, 0x0F12, &lightStatus_low_word);		
	err = s5k4ecgx_write(client, 0x002E, 0x2C1A); 
	err = s5k4ecgx_read (client, 0x0F12, &lightStatus_high_word);	

	lightStatus = lightStatus_low_word  | (lightStatus_high_word << 16);

	Cam_Printk(KERN_NOTICE"s5k4ecgx_check_illuminance_status() : lux_value == 0x%x\n,high == 0x%x\n,low == 0x%x\n", lightStatus,lightStatus_high_word,lightStatus_low_word);

	if(err < 0){
		printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status - Failed to read a lowlight status \r\n");
		return -EIO;
	}

	if( lightStatus < luxcheck_low)
	{	// Lowlight Snapshot
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_LOW \r\n");
		LightStatus = CAMERA_SENSOR_LIGHT_STATUS_LOW; 
	}	
	else
	{	// Normal Snapshot
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_NORMAL \r\n");
		LightStatus =  CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
	}
	

	return LightStatus;
}

int s5k4ecgx_s_exif_info(struct i2c_client *client)
{

	Cam_Printk(KERN_NOTICE "[DHL] EXIF Info.. \r\n");

	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	u32 exposure_time =0;
	u32 exposure_time_lsb = 0;
	u32 exposure_time_msb= 0;	
	u32 iso_gain= 0;
	u32 iso_a_gain= 0;
	u32 iso_d_gain= 0;
	u32 iso_value= 0;
	u32 temp1= 0;
	int err;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_s_exposure_time() \r\n");

	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);
	err = s5k4ecgx_write(client,0x002E, 0x2BC0);
	err = s5k4ecgx_read (client, 0x0F12, &exposure_time_lsb);
	err = s5k4ecgx_write(client,0x002E, 0x2BC2);
	err = s5k4ecgx_read (client, 0x0F12, &exposure_time_msb);

	temp1 = ((exposure_time_msb << 16) | exposure_time_lsb)/400;
	exposure_time = 1000 / temp1;
	Cam_Printk(KERN_NOTICE "[DHL]exposure_time : %d \r\n",exposure_time);
	Cam_Printk(KERN_NOTICE "[DHL]exposure_time : %d \r\n",temp1);

	sensor->exif_info.exposure_time.inumerator=1;
	sensor->exif_info.exposure_time.denominal=exposure_time;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_s_ISO() \r\n");

	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);
	err = s5k4ecgx_write(client,0x002E, 0x2BC4);
	err = s5k4ecgx_read (client, 0x0F12, &iso_a_gain);	
	err = s5k4ecgx_write(client,0x002E, 0x2BC6);
	err = s5k4ecgx_read (client, 0x0F12, &iso_d_gain);	

	iso_gain = ((iso_a_gain * iso_d_gain) / 100)/2;

	Cam_Printk(KERN_NOTICE "iso_gain() %d \n",iso_gain);

	if(iso_gain < 0xD0)  // 208
		iso_value =50;
	else if(iso_gain < 0x1A0)  // 416
		iso_value =100;
	else if(iso_gain < 0x380)  // 896
		iso_value = 200;
	else 
		iso_value = 400;

	sensor->exif_info.iso_speed_rationg=iso_value;

	gflash_exif = gflash_status;

	return 0;
}

static int s5k4ecgx_s_thumbnail_size(struct i2c_client *client, struct v4l2_pix_format *thumbnail)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	struct v4l2_pix_format* pix = &sensor->thumbnail;
	pix->width= thumbnail->width;
	pix->height= thumbnail->height;
	int retval = 0;

	if(( thumbnail->width == 160) && (thumbnail->height == 120))
	s5k4ecgx_write_regs(client, regs_s5k4ecgx_preview_640_480, ARRAY_SIZE(regs_s5k4ecgx_preview_640_480),"regs_s5k4ecgx_preview_640_480");	
	else
	s5k4ecgx_write_regs(client, regs_s5k4ecgx_preview_320_240, ARRAY_SIZE(regs_s5k4ecgx_preview_320_240),"regs_s5k4ecgx_preview_VGA_320_240");	
	
	Cam_Printk(KERN_NOTICE "s5k4ecgx_s_thumbnail_size is called...(Width %d Height %d)\n",pix->width,pix->height);

	return retval;
}


static int s5k4ecgx_AE_AWB_Status(struct i2c_client *client,int *LuxCheck)
{
	u16 AE_Check =0;
	u16 AWB_Check=0;
	u32 lightStatus = CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
	u16 lightStatus_low_word = 0;
	u16 lightStatus_high_word= 0;	
	int err;
	u32 luxcheck_high = 0xFFFE;
	u32 luxcheck_low = 0x0080;	

	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	//illuminance = s5k4ecgx_check_illuminance_status(client);

	Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status() \r\n");
	camera_light_status_type LightStatus = CAMERA_SENSOR_LIGHT_STATUS_INVALID;

	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);

	err = s5k4ecgx_write(client, 0x002E, 0x2A3C); 
	err = s5k4ecgx_read (client, 0x0F12, &lightStatus_low_word);		
	err = s5k4ecgx_write(client, 0x002E, 0x2A3E); 
	err = s5k4ecgx_read (client, 0x0F12, &lightStatus_high_word);	

	lightStatus = lightStatus_low_word  | (lightStatus_high_word << 16);
	
	if ( lightStatus > luxcheck_high)	
	{	// Highlight Snapshot
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_HIGH \r\n");
		LightStatus = CAMERA_SENSOR_LIGHT_STATUS_HIGH;
	}
	else if( lightStatus < luxcheck_low)
	{	// Lowlight Snapshot
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_LOW \r\n");
		LightStatus = CAMERA_SENSOR_LIGHT_STATUS_LOW; 
	}	
	else
	{	// Normal Snapshot
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_NORMAL \r\n");
		LightStatus =  CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
	}
		
	if((LightStatus==CAMERA_SENSOR_LIGHT_STATUS_LOW)&&(sensor->initial == S5K4ECGX_STATE_INITIAL))
	{
		s5k4ecgx_write(client,0xFCFC, 0xD000);
		s5k4ecgx_write(client,0x002C, 0x7000);
		s5k4ecgx_write(client, 0x002E, 0x2A70); 
		s5k4ecgx_read(client, 0x0F12, &AE_Check);		

		s5k4ecgx_write(client,0xFCFC, 0xD000);
		s5k4ecgx_write(client,0x002C, 0x7000);
		s5k4ecgx_write(client, 0x002E, 0x2A74); 
		s5k4ecgx_read(client, 0x0F12, &AWB_Check);	

		if((AE_Check==0x01)&&(AWB_Check==0x01))
		{	
			Cam_Printk(KERN_NOTICE "AE & AWB Ready\n");
		}
		else
		{
				Cam_Printk(KERN_NOTICE "AE & AWB Ready Going On...[Done AE %x AWB %x] \n", AE_Check,AWB_Check);
			msleep(5);
			
	}

	*LuxCheck	= (AE_Check&AWB_Check);
		*LuxCheck = 0x00;
	}
	else{
		*LuxCheck = 0x01;
	}	
	Cam_Printk(KERN_NOTICE "*LuxCheck %d .. \n",*LuxCheck);
	return 0;
}

//#ifdef CONFIG_VIDEO_ADV_DEBUG
static int s5k4ecgx_g_register(struct v4l2_subdev *sd,  struct v4l2_dbg_register * reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int retval = 0;
	
	switch (reg->reg) {
		case V4L2_CID_AF_STATUS:
		{
			struct v4l2_control focusCtrl;
			s5k4ecgx_get_focus_status( client, &focusCtrl, S5K4ECGX_AF_CHECK_STATUS );
			reg->val = (__s64)focusCtrl.value;
			break;
		}				
		case V4L2_CID_AF_2nd_STATUS:
		{
			struct v4l2_control focusCtrl;
			s5k4ecgx_get_focus_status( client, &focusCtrl, S5K4ECGX_AF_CHECK_2nd_STATUS );
			reg->val = (__s64)focusCtrl.value;
			break;
		}
		case V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL:
		{
			struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
			printk( "[DHL]V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL.. \n");
			reg->val = (__s64)sensor->exif_info.exposure_time.denominal;
			break;
		}
		case V4L2_CID_GET_EXIF_ISO_SPEED:
		{
			struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
			printk( "[DHL]V4L2_CID_GET_EXIF_ISO_SPEED.. \n");
			reg->val = (__s64)sensor->exif_info.iso_speed_rationg;
			break;
		}
		case V4L2_CID_GET_EXIF_FLASH:
		{
			struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
			printk( "[DHL]V4L2_CID_GET_EXIF_FLASH.. \n");
			reg->val = gflash_exif;
			break;
		}
		case V4L2_CID_GET_FLASH_STATUS:
			printk( "[DHL]V4L2_CID_GET_FLASH_STATUS.. \n");
			reg->val = s5k4ecgx_get_flash_status(client);
			break;
		case V4L2_CID_ESD_CHECK:
		{
			struct v4l2_control ESDCtrl;
			printk( "[DHL]V4L2_CID_ESD_CHECK.. \n");
			s5k4ecgx_ESD_check(client, &ESDCtrl);
			reg->val = (__s64)ESDCtrl.value;
			break;
		}
	default:
		return -EINVAL;
	}

	return retval;
}

static int s5k4ecgx_s_register(struct v4l2_subdev *sd,  struct v4l2_dbg_register * reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int retval = 0;

	Cam_Printk(KERN_NOTICE "ioctl_s_ctrl is called...(%d)\n", reg->reg);

	switch (reg->reg) 
	{ 
		case V4L2_CID_ISO:
			retval = s5k4ecgx_t_ISO(client,reg->val);
			break;
		case V4L2_CID_BRIGHTNESS:
			retval = s5k4ecgx_t_brightness(client,reg->val);
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			retval = s5k4ecgx_t_whitebalance(client,reg->val);
			break;
		case V4L2_CID_EFFECT:
			retval = s5k4ecgx_t_effect(client,reg->val);
			break;
		case V4L2_CID_CONTRAST:
			retval = s5k4ecgx_t_contrast(client,reg->val);
			break;
		case V4L2_CID_SATURATION:
			retval = s5k4ecgx_t_saturation(client,reg->val);
			break;
		case V4L2_CID_SHARPNESS:
			retval = s5k4ecgx_t_sharpness(client,reg->val);
			break;			
		case V4L2_CID_SCENE:
			retval = s5k4ecgx_t_scene(client,reg->val);
			break;
		case V4L2_CID_PHOTOMETRY:
			retval = s5k4ecgx_t_photometry(client,reg->val);
			break;
		case V4L2_CID_QUALITY:
			retval = s5k4ecgx_t_quality(client,reg->val);
			break;
		case V4L2_CID_FPS:
			retval = s5k4ecgx_t_fps(client,reg->val);
			break;
		case V4L2_CID_CAMERA_CHECK_DATALINE:
			retval = s5k4ecgx_t_dtp_on(client);
			break;	
		case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
			retval = s5k4ecgx_t_dtp_stop(client); 
			break;			
		case V4L2_CID_SELECT_MODE:
			retval = s5k4ecgx_set_mode(client,reg->val);
			break;
		case V4L2_CID_CAMERA_PREVIEW_SIZE:
			retval = s5k4ecgx_preview_size(client,reg->val); 
			break;
		case V4L2_CID_FOCUS_MODE_STEP1:
			retval = s5k4ecgx_set_focus_mode(client,reg->val, SET_FOCUS_STEP1);
			break;
		case V4L2_CID_FOCUS_MODE_STEP2:
			retval = s5k4ecgx_set_focus_mode(client,reg->val, SET_FOCUS_STEP2);
			break;
		case V4L2_CID_FOCUS_MODE_STEP3:
			retval = s5k4ecgx_set_focus_mode(client,reg->val, SET_FOCUS_STEP3);
			break;
		case V4L2_CID_FOCUS_MODE:
			retval = s5k4ecgx_set_focus_mode(client,reg->val, SET_FOCUS_ALL);
			break;
		case V4L2_CID_AF:
			retval = s5k4ecgx_set_focus(client,reg->val);
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
			sensor->focus_type = TOUCH_FOCUS_MODE;
			retval = s5k4ecgx_set_focus_touch_position(client,reg->val);
			break;
		case V4L2_CID_AF_POSITION_STOP:
			retval = s5k4ecgx_set_focus_mode(client,reg->val, SET_FOCUS_ALL);
			break;
		case V4L2_CID_AE_LOCK:
			retval = s5k4ecgx_AE_AWB_lock(client,reg->val);
			break;
		case V4L2_CID_SET_STILL_STATUS:
			retval = s5k4ecgx_set_still_status();
			break;
		case V4L2_CID_SET_PREVIEW_STATUS:
			retval = s5k4ecgx_set_preview_status();
			break;		
		case V4L2_CID_SET_FLASH_STATUS:
			printk( "[DHL]V4L2_CID_SET_FLASH_STATUS.. \n");			
			s5k4ecgx_set_flash_mode(client, reg->val);
			break;
		case V4L2_CID_SET_FLASH_MODE:
			{
			sensor->flash_mode = reg->val;
				if(sensor->scene == SCENE_BACKLIGHT)
				{
					if(sensor->flash_mode == S5K4ECGX_FLASH_OFF )
					{
						printk("The Scene Backlight mode change to Spot setting when the flash off...\n");
						s5k4ecgx_write_regs(client, regs_s5k4ecgx_metering_spot, ARRAY_SIZE(regs_s5k4ecgx_metering_spot),"regs_s5k4ecgx_metering_spot");	
					}
					else
					{
						printk("The Scene Backlight mode change to CenterWeighted setting when the flash off...\n");
						s5k4ecgx_write_regs(client, regs_s5k4ecgx_metering_centerweighted, ARRAY_SIZE(regs_s5k4ecgx_metering_centerweighted),"regs_s5k4ecgx_metering_centerweighted");
					}
				}
			}
			break;
				
		default:
			Cam_Printk(S5K4ECGX_MOD_NAME "[id]Invalid value is ordered!!!\n");
			break;
	}
	return retval;
}
//#endif

static struct i2c_device_id s5k4ecgx_idtable[] = {
	{ "s5k4ecgx", 1 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, s5k4ecgx_idtable);

/**************************************************************************/
/*                 AF Focus Mopde                                         */
/**************************************************************************/

static void s5k4ecgx_AE_lock(struct i2c_client *client,s32 value)
{       
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

    Cam_Printk(KERN_NOTICE "AE set value = %d\n", value);

	switch(value)
	{
		case AE_LOCK :
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_AE_lock, ARRAY_SIZE(regs_s5k4ecgx_AE_lock),"regs_s5k4ecgx_AE_lock");			
			Cam_Printk(KERN_NOTICE "AE_LOCK \n");
			s5k4ecgx_AE_lock_state = true;
			break;
		case AE_UNLOCK :
			if(s5k4ecgx_AE_lock_state == true)
			{
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AE_unlock, ARRAY_SIZE(regs_s5k4ecgx_AE_unlock),"regs_s5k4ecgx_AE_unlock");			
				Cam_Printk(KERN_NOTICE "AE_UNLOCK \n");
				s5k4ecgx_AE_lock_state = false;
			}
			else
				Cam_Printk(KERN_NOTICE "No need to AWB_AE_UNLOCK \n");
			break;
	}

	sensor->ae = value;
	}


static void s5k4ecgx_AWB_lock(struct i2c_client *client,s32 value)
{       
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

   	Cam_Printk(KERN_NOTICE "AWB set value = %d\n", value);

	switch(value)
	{
		case AWB_LOCK :	
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_AWB_lock, ARRAY_SIZE(regs_s5k4ecgx_AWB_lock),"regs_s5k4ecgx_AWB_lock");			
			Cam_Printk(KERN_NOTICE "AWB_LOCK \n");
			s5k4ecgx_AWB_lock_state = true;
			break;
		case AWB_UNLOCK :
			if(s5k4ecgx_AWB_lock_state == true)
			{		
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AWB_unlock, ARRAY_SIZE(regs_s5k4ecgx_AWB_unlock),"regs_s5k4ecgx_AWB_unlock");			
				Cam_Printk(KERN_NOTICE "AWB_UNLOCK \n");
				s5k4ecgx_AWB_lock_state = false;
			}
			else
				Cam_Printk(KERN_NOTICE "No need to AWB UNLOCK \n");
			break;
}

	sensor->awb = value;
}

static int s5k4ecgx_AE_AWB_lock(struct i2c_client *client,s32 value)
{       
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

    Cam_Printk(KERN_NOTICE "AE_AWB set value = %d\n", value);

	switch(value)
	{
		case AWB_AE_LOCK :			
			Cam_Printk(KERN_NOTICE "AWB_AE_LOCK \n");
			break;
		case AWB_AE_UNLOCK :
			if((s5k4ecgx_AE_lock_state == true) || (s5k4ecgx_AWB_lock_state == true))
			{
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AE_unlock, ARRAY_SIZE(regs_s5k4ecgx_AE_unlock),"regs_s5k4ecgx_AE_unlock");			
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AWB_unlock, ARRAY_SIZE(regs_s5k4ecgx_AWB_unlock),"regs_s5k4ecgx_AWB_unlock");			
				Cam_Printk(KERN_NOTICE "AWB_AE_UNLOCK \n");
				s5k4ecgx_AE_lock_state = false;
				s5k4ecgx_AWB_lock_state = false;
			}
			else
				Cam_Printk(KERN_NOTICE "No need to AWB_AE_UNLOCK \n");
			break;

		default:
			Cam_Printk(KERN_NOTICE "[AE]Invalid value is ordered!!! : %d\n",value);
			goto focus_fail;
	}

	return 0;

	focus_fail:
	Cam_Printk(KERN_NOTICE "s5k4ecgx_AE_lock is failed!!!\n");
	return -EINVAL;
}

static int s5k4ecgx_set_focus_mode(struct i2c_client *client, s32 value, int step)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	u16 frame_delay = 0;

	Cam_Printk(KERN_NOTICE "AF set value = %d\n", value);
	
	switch( step )
	{
		case SET_FOCUS_STEP1:
			if( value == S5K4ECGX_AF_SET_NORMAL )
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_1, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_1),"regs_s5k4ecgx_AF_normal_mode_1");			
			else if ( value == S5K4ECGX_AF_SET_MACRO )
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_1, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_1),"regs_s5k4ecgx_AF_macro_mode_1");			
			else
			{
				Cam_Printk(KERN_NOTICE "[AF]Invalid value is ordered!!! : %d\n",value);
				goto focus_fail;
			}
			break;
			
		case SET_FOCUS_STEP2:
			if( value == S5K4ECGX_AF_SET_NORMAL )
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_2, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_2),"regs_s5k4ecgx_AF_normal_mode_2");			
			else if ( value == S5K4ECGX_AF_SET_MACRO )
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_2, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_2),"regs_s5k4ecgx_AF_macro_mode_2");			
			else
			{
				Cam_Printk(KERN_NOTICE "[AF]Invalid value is ordered!!! : %d\n",value);
				goto focus_fail;
			}
			break;
			
		case SET_FOCUS_STEP3:
			if( value == S5K4ECGX_AF_SET_NORMAL )
			{
				if(sensor->scene != SCENE_NIGHT)
					s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_3, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_3),"regs_s5k4ecgx_AF_normal_mode_3");			
				Cam_Printk(KERN_NOTICE "S5K4ECGX_AF_SET_NORMAL \n");
				sensor->focus_mode = value;
			}
			else if ( value == S5K4ECGX_AF_SET_MACRO )
			{
				if(sensor->scene != SCENE_NIGHT)
					s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_3, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_3),"regs_s5k4ecgx_AF_macro_mode_3");			
				Cam_Printk(KERN_NOTICE "S5K4ECGX_AF_SET_MACRO \n");	
				sensor->focus_mode = value;			
			}
			else
			{
				Cam_Printk(KERN_NOTICE "[AF]Invalid value is ordered!!! : %d\n",value);
				goto focus_fail;
			}
			break;
			
		case SET_FOCUS_ALL:
			frame_delay = s5k4ecgx_get_frame_time(client);
			Cam_Printk(KERN_NOTICE "[DHL] frame_delay = %d\n", frame_delay);
			switch(value)
			{
				case S5K4ECGX_AF_SET_NORMAL :
					s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_1, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_1),"regs_s5k4ecgx_AF_normal_mode_1");			
					msleep(frame_delay);	//delay_1_frame;
					s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_2, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_2),"regs_s5k4ecgx_AF_normal_mode_2");			
					msleep(frame_delay);	//delay_1_frame;
					if(sensor->scene != SCENE_NIGHT)
						s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_3, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_3),"regs_s5k4ecgx_AF_normal_mode_3");			
					Cam_Printk(KERN_NOTICE "S5K4ECGX_AF_SET_NORMAL \n");
					break;					
				case S5K4ECGX_AF_SET_MACRO :
					s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_1, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_1),"regs_s5k4ecgx_AF_macro_mode_1");			
					msleep(frame_delay);	//delay_1_frame;
					s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_2, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_2),"regs_s5k4ecgx_AF_macro_mode_2");			
					msleep(frame_delay);	//delay_1_frame;
					if(sensor->scene != SCENE_NIGHT)
						s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_3, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_3),"regs_s5k4ecgx_AF_macro_mode_3");			
					Cam_Printk(KERN_NOTICE "S5K4ECGX_AF_SET_MACRO \n");
					break;		
				default:
					Cam_Printk(KERN_NOTICE "[AF]Invalid value is ordered!!! : %d\n",value);
					goto focus_fail;
			}		
			sensor->focus_mode = value;
			break;
	}
	return 0;

	focus_fail:
	Cam_Printk(KERN_NOTICE "s5k4ecgx_set_focus is failed!!!\n");
	return -EINVAL;
}


static int s5k4ecgx_set_focus(struct i2c_client *client,s32 value)
{       
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	camera_light_status_type illuminance=CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
	camera_ae_stable_type ae_status=CAMERA_SENSOR_AE_STABLE;
	u16 i=0;

	Cam_Printk(KERN_NOTICE "s5k4ecgx_set_focus_status is called...[%d]\n",value);

	if(value == S5K4ECGX_AF_START)
	{

		illuminance = s5k4ecgx_check_illuminance_status(client);

		if (((sensor->flash_mode == S5K4ECGX_FLASH_AUTO) && (illuminance == CAMERA_SENSOR_LIGHT_STATUS_LOW))
			|| (sensor->flash_mode == S5K4ECGX_FLASH_ON)) 
		{				
			Cam_Printk(KERN_NOTICE "Pre-Flash mode is start...\n");  

			need_flash = TRUE;	
			/**++ Pre-Flash Process Start ++**/
			s5k4ecgx_write_regs(client,reg_s5k4ecgx_FAST_AE_ON,ARRAY_SIZE(reg_s5k4ecgx_FAST_AE_ON),"reg_s5k4ecgx_FAST_AE_ON");	
			s5k4ecgx_write_regs(client,reg_s5k4ecgx_Pre_Flash_On,ARRAY_SIZE(reg_s5k4ecgx_Pre_Flash_On),"reg_s5k4ecgx_Pre_Flash_On");
			s5k4ecgx_set_flash(client, PRE_CAPTURE_FLASH);
			/**-- Pre-Flash Process Start --**/

			msleep(400);

			#if 0
			for (i=0 ; i < 10; i++){
			
					if(s5k4ecgx_check_ae_status(client) == 0001)
			{
				         	Cam_Printk(KERN_NOTICE "The AE_status is stable : %d \n", i);  							
                                     	break;
			}
					else
					{
						Cam_Printk(KERN_NOTICE "The AE_status is re-check : 0x%x \n", i);  					
				       	msleep(20);
					}
				}
			#endif

		}	
		
		if(sensor->focus_type == AUTO_FOCUS_MODE)
		{
			Cam_Printk(KERN_NOTICE "FOCUS_MODE_AUTO! AE Lock.. \n");
			s5k4ecgx_AE_lock(client, AE_LOCK);

			if(!need_flash)
			{
			s5k4ecgx_AWB_lock(client, AWB_LOCK);
				Cam_Printk(KERN_NOTICE "AF without Flash..!!AWB Lock.. \n");
		}
		}
		else
		{
			Cam_Printk(KERN_NOTICE "CHECK FOCUS MODE for not AE,AWB Lock = %d\n",sensor->focus_type);		
		}
	}
	
	switch(value) 
	{
		case S5K4ECGX_AF_START :
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_start, ARRAY_SIZE(regs_s5k4ecgx_AF_start),"regs_s5k4ecgx_AF_start");			
			Cam_Printk(KERN_NOTICE "S5K4ECGX_AF_START \n");
			break;
			
		case S5K4ECGX_AF_STOP :
			Cam_Printk(KERN_NOTICE "set_focus :start AF stop.\n");			
			sensor->focus_type = AUTO_FOCUS_MODE;		
			s5k4ecgx_set_focus_mode(client,sensor->focus_mode, SET_FOCUS_ALL);
	
			Cam_Printk(KERN_NOTICE "AF is stopped..!! AE & AWB Unlock.. \n");
			s5k4ecgx_AE_lock(client, AE_UNLOCK);
			s5k4ecgx_AWB_lock(client, AWB_UNLOCK);

			if(need_flash == TRUE)
			{
				Cam_Printk(KERN_NOTICE "AF is failed..!! Pre-Flash Off.. \n");
				s5k4ecgx_write_regs(client,reg_s5k4ecgx_FAST_AE_Off,ARRAY_SIZE(reg_s5k4ecgx_FAST_AE_Off),"reg_s5k4ecgx_FAST_AE_Off");	
				s5k4ecgx_write_regs(client,reg_s5k4ecgx_Pre_Flash_Off,ARRAY_SIZE(reg_s5k4ecgx_Pre_Flash_Off),"reg_s5k4ecgx_Pre_Flash_Off");
				s5k4ecgx_set_flash(client, FLASH_OFF);
				need_flash = FALSE;
		}	
			break;
			
		case S5K4ECGX_AF_STOP_STEP_1 :
			if(sensor->focus_mode == S5K4ECGX_AF_SET_NORMAL){
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_1, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_1),"regs_s5k4ecgx_AF_normal_mode_1");			
			}else if(sensor->focus_mode == S5K4ECGX_AF_SET_MACRO){
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_1, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_1),"regs_s5k4ecgx_AF_macro_mode_1");			
			}
			Cam_Printk(KERN_NOTICE "set_focus : AF stop(1).\n");
			break;
			
		case S5K4ECGX_AF_STOP_STEP_2 :
			if(sensor->focus_mode == S5K4ECGX_AF_SET_NORMAL){
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_2, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_2),"regs_s5k4ecgx_AF_normal_mode_2");			
			}else if(sensor->focus_mode == S5K4ECGX_AF_SET_MACRO){
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_2, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_2),"regs_s5k4ecgx_AF_macro_mode_2");			
			}
			Cam_Printk(KERN_NOTICE "set_focus : AF stop(2).\n");
			break;
			
		case S5K4ECGX_AF_STOP_STEP_3 :
			if(sensor->focus_mode == S5K4ECGX_AF_SET_NORMAL){
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_3, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_3),"regs_s5k4ecgx_AF_normal_mode_3");			
			}else if(sensor->focus_mode == S5K4ECGX_AF_SET_MACRO){
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_3, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_3),"regs_s5k4ecgx_AF_macro_mode_3");			
			}
			Cam_Printk(KERN_NOTICE "set_focus : AF stop(3).\n");
			break;

		default:
			Cam_Printk(KERN_NOTICE "[AF]Invalid value is ordered!!!  : %d\n",value);
			goto focus_status_fail;
	}

	return 0;

	focus_status_fail:
		Cam_Printk(KERN_NOTICE "s5k4ecgx_set_focus_status is failed!!!\n");
		return -EINVAL;
}


static int s5k4ecgx_get_focus_status(struct i2c_client *client, struct v4l2_control *ctrl, s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	u16 status= 0x00;
	camera_light_status_type LightStatus;
	
	switch(value) 
	{
		case S5K4ECGX_AF_CHECK_STATUS :
			Cam_Printk(KERN_NOTICE "s5k4ecgx_get_auto_focus is called...\n"); 
			s5k4ecgx_write(client, 0x002C, 0x7000);
			s5k4ecgx_write(client, 0x002E, 0x2EEE);
			s5k4ecgx_read(client, 0x0F12, (unsigned short*)&status);
			Cam_Printk(KERN_NOTICE "AutoFocus_STATUS : 0x%04X\n", status);
			switch(status & 0xFFFF) 
			{    
				case 1:
					Cam_Printk(KERN_NOTICE "[1st]AF - PROGRESS \n");
					ctrl->value = S5K4ECGX_AF_STATUS_PROGRESS;
					break;
				case 2:
					Cam_Printk(KERN_NOTICE "[1st]AF - SUCCESS \n");
					ctrl->value = S5K4ECGX_AF_STATUS_SUCCESS;
					break;
				default:
					{
					Cam_Printk(KERN_NOTICE "[1st]AF - FAIL\n");
					ctrl->value = S5K4ECGX_AF_STATUS_FAIL;

						Cam_Printk(KERN_NOTICE "[AF-FAIL]Pre-Flash mode is end...\n");  
						
						/**++ Pre-Flash Process End ++**/
						s5k4ecgx_write_regs(client,reg_s5k4ecgx_FAST_AE_Off,ARRAY_SIZE(reg_s5k4ecgx_FAST_AE_Off),"reg_s5k4ecgx_FAST_AE_Off");	
						s5k4ecgx_write_regs(client,reg_s5k4ecgx_Pre_Flash_Off,ARRAY_SIZE(reg_s5k4ecgx_Pre_Flash_Off),"reg_s5k4ecgx_Pre_Flash_Off");
						s5k4ecgx_set_flash(client, FLASH_OFF);
						sensor->focus_type = AUTO_FOCUS_MODE;
						/**-- Pre-Flash Process End --**/
					}
					break;
			}
			break;

		case S5K4ECGX_AF_CHECK_2nd_STATUS :
			s5k4ecgx_write(client, 0x002C, 0x7000);
			s5k4ecgx_write(client, 0x002E, 0x2207);
			s5k4ecgx_read(client, 0x0F12, (unsigned short*)&status);
			Cam_Printk(KERN_NOTICE, "AutoFocus_2nd_STATUS : 0x%04X\n", status);
			switch((status & 0xFF00) >= 0x0100) 
			{
				case 1:
					Cam_Printk(KERN_NOTICE "[2nd]AF - PROGRESS \n");
					ctrl->value = S5K4ECGX_AF_STATUS_PROGRESS;
					break;
				case 0:
					Cam_Printk(KERN_NOTICE "[2nd]AF - SUCCESS \n");
					ctrl->value = S5K4ECGX_AF_STATUS_SUCCESS;
					break;
				default:
					Cam_Printk(KERN_NOTICE "[2nd]AF - PROGRESS \n");
					ctrl->value = S5K4ECGX_AF_STATUS_PROGRESS;
					break;
			}
			break;

		default:
			printk(S5K4ECGX_MOD_NAME"AF CHECK CONTROL NOT MATCHED\n");
			break;
		}
	
		Cam_Printk(KERN_NOTICE, "result = %d (1.progress, 2.success, 3.fail)\n", ctrl->value);

	if((value == S5K4ECGX_AF_CHECK_2nd_STATUS) && (ctrl->value == S5K4ECGX_AF_STATUS_SUCCESS) && (need_flash == TRUE))
		{
			Cam_Printk(KERN_NOTICE "Pre-Flash mode is end...\n");  
			
			/**++ Pre-Flash Process End ++**/
			s5k4ecgx_write_regs(client,reg_s5k4ecgx_FAST_AE_Off,ARRAY_SIZE(reg_s5k4ecgx_FAST_AE_Off),"reg_s5k4ecgx_FAST_AE_Off");	
			s5k4ecgx_write_regs(client,reg_s5k4ecgx_Pre_Flash_Off,ARRAY_SIZE(reg_s5k4ecgx_Pre_Flash_Off),"reg_s5k4ecgx_Pre_Flash_Off");
			s5k4ecgx_set_flash(client, FLASH_OFF);
			sensor->focus_type = AUTO_FOCUS_MODE;
			/**-- Pre-Flash Process End --**/
		}

		return 0;
  
}

/* 640x480 Window Size */
#define INNER_WINDOW_WIDTH              143
#define INNER_WINDOW_HEIGHT             143
#define OUTER_WINDOW_WIDTH              320
#define OUTER_WINDOW_HEIGHT             266

static int s5k4ecgx_set_focus_touch_position(struct i2c_client *client, s32 value)
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

	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	Cam_Printk(KERN_NOTICE "value : %d\n", value);

	/* get x,y touch position */
	touch_x = (u16)sensor->position.x;
	touch_y = (u16)sensor->position.y;

	/* get preview width,
	 * height */
	width = s5k4ecgx_preview_sizes[sensor->preview_size].width;
	height = s5k4ecgx_preview_sizes[sensor->preview_size].height;

	//touch_x = width - touch_x;
	//touch_y = height - touch_y;

	tagCamReg32_t S5K4ECGX_TOUCH_AF[] =
	{
		{0xFCFC, 0xD000},
		{0x0028, 0x7000},
		{0x002A, 0x0294},       //AF window setting
		{0x0F12, 0x0100},       //REG_TC_AF_FstWinStartX 
		{0x0F12, 0x00E3},       //REG_TC_AF_FstWinStartY
		{0x002A, 0x029C},       //AF window setting
		{0x0F12, 0x01C6},       //REG_TC_AF_ScndWinStartX
		{0x0F12, 0x0166},       //REG_TC_AF_ScndWinStartY
		{0x002A, 0x02A4},       //AF window setting
		{0x0F12, 0x0001},       //REG_TC_AF_WinSizesUpdated
	};
	err = s5k4ecgx_write(client, 0xFCFC, 0xD000);
	err = s5k4ecgx_write(client, 0x002C, 0x7000);
	err = s5k4ecgx_write(client, 0x002E, 0x0298);
	err = s5k4ecgx_read(client, 0x0F12, (u16 *)&read_value);
	Cam_Printk(KERN_NOTICE "outter_width : %x(%d)\n", read_value, read_value);
	outter_window_width = (u32)(read_value * width / 1024);
	read_value = 0;

	err = s5k4ecgx_write(client, 0x002E, 0x029A);
	err = s5k4ecgx_read(client, 0x0F12, &read_value);
	Cam_Printk(KERN_NOTICE "outter_height : %x(%d)\n", read_value, read_value);
	outter_window_height = (u32)(read_value * height / 1024);
	read_value = 0;

	err = s5k4ecgx_write(client, 0x002E, 0x02A0);
	err = s5k4ecgx_read(client, 0x0F12, &read_value);
	Cam_Printk(KERN_NOTICE "inner_width : %x(%d)\n", read_value, read_value);
	inner_window_width = (u32)(read_value * width / 1024);
	read_value = 0;

	err = s5k4ecgx_write(client, 0x002E, 0x02A2);
	err = s5k4ecgx_read(client, 0x0F12, &read_value);
	Cam_Printk(KERN_NOTICE "inner_height : %x(%d)\n", read_value, read_value);
	inner_window_height = (u32)(read_value * height / 1024);
	read_value = 0;

	if (touch_x <= inner_window_width/2) {
		// inner window, outter window should be positive.
		outter_x = 0;
		inner_x = 0;
	} else if (touch_x <= outter_window_width/2) {
		// outter window should be positive.
		inner_x = touch_x - inner_window_width/2;
		outter_x = 0;
	} else if (touch_x >= ((width - 1) - inner_window_width/2)) {
		// inner window, outter window should be less than LCD Display Size
		inner_x = (width - 1) - inner_window_width;
		outter_x = (width - 1) - outter_window_width;
	} else if (touch_x >= ((width -1) - outter_window_width/2)) {
		// outter window should be less than LCD Display Size
		inner_x = touch_x - inner_window_width/2;
		outter_x = (width -1) - outter_window_width;
	} else {
		// touch_x is not corner, so set using touch point.
		inner_x = touch_x - inner_window_width/2;
		outter_x = touch_x - outter_window_width/2;
	}

	if (touch_y <= inner_window_height/2) {	
		// inner window, outter window should be positive.
		outter_y = 0;
		inner_y = 0;
	} else if (touch_y <= outter_window_height/2) {
		// outter window should be positive.
		inner_y = touch_y - inner_window_height/2;
		outter_y = 0;
	} else if (touch_y >= ((height - 1) - inner_window_height/2)) {
		// inner window, outter window should be less than LCD Display Size
		inner_y = (height - 1) - inner_window_height;
		outter_y = (height - 1) - outter_window_height;
	} else if (touch_y >= ((height - 1) - outter_window_height/2)) {
		// outter window should be less than LCD Display Size
		inner_y = touch_y - inner_window_height/2;
		outter_y = (height - 1) - outter_window_height;
	} else {
		// touch_x is not corner, so set using touch point.
		inner_y = touch_y - inner_window_height/2;
		outter_y = touch_y - outter_window_height/2;
	}

	if (!outter_x) outter_x = 1;
	if (!outter_y) outter_y = 1;
	if (!inner_x) inner_x= 1;
	if (!inner_y) inner_y= 1;


	Cam_Printk(KERN_NOTICE "touch position(%d, %d), preview size(%d, %d)\n",
			touch_x, touch_y, width, height);
	Cam_Printk(KERN_NOTICE "point first(%d, %d), second(%d, %d)\n",
			outter_x, outter_y, inner_x, inner_y);

	{
		S5K4ECGX_TOUCH_AF[3].value = outter_x * 1024 / width;
		S5K4ECGX_TOUCH_AF[4].value = outter_y * 1024 / height;

		S5K4ECGX_TOUCH_AF[6].value = inner_x * 1024 / width;
		S5K4ECGX_TOUCH_AF[7].value = inner_y * 1024 / height;
	}

	Cam_Printk(KERN_NOTICE "fisrt reg(0x%x(%d), 0x%x(%d)) second reg(0x%x(%d), 0x%x(%d)\n",
			S5K4ECGX_TOUCH_AF[3].value, S5K4ECGX_TOUCH_AF[3].value,
			S5K4ECGX_TOUCH_AF[4].value, S5K4ECGX_TOUCH_AF[4].value,
			S5K4ECGX_TOUCH_AF[6].value, S5K4ECGX_TOUCH_AF[6].value,
			S5K4ECGX_TOUCH_AF[7].value, S5K4ECGX_TOUCH_AF[7].value);

	for (i=0 ; i <ARRAY_SIZE(S5K4ECGX_TOUCH_AF); i++) {
		err = s5k4ecgx_write(client, S5K4ECGX_TOUCH_AF[i].addr, S5K4ECGX_TOUCH_AF[i].value);
	}
//	s5k4ecgx_wait_1_frame();

	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for touch_auto_focus\n", __func__);
		return -EIO;
	}

	return 0;
}

void s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(struct i2c_client *client,int bit, int set) 
{
	//struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int REG_TC_DBG_AutoAlgEnBits = 0; 

	/* Read 04E6 */
	s5k4ecgx_write(client, 0x002C, 0x7000);
	s5k4ecgx_write(client, 0x002E, 0x04E6);
	s5k4ecgx_read(client, 0x0F12, (unsigned short*)&REG_TC_DBG_AutoAlgEnBits);

	if(bit == 3 && set == true) {
		if(REG_TC_DBG_AutoAlgEnBits & 0x8 == 1) return;
		msleep(100);
		REG_TC_DBG_AutoAlgEnBits = REG_TC_DBG_AutoAlgEnBits | 0x8; 
		s5k4ecgx_write(client, 0x0028, 0x7000);
		s5k4ecgx_write(client, 0x002A, 0x04E6);
		s5k4ecgx_write(client, 0x0F12, REG_TC_DBG_AutoAlgEnBits);
	} else if(bit == 3 && set == false) {
		if(REG_TC_DBG_AutoAlgEnBits & 0x8 == 0)return;
		msleep(100);
		REG_TC_DBG_AutoAlgEnBits = REG_TC_DBG_AutoAlgEnBits & 0xFFF7;
		s5k4ecgx_write(client, 0x0028, 0x7000);
		s5k4ecgx_write(client, 0x002A, 0x04E6);
		s5k4ecgx_write(client, 0x0F12, REG_TC_DBG_AutoAlgEnBits);
	} else if(bit == 5 && set == true) {
		if(REG_TC_DBG_AutoAlgEnBits & 0x20 == 1)return;
		msleep(100);
		REG_TC_DBG_AutoAlgEnBits = REG_TC_DBG_AutoAlgEnBits | 0x20;
		s5k4ecgx_write(client, 0x0028, 0x7000);
		s5k4ecgx_write(client, 0x002A, 0x04E6);
		s5k4ecgx_write(client, 0x0F12, REG_TC_DBG_AutoAlgEnBits);
	} else if(bit == 5 && set == false) {
		if(REG_TC_DBG_AutoAlgEnBits & 0x20 == 0)return;
		msleep(100);
		REG_TC_DBG_AutoAlgEnBits = REG_TC_DBG_AutoAlgEnBits & 0xFFDF;
		s5k4ecgx_write(client, 0x0028, 0x7000);
		s5k4ecgx_write(client, 0x002A, 0x04E6);
		s5k4ecgx_write(client, 0x0F12, REG_TC_DBG_AutoAlgEnBits);
	}

	return;
}

static int s5k4ecgx_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_2_LANE;

	return 0;
}

#if 0
static struct soc_camera_ops s5k4ecgx_ops = {
	.set_bus_param		= s5k4ecgx_set_bus_param,
	//.query_bus_param	= s5k4ecgx_query_bus_param,
	.controls		= s5k4ecgx_controls,
	.num_controls		= ARRAY_SIZE(s5k4ecgx_controls),
};
#endif
static struct v4l2_subdev_core_ops s5k4ecgx_core_ops = {
	//.g_ctrl			= s5k4ecgx_g_ctrl,
	.s_ctrl			= s5k4ecgx_s_ctrl,
	.init				= s5k4ecgx_init,
	.g_chip_ident		= s5k4ecgx_g_chip_ident,
//#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= s5k4ecgx_g_register,
	.s_register		= s5k4ecgx_s_register,
//#endif

};

static struct v4l2_subdev_video_ops s5k4ecgx_video_ops = {
	.s_stream		= s5k4ecgx_s_stream,
	.s_mbus_fmt		= s5k4ecgx_s_fmt,
	.try_mbus_fmt		= s5k4ecgx_try_fmt,
	.enum_mbus_fmt		= s5k4ecgx_enum_fmt,
	.enum_mbus_fsizes	= s5k4ecgx_enum_fsizes,
	.g_parm				= s5k4ecgx_g_parm,
	.s_parm				= s5k4ecgx_s_parm,
	.g_mbus_config	= s5k4ecgx_g_mbus_config,
	//.cropcap		= s5k4ecgx_cropcap,
	//.g_crop			= s5k4ecgx_g_crop,
};

static struct v4l2_subdev_ops s5k4ecgx_subdev_ops = {
	.core			= &s5k4ecgx_core_ops,
	.video			= &s5k4ecgx_video_ops,
};

/*
 * i2c_driver function
 */


static int s5k4ecgx_command(struct i2c_client *client, unsigned int cmd, void *arg)
{


	switch (cmd) { 
		case VIDIOC_DBG_G_CHIP_IDENT:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_DBG_G_CHIP_IDENT\n");
			return v4l2_chip_ident_i2c_client(client, arg, V4L2_IDENT_S5K4ECGX, 0);		
		case VIDIOC_INT_RESET:
			s5k4ecgx_reset(client);
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_INT_RESET\n");
			return 0;
		case VIDIOC_QUERYCAP:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_QUERYCAP\n");
			return s5k4ecgx_querycap(client, (struct v4l2_capability *) arg);
#if 0			
		case VIDIOC_ENUM_FMT:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_ENUM_FMT\n");
			return s5k4ecgx_enum_fmt(client, index, (struct v4l2_fmtdesc *) arg);
#endif
		case VIDIOC_ENUM_FRAMESIZES:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_ENUM_FRAMESIZES\n");
			return s5k4ecgx_enum_fsizes(client, (struct v4l2_frmsizeenum *) arg);
		case VIDIOC_TRY_FMT:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_TRY_FMT\n");
			return s5k4ecgx_try_fmt(client, (struct v4l2_format *) arg);
		case VIDIOC_S_FMT:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_S_FMT\n");
			return s5k4ecgx_s_fmt(client, (struct v4l2_format *) arg);
#if 0			
		case VIDIOC_QUERYCTRL:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_QUERYCTRL\n");
			return s5k4ecgx_queryctrl(client, (struct v4l2_queryctrl *) arg);
#endif
		case VIDIOC_S_CTRL:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_S_CTRL\n");
			printk(" s5k4ecgx_command : VIDIOC_S_CTRL\n");
			return s5k4ecgx_s_register(client, (struct v4l2_control *) arg);
		case VIDIOC_G_CTRL:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_G_CTRL\n");
			return s5k4ecgx_g_register(client, (struct v4l2_control *) arg);
		case VIDIOC_S_PARM:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_S_PARM\n");
			return s5k4ecgx_s_parm(client, (struct v4l2_streamparm *) arg);
		case VIDIOC_G_PARM:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_G_PARM\n");
			return s5k4ecgx_g_parm(client, (struct v4l2_streamparm *) arg);
#if 0		
		case VIDIOC_S_INPUT:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_S_INPUT\n");
			return s5k4ecgx_s_input(client, (int *) arg);
#endif
		case VIDIOC_STREAMON:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_STREAMON\n");
			return s5k4ecgx_streamon(client);
		case VIDIOC_STREAMOFF:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_STREAMOFF\n");
			return s5k4ecgx_streamoff(client);
#ifdef CONFIG_VIDEO_ADV_DEBUG
		case VIDIOC_DBG_G_REGISTER:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_DBG_G_REGISTER\n");
			return s5k4ecgx_g_register(client, (struct v4l2_dbg_register *) arg);
		case VIDIOC_DBG_S_REGISTER:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_DBG_S_REGISTER\n");
			return s5k4ecgx_s_register(client, (struct v4l2_dbg_register *) arg);
#endif
		case VIDIOC_G_EXIF:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_G_EXIF\n");
			return s5k4ecgx_g_exif_info(client, (struct v4l2_exif_info *) arg);
		case VIDIOC_S_THUMBNAIL:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_S_THUMBNAIL\n");
			return s5k4ecgx_s_thumbnail_size(client, (struct v4l2_pix_format *) arg);
		case VIDIOC_AE_AWB_STATUS:
			Cam_Printk(KERN_NOTICE " s5k4ecgx_command : VIDIOC_AE_AWB_STATUS\n");
			return s5k4ecgx_AE_AWB_Status(client, (int *)arg);
	}
	return -EINVAL;
}

static int s5k4ecgx_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct s5k4ecgx_info *priv;
	struct soc_camera_device *icd	= client->dev.platform_data;
	struct soc_camera_link *icl;
	int ret;

	printk("------------s5k4ecgx_probe--------------\n");

	if (!icd) {
		dev_err(&client->dev, "Missing soc-camera data!\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(struct s5k4ecgx_info), GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "Failed to allocate private data!\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &s5k4ecgx_subdev_ops);

	//icd->ops = &s5k4ecgx_ops;

	ret = s5k4ecgx_video_probe(icd, client);
	if (ret < 0) {
		//icd->ops = NULL;
		kfree(priv);
	}
	
	printk("------------s5k4ecgx_probe---return --ret = %d---------\n", ret);
	return ret;
}

static int s5k4ecgx_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_driver s5k4ecgx_driver = {
	.driver = {
		.name	= "s5k4ecgx",
	},
	.id_table       = s5k4ecgx_idtable,	
	.command	= s5k4ecgx_command,
	.probe		= s5k4ecgx_probe,
	.remove		= s5k4ecgx_remove,
};

/*
 * Module initialization
 */
static int __init s5k4ecgx_mod_init(void)
{
	int ret =0;
	Cam_Printk(KERN_NOTICE "SYS.LSI s5k4ecgx sensor driver, at your service\n");
	ret = i2c_add_driver(&s5k4ecgx_driver);
	Cam_Printk(KERN_NOTICE "SYS.LSI s5k4ecgx :%d \n ",ret);
	return ret;
	//return i2c_add_driver(&s5k4ecgx_driver);
}

static void __exit s5k4ecgx_mod_exit(void)
{
	i2c_del_driver(&s5k4ecgx_driver);
}

module_init(s5k4ecgx_mod_init);
//module_init(s5k4ecgx_mod_init);
module_exit(s5k4ecgx_mod_exit);

