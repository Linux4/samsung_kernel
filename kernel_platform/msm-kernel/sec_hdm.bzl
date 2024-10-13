# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

__hdm_platform_map =  {
    "pineapple": {
        "gki": [
            # keep sorted & in-tree modules only
            "drivers/samsung/knox/hdm/hdm.ko",
        ],
        "consolidate": [
            # keep sorted
        ],
    },
}

def hdm_module_list(target, variant):
    if not target in __hdm_platform_map:
        return []

    target_map = __hdm_platform_map[target]
    if not variant in target_map:
        return []

    if variant == "consolidate":
        return target_map[variant] + target_map["gki"]

    return target_map[variant]
