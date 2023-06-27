#ifndef _BEAM_GPIO_I2C_H_
#define _BEAM_GPIO_I2C_H_


#if defined(CONFIG_MACH_CS05) 
#include <../arch/arm/mach-mmp/include/mach/mfp-pxa1088-cs05.h>
#endif

#define GPIO_BEAM_SCL			GPIO005_GPIO_5
#define GPIO_BEAM_SDA			GPIO006_GPIO_6
#define GPIO_BEAM_INT			mfp_to_gpio(GPIO127_GPIO_127)
#define GPIO_BEAM_MP_ON		mfp_to_gpio(GPIO029_GPIO_29)
#define GPIO_BEAM_PRJ_ON		mfp_to_gpio(GPIO004_GPIO_4)


#define BEAM_SCL    		mfp_to_gpio(GPIO_BEAM_SCL)
#define BEAM_SDA      		mfp_to_gpio(GPIO_BEAM_SDA)
#define BEAM_INT			mfp_to_gpio(GPIO_BEAM_INT)

//--------------------------------------------------
// Functions
//--------------------------------------------------

unsigned char beam_gpio_i2c_write( unsigned char ucAddress, unsigned char *pucData , int nLength );
unsigned char beam_gpio_i2c_read( unsigned char ucAddress, unsigned char *pucData, int nLength );


#define BEAM_I2C_EMUL

#ifdef BEAM_I2C_EMUL
int beam_i2c_read( struct i2c_client *client, unsigned short  typeaddr, char *buf, char *bufR, int count, int countR);
//int beam_i2c_read( struct i2c_client *client, unsigned short  typeaddr, char *buf, int count, int countR);
int beam_i2c_write(struct i2c_client *client, unsigned short  typeaddr, char *buf, int count);
#else
int beam_i2c_read(unsigned short  addr, int length, char *value);
int beam_i2c_write(unsigned short  addr, char *buf, int length);
#endif

#endif

