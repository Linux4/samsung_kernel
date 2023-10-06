/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021.
 */
/* HS03 code for SR-SL6215-01-181 by gaochao at 20210713 start */

#ifndef __BATTERY_ID_VIA_ADC__
#define __BATTERY_ID_VIA_ADC__

/* Tab A8 code for SR-AX6300-01-181 by zhangyanlong at 20210817 start */
#ifdef  CONFIG_TARGET_UMS9230_4H10
/* Tab A8 code for SR-AX6300-01-181 by zhangyanlong at 20210817 end */
/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
enum multiBatteryID {
    BATTERY_SCUD_BYD = 0,
    BATTERY_ATL_NVT,
    BATTERY_SCUD_SDI,
    BATTERY_UNKNOWN,
};

#define BATTERY_SCUD_BYD_ID_VOLTAGE_UP     1250    // ID: 200K
#define BATTERY_SCUD_BYD_ID_VOLTAGE_LOW    1052    // 950
#define BATTERY_ATL_NVT_ID_VOLTAGE_UP      817     // 880
#define BATTERY_ATL_NVT_ID_VOLTAGE_LOW     600     // ID: 47K
#define BATTERY_SCUD_SDI_ID_VOLTAGE_UP     1009
#define BATTERY_SCUD_SDI_ID_VOLTAGE_LOW    903     // ID: 100K
/* Tab A7 T618 code for SR-AX6189A-01-108 by shixuanxuan at 20211223 start */
#endif

#if defined(CONFIG_UMS512_25C10_CHARGER)
enum multiBatteryID {
    BATTERY_SCUD_BYD = 0,
    BATTERY_ATL_NVT,
    BATTERY_SCUD_ATL,
    BATTERY_UNKNOWN,
};

#define BATTERY_SCUD_BYD_ID_VOLTAGE_UP    691    // ID: 30K
#define BATTERY_SCUD_BYD_ID_VOLTAGE_LOW   498
#define BATTERY_ATL_NVT_ID_VOLTAGE_UP     364     // ID: 10K
#define BATTERY_ATL_NVT_ID_VOLTAGE_LOW    226
#define BATTERY_SCUD_ATL_ID_VOLTAGE_UP    1009    // ID: 1000k
#define BATTERY_SCUD_ATL_ID_VOLTAGE_LOW   903
/* Tab A8 code for SR-AX6300-01-181 by zhangyanlong at 20210817 start */
#elif defined(CONFIG_TARGET_UMS512_1H10)
/* Tab A8 code for SR-AX6300-01-181 by zhangyanlong at 20210817 start */
/* Tab A7 T618 code for SR-AX6189A-01-108 by shixuanxuan at 20211223 end */
enum multiBatteryID {
    BATTERY_ATL_NVT = 0,
    BATTERY_SCUD_BYD,
    BATTERY_SCUD_ATL,
    BATTERY_UNKNOWN,
};

#define BATTERY_ATL_NVT_ID_VOLTAGE_UP     842     // ID: 47K
#define BATTERY_ATL_NVT_ID_VOLTAGE_LOW    680
#define BATTERY_SCUD_BYD_ID_VOLTAGE_UP    1182    // ID: 200K
#define BATTERY_SCUD_BYD_ID_VOLTAGE_LOW   1023
#define BATTERY_SCUD_ATL_ID_VOLTAGE_UP    435     // ID: 12K
#define BATTERY_SCUD_ATL_ID_VOLTAGE_LOW   263
#endif
/* Tab A8 code for SR-AX6300-01-181 by zhangyanlong at 20210817 end */

// static int bat_id_get_adc_info(struct device *dev);
int bat_id_get_adc_num(void);
int battery_get_bat_id_voltage(void);
int battery_get_bat_id(void);
#endif
/* HS03 code for SR-SL6215-01-181 by gaochao at 20210713 end */
