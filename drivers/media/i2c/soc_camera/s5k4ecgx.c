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
#include <media/mrvl-camera.h>
#include <media/soc_camera.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#if defined(CONFIG_MACH_YANGZILTE) || defined(CONFIG_MACH_MOUTAILTE_CHN)
#include "s5k4ecgx_regs_sglte.h"
#else
#include "s5k4ecgx_regs.h"
#endif
#include "s5k4ecgx.h"

MODULE_AUTHOR("Jonathan Corbet <corbet@lwn.net>");
MODULE_DESCRIPTION("A low-level driver for SYS.LSI S5K4ECGX sensors");
MODULE_LICENSE("GPL");
#define to_s5k4ecgx(sd)		container_of(sd, struct s5k4ecgx_info, subdev)
//#define ENABLE_WQ_I2C_S5K4EC (1)

#ifdef ENABLE_WQ_I2C_S5K4EC
static DEFINE_MUTEX(i2c_lock_s5k4ec);
#endif

extern unsigned int system_rev;
extern void soc_camera_set_ddr_qos(s32 value);
extern void soc_camera_set_gc2d_qos(s32 value);
struct class *camera_class = NULL;
struct device *cam_dev_rear;

static const struct s5k4ecgx_datafmt s5k4ecgx_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_VYUY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YVYU8_2X8, V4L2_COLORSPACE_JPEG},
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
static int auto_flash_lock = FALSE;
static int gflash_exif = FALSE;
static int gFPS_flag = FALSE;
static int gFPS_value = FALSE;
static int gTouch_shutter_value = FALSE;
static int Recording_Snapshot_State = FALSE;	/* Flag for recording snapshot state */
static int s5k4ecgx_Movie_flash_state = FALSE; 	/* Flag for flash state in camcorder mode */
/*
 * Basic window sizes.  These probably belong somewhere more globally
 * useful.
 */
#define WIDTH_2576		2576
#define HEIGHT_1932  	1932
#define WQXGA_WIDTH		2560
#define WQXGA_HEIGHT	1920
#define Wide4M_WIDTH	2560
#define Wide4M_HEIGHT	1536
#define HEIGHT_1440		1440
#define QXGA_WIDTH		2048
#define QXGA_HEIGHT	1536
#define Wide2_4M_WIDTH	2048
#define Wide2_4M_HEIGHT	1232
#define HEIGHT_1152		1152
#define UXGA_WIDTH		1600
#define UXGA_HEIGHT     	1200
#define Wide1_5M_WIDTH	1600
#define Wide1_5M_HEIGHT	 960
#define WIDTH_960		960
#define HEIGHT_720		720
#define WIDTH_720		720
#define HEIGHT_540		540
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
static int s5k4ecgx_get_start_af_delay(struct i2c_client *client, struct v4l2_control *ctrl);
static int s5k4ecgx_get_AE_status(struct i2c_client *client, struct v4l2_control *ctrl);
void s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(struct i2c_client *client,int bit, int set); 
camera_light_status_type s5k4ecgx_check_illuminance_status(struct i2c_client *client);
camera_ae_stable_type s5k4ecgx_check_ae_status(struct i2c_client *client);
int s5k4ecgx_s_exif_info(struct i2c_client *client);
static void s5k4ecgx_set_flash(struct i2c_client *client, int mode);
static int s5k4ecgx_get_flash_status(struct i2c_client *client);
static int s5k4ecgx_set_flash_mode(struct i2c_client *client, int mode);
static int s5k4ecgx_t_fps(struct i2c_client *client, int value);
static int s5k4ecgx_t_brightness(struct i2c_client *client, int value);
static int s5k4ecgx_get_SensorVendorID(struct i2c_client *client);

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
			},
	.cam_mode           =  CAMERA_MODE,
};
#if 0
extern struct s5k4ecgx_platform_data s5k4ecgx_platform_data0;
#endif
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

#ifdef ENABLE_WQ_I2C_S5K4EC
	printk("s5k4ecgx_read s \n");
	mutex_lock(&i2c_lock_s5k4ec);
#endif

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
	*value = data[0]<<8 | data[1] ;
out:

#ifdef ENABLE_WQ_I2C_S5K4EC
	mutex_unlock(&i2c_lock_s5k4ec);
	printk("s5k4ecgx_read e \n");
#endif

	return (ret < 0) ? ret: 0;
}
static int s5k4ecgx_write(struct i2c_client *c, u16 reg,  u16 val)
{
#ifdef ENABLE_WQ_I2C_S5K4EC
	printk("s5k4ecgx_write s %x, %x\n", reg, val);
	mutex_lock(&i2c_lock_s5k4ec);
#endif

	u8 data[4];
	int ret = 0;
	data[0] = reg>>8;
	data[1] = reg;
	data[2]=  val>>8;
	data[3] = val;
	
	
	if (reg == 0xffff) {
		msleep(val);  /* Wait for reset to run */
#ifdef ENABLE_WQ_I2C_S5K4EC
		mutex_unlock(&i2c_lock_s5k4ec);
#endif
		return 0;
	}
	
	ret = i2c_master_send(c, data, 4);

#ifdef ENABLE_WQ_I2C_S5K4EC
	mutex_unlock(&i2c_lock_s5k4ec);
	printk("s5k4ecgx_write e \n");
#endif

	return (ret < 0) ? ret: 0;
}
static u16 VENDOR_ID = NULL;
static int s5k4ecgx_get_SensorVendorID(struct i2c_client *client)
{
	u16 SensorID = 0xFFFF;

	printk("[DHL] s5k4ecgx_get_SensorVendorID..!!\n");
	
	s5k4ecgx_write(client,0xFCFC, 0xD000);
	s5k4ecgx_write(client,0x0028, 0xD000);
	s5k4ecgx_write(client,0x002A, 0x0012);
	s5k4ecgx_write(client,0x0F12, 0x0001);
	s5k4ecgx_write(client,0x002A, 0x007A);
	s5k4ecgx_write(client,0x0F12, 0x0000);
	s5k4ecgx_write(client,0x002A, 0xA000);	
	s5k4ecgx_write(client,0x0F12, 0x0004);
	s5k4ecgx_write(client,0x002A, 0xA002);
	s5k4ecgx_write(client,0x0F12, 0x0006); // 6page_select
	s5k4ecgx_write(client,0x002A, 0xA000);
	s5k4ecgx_write(client,0x0F12, 0x0001); // set read mode

	msleep(100);

	// 0xA006, 0xA007 read
	s5k4ecgx_write(client,0x002C, 0xD000);
	s5k4ecgx_write(client,0x002E, 0xA006);
	s5k4ecgx_read(client,0x0F12, &SensorID);

	if(SensorID ==  0xFFFF){
		printk(S5K4ECGX_MOD_NAME"##########################################\n");
		printk(S5K4ECGX_MOD_NAME"   [DHL] There is no sensor..!!\n");
		printk(S5K4ECGX_MOD_NAME"##########################################\n");
	}else{
		VENDOR_ID = SensorID;
		printk(S5K4ECGX_MOD_NAME"##########################################\n");
		printk(S5K4ECGX_MOD_NAME"   [DHL] 5M Sensor Vendor ID : 0x%04X\n", SensorID);
		printk(S5K4ECGX_MOD_NAME"##########################################\n");

		}

	return 0;
	
}

static int s5k4ecgx_detect(struct i2c_client *client)
{
	u16 ID = 0xFFFF;
	s5k4ecgx_write(client,0xFCFC, 0xD000);
	s5k4ecgx_write(client,0x002C, 0x7000);
	s5k4ecgx_write(client,0x002E, 0x01A6);
	s5k4ecgx_read(client, 0x0F12, &ID);	
	
	if(ID == 0x0011){
		printk(S5K4ECGX_MOD_NAME"========================================\n");
		printk(S5K4ECGX_MOD_NAME"   [5MEGA CAM] vendor_id ID : 0x%04X\n", ID);
		printk(S5K4ECGX_MOD_NAME"========================================\n");
	}else {
		printk(S5K4ECGX_MOD_NAME"-------------------------------------------------\n");
		printk(S5K4ECGX_MOD_NAME"   [5MEGA CAM] sensor detect failure !!\n");
		printk(S5K4ECGX_MOD_NAME"   ID : 0x%04X[ID should be 0x0011]\n", ID);
		printk(S5K4ECGX_MOD_NAME"-------------------------------------------------\n");
		return -EINVAL;
	}
	return 0;
}

#ifdef ENABLE_WQ_I2C_S5K4EC
static struct workqueue_struct *s5k4ec_wq;

typedef struct {  
	struct work_struct s5k4ec_work;

	struct i2c_client *c; 
	tagCamReg32_t *vals; 
	u32 reg_length; 
	char *name;

}s5k4ec_work_t;

s5k4ec_work_t *s5k4ec_work;

static void s5k4ec_wq_function( struct work_struct *work)
{ 
  	s5k4ec_work_t *s5k4ec_work = (s5k4ec_work_t *)work;  

	int i = 0, ret=0;

	u8 iic_data[3000] = {0};
	u16 iic_length = 2;

	struct i2c_client *c; 
	tagCamReg32_t *vals; 
	u32 reg_length; 
	char *name;

	
	c = s5k4ec_work->c;
	vals = s5k4ec_work->vals;
	reg_length = s5k4ec_work->reg_length;
	name = s5k4ec_work->name;

	printk("s5k4ec_wq_function called %s \n",name);
	
	for(i=0;i<reg_length;i++){
		if( vals[i].addr == 0x0F12 ){	
			iic_data[iic_length] = vals[i].value >>8;
			iic_data[iic_length+1] = vals[i].value & 0x00FF;
				
			iic_length = iic_length+2;
			
			if(i==(reg_length-1)){
				iic_data[0]=0x0F;
				iic_data[1]=0x12;
				i2c_master_send(c, iic_data, iic_length);
				iic_length =2;
			}
		}else{
			if(iic_length !=2){
				iic_data[0]=0x0F;
				iic_data[1]=0x12;
			i2c_master_send(c, iic_data, iic_length);
			iic_length =2;
			}
			
			{
			u8 data[4];
			data[0] = vals[i].addr>>8;
			data[1] = vals[i].addr;
			data[2]=  vals[i].value>>8;
			data[3] = vals[i].value;
			
			
			if (vals[i].addr == 0xffff) {
				msleep(vals[i].value);  /* Wait for reset to run */
				ret = 0;
			}else{
				ret = i2c_master_send(c, data, 4);
			}
			}
			if (ret < 0)	{
				printk(KERN_NOTICE "======[s5k4ec_wq_function error %d]====== \n", ret );	
				mutex_unlock(&i2c_lock_s5k4ec);
				return ;
			}
		}
	}

	printk("s5k4ec_wq_function called %s end\n",name);

	mutex_unlock(&i2c_lock_s5k4ec);
  	return;
}

