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

"""Common features and helper functions for configuring CC toolchain for Android kernel."""

load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "action_config",
    "feature",
    "flag_group",
    "flag_set",
    "variable_with_value",
)
load(
    "@rules_cc//cc:action_names.bzl",
    "ACTION_NAMES",
    "ALL_CC_COMPILE_ACTION_NAMES",
    "ALL_CC_LINK_ACTION_NAMES",
)

def _action_configs(ctx):
    compile = action_config(
        action_name = ACTION_NAMES.c_compile,
        tools = [
            struct(
                type_name = "tool",
                tool = ctx.file.clang,
            ),
        ],
    )
    compile_plus_plus = action_config(
        action_name = ACTION_NAMES.cpp_compile,
        tools = [
            struct(
                type_name = "tool",
                tool = ctx.file.clang_plus_plus,
            ),
        ],
    )
    link = action_config(
        action_name = ACTION_NAMES.cpp_link_executable,
        tools = [
            struct(
                type_name = "tool",
                tool = ctx.file.ld,
            ),
        ],
    )
    ar = action_config(
        action_name = ACTION_NAMES.cpp_link_static_library,
        tools = [
            struct(
                type_name = "tool",
                tool = ctx.file.ar,
            ),
        ],
        implies = [
            "archiver_flags",
        ],
    )
    strip = action_config(
        action_name = ACTION_NAMES.strip,
        tools = [
            struct(
                type_name = "tool",
                tool = ctx.file.strip,
            ),
        ],
    )
    objcopy = action_config(
        action_name = ACTION_NAMES.objcopy_embed_data,
        tools = [
            struct(
                type_name = "tool",
                tool = ctx.file.objcopy,
            ),
        ],
    )

    return [
        compile,
        compile_plus_plus,
        link,
        ar,
        strip,
        objcopy,
    ]

def _tool_attrs():
    return {
        "clang": attr.label(allow_single_file = True),
        "clang_plus_plus": attr.label(allow_single_file = True),
        "ld": attr.label(allow_single_file = True),
        "strip": attr.label(allow_single_file = True),
        "ar": attr.label(allow_single_file = True),
        "objcopy": attr.label(allow_single_file = True),
    }

def _common_cflags():
    return feature(
        name = "kleaf-no-canonical-prefixes",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = ALL_CC_COMPILE_ACTION_NAMES,
                flag_groups = [
                    flag_group(
                        flags = [
                            # Work around https://github.com/bazelbuild/bazel/issues/4605
                            # "cxx_builtin_include_directory doesn't work with non-absolute path"
                            "-no-canonical-prefixes",
                        ],
                    ),
                ],
            ),
        ],
    )

def _lld_compiler_rt():
    # From _setup_env.sh
    return feature(
        name = "kleaf-lld-compiler-rt",
        enabled = False,  # Not enabled unless implied by individual os
        flag_sets = [
            flag_set(
                actions = ALL_CC_LINK_ACTION_NAMES,
                flag_groups = [
                    flag_group(
                        flags = [
                            "-fuse-ld=lld",
                            "--rtlib=compiler-rt",
                        ],
                    ),
                ],
            ),
        ],
    )

def _common_features(_ctx):
    """Features that applies to both android and linux toolchain."""
    return [
        _common_cflags(),
        _lld_compiler_rt(),
    ]

common = struct(
    features = _common_features,
    action_configs = _action_configs,
    tool_attrs = _tool_attrs,
)
