/*****************************************************************************
 *
 * Filename:
 * ---------
 *   BF3905mipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description: 
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
/* #include <linux/xlog.h> */

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "bf3905mipiyuv_Sensor.h"
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define PFX "BF3905_camera_sensor"
#define LOG_1 LOG_INF("BF3905,MIPI 1LANE\n")
#define LOG_2 LOG_INF("preview 640*480@30fps, 1 lane; video 640*480@30,1lane; capture 0.3M@30fps,1 lane\n")

/****************************   Modify end    *******************************************/
#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static BOOL set_awb_mode(UINT16 para);
static void  set_scene_mode(UINT16 para);
static void af_Init(void);


static imgsensor_info_struct imgsensor_info = { 
	.sensor_id = BF3905_SENSOR_ID,
	.checksum_value = 0xdaea5f51,
	.pre = {
		.pclk = 24000000,				//record different mode's pclk
		.linelength =784,				//record different mode's linelength
		.framelength = 510, 			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 640,		//record different mode's width of grabwindow
		.grabwindow_height = 480,		//record different mode's height of grabwindow

		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
	.cap = {
		.pclk = 24000000,
		.linelength =784,
		.framelength = 510,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 12000000,
		.linelength =784,
		.framelength = 510,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 150,
	},
	.normal_video = {
		.pclk = 24000000,
		.linelength =784,
		.framelength = 510,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 24000000,
		.linelength =784,
		.framelength = 510,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 24000000,
		.linelength =784,
		.framelength = 510,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
	},
	.margin = 4,
	.min_shutter = 1,
	.max_frame_length = 0xffff,
	.awb_delay_frame = 3,
	.effect_delay_frame = 2,
	.ae_delay_frame = 1,
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num
	
	.cap_delay_frame = 3, 
	.pre_delay_frame = 3,  
	.video_delay_frame = 4,
	.hs_video_delay_frame = 4,
	.slim_video_delay_frame = 4,

	.InitDelay= 3,          //init setting delay frame
	.EffectDelay= 0,         //EffectDelay setting delay frame
	.AwbDelay= 0,           //AwbDelay setting delay frame
	.AFSwitchDelayFrame=50,  //touch af back to continue af  need frame num ,its value awlas 50
	.EvDelay = 1,            
	.SatDelay = 0,
	.BrightDelay = 0,
	.ContrastDelay= 0,
	
	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_YUYV,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_1_LANE,
	.i2c_addr_table = {0xdc,0x6e,0xff},
	.i2c_speed = 300, // i2c read/write speed

	.minframerate=0,
	.maxframerate=30,
	.flash_bv_threshold=0x25,
};


static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x100,					//current shutter
	.gain = 0x3F,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 0,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.iso_speed=AE_ISO_100,
	.FNumber = 28,
	.awb_mode=AWB_MODE_AUTO,
	.scene_mode=SCENE_MODE_OFF,
	.awbaelock=KAL_FALSE,
	.ihdr_en = KAL_FALSE, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0xdc,
	.af_coordinate_y=0x00,//touch af window
	.af_coordinate_y=0x00,
};


//=====================For touch ae==========================//
#define AE_WIN_MUN_H (4)
#define AE_WIN_MUN_W (4)
//static UINT32 ae_win_w[AE_WIN_MUN_W] = {FALSE};
//static UINT32 ae_win_h[AE_WIN_MUN_H] = {FALSE};
//static BOOL ae_pline_table[AE_WIN_MUN_W][AE_WIN_MUN_H] = {{FALSE},{FALSE},{FALSE},{FALSE}};//how to ....

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
	char puSendCmd[1] = {(char)(addr & 0xFF) };
	kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
	iReadRegI2C(puSendCmd , 1, (u8*)&get_byte,1,imgsensor.i2c_write_id);
	LOG_INF("addr (%x),  value(%x)\n",  addr, get_byte);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char puSendCmd[2] = {(char)(addr & 0xFF) ,((char)(para & 0xFF))};
	kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
	iWriteRegI2C(puSendCmd , 2,imgsensor.i2c_write_id);
	LOG_INF("addr (%x),  value(%x)\n",  addr, para);

}

static void get_exifInfo(UINT32  *exifAddr)
{   
	SENSOR_EXIF_INFO_STRUCT   *pExifInfo = (SENSOR_EXIF_INFO_STRUCT*)exifAddr;

	LOG_INF("E\n");
	pExifInfo->FNumber = imgsensor.FNumber;
	pExifInfo->AEISOSpeed = imgsensor.iso_speed;
	pExifInfo->AWBMode = imgsensor.awb_mode;
	pExifInfo->CapExposureTime = 0;
	pExifInfo->FlashLightTimeus = 0;
	pExifInfo->RealISOValue = imgsensor.iso_speed;
}

static void set_dummy(kal_uint16 dummy_pixels, kal_uint16 dummy_lines)
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", dummy_lines, dummy_pixels);
	
	write_cmos_sensor(0x92, dummy_lines&0xff); //Dummy line insert after active line low 8 Bits
	write_cmos_sensor(0x93, (dummy_lines>>8)&0xff); 	// 
	write_cmos_sensor(0x2b, (dummy_pixels)&0xff); //Dummy Pixel Insert LSB
	write_cmos_sensor(0x2a, (((dummy_pixels)>>8)<<4)|((read_cmos_sensor(0x2a))&0x0f)); 	//Dummy Pixel Insert MSB

}    /*    set_dummy  */

#ifdef NOT_USED
static void write_shutter(kal_uint32 shutter)
{
#if 0
	kal_uint16 realtime_fps = 0;
         kal_uint32 frame_length = 0;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.shutter = shutter;
	 spin_unlock(&imgsensor_drv_lock);
	spin_unlock(&imgsensor_drv_lock);
	
spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)       
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
	
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	write_cmos_sensor(0x380F,(imgsensor.frame_length&0xFF));	   
	write_cmos_sensor(0x380E,((imgsensor.frame_length&0xFF00)>>8));

	shutter*=16; 
	write_cmos_sensor(0x3502, (shutter & 0x00FF));           //AEC[7:0]
	write_cmos_sensor(0x3501, ((shutter & 0x0FF00) >>8));  //AEC[15:8]
	write_cmos_sensor(0x3500, ((shutter & 0xFF0000) >> 16));	
#endif	
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
} 
#endif
/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
#ifdef NOT_USED
static void set_shutter(kal_uint16 shutter)
{
	LOG_INF("set_shutter shutter:%d\n",shutter);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.shutter = shutter;
	spin_unlock(&imgsensor_drv_lock);
	write_shutter(shutter);
}	/*	set_shutter */
#endif

static kal_uint32 get_shutter(void)
{
	unsigned int shutter_value;
	kal_uint8 temp_msb=0x00,temp_lsb=0x00;

	temp_msb=read_cmos_sensor(0x8c);
	temp_msb = (temp_msb<<8)&0xff00;

	temp_lsb=read_cmos_sensor(0x8d);
	temp_lsb = temp_lsb&0x00ff;

	shutter_value = temp_msb|temp_lsb;

 	spin_lock(&imgsensor_drv_lock);
	imgsensor.shutter = shutter_value;
	spin_lock(&imgsensor_drv_lock);
	LOG_INF("Exit! shutter =%d\n", shutter_value);
	return shutter_value;
}
#ifdef NOT_USED
static kal_uint16 set_gain(kal_uint16 gain)
{
	LOG_INF("E\n");

	return 0;
} 
#endif
static kal_uint32 get_gain(void)
{
	unsigned int interval = 0;
	interval = read_cmos_sensor(0x87); //0x10 means 1X gain.
	interval = interval*4;

	LOG_INF("Exit! gain =%d\n", interval);
	return interval; 
}

