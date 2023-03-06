/** Samsung Exynos5 SoC series FIMC-IS EEPROM driver** exynos5 fimc-is core functions** Copyright (c) 2011 Samsung Electronics Co., Ltd** This program is free software; you can redistribute it and/or modify* it under the terms of the GNU General Public License version 2 as* published by the Free Software Foundation.*/struct fimc_is_eeprom_gpio {	bool use_i2c_pinctrl;	char *sda;	char *scl;	char *pinname;	int pinfunc_on;	int pinfunc_off;};#ifdef CONFIG_CAMERA_SUPPORT_MULTI_EEPROMstruct fimc_is_eeprom_ext_module_info {	bool use_module_i2c_check;	int actuator_id;};#endifstruct fimc_is_device_eeprom {
	struct v4l2_device				v4l2_dev;
	struct platform_device				*pdev;
	unsigned long					state;
	struct exynos_platform_fimc_is_sensor		*pdata;
	struct i2c_client			*client;
	struct fimc_is_core		*core;
	struct fimc_is_eeprom_gpio gpio;#ifdef CONFIG_CAMERA_SUPPORT_MULTI_EEPROM	struct fimc_is_eeprom_ext_module_info ext_module_info;#endif
	int driver_data;
};
