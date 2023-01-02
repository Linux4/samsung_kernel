/*
 * file create by gaozhengwei for huaqin 2021.05.20
 */

#ifndef _LCD_BIAS_H
#define _LCD_BIAS_H

#ifdef BUILD_LK
#include <platform/mt_i2c.h>
#endif

#define LCD_BIAS_VPOS_ADDR    0x00
#define LCD_BIAS_VNEG_ADDR    0x01
#define LCD_BIAS_APPS_ADDR    0x03
//#define LCD_BIAS_OUTPUT_APPS  0x43

#ifdef BUILD_LK
#define LCD_BIAS_I2C_BUSNUM   5	/* for I2C channel 0 */
#define LCD_BIAS_I2C_ADDR       0x3E /*for I2C slave dev addr*/

#define LCD_BIAS_ST_MODE         ST_MODE
#define LCD_BIAS_MAX_ST_MODE_SPEED 100  /* khz */

#define GPIO_LCD_BIAS_ENP_PIN (GPIO21 | 0x80000000)
#define GPIO_LCD_BIAS_ENN_PIN (GPIO22 | 0x80000000)

#define LCD_BIAS_PRINT printf

#else

#define LCD_BIAS_PRINT printk

/* DTS state */
typedef enum {
	LCD_BIAS_GPIO_STATE_ENP0,
	LCD_BIAS_GPIO_STATE_ENP1,
	LCD_BIAS_GPIO_STATE_ENN0,
	LCD_BIAS_GPIO_STATE_ENN1,
	LCD_BIAS_GPIO_STATE_MAX,	/* for array size */
} LCD_BIAS_GPIO_STATE;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
void lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE s);
#endif

typedef enum {
	FIRST_VSP_AFTER_VSN = 0,
	FIRST_VSN_AFTER_VSP = 1
} LCD_BIAS_POWER_ON_SEQUENCE;

#ifndef CONFIG_HQ_EXT_LCD_BIAS
extern void pmic_lcd_bias_set_vspn_vol(unsigned int value);
extern void pmic_lcd_bias_set_vspn_en(unsigned int en, unsigned int seq);
#endif

void lcd_bias_set_vspn(unsigned int en, unsigned int seq, unsigned int value);

#endif /* _LCM_BIAS_H */

