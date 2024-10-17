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
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-imx754.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-hp2.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-3lu.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-imx564.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/actuator/is-actuator-ak737x.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/flash/is-flash-s2mpb02-i2c.ko",
    "drivers/leds/leds-s2mpb02.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/laser_af/is-laser-af-vl53l8.ko",
    "drivers/gpu/drm/samsung/dpu/dp_ext_func/dp_ext_func.ko",
]
