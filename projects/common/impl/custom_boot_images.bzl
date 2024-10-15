# Copyright (C) 2022 The Android Open Source Project
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
"""
Rules for building custom boot images(vendor_boot).
"""
load("@bazel_skylib//lib:paths.bzl", "paths")
load("//build/kernel/kleaf/impl:common_providers.bzl", "KernelBuildInfo", "KernelEnvAndOutputsInfo")
load("//build/kernel/kleaf/impl:debug.bzl", "debug")
load(":extract_modules.bzl", "ExtractModulesInfo")
load("//build/kernel/kleaf/impl:utils.bzl", "utils")

def _custom_boot_images_impl(ctx):
    vendor_boot_img = ctx.actions.declare_file("{}/vendor_boot.img".format(ctx.label.name))
    vendor_bootconfig_img = ctx.actions.declare_file("{}/vendor-bootconfig.img".format(ctx.label.name))
    ## Declare implicit outputs of the command
    ## This is like ctx.actions.declare_directory(ctx.label.name) without actually declaring it.
    outdir_short = paths.join(
        paths.dirname(ctx.build_file_path),
        ctx.label.name,
    )
    outdir = paths.join(
        ctx.bin_dir.path,
        outdir_short,
    )
    modules_staging_dir = outdir + "/staging"
    mkbootimg_staging_dir = modules_staging_dir + "/mkbootimg_staging"

    initramfs_staging_archive = ctx.attr.initramfs_staging[ExtractModulesInfo].staging_archive
    initramfs_staging_dir = modules_staging_dir + "/initramfs_staging"

    kernel_build_outs = depset(transitive = [
        ctx.attr.kernel_build[KernelBuildInfo].outs,
        ctx.attr.kernel_build[KernelBuildInfo].base_kernel_files,
    ])

    output = [
        "vendor_boot.img",
        "vendor-bootconfig.img",
    ]

    inputs = [
        ctx.file.mkbootimg,
    ]
    inputs += [
        initramfs_staging_archive,
    ]
    inputs += ctx.files.vendor_ramdisk_binaries

    transitive_inputs = [
        kernel_build_outs,
        ctx.attr.kernel_build[KernelEnvAndOutputsInfo].inputs,
    ]

    tools = [ctx.executable._search_and_cp_output]
    transitive_tools = [ctx.attr.kernel_build[KernelEnvAndOutputsInfo].tools]

    command = ctx.attr.kernel_build[KernelEnvAndOutputsInfo].get_setup_script(
        data = ctx.attr.kernel_build[KernelEnvAndOutputsInfo].data,
        restore_out_dir_cmd = utils.get_check_sandbox_cmd(),
    )

    # This is not for boot.img
    boot_flag_cmd = "BUILD_BOOT_IMG="

    if ctx.attr.vendor_boot_name == "vendor_boot":
        vendor_boot_flag_cmd = """
            BUILD_VENDOR_BOOT_IMG=1
            SKIP_VENDOR_BOOT=
            BUILD_VENDOR_KERNEL_BOOT=
        """
    else:
        fail("{}: unknown vendor_boot_name {}".format(ctx.label, ctx.attr.vendor_boot_name))

    if ctx.files.vendor_ramdisk_binaries:
        # build_utils.sh uses singular VENDOR_RAMDISK_BINARY
        command += """
            VENDOR_RAMDISK_BINARY="{vendor_ramdisk_binaries}"
        """.format(
            vendor_ramdisk_binaries = " ".join([file.path for file in ctx.files.vendor_ramdisk_binaries]),
        )

    command += """
             # Create and restore initramfs_staging_dir
               mkdir -p {initramfs_staging_dir}
               tar xf {initramfs_staging_archive} -C {initramfs_staging_dir}
    """.format(
        initramfs_staging_dir = initramfs_staging_dir,
        initramfs_staging_archive = initramfs_staging_archive.path,
    )

    command += """
             # Create and restore DIST_DIR.
             # We don't need all of *_for_dist. Copying all declared outputs of kernel_build is
             # sufficient.
               mkdir -p ${{DIST_DIR}}
               cp {kernel_build_outs} ${{DIST_DIR}}
    """.format(
        kernel_build_outs = " ".join([out.path for out in kernel_build_outs.to_list()]),
    )

    set_initramfs_var_cmd = """
            BUILD_INITRAMFS=1
            INITRAMFS_STAGING_DIR={initramfs_staging_dir}
    """.format(
        initramfs_staging_dir = initramfs_staging_dir,
    )

    if ctx.attr.avb_sign_boot_img:
        if not ctx.attr.avb_boot_partition_size or \
           not ctx.attr.avb_boot_key or not ctx.attr.avb_boot_algorithm or \
           not ctx.attr.avb_boot_partition_name:
            fail("avb_sign_boot_img is true, but one of [avb_boot_partition_size, avb_boot_key," +
                 " avb_boot_algorithm, avb_boot_partition_name] is not specified.")

        boot_flag_cmd += """
            AVB_SIGN_BOOT_IMG=1
            AVB_BOOT_PARTITION_SIZE={avb_boot_partition_size}
            AVB_BOOT_KEY={avb_boot_key}
            AVB_BOOT_ALGORITHM={avb_boot_algorithm}
            AVB_BOOT_PARTITION_NAME={avb_boot_partition_name}
        """.format(
            avb_boot_partition_size = ctx.attr.avb_boot_partition_size,
            avb_boot_key = ctx.file.avb_boot_key.path,
            avb_boot_algorithm = ctx.attr.avb_boot_algorithm,
            avb_boot_partition_name = ctx.attr.avb_boot_partition_name,
        )

    command += """
             # Build boot images
               (
                 {boot_flag_cmd}
                 {vendor_boot_flag_cmd}
                 {set_initramfs_var_cmd}
                 MKBOOTIMG_STAGING_DIR=$(readlink -m {mkbootimg_staging_dir})
                 build_boot_images
               )
               {search_and_cp_output} --srcdir ${{DIST_DIR}} --dstdir {outdir} {outs}
             # Remove staging directories
               rm -rf {modules_staging_dir}
    """.format(
        mkbootimg_staging_dir = mkbootimg_staging_dir,
        search_and_cp_output = ctx.executable._search_and_cp_output.path,
        outdir = outdir,
        outs = " ".join([f for f in output]),
        modules_staging_dir = modules_staging_dir,
        boot_flag_cmd = boot_flag_cmd,
        vendor_boot_flag_cmd = vendor_boot_flag_cmd,
        set_initramfs_var_cmd = set_initramfs_var_cmd,
    )

    debug.print_scripts(ctx, command)
    ctx.actions.run_shell(
        mnemonic = "BootImages",
        inputs = depset(inputs, transitive = transitive_inputs),
        outputs = [
            vendor_boot_img,
            vendor_bootconfig_img
        ],
        tools = depset(tools, transitive = transitive_tools),
        progress_message = "Building custom boot images {}".format(ctx.label),
        command = command,
    )
    return DefaultInfo(files = depset([
        vendor_boot_img,
        vendor_bootconfig_img
    ]))

