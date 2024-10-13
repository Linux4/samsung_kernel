# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

__dtb_platform_map = {
    "pineapple": {
        "dtb_list": [
            # keep sorted
        ],
        "dtbo_list": [
            # keep sorted
        ],
    },
}

def sec_dtb(target, binary):
    if not target in __dtb_platform_map:
        return []

    target_map = __dtb_platform_map[target]
    if not binary in target_map:
        return []

    return target_map[binary]
