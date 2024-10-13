/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sensorlist.h"

#include <math.h>

#include "hubdefs.h"

using namespace android;

const int kVersion = 1;

#define USE_ACC_GYRO
#undef USE_SD_SC
#define USE_MAG
#define USE_LIGHT_PROC
#define USE_PRESS
#define USE_EXYNOS_FUSION
#undef USE_TEMP
#undef USE_ORIENT
#undef USE_DOUBLE_TAP
#undef USE_DOUBLE_TWIST
#undef USE_SYNC
#undef USE_VIRTUAL_SENSOR
#undef USE_DEVICE_ORIENT

//MUST match max rate with sensor driver
#define SUPPORT_400HZ
#undef SUPPORT_1600HZ

#ifdef USE_ACC_GYRO
const float kMinSampleRateHzAccel = 12.5f;
#if defined(SUPPORT_1600HZ)
const float kMaxSampleRateHzAccel = 1600.0f;
#elif defined(SUPPORT_400HZ)
const float kMaxSampleRateHzAccel = 400.0f;
#else
const float kMaxSampleRateHzAccel = 200.0f;
#endif
const float kAccelRangeG = 8.0f;
extern const float kScaleAccel = (8.0f * 9.81f / 32768.0f);

const float kMinSampleRateHzGyro = 12.5f;
#if defined(SUPPORT_1600HZ)
const float kMaxSampleRateHzGyro = 1600.0f;
#else
const float kMaxSampleRateHzGyro = 400.0f;
#endif
#endif

#ifdef USE_MAG
const float kMinSampleRateHzMag = 5.0f;
const float kMaxSampleRateHzMag = 100.0f;
#endif
extern const float kScaleMag = 0.0625f;         // 1.0f / 16.0f

#ifdef USE_LIGHT_PROC
const float kMinSampleRateHzLight = 5.0f;
const float kMaxSampleRateHzLight = 5.0f;

const float kMinSampleRateHzProximity = 5.0f;
const float kMaxSampleRateHzProximity = 5.0f;
#endif

#ifdef USE_PRESS
const float kMinSampleRateHzPressure = 1.0f;
const float kMaxSampleRateHzPressure = 20.0f;
#endif

#ifdef USE_TEMP
const float kMinSampleRateHzPolling = 0.1f;
const float kMaxSampleRateHzPolling = 25.0f;

const float kMinSampleRateHzTemperature = kMinSampleRateHzPolling;
const float kMaxSampleRateHzTemperature = kMaxSampleRateHzPolling;
#endif

#if defined(USE_ORIENT) || defined (USE_VIRTUAL_SENSOR) || defined (USE_EXYNOS_FUSION)
const float kMinSampleRateHzOrientation = 12.5f;
const float kMaxSampleRateHzOrientation = 200.0f;
#endif

#ifdef DIRECT_REPORT_ENABLED
constexpr uint32_t kDirectReportFlagAccel = (
        // support up to rate level fast (nominal 200Hz);
        (SENSOR_DIRECT_RATE_FAST << SENSOR_FLAG_SHIFT_DIRECT_REPORT)
        // support ashmem and gralloc direct channel
        | SENSOR_FLAG_DIRECT_CHANNEL_ASHMEM
        | SENSOR_FLAG_DIRECT_CHANNEL_GRALLOC);
constexpr uint32_t kDirectReportFlagGyro = (
        // support up to rate level fast (nominal 200Hz);
        (SENSOR_DIRECT_RATE_FAST << SENSOR_FLAG_SHIFT_DIRECT_REPORT)
        // support ashmem and gralloc direct channel
        | SENSOR_FLAG_DIRECT_CHANNEL_ASHMEM
        | SENSOR_FLAG_DIRECT_CHANNEL_GRALLOC);
#else
constexpr uint32_t kDirectReportFlagAccel = 0;
constexpr uint32_t kDirectReportFlagGyro = 0;
#endif

/*
 * The fowllowing max count is determined by the total number of blocks
 * avaliable in the shared nanohub buffer and number of samples each type of
 * event can hold within a buffer block.
 * For bullhead's case, there are 239 blocks in the shared sensor buffer and
 * each block can hold 30 OneAxis Samples, 15 ThreeAxis Samples or 24
 * RawThreeAxies Samples.
 */
