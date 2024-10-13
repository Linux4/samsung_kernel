load("@bazel_skylib//rules:write_file.bzl", "write_file")

common_configs = [
    "CONFIG_CAMERA_SYSFS_V2=y",
    "CONFIG_SAMSUNG_OIS_MCU_STM32=y",
    "CONFIG_CAMERA_FRAME_CNT_DBG=y",
    "CONFIG_CAMERA_FRAME_CNT_CHECK=y",
    "CONFIG_SAMSUNG_FRONT_EEPROM=y",
    "CONFIG_SAMSUNG_REAR_DUAL=y",
    "CONFIG_SAMSUNG_REAR_TRIPLE=y",
    "CONFIG_SAMSUNG_CAMERA=y",
    "CONFIG_SENSOR_RETENTION=y",
    "CONFIG_CAMERA_RF_MIPI=y",
    "CONFIG_SAMSUNG_ACTUATOR_READ_HALL_VALUE=y",
    "CONFIG_SAMSUNG_DEBUG_SENSOR_I2C=y",
    "CONFIG_SAMSUNG_DEBUG_SENSOR_TIMING=y",
    "CONFIG_SAMSUNG_DEBUG_HW_INFO=y",
    "CONFIG_USE_CAMERA_HW_BIG_DATA=y",
    "CONFIG_CAMERA_CDR_TEST=y",
    "CONFIG_CAMERA_HW_ERROR_DETECT=y",
    #"CONFIG_CAMERA_ADAPTIVE_MIPI=y",
]

project_configs = select({
    # Project-specific configs
    ":no_project": [],
    ":e1q_project": common_configs + [
        "CONFIG_SEC_E1Q_PROJECT=y",
    ],
    ":e2q_project": common_configs + [
        "CONFIG_SEC_E2Q_PROJECT=y",
    ],
    ":e3q_project": common_configs + [
        "CONFIG_SEC_E3Q_PROJECT=y",
        "CONFIG_SAMSUNG_REAR_QUADRA=y",
        "CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING=y",
        "CONFIG_SAMSUNG_READ_BPC_FROM_OTP=y",
        #"CONFIG_SAMSUNG_WACOM_NOTIFIER=y",
    ],
    ":q6q_project": common_configs + [
        "CONFIG_SEC_Q6Q_PROJECT=y",
    ],
    ":b6q_project": common_configs + [
        "CONFIG_SEC_B6Q_PROJECT=y",
    ],
    ":gts10p_project": common_configs + [
        "CONFIG_SEC_GTS10P_PROJECT=y",
    ],
    ":gts10u_project": common_configs + [
        "CONFIG_SEC_GTS10U_PROJECT=y",
    ],
    ":q6aq_project": common_configs + [
        "CONFIG_SEC_Q6AQ_PROJECT=y",
    ],
})

"""
Return a label which defines a project-specific defconfig snippet to be
applied on top of the platform defconfig.
"""

def get_project_defconfig(target, variant):
    rule_name = "{}_{}_project_defconfig".format(target, variant)

    write_file(
        name = rule_name,
        out = "{}.generated".format(rule_name),
        content = project_configs + [""],
    )

    return rule_name
