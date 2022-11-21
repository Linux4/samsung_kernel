/*
 *  ss_thermal.h
 *  Samsung thermal mitigation through core wear levelling
 *
 *  Copyright (C) 2015 Samsung Electronics
 *
 */

#define CORE_MITIGATION_TEMP_VALUE 94
#define TSENS_NAME_MAX_LENGTH 20
#define SENSOR_ID_PERF0 4
#define SENSOR_ID_PERF1 5
#define SENSOR_ID_PERF2 6
#define SENSOR_ID_PERF3 7
#define SENSOR_ID_POWER_ALL 9

#define CORE_ID_PERF0 4
#define CORE_ID_PERF1 5
#define CORE_ID_PERF2 6
#define CORE_ID_PERF3 7

#define CORE_ID_POWER0 0
#define CORE_ID_POWER1 1
#define CORE_ID_POWER2 2
#define CORE_ID_POWER3 3

#define TEMP_STEP_UP_VALUE 5
#define TEMP_STEP_DOWN_VALUE 10
#define MAX_CORE_WEAR_LEVEL 6


struct core_mitigation_temp_unplug {
    unsigned int core_mitigation_temp_step[6];
    unsigned int core_to_unplug[6];
};

struct tsens_id_to_core {
    unsigned int sensor_id;
    unsigned int core_id;
    bool unplugged;
};