#if defined(USE_SD_SC) || defined(USE_LIGHT_PROC)
const int kMaxOneAxisEventCount = 7170;
#endif
const int kMaxThreeAxisEventCount = 3585;
const int kMaxRawThreeAxisEventCount = 5736;

const int kMinFifoReservedEventCount = 20;

#define MINDELAY(x) ((int32_t)ceil(1.0E6f/(x)))

#ifdef USE_TEMP
const char SENSOR_STRING_TYPE_INTERNAL_TEMPERATURE[] = "com.google.sensor.internal_temperature";
#endif
#ifdef USE_SYNC
const char SENSOR_STRING_TYPE_SYNC[] = "com.google.sensor.sync";
#endif
#ifdef USE_DOUBLE_TWIST
const char SENSOR_STRING_TYPE_DOUBLE_TWIST[] = "com.google.sensor.double_twist";
#endif
#ifdef USE_DOUBLE_TAP
const char SENSOR_STRING_TYPE_DOUBLE_TAP[] = "com.google.sensor.double_tap";
#endif
#ifdef USE_EXYNOS_FUSION
const char SENSOR_STRING_TYPE_ANY_MOTION[] = "com.google.sensor.any_motion";
const char SENSOR_STRING_TYPE_PEDOMETER[] = "com.google.sensor.pedometer";
#endif

vendor_sensor_list_t vensor_sensor_list[] = {
    {unactive_sensor, default_vendor},
    {"TMD3725 Proximity Sensor", "AMS"},
    {"TMD3725 Light Sensor", "AMS"},
    {"BMI320 accelerometer", "Bosch"},
    {"BMI320 accelerometer (uncalibrated)", "Bosch"},
    {"BMI320 gyro", "Bosch"},
    {"BMI320 gyro (uncalibrated)", "Bosch"},
    {"AKM_AK09918 magnetometer", "AKM"},
    {"AKM_AK09918 magnetometer (uncalibrated)", "AKM"},
    {"BMP380 pressure", "Bosch"},
};