#endif




static int s5k4ecgx_write_regs(struct i2c_client *c, tagCamReg32_t *vals, u32 reg_length, char *name)
{
#ifdef ENABLE_WQ_I2C_S5K4EC
	printk("=========%s : %s wq call ============ \n ",__func__,name);

	mutex_lock(&i2c_lock_s5k4ec);

	s5k4ec_work->c = c;
	s5k4ec_work->vals = vals;
	s5k4ec_work->reg_length = reg_length;
	s5k4ec_work->name = name;
	queue_work( s5k4ec_wq, (struct work_struct *)s5k4ec_work );
	return 0;

#else
	int i = 0, ret=0;
	u8 iic_data[3000] = {0};
	u16 iic_length = 2;
	for(i=0;i<reg_length;i++){
		if( vals[i].addr == 0x0F12 ){	
			iic_data[iic_length] = vals[i].value >>8;
			iic_data[iic_length+1] = vals[i].value & 0x00FF;
				
			iic_length = iic_length+2;
			
			if(i==(reg_length-1)){
				iic_data[0]=0x0F;
				iic_data[1]=0x12;
				i2c_master_send(c, iic_data, iic_length);
				iic_length =2;
			}
		}else{
			if(iic_length !=2){
				iic_data[0]=0x0F;
				iic_data[1]=0x12;
			i2c_master_send(c, iic_data, iic_length);
			iic_length =2;
			}
			ret = s5k4ecgx_write(c, vals[i].addr, vals[i].value);
			if (ret < 0)	{
				printk(KERN_NOTICE "======[s5k4ecgx_write_array %d]====== \n", ret );	
				return ret;
			}
		}
	}
	return 0;
#endif		
}
static int s5k4ecgx_set_regs(struct i2c_client *c, u32 id, 
					struct param_reg_tables *param_regs_table, u32 regs_table_num)
{
	u32 i =0, ret = 0;
	for( i = 0; i < regs_table_num; i++) {
		if(param_regs_table[i].param_id == id) {
			break;
		}
	}
	if(i < regs_table_num) {
		ret = s5k4ecgx_write_regs(c, param_regs_table[i].reg_table, param_regs_table[i].reg_num, param_regs_table[i].reg_table_name);
	} else {
		ret = -1;
		printk(KERN_NOTICE "Could not found regs: %d and %s\n", id, param_regs_table[0].reg_table_name);
	}
	return ret;
}
static void s5k4ecgx_reset(struct i2c_client *client)
{
	msleep(1);
}
static int s5k4ecgx_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int result =0;
	
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
	/* QCIF */
	{
		.width		= QCIF_WIDTH,
		.height		= QCIF_HEIGHT,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
	},
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
	},
	/* 800*480 */
	{
		.width		= Wide4K_WIDTH,
		.height		= Wide4K_HEIGHT,
	},
	/* HD */
	{
		.width    = HD_WIDTH,
		.height   = HD_HEIGHT,
	},
	/* UXGA */
	{
		.width          = UXGA_WIDTH,
		.height         = UXGA_HEIGHT,
	},
	/* QXGA */
	{
		.width          = QXGA_WIDTH,
		.height         = QXGA_HEIGHT,
	},
	/* 2576*1932 */
	{
		.width		= WIDTH_2576,
		.height		= HEIGHT_1932,
	},
	/* 4M Wide */
	{
		.width		= Wide4M_WIDTH,
		.height		= Wide4M_HEIGHT,
	},
	/* CIF */
	{
		.width		= CIF_WIDTH,
		.height		= CIF_HEIGHT,
	},
	/* 720*540 */
	{
		.width		= WIDTH_720,
		.height		= HEIGHT_540,
	},
	/* 704*576 */
	{
		.width		= WIDTH_704,
		.height		= HEIGHT_576,
	},
	/* 960*540 */
	{
		.width		= WIDTH_960,
		.height		= HEIGHT_540,
	},
	/* 960*720 */
	{
		.width		= WIDTH_960,
		.height		= HEIGHT_720,
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
	/* SXGA */
	{
		.width          = SXGA_WIDTH,
		.height         = SXGA_HEIGHT,
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
static struct s5k4ecgx_win_size  s5k4ecgx_win_sizes_capture[] = {
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
	/* 2576*1932 */
	{
		.width      = WIDTH_2576,  
		.height     = HEIGHT_1932,
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
		printk(KERN_ERR" argp is NULL %s %d\n", __func__, __LINE__);
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
	case V4L2_MBUS_FMT_YVYU8_2X8:
	case V4L2_MBUS_FMT_YUYV8_2X8:
		if (fsize->index >= ARRAY_SIZE(s5k4ecgx_win_sizes)) {
			dev_warn(&client->dev,
				"s5k4ecgx unsupported size %d!\n", fsize->index);
			return -EINVAL;
		}
		fsize->discrete.height = s5k4ecgx_win_sizes[fsize->index].height;
		fsize->discrete.width = s5k4ecgx_win_sizes[fsize->index].width;
		break;
	default:
		dev_err(&client->dev, "s5k4ecgx unsupported format!\n");
		return -EINVAL;
	}
	return 0;
}
static int s5k4ecgx_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
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
	case V4L2_MBUS_FMT_YVYU8_2X8:
	case V4L2_MBUS_FMT_YUYV8_2X8:
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
	
	printk(KERN_INFO "[DHL]s5k4ecgx_s_fmt..!!!\n");
	printk(KERN_INFO "[DHL]mf->code : [%d]\n", mf->code);
	printk(KERN_INFO "[DHL]mf->width : [%d]\n", mf->width);
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
			if(s5k4ecgx_cam_state == S5K4ECGX_STATE_PREVIEW){
				Cam_Printk(KERN_NOTICE "Set Preview format\n");
				switch (mf->width){
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
						s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_352_288, ARRAY_SIZE(regs_s5k4ecgx_preview_352_288),"regs_s5k4ecgx_preview_352_288");					
						Cam_Printk(KERN_ERR"choose qvga(352x288) Preview setting \n");
						break;
					case WIDTH_704:
						s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_704_576, ARRAY_SIZE(regs_s5k4ecgx_preview_704_576),"regs_s5k4ecgx_preview_704_576");					
						Cam_Printk(KERN_ERR"choose qvga(704x576) Preview setting \n");
						break;
					case D1_WIDTH:
						if(mf->height == HEIGHT_540){		
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_720_540, ARRAY_SIZE(regs_s5k4ecgx_preview_720_540),"regs_s5k4ecgx_preview_720_540");					
							Cam_Printk(KERN_ERR"choose d1(720x540) Preview setting \n");
							sensor->preview_size = PREVIEW_SIZE_720_540;
						}else{
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_720_480, ARRAY_SIZE(regs_s5k4ecgx_preview_720_480),"regs_s5k4ecgx_preview_720_480");
							Cam_Printk(KERN_ERR"choose d1(720x480) Preview setting \n");
							sensor->preview_size = PREVIEW_SIZE_720_480;
						}
						break;
					case VGA_WIDTH:
						s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_640_480, ARRAY_SIZE(regs_s5k4ecgx_preview_640_480),"regs_s5k4ecgx_preview_640_480");
						Cam_Printk(KERN_ERR"choose vga(640x480) Preview setting \n");
						sensor->preview_size = PREVIEW_SIZE_640_480;
						break;
					case Wide4K_WIDTH:
						s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_800_480, ARRAY_SIZE(regs_s5k4ecgx_preview_800_480),"regs_s5k4ecgx_preview_800_480");					
						Cam_Printk(KERN_ERR"choose WVGA(800x480) Preview setting \n");
						break;
					case WIDTH_960:
						if(mf->height == HEIGHT_540){
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_960_540, ARRAY_SIZE(regs_s5k4ecgx_preview_960_540),"regs_s5k4ecgx_preview_960_540");					
							Cam_Printk(KERN_ERR"choose WVGA(960x540) Preview setting \n");
							sensor->preview_size = PREVIEW_SIZE_960_540;
						}else{
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_960_720, ARRAY_SIZE(regs_s5k4ecgx_preview_960_720),"regs_s5k4ecgx_preview_960_720");					
							Cam_Printk(KERN_ERR"choose WVGA(960x720) Preview setting \n");
						}
						break;
					case HD_WIDTH:
						s5k4ecgx_write_regs(c, regs_s5k4ecgx_preview_1280_720, ARRAY_SIZE(regs_s5k4ecgx_preview_1280_720),"regs_s5k4ecgx_preview_1280_720");
						Cam_Printk(KERN_ERR"choose HD(1280x720) Preview setting \n");
						sensor->preview_size = PREVIEW_SIZE_1280_720;
						break;
					default:
						printk("\n unsupported size for preview! %s %d w=%d h=%d\n", __FUNCTION__, __LINE__, mf->width, mf->height);
						goto out;
						break;
				}
				if(sensor->mode == S5K4ECGX_MODE_CAMCORDER){
					Cam_Printk(KERN_NOTICE "[DHL] Stable Time for AE [200msec]... !!!! \n");
					msleep(200);
				}
			}
			else if(s5k4ecgx_cam_state == S5K4ECGX_STATE_CAPTURE){
				switch (mf->width){			
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
						} else if((mf->height)==Wide2_4M_HEIGHT){//2.4M  Wide Capture
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_2048_1232, ARRAY_SIZE(regs_s5k4ecgx_capture_2048_1232),"regs_s5k4ecgx_capture_2048_1232");
							Cam_Printk(KERN_ERR"choose 2.4M(2048x1232) Wide jpeg setting \n");
						} else if((mf->height)==HEIGHT_1152){//2.4M 16:9 Wide Capture
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_2048_1152, ARRAY_SIZE(regs_s5k4ecgx_capture_2048_1152),"regs_s5k4ecgx_capture_2048_1152");
							Cam_Printk(KERN_ERR"choose 2.4M(2048x1152) Wide jpeg setting \n");
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
						} else if((mf->height)==Wide4M_HEIGHT){//4M Wide Capture
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_2560_1536, ARRAY_SIZE(regs_s5k4ecgx_capture_2560_1536),"regs_s5k4ecgx_capture_2560_1536");
							Cam_Printk(KERN_ERR"choose 4M(2560x1536) Wide jpeg setting \n");
						} else if((mf->height)==HEIGHT_1440){//4M Wide Capture
							s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_2560_1440, ARRAY_SIZE(regs_s5k4ecgx_capture_2560_1440),"regs_s5k4ecgx_capture_2560_1440");
							Cam_Printk(KERN_ERR"choose 4M(2560x1440) Wide jpeg setting \n");
						}
						break;
					case WIDTH_2576: // 4K Wide Capture
						s5k4ecgx_write_regs(c, regs_s5k4ecgx_capture_2576_1932, ARRAY_SIZE(regs_s5k4ecgx_capture_2576_1932),"regs_s5k4ecgx_capture_2576_1932");
						Cam_Printk(KERN_ERR"choose 5M(2576x1932) jpeg setting \n");
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
			printk("\n unsupported format! %s %d\n", __func__, __LINE__);
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
	struct v4l2_captureparm *cp = &parms->parm.capture;
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.numerator = 1;
	cp->timeperframe.denominator = S5K4ECGX_FRAME_RATE;
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
	u32 ret = 0;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	ret = s5k4ecgx_set_regs(client, value, saturation_tables, _ARRAY_SIZE(saturation_tables));
	if(ret) {
		printk(S5K4ECGX_MOD_NAME "saturation value is not supported or write failed!!!\n");
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
	*value = sensor->saturation;
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
	u32 ret = 0;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	ret = s5k4ecgx_set_regs(client, value, ev_tables, _ARRAY_SIZE(ev_tables));
	if(ret) {
		printk(S5K4ECGX_MOD_NAME "ev value is not supported or write failed!!!\n");
		return -EINVAL;
	}
	sensor->ev = value;
	Cam_Printk(KERN_NOTICE "%s success [ev:%d]\n",__func__, sensor->ev);
	return 0;
}
static int s5k4ecgx_q_brightness(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	Cam_Printk(KERN_NOTICE "s5k4ecgx_get_scene is called...\n"); 
	*value = sensor->ev;
	return 0;
}
static int s5k4ecgx_t_contrast(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->contrast;
	u32 ret = 0;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	ret = s5k4ecgx_set_regs(client, value, contrast_tables, _ARRAY_SIZE(contrast_tables));
	if(ret) {
		printk(S5K4ECGX_MOD_NAME "contrast value is not supported or write failed!!!\n");
		return -EINVAL;
	}
	sensor->contrast = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->contrast);
	return 0;
}
static int s5k4ecgx_q_contrast(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	Cam_Printk(KERN_NOTICE "s5k4ecgx_q_quality is called...\n"); 
	*value = sensor->contrast;
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
static int s5k4ecgx_t_whitebalance(struct i2c_client *client, int value);
static int s5k4ecgx_t_effect(struct i2c_client *client, int value);
static int s5k4ecgx_t_scene(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->scene;
	u32 ret = 0;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	if(value != SCENE_OFF){
		s5k4ecgx_write_regs(client,regs_s5k4ecgx_scene_off,ARRAY_SIZE(regs_s5k4ecgx_scene_off),"regs_s5k4ecgx_scene_off");		
		Cam_Printk(KERN_NOTICE "regs_s5k4ecgx_scene_off");
	}
	s5k4ecgx_t_whitebalance(client,WB_AUTO);
	s5k4ecgx_t_effect(client,EFFECT_OFF);
	if(value == SCENE_PARTY || value == SCENE_BEACH || value == SCENE_FIRE) {
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,5,0);
	}
	ret = s5k4ecgx_set_regs(client, value, scene_tables, _ARRAY_SIZE(scene_tables));
	if(ret) {
		printk(S5K4ECGX_MOD_NAME "scene value is not supported or write failed!!!\n");
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
	*value = sensor->scene;
	return 0;
}
static int s5k4ecgx_t_whitebalance(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->wb;
	u32 ret = 0;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	if(value >= 0 && value <= WB_INCANDESCENT) {
			if(value == WB_AUTO) {
				s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,3,1);
			} else {
				s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,3,0);
			}
	}
	ret = s5k4ecgx_set_regs(client, value, whitebalance_tables, _ARRAY_SIZE(whitebalance_tables));
	if(ret) {
		printk(S5K4ECGX_MOD_NAME "whitebalance value is not supported or write failed!!!\n");
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
	*value = sensor->wb;
	return 0;
}
static int s5k4ecgx_t_effect(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->effect;
	u32 ret = 0;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	
	ret = s5k4ecgx_set_regs(client, value, effect_tables, _ARRAY_SIZE(effect_tables));
	if(ret) {
		printk(S5K4ECGX_MOD_NAME "effect value is not supported or write failed!!!\n");
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
	*value = sensor->effect;
	return 0;
}
static int s5k4ecgx_t_ISO(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->iso;
	u32 ret = 0;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	if(value >= 0 && value <= ISO_400) {
			if(value == ISO_AUTO) {
				s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,5,1);
			} else {
				s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(client,5,0);
			}
	}
	ret = s5k4ecgx_set_regs(client, value, ISO_tables, _ARRAY_SIZE(ISO_tables));
	if(ret) {
		printk(S5K4ECGX_MOD_NAME "ISO value is not supported or write failed!!!\n");
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
	*value = sensor->iso;
	return 0;
}
static int s5k4ecgx_t_photometry(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->photometry;
	u32 ret = 0;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	ret = s5k4ecgx_set_regs(client, value, metering_tables, _ARRAY_SIZE(metering_tables));
	if(ret) {
		printk(S5K4ECGX_MOD_NAME "metering value is not supported or write failed!!!\n");
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
	*value = sensor->photometry;
	return 0;
}
static int s5k4ecgx_t_quality(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->quality;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	switch(value){
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
	*value = sensor->quality;
	return 0;
}

static int s5k4ecgx_t_sharpness(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->sharpness;
	u32 ret = 0;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	ret = s5k4ecgx_set_regs(client, value, sharpness_tables, _ARRAY_SIZE(sharpness_tables));
	if(ret) {
		printk(S5K4ECGX_MOD_NAME "sharpness value is not supported or write failed!!!\n");
		return -EINVAL;
	}
	sensor->sharpness = value;
	Cam_Printk(KERN_NOTICE "%s success [QUALITY e:%d]\n",__func__, sensor->sharpness);
	return 0;
}
static int s5k4ecgx_q_sharpness(struct i2c_client *client, __s32 *value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	Cam_Printk(KERN_NOTICE "s5k4ecgx_q_sharpness is called...\n"); 
	*value = sensor->sharpness;
	return 0;
}
static int s5k4ecgx_t_fps(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->fps;
	u32 ret = 0;
	Cam_Printk(KERN_NOTICE "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);
	if(old_value == value){
		Cam_Printk(KERN_NOTICE "%s new value is same as existing value\n", __func__);
	}
	ret = s5k4ecgx_set_regs(client, value, fps_tables, _ARRAY_SIZE(fps_tables));
	if(ret) {
		printk(S5K4ECGX_MOD_NAME "fps value is not supported or write failed!!!\n");
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
	*value = sensor->fps;
	return 0;
}
static int s5k4ecgx_g_frame_time(struct i2c_client *client,__s32 *value)
{
		u16 frame_time =0;
		u16 temp1 = 0;
		int err;
		
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
		u16 frame_time =0;
		u16 temp1 = 0;
		int err;
		
		err = s5k4ecgx_write(client,0xFCFC, 0xD000);
		err = s5k4ecgx_write(client,0x002C, 0x7000);
	
		err = s5k4ecgx_write(client, 0x002E, 0x215E); 
		err = s5k4ecgx_read (client, 0x0F12, &temp1);
	
		*value = temp1;
	
		Cam_Printk(KERN_NOTICE "%s --------s5k4ecgx_mode_switch_check [ %d ]\n",__func__,temp1);
	
		return 0;
}

static int s5k4ecgx_get_frame_time(struct i2c_client *client)
{
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
	if (temp3 <= 10){
		Cam_Printk(KERN_NOTICE "Lightness_Low.. \r\n");
		*value = Lightness_Low;
	}else{
		Cam_Printk(KERN_NOTICE "Lightness_Normal.. \r\n");
		*value = Lightness_Normal;
	}
	Cam_Printk(KERN_NOTICE "%s success [Frame Time :%d ]\n",__func__,*value);
	return 0;
}
static int s5k4ecgx_ESD_check(struct i2c_client *client,  struct v4l2_control *ctrl)
{
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
	if((esd_value1 != 0x0000)||(esd_value2 != 0xAAAA)){
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
	Cam_Printk(KERN_NOTICE "s5k4ecgx_set_mode is called... mode = %d\n", sensor->mode);
	return 0;
}
static const struct v4l2_queryctrl s5k4ecgx_controls[] = {
	{
		.id		= V4L2_CID_PRIVATE_GET_MIPI_PHY,
		.type		= V4L2_CTRL_TYPE_CTRL_CLASS,
		.name		= "get mipi phy config"
	},
};
static int s5k4ecgx_set_still_status(void)
{
	Cam_Printk(KERN_NOTICE "[DHL]s5k4ecgx_set_still_status.. \n");
	soc_camera_set_ddr_qos(528000);
	s5k4ecgx_cam_state = S5K4ECGX_STATE_CAPTURE;	
	return 0;
}
static int s5k4ecgx_set_preview_status(struct i2c_client *client, int value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->cam_mode;
	Cam_Printk(KERN_NOTICE "[DHL]s5k4ecgx_set_preview_status.. \n");
	soc_camera_set_ddr_qos(312000);
	if(value == CAMCORDER_MODE){ // Camcorder state
		s5k4ecgx_write_regs(client, regs_s5k4ecgx_camcorder, ARRAY_SIZE(regs_s5k4ecgx_camcorder),"regs_s5k4ecgx_camcorder");
		Cam_Printk(KERN_NOTICE "s5k4ecgx_set_preview_status : Camcorder mode\n");
		sensor->mode = S5K4ECGX_MODE_CAMCORDER;
		s5k4ecgx_Movie_flash_state = MOVIE_FLASH;
		printk("s5k4ecgx_set_preview_status : Current movie flash state is Movie state\n");
	}
	else{ // Preview state
		if(old_value == CAMCORDER_MODE){
			/* When return to preview from camcorder, camcorder disable code should be written to sensor */
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_camcorder_disable, ARRAY_SIZE(regs_s5k4ecgx_camcorder_disable),"regs_s5k4ecgx_camcorder_disable");
			msleep(200);
			printk("s5k4ecgx_set_preview_statusg: Return to preview from camcorder, set camcorders disable.\n");
		}
		sensor->mode = S5K4ECGX_MODE_CAMERA;
		s5k4ecgx_Movie_flash_state = FALSE;
		printk("s5k4ecgx_set_preview_status : Current movie flash state is Preview state\n");
	}	
	sensor->cam_mode = value;
	s5k4ecgx_cam_state = S5K4ECGX_STATE_PREVIEW;	
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
	
	Cam_Printk(KERN_NOTICE " s5k4ecgx_set_flash_mode...\n");
	if (mode){		/* start AF */
		need_flash = 0;
		Cam_Printk(KERN_NOTICE " Flash Turn ON...\n");
		if(sensor->flash_mode == S5K4ECGX_FLASH_ON)
			need_flash = 1;
		else if(sensor->flash_mode == S5K4ECGX_FLASH_TORCH){
			need_flash = 1;
			s5k4ecgx_Movie_flash_state = MOVIE_FLASH;
		}else if(sensor->flash_mode == S5K4ECGX_FLASH_AUTO){
			LightStatus1 = s5k4ecgx_check_illuminance_status(client);
			if(LightStatus1 == CAMERA_SENSOR_LIGHT_STATUS_LOW)
				need_flash = 1;
		}
		if(need_flash == 1)
			s5k4ecgx_set_flash(client, MOVIE_FLASH);
	}else {
		if( !Recording_Snapshot_State ){ // In recording snapshot mode, torch mode camera flash should not be turned off
		Cam_Printk(KERN_NOTICE " Flash Turn OFF...\n");
		s5k4ecgx_set_flash(client, FLASH_OFF);
	        }
	}
	return 0;
}
static int s5k4ecgx_get_flash_status(struct i2c_client *client)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int flash_status = 0;
	camera_light_status_type LightStatus2 = CAMERA_SENSOR_LIGHT_STATUS_INVALID;
	Cam_Printk(KERN_NOTICE " s5k4ecgx_get_flash_status...\n");
	LightStatus2 = s5k4ecgx_check_illuminance_status(client);
	if(LightStatus2 == CAMERA_SENSOR_LIGHT_STATUS_LOW){ 
		if ((sensor->flash_mode == S5K4ECGX_FLASH_AUTO) || (sensor->flash_mode == S5K4ECGX_FLASH_ON) || (sensor->flash_mode == S5K4ECGX_FLASH_TORCH) ){
			flash_status = 1;
		}
	}
	Cam_Printk(KERN_NOTICE " %d", flash_status);
	return flash_status;
}
static void s5k4ecgx_set_flash(struct i2c_client *client, int mode)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int i = 0;
	Cam_Printk(KERN_NOTICE " %d", mode);
	Cam_Printk(KERN_NOTICE "s5k4ecgx_set_flash : %d...\n", mode);  
	switch(mode){
		case FLASH_OFF:
			if( gflash_status == TRUE ){
				Cam_Printk(KERN_NOTICE " FLASH OFF");
				gflash_status = FALSE;
				ledtrig_flash_ctrl(0);
				ledtrig_torch_ctrl(0);
			}
			break;
			
		case MOVIE_FLASH:
			Cam_Printk(" MOVIE FLASH ON");		
			gflash_status = TRUE;	
			ledtrig_torch_ctrl(1);
			break;

		case CAPTURE_FLASH:
			Cam_Printk(" CAPTURE FLASH ON");
			gflash_status = TRUE;
			ledtrig_flash_ctrl(1);
			break;
		case PRE_CAPTURE_FLASH:	
			Cam_Printk(" Pre-CAPTURE FLASH ON");	
			gflash_status = TRUE;	
			ledtrig_torch_ctrl(1);
			break;
	
		default:
			printk(S5K4ECGX_MOD_NAME "The Flash Mode is not supported! : %d\n",mode);
			break;
	}
}
static void s5k4ecgx_main_flash_mode(struct i2c_client *client, int mode)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int i = 0;
	Cam_Printk(KERN_NOTICE "s5k4ecgx_main_flash_mode : %d\n", mode);  
	switch(mode){
		case MAIN_FLASH_ON :	
			s5k4ecgx_write_regs(client, reg_s5k4ecgx_Main_Flash_On, ARRAY_SIZE(reg_s5k4ecgx_Main_Flash_On),"reg_s5k4ecgx_Main_Flash_On");			
			Cam_Printk(KERN_NOTICE "MAIN_FLASH_ON \n");
			sensor->main_flash_mode = true;
			break;
			
		case MAIN_FLASH_OFF :
			if(sensor->main_flash_mode == true){		
				s5k4ecgx_write_regs(client, reg_s5k4ecgx_Main_Flash_Off, ARRAY_SIZE(reg_s5k4ecgx_Main_Flash_Off),"reg_s5k4ecgx_Main_Flash_Off");			
				Cam_Printk(KERN_NOTICE "MAIN_FLASH_OFF \n");
				sensor->main_flash_mode = false;
			}else
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
	volatile int checkvalue = 0, timeout = 30;
	int err = 0;
	u16 frame_time = 0;
	camera_light_status_type LightStatus1 = CAMERA_SENSOR_LIGHT_STATUS_INVALID;
	Cam_Printk(KERN_NOTICE "s5k4ecgx_streamon is called...\n");  
	if(s5k4ecgx_cam_state == S5K4ECGX_STATE_CAPTURE){
		if(s5k4ecgx_Movie_flash_state == MOVIE_FLASH){ // Capture image in recording mode is a Recording snapshot mode
			//J110F don't use recording snapshot.... 
			//disable recording feature for fix Google drive apk flash off issue
			//			Recording_Snapshot_State = TRUE;
			Cam_Printk(KERN_NOTICE "s5k4ecgx_streamon : Recording Snapshot Mode\n");  
		}
	
		frame_time = s5k4ecgx_get_frame_time(client);
		if((sensor->flash_mode == S5K4ECGX_FLASH_ON) || (sensor->flash_mode == S5K4ECGX_FLASH_TORCH)){
			need_flash = TRUE;
		}else if(sensor->flash_mode == S5K4ECGX_FLASH_AUTO){
			if(auto_flash_lock == FALSE) {
				LightStatus1 = s5k4ecgx_check_illuminance_status(client);
				
				if(LightStatus1 == CAMERA_SENSOR_LIGHT_STATUS_LOW)
					need_flash = TRUE;
				else
					need_flash = FALSE;
			}
		}else {
			need_flash = FALSE;
		}
		if( (need_flash == TRUE) && (!Recording_Snapshot_State) ){
			Cam_Printk(KERN_NOTICE "Main Flash ON...\n");  
			s5k4ecgx_set_flash(client, CAPTURE_FLASH);
			s5k4ecgx_main_flash_mode(client, MAIN_FLASH_ON);
		} else {
			msleep(frame_time);
		}
		
		s5k4ecgx_write_regs(client, regs_s5k4ecgx_Capture_Start, ARRAY_SIZE(regs_s5k4ecgx_Capture_Start),"regs_s5k4ecgx_Capture_Start"); 
 		sensor->state = S5K4ECGX_STATE_CAPTURE;
	
		while((checkvalue != 0x0100) && (timeout-- > 0)){
			msleep(10);
			s5k4ecgx_mode_switch_check(client, &checkvalue);
			if((checkvalue != 0x0100) && (timeout == 4))
				Cam_Printk(KERN_NOTICE "s5k4ecgx polling failed, and reset the capture mode...\n"); 
		}
		s5k4ecgx_s_exif_info(client); // Update EXIF	
 	}else { // Preview state & Recording state
		if(sensor->main_flash_mode == TRUE){
			Cam_Printk(KERN_NOTICE "[DHL]Flash OFF...!!!!! \n");  
 			s5k4ecgx_main_flash_mode(client, MAIN_FLASH_OFF);
			s5k4ecgx_set_flash(client, FLASH_OFF);
			need_flash = FALSE;
		}
		if(sensor->state == S5K4ECGX_STATE_CAPTURE){
			Cam_Printk(KERN_NOTICE "The Capture  is finished..!! AE & AWB Unlock.. \n");
			s5k4ecgx_AE_lock(client, AE_UNLOCK);
			s5k4ecgx_AWB_lock(client, AWB_UNLOCK);
			sensor->focus_type = AUTO_FOCUS_MODE;
			Recording_Snapshot_State = false; // Init recording snapshot flag as false
		}
		
		err = s5k4ecgx_t_whitebalance(client, sensor->wb);
		if((sensor->scene == SCENE_SUNSET) ||(sensor->scene == SCENE_DAWN) ||(sensor->scene == SCENE_CANDLE) || (sensor->scene == SCENE_NIGHT))
			err = s5k4ecgx_t_scene(client, sensor->scene); 	
		
		if(sensor->scene == SCENE_OFF){ // Scene mode OFF state
			Cam_Printk(KERN_NOTICE "Added setting before preview!!!!!!! \n");
			s5k4ecgx_AE_lock(client, AE_UNLOCK);
			s5k4ecgx_AWB_lock(client, AWB_UNLOCK);
			s5k4ecgx_t_ISO(client,sensor->iso);
			s5k4ecgx_t_photometry(client,sensor->photometry);
			if(sensor->mode != S5K4ECGX_MODE_CAMCORDER){ // Preview mode
			s5k4ecgx_t_brightness(client,sensor->ev);
		        }		
		}
		if(sensor->mode != S5K4ECGX_MODE_CAMCORDER){ // Preview mode
			s5k4ecgx_write_regs(client, regs_s5k4ecgx_Preview_Return, ARRAY_SIZE(regs_s5k4ecgx_Preview_Return),"regs_s5k4ecgx_Preview_Return"); 	
			Cam_Printk(KERN_NOTICE "Return to Preview..\n");
 			Cam_Printk(KERN_NOTICE "[start Preview] !!!\n");
 		}
		if((sensor->mode == S5K4ECGX_MODE_CAMCORDER) && (sensor->preview_size == PREVIEW_SIZE_1280_720)){
			printk("aibing debug GC 312\n");
			soc_camera_set_gc2d_qos(312000);
		}
 	}
	return 0;
}

static int s5k4ecgx_streamoff(struct i2c_client *client)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	/* What's wrong with this sensor, it has no stream off function, oh!,Vincent.Wan */
       Cam_Printk(KERN_NOTICE "[DHL]Stream OFF...!!!!\n"); 
	   s5k4ecgx_write_regs(client, regs_s5k4ecgx_stream_off, ARRAY_SIZE(regs_s5k4ecgx_stream_off),"regs_s5k4ecgx_stream_off");
	   if((sensor->mode == S5K4ECGX_MODE_CAMCORDER) && (sensor->preview_size == PREVIEW_SIZE_1280_720)){
		   printk("aibing debug GC 156\n"); 
		   soc_camera_set_gc2d_qos(156000); 
	   }
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
			printk(KERN_INFO "[DHL]V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL.. \n");
			ctrl->value = (__s32)sensor->exif_info.exposure_time.denominal;
			break;
		}
		case V4L2_CID_GET_EXIF_ISO_SPEED:
		{
			struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
			printk(KERN_INFO "[DHL]V4L2_CID_GET_EXIF_ISO_SPEED.. \n");
			ctrl->value = (__s32)sensor->exif_info.iso_speed_rationg;
			break;
		}
		case V4L2_CID_GET_EXIF_FLASH:
		{
			struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
			printk(KERN_INFO "[DHL]V4L2_CID_GET_EXIF_FLASH.. \n");
			ctrl->value = gflash_exif;
			break;
		}
		case V4L2_CID_GET_FLASH_STATUS:
			printk(KERN_INFO "[DHL]V4L2_CID_GET_FLASH_STATUS.. \n");
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
	Cam_Printk(KERN_NOTICE "ioctl_s_ctrl is called...(%llx)\n", ctrl->id);
	switch (ctrl->id){
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
		case V4L2_CID_FPS:
			retval = s5k4ecgx_t_fps(client,ctrl->value);
			break;
		case V4L2_CID_SELECT_MODE:
			retval = s5k4ecgx_set_mode(client,ctrl->value);
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
			retval = s5k4ecgx_set_preview_status(client, ctrl->value);
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
static int s5k4ecgx_video_probe(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k4ecgx_info *priv = to_s5k4ecgx(sd);
	u8 modelhi, modello;
	int ret, i;
	s5k4ecgx_get_SensorVendorID(client);

	/*
	 * Make sure it's an s5k4ecgx
	 */
	for(i = 0; i < 3; i++) {
		ret = s5k4ecgx_detect(client);
		if (!ret) {
			Cam_Printk(KERN_NOTICE "=========SYS.LSI s5k4ecgx sensor detected==========\n");
			goto out;
		}
	}
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
	printk(KERN_INFO "get_lux : %d lux\n", cur_lux);
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
	if( AE_Status == AE_Stable_Value){	
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_ae_status - CAMERA_SENSOR_AE_STABLE \r\n");
		AE_Status = CAMERA_SENSOR_AE_STABLE; 
	}else{
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
	u32 luxcheck_low = 0x0038;
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
	if( lightStatus < luxcheck_low){	// Lowlight Snapshot
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_LOW \r\n");
		LightStatus = CAMERA_SENSOR_LIGHT_STATUS_LOW; 
	}else{	// Normal Snapshot
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
	u16 exposure_time_lsb = 0;
	u16 exposure_time_msb= 0;	
	u32 iso_gain= 0;
	u16 iso_a_gain= 0;
	u16 iso_d_gain= 0;
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
	temp1 = ((exposure_time_msb << 16) | exposure_time_lsb);
	exposure_time = 400000 / temp1;
	if(sensor->scene != SCENE_NIGHT && exposure_time<8)
		exposure_time=8;

	sensor->exif_info.exposure_time.inumerator=1;
	sensor->exif_info.exposure_time.denominal=exposure_time;
	Cam_Printk(KERN_NOTICE "s5k4ecgx_s_ISO() \r\n");
	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);
	err = s5k4ecgx_write(client,0x002E, 0x2BC4);
	err = s5k4ecgx_read (client, 0x0F12, &iso_a_gain);	
	err = s5k4ecgx_write(client,0x002E, 0x2BC6);
	err = s5k4ecgx_read (client, 0x0F12, &iso_d_gain);	
	iso_gain = ((iso_a_gain * iso_d_gain) / 0x100)/2;
	Cam_Printk(KERN_NOTICE "iso_gain() %d \n",iso_gain);
	if(iso_gain < 0x100)  
		iso_value =50;
	else if(iso_gain < 0x200) 
		iso_value =100;
	else if(iso_gain < 0x380) 
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
	Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status() \r\n");
	camera_light_status_type LightStatus = CAMERA_SENSOR_LIGHT_STATUS_INVALID;
	err = s5k4ecgx_write(client,0xFCFC, 0xD000);
	err = s5k4ecgx_write(client,0x002C, 0x7000);
	err = s5k4ecgx_write(client, 0x002E, 0x2A3C); 
	err = s5k4ecgx_read (client, 0x0F12, &lightStatus_low_word);		
	err = s5k4ecgx_write(client, 0x002E, 0x2A3E); 
	err = s5k4ecgx_read (client, 0x0F12, &lightStatus_high_word);	
	lightStatus = lightStatus_low_word  | (lightStatus_high_word << 16);
	
	if ( lightStatus > luxcheck_high){	// Highlight Snapshot
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_HIGH \r\n");
		LightStatus = CAMERA_SENSOR_LIGHT_STATUS_HIGH;
	}else if( lightStatus < luxcheck_low){	// Lowlight Snapshot
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_LOW \r\n");
		LightStatus = CAMERA_SENSOR_LIGHT_STATUS_LOW; 
	}else{	// Normal Snapshot
		Cam_Printk(KERN_NOTICE "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_NORMAL \r\n");
		LightStatus =  CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
	}
		
	if((LightStatus==CAMERA_SENSOR_LIGHT_STATUS_LOW)&&(sensor->initial == S5K4ECGX_STATE_INITIAL)){
		s5k4ecgx_write(client,0xFCFC, 0xD000);
		s5k4ecgx_write(client,0x002C, 0x7000);
		s5k4ecgx_write(client, 0x002E, 0x2A70); 
		s5k4ecgx_read(client, 0x0F12, &AE_Check);		
		s5k4ecgx_write(client,0xFCFC, 0xD000);
		s5k4ecgx_write(client,0x002C, 0x7000);
		s5k4ecgx_write(client, 0x002E, 0x2A74); 
		s5k4ecgx_read(client, 0x0F12, &AWB_Check);	
		if((AE_Check==0x01)&&(AWB_Check==0x01))
			Cam_Printk(KERN_NOTICE "AE & AWB Ready\n");
		else{
			Cam_Printk(KERN_NOTICE "AE & AWB Ready Going On...[Done AE %x AWB %x] \n", AE_Check,AWB_Check);
			msleep(5);
		}
	        *LuxCheck = (AE_Check&AWB_Check);
		*LuxCheck = 0x00;
	}else
		*LuxCheck = 0x01;
	
	Cam_Printk(KERN_NOTICE "*LuxCheck %d .. \n",*LuxCheck);
	return 0;
}
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
			printk(KERN_INFO "[DHL]V4L2_CID_GET_EXIF_EXPOSURETIME_DENOMINAL.. \n");
			reg->val = (__s64)sensor->exif_info.exposure_time.denominal;
			break;
		}
		case V4L2_CID_GET_EXIF_ISO_SPEED:
		{
			struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
			printk(KERN_INFO "[DHL]V4L2_CID_GET_EXIF_ISO_SPEED.. \n");
			reg->val = (__s64)sensor->exif_info.iso_speed_rationg;
			break;
		}
		case V4L2_CID_GET_EXIF_FLASH:
		{
			struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
			printk(KERN_INFO "[DHL]V4L2_CID_GET_EXIF_FLASH.. \n");
			reg->val = gflash_exif;
			break;
		}
		case V4L2_CID_GET_FLASH_STATUS:
			printk(KERN_INFO "[DHL]V4L2_CID_GET_FLASH_STATUS.. \n");
			reg->val = s5k4ecgx_get_flash_status(client);
			break;
		case V4L2_CID_ESD_CHECK:
		{
			struct v4l2_control ESDCtrl;
			printk(KERN_INFO "[DHL]V4L2_CID_ESD_CHECK.. \n");
			s5k4ecgx_ESD_check(client, &ESDCtrl);
			reg->val = (__s64)ESDCtrl.value;
			break;
		}
		case V4L2_CID_AF:
                {
			struct v4l2_control focusCtrl;
			s5k4ecgx_get_start_af_delay( client, &focusCtrl);
			reg->val = (__s64)focusCtrl.value;
			break;
		}
		case V4L2_CID_GET_AE_STATUS:
		{
			struct v4l2_control focusCtrl;
			s5k4ecgx_get_AE_status( client, &focusCtrl);
			reg->val = (__s64)focusCtrl.value;
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
	Cam_Printk(KERN_NOTICE "s5k4ecgx_s_register is called...(%llx)\n", reg->reg);
	switch (reg->reg){
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
		case V4L2_CID_FPS:
			retval = s5k4ecgx_t_fps(client,reg->val);
			break;
		case V4L2_CID_SELECT_MODE:
			retval = s5k4ecgx_set_mode(client,reg->val);
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
			retval = s5k4ecgx_set_preview_status(client, reg->val);
			break;		
		case V4L2_CID_SET_FLASH_STATUS:
			printk( "[DHL]V4L2_CID_SET_FLASH_STATUS.. \n");			
			s5k4ecgx_set_flash_mode(client, reg->val);
			break;
		case V4L2_CID_SET_FLASH_MODE:
			sensor->flash_mode = reg->val;
				if(sensor->scene == SCENE_BACKLIGHT){
					if(sensor->flash_mode == S5K4ECGX_FLASH_OFF ){
						printk("The Scene Backlight mode change to Spot setting when the flash off...\n");
						s5k4ecgx_write_regs(client, regs_s5k4ecgx_metering_spot, ARRAY_SIZE(regs_s5k4ecgx_metering_spot),"regs_s5k4ecgx_metering_spot");	
                                        }else{
						printk("The Scene Backlight mode change to CenterWeighted setting when the flash off...\n");
						s5k4ecgx_write_regs(client, regs_s5k4ecgx_metering_centerweighted, ARRAY_SIZE(regs_s5k4ecgx_metering_centerweighted),"regs_s5k4ecgx_metering_centerweighted");
					}
				}
			break;
				
		default:
			Cam_Printk(S5K4ECGX_MOD_NAME "[id]Invalid value is ordered!!!\n");
			break;
	}
	return retval;
}
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
	switch(value){
		case AE_LOCK :
			if(s5k4ecgx_AE_lock_state == false){
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AE_lock, ARRAY_SIZE(regs_s5k4ecgx_AE_lock),"regs_s5k4ecgx_AE_lock");			
				Cam_Printk(KERN_NOTICE "AE_LOCK \n");
				s5k4ecgx_AE_lock_state = true;
			}else
				Cam_Printk(KERN_NOTICE "No need to AWB_AE_LOCK \n");
			break;
		case AE_UNLOCK :
			if(s5k4ecgx_AE_lock_state == true){
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AE_unlock, ARRAY_SIZE(regs_s5k4ecgx_AE_unlock),"regs_s5k4ecgx_AE_unlock");			
				Cam_Printk(KERN_NOTICE "AE_UNLOCK \n");
				s5k4ecgx_AE_lock_state = false;
			}else
				Cam_Printk(KERN_NOTICE "No need to AWB_AE_UNLOCK \n");
			break;
	}
	sensor->ae = value;
}

