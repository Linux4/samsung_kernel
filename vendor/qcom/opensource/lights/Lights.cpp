/*
 * Copyright (C) 2021, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "vendor.qti.lights"

#include "Lights.h"
#include <log/log.h>
#include <android-base/logging.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace {

enum rgb_led {
    LED_RED,
    LED_GREEN,
    LED_BLUE,
};

static const char *rgb_led_name[] = {
    [LED_RED] = "red",
    [LED_GREEN] = "green",
    [LED_BLUE] = "blue",
};

static int write_str_to_file(const char *path, char const *str) {
    int fd;

    fd = open(path, O_WRONLY);
    if (fd >= 0) {
        ssize_t amt = write(fd, str, strlen(str));
        close(fd);

        if (amt == -1) {
            ALOGE("Write to %s failed: %d", path, -errno);
            return -errno;
        }

        return 0;
    }

    ALOGE("Could not open %s: %d", path, -errno);
    return -errno;
}

static int write_int_to_file(const char *path, int value) {
    int fd;

    fd = open(path, O_WRONLY);
    if (fd >= 0) {
        char buf[16];
        size_t bytes = snprintf(buf, sizeof(buf), "%d\n", value);
        ssize_t amt = write(fd, buf, bytes);
        close(fd);

        if (amt == -1) {
            ALOGE("Write to %s failed: %d", path, -errno);
            return -errno;
        }

        return 0;
    }

    ALOGE("Could not open %s: %d", path, -errno);
    return -errno;
}

static int setLedDelayParams(enum rgb_led led, int flashOnMs, int flashOffMs) {
    char file[48];
    int rc;

    snprintf(file, sizeof(file), "/sys/class/leds/%s/delay_on", rgb_led_name[led]);
    rc = write_int_to_file(file, flashOnMs);
    if (rc < 0)
        goto out;

    snprintf(file, sizeof(file), "/sys/class/leds/%s/delay_off", rgb_led_name[led]);
    rc = write_int_to_file(file, flashOffMs);
    if (rc < 0)
        goto out;

    return 0;
out:
    ALOGE("%s does not support timer trigger", rgb_led_name[led]);
    return rc;
}

static int setLedBrightness(enum rgb_led led, int brightness) {
    int rc;
    char file[48];

    snprintf(file, sizeof(file), "/sys/class/leds/%s/brightness", rgb_led_name[led]);
    rc = write_int_to_file(file, brightness);
    if (rc < 0)
        return rc;

    /* Reset trigger to timer if LED's brightness is cleared */
    if (brightness == 0) {
        snprintf(file, sizeof(file), "/sys/class/leds/%s/trigger", rgb_led_name[led]);
        rc = write_str_to_file(file, "timer");
        if (rc < 0)
            ALOGE("Couldn't reset timer trigger on %s led: %d", rgb_led_name[led], rc);
    }

    return rc;
}
} // namespace anonymous

namespace aidl {
namespace android {
namespace hardware {
namespace light {

const static std::vector<LightType> kLogicalLights = {
    LightType::BACKLIGHT,
    LightType::KEYBOARD,
    LightType::BUTTONS,
    LightType::BATTERY,
    LightType::NOTIFICATIONS,
    LightType::ATTENTION,
    LightType::BLUETOOTH,
    LightType::WIFI,
};

Lights::Lights() {
    for (auto i = kLogicalLights.begin(); i != kLogicalLights.end(); i++) {
        HwLight hwLight{};
        hwLight.id = (int)(*i);
        hwLight.type = *i;
        hwLight.ordinal = 0;
        mAvailableLights.emplace_back(hwLight);
    }
}

int Lights::setRgbLedsParams(const HwLightState& state) {
    int rc = 0;
    int const colorRGB = state.color;
    int const red = (colorRGB >> 16) & 0xFF;
    int const green = (colorRGB >> 8) & 0xFF;
    int const blue = colorRGB & 0xFF;

    switch (state.flashMode) {
    case FlashMode::TIMED:
        if (!!red)
            rc = setLedDelayParams(LED_RED, state.flashOnMs, state.flashOffMs);
        if (!!green)
            rc |= setLedDelayParams(LED_GREEN, state.flashOnMs, state.flashOffMs);
        if (!!blue)
            rc |= setLedDelayParams(LED_BLUE, state.flashOnMs, state.flashOffMs);
        /* Fallback to constant on if blinking is not supported */
        if (rc == 0)
            break;
    case FlashMode::NONE:
        FALLTHROUGH_INTENDED;
    default:
        rc = setLedBrightness(LED_RED, red);
        rc |= setLedBrightness(LED_GREEN, green);
        rc |= setLedBrightness(LED_BLUE, blue);
        break;
    }

    ALOGD("colorRGB = %08X, onMs = %d, offMs = %d, rc: %d", colorRGB, state.flashOnMs, state.flashOffMs, rc);

    return rc;
}

ndk::ScopedAStatus Lights::setLightState(int id, const HwLightState& state) {
    if (id < 0 || id >= mAvailableLights.size()) {
        ALOGE("Invalid Light id : %d", id);
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    HwLight const& light = mAvailableLights[id];

    // Don't set LEDs for other notifications if battery notification is set
    if (light.type != LightType::BATTERY && mBatteryNotification)
        return ndk::ScopedAStatus::ok();

    int ret = setRgbLedsParams(state);
    if (ret != 0)
            return ndk::ScopedAStatus::fromServiceSpecificError(ret);

    if (light.type == LightType::BATTERY)
        mBatteryNotification = (state.color & 0x00FFFFFF);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Lights::getLights(std::vector<HwLight>* lights) {
    for (auto i = mAvailableLights.begin(); i != mAvailableLights.end(); i++) {
        lights->push_back(*i);
    }
    return ndk::ScopedAStatus::ok();
}

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl
