load("//kernel-6.1:modules.bzl", "get_gki_modules_list")

mgk_module_outs = get_gki_modules_list("arm64") + [
    "drivers/firmware/arm_ffa/ffa-module.ko",
    "drivers/gpu/drm/display/drm_display_helper.ko",
    "drivers/gpu/drm/drm_dma_helper.ko",
    "drivers/i2c/busses/i2c-gpio.ko",
    "drivers/iio/buffer/industrialio-triggered-buffer.ko",
    "drivers/iio/buffer/kfifo_buf.ko",
    "drivers/leds/leds-pwm.ko",
    "drivers/media/v4l2-core/v4l2-flash-led-class.ko",
    "drivers/perf/arm_dsu_pmu.ko",
    "drivers/phy/mediatek/phy-mtk-mipi-dsi-drv.ko",
    "drivers/power/reset/reboot-mode.ko",
    "drivers/power/reset/syscon-reboot-mode.ko",
    "drivers/tee/tee.ko",
    "drivers/thermal/thermal-generic-adc.ko",
    "net/wireless/cfg80211.ko",
    "net/mac80211/mac80211.ko",
]

mgk_module_eng_outs = [
    "fs/pstore/pstore_blk.ko",
    "fs/pstore/pstore_zone.ko",
]

mgk_module_userdebug_outs = [
    "fs/pstore/pstore_blk.ko",
    "fs/pstore/pstore_zone.ko",
]

mgk_module_user_outs = [
]
