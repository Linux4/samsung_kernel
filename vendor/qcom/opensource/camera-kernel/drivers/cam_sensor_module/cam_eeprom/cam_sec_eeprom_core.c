// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/crc32.h>
#include <linux/firmware.h>
#include <media/cam_sensor.h>

#include "cam_sec_eeprom_core.h"
#include "cam_eeprom_soc.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#include <linux/ctype.h>

#if defined(CONFIG_SAMSUNG_WACOM_NOTIFIER)
#include "cam_notifier.h"
#endif

#define CAM_EEPROM_DBG  1
#define MAX_READ_SIZE  0x7FFFF

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

char cam_cal_check[MAX_EEP_CAMID][SYSFS_FW_VER_SIZE] = { "NULL", "NULL", "NULL", "NULL", "NULL", "NULL", "NULL"};
char camera_info[MAX_EEP_CAMID][10] = { "Rear", "Front", "Rear2", "Rear3", "Rear4", "Front2", "Front3"};

#if defined(CONFIG_SAMSUNG_REAR_BOKEH)
char bokeh_module_fw_ver[FROM_MODULE_FW_INFO_SIZE+1];
#endif

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
char rear3_module_fw_ver[FROM_MODULE_FW_INFO_SIZE+1];
#endif

#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
char rear4_module_fw_ver[FROM_MODULE_FW_INFO_SIZE+1];
#endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
uint8_t ois_m1_xysr[OIS_XYSR_SIZE] = { 0, };
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
uint8_t ois_m2_xysr[OIS_XYSR_SIZE] = { 0, };
uint8_t ois_m2_cross_talk[OIS_CROSSTALK_SIZE] = { 0, };
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
uint8_t ois_m3_xysr[OIS_XYSR_SIZE] = { 0, };
uint8_t ois_m3_cross_talk[OIS_CROSSTALK_SIZE] = { 0, };
#endif
#endif

ConfigInfo_t ConfigInfo[MAX_CONFIG_INFO_IDX];

char M_HW_INFO[HW_INFO_MAX_SIZE] = "";
char M_SW_INFO[SW_INFO_MAX_SIZE] = "";
char M_VENDOR_INFO[VENDOR_INFO_MAX_SIZE] = "";
char M_PROCESS_INFO[PROCESS_INFO_MAX_SIZE] = "";

char S_HW_INFO[HW_INFO_MAX_SIZE] = "";
char S_SW_INFO[SW_INFO_MAX_SIZE] = "";
char S_VENDOR_INFO[VENDOR_INFO_MAX_SIZE] = "";
char S_PROCESS_INFO[PROCESS_INFO_MAX_SIZE] = "";

uint8_t CriterionRev;
uint8_t ModuleVerOnPVR;
uint8_t ModuleVerOnSRA;
uint8_t minCalMapVer;

#if defined(CONFIG_HI847_OTP)
#include "hi847_otp.h"

struct cam_sensor_i2c_reg_setting load_hi847_otp_setfile = {
	load_sensor_hi847_otp_setfile_reg,
	sizeof(load_sensor_hi847_otp_setfile_reg)/sizeof(load_sensor_hi847_otp_setfile_reg[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_WORD,
	50
};

struct cam_sensor_i2c_reg_setting hi847_otp_init_setting1 = {
	hi847_otp_init_reg1,
	sizeof(hi847_otp_init_reg1)/sizeof(hi847_otp_init_reg1[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_BYTE,
	10
};

struct cam_sensor_i2c_reg_setting hi847_otp_init_setting2 = {
	hi847_otp_init_reg2,
	sizeof(hi847_otp_init_reg2)/sizeof(hi847_otp_init_reg2[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_BYTE,
	10
};

struct cam_sensor_i2c_reg_setting hi847_otp_finish_setting1 = {
	hi847_otp_finish_reg1,
	sizeof(hi847_otp_finish_reg1)/sizeof(hi847_otp_finish_reg1[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_BYTE,
	10
};

struct cam_sensor_i2c_reg_setting hi847_otp_finish_setting2 = {
	hi847_otp_finish_reg2,
	sizeof(hi847_otp_finish_reg2)/sizeof(hi847_otp_finish_reg2[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_BYTE,
	10
};

#endif
#if defined(CONFIG_HI1337_OTP)
#include "hi1337_otp.h"

struct cam_sensor_i2c_reg_setting load_hi1337_otp_setfile = {
	load_sensor_hi1337_otp_setfile_reg,
	sizeof(load_sensor_hi1337_otp_setfile_reg)/sizeof(load_sensor_hi1337_otp_setfile_reg[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_WORD,
	50
};

struct cam_sensor_i2c_reg_setting hi1337_otp_init_setting1 = {
	hi1337_otp_init_reg1,
	sizeof(hi1337_otp_init_reg1)/sizeof(hi1337_otp_init_reg1[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_BYTE,
	10
};

struct cam_sensor_i2c_reg_setting hi1337_otp_init_setting2 = {
	hi1337_otp_init_reg2,
	sizeof(hi1337_otp_init_reg2)/sizeof(hi1337_otp_init_reg2[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_BYTE,
	10
};

struct cam_sensor_i2c_reg_setting hi1337_otp_finish_setting1 = {
	hi1337_otp_finish_reg1,
	sizeof(hi1337_otp_finish_reg1)/sizeof(hi1337_otp_finish_reg1[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_BYTE,
	10
};

struct cam_sensor_i2c_reg_setting hi1337_otp_finish_setting2 = {
	hi1337_otp_finish_reg2,
	sizeof(hi1337_otp_finish_reg2)/sizeof(hi1337_otp_finish_reg2[0]),
	CAMERA_SENSOR_I2C_TYPE_WORD,
	CAMERA_SENSOR_I2C_TYPE_BYTE,
	10
};
#endif

#if defined(CONFIG_HI847_OTP)
static int cam_otp_hi847_init( struct camera_io_master *io_master_info)
{
    int	rc = 0;

    if ( !io_master_info )
    {
        CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
        return(-EINVAL);
    }

    /* load otp global setfile */

    rc = camera_io_dev_write( io_master_info, &load_hi847_otp_setfile );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "load otp globle setfile failed" );
        return(rc);
    }

    /* OTP initial setting1 write */
    rc = camera_io_dev_write( io_master_info, &hi847_otp_init_setting1 );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "load otp initial setfile1 failed" );
        return(rc);
    }

    msleep(10);

    /* OTP initial setting2 write */
    rc = camera_io_dev_write( io_master_info, &hi847_otp_init_setting2 );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "load otp initial setfile2 failed" );
        return(rc);
    }

    CAM_INFO( CAM_EEPROM, "load otp init setting done!");
    return rc;
}

static int cam_otp_hi847_read( struct camera_io_master *io_master_info, uint32_t addr,
                                uint8_t *memptr )
{
    int					rc = 0;
    struct cam_sensor_i2c_reg_setting	i2c_reg_settings;
    struct cam_sensor_i2c_reg_array		i2c_reg_array;
    enum camera_sensor_i2c_type		addr_type	= CAMERA_SENSOR_I2C_TYPE_WORD;
    enum camera_sensor_i2c_type		data_type	= CAMERA_SENSOR_I2C_TYPE_BYTE;
    uint32_t				read_addr		= 0;

    if ( !io_master_info )
    {
        CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
        return(-EINVAL);
    }

    i2c_reg_settings.addr_type	= addr_type;
    i2c_reg_settings.data_type	= data_type;
    i2c_reg_settings.size		= 1;
    i2c_reg_settings.delay		= 4;
    i2c_reg_array.delay		= 4;

    /* high address */
    i2c_reg_array.reg_addr		= 0x030a;
    i2c_reg_array.reg_data		= (addr >> 8) & 0xff;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write high address failed" );
        goto err;
    }

    /* low address */
    i2c_reg_array.reg_addr		= 0x030b;
    i2c_reg_array.reg_data		= addr & 0xff;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write low address failed" );
        goto err;
    }

    i2c_reg_array.reg_addr		= 0x031C;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write otp signal 1 failed" );
        goto err;
    }
    i2c_reg_array.reg_addr		= 0x031D;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write otp signal 2 failed" );
        goto err;
    }
    /* OTP continue read mode */
    i2c_reg_array.reg_addr		= 0x0302;
    i2c_reg_array.reg_data		= 0x01;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }

    /* OTP data verify */
    rc = camera_io_dev_read( io_master_info, 0x030a, &read_addr, addr_type, addr_type, false);
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
    }
    if(read_addr != addr)
    CAM_INFO( CAM_EEPROM, "ERROR WRONG addr=0x%x read_addr=0x%x", addr, read_addr );

    CAM_INFO( CAM_EEPROM, "addr=0x%x read_addr=0x%x", addr, read_addr );
    /* OTP data read */
    rc = camera_io_dev_read_seq( io_master_info, 0x0308, memptr, addr_type, data_type, 1 );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
    }

    CAM_ERR( CAM_EEPROM, "addr=0x%x  read_addr=0x%x  *memptr=0x%x", addr, read_addr, *memptr );

err:
    return(rc);
}


static int cam_otp_hi847_burst_read( struct camera_io_master *io_master_info, uint32_t addr,
                                      uint8_t *memptr, uint32_t read_size )
{
    int					rc = 0;
    struct cam_sensor_i2c_reg_setting	i2c_reg_settings;
    struct cam_sensor_i2c_reg_array		i2c_reg_array;
    enum camera_sensor_i2c_type		addr_type	= CAMERA_SENSOR_I2C_TYPE_WORD;
    enum camera_sensor_i2c_type		data_type	= CAMERA_SENSOR_I2C_TYPE_BYTE;
    uint32_t				read_addr		= 0;

    if ( !io_master_info )
    {
        CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
        return(-EINVAL);
    }

    i2c_reg_settings.addr_type	= addr_type;
    i2c_reg_settings.data_type	= data_type;
    i2c_reg_settings.size		= 1;
    i2c_reg_settings.delay		= 4;
    i2c_reg_array.delay		= 4;

    /* high address */
    i2c_reg_array.reg_addr		= 0x030a;
    i2c_reg_array.reg_data		= (addr >> 8) & 0xff;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write high address failed" );
        goto err;
    }

    /* low address */
    i2c_reg_array.reg_addr		= 0x030b;
    i2c_reg_array.reg_data		= addr & 0xff;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write low address failed" );
        goto err;
    }

    i2c_reg_array.reg_addr		= 0x031C;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write otp signal 1 failed" );
        goto err;
    }
    i2c_reg_array.reg_addr		= 0x031D;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write otp signal 2 failed" );
        goto err;
    }
    /* OTP continue read mode */
    i2c_reg_array.reg_addr		= 0x0302;
    i2c_reg_array.reg_data		= 0x01;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }

    /* OTP data verify*/
    rc = camera_io_dev_read( io_master_info, 0x030a, &read_addr, addr_type, addr_type, false);
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
    }

    CAM_INFO( CAM_EEPROM, "CHECK ERROR addr=0x%x read_addr=0x%x", addr, read_addr );

    /* burst read on */
    i2c_reg_array.reg_addr		= 0x0712;
    i2c_reg_array.reg_data		= 0x01;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }

    /* OTP data burst read */
    rc = camera_io_dev_read_seq( io_master_info, 0x0308, memptr, addr_type, data_type, read_size );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
    }

    /* burst read off */
    i2c_reg_array.reg_addr		= 0x0712;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }

err:
    return(rc);
}

int cam_otp_hi847_read_memory( struct cam_eeprom_ctrl_t *e_ctrl,
                                       struct cam_eeprom_memory_block_t *block )

{
    struct cam_eeprom_memory_map_t	*emap	= block->map;
    struct cam_eeprom_soc_private	*eb_info;
    uint32_t	addr		= 0;
    uint32_t	read_size	= 0;
    uint32_t	offset = 0;
    uint8_t		OTP_Bank	= 0;
    uint8_t				*memptr = block->mapdata;
    int		read_bytes	= 0;
    int		rc	= 0;
    int		j	= 0;

    if ( !e_ctrl )
    {
        CAM_ERR( CAM_EEPROM, "e_ctrl is NULL" );
        return(-EINVAL);
    }

    eb_info = (struct cam_eeprom_soc_private *) e_ctrl->soc_info.soc_private;

    rc = cam_otp_hi847_init(&e_ctrl->io_master_info);
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "OTP init failed" );
        goto err;
    }

    /* select bank */
    rc = cam_otp_hi847_read( &e_ctrl->io_master_info, SENSOR_HI847_OTP_BANK_SELECT_REGISTER, &OTP_Bank );

    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read data failed" );
        goto err;
    }
    CAM_INFO( CAM_EEPROM, "current OTP_Bank: %d", OTP_Bank );

    switch ( OTP_Bank )
    {
    /* Refer to OTP document */
    case 0:
    case 1:
        offset = 0x0704;
        break;

    case 3:
        offset = 0x0D04;
        break;

    case 7:
        offset = 0x1304;
        break;

    case 0xF:
        offset = 0x1904;
        break;

    default:
        CAM_INFO( CAM_EEPROM, "Bank error : Bank(%d)", OTP_Bank );
        return EINVAL;
    }
    CAM_INFO( CAM_EEPROM, "read OTP offset: 0x%x", offset );

    for ( j = 1; j < block->num_map; j++ )
    {
        read_size	= emap[j].mem.valid_size;
        memptr		= block->mapdata + emap[j].mem.addr;
        addr		= emap[j].mem.addr + offset;

        CAM_INFO( CAM_EEPROM, "emap[%d / %d].mem.addr=0x%x OTP addr=0x%x read_size=0x%x mapdata=%pK memptr=%pK subdev=%d type=%d",
                  j, block->num_map, emap[j].mem.addr, addr, read_size, block->mapdata, memptr, e_ctrl->soc_info.index, e_ctrl->eeprom_device_type );

        cam_otp_hi847_burst_read( &e_ctrl->io_master_info, addr, memptr, read_size );
        memptr		+= read_size;
    }
    CAM_INFO( CAM_EEPROM, "read data done memptr=%pK VR:: End read_bytes=0x%x\n", memptr, read_bytes );

    /* OTP finish setting1 write */
    rc = camera_io_dev_write( &e_ctrl->io_master_info, &hi847_otp_finish_setting1 );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "load otp finish setfile1 failed" );
        return(rc);
    }

    msleep(10);

    /* OTP finish setting2 write */
    rc = camera_io_dev_write( &e_ctrl->io_master_info, &hi847_otp_finish_setting2 );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "load otp finish setfile2 failed" );
        return(rc);
    }

err:
    return(rc);
}

#endif
#if defined(CONFIG_HI1337_OTP)
static int cam_otp_hi1337_init( struct camera_io_master *io_master_info)
{
    int	rc = 0;

    if ( !io_master_info )
    {
        CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
        return(-EINVAL);
    }

    /* load otp global setfile */

    rc = camera_io_dev_write( io_master_info, &load_hi1337_otp_setfile );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "load otp globle setfile failed" );
        return(rc);
    }

    /* OTP initial setting1 write */
    rc = camera_io_dev_write( io_master_info, &hi1337_otp_init_setting1 );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "load otp initial setfile1 failed" );
        return(rc);
    }

    msleep(10);

    /* OTP initial setting2 write */
    rc = camera_io_dev_write( io_master_info, &hi1337_otp_init_setting2 );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "load otp initial setfile2 failed" );
        return(rc);
    }

    CAM_INFO( CAM_EEPROM, "load otp init setting done!");
    return rc;
}