static void s5k4ecgx_AWB_lock(struct i2c_client *client,s32 value)
{       
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
   	Cam_Printk(KERN_NOTICE "AWB set value = %d\n", value);
	switch(value){
		case AWB_LOCK :	
			if(s5k4ecgx_AWB_lock_state == false && sensor->wb==WB_AUTO){
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AWB_lock, ARRAY_SIZE(regs_s5k4ecgx_AWB_lock),"regs_s5k4ecgx_AWB_lock");			
				Cam_Printk(KERN_NOTICE "AWB_LOCK \n");
				s5k4ecgx_AWB_lock_state = true;
			}else
				Cam_Printk(KERN_NOTICE "No need to AWB LOCK \n");
			break;
		case AWB_UNLOCK :
			if(s5k4ecgx_AWB_lock_state == true && sensor->wb==WB_AUTO){		
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AWB_unlock, ARRAY_SIZE(regs_s5k4ecgx_AWB_unlock),"regs_s5k4ecgx_AWB_unlock");			
				Cam_Printk(KERN_NOTICE "AWB_UNLOCK \n");
				s5k4ecgx_AWB_lock_state = false;
			}else
				Cam_Printk(KERN_NOTICE "No need to AWB UNLOCK \n");
			break;
}
	sensor->awb = value;
}
static int s5k4ecgx_AE_AWB_lock(struct i2c_client *client,s32 value)
{       
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
    Cam_Printk(KERN_NOTICE "AE_AWB set value = %d\n", value);
	switch(value){
		case AE_LOCK :
		case AE_UNLOCK :
			s5k4ecgx_AE_lock(client, value);
			break;
		case AWB_LOCK :
		case AWB_UNLOCK:
			s5k4ecgx_AWB_lock(client, value);
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
	
	switch( step ){
		case SET_FOCUS_STEP1:
			if( value == S5K4ECGX_AF_SET_NORMAL )
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_1, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_1),"regs_s5k4ecgx_AF_normal_mode_1");			
			else if ( value == S5K4ECGX_AF_SET_MACRO )
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_1, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_1),"regs_s5k4ecgx_AF_macro_mode_1");			
			else{
				Cam_Printk(KERN_NOTICE "[AF]Invalid value is ordered!!! : %d\n",value);
				goto focus_fail;
			}
			break;
			
		case SET_FOCUS_STEP2:
			if( value == S5K4ECGX_AF_SET_NORMAL )
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_2, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_2),"regs_s5k4ecgx_AF_normal_mode_2");			
			else if ( value == S5K4ECGX_AF_SET_MACRO )
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_2, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_2),"regs_s5k4ecgx_AF_macro_mode_2");			
			else{
				Cam_Printk(KERN_NOTICE "[AF]Invalid value is ordered!!! : %d\n",value);
				goto focus_fail;
			}
			break;
			
		case SET_FOCUS_STEP3:
			if( value == S5K4ECGX_AF_SET_NORMAL ){
				if(sensor->scene != SCENE_NIGHT)
					s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_3, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_3),"regs_s5k4ecgx_AF_normal_mode_3");			
				Cam_Printk(KERN_NOTICE "S5K4ECGX_AF_SET_NORMAL \n");
				sensor->focus_mode = value;
			}else if ( value == S5K4ECGX_AF_SET_MACRO ){
				if(sensor->scene != SCENE_NIGHT)
					s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_3, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_3),"regs_s5k4ecgx_AF_macro_mode_3");			
				Cam_Printk(KERN_NOTICE "S5K4ECGX_AF_SET_MACRO \n");	
				sensor->focus_mode = value;			
			}else{
				Cam_Printk(KERN_NOTICE "[AF]Invalid value is ordered!!! : %d\n",value);
				goto focus_fail;
			}
			break;

		case SET_FOCUS_ALL:
			frame_delay = s5k4ecgx_get_frame_time(client);
			Cam_Printk(KERN_NOTICE "[DHL] frame_delay = %d\n", frame_delay);
			switch(value){
			case S5K4ECGX_AF_SET_NORMAL :
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_1, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_1),"regs_s5k4ecgx_AF_normal_mode_1");
				/* This soft-landing delay is for solving tick-noise.
				   If camera switching spec over issue happens, consider to reduce delay time. */
				msleep(frame_delay);	//delay_1_frame

				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_2, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_2),"regs_s5k4ecgx_AF_normal_mode_2");

				msleep(frame_delay);	//delay_1_frame

				if(sensor->scene != SCENE_NIGHT){
					s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_normal_mode_3, ARRAY_SIZE(regs_s5k4ecgx_AF_normal_mode_3),"regs_s5k4ecgx_AF_normal_mode_3");
					msleep(100); // Lens soft-landing delay
				}
				Cam_Printk(KERN_NOTICE "S5K4ECGX_AF_SET_NORMAL \n");
				break;
			case S5K4ECGX_AF_SET_MACRO :
				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_1, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_1),"regs_s5k4ecgx_AF_macro_mode_1");
				/* This soft-landing delay is for solving tick-noise.
				   If camera switching spec over issue happens, consider to reduce delay time. */
				msleep(frame_delay);	//delay_1_frame

				s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_2, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_2),"regs_s5k4ecgx_AF_macro_mode_2");

				msleep(frame_delay);	//delay_1_frame

				if(sensor->scene != SCENE_NIGHT){
					s5k4ecgx_write_regs(client, regs_s5k4ecgx_AF_macro_mode_3, ARRAY_SIZE(regs_s5k4ecgx_AF_macro_mode_3),"regs_s5k4ecgx_AF_macro_mode_3");
					msleep(100); // Lens soft-landing delay
				}
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

