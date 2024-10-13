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

"""Registers all clang toolchains defined in this package."""

load(":architecture_constants.bzl", "SUPPORTED_ARCHITECTURES")
load(":user_clang_toolchain_repository.bzl", "user_clang_toolchain_repository")
load(":versions.bzl", "VERSIONS")

# buildifier: disable=unnamed-macro
def register_clang_toolchains():
    """Registers all clang toolchains defined in this package.

    The user clang toolchain is expected from the path defined in the
    `KLEAF_USER_CLANG_TOOLCHAIN_PATH` environment variable, if set.
    """

    user_clang_toolchain_repository(
        name = "kleaf_user_clang_toolchain",
    )

    for target_os, target_cpu in SUPPORTED_ARCHITECTURES:
        native.register_toolchains(
            "@kleaf_user_clang_toolchain//:user_{}_{}_clang_toolchain".format(
                target_os,
                target_cpu,
            ),
        )

    for version in VERSIONS:
        for target_os, target_cpu in SUPPORTED_ARCHITECTURES:
            native.register_toolchains(
                "//prebuilts/clang/host/linux-x86/kleaf:{}_{}_{}_clang_toolchain".format(version, target_os, target_cpu),
            )

    for target_os, target_cpu in SUPPORTED_ARCHITECTURES:
        native.register_toolchains(
            "//prebuilts/clang/host/linux-x86/kleaf:{}_{}_clang_toolchain".format(target_os, target_cpu),
        )
