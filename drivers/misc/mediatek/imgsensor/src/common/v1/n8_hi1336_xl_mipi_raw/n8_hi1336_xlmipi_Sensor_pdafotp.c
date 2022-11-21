#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/types.h>


//#include "n8_hi1336_xl_mipi_raw_Sensor.h"

#define PFX "N8_HI1336_XL_pdafotp"
#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)

//#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iMultiReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId, u8 number);


#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)

#define N8_HI1336_XL_EEPROM_READ_ID  0xA2
#define N8_HI1336_XL_EEPROM_WRITE_ID   0xA3
#define N8_HI1336_XL_I2C_SPEED        100  
#define N8_HI1336_XL_MAX_OFFSET		0xFFFF

#define DATA_SIZE 2048

#define READ_EEPROM_ID 	0xB0
#define	I2C_SPEED	100

BYTE n8_hi1336_xl_eeprom_data[DATA_SIZE]= {0};
static bool get_done = false;
static int last_size = 0;
static int last_offset = 0;


BYTE eeprom_data[DATA_SIZE]= {0};

bool getPDAFCalDataFromFile(void)
{
   bool Flag = false;
   int fd;
   mm_segment_t old_fs = get_fs();
   set_fs(KERNEL_DS);

   //fd = sys_open("/data/pdaf.txt", O_RDONLY, 777);
   fd = sys_open("/sdcard/DCIM/pdaf.txt", O_RDONLY, 777);

   if( fd < 0 )   
   {
     LOG_INF("KYM PDAF FILE READ FAIL\n");
     goto RESULT;
   }
   else
   {
//	 if( sys_read(fd, (char *)&n8_hi1336_xl_eeprom_data[0], 1372) )
 	if( sys_read(fd, (char *)&n8_hi1336_xl_eeprom_data[0], 1404) )
     {
       LOG_INF("KYM PDAF FILE READ PASS\n");
       Flag = true;
     }
   }

RESULT:
    sys_close(fd);
    set_fs(old_fs);
    return Flag;
}

bool read_n8_hi1336_xl_eeprom( kal_uint16 addr, BYTE* data, kal_uint32 size)
{
 	bool Flag;
	addr = 0x077b;
	size = 1382;

	
 	Flag = getPDAFCalDataFromFile();
 	if( Flag )  memcpy( data, n8_hi1336_xl_eeprom_data, size );

 	return Flag;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		read EEPROM In VCM 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if 1
static bool selective_read_eeprom(kal_uint16 addr, BYTE* data)
{
	
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };

	if(addr > N8_HI1336_XL_MAX_OFFSET	)
		return false;

	//kdSetI2CSpeed(I2C_SPEED);

	if(iReadRegI2C(pu_send_cmd, 2, (u8*)data, 1, READ_EEPROM_ID   )<0)
	{
       		LOG_INF("VCM E2PROM READ fail\n");
		return false;
	}

	//LOG_INF( "read eeprom : 0x%d \n", (u8*)data );

    return true;
}

bool _read_eeprom(kal_uint16 addr, BYTE* data, kal_uint32 size )
{
	int i = 0;
	int index = 0;
	int offset = addr;

	for(i = 0; i < 496; i++) {
		if(!selective_read_eeprom(offset, &data[i])){
			return false;
		}
		//LOG_INF("VCM1 read_eeprom 0x%0x %d\n",offset, data[i]);
		offset++;
		index++;
	}

	offset =0x96b;

	for(i = 0; i < 886; i++) { //ok
		if(!selective_read_eeprom(offset, &data[index])){
			return false;
		}
		
		//LOG_INF("VCM2 read_eeprom 0x%0x %d\n",offset, data[i]);
		offset++;
		index++;
	}




	get_done = true;
	last_size = size;
	last_offset = addr;
    return true;
}

bool read_eeprom( kal_uint16 addr, BYTE* data, kal_uint32 size)
{
	BYTE af_pos[4];

	size = 0x0566;//0x5BC;//0x057c;
	addr = 0x077b;
	if(!get_done || last_size != size || last_offset != addr) {
		if(!_read_eeprom(addr, eeprom_data, size)){
			get_done = 0;
            last_size = 0;
            last_offset = 0;
			return false;
		}
	}
	memcpy(data, eeprom_data, size);

	selective_read_eeprom(0x0011, &af_pos[0]);
	selective_read_eeprom(0x0012, &af_pos[1]);
	selective_read_eeprom(0x0013, &af_pos[2]);
	selective_read_eeprom(0x0014, &af_pos[3]);
	LOG_INF("VCM read_eeprom Inf1 : 0x%0x%0x  , Macro : 0x%0x%0x \n", af_pos[0], af_pos[1],af_pos[2], af_pos[3]);

	
    return true;
}
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////////////