static int cam_otp_hi1337_read( struct camera_io_master *io_master_info, uint32_t addr,
                                uint8_t *memptr )
{
    int					rc = 0;
    struct cam_sensor_i2c_reg_setting	i2c_reg_settings;
    struct cam_sensor_i2c_reg_array		i2c_reg_array;
    enum camera_sensor_i2c_type		addr_type	= CAMERA_SENSOR_I2C_TYPE_WORD;
    enum camera_sensor_i2c_type		data_type	= CAMERA_SENSOR_I2C_TYPE_BYTE;
    uint32_t				read_addr		= 0;

    if ( !io_master_info )
    {
        CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
        return(-EINVAL);
    }

    i2c_reg_settings.addr_type	= addr_type;
    i2c_reg_settings.data_type	= data_type;
    i2c_reg_settings.size		= 1;
    i2c_reg_settings.delay		= 4;
    i2c_reg_array.delay		= 4;

    /* high address */
    i2c_reg_array.reg_addr		= 0x030a;
    i2c_reg_array.reg_data		= (addr >> 8) & 0xff;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write high address failed" );
        goto err;
    }

    /* low address */
    i2c_reg_array.reg_addr		= 0x030b;
    i2c_reg_array.reg_data		= addr & 0xff;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write low address failed" );
        goto err;
    }

    i2c_reg_array.reg_addr		= 0x031C;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write otp signal 1 failed" );
        goto err;
    }
    i2c_reg_array.reg_addr		= 0x031D;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write otp signal 2 failed" );
        goto err;
    }
    /* OTP continue read mode */
    i2c_reg_array.reg_addr		= 0x0302;
    i2c_reg_array.reg_data		= 0x01;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }

    /* OTP data verify */
    rc = camera_io_dev_read( io_master_info, 0x030a, &read_addr, addr_type, addr_type, false);
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
    }
    if(read_addr != addr)
    CAM_INFO( CAM_EEPROM, "ERROR WRONG addr=0x%x read_addr=0x%x", addr, read_addr );

    CAM_INFO( CAM_EEPROM, "addr=0x%x read_addr=0x%x", addr, read_addr );
    /* OTP data read */
    rc = camera_io_dev_read_seq( io_master_info, 0x0308, memptr, addr_type, data_type, 1 );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
    }

    CAM_ERR( CAM_EEPROM, "addr=0x%x  read_addr=0x%x  *memptr=0x%x", addr, read_addr, *memptr );

err:
    return(rc);
}


static int cam_otp_hi1337_burst_read( struct camera_io_master *io_master_info, uint32_t addr,
                                      uint8_t *memptr, uint32_t read_size )
{
    int					rc = 0;
    struct cam_sensor_i2c_reg_setting	i2c_reg_settings;
    struct cam_sensor_i2c_reg_array		i2c_reg_array;
    enum camera_sensor_i2c_type		addr_type	= CAMERA_SENSOR_I2C_TYPE_WORD;
    enum camera_sensor_i2c_type		data_type	= CAMERA_SENSOR_I2C_TYPE_BYTE;
    uint32_t				read_addr		= 0;

    if ( !io_master_info )
    {
        CAM_ERR( CAM_EEPROM, "io_master_info is NULL" );
        return(-EINVAL);
    }

    i2c_reg_settings.addr_type	= addr_type;
    i2c_reg_settings.data_type	= data_type;
    i2c_reg_settings.size		= 1;
    i2c_reg_settings.delay		= 4;
    i2c_reg_array.delay		= 4;

    /* high address */
    i2c_reg_array.reg_addr		= 0x030a;
    i2c_reg_array.reg_data		= (addr >> 8) & 0xff;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write high address failed" );
        goto err;
    }

    /* low address */
    i2c_reg_array.reg_addr		= 0x030b;
    i2c_reg_array.reg_data		= addr & 0xff;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write low address failed" );
        goto err;
    }

    i2c_reg_array.reg_addr		= 0x031C;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write otp signal 1 failed" );
        goto err;
    }
    i2c_reg_array.reg_addr		= 0x031D;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "write otp signal 2 failed" );
        goto err;
    }
    /* OTP continue read mode */
    i2c_reg_array.reg_addr		= 0x0302;
    i2c_reg_array.reg_data		= 0x01;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }

    /* OTP data verify*/
    rc = camera_io_dev_read( io_master_info, 0x030a, &read_addr, addr_type, addr_type, false);
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
    }

    CAM_INFO( CAM_EEPROM, "CHECK ERROR addr=0x%x read_addr=0x%x", addr, read_addr );

    /* burst read on */
    i2c_reg_array.reg_addr		= 0x0712;
    i2c_reg_array.reg_data		= 0x01;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }

    /* OTP data burst read */
    rc = camera_io_dev_read_seq( io_master_info, 0x0308, memptr, addr_type, data_type, read_size );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read failed rc %d", rc );
    }

    /* burst read off */
    i2c_reg_array.reg_addr		= 0x0712;
    i2c_reg_array.reg_data		= 0x00;
    i2c_reg_settings.reg_setting	= &i2c_reg_array;

    rc = camera_io_dev_write( io_master_info, &i2c_reg_settings );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "continuous read failed" );
        goto err;
    }

err:
    return(rc);
}

int cam_otp_hi1337_read_memory( struct cam_eeprom_ctrl_t *e_ctrl,
                                       struct cam_eeprom_memory_block_t *block )

{
    struct cam_eeprom_memory_map_t	*emap	= block->map;
    struct cam_eeprom_soc_private	*eb_info;
    uint32_t	addr		= 0;
    uint32_t	read_size	= 0;
    uint32_t	offset = 0;
    uint8_t		OTP_Bank	= 0;
    uint8_t				*memptr = block->mapdata;
    int		read_bytes	= 0;
    int		rc	= 0;
    int		j	= 0;

    if ( !e_ctrl )
    {
        CAM_ERR( CAM_EEPROM, "e_ctrl is NULL" );
        return(-EINVAL);
    }

    eb_info = (struct cam_eeprom_soc_private *) e_ctrl->soc_info.soc_private;

    rc = cam_otp_hi1337_init(&e_ctrl->io_master_info);
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "OTP init failed" );
        goto err;
    }

    /* select bank */
    rc = cam_otp_hi1337_read( &e_ctrl->io_master_info, SENSOR_HI1337_OTP_BANK_SELECT_REGISTER, &OTP_Bank );

    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "read data failed" );
        goto err;
    }
    CAM_INFO( CAM_EEPROM, "current OTP_Bank: %d", OTP_Bank );

    switch ( OTP_Bank )
    {
    /* Refer to OTP document */
    case 0:
    case 1:
        offset = 0x0704;
        break;

    case 3:
        offset = 0x0D04;
        break;

    case 7:
        offset = 0x1304;
        break;

    case 0xF:
        offset = 0x1904;
        break;

    default:
        CAM_INFO( CAM_EEPROM, "Bank error : Bank(%d)", OTP_Bank );
        return EINVAL;
    }
    CAM_INFO( CAM_EEPROM, "read OTP offset: 0x%x", offset );

    for ( j = 1; j < block->num_map; j++ )
    {
        read_size	= emap[j].mem.valid_size;
        memptr		= block->mapdata + emap[j].mem.addr;
        addr		= emap[j].mem.addr + offset;

        CAM_INFO( CAM_EEPROM, "emap[%d / %d].mem.addr=0x%x OTP addr=0x%x read_size=0x%x mapdata=%pK memptr=%pK subdev=%d type=%d",
                  j, block->num_map, emap[j].mem.addr, addr, read_size, block->mapdata, memptr, e_ctrl->soc_info.index, e_ctrl->eeprom_device_type );

        cam_otp_hi1337_burst_read( &e_ctrl->io_master_info, addr, memptr, read_size );
        memptr		+= read_size;
    }
    CAM_INFO( CAM_EEPROM, "read data done memptr=%pK VR:: End read_bytes=0x%x\n", memptr, read_bytes );

    /* OTP finish setting1 write */
    rc = camera_io_dev_write( &e_ctrl->io_master_info, &hi1337_otp_finish_setting1 );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "load otp finish setfile1 failed" );
        return(rc);
    }

    msleep(10);

    /* OTP finish setting2 write */
    rc = camera_io_dev_write( &e_ctrl->io_master_info, &hi1337_otp_finish_setting2 );
    if ( rc < 0 )
    {
        CAM_ERR( CAM_EEPROM, "load otp finish setfile2 failed" );
        return(rc);
    }

err:
    return(rc);
}
#endif

#ifdef CAM_EEPROM_DBG_DUMP
int cam_sec_eeprom_dump(uint32_t subdev_id, uint8_t *mapdata, uint32_t addr, uint32_t size)
{
	int rc = 0;
	int j;

	if (mapdata == NULL) {
		CAM_ERR(CAM_EEPROM, "mapdata is NULL");
		return -1;
	}
	if (size == 0) {
		CAM_ERR(CAM_EEPROM, "size is 0");
		return -1;
	}

	CAM_INFO(CAM_EEPROM, "subdev_id: %d, eeprom dump addr = 0x%04X, total read size = %d", subdev_id, addr, size);
	for (j = 0; j < size; j++)
		CAM_INFO(CAM_EEPROM, "addr = 0x%04X, data = 0x%02X", addr+j, mapdata[addr+j]);

	return rc;
}
#endif

static int isValidIdx(eConfigNameInfoIdx confIdx, uint32_t *ConfAddr)
{
	if (confIdx >= MAX_CONFIG_INFO_IDX)
	{
		CAM_ERR(CAM_EEPROM, "invalid index: %d, max:%d", confIdx, MAX_CONFIG_INFO_IDX);
		return 0;
	}

	if (ConfigInfo[confIdx].isSet == 1)
	{
		*ConfAddr = ConfigInfo[confIdx].value;
		CAM_DBG(CAM_EEPROM, "%s: %d, isSet: %d, addr: 0x%08X",
			ConfigInfoStrs[confIdx], confIdx, ConfigInfo[confIdx].isSet,
			ConfigInfo[confIdx].value);

		return 1;
	}
	else
	{
		*ConfAddr = 0;
		CAM_DBG(CAM_EEPROM, "%s: %d, isSet: %d",
			ConfigInfoStrs[confIdx], confIdx, ConfigInfo[confIdx].isSet);

		return 0;
	}
}

static int cam_sec_eeprom_module_info_set_sensor_id(ModuleInfo_t *mInfo, eConfigNameInfoIdx idx, uint8_t *pMapData)
{
	char 	*sensorId = "";

	if ((mInfo == NULL || mInfo->mVer.sensor_id == NULL || mInfo->mVer.sensor2_id == NULL)
		|| (idx != ADDR_M_SENSOR_ID && idx != ADDR_S_SENSOR_ID))
	{
		CAM_ERR(CAM_EEPROM, "sensor_id is NULL");
		return 1;
	}

	if (idx == ADDR_S_SENSOR_ID)
		sensorId = mInfo->mVer.sensor2_id;
	else
		sensorId = mInfo->mVer.sensor_id;

	memcpy(sensorId, pMapData, FROM_SENSOR_ID_SIZE);
	sensorId[FROM_SENSOR_ID_SIZE] = '\0';

	CAM_INFO(CAM_EEPROM,
		"%s sensor_id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		mInfo->typeStr,
		sensorId[0], sensorId[1], sensorId[2], sensorId[3],
		sensorId[4], sensorId[5], sensorId[6], sensorId[7],
		sensorId[8], sensorId[9], sensorId[10], sensorId[11],
		sensorId[12], sensorId[13], sensorId[14], sensorId[15]);

	return 0;
}

static int cam_sec_eeprom_module_info_set_module_id(ModuleInfo_t *mInfo, uint8_t *pMapData)
{
	char 	*moduleId = "";

	if (mInfo == NULL || mInfo->mVer.module_id == NULL)
	{
		CAM_ERR(CAM_EEPROM, "module_id is NULL");
		return 1;
	}
	moduleId = mInfo->mVer.module_id;

	memcpy(moduleId, pMapData, FROM_MODULE_ID_SIZE);
	moduleId[FROM_MODULE_ID_SIZE] = '\0';

	CAM_DBG(CAM_EEPROM, "%s module_id = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
		mInfo->typeStr,
		moduleId[0], moduleId[1], moduleId[2], moduleId[3],
		moduleId[4], moduleId[5], moduleId[6], moduleId[7],
		moduleId[8], moduleId[9]);

	return 0;
}

static int cam_sec_eeprom_module_info_set_load_version(int rev, struct cam_eeprom_ctrl_t *e_ctrl, ModuleInfo_t *mInfo)
{
	int         rc                  = 0;
	int         i                   = 0;

	uint8_t     loadfrom            = 'N';
	uint8_t     sensor_ver[2]       = {0,};
	uint8_t     dll_ver[2]          = {0,};
	uint32_t    normal_is_supported = 0;
	uint8_t     normal_cri_rev      = 0;
	uint8_t     bVerNull            = FALSE;

	uint32_t    ConfIdx             = 0;
	uint32_t    ConfAddr            = 0;

	char        cal_ver[FROM_MODULE_FW_INFO_SIZE+1]  = "";
	char        ideal_ver[FROM_MODULE_FW_INFO_SIZE+1] = "";

	char        *module_fw_ver;
	char        *load_fw_ver;
	char        *phone_fw_ver;

	if ((e_ctrl == NULL) || (e_ctrl->cal_data.mapdata == NULL)) {
		CAM_ERR(CAM_EEPROM, "e_ctrl is NULL");
		return -EINVAL;
	}

	if ((mInfo == NULL) || (mInfo->mVer.sensor_id == NULL))
	{
		CAM_ERR(CAM_EEPROM, "invalid argument");
		rc = 1;
		return rc;
	}

	module_fw_ver = mInfo->mVer.module_fw_ver;
	phone_fw_ver = mInfo->mVer.phone_fw_ver;
	load_fw_ver = mInfo->mVer.load_fw_ver;

	memset(module_fw_ver, 0x00, FROM_MODULE_FW_INFO_SIZE+1);
	memset(phone_fw_ver, 0x00, FROM_MODULE_FW_INFO_SIZE+1);
	memset(load_fw_ver, 0x00, FROM_MODULE_FW_INFO_SIZE+1);

	if (isValidIdx(ADDR_M_CALMAP_VER, &ConfAddr) == 1) {
		ConfAddr += 0x03;
		mInfo->mapVer = e_ctrl->cal_data.mapdata[ConfAddr];
	}

	if (mInfo->mapVer >= 0x80 || !isalnum(mInfo->mapVer)) {
		CAM_INFO(CAM_EEPROM, "subdev_id: %d, map version = 0x%x", mInfo->type, mInfo->mapVer);
		mInfo->mapVer = '0';
	} else {
		CAM_INFO(CAM_EEPROM, "subdev_id: %d, map version = %c [0x%x]", mInfo->type, mInfo->mapVer, mInfo->mapVer);
	}

	if (mInfo->M_or_S == MAIN_MODULE) {
		ConfIdx = ADDR_M_FW_VER;
	} else {
		ConfIdx = ADDR_S_FW_VER;
	}

	if (isValidIdx(ConfIdx, &ConfAddr) == 1)
	{
		memcpy(module_fw_ver, &e_ctrl->cal_data.mapdata[ConfAddr], FROM_MODULE_FW_INFO_SIZE);
		module_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"%s manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			mInfo->typeStr,
			module_fw_ver[0], module_fw_ver[1], module_fw_ver[2], module_fw_ver[3], module_fw_ver[4],
			module_fw_ver[5], module_fw_ver[6], module_fw_ver[7], module_fw_ver[8], module_fw_ver[9],
			module_fw_ver[10]);

		/* temp phone version */
		snprintf(phone_fw_ver, FROM_MODULE_FW_INFO_SIZE+1, "%s%s%s%s",
			mInfo->mVer.phone_hw_info, mInfo->mVer.phone_sw_info,
			mInfo->mVer.phone_vendor_info, mInfo->mVer.phone_process_info);
		phone_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM,
			"%s phone info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			mInfo->typeStr,
			phone_fw_ver[0], phone_fw_ver[1], phone_fw_ver[2], phone_fw_ver[3], phone_fw_ver[4],
			phone_fw_ver[5], phone_fw_ver[6], phone_fw_ver[7], phone_fw_ver[8], phone_fw_ver[9],
			phone_fw_ver[10]);
	}

