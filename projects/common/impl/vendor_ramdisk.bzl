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

load("//build/kernel/kleaf/impl:common_providers.bzl", "KernelBuildInfo", "KernelEnvAndOutputsInfo")
load("//build/kernel/kleaf/impl:utils.bzl", "utils")

def _create_vendor_ramdisk_impl(ctx):
    vendor_ramdisk_archive = ctx.actions.declare_file("{}/vendor_ramdisk.cpio".format(ctx.label.name))
    modules_staging_dir = vendor_ramdisk_archive.dirname + "/staging"
    vendor_ramdisk_staging_dir = modules_staging_dir + "/vendor_ramdisk_staging"

    inputs = []
    transitive_inputs = [
        ctx.attr.kernel_build[KernelEnvAndOutputsInfo].inputs,
    ]
    tools = ctx.attr.kernel_build[KernelEnvAndOutputsInfo].tools

    inputs += ctx.files.vendor_files

    command = ctx.attr.kernel_build[KernelEnvAndOutputsInfo].get_setup_script(
        data = ctx.attr.kernel_build[KernelEnvAndOutputsInfo].data,
        restore_out_dir_cmd = utils.get_check_sandbox_cmd(),
    )

    command += """
        mkdir -p {vendor_ramdisk_staging_dir}
        mkdir -p {vendor_ramdisk_staging_dir}/first_stage_ramdisk
        mkdir -p {vendor_ramdisk_staging_dir}/etc/init
    """.format(
        vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
    )

    # copy fstab
    vendor_fstab = " ".join([f.path for f in ctx.files.vendor_files if "fstab" in f.path])
    if vendor_fstab:
        command += """
            cp -vf {vendor_fstab} {vendor_ramdisk_staging_dir}/first_stage_ramdisk/fstab.${{TARGET_SOC}}
            cp -vf {vendor_fstab} {vendor_ramdisk_staging_dir}/fstab.${{TARGET_SOC}}
            cp -vf {vendor_fstab} {vendor_ramdisk_staging_dir}/etc/recovery.fstab
        """.format(
            vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
            vendor_fstab = vendor_fstab,
        )

    # copy init.recovery.rc
    vendor_init_recovery_rc = " ".join([f.path for f in ctx.files.vendor_files if "recovery" in f.path])
    if vendor_init_recovery_rc:
        command += """
            cp -vf {vendor_init_recovery_rc} {vendor_ramdisk_staging_dir}/init.recovery.${{TARGET_SOC}}.rc
        """.format(
            vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
            vendor_init_recovery_rc = vendor_init_recovery_rc,
        )

    # copy init.usb.rc
    vendor_init_usb_rc = " ".join([f.path for f in ctx.files.vendor_files if "usb" in f.path])
    if vendor_init_usb_rc:
        command += """
                cp -vf {vendor_init_usb_rc} {vendor_ramdisk_staging_dir}/etc/init/init.${{TARGET_SOC}}.usb.rc
        """.format(
            vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
            vendor_init_usb_rc = vendor_init_usb_rc,
        )

    # copy firmware files
    vendor_firmware_files = " ".join([f.path for f in ctx.files.vendor_files if "firmware" in f.path])
    if vendor_firmware_files:
        command += """
                mkdir -p {vendor_ramdisk_staging_dir}/lib/firmware/sgpu
                cp -f {vendor_firmware_files} {vendor_ramdisk_staging_dir}/lib/firmware/sgpu/
    """.format(
        vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
        vendor_firmware_files = vendor_firmware_files,
    )

    # copy prebuilt package
    vendor_prebuilt_files = " ".join([f.path for f in ctx.files.vendor_files if "prebuilt_files" in f.path])
    if vendor_prebuilt_files:
        command += """
            mkdir -p {vendor_ramdisk_staging_dir}/system/
            tar xzf {vendor_prebuilt_files} -C {vendor_ramdisk_staging_dir}/system
        """.format(
            vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
            vendor_prebuilt_files = vendor_prebuilt_files,
        )

    command += """
        mkbootfs "{vendor_ramdisk_staging_dir}" >{output}
    """.format(
        vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
        output = vendor_ramdisk_archive.path,
    )

    ctx.actions.run_shell(
        mnemonic = "VendorRamdisk",
        progress_message = "Create vendor ramdisk.cpio",
        inputs = depset(inputs, transitive = transitive_inputs),
        tools = tools,
        outputs = [vendor_ramdisk_archive],
        command = command,
    )
    return DefaultInfo(files = depset([vendor_ramdisk_archive]))

create_vendor_ramdisk = rule(
    implementation = _create_vendor_ramdisk_impl,
    doc = """Create Vendor ramdisk binary.
    """,
    attrs = {
        "kernel_build": attr.label(
            mandatory = True,
            providers = [KernelEnvAndOutputsInfo, KernelBuildInfo],
        ),
        "vendor_files": attr.label_list(
            allow_files = True,
        ),
    },
) 
