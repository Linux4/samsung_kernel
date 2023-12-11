#ifndef _PM6125_FLASH_GPIO_H_
#define _PM6125_FLASH_GPIO_H_

#include "../cam_sensor_utils/cam_sensor_cmn_header.h"

/* DTS state */
typedef enum
{
	PM6125_FLASH_GPIO_STATE_ACTIVE,
	PM6125_FLASH_GPIO_STATE_SUSPEND,
	PM6125_FLASH_GPIO_STATE_MAX, /* for array size */
} PM6125_FLASH_GPIO_STATE;

enum
{
	CAMERA_SENSOR_FLASH_STATUS_OFF,
	CAMERA_SENSOR_FLASH_STATUS_LOW,
	CAMERA_SENSOR_FLASH_STATUS_HIGH
};

#define PM6125_PWM_PERIOD 50000
#define FLASH_FIRE_HIGH_MAXCURRENT 1315
#define FLASH_FIRE_LOW_MAXCURRENT 141
#define FLASH_FIRE_BRIGHTNESS_LEVEL_1 60  // 50mA
#define FLASH_FIRE_BRIGHTNESS_LEVEL_2 80  // 65mA
#define FLASH_FIRE_BRIGHTNESS_LEVEL_3 100 // 80mA Duty cycle 0.7
#define FLASH_FIRE_BRIGHTNESS_LEVEL_5 120 // 100mA
#define FLASH_FIRE_BRIGHTNESS_LEVEL_7 141 // 120mA

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
extern void pm6125_flash_gpio_select_state(PM6125_FLASH_GPIO_STATE s, enum camera_flash_opcode opcode, u64 flash_current);
extern void pm6125_flash_control_create_device(void);

int pm6125_flash_gpio_init_module(void);
void pm6125_flash_gpio_exit_module(void);
#endif /* _PM6125_FLASH_GPIO_H_*/