static kal_uint16 get_awb_rgain(void)
{
	kal_uint16 temp=0x0000;

	LOG_INF("E\n");

	temp=read_cmos_sensor(0x02);
	temp = temp*4;
	return temp;
}
static kal_uint16 get_awb_bgain(void)
{
	kal_uint16 temp=0x0000;

	LOG_INF("E\n");

	temp=read_cmos_sensor(0x01);
	temp = temp*4;
	return temp;
}

static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 mirror= 0;
	LOG_INF("image_mirror = %d", image_mirror);
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.mirror= image_mirror; 
	spin_unlock(&imgsensor_drv_lock);
	
	mirror= read_cmos_sensor(0x1e);	

	switch (image_mirror)
		{
		case IMAGE_NORMAL:
			write_cmos_sensor(0x1e, mirror & 0xCF); // 0x1e[4] vertical flip, 0:normal, 1:vertical  //0x1e[5] mirror, 0:normal, 1:mirror				                                                    //0x1e[5] mirror, 0:normal, 1:mirror
			break;
		case IMAGE_H_MIRROR:
			write_cmos_sensor(0x1e, mirror | 0x20); 
			break;
		case IMAGE_V_MIRROR: 
			write_cmos_sensor(0x1e, mirror | 0x10); 
			break;		
		case IMAGE_HV_MIRROR:
			write_cmos_sensor(0x1e, mirror | 0x30); 
			break;		
		default:
			LOG_INF("Error image_mirror setting\n");
		}
	
	
}
static void set_ae_lock(void)
{
	kal_uint8 temp_r = 0;	

	LOG_INF("set_ae_lock\n");
	temp_r =  read_cmos_sensor(0x13);	
	write_cmos_sensor(0x13, temp_r&0xfa);//0x13[0]=0, disable agc,aec 1010
}
static void set_ae_unlock(void)
{
	kal_uint8 temp_r = 0;	

	LOG_INF("set_ae_unlock\n");
	temp_r =  read_cmos_sensor(0x13);
	write_cmos_sensor(0x13,temp_r|0x05);//0x13[0]=1, enable agc,aec 0101
}
static void set_awb_lock(void)
{
	kal_uint8 temp_r = 0;	

	LOG_INF("set_awb_lock\n");
	temp_r =  read_cmos_sensor(0x13);	
	write_cmos_sensor(0x13, temp_r&0xf9); //bit[1]:AWB  disable agc,awb 1001

}
static void set_awb_unlock(void)
{
	kal_uint8 temp_r = 0;

	LOG_INF("set_awb_unlock\n");
	temp_r =  read_cmos_sensor(0x13);	 
	write_cmos_sensor(0x13, temp_r|0x06); //bit[1]:AWB enable agc,awb 0110
}


void set_3actrl_info(ACDK_SENSOR_3A_LOCK_ENUM action)
{  //added by bydzzg 20150227
	LOG_INF("set_3actrl_info\n");
	LOG_INF("E action =%d\n",action); 
	switch(action)
	{
		case SENSOR_3A_AE_LOCK:
			set_ae_lock();
			break;
		case SENSOR_3A_AE_UNLOCK:
			set_ae_unlock();
			break;
		case SENSOR_3A_AWB_LOCK:
		     set_awb_lock();
			break;
		case SENSOR_3A_AWB_UNLOCK:
		     set_awb_unlock();
#if 1
			if(!((SCENE_MODE_OFF==imgsensor.scene_mode)||(SCENE_MODE_NORMAL==imgsensor.scene_mode)||(SCENE_MODE_HDR==imgsensor.scene_mode)))
			{
				set_scene_mode(imgsensor.scene_mode);
				LOG_INF("E imgsensor.scene_mode=%d\n",imgsensor.scene_mode);
			}

			if(!((AWB_MODE_OFF==imgsensor.awb_mode)||(AWB_MODE_AUTO==imgsensor.awb_mode)))
			{

				LOG_INF("E imgsensor.awb_mode=%d\n",imgsensor.awb_mode);
				set_awb_mode(imgsensor.awb_mode);
			}
#endif
		break;
		default:
		break;
		}
}

static BOOL set_awb_mode(UINT16 para)
{
    kal_uint8 temp_r=0;

    LOG_INF("E imgsensor.awb_mode =%d\n",para);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.awb_mode = para;
    spin_unlock(&imgsensor_drv_lock);
	
    temp_r = read_cmos_sensor(0x13);
    	
    switch (para)
		{
		case AWB_MODE_AUTO:
			write_cmos_sensor(0x13, temp_r|0x02); //bit[1]:AWB Auto:1 menual:0
			write_cmos_sensor(0x01, 0x15);
			write_cmos_sensor(0x02, 0x23);
			break;

		case AWB_MODE_CLOUDY_DAYLIGHT:
			//======================================================================
			//	MWB : Cloudy_D65
			//======================================================================
			write_cmos_sensor(0x13, temp_r&0xfd); //bit[1]:AWB Auto:1 menual:0
			write_cmos_sensor(0x01, 0x10);
			write_cmos_sensor(0x02, 0x28);
			break;

		case AWB_MODE_DAYLIGHT:
			//==============================================
			//	MWB : sun&daylight_D50
			//==============================================
			write_cmos_sensor(0x13, temp_r&0xfd); //bit[1]:AWB Auto:1 menual:0
			write_cmos_sensor(0x01, 0x11);
			write_cmos_sensor(0x02, 0x26);
			break;

		case AWB_MODE_INCANDESCENT:
			//==============================================
			//	MWB : Incand_Tungsten
			//==============================================
			write_cmos_sensor(0x13, temp_r&0xfd); //bit[1]:AWB Auto:1 menual:0
			write_cmos_sensor(0x01, 0x1f);
			write_cmos_sensor(0x02, 0x15);
			break;

		case AWB_MODE_FLUORESCENT:
			//==================================================================
			//	MWB : Florescent_TL84
			//==================================================================
			write_cmos_sensor(0x13, temp_r&0xfd); //bit[1]:AWB Auto:1 menual:0
			write_cmos_sensor(0x01, 0x1a);
			write_cmos_sensor(0x02, 0x1e);
			break;
		case AWB_MODE_TUNGSTEN:
			write_cmos_sensor(0x13, temp_r&0xfd); //bit[1]:AWB Auto:1 menual:0
			write_cmos_sensor(0x01, 0x1a);
			write_cmos_sensor(0x02, 0x0d);
			break;
		default:
			return KAL_FALSE;
	}
    
    return TRUE;
} 

static void set_contrast(UINT16 para)
{	
	LOG_INF("E set_contrast =%d\n",para);
	switch (para)
	{
		case ISP_CONTRAST_LOW:
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x56, 0x15); //low
			write_cmos_sensor(0x94, 0x92);		
			write_cmos_sensor(0x0b, 0x02);//skip frame
			break;
		case ISP_CONTRAST_MIDDLE:
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x56, 0x40);	//middle
			write_cmos_sensor(0x94, 0x92);				
			write_cmos_sensor(0x0b, 0x02);//skip frame
			break;
		case ISP_CONTRAST_HIGH:
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x56, 0x50); //high
			write_cmos_sensor(0x94, 0xf2);			
			write_cmos_sensor(0x0b, 0x02);//skip frame
			break;
		default:
			LOG_INF(" default case\n");
			
	}
	return;
}

