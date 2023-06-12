/*
 * Copyright (C) 2011, SAMSUNG Corporation.
 * Author: YongTaek Lee  <ytk.lee@samsung.com> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


struct smuic_platform_data {
        unsigned int    usb_dm_gpio;
        unsigned int    usb_dp_gpio;
//        bool (*get_charger_full_status)(void);

};

struct smuic_driver_data{

	struct smuic_platform_data *pdata;

	unsigned int    smuic_dm_gpio;
	unsigned int    smuic_dp_gpio;

	struct mutex		lock;
	struct mutex		api_lock;

	struct timer_list	Charge_timer;
	struct timer_list	reCharge_start_timer; 

};
		
