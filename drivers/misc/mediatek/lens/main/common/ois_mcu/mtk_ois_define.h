#ifndef MTK_OIS_MCU_DEFINE_H
#define MTK_OIS_MCU_DEFINE_H

#include <linux/gpio.h>
#include <linux/platform_device.h>

#define OIS_DRV_NAME "mtk_ois_drv"

#define OIS_I2C_SLAVE_ADDR               0x18
#define MCU_I2C_SLAVE_R_ADDR             0xC5
#define MCU_I2C_SLAVE_W_ADDR             0xC4
#define MAIN_EEPROM_I2C_SLAVE_ADDR       0xA0
#define OIS_GYRO_SCALE_FACTOR_LSM6DSO    114
#define MAX_EFS_DATA_LENGTH              30
#define OIS_GYRO_CAL_VALUE_FROM_EFS      "/efs/FactoryApp/camera_ois_gyro_cal"

#define OIS_DEBUG
#ifdef OIS_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_info(OIS_DRV_NAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

#define LOG_ERR(format, args...)                                               \
	pr_err(OIS_DRV_NAME " [%s] " format, __func__, ##args)

#define LOG_WARN(format, args...)                                               \
	pr_warn(OIS_DRV_NAME " [%s] " format, __func__, ##args)

#define LOG_DBG(format, args...)                                               \
	pr_debug(OIS_DRV_NAME " [%s] " format, __func__, ##args)

#endif

