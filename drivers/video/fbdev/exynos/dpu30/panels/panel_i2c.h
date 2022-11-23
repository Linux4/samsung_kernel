#ifndef __PANEL_I2C_H__
#define __PANEL_I2C_H__
#include <linux/i2c.h>

int panel_i2c_write(unsigned char reg, unsigned char value);

#endif /* __PANEL_I2C_H__ */
