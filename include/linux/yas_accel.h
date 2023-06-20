/*
 * Copyright (c) 2010-2011 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef __YAS_ACCEL_H__
#define __YAS_ACCEL_H__

#ifdef CONFIG_YAS_ACC_DRIVER_LIS3DH
int yas_acc_driver_lis3dh_init(struct yas_acc_driver *f);
#endif

#ifdef CONFIG_YAS_ACC_DRIVER_BMA250
int yas_acc_driver_BMA25X_init(struct yas_acc_driver *f);
#endif


enum {
	#ifdef CONFIG_YAS_ACC_DRIVER_LIS3DH
	K3DH_ENABLED,
	#endif
	#ifdef CONFIG_YAS_ACC_DRIVER_BMA250
	BMA25X_ENABLED,
	#endif
	MAX_CHIP_NUM,
};

struct accel_platform_data {
	bool i2c_pull_up;
	int gpio_int;
	u32 irq_gpio_flags;
	int gpio_sda;
	u32 sda_gpio_flags;
	int gpio_scl;
	u32 scl_gpio_flags;
	const u8 *str_l6;
	const u8 *str_lvs1;
	u32 used_chip_flags;
	int used_chip;
	u32 position_flags;
	int position;
};
#define MAX_LEN_OF_NAME 10


#endif /*__YAS_ACCEL_H__ */