static int s5k4ecgx_get_AE_status(struct i2c_client *client, struct v4l2_control *ctrl){

	u32 AE_value = 0x0001;
	s5k4ecgx_write(client,0xFCFC, 0xD000);
	s5k4ecgx_write(client,0x002C, 0x7000);
	s5k4ecgx_write(client, 0x002E, 0x2C74);
	s5k4ecgx_read (client, 0x0F12, &AE_value);
	Cam_Printk(KERN_NOTICE "%s : %x \n",__func__,AE_value);  
	ctrl->value = AE_value;

	return 0;
}

static int s5k4ecgx_get_start_af_delay(struct i2c_client *client, struct v4l2_control *ctrl)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	camera_light_status_type illuminance=CAMERA_SENSOR_LIGHT_STATUS_NORMAL;

	ctrl->value = 0;

	illuminance = s5k4ecgx_check_illuminance_status(client);
	if (sensor->flash_mode == S5K4ECGX_FLASH_AUTO)
		auto_flash_lock = TRUE;

	if (((sensor->flash_mode == S5K4ECGX_FLASH_AUTO) && (illuminance == CAMERA_SENSOR_LIGHT_STATUS_LOW))
			|| (sensor->flash_mode == S5K4ECGX_FLASH_ON) || (sensor->flash_mode == S5K4ECGX_FLASH_TORCH)){
		if(s5k4ecgx_Movie_flash_state != MOVIE_FLASH){ /* Disable to set pre-flash state in camcorder mode */
			Cam_Printk(KERN_NOTICE "Pre-Flash mode is start...\n");  

			need_flash = TRUE;
			/**++ Pre-Flash Process Start ++**/
			s5k4ecgx_write_regs(client,reg_s5k4ecgx_FAST_AE_ON,ARRAY_SIZE(reg_s5k4ecgx_FAST_AE_ON),"reg_s5k4ecgx_FAST_AE_ON");	
			s5k4ecgx_write_regs(client,reg_s5k4ecgx_Pre_Flash_On,ARRAY_SIZE(reg_s5k4ecgx_Pre_Flash_On),"reg_s5k4ecgx_Pre_Flash_On");
			s5k4ecgx_set_flash(client, PRE_CAPTURE_FLASH);
			/**-- Pre-Flash Process Start --**/

			ctrl->value = 400;
		}
	}	

	return 0;
}

