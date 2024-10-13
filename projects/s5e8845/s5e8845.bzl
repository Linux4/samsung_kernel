# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load(
    "//build/kernel/kleaf:kernel.bzl",
    "kernel_abi",
    "kernel_build",
    "kernel_modules_install",
    "merged_kernel_uapi_headers",
    "kernel_images",
    "kernel_unstripped_modules_archive",
)
load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")
load("@kernel_toolchain_info//:dict.bzl", "CLANG_VERSION")
load("//kernel:modules.bzl", "get_gki_modules_list")
load("//kernel:lego.bzl", "lego_module_list", "lego_dtbo_list")
load("//projects/common/impl:dtbo.bzl", "dtbo")
load("//projects/common/impl:vendor_ramdisk.bzl", "create_vendor_ramdisk")
load("//projects/common/impl:extract_modules.bzl", "extract_modules")
load("//projects/common/impl:custom_boot_images.bzl", "custom_boot_images")
load(":s5e8845_model_variant_modules.bzl", "MODEL_VARIANT_MODULE_LIST")
load(
    ":s5e8845_modules.bzl",
    "VENDOR_EARLY_MODULE_LIST",
    "VENDOR_MODULE_LIST",
    "VENDOR_DLKM_MODULE_LIST",
    "VENDOR_DEV_MODULE_LIST",
    "FACTORY_BUILD_MODULE_LIST"
)

def define_s5e8845_kernel(
        name,
        outs = None,
        build_config = None,
        module_outs = None,
        kmi_symbol_list_add_only = None,
        dist_dir = None):

    native.filegroup(
        name = name + "_common_config",
        srcs = [":build.config.s5e8845.common"],
    )

    make_goals = [
        "Image",
        "modules",
        "dtbs",
    ]

    _exynos_vendor_boot_list = VENDOR_MODULE_LIST + lego_module_list + MODEL_VARIANT_MODULE_LIST
    if "debug" in name or "eng" in name:
        _exynos_vendor_boot_list = _exynos_vendor_boot_list + VENDOR_DEV_MODULE_LIST
    if "fac" in name:
        # [MX_BSP] support for factory module build
        _exynos_vendor_boot_list = _exynos_vendor_boot_list + FACTORY_BUILD_MODULE_LIST
    _all_exynos_module_list = VENDOR_EARLY_MODULE_LIST + _exynos_vendor_boot_list + VENDOR_DLKM_MODULE_LIST

    if "abi" in name:
        # For ABI
        kernel_build(
            name = name,
            base_kernel = "//kernel:kernel_aarch64",
            outs = [],
            srcs = [
                "//kernel:common_kernel_sources",
                name + "_common_config",
            ],
            build_config = ":build.config.s5e8845_user",
            make_goals = make_goals,
            # List of in-tree kernel modules.
            module_outs =  _all_exynos_module_list,
            kmi_symbol_list = "//kernel:android/abi_gki_aarch64_exynos",
            kmi_symbol_list_strict_mode = False,
            collect_unstripped_modules = True,
        )
        kernel_abi(
            name = name + "_abi",
            kernel_build = name,
            kmi_enforced = True,
            kmi_symbol_list_add_only = True,
        )

    else:
        kernel_build(
            name = name,
            outs = [
                "Image",
                "System.map",
                "vmlinux",
                "s5e8845.dtb",
                "modules.builtin.modinfo",
                "modules.builtin",
                "modules.order",
                "Module.symvers",
                ".config",
            ],
            implicit_outs = [
                "scripts/sign-file",
                "certs/signing_key.pem",
                "certs/signing_key.x509",
            ]+lego_dtbo_list,
            srcs = [
                "//kernel:common_kernel_sources",
                name + "_common_config",
            ],
            # List of in-tree kernel modules.
            module_outs =  _all_exynos_module_list,
            module_implicit_outs = get_gki_modules_list("arm64"),
            build_config = ":build.config."+ name,
            make_goals = make_goals,
            collect_unstripped_modules = True,
            strip_modules = True,
        )

    kernel_unstripped_modules_archive(
        name = name + "_module_symbols",
        kernel_build = name,
    )

    kernel_modules_install(
        name = name + "_modules_install",
        kernel_build = name,
        # List of external modules.
        kernel_modules = [],
    )

    native.filegroup(
        name = name + "_dtbo_config",
        srcs = [ name + '/' + f for f in lego_dtbo_list ],
    )

    dtbo(
        name = "{}_dtbo".format(name),
        srcs = [ name + "_dtbo_config"],
        kernel_build = name,
    )

    native.filegroup(
        name = name + "_vendor_files",
        srcs = [ "//prebuilts/platform/common:fstab.s5e8845.ab",
                 "//prebuilts/platform/erd8845:conf/init.recovery.s5e8845.rc",
                 "//prebuilts/platform/erd8845:conf/init.s5e8845.usb.rc",
                 "//prebuilts/platform/erd8845:firmware/sgpu/vangogh_lite_unified_evt0.bin",
		 "//prebuilts/boot-artifacts/arm64/exynos:system_prebuilt_files.tar.gz",
        ],
    )

    # create custom vendor ramdisk for exynos
    create_vendor_ramdisk(
        name = name + "_custom_vendor_ramdisk",
        vendor_files = [ name + "_vendor_files"],
        kernel_build = name,
    )

    # create system_dlkm_staging.tar.gz
    extract_modules(
        name = name + "_system_dlkm_staging",
        target_dir = "system",
        module_sign = True,
        kernel_build = name,
        module_list = get_gki_modules_list("arm64"),
    )

    # create vendor_boot_staging.tar.gz that is module reordered.
    extract_modules(
        name = name + "_vendor_boot_staging",
        kernel_build = name,
        module_sign = False,
        module_early_list = VENDOR_EARLY_MODULE_LIST,
        module_list = _exynos_vendor_boot_list,
    )

    extract_modules(
        name = name + "_vendor_dlkm_staging",
        target_dir = "vendor",
        kernel_build = name,
        module_list = VENDOR_DLKM_MODULE_LIST,
    )

    # generate boot.img
    kernel_images(
        name = name + "_images",
        build_initramfs = True,
        build_boot = True,
        kernel_build = name,
        kernel_modules_install = name + "_modules_install",
        deps = [
            "//prebuilts/boot-artifacts/arm64/exynos:file_contexts",
        ],
    )

    # generate vendor_boot.img, vendor_bootconfig.img
    custom_boot_images(
        name = "{}_custom_vendor_boot_images".format(name),
        kernel_build = name,
        initramfs_staging = name + "_vendor_boot_staging",
        vendor_ramdisk_binaries = [ name + "_custom_vendor_ramdisk" ],
        vendor_boot_name = "vendor_boot",
    )

    copy_to_dist_dir(
        name = name + "_dist",
        data = [
            name,
            name + "_module_symbols",
            name + "_images",
            name + "_modules_install",
            name + "_dtbo",
            name + "_system_dlkm_staging",
            name + "_vendor_dlkm_staging",
            name + "_vendor_boot_staging",
            name + "_custom_vendor_boot_images",
            "init_insmod_vendor_dlkm_list",
        ],
        dist_dir = "out/{name}/dist".format(name = name),
        flat = True,
        log = "info",
    )
