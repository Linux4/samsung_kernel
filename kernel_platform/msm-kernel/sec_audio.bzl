# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

__module_platform_map =  {
    "pineapple": {
        "gki": [
            # keep sorted & in-tree modules only
            "cs_dsp.ko",
            "snd-soc-cirrus-amp.ko",
            "snd-soc-cs35l43-i2c.ko",
            "snd-soc-wm-adsp.ko",
            "snd_debug_proc.ko",
            "sec_audio_sysfs.ko",
        ],
        "consolidate": [
            # keep sorted
        ],
    },
}

def sec_audio(target, variant):
    if not target in __module_platform_map:
        return []

    target_map = __module_platform_map[target]
    if not variant in target_map:
        return []

    if variant == "consolidate":
        return target_map[variant] + target_map["gki"]

    return target_map[variant]