static int s5k4ecgx_set_focus(struct i2c_client *client,s32 value)
{       
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	camera_light_status_type illuminance=CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
	camera_ae_stable_type ae_status=CAMERA_SENSOR_AE_STABLE;
	u16 i=0;
	Cam_Printk(KERN_NOTICE "s5k4ecgx_set_focus_status is called...[%d]\n",value);
	if(value == S5K4ECGX_AF_START){
		if(sensor->focus_type == AUTO_FOCUS_MODE){
			Cam_Printk(KERN_NOTICE "FOCUS_MODE_AUTO! AE Lock.. \n");
			s5k4ecgx_AE_lock(client, AE_LOCK);
			if(!need_flash){
			s5k4ecgx_AWB_lock(client, AWB_LOCK);
				Cam_Printk(KERN_NOTICE "AF without Flash..!!AWB Lock.. \n");
		        }
		}else if((sensor->focus_type == TOUCH_FOCUS_MODE)&&(gTouch_shutter_value==TRUE)){
			Cam_Printk(KERN_NOTICE "FOCUS_MODE_HALF_SHUTTER_TOUCH! AE Lock.. \n");
			s5k4ecgx_AE_lock(client, AE_LOCK);
			if(!need_flash){
			s5k4ecgx_AWB_lock(client, AWB_LOCK);
				Cam_Printk(KERN_NOTICE "AF without Flash..!!AWB Lock.. \n");
			}
			gTouch_shutter_value=FALSE;	
		}else{
			Cam_Printk(KERN_NOTICE "AE & AWB Unlock.. \n");
			s5k4ecgx_AE_lock(client, AE_UNLOCK);
			s5k4ecgx_AWB_lock(client, AWB_UNLOCK);
			Cam_Printk(KERN_NOTICE "CHECK FOCUS MODE for not AE,AWB Lock = %d\n",sensor->focus_type);
		}
	}
	switch(value){
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
			if( (need_flash == TRUE) && (s5k4ecgx_Movie_flash_state != MOVIE_FLASH) ){ /* If Record flash mode, pass this pre-flash off code */
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
	
	switch(value){
		case S5K4ECGX_AF_CHECK_STATUS :
			Cam_Printk(KERN_NOTICE "s5k4ecgx_get_auto_focus is called...\n"); 
			s5k4ecgx_write(client, 0x002C, 0x7000);
			s5k4ecgx_write(client, 0x002E, 0x2EEE);
			s5k4ecgx_read(client, 0x0F12, (unsigned short*)&status);
			Cam_Printk(KERN_NOTICE "AutoFocus_STATUS : 0x%04X\n", status);
			switch(status & 0xFFFF){    
				case 1:
					Cam_Printk(KERN_NOTICE "[1st]AF - PROGRESS \n");
					ctrl->value = S5K4ECGX_AF_STATUS_PROGRESS;
					break;
				case 2:
					Cam_Printk(KERN_NOTICE "[1st]AF - SUCCESS \n");
					ctrl->value = S5K4ECGX_AF_STATUS_SUCCESS;
					Cam_Printk(KERN_NOTICE "AF is SUCCESS..!! \n");
					break;
				default:
					{
					Cam_Printk(KERN_NOTICE "[1st]AF - FAIL\n");
					ctrl->value = S5K4ECGX_AF_STATUS_FAIL;
						if(s5k4ecgx_Movie_flash_state != MOVIE_FLASH){ /* Disable to set pre-flash state in camcorder mode */
							Cam_Printk(KERN_NOTICE "[AF-FAIL]Pre-Flash mode is end...\n");  
							
							/**++ Pre-Flash Process End ++**/
							s5k4ecgx_write_regs(client,reg_s5k4ecgx_FAST_AE_Off,ARRAY_SIZE(reg_s5k4ecgx_FAST_AE_Off),"reg_s5k4ecgx_FAST_AE_Off");	
							s5k4ecgx_write_regs(client,reg_s5k4ecgx_Pre_Flash_Off,ARRAY_SIZE(reg_s5k4ecgx_Pre_Flash_Off),"reg_s5k4ecgx_Pre_Flash_Off");
							//For prevent goggle apk flash turn off in torch mode
							if(sensor->flash_mode != S5K4ECGX_FLASH_TORCH)
								s5k4ecgx_set_flash(client, FLASH_OFF);
							sensor->focus_type = AUTO_FOCUS_MODE;
							/**-- Pre-Flash Process End --**/
						}
					}
					Cam_Printk(KERN_NOTICE "AF is FAIL..!! AE & AWB Unlock.. \n");
					s5k4ecgx_AE_lock(client, AE_UNLOCK);
					s5k4ecgx_AWB_lock(client, AWB_UNLOCK);
					break;
			}
			break;
		case S5K4ECGX_AF_CHECK_2nd_STATUS :
			s5k4ecgx_write(client, 0x002C, 0x7000);
			s5k4ecgx_write(client, 0x002E, 0x2207);
			s5k4ecgx_read(client, 0x0F12, (unsigned short*)&status);
			Cam_Printk(KERN_NOTICE, "AutoFocus_2nd_STATUS : 0x%04X\n", status);
			switch((status & 0xFF00) >= 0x0100){
				case 1:
					Cam_Printk(KERN_NOTICE "[2nd]AF - PROGRESS \n");
					ctrl->value = S5K4ECGX_AF_STATUS_PROGRESS;
					break;
				case 0:
					Cam_Printk(KERN_NOTICE "[2nd]AF - SUCCESS \n");
					ctrl->value = S5K4ECGX_AF_STATUS_SUCCESS;
					break;
			}
			break;
		default:
			printk(S5K4ECGX_MOD_NAME"AF CHECK CONTROL NOT MATCHED\n");
			break;
		}
	
		Cam_Printk(KERN_NOTICE, "result = %d (1.progress, 2.success, 3.fail)\n", ctrl->value);
	if((value == S5K4ECGX_AF_CHECK_2nd_STATUS) && (ctrl->value == S5K4ECGX_AF_STATUS_SUCCESS) && (need_flash == TRUE)){
		if(s5k4ecgx_Movie_flash_state != MOVIE_FLASH){ /* Disable to set pre-flash state in camcorder mode */
			Cam_Printk(KERN_NOTICE "Pre-Flash mode is end...\n");  
			/**++ Pre-Flash Process End ++**/
			s5k4ecgx_write_regs(client,reg_s5k4ecgx_FAST_AE_Off,ARRAY_SIZE(reg_s5k4ecgx_FAST_AE_Off),"reg_s5k4ecgx_FAST_AE_Off");	
			s5k4ecgx_write_regs(client,reg_s5k4ecgx_Pre_Flash_Off,ARRAY_SIZE(reg_s5k4ecgx_Pre_Flash_Off),"reg_s5k4ecgx_Pre_Flash_Off");
			//For prevent goggle apk flash turn off in torch mode
			if(sensor->flash_mode != S5K4ECGX_FLASH_TORCH)
				s5k4ecgx_set_flash(client, FLASH_OFF);
			sensor->focus_type = AUTO_FOCUS_MODE;
			s5k4ecgx_AE_lock(client, AE_UNLOCK);
			s5k4ecgx_AWB_lock(client, AWB_UNLOCK);

			/**-- Pre-Flash Process End --**/
		}
	}
	return 0;
  
}
/* 640x480 Window Size */
#define INNER_WINDOW_WIDTH              143
#define INNER_WINDOW_HEIGHT             143
#define OUTER_WINDOW_WIDTH              320
#define OUTER_WINDOW_HEIGHT             266
#define HALF_SHUTTER_TOUCH_AF		0
#define NORMAL_TOUCH_AF	1
static int s5k4ecgx_set_focus_touch_position(struct i2c_client *client, s32 value)
{
	int err, i=0;
	u16 read_value = 0;
	u16 touch_x, touch_y;
	u16 outter_x, outter_y;
	u16 inner_x, inner_y;
	u32 width, height;
	u16 outter_window_width, outter_window_height;
	u16 inner_window_width, inner_window_height;
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	Cam_Printk(KERN_NOTICE "value : %d\n", value);
	if(value==HALF_SHUTTER_TOUCH_AF){
		Cam_Printk(KERN_NOTICE "[DHL] The Half Shutter Touch Mode On..\n");
		gTouch_shutter_value=TRUE;
	}
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
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for touch_auto_focus\n", __func__);
		return -EIO;
	}
		return 0;
}
void s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(struct i2c_client *client,int bit, int set) 
{
	int REG_TC_DBG_AutoAlgEnBits = 0; 
	/* Read 04E6 */
	s5k4ecgx_write(client, 0x002C, 0x7000);
	s5k4ecgx_write(client, 0x002E, 0x04E6);
	s5k4ecgx_read(client, 0x0F12, (unsigned short*)&REG_TC_DBG_AutoAlgEnBits);
	if(bit == 3 && set == true) {
		if(REG_TC_DBG_AutoAlgEnBits & 0x8) return;
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
		if(REG_TC_DBG_AutoAlgEnBits & 0x20) return;
		msleep(100);
		REG_TC_DBG_AutoAlgEnBits = REG_TC_DBG_AutoAlgEnBits | 0x20;
		s5k4ecgx_write(client, 0x0028, 0x7000);
		s5k4ecgx_write(client, 0x002A, 0x04E6);
		s5k4ecgx_write(client, 0x0F12, REG_TC_DBG_AutoAlgEnBits);
	} else if(bit == 5 && set == false) {
		if(REG_TC_DBG_AutoAlgEnBits & 0x20)return;
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
static struct v4l2_subdev_core_ops s5k4ecgx_core_ops = {
	//.g_ctrl			= s5k4ecgx_g_ctrl,
	.s_ctrl			= s5k4ecgx_s_ctrl,
	.init				= s5k4ecgx_init,
	.g_register		= s5k4ecgx_g_register,
	.s_register		= s5k4ecgx_s_register,
};
static struct v4l2_subdev_video_ops s5k4ecgx_video_ops = {
	.s_stream		= s5k4ecgx_s_stream,
	.s_mbus_fmt		= s5k4ecgx_s_fmt,
	.try_mbus_fmt		= s5k4ecgx_try_fmt,
	.enum_mbus_fmt		= s5k4ecgx_enum_fmt,
	.enum_mbus_fsizes	= s5k4ecgx_enum_fsizes,
	.enum_framesizes	= s5k4ecgx_enum_fsizes,
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
char gVendor_ID[12] = {0};
ssize_t rear_camera_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camera_type: SOC\n");
	return sprintf(buf, "SLSI_S5K4ECGX\n");

}

ssize_t rear_camerafw_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camera_type: S5K4ECGX N\n");
	return sprintf(buf, "S5K4ECGX N\n");
}

ssize_t rear_camera_full(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camera_type: S5K4ECGX N N\n");
	return sprintf(buf, "S5K4ECGX N N\n");
}

ssize_t rear_camera_vendorid(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camera_vendorid\n");
	if(VENDOR_ID)
		return sprintf(buf, "0x%04X\n", VENDOR_ID);
	else
		return sprintf(buf, "NULL\n");
}
char *cam_checkfw_factory = "OK";
char *cam_checkfw_user = "OK";
ssize_t rear_camera_checkfw_factory(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camera_checkfw_factory : %s\n",cam_checkfw_factory);

	return sprintf(buf, "%s\n", cam_checkfw_factory);
}

ssize_t rear_camera_checkfw_user(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camera_checkfw_user : %s\n",cam_checkfw_user);

	return sprintf(buf, "%s\n", cam_checkfw_user);
}

char *rear_camera_info_val = "ISP=SOC;CALMEM=N;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;";

ssize_t rear_camera_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camera_info_val : %s\n", rear_camera_info_val);

	return sprintf(buf, "%s\n", rear_camera_info_val);
}


int camera_antibanding_val = 2; /*default : CAM_BANDFILTER_50HZ = 2*/
int camera_antibanding_get (void) {
	return camera_antibanding_val;
}
ssize_t camera_antibanding_show (struct device *dev, struct device_attribute *attr, char *buf) {
	int count;
	count = sprintf(buf, "%d", camera_antibanding_val);
	printk("%s : antibanding is %d \n", __func__, camera_antibanding_val);
	return count;
}
ssize_t camera_antibanding_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
	int tmp = 0;
	sscanf(buf, "%d", &tmp);
	if ((2 == tmp) || (3 == tmp)) { /* CAM_BANDFILTER_50HZ = 2, CAM_BANDFILTER_60HZ = 3*/
		camera_antibanding_val = tmp;
		printk("%s : antibanding is %d\n",__func__, camera_antibanding_val);
	}
	return size;
}