static void set_brightness(UINT16 para)
{
	LOG_INF("E set_contrast =%d\n",para);
	switch (para)
	{
		case ISP_BRIGHT_LOW:
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x24, 0x30);
			write_cmos_sensor(0x97, 0x30);
			write_cmos_sensor(0x94, 0x92);			
			write_cmos_sensor(0x86, 0x60);			
			 break;
		case ISP_BRIGHT_HIGH:
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x24, 0x68);
			write_cmos_sensor(0x97, 0x68);
			write_cmos_sensor(0x94, 0xf2);				
			write_cmos_sensor(0x86, 0xa0);						
			 break;
		case ISP_BRIGHT_MIDDLE:
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x24, 0x48);
			write_cmos_sensor(0x97, 0x48);
			write_cmos_sensor(0x94, 0x92);			
			write_cmos_sensor(0x86, 0x77);							
			 break;
		default:
			 return;
			 break;
	}
	return;
}

static void set_saturation(UINT16 para)
{
	LOG_INF("E set_saturation =%d\n",para);
	switch (para)
	{
		case ISP_SAT_LOW: 
		write_cmos_sensor(0xb4,0xe3);
		write_cmos_sensor(0xb1,0x33);
		write_cmos_sensor(0xb2,0x33);  
		write_cmos_sensor(0xb4,0x63);
		write_cmos_sensor(0xb1,0x33); 
		write_cmos_sensor(0xb2,0x33); 
		break;
		case ISP_SAT_MIDDLE: 	
		write_cmos_sensor(0xb4,0xe3);
		write_cmos_sensor(0xb1,0xef);
		write_cmos_sensor(0xb2,0xc0); 
		write_cmos_sensor(0xb4,0x63);
		write_cmos_sensor(0xb1,0xb5); 
		write_cmos_sensor(0xb2,0x9c); 
		break;
		case ISP_SAT_HIGH: 	
		write_cmos_sensor(0xb4,0xe3);
		write_cmos_sensor(0xb1,0xff);
		write_cmos_sensor(0xb2,0xff);  //ef  //ryx
		write_cmos_sensor(0xb4,0x63);
		write_cmos_sensor(0xb1,0xff);//ba yzx 1.6
		write_cmos_sensor(0xb2,0xff);//aa  yzx 1.6 //a0
		default:
			return;
	}
	 mDELAY(50);
	 return;
}

void set_free_framerate(UINT16 u2FrameRate) 
{ 
	kal_uint16 fr_time = 0, temp_r = 0;

	LOG_INF("Jeff: set_framerate %d\n",u2FrameRate);
	LOG_INF("Jeff: imgsensor.pclk (%d)\n",imgsensor.pclk);
	LOG_INF("Jeff: imgsensor.line_length(%d)\n",imgsensor.line_length);
	LOG_INF("Jeff: imgsensor.frame_length(%d)\n",imgsensor.frame_length);
	
        set_dummy(0x00,0x00);
	temp_r = read_cmos_sensor(0x89);
	
	if(gBanding == AE_FLICKER_MODE_50HZ)
	{
		fr_time=100/u2FrameRate;
		write_cmos_sensor(0x89,((temp_r&0x07)| (fr_time<<3)));//setMinFramerate
	}
	else if(gBanding ==AE_FLICKER_MODE_60HZ)
	{
		fr_time=120/u2FrameRate;
		write_cmos_sensor(0x89,((temp_r&0x07)| (fr_time<<3)));//setMinFramerate
	}
	else
	{
		fr_time=100/u2FrameRate;
		write_cmos_sensor(0x89,((temp_r&0x07)| (fr_time<<3)));//setMinFramerate
	}

}

void set_fixed_framerate(UINT16 u2FrameRate)
{
	kal_uint16 fr_time = 0, temp_r = 0;
         kal_uint32 dummylines=0;

	LOG_INF("Jeff: set_framerate %d\n",u2FrameRate);
	LOG_INF("Jeff: imgsensor.pclk (%d)\n",imgsensor.pclk);
	LOG_INF("Jeff: imgsensor.line_length(%d)\n",imgsensor.line_length);
	LOG_INF("Jeff: imgsensor.frame_length(%d)\n",imgsensor.frame_length);
	
	/*ע��fps�Ƿ�Ҫ����10base*/
	dummylines =  imgsensor.pclk/2/u2FrameRate/imgsensor.line_length - imgsensor.frame_length;

	LOG_INF("Jeff: set_framerate-- dummylines(%d)\n", dummylines);
	set_dummy(0x00,dummylines); 

	/**REG0X89,  become larger in order to adjust the picture more fast.*/
	temp_r = read_cmos_sensor(0x89);
	
	if(gBanding == AE_FLICKER_MODE_50HZ)
	{
		fr_time=100/u2FrameRate;
		write_cmos_sensor(0x89,((temp_r&0x07)| (fr_time<<3)));//setMinFramerate
	}
	else if(gBanding ==AE_FLICKER_MODE_60HZ)
	{
		fr_time=120/u2FrameRate;
		write_cmos_sensor(0x89,((temp_r&0x07)| (fr_time<<3)));//setMinFramerate
	}
	else
	{
		fr_time=100/u2FrameRate;
		write_cmos_sensor(0x89,((temp_r&0x07)| (fr_time<<3)));//setMinFramerate
	}

}

static UINT32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate)
{
	LOG_INF("E frameRate=%d\n", frameRate);

	if(0 == frameRate ) //dynamic framerate
	{
		return ERROR_NONE;
	}
	if( 5 <= frameRate && frameRate <= 30 )
	{
		set_fixed_framerate(frameRate);
	}
	else
	{
		LOG_INF("Wrong Frame Rate \n");
	}
	return TRUE; 
}
static void set_scenemode_portrait(void)
{
}

static void set_scenemode_landscape(void)
{
}

static void set_scenemode_sunset(void)
{
}
static void set_scenemode_sports(void)
{
}
static void preview_setting(void)
{
	LOG_INF("E! ");
}
#ifdef NOT_USED
static void set_scenemode_night(void)
{
	LOG_INF("E\n");
}	
#endif

static void set_scenemode_off(void)
{
}

static UINT32 set_video_mode(UINT16 u2FrameRate)
{
	LOG_INF("Jeff: set_video_mode u2FrameRate(%d)\n",u2FrameRate);
			
	if (u2FrameRate == 30)
	{
		LOG_INF("Jeff: set_video_mode normal preview\n");
		set_fixed_framerate(30);	
	}
	else if (u2FrameRate == 15)       
	{
		LOG_INF("Jeff: set_video_mode for night video \n");
		set_fixed_framerate(15);	
	}
	else
	{

		LOG_INF("Wrong Frame Rate"); 

	}

	return TRUE;

}

static void set_night_mode(kal_bool enable)
{
	LOG_INF("enable(%d)\n", enable);
	
	if(enable) // night mode for preview and video
	{
			LOG_INF("set_night_mode for preview\n");
			set_free_framerate(5);	
	}
	else  // normal mode for preview and video (not night)
	{
			LOG_INF("set_normal_mode for preview\n");
			set_free_framerate(10);			
		      
	}
		
	spin_lock(&imgsensor_drv_lock);
	gNightMode = enable;
	spin_unlock(&imgsensor_drv_lock);
         LOG_INF("set_night_mode end \n");
}	/* set_night_mode */

static void  set_scene_mode(UINT16 para)
{
	LOG_INF("E imgsensor.scene_mode=%d\n",para);	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.scene_mode=para;
	spin_unlock(&imgsensor_drv_lock);
	switch (para)
	{ 
		case SCENE_MODE_NIGHTSCENE:
			set_night_mode(1);
			break;
		case SCENE_MODE_PORTRAIT:
			set_scenemode_portrait();		 
			 break;
		case SCENE_MODE_LANDSCAPE:
			set_scenemode_landscape();		 
			 break;
		case SCENE_MODE_SUNSET:
			set_scenemode_sunset();	 
			break;
		case SCENE_MODE_SPORTS:
			set_scenemode_sports(); 	 
			break;
		case SCENE_MODE_HDR:
			break;
		case SCENE_MODE_OFF:
			set_night_mode(0);
			break;
		default:
			set_scenemode_off();
			LOG_INF("this is default case of set_scene_mode\n");
			break;
	}
	return;
}
static void set_hue(UINT16 para)
{
	return;
}