custom_boot_images = rule(
    implementation = _custom_boot_images_impl,
    doc = """Build custom boot images, including `vendor_boot.img`, etc.

custom boot images are generated from prebuilt initramfs staging archives.
Execute `build_boot_images` in `build_utils.sh`.""",
    attrs = {
        "kernel_build": attr.label(
            mandatory = True,
            providers = [KernelEnvAndOutputsInfo, KernelBuildInfo],
        ),
        "initramfs_staging": attr.label(
            mandatory = True,
            providers = [ExtractModulesInfo],
        ),
        "mkbootimg": attr.label(
            allow_single_file = True,
            default = "//tools/mkbootimg:mkbootimg.py",
        ),
        "vendor_boot_name": attr.string(doc = """
* If `"vendor_boot"`, build `vendor_boot.img`
* If `None`, skip `vendor_boot`.
""", values = ["vendor_boot"]),
        "vendor_ramdisk_binaries": attr.label_list(allow_files = True),
        "avb_sign_boot_img": attr.bool(
            doc = """ If set to `True` signs the boot image using the avb_boot_key.
            The kernel prebuilt tool `avbtool` is used for signing.""",
        ),
        "avb_boot_partition_size": attr.int(doc = """Size of the boot partition
            in bytes. Used when `avb_sign_boot_img` is True."""),
        "avb_boot_key": attr.label(
            doc = """ Key used for signing.
            Used when `avb_sign_boot_img` is True.""",
            allow_single_file = True,
        ),
        # Note: The actual values comes from:
        # https://cs.android.com/android/platform/superproject/+/master:external/avb/avbtool.py
        "avb_boot_algorithm": attr.string(
            doc = """ `avb_boot_key` algorithm
            used e.g. SHA256_RSA2048. Used when `avb_sign_boot_img` is True.""",
            values = [
                "NONE",
                "SHA256_RSA2048",
                "SHA256_RSA4096",
                "SHA256_RSA8192",
                "SHA512_RSA2048",
                "SHA512_RSA4096",
                "SHA512_RSA8192",
            ],
        ),
        "avb_boot_partition_name": attr.string(doc = """Name of the boot partition.
            Used when `avb_sign_boot_img` is True."""),
        "_debug_print_scripts": attr.label(
            default = "//build/kernel/kleaf:debug_print_scripts",
        ),
        "_search_and_cp_output": attr.label(
            default = Label("//build/kernel/kleaf:search_and_cp_output"),
            cfg = "exec",
            executable = True,
        ),
    },
)
