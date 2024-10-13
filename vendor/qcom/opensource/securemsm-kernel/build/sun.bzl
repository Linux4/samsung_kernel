load(":securemsm_kernel.bzl", "define_consolidate_gki_modules")

def define_sun():
    define_consolidate_gki_modules(
        target = "sun",
        modules = [
            "smcinvoke_dlkm",
            "tz_log_dlkm",
            "qseecom_dlkm"
         ],
         extra_options = [
             "CONFIG_QCOM_SMCINVOKE",
             "CONFIG_QSEECOM_COMPAT",
         ],
     )