static void set_iso(UINT16 para)
{
	kal_uint8 temp_r;

	LOG_INF("E ISO=%d\n",para);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.iso_speed = para;
	spin_unlock(&imgsensor_drv_lock);	 

	temp_r = read_cmos_sensor(0x13);

	switch (para)
	{
		case AE_ISO_AUTO:
			write_cmos_sensor(0x13,temp_r&0xBF);//0x13[6]=0
			write_cmos_sensor(0x9f, 0x61); //auto
			break;
		case AE_ISO_100:
			write_cmos_sensor(0x13,temp_r|0x50);//0x13[6]=1
			write_cmos_sensor(0x9f, 0x62); //iso 100
			break;
		case AE_ISO_200:
			write_cmos_sensor(0x13,temp_r|0x50);//0x13[6]=1
			write_cmos_sensor(0x9f, 0x63);	//iso 200
			break;
		case AE_ISO_400:
			write_cmos_sensor(0x13,temp_r|0x50);//0x13[6]=1
			write_cmos_sensor(0x9f, 0x64); //iso 400
			break;
		case AE_ISO_800:
			write_cmos_sensor(0x13,temp_r|0x50);//0x13[6]=1
			write_cmos_sensor(0x9f, 0x65); //iso 800
			break;
		case AE_ISO_1600:
			write_cmos_sensor(0x13,temp_r|0x50);//0x13[6]=1
			write_cmos_sensor(0x9f, 0x66); //iso 1600
			break;
		default:
			return  ;
	}
	return;
}


static void set_edge(UINT16 para)
{

	LOG_INF("set_param_edge func:para = %d\n",para);

	switch (para)
	{
		case ISP_EDGE_LOW:
			write_cmos_sensor(0x7a, 0x00); //low
			write_cmos_sensor(0x73, 0x00);
			write_cmos_sensor(0x0b, 0x02);//skip frame
			break;
		case ISP_EDGE_MIDDLE:
			write_cmos_sensor(0x7a, 0x24);	//middle
			write_cmos_sensor(0x73, 0x27);
			write_cmos_sensor(0x0b, 0x02);//skip frame
			break;
		case ISP_EDGE_HIGH:
			write_cmos_sensor(0x7a, 0x77); //high
			write_cmos_sensor(0x73, 0x27);
			write_cmos_sensor(0x0b, 0x02);//skip frame
			break;
		default:
			return ;
	}
	return ;

}

static BOOL set_param_effect(UINT16 para)
{
	LOG_INF("E effect=%d\n",para);
	
	switch (para)
	{
	   case MEFFECT_OFF:
			write_cmos_sensor(0xb4,0x63); // Normal, 
			write_cmos_sensor(0x69,0x00); // Normal, 
			write_cmos_sensor(0x67,0x80); // Normal, 
			write_cmos_sensor(0x68,0x80); // Normal, 
			write_cmos_sensor(0x70,0x0b); // Normal, 
			write_cmos_sensor(0x56,0x40);	//middle
			break;
	   case MEFFECT_MONO:
			write_cmos_sensor(0xb4,0x03); // Normal, 
			write_cmos_sensor(0x69,0x20); // Normal, 
			write_cmos_sensor(0x67,0x80); // Normal, 
			write_cmos_sensor(0x68,0x80); // Normal, 
			write_cmos_sensor(0x70,0x0b); // Normal, 
			write_cmos_sensor(0x56,0x40);	//middle
		   break;
		   
	   case MEFFECT_NEGATIVE: 
			write_cmos_sensor(0xb4,0x03); // Normal, 
			write_cmos_sensor(0x69,0x01); // Normal, 
			write_cmos_sensor(0x67,0x80); // Normal, 
			write_cmos_sensor(0x68,0x80); // Normal, 
			write_cmos_sensor(0x70,0x0b); // Normal, 
			write_cmos_sensor(0x56,0x40);	//middle
			break;
	   case MEFFECT_SEPIA:
			write_cmos_sensor(0xb4,0x03); // Normal, 
			write_cmos_sensor(0x69,0x20); // Normal, 
			write_cmos_sensor(0x67,0x60); // Normal, 
			write_cmos_sensor(0x68,0xa0); // Normal, 
			write_cmos_sensor(0x70,0x0b); // Normal, 
			write_cmos_sensor(0x56,0x40);	//middle
			break;

	   default:
			break;
   }
     
    return KAL_FALSE;
} 

static BOOL set_flicker_mode(UINT16 para)
{
	LOG_INF("E flicker=%d\n",para);
	
	 switch (para)	
	{
	case AE_FLICKER_MODE_50HZ:		
		LOG_INF("BF3905_MIPI_set_param_banding 50hz \n");
		write_cmos_sensor(0x8a,0x99);//int_step_50);//96 //ryx//24000000/2/(784+16)/100  zhuan  16 jinzhi
		write_cmos_sensor(0x80,read_cmos_sensor(0x80)|0x02); //0x80[1]=1, select 50hz banding filter	
		write_cmos_sensor(0x0b,0x05);					
		spin_lock(&imgsensor_drv_lock);
		gBanding = para;
		spin_unlock(&imgsensor_drv_lock);

		//set_night_mode(gNightMode);
		break;
	case AE_FLICKER_MODE_60HZ:		
		LOG_INF("BF3905_MIPI_set_param_banding 60hz \n");
		write_cmos_sensor(0x8b,0x7f);//int_step_60);//7d //ryx//24000000/2/(784+16)/120  zhuan  16 jinzhi
		write_cmos_sensor(0x80,read_cmos_sensor(0x80)&0xfd); //0x80[1]=0, select 60hz banding filter					
		write_cmos_sensor(0x0b,0x05);			
		spin_lock(&imgsensor_drv_lock);
		gBanding = para;
		spin_unlock(&imgsensor_drv_lock);
		//set_night_mode(gNightMode);
		break;
	default:
		LOG_INF("BF3905_MIPI_set_param_banding default,we choose 50hz instead\n");
		write_cmos_sensor(0x8a,0x99);//int_step_50);
		write_cmos_sensor(0x80,read_cmos_sensor(0x80)|0x02); //0x80[1]=1, select 50hz banding filter	
		write_cmos_sensor(0x0b,0x05);
		spin_lock(&imgsensor_drv_lock);
		gBanding = para;
		spin_unlock(&imgsensor_drv_lock);
		//set_night_mode(gNightMode);
		break;
	} 
	return KAL_FALSE;
}

static BOOL set_ev(UINT16 para)
{
	LOG_INF("E ev=%d\n",para);
	switch (para)
	{
		case AE_EV_COMP_n20:	
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x55, 0xa8); //EV -2
			write_cmos_sensor(0x0b, 0x04);//skip frame
			break;
		case AE_EV_COMP_n10:
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x55, 0x98); //EV -1
			write_cmos_sensor(0x0b, 0x04);//skip frame
			break;
		case AE_EV_COMP_00:
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x55, 0x00); //EV 0   
			write_cmos_sensor(0x0b, 0x04);//skip frame
			break;
		case AE_EV_COMP_10:	
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x55, 0x10);  //EV +1
			write_cmos_sensor(0x0b, 0x04);//skip frame
			break;			
		case AE_EV_COMP_20:
			write_cmos_sensor(0x0b, 0x04);//skip frame
			mDELAY(100);
			write_cmos_sensor(0x55, 0x20); //EV +2
			write_cmos_sensor(0x0b, 0x04);//skip frame
			break;	
		default:
			return FALSE;
	} 
    return TRUE;
} 

