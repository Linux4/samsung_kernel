/* board-h-thermistor.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>

#ifdef CONFIG_SEC_THERMISTOR
#include <mach/sec_thermistor.h>

static struct sec_therm_adc_table temper_table_ap[] = {
        {233, 900},
        {244, 890},
        {255, 880},
        {265, 870},
        {276, 860},
        {287, 850},
        {299, 840},
        {310, 830},
        {322, 820},
        {334, 810},
        {345, 800},
        {358, 790},
        {370, 780},
        {382, 770},
        {394, 760},
        {407, 750},
        {422, 740},
        {437, 730},
        {452, 720},
        {467, 710},
        {482, 700},
        {500, 690},
        {519, 680},
        {538, 670},
        {556, 660},
        {575, 650},
        {593, 640},
        {612, 630},
        {630, 620},
        {648, 610},
        {667, 600},
        {691, 590},
        {715, 580},
        {740, 570},
        {764, 560},
        {789, 550},
        {818, 540},
        {847, 530},
        {876, 520},
        {905, 510},
        {935, 500},
        {960, 490},
        {985, 480},
        {1010, 470},
        {1035, 460},
        {1060, 450},
        {1100, 440},
        {1140, 430},
        {1180, 420},
        {1220, 410},
        {1260, 400},
        {1295, 390},
        {1330, 380},
        {1365, 370},
        {1399, 360},
        {1434, 350},
        {1474, 340},
        {1515, 330},
        {1555, 320},
        {1596, 310},
        {1636, 300},
        {1682, 290},
        {1728, 280},
        {1775, 270},
        {1821, 260},
        {1867, 250},
        {1905, 240},
        {1944, 230},
        {1982, 220},
        {2021, 210},
        {2059, 200},
        {2097, 190},
        {2135, 180},
        {2173, 170},
        {2211, 160},
        {2249, 150},
        {2292, 140},
        {2334, 130},
        {2376, 120},
        {2418, 110},
        {2460, 100},
        {2499, 90},
        {2538, 80},
        {2577, 70},
        {2616, 60},
        {2654, 50},
        {2690, 40},
        {2725, 30},
        {2760, 20},
        {2795, 10},
        {2831, 0},
        {2865, -10},
        {2900, -20},
        {2935, -30},
        {2970, -40},
        {3005, -50},
        {3032, -60},
        {3060, -70},
        {3088, -80},
        {3116, -90},
        {3143, -100},
        {3168, -110},
        {3193, -120},
        {3217, -130},
        {3242, -140},
        {3267, -150},
        {3285, -160},
        {3304, -170},
        {3322, -180},
        {3341, -190},
        {3359, -200},
};

static struct sec_therm_platform_data sec_therm_pdata = {
	.adc_channel	= 0,
	.adc_arr_size	= ARRAY_SIZE(temper_table_ap),
	.adc_table	= temper_table_ap,
};

struct platform_device sec_device_thermistor = {
	.name = "sec-thermistor",
	.id = -1,
	.dev.platform_data = &sec_therm_pdata,
};
#endif

extern unsigned int system_rev;

void __init exynos5_universal5260_thermistor_init(void)
{
#ifdef CONFIG_SEC_THERMISTOR
	platform_device_register(&sec_device_thermistor);
#endif
}
