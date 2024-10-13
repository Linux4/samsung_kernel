# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

MODEL_VARIANT_MODULE_LIST = [
    # Please add to model variant drivers
    # Chipset common driver have to add to {TARGET_SOC}_modules.bzl
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-imx616.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-imx906.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-imx258.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-gc05a3.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-gn5.ko",
    "drivers/leds/leds-sm5714-fled.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/flash/is-flash-sm5714.ko",
    "drivers/leds/leds-s2mf301.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/flash/is-flash-s2mf301.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/actuator/is-actuator-ak737x.ko",
    "drivers/samsung/pm/sec_pon_alarm.ko",
]