static void  set_AE_mode(kal_bool AE_enable)
{
	kal_uint8 temp_r = 0;
	LOG_INF("BF3905_MIPI_set_AE_mode %d \n",AE_enable);
	
	temp_r =  read_cmos_sensor(0x13);
	
	if(AE_enable==KAL_TRUE)
	{
		write_cmos_sensor(0x13,temp_r|0x01);//0x13[0]=1, enable
	}
	else
	{
		write_cmos_sensor(0x13,temp_r&0xfe);//0x13[0]=0, disable
	}
}

static UINT32 set_yuv_cmd(FEATURE_ID iCmd, UINT32 iPara)
{
	LOG_INF("E iCmd=%d,iPara=%d\n",iCmd,iPara);
	switch (iCmd)
    {
        case FID_SCENE_MODE:     //
            set_scene_mode(iPara);
            break;
        case FID_AWB_MODE:
            set_awb_mode(iPara);
            break;
        case FID_COLOR_EFFECT:
            set_param_effect(iPara);
            break;
        case FID_AE_EV:
            set_ev(iPara);
            break;
        case FID_AE_FLICKER:
            set_flicker_mode(iPara);
            break;
        case FID_ISP_CONTRAST:
            set_contrast(iPara);
            break;
        case  FID_ISP_BRIGHT:
            set_brightness(iPara);
            break;
        case FID_ISP_SAT:
            set_saturation(iPara);
            break;
        case FID_AE_ISO:
            set_iso(iPara);
            break;
	case FID_ISP_HUE:
		    set_hue(iPara);
		break;
	case FID_ISP_EDGE:
		set_edge(iPara);
		
	case FID_AE_SCENE_MODE:
		spin_lock(&imgsensor_drv_lock);
		if (iPara == AE_MODE_OFF) 
		{
			gAE_ENABLE = KAL_FALSE;
		}
		else 
		{
			gAE_ENABLE = KAL_TRUE;
		}
		spin_unlock(&imgsensor_drv_lock);
                  set_AE_mode(gAE_ENABLE);
		break;
        default:
            break;
    }
    return TRUE;
}

static void sensor_init(void)
{

	LOG_INF("sensor_init enter\n");
	write_cmos_sensor(0x12,0x80);
	write_cmos_sensor(0x20,0x09);///09   //2014.1.25     ///49
	write_cmos_sensor(0x09,0x00);///00 //6f//60//6c  //2014.1.25   
	write_cmos_sensor(0x12,0x00);
	write_cmos_sensor(0x3a,0x00);//0x20:uyvy,0x00:yuyv
	write_cmos_sensor(0x17,0x00);
	write_cmos_sensor(0x18,0xa0);
	write_cmos_sensor(0x19,0x00);
	write_cmos_sensor(0x1a,0x78);
	write_cmos_sensor(0x03,0x00);
	write_cmos_sensor(0x5d,0xb3);
	write_cmos_sensor(0xbf,0x08);
	write_cmos_sensor(0xc3,0x08);
	write_cmos_sensor(0xca,0x10);
	write_cmos_sensor(0x15,0x12);
	write_cmos_sensor(0x62,0x00);
	write_cmos_sensor(0x63,0x00);

	write_cmos_sensor(0x1e,0x70);////40/ryx   ///70  yzx 1.6
	write_cmos_sensor(0x2a,0x00); //dummy pixel
	write_cmos_sensor(0x2b,0x00); //dummy pixel
	write_cmos_sensor(0x92,0x1f); //dummy line
	write_cmos_sensor(0x93,0x00); //dummy line
	write_cmos_sensor(0xd9,0x25);
	write_cmos_sensor(0xdf,0x25);
	write_cmos_sensor(0x4a,0x0c);
	write_cmos_sensor(0xda,0x00);
	write_cmos_sensor(0xdb,0xa2);
	write_cmos_sensor(0xdc,0x00);
	write_cmos_sensor(0xdd,0x7a);
	write_cmos_sensor(0xde,0x00);
	write_cmos_sensor(0x1b,0x0e);///  0e///ryx  1 bei pin
	write_cmos_sensor(0x60,0xe5);
	write_cmos_sensor(0x61,0xf2);
	write_cmos_sensor(0x6d,0xc0);
	write_cmos_sensor(0xb9,0x00);
	write_cmos_sensor(0x64,0x00);
	write_cmos_sensor(0xbb,0x10);
	write_cmos_sensor(0x08,0x02);
	write_cmos_sensor(0x21,0x4f);
	write_cmos_sensor(0x3e,0x83);
	write_cmos_sensor(0x16,0xa1);
	write_cmos_sensor(0x2f,0xc4);
	write_cmos_sensor(0x13,0x00);
	write_cmos_sensor(0x01,0x15);
	write_cmos_sensor(0x02,0x23);
	write_cmos_sensor(0x9d,0x20);
	write_cmos_sensor(0x8c,0x03);
	write_cmos_sensor(0x8d,0x11);
	write_cmos_sensor(0x33,0x10);
	write_cmos_sensor(0x34,0x1d);
	write_cmos_sensor(0x36,0x45);
	write_cmos_sensor(0x6e,0x20);
	write_cmos_sensor(0xbc,0x0d);
	write_cmos_sensor(0x35,0x30);
	write_cmos_sensor(0x65,0x2a);
	write_cmos_sensor(0x66,0x2a);
	write_cmos_sensor(0xbd,0xf4);
	write_cmos_sensor(0xbe,0x44);
	write_cmos_sensor(0x9b,0xf4);
	write_cmos_sensor(0x9c,0x44);
	write_cmos_sensor(0x37,0xf4);
	write_cmos_sensor(0x38,0x44);
	write_cmos_sensor(0x71,0x0f);
	write_cmos_sensor(0x72,0x4c);
	write_cmos_sensor(0x73,0x27);//a1 //ryx  2014.08.22
	write_cmos_sensor(0x75,0x8a);
	write_cmos_sensor(0x76,0x98);
	write_cmos_sensor(0x77,0x5a);
	write_cmos_sensor(0x78,0xff);
	write_cmos_sensor(0x79,0x24);///64 //ryx  2014.08.22
	write_cmos_sensor(0x7a,0x24);///12 //ryx  2014.08.22
	write_cmos_sensor(0x7b,0x58);
	write_cmos_sensor(0x7c,0x55);
	write_cmos_sensor(0x7d,0x00);
	write_cmos_sensor(0x7e,0x84);
	write_cmos_sensor(0x7f,0x3c);
	write_cmos_sensor(0x13,0x07);
	write_cmos_sensor(0x24,0x48);///4f
	write_cmos_sensor(0x25,0x88);
	write_cmos_sensor(0x80,0x92);
	write_cmos_sensor(0x81,0x00);
	write_cmos_sensor(0x82,0x2a);
	write_cmos_sensor(0x83,0x54);
	write_cmos_sensor(0x84,0x39);
	write_cmos_sensor(0x85,0x5d);
	write_cmos_sensor(0x86,0x77);
	write_cmos_sensor(0x89,0x53);
	write_cmos_sensor(0x8a,0x99);
	write_cmos_sensor(0x8b,0x7f);
	write_cmos_sensor(0x8f,0x82);
	write_cmos_sensor(0x94,0x92);//42->92 avoid ae vabration
	write_cmos_sensor(0x95,0x84);
	write_cmos_sensor(0x96,0xb3);
	write_cmos_sensor(0x97,0x48);//40->48 avoid vabration
	write_cmos_sensor(0x98,0x8a);//9b->8a
	write_cmos_sensor(0x99,0x10);
	write_cmos_sensor(0x9a,0x50);
	write_cmos_sensor(0x9f,0x64);
	write_cmos_sensor(0x39,0x98);
	write_cmos_sensor(0x3f,0x18);
	write_cmos_sensor(0x90,0x20);
	write_cmos_sensor(0x91,0xd0);

#if 1
	//gamma1   default  
	write_cmos_sensor(0X40,0X3b);
	write_cmos_sensor(0X41,0X36);
	write_cmos_sensor(0X42,0X2b);
	write_cmos_sensor(0X43,0X1d);
	write_cmos_sensor(0X44,0X1a);
	write_cmos_sensor(0X45,0X14);
	write_cmos_sensor(0X46,0X11);
	write_cmos_sensor(0X47,0X0e);
	write_cmos_sensor(0X48,0X0d);
	write_cmos_sensor(0X49,0X0c);
	write_cmos_sensor(0X4b,0X0b);
	write_cmos_sensor(0X4c,0X09);
	write_cmos_sensor(0X4e,0X08);
	write_cmos_sensor(0X4f,0X07);
	write_cmos_sensor(0X50,0X07);
#endif

	//color   out door
	write_cmos_sensor(0x5a,0x56);
	write_cmos_sensor(0x51,0x13);
	write_cmos_sensor(0x52,0x05);
	write_cmos_sensor(0x53,0x91);
	write_cmos_sensor(0x54,0x72);
	write_cmos_sensor(0x57,0x96);
	write_cmos_sensor(0x58,0x35);
	//color  fu se hao 
	write_cmos_sensor(0x5a,0xd6);
	write_cmos_sensor(0x51,0x17);
	write_cmos_sensor(0x52,0x13);
	write_cmos_sensor(0x53,0x5e);
	write_cmos_sensor(0x54,0x38);
	write_cmos_sensor(0x57,0x38);
	write_cmos_sensor(0x58,0x02);
	/*
	//color  indoor
	write_cmos_sensor(0x5a,0xd6);
	write_cmos_sensor(0x51,0x29);
	write_cmos_sensor(0x52,0x0D);
	write_cmos_sensor(0x53,0x91);
	write_cmos_sensor(0x54,0x81);
	write_cmos_sensor(0x57,0x56);
	write_cmos_sensor(0x58,0x09);
	*/

	write_cmos_sensor(0x5b,0x02);
	write_cmos_sensor(0x5c,0x30);
	write_cmos_sensor(0xb0,0xe0);
	write_cmos_sensor(0xb3,0x5a); 
	write_cmos_sensor(0xb4,0xe3);
	write_cmos_sensor(0xb1,0xef);
	write_cmos_sensor(0xb2,0xc0); 
	write_cmos_sensor(0xb4,0x63);

	write_cmos_sensor(0xb1,0xb5);
	write_cmos_sensor(0xb2,0x9c);
	write_cmos_sensor(0x55,0x00);
	write_cmos_sensor(0x56,0x40);	
	write_cmos_sensor(0x6a,0x81);

	write_cmos_sensor(0x70,0x0b);
	write_cmos_sensor(0x69,0x00); // effect normal 
	write_cmos_sensor(0x67,0x80); // Normal, 
	write_cmos_sensor(0x68,0x80); // Normal, 
	write_cmos_sensor(0xb4,0x63); // Normal, 
	write_cmos_sensor(0x23,0x55);
	write_cmos_sensor(0xa0,0x00);
	write_cmos_sensor(0xa1,0x31);
	write_cmos_sensor(0xa2,0x0d); 
	write_cmos_sensor(0xa3,0x26);
	write_cmos_sensor(0xa4,0x09);
	write_cmos_sensor(0xa5,0x22);
	write_cmos_sensor(0xa6,0x04);
	write_cmos_sensor(0xa7,0x1a);
	write_cmos_sensor(0xa8,0x18);
	write_cmos_sensor(0xa9,0x13);
	write_cmos_sensor(0xaa,0x18);
	write_cmos_sensor(0xab,0x24);
	write_cmos_sensor(0xac,0x3c);
	write_cmos_sensor(0xad,0xf0);
	write_cmos_sensor(0xae,0x59);
	write_cmos_sensor(0xc5,0xaa);
	write_cmos_sensor(0xc6,0xbb);
	write_cmos_sensor(0xc7,0x30);
	write_cmos_sensor(0xc8,0x0d);
	write_cmos_sensor(0xc9,0x10);
	write_cmos_sensor(0xd0,0xa3);
	write_cmos_sensor(0xd1,0x00);
	write_cmos_sensor(0xd2,0x58);
	write_cmos_sensor(0xd3,0x09);
	write_cmos_sensor(0xd4,0x24);
	write_cmos_sensor(0xee,0x30);
    LOG_INF("BF3905_MIPI_Initialize_Setting end\n");


}
static void pip_capture_setting(void)
{
	LOG_INF( "Enter!\n");
}

static void normal_capture_setting(void)
{
	LOG_INF( "Enter!\n");
}


static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	if(currefps==300)
		normal_capture_setting();
	else if(currefps==150) // PIP
		pip_capture_setting();
	else
		normal_capture_setting();
}
static void hs_video_setting(void)
{
	LOG_INF("E! ");
}
static void normal_video_setting(void)
{
	LOG_INF("E! ");
}

static void slim_video_setting(void)
{
	LOG_INF("E! ");
}
static void set_max_min_fps(UINT32 u2MinFrameRate, UINT32 u2MaxFrameRate)
{//not call on mt6580
	/*preview                  :30~ 5,
	    normal         video:30~ 15, 
	    night mode video:15~ 15,
	*/
	LOG_INF("Jeff:u2MinFrameRate(%d) u2MaxFrameRate(%d)\n",u2MinFrameRate,u2MaxFrameRate);

	spin_lock(&imgsensor_drv_lock);
	imgsensor_info.minframerate = u2MinFrameRate;
	imgsensor_info.maxframerate = u2MaxFrameRate;
	spin_unlock(&imgsensor_drv_lock);

	return;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id) 
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) 
	{
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do 
		{
			*sensor_id = (read_cmos_sensor(0xfc) << 8) | read_cmos_sensor(0xfd);
			if (*sensor_id == imgsensor_info.sensor_id) 
			{               
				LOG_INF("Jeff: i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);      
				return ERROR_NONE;
			}   
			LOG_INF("Jeff :Read sensor id fail, id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
			
			retry--;
		} while(retry > 0);

	    i++;
	    retry = 2;
	}
	
    if (*sensor_id != imgsensor_info.sensor_id) 
    {
        // if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF 
        *sensor_id = 0xFFFFFFFF;
         return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}   
static UINT32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0; 
	LOG_1;
	LOG_2;
	
	while (imgsensor_info.i2c_addr_table[i] != 0xff)
	{
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		
		do
		{
			sensor_id =(read_cmos_sensor(0xfc) << 8) | read_cmos_sensor(0xfd);
			
			if (sensor_id == imgsensor_info.sensor_id) 
			{                
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);   
				break;
			}   
			LOG_INF("Read sensor id fail, id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
			retry--;
		} while(retry > 0);
		
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}        
	
	if (imgsensor_info.sensor_id != sensor_id)
	return ERROR_SENSOR_CONNECT_FAIL;
	/* initail sequence write in  */
	sensor_init();
	af_Init();
	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	
static UINT32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function*/ 
	
	return ERROR_NONE;
}

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength; 
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(IMAGE_V_MIRROR);

//	set_night_mode(gNightMode);  //BF3905

	return ERROR_NONE;
}	 
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) // 30fps
	{
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	else //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
	{
		if (imgsensor.current_fps != imgsensor_info.cap1.max_framerate)
			LOG_INF("Warning: current_fps is not support, so use cap1's setting!\n");	
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} 

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n",imgsensor.current_fps);
	capture_setting(imgsensor.current_fps); 
	set_mirror_flip(IMAGE_V_MIRROR);
	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;  
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	set_mirror_flip(IMAGE_V_MIRROR);	
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength; 
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(IMAGE_V_MIRROR);
	return ERROR_NONE;
}	/*	hs_video   */


static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength; 
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(IMAGE_V_MIRROR);
	return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;		

	
	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;
	
	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}	

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d", scenario_id);
	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 1; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 1; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame; 
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame; 
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame; 
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	
	sensor_info->YUVAwbDelayFrame = imgsensor_info.awb_delay_frame;
	sensor_info->YUVEffectDelayFrame= imgsensor_info.effect_delay_frame; 
	sensor_info->AEShutDelayFrame= imgsensor_info.ae_delay_frame;	//Jeffbie
	
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num; 
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */
	
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 14;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x 
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;		
			
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
				  
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc; 

			break;	 
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			
			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
	   
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc; 

			break;	  
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO: 		
			sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
				  
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc; 

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
				  
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc; 

			break;
		default:			
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;		
			
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}
	
	return ERROR_NONE;
}

static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			preview(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			capture(image_window, sensor_config_data);
			break;	
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;	  
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			hs_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			slim_video(image_window, sensor_config_data);
			break;	  	  
		default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	

static void get_evawb_ref(PSENSOR_AE_AWB_REF_STRUCT  Ref)
{
	LOG_INF("E \n");
	Ref->SensorAERef.AeRefLV05Shutter = 5422;
	Ref->SensorAERef.AeRefLV05Gain = 478; /* 128 base */
	Ref->SensorAERef.AeRefLV13Shutter = 80;
	Ref->SensorAERef.AeRefLV13Gain = 128; /*  128 base */
	Ref->SensorAwbGainRef.AwbRefD65Rgain = 186; /* 128 base */
	Ref->SensorAwbGainRef.AwbRefD65Bgain = 158; /* 128 base */
	Ref->SensorAwbGainRef.AwbRefCWFRgain = 196; /* 1.25x, 128 base */
	Ref->SensorAwbGainRef.AwbRefCWFBgain = 278; /* 1.28125x, 128 base */	
}

static void get_curAeAwb_Info(PSENSOR_AE_AWB_CUR_STRUCT Info)
{
	LOG_INF("E \n");
	Info->SensorAECur.AeCurShutter = get_shutter();
	Info->SensorAECur.AeCurGain = get_gain() * 2; /* 128 base */
	Info->SensorAwbGainCur.AwbCurRgain = get_awb_rgain()<< 1; /* 128 base */
	Info->SensorAwbGainCur.AwbCurBgain = get_awb_bgain()<< 1; /* 128 base */
}

static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate) 
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	switch (scenario_id) 
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*framerate = imgsensor_info.pre.max_framerate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*framerate = imgsensor_info.normal_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*framerate = imgsensor_info.cap.max_framerate;
			break;		
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*framerate = imgsensor_info.hs_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO: 
			*framerate = imgsensor_info.slim_video.max_framerate;
			break;
		default:
			*framerate = imgsensor_info.pre.max_framerate;
			break;
	}
	return ERROR_NONE;
}

static void get_AeAwb_lock_info(UINT32 *pAElockRet32, UINT32 *pAWBlockRet32)
{
	*pAElockRet32 = 1;
	*pAWBlockRet32 = 1;
	LOG_INF("pAElockRet32 = %d,pAWBlockRet32=%d\n", *pAElockRet32,*pAWBlockRet32);
}

static void get_delay_Info(uintptr_t  pdelayAddr)
{
	SENSOR_DELAY_INFO_STRUCT *pDelayInfo=(SENSOR_DELAY_INFO_STRUCT*)pdelayAddr;

	LOG_INF("E \n");
	pDelayInfo->InitDelay = imgsensor_info.InitDelay;
	pDelayInfo->EffectDelay = imgsensor_info.EffectDelay;
	pDelayInfo->AwbDelay = imgsensor_info.AwbDelay;
	pDelayInfo->AFSwitchDelayFrame = imgsensor_info.AFSwitchDelayFrame;
	pDelayInfo->ContrastDelay = imgsensor_info.ContrastDelay;
    pDelayInfo->EvDelay = imgsensor_info.EvDelay;
    pDelayInfo->BrightDelay = imgsensor_info.BrightDelay;
    pDelayInfo->SatDelay = imgsensor_info.SatDelay;
} 

static void set_flashtrigger_info(unsigned int *pFeatureReturnPara32)
{
}
#ifdef NOT_USED
static void get_flash_Info(UINT32 infoAddr)
{
}
#endif

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);
	if (enable) 
	{
	    write_cmos_sensor(0x13,0x08);
		write_cmos_sensor(0x8c,0x03);
		write_cmos_sensor(0x8d,0xbc);
		write_cmos_sensor(0x87,0x88);
	         write_cmos_sensor(0xf1,0x6b);
		write_cmos_sensor(0x28,0x00);
		write_cmos_sensor(0x06,0xc8);
		write_cmos_sensor(0xb9,0x80);
	} 
	else 
	{  

        write_cmos_sensor(0x13,0x07);		
	    write_cmos_sensor(0xf1,0x00);
	    write_cmos_sensor(0x28,0xff);
		write_cmos_sensor(0x06,0x68);
		write_cmos_sensor(0xb9,0x00);
	}	 
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static void af_Init(void)
{
	LOG_INF("E\n");
}

static void constant_af(void)
{
	LOG_INF("E\n");
}   

static void single_af(void)
{
}

static void get_af_max_num_focus_areas(UINT32 *pFeatureReturnPara32)
{ 	  
    *pFeatureReturnPara32 = 0;    //0 is means it is not support touch af,1 is means it is support touch af 
     LOG_INF("get_af_max_num_focus_areas,0  not support touch af,1 support touch af , *pFeatureReturnPara32 = %d\n",*pFeatureReturnPara32);
}

static void get_ae_max_num_metering_areas(UINT32 *pFeatureReturnPara32)
{
	*pFeatureReturnPara32 = 0;    //0 is means it is not support touch ae,1 is means it is support touch ae 	
}
#ifdef NOT_USED
static void set_preview_coordinate_to_sensor_coordinate(unsigned int prevX,unsigned int prevY)
{
}
#endif

static void set_af_window(unsigned int zone_addr,unsigned int prevW,unsigned int prevH)
{//update global zone
	LOG_INF("E:\n");
}

static void get_af_macro(UINT32 *pFeatureReturnPara32)
{
}

static void get_af_Inf(UINT32 * pFeatureReturnPara32)
{
}   

static void get_af_status(UINT32 *pFeatureReturnPara32)
{
}  

static void cancel_af(void)
{
}