#if defined(CONFIG_SAMSUNG_REAR_BOKEH)
	if (mInfo->type == SEC_WIDE_SENSOR) {
		ConfIdx = ADDR_CUSTOM_FW_VER;
		memset(bokeh_module_fw_ver,0x00,sizeof(bokeh_module_fw_ver));
		if (isValidIdx(ConfIdx, &ConfAddr) == 1)
		{
			memcpy(bokeh_module_fw_ver, &e_ctrl->cal_data.mapdata[ConfAddr], FROM_MODULE_FW_INFO_SIZE);
			bokeh_module_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM,
					"[BOKEH]%s manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
					mInfo->typeStr,
					bokeh_module_fw_ver[0], bokeh_module_fw_ver[1], bokeh_module_fw_ver[2], bokeh_module_fw_ver[3], bokeh_module_fw_ver[4],
					bokeh_module_fw_ver[5], bokeh_module_fw_ver[6], bokeh_module_fw_ver[7], bokeh_module_fw_ver[8], bokeh_module_fw_ver[9],
					bokeh_module_fw_ver[10]);
		}
		ConfIdx = ADDR_CUSTOM_SENSOR_ID;
		memset(sensor_id[EEP_REAR3],0x00,sizeof(sensor_id[EEP_REAR3]));
		if (isValidIdx(ConfIdx, &ConfAddr) == 1)
		{
			memcpy(sensor_id[EEP_REAR3], &e_ctrl->cal_data.mapdata[ConfAddr], FROM_SENSOR_ID_SIZE);
			sensor_id[EEP_REAR3][FROM_SENSOR_ID_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM,
					"[BOKEH]%s sensor_id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
					mInfo->typeStr,
					sensor_id[EEP_REAR3][0], sensor_id[EEP_REAR3][1], sensor_id[EEP_REAR3][2], sensor_id[EEP_REAR3][3],
					sensor_id[EEP_REAR3][4], sensor_id[EEP_REAR3][5], sensor_id[EEP_REAR3][6], sensor_id[EEP_REAR3][7],
					sensor_id[EEP_REAR3][8], sensor_id[EEP_REAR3][9], sensor_id[EEP_REAR3][10], sensor_id[EEP_REAR3][11],
					sensor_id[EEP_REAR3][12], sensor_id[EEP_REAR3][13], sensor_id[EEP_REAR3][14], sensor_id[EEP_REAR3][15]);
		}
	}
	//fill rear3 fw info
	sprintf(cam_fw_ver[EEP_REAR3], "%s %s\n", bokeh_module_fw_ver, bokeh_module_fw_ver);
	sprintf(cam_fw_full_ver[EEP_REAR3], "%s %s %s\n", bokeh_module_fw_ver, bokeh_module_fw_ver,bokeh_module_fw_ver);
#endif

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
	if (mInfo->type == SEC_TELE_SENSOR) {
		ConfIdx = ADDR_M_FW_VER;
		memset(rear3_module_fw_ver, 0x00, sizeof(rear3_module_fw_ver));
		if (isValidIdx(ConfIdx, &ConfAddr) == 1)
		{
			memcpy(rear3_module_fw_ver, &e_ctrl->cal_data.mapdata[ConfAddr], FROM_MODULE_FW_INFO_SIZE);
			rear3_module_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM,
					"[TRIPLE]%s manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
					mInfo->typeStr,
					rear3_module_fw_ver[0], rear3_module_fw_ver[1], rear3_module_fw_ver[2], rear3_module_fw_ver[3], rear3_module_fw_ver[4],
					rear3_module_fw_ver[5], rear3_module_fw_ver[6], rear3_module_fw_ver[7], rear3_module_fw_ver[8], rear3_module_fw_ver[9],
					rear3_module_fw_ver[10]);
		}

		ConfIdx = ADDR_M_SENSOR_ID;
		memset(sensor_id[EEP_REAR3], 0x00, sizeof(sensor_id[EEP_REAR3]));
		if (isValidIdx(ConfIdx, &ConfAddr) == 1)
		{
			memcpy(sensor_id[EEP_REAR3], &e_ctrl->cal_data.mapdata[ConfAddr], FROM_SENSOR_ID_SIZE);
			sensor_id[EEP_REAR3][FROM_SENSOR_ID_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM,
					"[TRIPLE]%s sensor_id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
					mInfo->typeStr,
					sensor_id[EEP_REAR3][0], sensor_id[EEP_REAR3][1], sensor_id[EEP_REAR3][2], sensor_id[EEP_REAR3][3],
					sensor_id[EEP_REAR3][4], sensor_id[EEP_REAR3][5], sensor_id[EEP_REAR3][6], sensor_id[EEP_REAR3][7],
					sensor_id[EEP_REAR3][8], sensor_id[EEP_REAR3][9], sensor_id[EEP_REAR3][10], sensor_id[EEP_REAR3][11],
					sensor_id[EEP_REAR3][12], sensor_id[EEP_REAR3][13], sensor_id[EEP_REAR3][14], sensor_id[EEP_REAR3][15]);
		}
	}

	//fill rear3 fw info
	sprintf(cam_fw_ver[EEP_REAR3], "%s %s\n", rear3_module_fw_ver, rear3_module_fw_ver);
	sprintf(cam_fw_full_ver[EEP_REAR3], "%s %s %s\n", rear3_module_fw_ver, rear3_module_fw_ver, rear3_module_fw_ver);
#endif

#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
	if (mInfo->type == SEC_TELE2_SENSOR) {
		ConfIdx = ADDR_M_FW_VER;
		memset(rear4_module_fw_ver, 0x00, sizeof(rear4_module_fw_ver));
		if (isValidIdx(ConfIdx, &ConfAddr) == 1)
		{
			memcpy(rear4_module_fw_ver, &e_ctrl->cal_data.mapdata[ConfAddr], FROM_MODULE_FW_INFO_SIZE);
			rear4_module_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM,
					"[QUADRA]%s manufacturer info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
					mInfo->typeStr,
					rear4_module_fw_ver[0], rear4_module_fw_ver[1], rear4_module_fw_ver[2], rear4_module_fw_ver[3], rear4_module_fw_ver[4],
					rear4_module_fw_ver[5], rear4_module_fw_ver[6], rear4_module_fw_ver[7], rear4_module_fw_ver[8], rear4_module_fw_ver[9],
					rear4_module_fw_ver[10]);
		}

		ConfIdx = ADDR_M_SENSOR_ID;
		memset(sensor_id[EEP_REAR4], 0x00, sizeof(sensor_id[EEP_REAR4]));
		if (isValidIdx(ConfIdx, &ConfAddr) == 1)
		{
			memcpy(sensor_id[EEP_REAR4], &e_ctrl->cal_data.mapdata[ConfAddr], FROM_SENSOR_ID_SIZE);
			sensor_id[EEP_REAR4][FROM_SENSOR_ID_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM,
					"[TRIPLE]%s sensor_id = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
					mInfo->typeStr,
					sensor_id[EEP_REAR4][0], sensor_id[EEP_REAR4][1], sensor_id[EEP_REAR4][2], sensor_id[EEP_REAR4][3],
					sensor_id[EEP_REAR4][4], sensor_id[EEP_REAR4][5], sensor_id[EEP_REAR4][6], sensor_id[EEP_REAR4][7],
					sensor_id[EEP_REAR4][8], sensor_id[EEP_REAR4][9], sensor_id[EEP_REAR4][10], sensor_id[EEP_REAR4][11],
					sensor_id[EEP_REAR4][12], sensor_id[EEP_REAR4][13], sensor_id[EEP_REAR4][14], sensor_id[EEP_REAR4][15]);
		}
	}

	//fill rear4 fw info
	sprintf(cam_fw_ver[EEP_REAR4], "%s %s\n", rear4_module_fw_ver, rear4_module_fw_ver);
	sprintf(cam_fw_full_ver[EEP_REAR4], "%s %s %s\n", rear4_module_fw_ver, rear4_module_fw_ver, rear4_module_fw_ver);
#endif

	/* temp load version */
	if (mInfo->type == SEC_WIDE_SENSOR && mInfo->M_or_S == MAIN_MODULE &&
		strncmp(phone_fw_ver, module_fw_ver, HW_INFO_MAX_SIZE-1) == 0 &&
		strncmp(&phone_fw_ver[HW_INFO_MAX_SIZE-1], &module_fw_ver[HW_INFO_MAX_SIZE-1], SW_INFO_MAX_SIZE-1) >= 0)
	{
		CAM_INFO(CAM_EEPROM, "Load from phone");
		strcpy(load_fw_ver, phone_fw_ver);
		loadfrom = 'P';
	}
	else
	{
		CAM_INFO(CAM_EEPROM, "Load from EEPROM");
		strcpy(load_fw_ver, module_fw_ver);
		loadfrom = 'E';
	}

	//	basically, cal_ver is the version when the module is calibrated.
	//	It can be different in case that the module_fw_ver is updated by FW on F-ROM for testing.
	//	otherwise, module_fw_ver and cal_ver should be the same.
	if (mInfo->M_or_S == MAIN_MODULE) {
		ConfIdx = ADDR_M_FW_VER;
	} else {
		ConfIdx = ADDR_S_FW_VER;
	}

	if (isValidIdx(ConfIdx, &ConfAddr) == 1)
	{
		bVerNull = FALSE;
		for(i = 0; i < FROM_MODULE_FW_INFO_SIZE; i ++) {
			if (e_ctrl->cal_data.mapdata[ConfAddr + i] >= 0x80 || !isalnum(e_ctrl->cal_data.mapdata[ConfAddr + i])) {
				cal_ver[i] = ' ';
				bVerNull = TRUE;
			} else {
				cal_ver[i] = e_ctrl->cal_data.mapdata[ConfAddr + i];
			}

			if (phone_fw_ver[i] >= 0x80 || !isalnum(phone_fw_ver[i]))
			{
				phone_fw_ver[i] = ' ';
			}
		}
	}

	if (isValidIdx(ADDR_M_MODULE_ID, &ConfAddr) == 1)
	{
		ConfAddr += 0x06;
		cam_sec_eeprom_module_info_set_module_id(mInfo, &e_ctrl->cal_data.mapdata[ConfAddr]);
	}

	sensor_ver[0] = 0;
	sensor_ver[1] = 0;
	dll_ver[0] = 0;
	dll_ver[1] = 0;

	ConfIdx = ADDR_M_SENSOR_ID;
	if (isValidIdx(ConfIdx, &ConfAddr) == 1)
	{
		cam_sec_eeprom_module_info_set_sensor_id(mInfo, ConfIdx, &e_ctrl->cal_data.mapdata[ConfAddr]);
		sensor_ver[0] = mInfo->mVer.sensor_id[8];
	}

	if (isValidIdx(ADDR_M_DLL_VER, &ConfAddr) == 1)
	{
		ConfAddr += 0x03;
		dll_ver[0] = e_ctrl->cal_data.mapdata[ConfAddr] - '0';
	}

	normal_is_supported = e_ctrl->camera_normal_cal_crc;

	if (isValidIdx(DEF_M_CHK_VER, &ConfAddr) == 1)
	{
		normal_cri_rev = CriterionRev;
	}

	strcpy(ideal_ver, phone_fw_ver);
	if (module_fw_ver[9] < 0x80 && isalnum(module_fw_ver[9])) {
		ideal_ver[9] = module_fw_ver[9];
	}
	if (module_fw_ver[10] < 0x80 && isalnum(module_fw_ver[10])) {
		ideal_ver[10] = module_fw_ver[10];
	}

	if (rev < normal_cri_rev && bVerNull == TRUE)
	{
		strcpy(cal_ver, ideal_ver);
		loadfrom = 'P';
		CAM_ERR(CAM_EEPROM, "set tmp ver: %s", cal_ver);
	}

	snprintf(mInfo->mVer.module_info, SYSFS_MODULE_INFO_SIZE, "SSCAL %c%s%04X%04XR%02dM%cD%02XD%02XS%02XS%02X/%s%04X%04XR%02d",
		loadfrom, cal_ver, (e_ctrl->is_supported >> 16) & 0xFFFF, e_ctrl->is_supported & 0xFFFF,
		rev & 0xFF, mInfo->mapVer, dll_ver[0] & 0xFF, dll_ver[1] & 0xFF, sensor_ver[0] & 0xFF, sensor_ver[1] & 0xFF,
		ideal_ver, (normal_is_supported >> 16) & 0xFFFF, normal_is_supported & 0xFFFF, normal_cri_rev);
#ifdef CAM_EEPROM_DBG
	CAM_DBG(CAM_EEPROM, "%s info = %s", mInfo->typeStr, mInfo->mVer.module_info);
#endif

	/* update EEPROM fw version on sysfs */
	// if (mInfo->type != SEC_WIDE_SENSOR)
	{
		strncpy(load_fw_ver, module_fw_ver, FROM_MODULE_FW_INFO_SIZE);
		load_fw_ver[FROM_MODULE_FW_INFO_SIZE] = '\0';
		sprintf(phone_fw_ver, "N");
	}

	//	tele module
	if (mInfo->type == SEC_WIDE_SENSOR && mInfo->M_or_S != MAIN_MODULE)
	{
		sprintf(phone_fw_ver, "N");
	}

	sprintf(mInfo->mVer.cam_fw_ver, "%s %s\n", module_fw_ver, load_fw_ver);
	sprintf(mInfo->mVer.cam_fw_full_ver, "%s %s %s\n", module_fw_ver, phone_fw_ver, load_fw_ver);

#ifdef CAM_EEPROM_DBG
	CAM_DBG(CAM_EEPROM, "%s manufacturer info = %c %c %c %c %c %c %c %c %c %c %c",
		mInfo->typeStr,
		module_fw_ver[0], module_fw_ver[1], module_fw_ver[2], module_fw_ver[3], module_fw_ver[4],
		module_fw_ver[5], module_fw_ver[6], module_fw_ver[7], module_fw_ver[8], module_fw_ver[9],
		module_fw_ver[10]);

	CAM_DBG(CAM_EEPROM, "%s phone_fw_ver = %c %c %c %c %c %c %c %c %c %c %c",
		mInfo->typeStr,
		phone_fw_ver[0], phone_fw_ver[1], phone_fw_ver[2], phone_fw_ver[3], phone_fw_ver[4],
		phone_fw_ver[5], phone_fw_ver[6], phone_fw_ver[7], phone_fw_ver[8], phone_fw_ver[9],
		phone_fw_ver[10]);

	CAM_DBG(CAM_EEPROM, "%s load_fw_ver = %c %c %c %c %c %c %c %c %c %c %c",
		mInfo->typeStr,
		load_fw_ver[0], load_fw_ver[1], load_fw_ver[2], load_fw_ver[3], load_fw_ver[4],
		load_fw_ver[5], load_fw_ver[6], load_fw_ver[7], load_fw_ver[8], load_fw_ver[9],
		load_fw_ver[10]);
#endif

	return rc;
}

#if defined(CONFIG_SAMSUNG_REAR_DUAL) || defined(CONFIG_SAMSUNG_REAR_TRIPLE) || defined(CONFIG_SAMSUNG_REAR_TOF) || defined(CONFIG_SAMSUNG_FRONT_DUAL) || defined(CONFIG_SAMSUNG_FRONT_TOF) || defined(CONFIG_SAMSUNG_REAR_QUADRA)
static int cam_sec_eeprom_module_info_set_dual_tilt(eDualTiltMode tiltMode, uint32_t dual_addr_idx,
	uint32_t dual_size_idx, uint8_t *pMapData, char *log_str,
	ModuleInfo_t *mInfo)
{
	uint32_t offset_dll_ver          = 0;
	uint32_t offset_x                = 0;
	uint32_t offset_y                = 0;
	uint32_t offset_z                = 0;
	uint32_t offset_sx               = 0;
	uint32_t offset_sy               = 0;
	uint32_t offset_range            = 0;
	uint32_t offset_max_err          = 0;
	uint32_t offset_avg_err          = 0;
	uint32_t offset_project_cal_type = 0;

	uint8_t    *dual_cal;
	DualTilt_t *dual_tilt;

	uint32_t addr = 0;
	uint32_t size = 0;
	uint8_t  var  = 0;

	if (mInfo == NULL || mInfo->mVer.dual_cal == NULL || mInfo->mVer.DualTilt == NULL)
	{
		CAM_ERR(CAM_EEPROM, "dual_cal or DualTilt is NULL");
		return 1;
	}

	dual_cal = mInfo->mVer.dual_cal;
	dual_tilt = mInfo->mVer.DualTilt;
	memset(dual_tilt, 0x00, sizeof(DualTilt_t));

	if (isValidIdx(dual_addr_idx, &addr) == 1 && isValidIdx(dual_size_idx, &size) == 1)
	{
		switch (tiltMode)
		{
			case DUAL_TILT_REAR_WIDE:
			case DUAL_TILT_REAR_UW:
				offset_dll_ver          = 0x0000;
				offset_x                = 0x0060;
				offset_y                = 0x0064;
				offset_z                = 0x0068;
				offset_sx               = 0x00C0;
				offset_sy               = 0x00C4;
				offset_range            = 0x07E0;
				offset_max_err          = 0x07E4;
				offset_avg_err          = 0x07E8;
				offset_project_cal_type = 0x0108;
#if defined(CONFIG_SEC_B5Q_PROJECT)
				offset_dll_ver			= 0x007A;
				offset_x				= 0x00B8;
				offset_y				= 0x00BC;
				offset_z				= 0x00C0;
				offset_sx				= 0x00DC;
				offset_sy				= 0x00E0;
				offset_range			= 0x02D2;
				offset_max_err			= 0x02D6;
				offset_avg_err			= 0x02DA;
				offset_project_cal_type = 0x02DE;
#endif

				break;

			case DUAL_TILT_FRONT:
				offset_dll_ver          = 0x007A;
				offset_x                = 0x00B8;
				offset_y                = 0x00BC;
				offset_z                = 0x00C0;
				offset_sx               = 0x00DC;
				offset_sy               = 0x00E0;
				offset_range            = 0x02D2;
				offset_max_err          = 0x02D6;
				offset_avg_err          = 0x02DA;
				offset_project_cal_type = 0x02DE;
				break;

			case DUAL_TILT_TOF_REAR:
				offset_dll_ver = 0x0000;
				offset_x       = 0x006C;
				offset_y       = 0x0070;
				offset_z       = 0x0074;
				offset_sx      = 0x03C0;
				offset_sy      = 0x03C4;
				offset_range   = 0x04E0;
				offset_max_err = 0x04E4;
				offset_avg_err = 0x04E8;
				break;

			case DUAL_TILT_TOF_REAR2:
				offset_dll_ver = 0x0000;
				offset_x       = 0x0160;
				offset_y       = 0x0164;
				offset_z       = 0x0168;
				offset_sx      = 0x05C8;
				offset_sy      = 0x05CC;
				offset_range   = 0x06E8;
				offset_max_err = 0x06EC;
				offset_avg_err = 0x06F0;
				break;

			case DUAL_TILT_TOF_FRONT:
				offset_dll_ver = 0x07F4;
				offset_x       = 0x04B8;
				offset_y       = 0x04BC;
				offset_z       = 0x04C0;
				offset_sx      = 0x04DC;
				offset_sy      = 0x04E0;
				offset_range   = 0x07EC;
				offset_max_err = 0x07E8;
				offset_avg_err = 0x07E4;
				break;

			case DUAL_TILT_REAR_TELE:
				offset_dll_ver          = 0x02F4;
				offset_x                = 0x00B8;
				offset_y                = 0x00BC;
				offset_z                = 0x00C0;
				offset_sx               = 0x00DC;
				offset_sy               = 0x00E0;
				offset_range            = 0x02D2;
				offset_max_err          = 0x02D6;
				offset_avg_err          = 0x02DA;
				offset_project_cal_type = 0x02DF;
				break;

			default:
				break;
		}

		memcpy(dual_cal, &pMapData[addr], size);
		dual_cal[size] = '\0';
		CAM_INFO(CAM_EEPROM, "%s dual cal = %p", log_str, dual_cal);

		/* dual tilt */
		memcpy(&dual_tilt->dll_ver, &pMapData[addr + offset_dll_ver], 4);
		memcpy(&dual_tilt->x,       &pMapData[addr + offset_x], 4);
		memcpy(&dual_tilt->y,       &pMapData[addr + offset_y], 4);
		memcpy(&dual_tilt->z,       &pMapData[addr + offset_z], 4);
		memcpy(&dual_tilt->sx,      &pMapData[addr + offset_sx], 4);
		memcpy(&dual_tilt->sy,      &pMapData[addr + offset_sy], 4);
		memcpy(&dual_tilt->range,   &pMapData[addr + offset_range], 4);
		memcpy(&dual_tilt->max_err, &pMapData[addr + offset_max_err], 4);
		memcpy(&dual_tilt->avg_err, &pMapData[addr + offset_avg_err], 4);

		sprintf(dual_tilt->project_cal_type, "NONE");
		if (offset_project_cal_type)
		{
			for (var = 0; var < PROJECT_CAL_TYPE_MAX_SIZE; var++)
			{
				if ((pMapData[addr + offset_project_cal_type] == 0xFF) && (var == 0))
				{
					break;
				}

				if ((pMapData[addr + offset_project_cal_type+var] == 0xFF) || (pMapData[addr + offset_project_cal_type+var] == 0x00))
				{
					dual_tilt->project_cal_type[var] = '\0';
					break;
				}
				memcpy(&dual_tilt->project_cal_type[var], &pMapData[addr + offset_project_cal_type+var], 1);
			}
		}

		CAM_DBG(CAM_EEPROM,
			"%s dual tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d, project_cal_type=%s",
			log_str,
			dual_tilt->x, dual_tilt->y, dual_tilt->z, dual_tilt->sx,
			dual_tilt->sy, dual_tilt->range, dual_tilt->max_err,
			dual_tilt->avg_err, dual_tilt->dll_ver, dual_tilt->project_cal_type);
	}
	else
	{
		CAM_ERR(CAM_EEPROM,
			"isSet addr: %d, size: %d", ConfigInfo[dual_addr_idx].isSet, ConfigInfo[dual_addr_idx].isSet);
	}

	return 0;
}
#endif

static int cam_sec_eeprom_module_info_set_paf(uint32_t dual_addr_idx,
	uint32_t st_offset, uint32_t mid_far_size, uint8_t *pMapData, char *log_str,
	char *paf_cal, uint32_t paf_cal_size)
{
	int      i, step, offset = 0, cnt = 0;
	uint32_t size;
	uint32_t st_addr = 0;
	uint32_t size_offset = 1;

	if (mid_far_size == PAF_MID_SIZE)
		step = 8;
	else
		step = 2;

	size = mid_far_size/step;
	if (size > size_offset)
	{
		size = size - size_offset;
	}
	else
	{
		CAM_ERR(CAM_EEPROM, "mid_far_size was wrong mid_far_size = %d", mid_far_size);
		return 0;
	}

	CAM_DBG(CAM_EEPROM, "paf_cal: %p, paf_cal_size: %d", paf_cal, paf_cal_size);
	if (isValidIdx(dual_addr_idx, &st_addr) == 1)
	{
		st_addr += st_offset;

		memset(paf_cal, 0, paf_cal_size);

		for (i = 0; i < size; i++) {
			cnt = scnprintf(&paf_cal[offset], paf_cal_size-offset,
				"%d,", *((s16 *)&pMapData[st_addr + step*i]));
			offset += cnt;
		}

		cnt = scnprintf(&paf_cal[offset], paf_cal_size-offset,
			"%d\n", *((s16 *)&pMapData[st_addr + step*i]));
		offset += cnt;
		paf_cal[offset] = '\0';

		CAM_DBG(CAM_EEPROM, "%s = %s", log_str, paf_cal);
	}
	else
	{
		CAM_DBG(CAM_EEPROM,
			"isSet addr: %d, size: %d", ConfigInfo[dual_addr_idx].isSet, ConfigInfo[dual_addr_idx].isSet);
	}

	return 0;
}

static int cam_sec_eeprom_module_info_set_afcal(uint32_t st_addr_idx, AfIdx_t *af_idx, uint32_t num_idx,
	uint8_t *pMapData, char *af_cal_str, uint32_t af_cal_str_size)
{
	int  i, offset = 0, cnt = 0;
	uint32_t tempval;
	uint32_t st_addr = 0;
	int  len = 0;

	CAM_INFO(CAM_EEPROM, "st_addr_idx: 0x%04X, af_cal_str = %s", st_addr_idx, af_cal_str);

	if (isValidIdx(st_addr_idx, &st_addr) == 1)
	{
		memset(af_cal_str, 0, af_cal_str_size);

		for(i = 0; i < num_idx; i ++)
		{
			memcpy(&tempval, &pMapData[st_addr + af_idx[i].offset], 4);

			cnt = scnprintf(&af_cal_str[offset], af_cal_str_size-offset, "%d ", tempval);
			offset += cnt;
		}
		af_cal_str[offset] = '\0';

		len = strlen(af_cal_str);
		if (af_cal_str[len-1] == ' ')
			af_cal_str[len-1] = '\0';

		CAM_INFO(CAM_EEPROM, "af_cal_str = %s", af_cal_str);
	}
	else
	{
		CAM_ERR(CAM_EEPROM,
			"isSet addr: %d", ConfigInfo[st_addr_idx].isSet);
	}

	return 0;
}

#if defined(CONFIG_SAMSUNG_REAR_TOF) || defined(CONFIG_SAMSUNG_FRONT_TOF)
static int cam_sec_eeprom_module_info_tof(uint8_t *pMapData, char *log_str,
	uint8_t *tof_cal, uint8_t *tof_extra_cal, int *tof_uid, uint8_t *tof_cal_res,
	int *tof_cal_500, int *tof_cal_300)
{
	uint32_t st_addr        = 0;
	uint32_t uid_addr       = 0;
	uint32_t res_addr       = 0;
	uint32_t cal_size       = 0;
	uint32_t cal_extra_size = 0;
	uint32_t validation_500 = 0;
	uint32_t validation_300 = 0;

	if (isValidIdx(ADDR_TOFCAL_START, &st_addr) == 1 &&
		isValidIdx(ADDR_TOFCAL_SIZE, &cal_size) == 1 &&
		isValidIdx(ADDR_TOFCAL_UID, &uid_addr) == 1 &&
		isValidIdx(ADDR_TOFCAL_RESULT, &res_addr) == 1 &&
		isValidIdx(ADDR_VALIDATION_500, &validation_500) == 1 &&
		isValidIdx(ADDR_VALIDATION_300, &validation_300) == 1)
	{
		cal_extra_size = cal_size - TOFCAL_SIZE;
		CAM_INFO(CAM_EEPROM, "%s tof cal_size: %d, cal_extra_size: %d", log_str, cal_size, cal_extra_size);

		memcpy(tof_uid, &pMapData[uid_addr], 4);
		CAM_DBG(CAM_EEPROM, "%s tof uid = 0x%04X", log_str, *tof_uid);

		memcpy(tof_cal_500, &pMapData[validation_500], 2);
		CAM_DBG(CAM_EEPROM, "%s tof 500mm validation data = 0x%04X", log_str, *tof_cal_500);

		memcpy(tof_cal_300, &pMapData[validation_300], 2);
		CAM_DBG(CAM_EEPROM, "%s tof 300mm validation data = 0x%04X", log_str, *tof_cal_300);

		memcpy(tof_cal, &pMapData[st_addr], TOFCAL_SIZE);
		tof_cal[TOFCAL_SIZE] = '\0';
		CAM_DBG(CAM_EEPROM, "%s tof cal = %s", log_str, tof_cal);

		memcpy(tof_extra_cal, &pMapData[st_addr + TOFCAL_SIZE], cal_extra_size);
		tof_extra_cal[cal_extra_size] = '\0';
		CAM_DBG(CAM_EEPROM, "%s tof ext = %s", log_str, tof_extra_cal);

		CAM_DBG(CAM_EEPROM, "%s tof RESULT_ADDR 0x%x 0x%x 0x%x",
			log_str,
			pMapData[res_addr],
			pMapData[res_addr + 2],
			pMapData[res_addr + 4]);

		if (pMapData[res_addr] == 0x11 &&
			pMapData[res_addr + 2] == 0x11 &&
			pMapData[res_addr + 4] == 0x11)
		{
			*tof_cal_res = 1;
		}
	}
	else
	{
		CAM_ERR(CAM_EEPROM, "start: %d, end: %d, uid: %d, result: %d",
			ConfigInfo[ADDR_TOFCAL_START].isSet,
			ConfigInfo[ADDR_TOFCAL_SIZE].isSet,
			ConfigInfo[ADDR_TOFCAL_UID].isSet,
			ConfigInfo[ADDR_TOFCAL_RESULT].isSet);
	}

	return 0;
}
#endif

int cam_sec_eeprom_update_module_info(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int             rc = 0;

	uint32_t        ConfAddr 	  = 0;
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE) || defined(CONFIG_SAMSUNG_REAR_TOF) || defined(CONFIG_SAMSUNG_FRONT_TOF) || defined(CONFIG_SAMSUNG_REAR_QUADRA)
	uint32_t        ConfSize      = 0;
#endif

	ModuleInfo_t 	mInfo;
	ModuleInfo_t 	mInfoSub;

#if 0//TEMP_8350
	unsigned int rev = sec_hw_rev();
#else
	unsigned int rev = 0;
#endif

	CAM_INFO(CAM_EEPROM, "E");

	if (!e_ctrl) {
		CAM_ERR(CAM_EEPROM, "e_ctrl is NULL");
		return -EINVAL;
	}

	if (e_ctrl->soc_info.index >= SEC_SENSOR_ID_MAX) {
		CAM_ERR(CAM_EEPROM, "subdev_id: %d is not supported", e_ctrl->soc_info.index);
		return 0;
	}

	memset(&mInfo, 0x00, sizeof(ModuleInfo_t));
	memset(&mInfoSub, 0x00, sizeof(ModuleInfo_t));

	switch(e_ctrl->soc_info.index)
	{
		case SEC_WIDE_SENSOR:
			cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_REAR);
			mInfo.mVer.cam_cal_ack             = cam_cal_check[EEP_REAR];
			break;

#if defined(CONFIG_SEC_DM1Q_PROJECT)|| defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
		case SEC_TELE_SENSOR:
			cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_REAR3);
			mInfo.mVer.cam_cal_ack			   = cam_cal_check[EEP_REAR3];
			break;
#endif

#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
		case SEC_TELE2_SENSOR:
			cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_REAR4);
			mInfo.mVer.cam_cal_ack             = cam_cal_check[EEP_REAR4];
			break;