static struct device_attribute camera_antibanding_attr = {
	.attr = {
		.name = "Cam_antibanding",
		.mode = (S_IRUSR|S_IRGRP | S_IWUSR|S_IWGRP)},
	.show = camera_antibanding_show,
	.store = camera_antibanding_store
};

static DEVICE_ATTR(rear_vendorid, S_IRUGO | S_IXOTH,  rear_camera_vendorid, NULL);
static DEVICE_ATTR(rear_camtype, S_IRUGO | S_IXOTH,  rear_camera_type, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO | S_IXOTH,  rear_camerafw_type, NULL);
static DEVICE_ATTR(rear_camfw_full, S_IRUGO | S_IXOTH,  rear_camera_full, NULL);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO | S_IXOTH,  rear_camera_checkfw_factory, NULL);
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO | S_IXOTH,  rear_camera_checkfw_user, NULL);
static DEVICE_ATTR(rear_caminfo, S_IRUGO | S_IXOTH,  rear_camera_info, NULL);

static int s5k4ecgx_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct s5k4ecgx_info *priv;
	int ret;
	static int sysfs_init = 0;

	printk(KERN_INFO "------------s5k4ecgx_probe--------------\n");
	priv = kzalloc(sizeof(struct s5k4ecgx_info), GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "Failed to allocate private data!\n");
		return -ENOMEM;
	}
	v4l2_i2c_subdev_init(&priv->subdev, client, &s5k4ecgx_subdev_ops);
	
	ret = s5k4ecgx_video_probe(client);
	if (ret < 0) 
		kfree(priv);
