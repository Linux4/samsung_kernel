"""Copyright (C) 2023 The Android Open Source Project

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

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")

def _test_linker_alignment_flag_impl(ctx):
    """
    This test checks that the linker alignment flag is present for
    arm and arm 64 targets but it is not present for x86 and x86_64 targets.
    """

    env = analysistest.begin(ctx)
    actions = analysistest.target_actions(env)

    for action in actions:
        if action.mnemonic in ctx.attr.expected_action_mnemonics:
            for flag in ctx.attr.expected_flags:
                asserts.true(
                    env,
                    flag in action.argv,
                    "%s action did not contain %s flag" % (
                        action.mnemonic,
                        flag,
                    ),
                )
            for flag in ctx.attr.no_expected_flags:
                asserts.false(
                    env,
                    flag in action.argv,
                    "%s action contained unexpected flag %s" % (
                        action.mnemonic,
                        flag,
                    ),
                )
        else:
            for flag in ctx.attr.expected_flags:
                if action.argv != None:
                    asserts.false(
                        env,
                        flag in action.argv,
                        "%s action contained unexpected flag %s" % (
                            action.mnemonic,
                            flag,
                        ),
                    )

    return analysistest.end(env)

test_attrs = {
    "expected_flags": attr.string_list(
        doc = "Flags expected to be supplied to the command line",
    ),
    "expected_action_mnemonics": attr.string_list(
        doc = "Mnemonics for the actions that should have expected_flags",
    ),
    "no_expected_flags": attr.string_list(
        doc = "Flags not expected to be supplied to the command line",
    ),
}

linker_alignment_flag_arm_test = analysistest.make(
    impl = _test_linker_alignment_flag_impl,
    attrs = test_attrs,
    config_settings = {
        "//command_line_option:platforms": "@//build/bazel/tests/products:aosp_arm_for_testing",
    },
)

custom_linker_alignment_flag_arm_test = analysistest.make(
    impl = _test_linker_alignment_flag_impl,
    attrs = test_attrs,
    config_settings = {
        "//command_line_option:platforms": "@//build/bazel/tests/products:aosp_arm_for_testing_custom_linker_alignment",
    },
)

linker_alignment_flag_arm64_test = analysistest.make(
    impl = _test_linker_alignment_flag_impl,
    attrs = test_attrs,
    config_settings = {
        "//command_line_option:platforms": "@//build/bazel/tests/products:aosp_arm64_for_testing",
    },
)

custom_linker_alignment_flag_arm64_test = analysistest.make(
    impl = _test_linker_alignment_flag_impl,
    attrs = test_attrs,
    config_settings = {
        "//command_line_option:platforms": "@//build/bazel/tests/products:aosp_arm64_for_testing_custom_linker_alignment",
    },
)

linker_alignment_flag_x86_test = analysistest.make(
    impl = _test_linker_alignment_flag_impl,
    attrs = test_attrs,
    config_settings = {
        "//command_line_option:platforms": "@//build/bazel/tests/products:aosp_x86_for_testing",
    },
)

linker_alignment_flag_x86_64_test = analysistest.make(
    impl = _test_linker_alignment_flag_impl,
    attrs = test_attrs,
    config_settings = {
        "//command_line_option:platforms": "@//build/bazel/tests/products:aosp_x86_64_for_testing",
    },
)

def test_linker_alignment_flag_arm():
    name = "linker_alignment_flag_arm"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        tags = ["manual"],
    )

    linker_alignment_flag_arm_test(
        name = test_name,
        target_under_test = name,
        expected_action_mnemonics = ["CppLink"],
        expected_flags = [
            "-Wl,-z,max-page-size=4096",
        ],
        no_expected_flags = [],
    )
    return test_name

def test_custom_linker_alignment_flag_arm():
    name = "custom_linker_alignment_flag_arm"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        tags = ["manual"],
    )

    custom_linker_alignment_flag_arm_test(
        name = test_name,
        target_under_test = name,
        expected_action_mnemonics = ["CppLink"],
        expected_flags = [
            "-Wl,-z,max-page-size=65536",
        ],
        no_expected_flags = [],
    )
    return test_name

def test_linker_alignment_flag_arm64():
    name = "linker_alignment_flag_arm64"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        tags = ["manual"],
    )

    linker_alignment_flag_arm64_test(
        name = test_name,
        target_under_test = name,
        expected_action_mnemonics = ["CppLink"],
        expected_flags = [
            "-Wl,-z,max-page-size=4096",
        ],
        no_expected_flags = [],
    )
    return test_name

def test_custom_linker_alignment_flag_arm64():
    name = "custom_linker_alignment_flag_arm64"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        tags = ["manual"],
    )

    custom_linker_alignment_flag_arm64_test(
        name = test_name,
        target_under_test = name,
        expected_action_mnemonics = ["CppLink"],
        expected_flags = [
            "-Wl,-z,max-page-size=16384",
        ],
        no_expected_flags = [],
    )
    return test_name

def test_linker_alignment_flag_x86():
    name = "linker_alignment_flag_x86"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        tags = ["manual"],
    )

    linker_alignment_flag_x86_test(
        name = test_name,
        target_under_test = name,
        expected_action_mnemonics = ["CppCompile", "CppLink"],
        expected_flags = [],
        no_expected_flags = [
            "-Wl,-z,max-page-size=4096",
            "-Wl,-z,max-page-size=16384",
            "-Wl,-z,max-page-size=65536",
        ],
    )
    return test_name

def test_linker_alignment_flag_x86_64():
    name = "linker_alignment_flag_x86_64"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        tags = ["manual"],
    )

    linker_alignment_flag_x86_64_test(
        name = test_name,
        target_under_test = name,
        expected_action_mnemonics = ["CppCompile", "CppLink"],
        expected_flags = [],
        no_expected_flags = [
            "-Wl,-z,max-page-size=4096",
            "-Wl,-z,max-page-size=16384",
            "-Wl,-z,max-page-size=65536",
        ],
    )
    return test_name

def cc_toolchain_features_linker_alignment_test_suite(name):
    native.test_suite(
        name = name,
        tests = [
            test_linker_alignment_flag_arm(),
            test_linker_alignment_flag_arm64(),
            test_custom_linker_alignment_flag_arm(),
            test_custom_linker_alignment_flag_arm64(),
            test_linker_alignment_flag_x86(),
            test_linker_alignment_flag_x86_64(),
        ],
    )
