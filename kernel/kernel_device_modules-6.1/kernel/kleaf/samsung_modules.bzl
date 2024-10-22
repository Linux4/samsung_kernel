load("//kernel_device_modules-6.1:kernel/kleaf/samsung_product_modules.bzl",
     "product_device_modules",
     "product_gki_modules",
)

# modules in android/kernel/kernel_device_modules-6.x
_device_modules = [
    "block/blk-sec-common.ko",
    "block/blk-sec-stats.ko",
    "block/blk-sec-wb.ko",
    "block/ssg.ko",
    "drivers/samsung/debug/sec_debug.ko",
    "drivers/samsung/debug/sec_extra_info.ko",
    "drivers/samsung/sec_reboot.ko",
    "drivers/samsung/sec_reset.ko",
    "drivers/samsung/sec_ext.ko",
    "drivers/samsung/sec_bootstat.ko",
    "drivers/samsung/pm/sec_pm_debug.ko",
    "drivers/cpufreq/cpufreq_limit.ko",
    "drivers/usb/ss_function/usb_f_conn_gadget.ko",
    "drivers/usb/ss_function/usb_f_dm.ko",
    "drivers/usb/ss_function/usb_f_ss_mon_gadget.ko",
    "drivers/firmware/cirrus/cs_dsp.ko",
    "sound/soc/codecs/snd-soc-wm-adsp.ko",
    "sound/soc/codecs/snd-soc-cirrus-amp.ko",
    "sound/soc/codecs/snd-soc-cs35l45-i2c.ko",
    "sound/soc/samsung/sec_audio_sysfs.ko",
]

samsung_device_modules = _device_modules + product_device_modules

# gki modules in android/kernel-6.x
_gki_modules = [
    "drivers/watchdog/softdog.ko",
    "drivers/input/matrix-keymap.ko",
]

samsung_gki_modules = _gki_modules + product_gki_modules