#endif

		case SEC_FRONT_SENSOR:
			cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_FRONT);
			mInfo.mVer.cam_cal_ack             = cam_cal_check[EEP_FRONT];

			break;

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
		case SEC_FRONT_AUX1_SENSOR:
			cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_FRONT2);
			mInfo.mVer.cam_cal_ack             = cam_cal_check[EEP_FRONT2];
			break;
#endif

		case SEC_ULTRA_WIDE_SENSOR:
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
			cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_REAR2);
			mInfo.mVer.cam_cal_ack             = cam_cal_check[EEP_REAR2];
#endif
			break;

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
		case SEC_FRONT_TOP_SENSOR:
			cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_FRONT3);
			mInfo.mVer.cam_cal_ack             = cam_cal_check[EEP_FRONT];
			break;
#else
		case SEC_FRONT_TOP_SENSOR:
			cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_FRONT2);
			mInfo.mVer.cam_cal_ack             = cam_cal_check[EEP_FRONT];
			break;
#endif
#endif
		default:
			break;
	}

	memcpy(mInfo.mVer.phone_hw_info, M_HW_INFO, HW_INFO_MAX_SIZE);
	memcpy(mInfo.mVer.phone_sw_info, M_SW_INFO, SW_INFO_MAX_SIZE);
	memcpy(mInfo.mVer.phone_vendor_info, M_VENDOR_INFO, VENDOR_INFO_MAX_SIZE);
	memcpy(mInfo.mVer.phone_process_info, M_PROCESS_INFO, PROCESS_INFO_MAX_SIZE);

	cam_sec_eeprom_module_info_set_load_version(rev, e_ctrl, &mInfo);

	if (e_ctrl->soc_info.index == SEC_FRONT_SENSOR) {
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
		/* front af cal*/
        AfIdx_t front_idx[] = {
            {AF_CAL_NEAR_IDX, AF_CAL_NEAR_OFFSET_FROM_AF},
            {AF_CAL_FAR_IDX, AF_CAL_FAR_OFFSET_FROM_AF}
        };

        cam_sec_eeprom_module_info_set_afcal(ADDR_M_AF, front_idx, sizeof(front_idx)/sizeof(front_idx[0]),
            e_ctrl->cal_data.mapdata, front_af_cal_str, sizeof(front_af_cal_str));
#endif //!defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)

		/* front mtf exif */
		if (isValidIdx(ADDR_M0_MTF, &ConfAddr) == 1)
		{
			memcpy(front_mtf_exif, &e_ctrl->cal_data.mapdata[ConfAddr], FROM_MTF_SIZE);
			front_mtf_exif[FROM_MTF_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM, "front mtf exif = %s", front_mtf_exif);
		}

		if (isValidIdx(ADDR_M0_PAF, &ConfAddr) == 1)
		{
			ConfAddr += PAF_CAL_ERR_CHECK_OFFSET;
			memcpy(&front_paf_err_data_result, &e_ctrl->cal_data.mapdata[ConfAddr], 4);
		}

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
		/* front2 dual cal */
		mInfo.mVer.dual_cal = front2_dual_cal;
		mInfo.mVer.DualTilt = &front2_dual;
		cam_sec_eeprom_module_info_set_dual_tilt(DUAL_TILT_FRONT, ADDR_M_DUAL_CAL,
			SIZE_M_DUAL_CAL, e_ctrl->cal_data.mapdata, "front2", &mInfo);
#endif //#if defined(CONFIG_SAMSUNG_FRONT_DUAL)

#if defined(CONFIG_SAMSUNG_FRONT_TOF)
		/* front tof dual cal */
		mInfo.mVer.dual_cal = front_tof_dual_cal;
		mInfo.mVer.DualTilt = &front_tof_dual;
		cam_sec_eeprom_module_info_set_dual_tilt(DUAL_TILT_TOF_FRONT, ADDR_TOFCAL_START,
			ADDR_TOFCAL_SIZE, e_ctrl->cal_data.mapdata, "front_tof", &mInfo);
#endif
	}
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
	else if (e_ctrl->soc_info.index == SEC_FRONT_TOP_SENSOR)
    {
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
		/* front3 af cal*/
        AfIdx_t front3_idx[] = {
            {AF_CAL_NEAR_IDX, AF_CAL_NEAR_OFFSET_FROM_AF},
            {AF_CAL_FAR_IDX, AF_CAL_FAR_OFFSET_FROM_AF}
        };

        cam_sec_eeprom_module_info_set_afcal(ADDR_M_AF, front3_idx, sizeof(front3_idx)/sizeof(front3_idx[0]),
            e_ctrl->cal_data.mapdata, front3_af_cal_str, sizeof(front3_af_cal_str));
#endif //!defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
	}
#endif //#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SEC_DM3Q_PROJECT)
	else if ((e_ctrl->soc_info.index == SEC_WIDE_SENSOR)
		|| (e_ctrl->soc_info.index == SEC_ULTRA_WIDE_SENSOR)
		|| (e_ctrl->soc_info.index == SEC_TELE_SENSOR)
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
		|| (e_ctrl->soc_info.index == SEC_TELE2_SENSOR)
#endif
	)
#elif defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
	else if ((e_ctrl->soc_info.index == SEC_WIDE_SENSOR) || (e_ctrl->soc_info.index == SEC_TELE_SENSOR))
#else
	else if (e_ctrl->soc_info.index == SEC_WIDE_SENSOR)
#endif
	{
		/* rear mtf exif */
		if (isValidIdx(ADDR_M0_MTF, &ConfAddr) == 1)
		{
			memcpy(rear_mtf_exif, &e_ctrl->cal_data.mapdata[ConfAddr], FROM_MTF_SIZE);
			rear_mtf_exif[FROM_MTF_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM, "rear mtf exif = %s", rear_mtf_exif);
		}

		/* rear mtf2 exif */
		if (isValidIdx(ADDR_M1_MTF, &ConfAddr) == 1)
		{
			memcpy(rear_mtf2_exif, &e_ctrl->cal_data.mapdata[ConfAddr], FROM_MTF_SIZE);
			rear_mtf2_exif[FROM_MTF_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM, "rear mtf2 exif = %s", rear_mtf2_exif);
		}

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
		/* rear3 mtf exif */
		if (isValidIdx(ADDR_S0_MTF, &ConfAddr) == 1)
		{
			memcpy(rear3_mtf_exif, &e_ctrl->cal_data.mapdata[ConfAddr], FROM_MTF_SIZE);
			rear3_mtf_exif[FROM_MTF_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM, "rear3 mtf exif = %s", rear3_mtf_exif);
		}
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
		/* rear4 mtf exif */
		if (isValidIdx(ADDR_S0_MTF, &ConfAddr) == 1)
		{
			memcpy(rear4_mtf_exif, &e_ctrl->cal_data.mapdata[ConfAddr], FROM_MTF_SIZE);
			rear4_mtf_exif[FROM_MTF_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM, "rear4 mtf exif = %s", rear4_mtf_exif);
		}
#endif

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE) || defined(CONFIG_SAMSUNG_REAR_QUADRA)
#if defined(CONFIG_SEC_DM3Q_PROJECT)
		if (SEC_TELE2_SENSOR == e_ctrl->soc_info.index)
#elif defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
		if (SEC_TELE_SENSOR == e_ctrl->soc_info.index)
#else
		if (SEC_WIDE_SENSOR == e_ctrl->soc_info.index)
#endif
		{
			if ((1 == isValidIdx(ADDR_S_DUAL_CAL, &ConfAddr))
				&& (1 == isValidIdx(SIZE_S_DUAL_CAL, &ConfSize)))
			{
				if (e_ctrl->cal_data.num_data >= (ConfAddr + ConfSize))
				{
					mInfo.mVer.dual_cal = rear3_dual_cal;
					mInfo.mVer.DualTilt = &rear3_dual;
					cam_sec_eeprom_module_info_set_dual_tilt(DUAL_TILT_REAR_TELE, ADDR_S_DUAL_CAL,
						SIZE_S_DUAL_CAL, e_ctrl->cal_data.mapdata, "rear3 tele", &mInfo);
				}
			}
		}
#endif

		CAM_DBG(CAM_EEPROM, "[CAL] index:%d valid(%d, %d)", e_ctrl->soc_info.index, isValidIdx(ADDR_M_AF, &ConfAddr), isValidIdx(ADDR_S0_AF, &ConfAddr));
		{
#if defined(CONFIG_SEC_DM3Q_PROJECT)
			AfIdx_t rear_idx[] = {
				{AF_CAL_NEAR_IDX, AF_CAL_NEAR_OFFSET_FROM_AF},
				{AF_CAL_FAR_IDX, AF_CAL_FAR_OFFSET_FROM_AF},
				{AF_CAL_M1_IDX, AF_CAL_M1_OFFSET_FROM_AF},
				{AF_CAL_M2_IDX, AF_CAL_M2_OFFSET_FROM_AF},
			};

			if (e_ctrl->soc_info.index == SEC_WIDE_SENSOR) {
				cam_sec_eeprom_module_info_set_afcal(ADDR_M_AF, rear_idx, sizeof(rear_idx)/sizeof(rear_idx[0]),
					e_ctrl->cal_data.mapdata, rear_af_cal_str, sizeof(rear_af_cal_str));
			}
			else if (e_ctrl->soc_info.index == SEC_ULTRA_WIDE_SENSOR) {
				cam_sec_eeprom_module_info_set_afcal(ADDR_M_AF, rear_idx, sizeof(rear_idx)/sizeof(rear_idx[0]),
					e_ctrl->cal_data.mapdata, rear2_af_cal_str, sizeof(rear2_af_cal_str));
			}
			else if (e_ctrl->soc_info.index == SEC_TELE_SENSOR) {
				cam_sec_eeprom_module_info_set_afcal(ADDR_S0_AF, rear_idx, sizeof(rear_idx)/sizeof(rear_idx[0]),
					e_ctrl->cal_data.mapdata, rear3_af_cal_str, sizeof(rear3_af_cal_str));
			}
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
			else if (e_ctrl->soc_info.index == SEC_TELE2_SENSOR) {
				cam_sec_eeprom_module_info_set_afcal(ADDR_S0_AF, rear_idx, sizeof(rear_idx)/sizeof(rear_idx[0]),
					e_ctrl->cal_data.mapdata, rear4_af_cal_str, sizeof(rear4_af_cal_str));
			}
#endif
#else       //  #if defined(CONFIG_SEC_DM3Q_PROJECT)
			AfIdx_t rear_idx[] = {
				{AF_CAL_NEAR_IDX, AF_CAL_NEAR_OFFSET_FROM_AF},
				{AF_CAL_FAR_IDX, AF_CAL_FAR_OFFSET_FROM_AF},
				{AF_CAL_M1_IDX, AF_CAL_M1_OFFSET_FROM_AF},
				{AF_CAL_M2_IDX, AF_CAL_M2_OFFSET_FROM_AF},
			};

			cam_sec_eeprom_module_info_set_afcal(ADDR_M_AF, rear_idx, sizeof(rear_idx)/sizeof(rear_idx[0]),
				e_ctrl->cal_data.mapdata, rear_af_cal_str, sizeof(rear_af_cal_str));

			cam_sec_eeprom_module_info_set_afcal(ADDR_S0_AF, rear_idx, sizeof(rear_idx)/sizeof(rear_idx[0]),
				e_ctrl->cal_data.mapdata, rear3_af_cal_str, sizeof(rear3_af_cal_str));
#endif      //  #if defined(CONFIG_SEC_DM3Q_PROJECT)
		}

#elif defined(CONFIG_SAMSUNG_REAR_DUAL)     //  #if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
		/* AF Cal. data read */
#if defined(CONFIG_SEC_B5Q_PROJECT) || defined(CONFIG_SEC_GTS9P_PROJECT) || defined(CONFIG_SEC_GTS9U_PROJECT)
		{
			AfIdx_t rear_idx[] = {
				{AF_CAL_NEAR_IDX, AF_CAL_NEAR_OFFSET_FROM_AF},
				{AF_CAL_FAR_IDX, AF_CAL_FAR_OFFSET_FROM_AF},
			};

			cam_sec_eeprom_module_info_set_afcal(ADDR_M_AF, rear_idx, sizeof(rear_idx)/sizeof(rear_idx[0]),
				e_ctrl->cal_data.mapdata, rear_af_cal_str, sizeof(rear_af_cal_str));
		}		
#else
		{
			AfIdx_t rear_idx[] = {
				{AF_CAL_NEAR_IDX, AF_CAL_NEAR_OFFSET_FROM_AF},
				{AF_CAL_FAR_IDX, AF_CAL_FAR_OFFSET_FROM_AF},
				{AF_CAL_M1_IDX, AF_CAL_M1_OFFSET_FROM_AF},
				{AF_CAL_M2_IDX, AF_CAL_M2_OFFSET_FROM_AF},
			};

			cam_sec_eeprom_module_info_set_afcal(ADDR_M_AF, rear_idx, sizeof(rear_idx)/sizeof(rear_idx[0]),
			e_ctrl->cal_data.mapdata, rear_af_cal_str, sizeof(rear_af_cal_str));
		}
#endif
		/* rear2 sw dual cal */
		mInfo.mVer.dual_cal = rear2_dual_cal;
		mInfo.mVer.DualTilt = &rear2_dual;
		cam_sec_eeprom_module_info_set_dual_tilt(DUAL_TILT_REAR_UW, ADDR_M_DUAL_CAL,
			SIZE_M_DUAL_CAL, e_ctrl->cal_data.mapdata, "rear2 uw", &mInfo);
#else
		{
			AfIdx_t rear_idx[] = {
#if defined(CONFIG_SEC_GTS9_PROJECT)
				{AF_CAL_NEAR_IDX, AF_CAL_NEAR_OFFSET_FROM_AF},
				{AF_CAL_FAR_IDX, AF_CAL_FAR_OFFSET_FROM_AF},
#else
				{AF_CAL_NEAR_IDX, AF_CAL_NEAR_OFFSET_FROM_AF},
				{AF_CAL_FAR_IDX, AF_CAL_FAR_OFFSET_FROM_AF},
				{AF_CAL_M1_IDX, AF_CAL_M1_OFFSET_FROM_AF},
				{AF_CAL_M2_IDX, AF_CAL_M2_OFFSET_FROM_AF},
#endif
		};

			cam_sec_eeprom_module_info_set_afcal(ADDR_M_AF, rear_idx, sizeof(rear_idx)/sizeof(rear_idx[0]),
				e_ctrl->cal_data.mapdata, rear_af_cal_str, sizeof(rear_af_cal_str));
		}
