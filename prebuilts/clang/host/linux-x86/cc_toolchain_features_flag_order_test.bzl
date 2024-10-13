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

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@bazel_skylib//lib:sets.bzl", "sets")
load(":cc_toolchain_constants.bzl", "generated_config_constants")

def _test_flag_order_impl(ctx):
    """
    This test confirms that a given set of flags appears in the order they are
    supplied. It ignores any flags that are not specified.
    """

    env = analysistest.begin(ctx)
    actions = analysistest.target_actions(env)

    expected_flags_set = sets.make(ctx.attr.expected_flags_in_order)
    for action in actions:
        if action.mnemonic == ctx.attr.mnemonic:
            out_of_order = False
            expected_index = 0
            for flag in action.argv:
                # If we've found all the flags, we don't need to check any further
                if expected_index >= len(ctx.attr.expected_flags_in_order):
                    break
                if sets.contains(expected_flags_set, flag):
                    if ctx.attr.expected_flags_in_order[expected_index] != flag:
                        out_of_order = True
                        analysistest.fail(
                            env,
                            "Flag %s was out of order" % flag,
                        )
                    expected_index += 1
            if expected_index != len(ctx.attr.expected_flags_in_order):
                analysistest.fail(
                    env,
                    (
                        "Not all flags were found.\n" +
                        "Expected Length: %d\n" +
                        "Actual Length: %d"
                    ) % (
                        len(ctx.attr.expected_flags_in_order),
                        expected_index,
                    ),
                )
            if out_of_order:
                print("Expected flags: %s" % ctx.attr.expected_flags_in_order)
                print("Actual flags: %s" % action.argv)

    return analysistest.end(env)

flag_order_test = analysistest.make(
    impl = _test_flag_order_impl,
    attrs = {
        "expected_flags_in_order": attr.string_list(
            doc = "flags expected to be present in their expected order",
        ),
        "mnemonic": attr.string(
            doc = "The mnemonic for the actions that should be checked",
        ),
    },
)

# TODO(b/266723620): Bring to parity with Soong order test
def _test_compile_flag_order_arm():
    name = "compile_flag_order_arm"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        tags = ["manual"],
    )

    flag_order_test(
        name = test_name,
        target_under_test = name,
        expected_flags_in_order = (
            generated_config_constants.ArmThumbCflags +
            generated_config_constants.ArmCflags +
            generated_config_constants.CommonGlobalCflags
        ),
        mnemonic = "CppCompile",
        target_compatible_with = ["//build/bazel/platforms/arch:arm"],
    )
    return test_name

def _test_external_compile_flag_order():
    name = "external_compile_flag_order"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        tags = ["manual"],
        features = ["external_compiler_flags", "-non_external_compiler_flags"],
    )

    flag_order_test(
        name = test_name,
        target_under_test = name,
        expected_flags_in_order = (
            generated_config_constants.CommonGlobalCflags +
            generated_config_constants.DeviceGlobalCflags +
            generated_config_constants.ExternalCflags
        ),
        mnemonic = "CppCompile",
        target_compatible_with = ["//build/bazel/platforms/os:android"],
    )
    return test_name

def cc_toolchain_features_flag_order_test_suite(name):
    native.test_suite(
        name = name,
        tests = [
            _test_compile_flag_order_arm(),
            _test_external_compile_flag_order(),
        ],
    )
