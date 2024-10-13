# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

__module_platform_map =  {
    "pineapple": {
        "gki": [
            # keep sorted & in-tree modules only
            "lcd.ko",
        ],
        "consolidate": [
            # keep sorted
        ],
    },
}

def sec_display(target, variant):
    if not target in __module_platform_map:
        return []

    target_map = __module_platform_map[target]
    if not variant in target_map:
        return []

    if variant == "consolidate":
        return target_map[variant] + target_map["gki"]

    return target_map[variant]
