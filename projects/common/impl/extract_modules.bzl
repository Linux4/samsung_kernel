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

load("//build/kernel/kleaf/impl:common_providers.bzl",
    "KernelBuildInfo",
    "KernelEnvAndOutputsInfo"
)
load("//build/kernel/kleaf/impl:image/image_utils.bzl", "image_utils")
load("//build/kernel/kleaf/impl:utils.bzl", "utils")

ExtractModulesInfo = provider(
    doc = "Provides information about extracted modules.",
    fields = {
        "staging_archive": "Archive of  staging directory",
        "staging_modules_order": "modules.order of staging directory",
    },
)

def _extract_modules_impl(ctx):
    kernel_build = ctx.attr.kernel_build
    if not kernel_build:
        fail("No `kernel_build` provided.")

    modules_staging_archive = ctx.actions.declare_file("{}/{}_archive.tar.gz".format(ctx.label.name, ctx.label.name))
    modules_staging_order = ctx.actions.declare_file("{}/{}_modules.order".format(ctx.label.name, ctx.label.name))
    modules_staging_early_list = ctx.actions.declare_file("{}/{}_module_early_list".format(ctx.label.name, ctx.label.name))
    modules_staging_list = ctx.actions.declare_file("{}/{}_module_list".format(ctx.label.name, ctx.label.name))
    modules_staging_dir = modules_staging_archive.dirname + "/staging"
    kernel_build_outs = kernel_build[KernelBuildInfo].outs.to_list()
    system_map = utils.find_file(
        name = "System.map",
        files = kernel_build_outs,
        required = True,
        what = "{}: outs of dependent kernel_build {}".format(ctx.label, kernel_build), 
    )
    modules_builtin = utils.find_file(
        name = "modules.builtin",
        files = kernel_build_outs,
        required = True,
        what = "{}: essential file for depmod from kernel_build".format(ctx.label, kernel_build), 
    )
    modules_builtin_modinfo = utils.find_file(
        name = "modules.builtin.modinfo",
        files = kernel_build_outs,
        required = True,
        what = "{}: essential file for depmod from kernel_build".format(ctx.label, kernel_build), 
    )
    orig_module_order = utils.find_file(
        name = "modules.order",
        files = kernel_build_outs,
        required = True,
        what = "{}: essential file for depmod from kernel build".format(ctx.label, kernel_build),
    )
    simple_out = ctx.attr.simple_out
    module_sign = ctx.attr.module_sign
    if ctx.attr.target_dir:
        target_dir_pattern = "\\/" + ctx.attr.target_dir
    else:
        target_dir_pattern = ""

    inputs = []
    transitive_inputs = [
        ctx.attr.kernel_build[KernelEnvAndOutputsInfo].inputs,
    ]
    tools = ctx.attr.kernel_build[KernelEnvAndOutputsInfo].tools

    inputs.append(system_map)
    inputs.append(modules_builtin)
    inputs.append(modules_builtin_modinfo)
    inputs.append(orig_module_order)

    ctx.actions.write(modules_staging_list, "\n".join(ctx.attr.module_list))
    ctx.actions.write(modules_staging_early_list, "\n".join(ctx.attr.module_early_list))
    inputs.append(modules_staging_list)
    inputs.append(modules_staging_early_list)

    command = ctx.attr.kernel_build[KernelEnvAndOutputsInfo].get_setup_script(
        data = ctx.attr.kernel_build[KernelEnvAndOutputsInfo].data,
        restore_out_dir_cmd = utils.get_check_sandbox_cmd(),
    )

    command += """
        mkdir -p {modules_staging_dir}/lib/modules/0.0/
        touch {modules_staging_order}
    """.format(
        modules_staging_dir = modules_staging_dir,
        modules_staging_order = modules_staging_order.path,
    )

    GENERATE_MODULE_LIST = """
        for m in $(cat {module_early_list}); do
          echo $m >> {modules_staging_dir}/lib/modules/0.0/modules.temp
        done

        # choose modules from modules.order of kernel build
        for m in {module_lists}; do
          grep -w -f $m {orig_module_order} >> {modules_staging_dir}/lib/modules/0.0/modules.temp
        done
    """.format(
        modules_staging_dir = modules_staging_dir,
        modules_staging_order = modules_staging_order.path,
        orig_module_order = orig_module_order.path,
        module_lists = modules_staging_list.path,
        module_early_list = modules_staging_early_list.path,
    )

    COPY_MODULES = """
        # extract some *.ko modules from kernel build
        for m in $(cat {modules_staging_dir}/lib/modules/0.0/modules.temp); do
          if [ ! -z $(find ${{OUT_DIR}} -name $(basename $m)) ];then
            mkdir -p {modules_staging_dir}/lib/modules/0.0/lib/modules/
            cp -fL ${{OUT_DIR}}/$m {modules_staging_dir}/lib/modules/0.0/lib/modules/
          fi
        done
    """.format(
        modules_staging_dir = modules_staging_dir,
    )

    RUN_DEPMOD = """
        # generate modules.load
        for m in $(cat {modules_staging_dir}/lib/modules/0.0/modules.temp); do
          echo $(basename $m) >> {modules_staging_dir}/lib/modules/0.0/modules.order
        done
        cp -fL {modules_staging_dir}/lib/modules/0.0/modules.order {modules_staging_dir}/lib/modules/0.0/modules.load

        # run depmod on staging directory
        cp {modules_builtin} {modules_staging_dir}/lib/modules/0.0/
        cp {modules_builtin_modinfo} {modules_staging_dir}/lib/modules/0.0/
        depmod --errsyms --filesyms={system_map} --basedir={modules_staging_dir} 0.0

        # replace "lib/" to "/lib/"
        sed -i "s/lib\\//{target_dir_pattern}\\/lib\\//g" {modules_staging_dir}/lib/modules/0.0/modules.dep
        cp {modules_staging_dir}/lib/modules/0.0/modules.order {modules_staging_order}
        rm {modules_staging_dir}/lib/modules/0.0/modules.temp
        mv -f {modules_staging_dir}/lib/modules/0.0/modules* {modules_staging_dir}/lib/modules/0.0/lib/modules/
    """.format(
        modules_staging_dir = modules_staging_dir,
        orig_module_order = orig_module_order.path,
        modules_staging_order = modules_staging_order.path,
        system_map = system_map.path,
        modules_builtin = modules_builtin.path,
        modules_builtin_modinfo = modules_builtin_modinfo.path,
        module_lists = modules_staging_list.path,
        module_early_list = modules_staging_early_list.path,
        target_dir_pattern = target_dir_pattern,
    )

    if ctx.attr.module_list or ctx.attr.module_early_list:
        command += GENERATE_MODULE_LIST
        command += COPY_MODULES
        command += RUN_DEPMOD

    if module_sign:
        command += """
            for m in $(find {modules_staging_dir} -type f -name '*.ko'); do
              ${{OUT_DIR}}/scripts/sign-file sha1 \
              ${{OUT_DIR}}/certs/signing_key.pem \
              ${{OUT_DIR}}/certs/signing_key.x509 $m
            done
        """.format(
            modules_staging_dir = modules_staging_dir,
        )

    command += """
            # compress staging modules
            tar czf {modules_staging_archive} -C {modules_staging_dir}/lib/modules/0.0 .
    """.format(
        modules_staging_dir = modules_staging_dir,
        modules_staging_archive = modules_staging_archive.path,
    )

    ctx.actions.run_shell(
        mnemonic = "ExtractSelectedModules",
        inputs = depset(inputs, transitive = transitive_inputs),
        outputs = [
            modules_staging_archive,
            modules_staging_order,
        ],
        tools = tools,
        progress_message = "Extract selected modules",
        command = command,
    )
    return [
        ExtractModulesInfo(
            staging_archive = modules_staging_archive,
            staging_modules_order = modules_staging_order,
        ),
        DefaultInfo(files = depset([
            modules_staging_archive,
            modules_staging_order,
        ])),
    ]

extract_modules = rule(
    implementation = _extract_modules_impl,
    doc = "Extract modules for staging",
    attrs = {
        "kernel_build": attr.label(
            mandatory = True,
            providers = [KernelBuildInfo, KernelEnvAndOutputsInfo],
            doc = "Label referring to the `kenel_build` module.",
        ),
        "simple_out": attr.bool(),
        "module_sign": attr.bool(),
        "module_early_list": attr.string_list(
            allow_empty = True,
            doc = "selected *.ko has order and loaded earlier than module_list",
        ),
        "module_list": attr.string_list(
            mandatory = True,
            allow_empty = True,
            doc = "*.ko has no order and loaded in modules.order by kernel build ",
        ),
        "target_dir": attr.string(
            values = ["system", "vendor"],
            doc = """ Target directory where lib/ is located.""",
        ),
    },
)
