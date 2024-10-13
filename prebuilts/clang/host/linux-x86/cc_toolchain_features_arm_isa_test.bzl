"""Copyright (C) 2022 The Android Open Source Project

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

load(":cc_toolchain_constants.bzl", "generated_config_constants")
load(
    "//build/bazel/rules/test_common:flags.bzl",
    "action_flags_absent_for_mnemonic_test_with_config_settings",
    "action_flags_present_only_for_mnemonic_test_with_config_settings",
)

_action_flags_present_arm_test = action_flags_present_only_for_mnemonic_test_with_config_settings(
    {
        "//command_line_option:platforms": [
            "@//build/bazel/tests/products:aosp_arm_for_testing",
        ],
    },
)
_action_flags_absent_arm_test = action_flags_absent_for_mnemonic_test_with_config_settings(
    {
        "//command_line_option:platforms": [
            "@//build/bazel/tests/products:aosp_arm_for_testing",
        ],
    },
)

_compile_action_mnemonic = "CppCompile"

def _test_arm_isa_thumb_enabled_by_default():
    name = "arm_isa_thumb_enabled_by_default"
    thumb_flags_present_test_name = name + "_thumb_flags_present_test"
    arm_flags_absent_test_name = name + "_arm_flags_absent_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        tags = ["manual"],
    )

    _action_flags_present_arm_test(
        name = thumb_flags_present_test_name,
        target_under_test = name,
        mnemonics = [_compile_action_mnemonic],
        expected_flags = generated_config_constants.ArmThumbCflags,
    )

    _action_flags_absent_arm_test(
        name = arm_flags_absent_test_name,
        target_under_test = name,
        mnemonics = [_compile_action_mnemonic],
        expected_absent_flags = ["-fstrict-aliasing"],
    )

    return [
        thumb_flags_present_test_name,
        arm_flags_absent_test_name,
    ]

def _test_arm_isa_arm_overrides_thumb():
    name = "arm_isa_arm_overrides_thumb"
    arm_flags_present_test_name = name + "_arm_flags_present_test"
    thumb_flags_absent_test_name = name + "_thumb_flags_absent_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        features = ["arm_isa_arm"],
        tags = ["manual"],
    )

    _action_flags_present_arm_test(
        name = arm_flags_present_test_name,
        target_under_test = name,
        mnemonics = [_compile_action_mnemonic],
        expected_flags = ["-fstrict-aliasing"],
    )

    _action_flags_absent_arm_test(
        name = thumb_flags_absent_test_name,
        target_under_test = name,
        mnemonics = [_compile_action_mnemonic],
        expected_absent_flags = generated_config_constants.ArmThumbCflags,
    )

    return [
        arm_flags_present_test_name,
        thumb_flags_absent_test_name,
    ]

_action_flags_absent_arm64_test = action_flags_absent_for_mnemonic_test_with_config_settings(
    {
        "//command_line_option:platforms": [
            "@//build/bazel/tests/products:aosp_arm64_for_testing",
        ],
    },
)

def _test_arm_isa_arm_flags_absent_when_arch_not_arm():
    name = "arm_isa_flags_absent_when_arch_not_arm"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        features = ["arm_isa_arm"],
        tags = ["manual"],
    )

    _action_flags_absent_arm64_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [_compile_action_mnemonic],
        expected_absent_flags = ["-fstrict-aliasing"],
    )

    return [test_name]

def _test_arm_isa_thumb_flags_absent_when_arch_not_arm():
    name = "arm_isa_thumb_flags_absent_when_arch_not_arm"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        tags = ["manual"],
    )

    _action_flags_absent_arm64_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [_compile_action_mnemonic],
        expected_absent_flags = generated_config_constants.ArmThumbCflags,
    )

    return [test_name]

def cc_toolchain_features_arm_isa_test_suite(name):
    native.test_suite(
        name = name,
        tests = (
            _test_arm_isa_thumb_enabled_by_default() +
            _test_arm_isa_arm_overrides_thumb() +
            _test_arm_isa_arm_flags_absent_when_arch_not_arm() +
            _test_arm_isa_thumb_flags_absent_when_arch_not_arm()
        ),
    )
