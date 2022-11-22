/* board-garda-thermistor.c
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
        { 186 , 800 },
        { 191 , 790 },
        { 197 , 780 },
        { 203 , 770 },
        { 210 , 760 },
        { 217 , 750 },
        { 224 , 740 },
        { 232 , 730 },
        { 240 , 720 },
        { 248 , 710 },
        { 256 , 700 },
        { 265 , 690 },
        { 274 , 680 },
        { 283 , 670 },
        { 292 , 660 },
        { 301 , 650 },
        { 310 , 640 },
        { 320 , 630 },
        { 330 , 620 },
        { 340 , 610 },
        { 351 , 600 },
        { 362 , 590 },
        { 373 , 580 },
        { 384 , 570 },
        { 396 , 560 },
        { 408 , 550 },
        { 421 , 540 },
        { 434 , 530 },
        { 448 , 520 },
        { 462 , 510 },
        { 477 , 500 },
        { 493 , 490 },
        { 510 , 480 },
        { 527 , 470 },
        { 544 , 460 },
        { 561 , 450 },
        { 578 , 440 },
        { 595 , 430 },
        { 612 , 420 },
        { 629 , 410 },
        { 646 , 400 },
        { 664 , 390 },
        { 682 , 380 },
        { 700 , 370 },
        { 719 , 360 },
        { 738 , 350 },
        { 758 , 340 },
        { 778 , 330 },
        { 798 , 320 },
        { 819 , 310 },
        { 840 , 300 },
        { 862 , 290 },
        { 884 , 280 },
        { 906 , 270 },
        { 928 , 260 },
        { 950 , 250 },
        { 972 , 240 },
        { 995 , 230 },
        { 1018 , 220 },
        { 1041 , 210 },
        { 1064 , 200 },
        { 1088 , 190 },
        { 1112 , 180 },
        { 1136 , 170 },
        { 1160 , 160 },
        { 1184 , 150 },
        { 1207 , 140 },
        { 1230 , 130 },
        { 1252 , 120 },
        { 1274 , 110 },
        { 1296 , 100 },
        { 1317 , 90 },
        { 1338 , 80 },
        { 1359 , 70 },
        { 1380 , 60 },
        { 1401 , 50 },
        { 1422 , 40 },
        { 1442 , 30 },
        { 1462 , 20 },
        { 1482 , 10 },
        { 1502 , 0 },
        { 1522 , -10 },
        { 1542 , -20 },
        { 1562 , -30 },
        { 1582 , -40 },
        { 1602 , -50 },
        { 1619 , -60 },
        { 1636 , -70 },
        { 1653 , -80 },
        { 1669 , -90 },
        { 1685 , -100 },
        { 1701 , -110 },
        { 1717 , -120 },
        { 1731 , -130 },
        { 1745 , -140 },
        { 1758 , -150 },
        { 1771 , -160 },
        { 1785 , -170 },
        { 1798 , -180 },
        { 1811 , -190 },
        { 1823 , -200 },
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
