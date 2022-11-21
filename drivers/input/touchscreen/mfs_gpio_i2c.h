#ifndef _MFS_GPIO_I2C_H_
#define _MFS_GPIO_I2C_H_

//--------------------------------------------------
// Functions
//--------------------------------------------------

unsigned char mfs_gpio_i2c_write( unsigned char ucAddress, unsigned char *pucData , int nLength );
unsigned char mfs_gpio_i2c_read( unsigned char ucAddress, unsigned char *pucData, int nLength );


#endif