#ifdef ENABLE_WQ_I2C_S5K4EC
	s5k4ec_wq = create_workqueue("s5k4ec_queue"); 
	if (s5k4ec_wq) {	 
		s5k4ec_work = (s5k4ec_work_t *)kmalloc(sizeof(s5k4ec_work_t), GFP_KERNEL);	 
		if (s5k4ec_work) { 	
			INIT_WORK( (struct work_struct *)s5k4ec_work, s5k4ec_wq_function );	   
		}	 
	} 
#endif


	printk(KERN_INFO "------------s5k4ecgx_probe---return --ret = %d---------\n", ret);
	/*++ added for camera sysfs++*/
	if(sysfs_init == 0){
		sysfs_init = 1;
		if(camera_class == NULL)
			camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class))
			printk("Failed to create camera_class!\n");

		if(cam_dev_rear == NULL)
			cam_dev_rear = device_create(camera_class, NULL, 0, "%s", "rear");
		if (IS_ERR(cam_dev_rear))
			printk("Failed to create deivce (cam_dev_rear)\n");

		if (device_create_file(cam_dev_rear, &dev_attr_rear_camtype) < 0)
			printk("Failed to create device file(%s)!\n", dev_attr_rear_camtype.attr.name);
		if (device_create_file(cam_dev_rear, &dev_attr_rear_camfw) < 0)
			printk("Failed to create device file(%s)!\n", dev_attr_rear_camfw.attr.name);
		if (device_create_file(cam_dev_rear, &dev_attr_rear_camfw_full) < 0)
			printk("Failed to create device file(%s)!\n", dev_attr_rear_camfw_full.attr.name);
		if (device_create_file(cam_dev_rear, &dev_attr_rear_vendorid) < 0)
			printk("Failed to create device file(%s)!\n", dev_attr_rear_vendorid.attr.name);
		if (device_create_file(cam_dev_rear, &dev_attr_rear_checkfw_factory) < 0)
			printk("Failed to create device file(%s)!\n", dev_attr_rear_checkfw_factory.attr.name);
		if (device_create_file(cam_dev_rear, &dev_attr_rear_checkfw_user) < 0)
			printk("Failed to create device file(%s)!\n", dev_attr_rear_checkfw_user.attr.name);
		if (device_create_file(cam_dev_rear, &camera_antibanding_attr) < 0) 
			printk("Failed to create device file: %s\n", camera_antibanding_attr.attr.name);
		if (device_create_file(cam_dev_rear, &dev_attr_rear_caminfo) < 0)
			printk("Failed to create device file(%s)!\n", dev_attr_rear_caminfo.attr.name);

		/*-- camera sysfs --*/
	}	
	return ret;
}
static int s5k4ecgx_remove(struct i2c_client *client)
{
#ifdef ENABLE_WQ_I2C_S5K4EC
		kfree(s5k4ec_work);
#endif

	return 0;
}
static struct i2c_driver s5k4ecgx_driver = {
	.driver = {
		.name	= "s5k4ecgx",
	},
	.id_table       = s5k4ecgx_idtable,	
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
}
static void __exit s5k4ecgx_mod_exit(void)
{
	i2c_del_driver(&s5k4ecgx_driver);
}
module_init(s5k4ecgx_mod_init);
module_exit(s5k4ecgx_mod_exit);