sensor_t kSensorList[] = {
#ifdef USE_LIGHT_PROC
    {
        NULL,
        NULL,
        kVersion,
        COMMS_SENSOR_PROXIMITY,
        SENSOR_TYPE_PROXIMITY,
        5.0f,                                          // maxRange (cm)
        1.0f,                                          // resolution (cm)
        7.0f,                                          // XXX power
        MINDELAY(kMaxSampleRateHzProximity),           // minDelay
        300,                                           // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_PROXIMITY,
        "",                                            // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzProximity),    // maxDelay
        SENSOR_FLAG_WAKE_UP | SENSOR_FLAG_ON_CHANGE_MODE,
        { NULL, NULL }
    },
    {
        NULL,
        NULL,
        kVersion,
        COMMS_SENSOR_LIGHT,
        SENSOR_TYPE_LIGHT,
        43000.0f,                                  // maxRange (lx)
        10.0f,                                     // XXX resolution (lx)
        7.0f,                                      // XXX power
        MINDELAY(kMaxSampleRateHzLight),           // minDelay
        kMinFifoReservedEventCount,                // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                     // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_LIGHT,
        "",                                        // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzLight),    // maxDelay
        SENSOR_FLAG_ON_CHANGE_MODE,
        { NULL, NULL }
    },
#endif
#ifdef USE_ACC_GYRO
    {
        NULL,
        NULL,
        kVersion,
        COMMS_SENSOR_ACCEL,
        SENSOR_TYPE_ACCELEROMETER,
        GRAVITY_EARTH * kAccelRangeG,              // maxRange
        kScaleAccel,                               // resolution
        0.15f,                                      // XXX power
        MINDELAY(kMaxSampleRateHzAccel),           // minDelay
#if defined(SUPPORT_400HZ)
        3000,                                      // XXX fifoReservedEventCount
#else
        1000,
#endif
        kMaxRawThreeAxisEventCount,                // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_ACCELEROMETER,
        "",                                        // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzAccel),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE | kDirectReportFlagAccel,
        { NULL, NULL }
    },
    {
        NULL,
        NULL,
        kVersion,
        COMMS_SENSOR_ACCEL_UNCALIBRATED,
        SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED,
        GRAVITY_EARTH * kAccelRangeG,              // maxRange
        kScaleAccel,                               // resolution
        0.15f,                                      // XXX power
        MINDELAY(kMaxSampleRateHzAccel),           // minDelay
#if defined(SUPPORT_400HZ)
        3000,                                      // XXX fifoReservedEventCount
#else
        1000,
#endif
        kMaxRawThreeAxisEventCount,                // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_ACCELEROMETER_UNCALIBRATED,
        "",                                        // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzAccel),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE | kDirectReportFlagAccel,
        { NULL, NULL }
    },
    {
        NULL,
        NULL,
        kVersion,
        COMMS_SENSOR_GYRO,
        SENSOR_TYPE_GYROSCOPE,
        1000.0f * M_PI / 180.0f,                   // maxRange
        1000.0f * M_PI / (180.0f * 32768.0f),      // resolution
        0.5f,                                      // XXX power
        MINDELAY(kMaxSampleRateHzGyro),            // minDelay
        kMinFifoReservedEventCount,                // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                   // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_GYROSCOPE,
        "",                                        // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzGyro),     // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE | kDirectReportFlagGyro,
        { NULL, NULL }
    },
    {
        NULL,
        NULL,
        kVersion,
        COMMS_SENSOR_GYRO_UNCALIBRATED,
        SENSOR_TYPE_GYROSCOPE_UNCALIBRATED,
        1000.0f * M_PI / 180.0f,                   // maxRange
        1000.0f * M_PI / (180.0f * 32768.0f),      // resolution
        0.5f,                                      // XXX power
        MINDELAY(kMaxSampleRateHzGyro),            // minDelay
        kMinFifoReservedEventCount,                // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                   // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_GYROSCOPE_UNCALIBRATED,
        "",                                        // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzGyro),     // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE | kDirectReportFlagGyro,
        { NULL, NULL }
    },
#endif
#ifdef USE_MAG
    {
        NULL,
        NULL,
        kVersion,
        COMMS_SENSOR_MAG,
        SENSOR_TYPE_MAGNETIC_FIELD,
        1300.0f,                                   // XXX maxRange
        0.1f,                                      // XXX resolution
        1.1f,                                      // XXX power
        (int32_t)(1.0E6f / kMaxSampleRateHzMag),   // minDelay
        600,                                       // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                   // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_MAGNETIC_FIELD,
        "",                                        // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzMag),      // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
    {
        NULL,
        NULL,
        kVersion,
        COMMS_SENSOR_MAG_UNCALIBRATED,
        SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
        1300.0f,                                        // XXX maxRange
        0.1f,                                           // XXX resolution
        1.1f,                                           // XXX power
        (int32_t)(1.0E6f / kMaxSampleRateHzMag),        // minDelay
        600,                                            // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                        // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
        "",                                             // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzMag),           // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
#endif
#ifdef USE_PRESS
    {
        NULL,
        NULL,
        kVersion,
        COMMS_SENSOR_PRESSURE,
        SENSOR_TYPE_PRESSURE,
        1100.0f,                                      // maxRange (hPa)
        0.005f,                                       // resolution (hPa)
        0.68f,                                         // XXX power
        MINDELAY(kMaxSampleRateHzPressure),           // minDelay
        300,                                          // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                        // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_PRESSURE,
        "",                                           // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzPressure),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
#endif
#ifdef USE_TEMP
    {
        NULL,
        NULL,
        kVersion,
        COMMS_SENSOR_TEMPERATURE,
        SENSOR_TYPE_INTERNAL_TEMPERATURE,
        85.0f,                                           // maxRange (degC)
        0.01,                                            // resolution (degC)
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzTemperature),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                           // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_INTERNAL_TEMPERATURE,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzTemperature),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
#endif
#ifdef USE_EXYNOS_FUSION
    {
        "Step detector",
        "Samsung",
        kVersion,
        COMMS_SENSOR_STEP_DETECTOR,
        SENSOR_TYPE_STEP_DETECTOR,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.0f,                                   // XXX power
        0,                                      // minDelay
        100,                                    // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_STEP_DETECTOR,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_SPECIAL_REPORTING_MODE,
        { NULL, NULL }
    },
    {
        "Step counter",
        "Samsung",
        kVersion,
        COMMS_SENSOR_STEP_COUNTER,
        SENSOR_TYPE_STEP_COUNTER,
        1.0f,                                   // XXX maxRange
        1.0f,                                   // resolution
        0.0f,                                   // XXX power
        0,                                      // minDelay
        kMinFifoReservedEventCount,             // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_STEP_COUNTER,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_ON_CHANGE_MODE,
        { NULL, NULL }
    },
    {
        "Significant motion",
        "Samsung",
        kVersion,
        COMMS_SENSOR_SIGNIFICANT_MOTION,
        SENSOR_TYPE_SIGNIFICANT_MOTION,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.0f,                                   // XXX power
        -1,                                     // minDelay
        0,                                      // XXX fifoReservedEventCount
        0,                                      // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_SIGNIFICANT_MOTION,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_WAKE_UP | SENSOR_FLAG_ONE_SHOT_MODE,
        { NULL, NULL }
    },
    {
        "Any motion",
        "Samsung",
        kVersion,
        COMMS_SENSOR_ANY_MOTION,
        SENSOR_TYPE_ANY_MOTION,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.0f,                                   // XXX power
        0,                                     // minDelay
        0,                                      // XXX fifoReservedEventCount
        0,                                      // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_ANY_MOTION,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_WAKE_UP | SENSOR_FLAG_ON_CHANGE_MODE,
        { NULL, NULL }
    },
    {
        "Gravity",
        "Samsung",
        kVersion,
        COMMS_SENSOR_GRAVITY,
        SENSOR_TYPE_GRAVITY,
        1000.0f,                                         // maxRange
        1.0f,                                            // XXX resolution
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_GRAVITY,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
    {
        "Device Orientation",
        "Samsung",
        kVersion,
        COMMS_SENSOR_WINDOW_ORIENTATION,
        SENSOR_TYPE_DEVICE_ORIENTATION,
        3.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.1f,                                   // XXX power
        0,                                      // minDelay
        kMinFifoReservedEventCount,             // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_DEVICE_ORIENTATION,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_ON_CHANGE_MODE,
        { NULL, NULL }
    },
    {
        "Rotation Vector",
        "Samsung",
        kVersion,
        COMMS_SENSOR_ROTATION_VECTOR,
        SENSOR_TYPE_ROTATION_VECTOR,
        1.0f,                                            // maxRange
        0.001f,                                          // XXX resolution
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_ROTATION_VECTOR,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
    {
        "Game Rotation Vector",
        "Samsung",
        kVersion,
        COMMS_SENSOR_GAME_ROTATION_VECTOR,
        SENSOR_TYPE_GAME_ROTATION_VECTOR,
        1.0f,                                            // maxRange
        0.001f,                                          // XXX resolution
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        300,                                             // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_GAME_ROTATION_VECTOR,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
    {
        "Geomagnetic Rotation Vector",
        "Samsung",
        kVersion,
        COMMS_SENSOR_GEO_MAG,
        SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
        1.0f,                                            // maxRange
        0.001,                                           // XXX resolution
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
    {
        "Linear Acceleration",
        "Samsung",
        kVersion,
        COMMS_SENSOR_LINEAR_ACCEL,
        SENSOR_TYPE_LINEAR_ACCELERATION,
        1000.0f,                                         // maxRange
        1.0f,                                            // XXX resolution
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_LINEAR_ACCELERATION,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
    {
        "Tilt Detector",
        "Samsung",
        kVersion,
        COMMS_SENSOR_TILT,
        SENSOR_TYPE_TILT_DETECTOR,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.0f,                                   // XXX power
        0,                                      // minDelay
        kMinFifoReservedEventCount,             // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_TILT_DETECTOR,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_WAKE_UP | SENSOR_FLAG_SPECIAL_REPORTING_MODE,
        { NULL, NULL }
    },
    {
        "Orientation",
        "Samsung",
        kVersion,
        COMMS_SENSOR_ORIENTATION,
        SENSOR_TYPE_ORIENTATION,
        360.0f,                                          // maxRange (deg)
        1.0f,                                            // XXX resolution (deg)
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_ORIENTATION,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
	{
        "Stationary Detect",
        "Samsung",
        kVersion,
        COMMS_SENSOR_NO_MOTION,
        SENSOR_TYPE_STATIONARY_DETECT,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.0f,                                   // XXX power
        -1,                                     // minDelay
        0,                                      // XXX fifoReservedEventCount
        0,                                      // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_STATIONARY_DETECT,
        "",                                     // requiredPermission
        0,
        SENSOR_FLAG_ONE_SHOT_MODE,
        { NULL, NULL }
    },
    {
        "Pedometer",
        "Samsung",
        kVersion,
        COMMS_SENSOR_PEDOMETER,
        SENSOR_TYPE_PEDOMETER,
        1.0f,                                   // XXX maxRange
        1.0f,                                   // resolution
        0.0f,                                   // XXX power
        0,                                      // minDelay
        kMinFifoReservedEventCount,             // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_PEDOMETER,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_ON_CHANGE_MODE,
        { NULL, NULL }
    },
#else
#ifdef USE_ORIENT
    {
        "Orientation",
        "Google",
        kVersion,
        COMMS_SENSOR_ORIENTATION,
        SENSOR_TYPE_ORIENTATION,
        360.0f,                                          // maxRange (deg)
        1.0f,                                            // XXX resolution (deg)
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_ORIENTATION,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
#endif
#if USE_DEVICE_ORIENT
    {
        "Device Orientation",
        "Google",
        kVersion,
        COMMS_SENSOR_WINDOW_ORIENTATION,
        SENSOR_TYPE_DEVICE_ORIENTATION,
        3.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.1f,                                   // XXX power
        0,                                      // minDelay
        kMinFifoReservedEventCount,             // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_DEVICE_ORIENTATION,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_ON_CHANGE_MODE,
        { NULL, NULL }
    },
#endif
#ifdef USE_SD_SC
    {
        "BMI160 Step detector",
        "Bosch",
        kVersion,
        COMMS_SENSOR_STEP_DETECTOR,
        SENSOR_TYPE_STEP_DETECTOR,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.0f,                                   // XXX power
        0,                                      // minDelay
        100,                                    // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_STEP_DETECTOR,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_SPECIAL_REPORTING_MODE,
        { NULL, NULL }
    },
    {
        "BMI160 Step counter",
        "Bosch",
        kVersion,
        COMMS_SENSOR_STEP_COUNTER,
        SENSOR_TYPE_STEP_COUNTER,
        1.0f,                                   // XXX maxRange
        1.0f,                                   // resolution
        0.0f,                                   // XXX power
        0,                                      // minDelay
        kMinFifoReservedEventCount,             // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_STEP_COUNTER,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_ON_CHANGE_MODE,
        { NULL, NULL }
    },
#endif
#ifdef USE_VIRTUAL_SENSOR
    {
        "Tilt Detector",
        "Google",
        kVersion,
        COMMS_SENSOR_TILT,
        SENSOR_TYPE_TILT_DETECTOR,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.0f,                                   // XXX power
        0,                                      // minDelay
        kMinFifoReservedEventCount,             // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_TILT_DETECTOR,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_WAKE_UP | SENSOR_FLAG_SPECIAL_REPORTING_MODE,
        { NULL, NULL }
    },
    {
        "Significant motion",
        "Google",
        kVersion,
        COMMS_SENSOR_SIGNIFICANT_MOTION,
        SENSOR_TYPE_SIGNIFICANT_MOTION,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.0f,                                   // XXX power
        -1,                                     // minDelay
        0,                                      // XXX fifoReservedEventCount
        0,                                      // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_SIGNIFICANT_MOTION,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_WAKE_UP | SENSOR_FLAG_ONE_SHOT_MODE,
        { NULL, NULL }
    },
    {
        "Gravity",
        "Google",
        kVersion,
        COMMS_SENSOR_GRAVITY,
        SENSOR_TYPE_GRAVITY,
        1000.0f,                                         // maxRange
        1.0f,                                            // XXX resolution
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_GRAVITY,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
    {
        "Linear Acceleration",
        "Google",
        kVersion,
        COMMS_SENSOR_LINEAR_ACCEL,
        SENSOR_TYPE_LINEAR_ACCELERATION,
        1000.0f,                                         // maxRange
        1.0f,                                            // XXX resolution
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_LINEAR_ACCELERATION,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
    {
        "Rotation Vector",
        "Google",
        kVersion,
        COMMS_SENSOR_ROTATION_VECTOR,
        SENSOR_TYPE_ROTATION_VECTOR,
        1.0f,                                            // maxRange
        0.001f,                                          // XXX resolution
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_ROTATION_VECTOR,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
    {
        "Geomagnetic Rotation Vector",
        "Google",
        kVersion,
        COMMS_SENSOR_GEO_MAG,
        SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
        1.0f,                                            // maxRange
        0.001,                                           // XXX resolution
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        kMinFifoReservedEventCount,                      // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
    {
        "Game Rotation Vector",
        "Google",
        kVersion,
        COMMS_SENSOR_GAME_ROTATION_VECTOR,
        SENSOR_TYPE_GAME_ROTATION_VECTOR,
        1.0f,                                            // maxRange
        0.001f,                                          // XXX resolution
        0.0f,                                            // XXX power
        MINDELAY(kMaxSampleRateHzOrientation),           // minDelay
        300,                                             // XXX fifoReservedEventCount
        kMaxThreeAxisEventCount,                         // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_GAME_ROTATION_VECTOR,
        "",                                              // requiredPermission
        (long)(1.0E6f / kMinSampleRateHzOrientation),    // maxDelay
        SENSOR_FLAG_CONTINUOUS_MODE,
        { NULL, NULL }
    },
#endif
#endif
#ifdef USE_VIRTUAL_SENSOR
    {
        "Pickup Gesture",
        "Google",
        kVersion,
        COMMS_SENSOR_GESTURE,
        SENSOR_TYPE_PICK_UP_GESTURE,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.0f,                                   // XXX power
        -1,                                     // minDelay
        0,                                      // XXX fifoReservedEventCount
        0,                                      // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_PICK_UP_GESTURE,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_WAKE_UP | SENSOR_FLAG_ONE_SHOT_MODE,
        { NULL, NULL }
    },
#endif
#ifdef USE_SYNC
    {
        "Sensors Sync",
        "Google",
        kVersion,
        COMMS_SENSOR_SYNC,
        SENSOR_TYPE_SYNC,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.1f,                                   // XXX power
        0,                                      // minDelay
        kMinFifoReservedEventCount,             // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_SYNC,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_SPECIAL_REPORTING_MODE,
        { NULL, NULL }
    },
#endif
#ifdef USE_DOUBLE_TWIST
    {
        "Double Twist",
        "Google",
        kVersion,
        COMMS_SENSOR_DOUBLE_TWIST,
        SENSOR_TYPE_DOUBLE_TWIST,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.1f,                                   // XXX power
        0,                                      // minDelay
        kMinFifoReservedEventCount,             // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_DOUBLE_TWIST,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_WAKE_UP | SENSOR_FLAG_SPECIAL_REPORTING_MODE,
        { NULL, NULL }
    },
#endif
#ifdef USE_DOUBLE_TAP
    {
        "Double Tap",
        "Google",
        kVersion,
        COMMS_SENSOR_DOUBLE_TAP,
        SENSOR_TYPE_DOUBLE_TAP,
        1.0f,                                   // maxRange
        1.0f,                                   // XXX resolution
        0.1f,                                   // XXX power
        0,                                      // minDelay
        kMinFifoReservedEventCount,             // XXX fifoReservedEventCount
        kMaxOneAxisEventCount,                  // XXX fifoMaxEventCount
        SENSOR_STRING_TYPE_DOUBLE_TAP,
        "",                                     // requiredPermission
        0,                                      // maxDelay
        SENSOR_FLAG_SPECIAL_REPORTING_MODE,
        { NULL, NULL }
    },
#endif
};

extern const size_t kSensorCount = sizeof(kSensorList) / sizeof(sensor_t);