#endif

		cam_sec_eeprom_module_info_set_paf(ADDR_M0_PAF,
			PAF_MID_OFFSET, PAF_MID_SIZE, e_ctrl->cal_data.mapdata, "rear_paf_cal_data_mid",
			rear_paf_cal_data_mid, sizeof(rear_paf_cal_data_mid));

		cam_sec_eeprom_module_info_set_paf(ADDR_M0_PAF,
			PAF_FAR_OFFSET, PAF_FAR_SIZE, e_ctrl->cal_data.mapdata, "rear_paf_cal_data_far",
			rear_paf_cal_data_far, sizeof(rear_paf_cal_data_far));

		if (isValidIdx(ADDR_M0_PAF, &ConfAddr) == 1)
		{
			ConfAddr += PAF_CAL_ERR_CHECK_OFFSET;
			memcpy(&paf_err_data_result, &e_ctrl->cal_data.mapdata[ConfAddr], 4);
		}

		cam_sec_eeprom_module_info_set_paf(ADDR_M1_PAF,
			PAF_MID_OFFSET, PAF_MID_SIZE, e_ctrl->cal_data.mapdata, "rear_f2_paf_cal_data_mid",
			rear_f2_paf_cal_data_mid, sizeof(rear_f2_paf_cal_data_mid));

		cam_sec_eeprom_module_info_set_paf(ADDR_M1_PAF,
			PAF_FAR_OFFSET, PAF_FAR_SIZE, e_ctrl->cal_data.mapdata, "rear_f2_paf_cal_data_far",
			rear_f2_paf_cal_data_far, sizeof(rear_f2_paf_cal_data_far));

		if (isValidIdx(ADDR_M1_PAF, &ConfAddr) == 1)
		{
			ConfAddr += PAF_CAL_ERR_CHECK_OFFSET;
			memcpy(&f2_paf_err_data_result, &e_ctrl->cal_data.mapdata[ConfAddr], 4);
		}

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
		if (isValidIdx(ADDR_S0_PAF, &ConfAddr) == 1)
		{
			ConfAddr += PAF_CAL_ERR_CHECK_OFFSET;
			memcpy(&rear3_paf_err_data_result, &e_ctrl->cal_data.mapdata[ConfAddr], 4);
		}
#endif

#if defined(CONFIG_SEC_DM3Q_PROJECT)
		if (e_ctrl->soc_info.index == SEC_ULTRA_WIDE_SENSOR) {
			if (isValidIdx(ADDR_M0_PAF, &ConfAddr) == 1)
			{
				ConfAddr += PAF_CAL_ERR_CHECK_OFFSET;
				memcpy(&rear2_paf_err_data_result, &e_ctrl->cal_data.mapdata[ConfAddr], 4);
			}
		}

		if (e_ctrl->soc_info.index == SEC_TELE2_SENSOR) {
			if (isValidIdx(ADDR_S0_PAF, &ConfAddr) == 1)
			{
				ConfAddr += PAF_CAL_ERR_CHECK_OFFSET;
				memcpy(&rear4_paf_err_data_result, &e_ctrl->cal_data.mapdata[ConfAddr], 4);
			}
		}
#endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
		if (isValidIdx(ADDR_M_OIS, &ConfAddr) == 1) {

			ConfAddr += OIS_CAL_MARK_START_OFFSET;
			memcpy(&ois_m1_cal_mark, &e_ctrl->cal_data.mapdata[ConfAddr], 1);
			ConfAddr -= OIS_CAL_MARK_START_OFFSET;
			if (ois_m1_cal_mark == 0xBB) {
				ois_gain_rear_result = 0;
				ois_sr_rear_result = 0;
			} else {
				ois_gain_rear_result = 1;
				ois_sr_rear_result = 1;
			}

			ConfAddr += OIS_XYGG_START_OFFSET;
			memcpy(ois_m1_xygg, &e_ctrl->cal_data.mapdata[ConfAddr], OIS_XYGG_SIZE);
			ConfAddr -= OIS_XYGG_START_OFFSET;

			ConfAddr += OIS_XYSR_START_OFFSET;
			memcpy(ois_m1_xysr, &e_ctrl->cal_data.mapdata[ConfAddr], OIS_XYSR_SIZE);
		}

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
		if (isValidIdx(ADDR_S_OIS, &ConfAddr) == 1) {
			int isCal = 0, j = 0;
			uint8_t* cal_mark = &ois_m2_cal_mark;
			int* gain_result = &ois_gain_rear3_result;
			int* sr_result = &ois_sr_rear3_result;
			uint8_t *xygg = ois_m2_xygg;
			uint8_t *xysr = ois_m2_xysr;
			uint8_t *cross_talk = ois_m2_cross_talk;
			int* cross_talk_result = &ois_m2_cross_talk_result;
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
			if (e_ctrl->soc_info.index == SEC_TELE2_SENSOR) {
				cal_mark = &ois_m3_cal_mark;
				gain_result = &ois_gain_rear4_result;
				sr_result = &ois_sr_rear4_result;
				xygg = ois_m3_xygg;
				xysr = ois_m3_xysr;
				cross_talk = ois_m3_cross_talk;
				cross_talk_result = &ois_m3_cross_talk_result;
			}
#endif

			ConfAddr += OIS_CAL_MARK_START_OFFSET;
			memcpy(cal_mark, &e_ctrl->cal_data.mapdata[ConfAddr], 1);
			ConfAddr -= OIS_CAL_MARK_START_OFFSET;
			*gain_result = ((*cal_mark) == 0xBB) ? 0 : 1;
			*sr_result = ((*cal_mark) == 0xBB) ? 0 : 1;

			ConfAddr += OIS_XYGG_START_OFFSET;
			memcpy(xygg, &e_ctrl->cal_data.mapdata[ConfAddr], OIS_XYGG_SIZE);
			ConfAddr -= OIS_XYGG_START_OFFSET;

			ConfAddr += OIS_XYSR_START_OFFSET;
			memcpy(xysr, &e_ctrl->cal_data.mapdata[ConfAddr], OIS_XYSR_SIZE);
			ConfAddr -= OIS_XYSR_START_OFFSET;

			ConfAddr += OIS_CROSSTALK_START_OFFSET;
			memcpy(cross_talk, &e_ctrl->cal_data.mapdata[ConfAddr], OIS_CROSSTALK_SIZE);
			ConfAddr -= OIS_CROSSTALK_START_OFFSET;
			*cross_talk_result = 0;
			for (j = 0; j < OIS_CROSSTALK_SIZE; j++) {
				if (cross_talk[j] != 0xFF) {
					isCal = 1;
					break;
				}
			}
			*cross_talk_result = (isCal == 0) ?  1 : 0;
		}

		if (isValidIdx(ADDR_S_DUAL_CAL, &ConfAddr) == 1) {

			ConfAddr += WIDE_OIS_CENTER_SHIFT_START_OFFSET;
			memcpy(ois_m1_center_shift, &e_ctrl->cal_data.mapdata[ConfAddr], OIS_CENTER_SHIFT_SIZE);
			ConfAddr -= WIDE_OIS_CENTER_SHIFT_START_OFFSET;

			ConfAddr += TELE_OIS_CENTER_SHIFT_START_OFFSET;
			memcpy(ois_m2_center_shift,  &e_ctrl->cal_data.mapdata[ConfAddr], OIS_CENTER_SHIFT_SIZE);
			ConfAddr -= TELE_OIS_CENTER_SHIFT_START_OFFSET;

		}
#endif
#endif

#if defined(CONFIG_SAMSUNG_REAR_TOF)
		/* rear tof dual cal */
		mInfo.mVer.dual_cal = rear_tof_dual_cal;
		mInfo.mVer.DualTilt = &rear_tof_dual;
		cam_sec_eeprom_module_info_set_dual_tilt(DUAL_TILT_TOF_REAR, ADDR_TOFCAL_START,
			ADDR_TOFCAL_SIZE, e_ctrl->cal_data.mapdata, "rear tof", &mInfo);

		/* rear2 tof tilt */
		//	same dual_cal data between rear_tof and rear2_tof
		mInfo.mVer.dual_cal = rear_tof_dual_cal;
		mInfo.mVer.DualTilt = &rear2_tof_dual;
		cam_sec_eeprom_module_info_set_dual_tilt(DUAL_TILT_TOF_REAR2, ADDR_TOFCAL_START,
			ADDR_TOFCAL_SIZE, e_ctrl->cal_data.mapdata, "rear2 tof", &mInfo);
#endif
	}
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
	else if (e_ctrl->soc_info.index == SEC_ULTRA_WIDE_SENSOR) {
		/* rear2 mtf exif */
		if (isValidIdx(ADDR_M0_MTF, &ConfAddr) == 1)
		{
			memcpy(rear2_mtf_exif, &e_ctrl->cal_data.mapdata[ConfAddr], FROM_MTF_SIZE);
			rear2_mtf_exif[FROM_MTF_SIZE] = '\0';
			CAM_DBG(CAM_EEPROM, "rear2 mtf exif = %s", rear2_mtf_exif);
		}
	}
#endif
#if defined(CONFIG_SAMSUNG_REAR_TOF)
	else if (e_ctrl->soc_info.index == SEC_REAR_TOF_SENSOR) {
		cam_sec_eeprom_module_info_tof(e_ctrl->cal_data.mapdata, "rear",
			rear_tof_cal, rear_tof_cal_extra, &rear_tof_uid, &rear_tof_cal_result,
			&rear_tof_validation_500, &rear_tof_validation_300);
	}

#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOF)
	else if (e_ctrl->soc_info.index == SEC_FRONT_TOF_SENSOR) {
		cam_sec_eeprom_module_info_tof(e_ctrl->cal_data.mapdata, "front",
			front_tof_cal, front_tof_cal_extra, &front_tof_uid, &front_tof_cal_result,
			&rear_tof_validation_500, &rear_tof_validation_300);

		if (mInfo.mapVer < '5') {
			CAM_INFO(CAM_EEPROM, "invalid front tof uid (map_ver %c), force chage to 0xCB29", mInfo.mapVer);
			front_tof_uid = 0xCB29;
		}
	}
#endif

	rc = cam_sec_eeprom_check_firmware_cal(e_ctrl->is_supported, e_ctrl->camera_normal_cal_crc, &mInfo);

#if defined(CONFIG_SAMSUNG_WACOM_NOTIFIER)
	// Update for each module
	if (1 == (e_ctrl->is_supported & 0x1))
	{
		is_eeprom_info_update(e_ctrl->soc_info.index, mInfo.mVer.module_fw_ver);
	}

	// Probe Timing different for each model
#if defined(CONFIG_SEC_DM3Q_PROJECT)
	if (SEC_TELE2_SENSOR == e_ctrl->soc_info.index)
	{
		is_eeprom_wacom_update_notifier();
	}
#elif defined(CONFIG_SEC_Q2Q_PROJECT)
	if (SEC_FRONT_TOP_SENSOR == e_ctrl->soc_info.index)
	{
		is_eeprom_wacom_update_notifier();
	}
#else
#endif
#endif	/* CONFIG_SAMSUNG_WACOM_NOTIFIER */

	return rc;
}

void cam_sec_eeprom_link_module_info(struct cam_eeprom_ctrl_t *e_ctrl, ModuleInfo_t *mInfo, eeprom_camera_id_type camera_id)
{
	strlcpy(mInfo->typeStr, camera_info[camera_id], FROM_MODULE_FW_INFO_SIZE);
	mInfo->typeStr[FROM_MODULE_FW_INFO_SIZE-1] = '\0';

	mInfo->type                         = e_ctrl->soc_info.index;
	mInfo->M_or_S                       = MAIN_MODULE;

	mInfo->mVer.sensor_id               = sensor_id[camera_id];
	mInfo->mVer.sensor2_id              = sensor_id[camera_id];
	mInfo->mVer.module_id               = module_id[camera_id];

	mInfo->mVer.module_info             = module_info[camera_id];
	mInfo->mVer.cam_fw_ver              = cam_fw_ver[camera_id];
	mInfo->mVer.cam_fw_full_ver         = cam_fw_full_ver[camera_id];

	mInfo->mVer.fw_user_ver             = cam_fw_user_ver[camera_id];
	mInfo->mVer.fw_factory_ver          = cam_fw_factory_ver[camera_id];	
}

void cam_sec_eeprom_update_sysfs_fw_version(
	const char *update_fw_ver, cam_eeprom_fw_version_idx update_fw_index, ModuleInfo_t *mInfo)
{
	if (update_fw_index == EEPROM_FW_VER)
		strlcpy(mInfo->mVer.module_fw_ver, update_fw_ver, FROM_MODULE_FW_INFO_SIZE + 1);
	else if (update_fw_index == PHONE_FW_VER)
		strlcpy(mInfo->mVer.phone_fw_ver, update_fw_ver, FROM_MODULE_FW_INFO_SIZE + 1);
	else {
#if defined(CONFIG_SAMSUNG_REAR_TOF)
		if (mInfo->type == SEC_REAR_TOF_SENSOR)
		{
		    sprintf(mInfo->mVer.load_fw_ver, "N");
			CAM_INFO(CAM_EEPROM, "rear tof not support load_fw_ver");
		}
		else
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOF)
		if (mInfo->type == SEC_FRONT_TOF_SENSOR)
		{
		    sprintf(mInfo->mVer.load_fw_ver, "N");
			CAM_INFO(CAM_EEPROM, "front tof not support load_fw_ver");
		}
		else
#endif
		strlcpy(mInfo->mVer.load_fw_ver, update_fw_ver, FROM_MODULE_FW_INFO_SIZE + 1);
	}

	/* update EEPROM fw version on sysfs */
	//	all camera module except rear wide module.
	if ((mInfo->type != SEC_WIDE_SENSOR)
	 || (mInfo->type == SEC_WIDE_SENSOR && mInfo->M_or_S != MAIN_MODULE))
	{
		sprintf(mInfo->mVer.phone_fw_ver, "N");
	}

	sprintf(mInfo->mVer.cam_fw_ver, "%s %s\n", mInfo->mVer.module_fw_ver, mInfo->mVer.load_fw_ver);
	sprintf(mInfo->mVer.cam_fw_full_ver, "%s %s %s\n", mInfo->mVer.module_fw_ver, mInfo->mVer.phone_fw_ver, mInfo->mVer.load_fw_ver);

	CAM_INFO(CAM_EEPROM, "camera_idx: %d, cam_fw_full_ver: %s", mInfo->type, mInfo->mVer.cam_fw_full_ver);
}

