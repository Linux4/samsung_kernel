load(":touch_modules.bzl", "touch_driver_modules")
load(":touch_modules_build.bzl", "define_target_variant_modules")
load("//msm-kernel:target_variants.bzl", "get_all_la_variants", "get_all_le_variants", "get_all_lxc_variants")

def define_pineapple(t,v):
    define_target_variant_modules(
        target = t,
        variant = v,
        registry = touch_driver_modules,
        modules = [
            "nt36xxx-i2c",
            "atmel_mxt_ts",
            "dummy_ts",
            "goodix_ts"
        ],
        config_options = [
            "TOUCH_DLKM_ENABLE",
            "CONFIG_ARCH_PINEAPPLE",
            "CONFIG_MSM_TOUCH",
            "CONFIG_TOUCHSCREEN_GOODIX_BRL",
            "CONFIG_TOUCHSCREEN_NT36XXX_I2C",
            "CONFIG_TOUCHSCREEN_ATMEL_MXT",
            "CONFIG_TOUCHSCREEN_DUMMY"
        ],
)

def define_blair(t,v):
    define_target_variant_modules(
        target = t,
        variant = v,
        registry = touch_driver_modules,
        modules = [
            "nt36xxx-i2c",
            "goodix_ts",
            "focaltech_fts",
            "synaptics_tcm_ts"
        ],
        config_options = [
            "TOUCH_DLKM_ENABLE",
            "CONFIG_ARCH_BLAIR",
            "CONFIG_MSM_TOUCH",
            "CONFIG_TOUCHSCREEN_NT36XXX_I2C",
            "CONFIG_TOUCHSCREEN_GOODIX_BRL",
            "CONFIG_TOUCH_FOCALTECH",
            "CONFIG_TOUCHSCREEN_SYNAPTICS_TCM"
        ],
)

def define_touch_target():
    for (t, v) in get_all_la_variants() + get_all_le_variants() + get_all_lxc_variants():
        if t == "blair":
            define_blair(t, v)
        else:
            define_pineapple(t, v)