static void set_ae_window(unsigned int zone_addr,unsigned int prevW,unsigned int prevH)
{
	LOG_INF("E:\n");
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *pfeature_para,UINT32 *pfeature_para_len)
{
	UINT16 *pfeature_return_para_16=(UINT16 *) pfeature_para;
	//UINT16 *pfeature_data_16=(UINT16 *)pfeature_para;
	UINT32 *pfeature_return_para_32=(UINT32 *)pfeature_para;
	UINT32 *pfeature_data_32=(UINT32 *) pfeature_para;
	
	unsigned long long *pfeature_data=(unsigned long long *) pfeature_para;    
	
	//SENSOR_WINSIZE_INFO_STRUCT *wininfo;	
	MSDK_SENSOR_REG_INFO_STRUCT *psensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) pfeature_para;
        //MSDK_SENSOR_CONFIG_STRUCT *psensor_config_data=(MSDK_SENSOR_CONFIG_STRUCT *) pfeature_para;

	LOG_INF("feature_id = %d", feature_id);
	switch (feature_id)
	{
		case SENSOR_FEATURE_GET_RESOLUTION:
			*pfeature_return_para_16++ = imgsensor.line_length;
			*pfeature_return_para_16 = imgsensor.line_length;
			*pfeature_para_len=4;
			break;
		case SENSOR_FEATURE_GET_PERIOD:
			*pfeature_return_para_16++ = imgsensor.line_length;
			*pfeature_return_para_16 = imgsensor.frame_length;
			*pfeature_para_len=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:	 
			LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n", imgsensor.pclk,imgsensor.current_fps);
			*pfeature_return_para_32 = imgsensor.pclk;
			*pfeature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			//set_night_mode((BOOL) *pfeature_data);  20150316jeff
			break;
		case SENSOR_FEATURE_SET_MIN_MAX_FPS:
			set_max_min_fps((UINT32)*pfeature_data, (UINT32)*(pfeature_data+1));
			break;
		/**********************Strobe Ctrl Start *******************************/
		case SENSOR_FEATURE_SET_ESHUTTER:
			//set_shutter(*pfeature_data);
			break;
		case SENSOR_FEATURE_SET_GAIN:
			//set_gain((UINT16) *pfeature_data);
			break;
	        case SENSOR_FEATURE_SET_GAIN_AND_ESHUTTER:
			break;
		case SENSOR_FEATURE_GET_AE_FLASHLIGHT_INFO:
			//get_flash_Info((uintptr_t)*pfeature_data);
			break;
		case SENSOR_FEATURE_GET_TRIGGER_FLASHLIGHT_INFO:
			set_flashtrigger_info(pfeature_return_para_32);
			break;		
		case SENSOR_FEATURE_SET_FLASHLIGHT:
			break;
		/**********************Strobe Ctrl End *******************************/	
		
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			break;
		case SENSOR_FEATURE_SET_REGISTER:
			write_cmos_sensor(psensor_reg_data->RegAddr, psensor_reg_data->RegData);
			break;
		case SENSOR_FEATURE_GET_REGISTER:
			psensor_reg_data->RegData = read_cmos_sensor(psensor_reg_data->RegAddr);
			break; 

	        case SENSOR_FEATURE_GET_CONFIG_PARA://jeff 
	//	           memcpy(psensor_config_data, &BF3905_MIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	//	           *pfeature_para_len=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
		             break;
		case SENSOR_FEATURE_SET_CCT_REGISTER:
		case SENSOR_FEATURE_GET_CCT_REGISTER:
		case SENSOR_FEATURE_SET_ENG_REGISTER:
		case SENSOR_FEATURE_GET_ENG_REGISTER:
		case SENSOR_FEATURE_GET_REGISTER_DEFAULT:

		case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
		case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
		
		case SENSOR_FEATURE_GET_GROUP_INFO:
		case SENSOR_FEATURE_GET_ITEM_INFO:
		case SENSOR_FEATURE_SET_ITEM_INFO:
		case SENSOR_FEATURE_GET_ENG_INFO:
			break;
		case SENSOR_FEATURE_GET_GROUP_COUNT:
		            *pfeature_return_para_32++=0;
		            *pfeature_para_len=4;	    
			break;
			
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			*pfeature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
			*pfeature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_YUV_CMD:
			set_yuv_cmd((FEATURE_ID)*pfeature_data, *(pfeature_data+1));
			break;	
		case SENSOR_FEATURE_SET_YUV_3A_CMD:
			LOG_INF("Jeff:SENSOR_FEATURE_SET_YUV_3A_CMD. \n");
			set_3actrl_info((ACDK_SENSOR_3A_LOCK_ENUM)*pfeature_data);
			break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
			set_video_mode(*pfeature_data);
			break;
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			get_imgsensor_id(pfeature_return_para_32); 
			break; 
		case SENSOR_FEATURE_GET_EV_AWB_REF:
			get_evawb_ref((PSENSOR_AE_AWB_REF_STRUCT)(uintptr_t)*pfeature_data);
			break;		
		case SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN:
			get_curAeAwb_Info((PSENSOR_AE_AWB_CUR_STRUCT)(uintptr_t)*pfeature_data);		//jeff	
			break;
		case SENSOR_FEATURE_GET_EXIF_INFO:
			get_exifInfo((UINT32  *)(uintptr_t)*pfeature_data);//jeff
			break;
		case SENSOR_FEATURE_GET_DELAY_INFO:
			get_delay_Info((uintptr_t)*pfeature_data);  //ieff
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN: 
			set_test_pattern_mode((BOOL)*pfeature_data);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:			 
			*pfeature_return_para_32 = imgsensor_info.checksum_value;
			*pfeature_para_len=4;							 
			break;				
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*pfeature_data, *(pfeature_data+1));
			
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*pfeature_data, (MUINT32 *)(uintptr_t)(*(pfeature_data+1)));
			break;


		/******************AF control below*********************/
		case SENSOR_FEATURE_INITIALIZE_AF:
			 af_Init();
			 break;
		case SENSOR_FEATURE_MOVE_FOCUS_LENS:
		 //af_move_to(pfeature_data_32); // not implement yet.
			 break;
		case SENSOR_FEATURE_GET_AF_STATUS:
			 get_af_status(pfeature_data_32);
			 *pfeature_para_len = 4;
			 break;
		case SENSOR_FEATURE_SINGLE_FOCUS_MODE:
			 single_af();
			 break;
		case SENSOR_FEATURE_CONSTANT_AF:
			 constant_af();
			 break;
		case SENSOR_FEATURE_CANCEL_AF:
			 cancel_af();
			 break;
		case SENSOR_FEATURE_GET_AF_INF:
			 get_af_Inf(pfeature_data_32);
			 *pfeature_para_len=4;
			 break;
		case SENSOR_FEATURE_GET_AF_MACRO:
			 get_af_macro(pfeature_data_32);
			 *pfeature_para_len=4;
			 break;
		case SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS:
			 get_af_max_num_focus_areas(pfeature_data_32);
			 *pfeature_para_len=4;
			 break;
		case SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS:
			 get_ae_max_num_metering_areas(pfeature_data_32);
			 *pfeature_para_len=4;
			 break;
		case SENSOR_FEATURE_SET_AF_WINDOW:     
			 set_af_window((uintptr_t)*pfeature_data,imgsensor_info.pre.grabwindow_width, imgsensor_info.pre.grabwindow_height);
			 break;
		case SENSOR_FEATURE_SET_AE_WINDOW:
			 set_ae_window((uintptr_t)*pfeature_data,imgsensor_info.pre.grabwindow_width, imgsensor_info.pre.grabwindow_height);
			 break;
		case SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO:
			 get_AeAwb_lock_info((UINT32 *)(uintptr_t)(*pfeature_data), (UINT32 *)(uintptr_t)(*(pfeature_data+1)));
			 break;
			 
		default:
			break;			
		}
	return ERROR_NONE;
}	

SENSOR_FUNCTION_STRUCT	SensorFuncBF3905MIPI=
{
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 BF3905_MIPI_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&SensorFuncBF3905MIPI;
	return ERROR_NONE;
}	/* SensorInit() */