int32_t cam_sec_eeprom_check_firmware_cal(uint32_t camera_cal_crc, uint32_t camera_normal_cal_crc, ModuleInfo_t *mInfo)
{
	int rc = 0, offset = 0, cnt = 0;
	char final_cmd_ack[SYSFS_FW_VER_SIZE] = "NG_";
	char cam_cal_ack[SYSFS_FW_VER_SIZE] = "NULL";

	uint8_t isNeedUpdate = TRUE;
	uint8_t version_isp = 0, version_module_maker_ver = 0;
	uint8_t isValid_EEPROM_data = TRUE;
	uint8_t isQCmodule = TRUE;
	uint8_t camera_cal_ack = OK;
	uint8_t camera_fw_ack = OK;

	if ((mInfo == NULL) || (mInfo->mVer.cam_fw_ver == NULL))
	{
		CAM_ERR(CAM_EEPROM, "invalid argument");
		rc = 0;
		return rc;
	}

	version_isp = mInfo->mVer.cam_fw_ver[3];
	version_module_maker_ver = mInfo->mVer.cam_fw_ver[10];

	if (version_isp == 0xff || version_module_maker_ver == 0xff) {
		CAM_ERR(CAM_EEPROM, "invalid eeprom data");
		isValid_EEPROM_data = FALSE;
		cam_sec_eeprom_update_sysfs_fw_version("NULL", EEPROM_FW_VER, mInfo);
	}

	/* 1. check camera firmware and cal data */
	CAM_INFO(CAM_EEPROM, "camera_cal_crc: 0x%x", camera_cal_crc);

	if (camera_cal_crc == camera_normal_cal_crc) {
		camera_cal_ack = OK;
		strncpy(cam_cal_ack, "Normal", SYSFS_FW_VER_SIZE);
	} else {
		camera_cal_ack = CRASH;
		strncpy(cam_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);

		offset = strlen(final_cmd_ack);
		if (mInfo->type == SEC_WIDE_SENSOR) {
			camera_cal_ack = CRASH;
			strncpy(cam_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
			if ((camera_cal_crc & CAMERA_CAL_CRC_WIDE) != CAMERA_CAL_CRC_WIDE) {
				cnt = scnprintf(&final_cmd_ack[offset], SYSFS_FW_VER_SIZE-offset, "%s", "CD");
				offset += cnt;
			} else {
				cnt = scnprintf(&final_cmd_ack[offset], SYSFS_FW_VER_SIZE-offset, "%s", "CD4");
				offset += cnt;
			}
#else
			cnt = scnprintf(&final_cmd_ack[offset], SYSFS_FW_VER_SIZE-offset, "%s", "CD");
			offset += cnt;
#endif
		} else {
			camera_cal_ack = CRASH;
			strncpy(cam_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
			cnt = scnprintf(&final_cmd_ack[offset], SYSFS_FW_VER_SIZE-offset, "%s", "CD3");
			offset += cnt;
		}
		final_cmd_ack[offset] = '\0';

		switch(mInfo->type)
		{
			case SEC_FRONT_SENSOR:
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
			case SEC_FRONT_TOP_SENSOR:
#endif
#if defined(UNUSE_FRONT_EEPROM)
				strncpy(final_cmd_ack, "NG_", 3);
				strncpy(cam_cal_ack, "NULL", SYSFS_FW_VER_SIZE);
				camera_cal_ack = OK;
#endif
				break;

			default:
				break;
		}
	}

	/* 3-1. all success case: display LOAD FW */
	if (camera_fw_ack && camera_cal_ack)
		isNeedUpdate = FALSE;

	/* 3-2. fail case: update CMD_ACK on sysfs (load fw) */
	// If not QC module, return NG.
	if (version_isp >= 0x80 || !isalnum(version_isp))
		CAM_INFO(CAM_EEPROM, "ISP Ver : 0x%x", version_isp);
	else
		CAM_INFO(CAM_EEPROM, "ISP Ver : %c", version_isp);

	if (version_isp != 'Q' && version_isp != 'U' && version_isp != 'A' && version_isp != 'X' && version_isp != 'E') {
		CAM_ERR(CAM_EEPROM, "This is not Qualcomm module!");

		if (mInfo->type == SEC_WIDE_SENSOR) {
			strncpy(final_cmd_ack, "NG_FWCD", SYSFS_FW_VER_SIZE);
			strncpy(cam_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
		} else {
			strncpy(final_cmd_ack, "NG_CD3_L", SYSFS_FW_VER_SIZE);
			strncpy(cam_cal_ack, "Abnormal", SYSFS_FW_VER_SIZE);
		}

		isNeedUpdate = TRUE;
		isQCmodule = FALSE;
		camera_cal_ack = CRASH;
	}

#if defined(CONFIG_SAMSUNG_REAR_TOF)
		if (mInfo->type == SEC_REAR_TOF_SENSOR)
		{
		    isNeedUpdate = TRUE;
			CAM_INFO(CAM_EEPROM,"Set true sysfs update for TOF");
		}
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOF)
		if (mInfo->type == SEC_FRONT_TOF_SENSOR)
		{
		    isNeedUpdate = TRUE;
			CAM_INFO(CAM_EEPROM,"Set true sysfs update for TOF");
		}
#endif

	if (isNeedUpdate) {
		CAM_ERR(CAM_EEPROM, "final_cmd_ack : %s", final_cmd_ack);
		cam_sec_eeprom_update_sysfs_fw_version(final_cmd_ack, LOAD_FW_VER, mInfo);
	} else {
		// just display success fw version log
		CAM_INFO(CAM_EEPROM, "final_cmd_ack : %s", final_cmd_ack);
		memset(final_cmd_ack, 0, sizeof(final_cmd_ack));
		strncpy(final_cmd_ack, mInfo->mVer.cam_fw_full_ver, SYSFS_FW_VER_SIZE);
		final_cmd_ack[SYSFS_FW_VER_SIZE-1] = '\0';

		CAM_INFO(CAM_EEPROM, "final_cmd_ack : %s", final_cmd_ack);
	}

	/* 4. update CAL check ack on sysfs rear_calcheck */
	strlcpy(mInfo->mVer.cam_cal_ack, cam_cal_ack, SYSFS_FW_VER_SIZE);
	snprintf(cal_crc, SYSFS_FW_VER_SIZE, "%s %s\n", cam_cal_check[EEP_REAR], cam_cal_check[EEP_FRONT]);

	CAM_INFO(CAM_EEPROM,
		"version_module_maker: 0x%x, MODULE_VER_ON_PVR: 0x%x, MODULE_VER_ON_SRA: 0x%x",
		version_module_maker_ver, ModuleVerOnPVR, ModuleVerOnSRA);
	CAM_INFO(CAM_EEPROM,
		"cal_map_version: 0x%x vs FROM_CAL_MAP_VERSION: 0x%x",
		mInfo->mapVer, minCalMapVer);

	if ((isQCmodule == TRUE) && ((isValid_EEPROM_data == FALSE) || (mInfo->mapVer < minCalMapVer)
		|| (version_module_maker_ver < ModuleVerOnPVR))) {
		strncpy(mInfo->mVer.fw_user_ver, "NG", SYSFS_FW_VER_SIZE);
	} else {
		if (camera_cal_ack == CRASH)
			strncpy(mInfo->mVer.fw_user_ver, "NG", SYSFS_FW_VER_SIZE);
		else
			strncpy(mInfo->mVer.fw_user_ver, "OK", SYSFS_FW_VER_SIZE);
	}

	if ((isQCmodule == TRUE) && ((isValid_EEPROM_data == FALSE) || (mInfo->mapVer < minCalMapVer)
		|| (version_module_maker_ver < ModuleVerOnSRA))) {
		strncpy(mInfo->mVer.fw_factory_ver, "NG_VER", SYSFS_FW_VER_SIZE);
	} else {
		if (camera_cal_ack == CRASH) {
			if (mInfo->type == SEC_WIDE_SENSOR) {
				strncpy(mInfo->mVer.fw_factory_ver, "NG_VER", SYSFS_FW_VER_SIZE);
			} else {
				strncpy(mInfo->mVer.fw_factory_ver, "NG_CRC", SYSFS_FW_VER_SIZE);

				if (mInfo->type == SEC_FRONT_SENSOR || mInfo->type == SEC_ULTRA_WIDE_SENSOR || mInfo->type == SEC_FRONT_TOP_SENSOR) // TEMP_8550
					strncpy(mInfo->mVer.fw_factory_ver, "OK", SYSFS_FW_VER_SIZE);
			}
		}
		else {
			strncpy(mInfo->mVer.fw_factory_ver, "OK", SYSFS_FW_VER_SIZE);
		}
	}

	return rc;
}

/**
 * cam_sec_eeprom_verify_sum - verify crc32 checksum
 * @mem:			data buffer
 * @size:			size of data buffer
 * @sum:			expected checksum
 * @rev_endian:	compare reversed endian (0:little, 1:big)
 *
 * Returns 0 if checksum match, -EINVAL otherwise.
 */
static int cam_sec_eeprom_verify_sum(const char *mem, uint32_t size, uint32_t sum, uint32_t rev_endian)
{
	uint32_t crc = ~0;
	uint32_t cmp_crc = 0;

	/* check overflow */
	if (size > crc - sizeof(uint32_t))
		return -EINVAL;

	crc = crc32_le(crc, mem, size);

	crc = ~crc;
	if (rev_endian == 1) {
		cmp_crc = (((crc) & 0xFF) << 24)
				| (((crc) & 0xFF00) << 8)
				| (((crc) >> 8) & 0xFF00)
				| ((crc) >> 24);
	} else {
		cmp_crc = crc;
	}
	CAM_DBG(CAM_EEPROM, "endian %d, expect 0x%x, result 0x%x", rev_endian, sum, cmp_crc);

	if (cmp_crc != sum) {
		CAM_ERR(CAM_EEPROM, "endian %d, expect 0x%x, result 0x%x", rev_endian, sum, cmp_crc);
		return -EINVAL;
	} else {
		CAM_DBG(CAM_EEPROM, "checksum pass 0x%x", sum);
		return 0;
	}
}

/**
 * cam_sec_eeprom_match_crc - verify multiple regions using crc
 * @data:	data block to be verified
 *
 * Iterates through all regions stored in @data.  Regions with odd index
 * are treated as data, and its next region is treated as checksum.  Thus
 * regions of even index must have valid_size of 4 or 0 (skip verification).
 * Returns a bitmask of verified regions, starting from LSB.  1 indicates
 * a checksum match, while 0 indicates checksum mismatch or not verified.
 */
uint32_t cam_sec_eeprom_match_crc(struct cam_eeprom_memory_block_t *data, uint32_t subdev_id)
{
	int j, rc;
	uint32_t *sum;
	uint32_t ret = 0;
	uint8_t *memptr, *memptr_crc;
	struct cam_eeprom_memory_map_t *map;

	if (!data) {
		CAM_ERR(CAM_EEPROM, "data is NULL");
		return -EINVAL;
	}
	map = data->map;

#if 1
{

	uint8_t map_ver = 0;
	uint32_t ConfAddr = 0;

	if (subdev_id < SEC_SENSOR_ID_MAX)
	{
		if (isValidIdx(ADDR_M_CALMAP_VER, &ConfAddr) == 1) {
			ConfAddr += 0x03;
			map_ver = data->mapdata[ConfAddr];
		}
		else
		{
			CAM_INFO(CAM_EEPROM, "ADDR_M_CALMAP_VER is not set: %d", subdev_id);
		}
	}
	else
	{
		CAM_INFO(CAM_EEPROM, "subdev_id: %d is not supported", subdev_id);
		return 0;
	}

	if (map_ver >= 0x80 || !isalnum(map_ver))
		CAM_INFO(CAM_EEPROM, "map subdev_id = %d, version = 0x%x", subdev_id, map_ver);
	else
		CAM_INFO(CAM_EEPROM, "map subdev_id = %d, version = %c [0x%x]", subdev_id, map_ver, map_ver);
}
#endif

	//  idx 0 is the actual reading section (whole data)
	//  from idx 1, start to compare CRC checksum
	//  (1: CRC area for header, 2: CRC value)
	for (j = 1; j + 1 < data->num_map; j += 2) {
		memptr = data->mapdata + map[j].mem.addr;
		memptr_crc = data->mapdata + map[j+1].mem.addr;

		/* empty table or no checksum */
		if (!map[j].mem.valid_size || !map[j+1].mem.valid_size) {
			CAM_ERR(CAM_EEPROM, "continue");
			continue;
		}

		if (map[j+1].mem.valid_size < sizeof(uint32_t)) {
			CAM_ERR(CAM_EEPROM, "[%d : size 0x%X] malformatted data mapping", j+1, map[j+1].mem.valid_size);
			return -EINVAL;
		}
		CAM_DBG(CAM_EEPROM, "[%d] memptr 0x%x, memptr_crc 0x%x", j, map[j].mem.addr, map[j + 1].mem.addr);
		sum = (uint32_t *) (memptr_crc + map[j+1].mem.valid_size - sizeof(uint32_t));
		rc = cam_sec_eeprom_verify_sum(memptr, map[j].mem.valid_size, *sum, 0);

		if (!rc)
			ret |= 1 << (j/2);
	}

	CAM_INFO(CAM_EEPROM, "CRC result = 0x%08X", ret);

	return ret;
}

/**
 * cam_sec_eeprom_calc_calmap_size - Calculate cal array size based on the cal map
 * @e_ctrl:       ctrl structure
 *
 * Returns size of cal array
 */
int32_t cam_sec_eeprom_calc_calmap_size(struct cam_eeprom_ctrl_t *e_ctrl)
{
	struct cam_eeprom_memory_map_t    *map = NULL;
	uint32_t minMap, maxMap, minLocal, maxLocal;
	int32_t i;
	int32_t calmap_size = 0;

	if (e_ctrl == NULL ||
		(e_ctrl->cal_data.num_map == 0) ||
		(e_ctrl->cal_data.map == NULL)) {
		CAM_INFO(CAM_EEPROM, "cal size is wrong");
		return calmap_size;
	}

	map = e_ctrl->cal_data.map;
	minMap = minLocal = 0xFFFFFFFF;
	maxMap = maxLocal = 0x00;

	for (i = 0; i < e_ctrl->cal_data.num_map; i++) {
		minLocal = map[i].mem.addr;
		maxLocal = minLocal + map[i].mem.valid_size;

		if (minMap > minLocal)
		{
			minMap = minLocal;
		}

		if (maxMap < maxLocal)
		{
			maxMap = maxLocal;
		}

		CAM_DBG(CAM_EEPROM, "[%d / %d] minLocal = 0x%X, minMap = 0x%X, maxLocal = 0x%X, maxMap = 0x%X",
			i+1, e_ctrl->cal_data.num_map, minLocal, minMap, maxLocal, maxMap);
	}
	calmap_size = maxMap - minMap;

	CAM_INFO(CAM_EEPROM, "calmap_size = 0x%X, minMap = 0x%X, maxMap = 0x%X",
		calmap_size, minMap, maxMap);

	return calmap_size;
}

int32_t cam_sec_eeprom_fill_configInfo(char *configString, uint32_t value, ConfigInfo_t *ConfigInfo)
{
	int32_t i, ret = 1;

	for(i = 0; i < MAX_CONFIG_INFO_IDX; i ++)
	{
		if (ConfigInfo[i].isSet == 1)
			continue;

		if (!strcmp(configString, ConfigInfoStrs[i]))
		{
			ConfigInfo[i].isSet = 1;
			ConfigInfo[i].value = value;
			ret = 0;

			switch(i)
			{
				case DEF_M_CORE_VER:
					memset(M_HW_INFO, 0x00, HW_INFO_MAX_SIZE);
					M_HW_INFO[0] = (value) & 0xFF;

					memset(S_HW_INFO, 0x00, HW_INFO_MAX_SIZE);
					S_HW_INFO[0] = (value) & 0xFF;

					if ((value>>16) & 0xFF)
					{
						S_HW_INFO[0] = (value>>16) & 0xFF;
						CAM_DBG(CAM_EEPROM, "value: 0x%08X, S_HW_INFO[0]: 0x%02X", value, S_HW_INFO[0]);
					}

					break;

				case DEF_M_VER_HW:
					M_HW_INFO[1] = (value >> 24) & 0xFF;
					M_HW_INFO[2] = (value >> 16) & 0xFF;
					M_HW_INFO[3] = (value >>  8) & 0xFF;
					M_HW_INFO[4] = (value)       & 0xFF;

					CAM_DBG(CAM_EEPROM, "M_HW_INFO: %c %c%c %c %c",
						M_HW_INFO[0], M_HW_INFO[1], M_HW_INFO[2], M_HW_INFO[3], M_HW_INFO[4]);
					break;

				case DEF_M_VER_SW:
					memset(M_SW_INFO, 0x00, SW_INFO_MAX_SIZE);
					M_SW_INFO[0] = (value >> 24) & 0xFF;
					M_SW_INFO[1] = (value >> 16) & 0xFF;
					M_SW_INFO[2] = (value >>  8) & 0xFF;
					M_SW_INFO[3] = (value)       & 0xFF;

					CAM_DBG(CAM_EEPROM, "M_SW_INFO: %c %c %c%c",
						M_SW_INFO[0], M_SW_INFO[1], M_SW_INFO[2], M_SW_INFO[3]);

					memset(S_SW_INFO, 0x00, SW_INFO_MAX_SIZE);
					S_SW_INFO[0] = (value >> 24) & 0xFF;
					S_SW_INFO[1] = (value >> 16) & 0xFF;
					S_SW_INFO[2] = (value >>  8) & 0xFF;
					S_SW_INFO[3] = (value)       & 0xFF;
					break;

				case DEF_M_VER_ETC:
					memset(M_VENDOR_INFO, 0x00, VENDOR_INFO_MAX_SIZE);
					memset(M_PROCESS_INFO, 0x00, PROCESS_INFO_MAX_SIZE);
					M_VENDOR_INFO[0] = (value >> 24) & 0xFF;
					M_PROCESS_INFO[0] = (value >> 16) & 0xFF;

					CAM_DBG(CAM_EEPROM, "M_ETC_VER: %c %c",
						M_VENDOR_INFO[0], M_PROCESS_INFO[0]);

					memset(S_VENDOR_INFO, 0x00, VENDOR_INFO_MAX_SIZE);
					memset(S_PROCESS_INFO, 0x00, PROCESS_INFO_MAX_SIZE);
					S_VENDOR_INFO[0] = (value >> 24) & 0xFF;
					S_PROCESS_INFO[0] = (value >> 16) & 0xFF;
					break;

				case DEF_S_VER_HW:
					S_HW_INFO[1] = (value >> 24) & 0xFF;
					S_HW_INFO[2] = (value >> 16) & 0xFF;
					S_HW_INFO[3] = (value >>  8) & 0xFF;
					S_HW_INFO[4] = (value)       & 0xFF;

					CAM_DBG(CAM_EEPROM, "S_HW_INFO: %c %c%c %c %c",
						S_HW_INFO[0], S_HW_INFO[1], S_HW_INFO[2], S_HW_INFO[3], S_HW_INFO[4]);
					break;

				case DEF_M_CHK_VER:
					CriterionRev    = (value >> 24) & 0xFF;
					ModuleVerOnPVR  = (value >> 16) & 0xFF;
					ModuleVerOnSRA  = (value >>  8) & 0xFF;
					minCalMapVer    = ((value     ) & 0xFF) + '0';

					CAM_DBG(CAM_EEPROM, "value: 0x%08X, CriterionRev: %d, ModuleVerOnPVR: %c, ModuleVerOnSRA: %c, minCalMapVer: %d",
						value, CriterionRev, ModuleVerOnPVR, ModuleVerOnSRA, minCalMapVer);
					break;

				default:
					break;
			}
		}
	}

	return ret;
}

/**
 * cam_sec_eeprom_get_customInfo - parse the userspace IO config and
 *                            read phone version at eebindriver.bin
 * @e_ctrl:     ctrl structure
 * @csl_packet: csl packet received
 *
 * Returns success or failure
 */
int32_t cam_sec_eeprom_get_customInfo(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_packet *csl_packet)
{
	struct cam_buf_io_cfg *io_cfg;
	uint32_t              i = 0;
	int                   rc = 0;
	uintptr_t             buf_addr;
	size_t                buf_size = 0;
	uint8_t               *read_buffer;

	uint8_t               *pBuf = NULL;
	uint32_t              nConfig = 0;
	char                  *strConfigName = "CustomInfo";

	char                  configString[MaximumCustomStringLength] = "";
	uint32_t              configValue = 0;

	io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
		&csl_packet->payload +
		csl_packet->io_configs_offset);

	CAM_DBG(CAM_EEPROM, "number of IO configs: %d:",
		csl_packet->num_io_configs);

	for (i = 0; i < csl_packet->num_io_configs; i++) {
		CAM_DBG(CAM_EEPROM, "Direction: %d:", io_cfg->direction);
		if (io_cfg->direction == CAM_BUF_OUTPUT) {
			rc = cam_mem_get_cpu_buf(io_cfg->mem_handle[0],
				&buf_addr, &buf_size);
			CAM_DBG(CAM_EEPROM, "buf_addr : %pK, buf_size : %zu",
				(void *)buf_addr, buf_size);

			read_buffer = (uint8_t *)buf_addr;
			if (!read_buffer) {
				CAM_ERR(CAM_EEPROM,
					"invalid buffer to copy data");
				return -EINVAL;
			}
			read_buffer += io_cfg->offsets[0];

			if (buf_size < e_ctrl->cal_data.num_data) {
				CAM_ERR(CAM_EEPROM,
					"failed to copy, Invalid size");
				return -EINVAL;
			}

			CAM_DBG(CAM_EEPROM, "copy the data, len:%d, read_buffer[0] = %d, read_buffer[4] = %d",
				e_ctrl->cal_data.num_data, read_buffer[0], read_buffer[4]);

			memset(&ConfigInfo, 0x00, sizeof(ConfigInfo_t) * MAX_CONFIG_INFO_IDX);

			pBuf = read_buffer;
			if (strcmp(pBuf, strConfigName) == 0) {
				pBuf += strlen(strConfigName)+1+sizeof(uint32_t);

				memcpy(&nConfig, pBuf, sizeof(uint32_t));
				pBuf += sizeof(uint32_t);

				CAM_INFO(CAM_EEPROM, "nConfig: %d", nConfig);
				for(i = 0; i < nConfig; i ++) {
					memcpy(configString, pBuf, MaximumCustomStringLength);
					pBuf += MaximumCustomStringLength;

					memcpy(&configValue, pBuf, sizeof(uint32_t));
					pBuf += sizeof(uint32_t);

#if 0
					CAM_INFO(CAM_EEPROM, "ConfigInfo[%d] = %s     0x%04X", i, configString, configValue);
#endif

					cam_sec_eeprom_fill_configInfo(configString, configValue, ConfigInfo);
				}
			}

#if 0
			for(i = 0; i < MAX_CONFIG_INFO_IDX; i ++)
			{
				if (ConfigInfo[i].isSet == 1)
				{
					CAM_INFO(CAM_EEPROM, "ConfigInfo[%d] (%d) = %s     0x%04X",
						i, ConfigInfo[i].isSet, ConfigInfoStrs[i], ConfigInfo[i].value);
				}
			}
#endif

			memset(read_buffer, 0x00, e_ctrl->cal_data.num_data);
		} else {
			CAM_ERR(CAM_EEPROM, "Invalid direction");
			rc = -EINVAL;
		}
	}

	return rc;
}

/**
 * cam_sec_eeprom_get_phone_ver - parse the userspace IO config and
 *                            read phone version at eebindriver.bin
 * @e_ctrl:     ctrl structure
 * @csl_packet: csl packet received
 *
 * Returns success or failure
 */

#define REAR_MODULE_FW_VERSION (0x50)
int32_t cam_sec_eeprom_get_phone_ver(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_packet *csl_packet)
{
	(void) e_ctrl;
	(void) csl_packet;

	return 0;
#if 0
	struct cam_buf_io_cfg *io_cfg;
	uint32_t              i = 0, j = 0;
	int                   rc = 0;
	uintptr_t             buf_addr;
	size_t                buf_size;
	uint8_t               *read_buffer;

	int                   nVer = 0;
	uint8_t               *pBuf = NULL;
	uint8_t	              bVerNormal = TRUE;

	char                  tmp_hw_info[HW_INFO_MAX_SIZE];// = HW_INFO;
	char                  tmp_sw_info[SW_INFO_MAX_SIZE];// = SW_INFO;
	char                  tmp_vendor_info[VENDOR_INFO_MAX_SIZE];// = VENDOR_INFO;
	char                  tmp_process_info[PROCESS_INFO_MAX_SIZE];// = PROCESS_INFO;
	unsigned int          tmp_rev = 0;

	io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
		&csl_packet->payload +
		csl_packet->io_configs_offset);

	CAM_INFO(CAM_EEPROM, "number of IO configs: %d:",
		csl_packet->num_io_configs);

	for (i = 0; i < csl_packet->num_io_configs; i++) {
		CAM_INFO(CAM_EEPROM, "Direction: %d:", io_cfg->direction);
		if (io_cfg->direction == CAM_BUF_OUTPUT) {
			rc = cam_mem_get_cpu_buf(io_cfg->mem_handle[0],
				&buf_addr, &buf_size);
			CAM_INFO(CAM_EEPROM, "buf_addr : %pK, buf_size : %zu",
				(void *)buf_addr, buf_size);

			read_buffer = (uint8_t *)buf_addr;
			if (!read_buffer) {
				CAM_ERR(CAM_EEPROM,
					"invalid buffer to copy data");
				return -EINVAL;
			}
			read_buffer += io_cfg->offsets[0];

			if (buf_size < e_ctrl->cal_data.num_data) {
				CAM_ERR(CAM_EEPROM,
					"failed to copy, Invalid size");
				return -EINVAL;
			}

			CAM_INFO(CAM_EEPROM, "copy the data, len:%d, read_buffer[0] = %d, read_buffer[4] = %d",
				e_ctrl->cal_data.num_data, read_buffer[0], read_buffer[4]);

			pBuf = read_buffer;
			memcpy(&nVer, pBuf, sizeof(int));
			pBuf += sizeof(int);

			memcpy(&tmp_rev, pBuf, sizeof(int));
			pBuf += sizeof(int);

			bVerNormal = TRUE;
			for(j = 0; j < FROM_MODULE_FW_INFO_SIZE; j ++) {
				CAM_DBG(CAM_EEPROM, "mapdata[0x%04X] = 0x%02X",
					REAR_MODULE_FW_VERSION + j,
					e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION + j]);

				if (e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION + j] >= 0x80
					|| !isalnum(e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION + j] & 0xFF)) {
					CAM_ERR(CAM_EEPROM, "Invalid Version");
					bVerNormal = FALSE;
					break;
				}
			}

			if (bVerNormal == TRUE) {
				memcpy(hw_phone_info, &e_ctrl->cal_data.mapdata[REAR_MODULE_FW_VERSION],
					HW_INFO_MAX_SIZE);
				hw_phone_info[HW_INFO_MAX_SIZE-1] = '\0';
				CAM_INFO(CAM_EEPROM, "hw_phone_info: %s", hw_phone_info);
			}
