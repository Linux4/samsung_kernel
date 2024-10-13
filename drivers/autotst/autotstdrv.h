
#ifndef _AUTOTSTDRV_20130207_H__
#define _AUTOTSTDRV_20130207_H__

//-----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
//-----------------------------------------------------------------------------

#include <asm/ioctl.h>

//
//------------------------------------------------------------------------------
//
#define AUTOTST_IOCTL_MAGIC       'A'

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
struct autotst_i2c_info_t {
    unsigned char  bus;
    unsigned char  rcv0;
    unsigned short addr;

    unsigned short reg;
    unsigned char  regBits; // 8 or 16
    unsigned char  rcv1;

    unsigned char  rcv2;
    unsigned char  data_len;
    unsigned char  data[16];

};

//
#define AUTOTST_IOCTL_I2C_READ     _IOWR(AUTOTST_IOCTL_MAGIC, 0x00, struct autotst_i2c_info_t)
#define AUTOTST_IOCTL_I2C_WRITE    _IOWR(AUTOTST_IOCTL_MAGIC, 0x01, struct autotst_i2c_info_t)

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define AUTOTST_GPIO_DIR_IN       0
#define AUTOTST_GPIO_DIR_OUT      1

struct autotst_gpio_info_t {
    unsigned short gpio;
    unsigned char  dir;
    unsigned char  val;
    unsigned char  pup_enb;
    unsigned char  pup;
    unsigned char  pdwn_enb;
    unsigned char  pdwn;
};

#define AUTOTST_IOCTL_GPIO_INIT    _IOWR(AUTOTST_IOCTL_MAGIC, 0x10, struct autotst_gpio_info_t)
#define AUTOTST_IOCTL_GPIO_GET     _IOWR(AUTOTST_IOCTL_MAGIC, 0x11, struct autotst_gpio_info_t)
#define AUTOTST_IOCTL_GPIO_SET     _IOW (AUTOTST_IOCTL_MAGIC, 0x12, struct autotst_gpio_info_t)

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define AUTOTST_LCD_DATA_NUM       32
#define AUTOTST_IOCTL_LCD_DATA     _IOW (AUTOTST_IOCTL_MAGIC, 0x30, int)
#define  AUTOTST_IOCTL_LCD_MIPI_ON _IOW (AUTOTST_IOCTL_MAGIC, 0x31, int)
#define  AUTOTST_IOCTL_LCD_MIPI_OFF _IOW (AUTOTST_IOCTL_MAGIC, 0x32, int)

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define AUTOTST_KEY_INVALID_ROW    0xFFEE
#define AUTOTST_KEY_INVALID_COL    0xFFEE

struct autotst_key_info_t {
    unsigned short val;  // key value
    unsigned short row;  //
    unsigned short col;
	unsigned short gio;
};

#define AUTOTST_IOCTL_GET_KEYINFO  _IOWR(AUTOTST_IOCTL_MAGIC, 0x40, struct autotst_key_info_t)


//-----------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif // __cplusplus
//-----------------------------------------------------------------------------

#endif // _AUTOTSTDRV_20130207_H__
