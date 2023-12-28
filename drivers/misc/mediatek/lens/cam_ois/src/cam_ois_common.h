#ifndef CAM_OIS_COMMON_H
#define CAM_OIS_COMMON_H

#include <linux/gpio.h>
#include <linux/platform_device.h>

#define OIS_DRV_NAME "CAM_OIS D/D"

#define LOG_INF(format, args...)                                               \
	pr_info(OIS_DRV_NAME " [%s] " format, __func__, ##args)

#define LOG_ERR(format, args...)                                               \
	pr_err(OIS_DRV_NAME " [%s] " format, __func__, ##args)

#define LOG_WARN(format, args...)                                              \
	pr_warn(OIS_DRV_NAME " [%s] " format, __func__, ##args)

#define LOG_DBG(format, args...)                                               \
	pr_debug(OIS_DRV_NAME " [%s] " format, __func__, ##args)

#endif
