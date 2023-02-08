/* board-degas-thermistor.c
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

#define TEMP_AP_ADC_CHANEL 2

/* TODO: Temporary table. It should be updated */
static struct sec_therm_adc_table temper_table_ap[] = {
        { 132 , 800 },
        { 134 , 790 },
        { 137 , 780 },
        { 140 , 770 },
        { 143 , 760 },
        { 146 , 750 },
        { 150 , 740 },
        { 154 , 730 },
        { 158 , 720 },
        { 162 , 710 },
        { 167 , 700 },
        { 172 , 690 },
        { 177 , 680 },
        { 182 , 670 },
        { 187 , 660 },
        { 193 , 650 },
        { 199 , 640 },
        { 205 , 630 },
        { 211 , 620 },
        { 217 , 610 },
        { 224 , 600 },
        { 232 , 590 },
        { 240 , 580 },
        { 248 , 570 },
        { 256 , 560 },
        { 265 , 550 },
        { 272 , 540 },
        { 279 , 530 },
        { 286 , 520 },
        { 293 , 510 },
        { 301 , 500 },
        { 310 , 490 },
        { 319 , 480 },
        { 328 , 470 },
        { 337 , 460 },
        { 346 , 450 },
        { 356 , 440 },
        { 366 , 430 },
        { 376 , 420 },
        { 387 , 410 },
        { 398 , 400 },
        { 410 , 390 },
        { 422 , 380 },
        { 434 , 370 },
        { 447 , 360 },
        { 460 , 350 },
        { 474 , 340 },
        { 489 , 330 },
        { 504 , 320 },
        { 519 , 310 },
        { 534 , 300 },
        { 549 , 290 },
        { 564 , 280 },
        { 579 , 270 },
        { 594 , 260 },
        { 609 , 250 },
        { 628 , 240 },
        { 646 , 230 },
        { 664 , 220 },
        { 682 , 210 },
        { 700 , 200 },
        { 718 , 190 },
        { 736 , 180 },
        { 754 , 170 },
        { 772 , 160 },
        { 790 , 150 },
        { 810 , 140 },
        { 830 , 130 },
        { 850 , 120 },
        { 870 , 110 },
        { 890 , 100 },
        { 910 , 90 },
        { 930 , 80 },
        { 950 , 70 },
        { 970 , 60 },
        { 990 , 50 },
        { 1012 , 40 },
        { 1035 , 30 },
        { 1058 , 20 },
        { 1081 , 10 },
        { 1104 , 0 },
        { 1122 , -10 },
        { 1140 , -20 },
        { 1159 , -30 },
        { 1178 , -40 },
        { 1197 , -50 },
        { 1221 , -60 },
        { 1245 , -70 },
        { 1269 , -80 },
        { 1294 , -90 },
        { 1319 , -100 },
        { 1335 , -110 },
        { 1351 , -120 },
        { 1367 , -130 },
        { 1386 , -140 },
        { 1401 , -150 },
        { 1422 , -160 },
        { 1442 , -170 },
        { 1462 , -180 },
        { 1482 , -190 },
        { 1502 , -200 },
};

static struct sec_therm_platform_data sec_therm_pdata = {
	.adc_channel	= TEMP_AP_ADC_CHANEL,
	.adc_arr_size	= ARRAY_SIZE(temper_table_ap),
	.adc_table	= temper_table_ap,
};

struct platform_device sec_device_thermistor = {
	.name = "sec-thermistor",
	.id = -1,
	.dev.platform_data = &sec_therm_pdata,
};
#endif

void __init board_sec_thermistor_init(void)
{
#ifdef CONFIG_SEC_THERMISTOR
	platform_device_register(&sec_device_thermistor);
#endif
}
