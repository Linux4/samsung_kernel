# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

__power_platform_map =  {
    "pineapple": {
        "gki": [
            # keep sorted
            "drivers/rtc/sec_pon_alarm.ko",
            "drivers/cpufreq/cpufreq_limit.ko",
            "drivers/mfd/sec_ap_pmic.ko",
        ],
        "consolidate": [
            # keep sorted & in-tree modules only
            "drivers/pinctrl/qcom/secgpio_dvs.ko",
        ],
    },
}

def sec_power(target, variant):
    if not target in __power_platform_map:
        return []

    target_map = __power_platform_map[target]
    if not variant in target_map:
        return []

    if variant == "consolidate":
        return target_map[variant] + target_map["gki"]

    return target_map[variant]