#if 0
			else {
				memcpy(hw_phone_info, HW_INFO, HW_INFO_MAX_SIZE);
				memcpy(sw_phone_info, SW_INFO, SW_INFO_MAX_SIZE);
				memcpy(vendor_phone_info, VENDOR_INFO, VENDOR_INFO_MAX_SIZE);
				memcpy(process_phone_info, PROCESS_INFO, PROCESS_INFO_MAX_SIZE);
				CAM_INFO(CAM_EEPROM, "Set Ver : %s %s %s %s",
					hw_phone_info, sw_phone_info,
					vendor_phone_info, process_phone_info);
			}
#endif
			CAM_INFO(CAM_EEPROM, "hw_phone_info: %s", hw_phone_info);

			for (i = 0; i < nVer; i++) {
				memcpy(tmp_hw_info, pBuf, HW_INFO_MAX_SIZE);
				pBuf += HW_INFO_MAX_SIZE;

				memcpy(tmp_sw_info, pBuf, SW_INFO_MAX_SIZE);
				pBuf += SW_INFO_MAX_SIZE;

				memcpy(tmp_vendor_info, pBuf, VENDOR_INFO_MAX_SIZE);
				tmp_vendor_info[VENDOR_INFO_MAX_SIZE-1] = '\0';
				pBuf += VENDOR_INFO_MAX_SIZE-1;

				memcpy(tmp_process_info, pBuf, PROCESS_INFO_MAX_SIZE);
				tmp_process_info[PROCESS_INFO_MAX_SIZE-1] = '\0';
				pBuf += PROCESS_INFO_MAX_SIZE;

				CAM_INFO(CAM_EEPROM, "[temp %d/%d] : %s %s %s %s",
					i, nVer, tmp_hw_info, tmp_sw_info,
					tmp_vendor_info, tmp_process_info);

				if (strcmp(hw_phone_info, tmp_hw_info) == 0) {
					memcpy(sw_phone_info, tmp_sw_info, SW_INFO_MAX_SIZE);
					memcpy(vendor_phone_info, tmp_vendor_info, VENDOR_INFO_MAX_SIZE);
					memcpy(process_phone_info, tmp_process_info, PROCESS_INFO_MAX_SIZE);
					CAM_INFO(CAM_EEPROM, "rear [%d] : %s %s %s %s",
						i, hw_phone_info, sw_phone_info,
						vendor_phone_info, process_phone_info);
				}
#if 0
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
				else if (strcmp(rear2_hw_phone_info, tmp_hw_info) == 0) {
					memcpy(rear2_sw_phone_info, tmp_sw_info, SW_INFO_MAX_SIZE);
					memcpy(rear2_vendor_phone_info, tmp_vendor_info, VENDOR_INFO_MAX_SIZE);
					memcpy(rear2_process_phone_info, tmp_process_info, PROCESS_INFO_MAX_SIZE);
					CAM_INFO(CAM_EEPROM, "rear2 [%d] : %s %s %s %s",
						i, rear2_hw_phone_info, rear2_sw_phone_info,
						rear2_vendor_phone_info, rear2_process_phone_info);
				}
#endif
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
				else if (strcmp(rear3_hw_phone_info, tmp_hw_info) == 0) {
					memcpy(rear3_sw_phone_info, tmp_sw_info, SW_INFO_MAX_SIZE);
					memcpy(rear3_vendor_phone_info, tmp_vendor_info, VENDOR_INFO_MAX_SIZE);
					memcpy(rear3_process_phone_info, tmp_process_info, PROCESS_INFO_MAX_SIZE);
					CAM_INFO(CAM_EEPROM, "rear3 [%d] : %s %s %s %s",
						i, rear3_hw_phone_info, rear3_sw_phone_info,
						rear3_vendor_phone_info, rear3_process_phone_info);
				}
#endif
				else if (strcmp(front_hw_phone_info, tmp_hw_info) == 0) {
					memcpy(front_sw_phone_info, tmp_sw_info, SW_INFO_MAX_SIZE);
					memcpy(front_vendor_phone_info, tmp_vendor_info, VENDOR_INFO_MAX_SIZE);
					memcpy(front_process_phone_info, tmp_process_info, PROCESS_INFO_MAX_SIZE);
					CAM_INFO(CAM_EEPROM, "front [%d] : %s %s %s %s",
						i, front_hw_phone_info, front_sw_phone_info,
						front_vendor_phone_info, front_process_phone_info);
				}
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
				else if (strcmp(front2_hw_phone_info, tmp_hw_info) == 0) {
					memcpy(front2_sw_phone_info, tmp_sw_info, SW_INFO_MAX_SIZE);
					memcpy(front2_vendor_phone_info, tmp_vendor_info, VENDOR_INFO_MAX_SIZE);
					memcpy(front2_process_phone_info, tmp_process_info, PROCESS_INFO_MAX_SIZE);
					CAM_INFO(CAM_EEPROM, "front2 [%d] : %s %s %s %s",
						i, front2_hw_phone_info, front2_sw_phone_info,
						front2_vendor_phone_info, front2_process_phone_info);
				}
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
				else if (strcmp(front3_hw_phone_info, tmp_hw_info) == 0) {
					memcpy(front3_sw_phone_info, tmp_sw_info, SW_INFO_MAX_SIZE);
					memcpy(front3_vendor_phone_info, tmp_vendor_info, VENDOR_INFO_MAX_SIZE);
					memcpy(front3_process_phone_info, tmp_process_info, PROCESS_INFO_MAX_SIZE);
					CAM_INFO(CAM_EEPROM, "front3 [%d] : %s %s %s %s",
						i, front3_hw_phone_info, front3_sw_phone_info,
						front3_vendor_phone_info, front3_process_phone_info);
				}
#endif
#endif
				else {
					CAM_INFO(CAM_EEPROM, "invalid hwinfo: %s", tmp_hw_info);
				}
			}
			memset(read_buffer, 0x00, e_ctrl->cal_data.num_data);
		} else {
			CAM_ERR(CAM_EEPROM, "Invalid direction");
			rc = -EINVAL;
		}
	}

	return rc;
#endif
}
