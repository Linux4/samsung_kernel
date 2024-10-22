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

load("//build/kernel/kleaf:workspace.bzl", "define_kleaf_workspace")

load("//build/bazel_mgk_rules:kleaf/key_value_repo.bzl", "key_value_repo")
key_value_repo(
    name = "mgk_info",
)

load("@mgk_info//:dict.bzl","KERNEL_VERSION")
define_kleaf_workspace(common_kernel_package = "@//"+KERNEL_VERSION)

# Optional epilog for analysis testing.
load("//build/kernel/kleaf:workspace_epilog.bzl", "define_kleaf_workspace_epilog")
define_kleaf_workspace_epilog()

new_local_repository(
    name="mgk_internal",
    path="../vendor/mediatek",
    build_file = "//build/bazel_mgk_rules:kleaf/BUILD.internal"
)

new_local_repository(
    name="mgk_ko",
    path="../vendor/mediatek/kernel_modules",
    build_file = "//build/bazel_mgk_rules:kleaf/BUILD.ko"
)
