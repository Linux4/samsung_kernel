# Copyright (C) 2023 The Android Open Source Project
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

"""Creates a zip archive from the specified targets."""

def _zip_archive_impl(ctx):
    out_filename = ctx.attr.out if ctx.attr.out else ctx.attr.name + ".zip"
    out = ctx.actions.declare_file(out_filename)
    srcs_depset = depset(transitive = [target.files for target in ctx.attr.srcs])

    args = ctx.actions.args()
    args.add("-symlinks=false")
    args.add("-o", out)
    args.add_all(srcs_depset, before_each = "-f")

    ctx.actions.run(
        inputs = srcs_depset,
        outputs = [out],
        executable = ctx.executable._soong_zip,
        arguments = [args],
        mnemonic = "Zip",
        progress_message = "Zipping %s" % (out_filename),
    )
    return [DefaultInfo(files = depset([out]))]

zip_archive = rule(
    implementation = _zip_archive_impl,
    doc = "A rule to create a zip archive from specified targets",
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
            doc = "the targets with outputs to put in the generated archive",
        ),
        "out": attr.string(doc = "the output file name. Will use basename+\".zip\" if not specified"),
        "_soong_zip": attr.label(
            default = "//prebuilts/build-tools:soong_zip",
            executable = True,
            cfg = "exec",
        ),
    },
)
