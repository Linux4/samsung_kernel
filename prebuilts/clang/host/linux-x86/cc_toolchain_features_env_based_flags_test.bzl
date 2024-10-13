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

CC_LIB_NAME = "env_based_flags_test_cc_lib"

def _compile_flags_verification_test_impl(ctx):
    env = analysistest.begin(ctx)
    actions = analysistest.target_actions(env)
    compile_actions = [a for a in actions if a.mnemonic == "CppCompile"]
    asserts.true(
        env,
        len(compile_actions) == 1,
        "There should be only one compile action: %s" % compile_actions,
    )
    compile_action = compile_actions[0]

    for flag in ctx.attr.expected_flags:
        asserts.true(
            env,
            flag in compile_action.argv,
            "compile action did not contain %s flag [%s]" % (flag, compile_action.argv),
        )

    return analysistest.end(env)

auto_pattern_initialize_flags_verification_test = analysistest.make(
    _compile_flags_verification_test_impl,
    attrs = {
        "expected_flags": attr.string_list(
            doc = "Flags expected to be supplied to the command line",
        ),
    },
    config_settings = {
        "@//prebuilts/clang/host/linux-x86:auto_pattern_initialize_env": True,
    },
)

auto_zero_initialize_flags_verification_test = analysistest.make(
    _compile_flags_verification_test_impl,
    attrs = {
        "expected_flags": attr.string_list(
            doc = "Flags expected to be supplied to the command line",
        ),
    },
    config_settings = {
        "@//prebuilts/clang/host/linux-x86:auto_zero_initialize_env": True,
    },
)

auto_uninitialize_flags_verification_test = analysistest.make(
    _compile_flags_verification_test_impl,
    attrs = {
        "expected_flags": attr.string_list(
            doc = "Flags expected to be supplied to the command line",
        ),
    },
    config_settings = {
        "@//prebuilts/clang/host/linux-x86:auto_uninitialize_env": True,
    },
)

auto_initialize_default_flags_verification_test = analysistest.make(
    _compile_flags_verification_test_impl,
    attrs = {
        "expected_flags": attr.string_list(
            doc = "Flags expected to be supplied to the command line",
        ),
    },
)

independent_global_flags_verification_test = analysistest.make(
    _compile_flags_verification_test_impl,
    attrs = {
        "expected_flags": attr.string_list(
            doc = "Flags expected to be supplied to the command line",
        ),
    },
    config_settings = {
        "@//prebuilts/clang/host/linux-x86:use_ccache_env": True,
        "@//prebuilts/clang/host/linux-x86:llvm_next_env": True,
        "@//prebuilts/clang/host/linux-x86:allow_unknown_warning_option_env": True,
    },
)

def test_auto_pattern_initialize_flags():
    test_name = "auto_pattern_initialize_test"

    auto_pattern_initialize_flags_verification_test(
        name = test_name,
        target_under_test = CC_LIB_NAME,
        expected_flags = [
            "-ftrivial-auto-var-init=pattern",
        ],
    )

    return test_name

def test_auto_uninitialize_flags():
    test_name = "auto_uninitialize_test"

    auto_uninitialize_flags_verification_test(
        name = test_name,
        target_under_test = CC_LIB_NAME,
        expected_flags = [
            "-ftrivial-auto-var-init=uninitialized",
        ],
    )

    return test_name

def test_auto_zero_initialize_flags():
    test_name = "auto_zero_initialize_test"

    auto_zero_initialize_flags_verification_test(
        name = test_name,
        target_under_test = CC_LIB_NAME,
        expected_flags = [
            "-ftrivial-auto-var-init=zero",
            "-Wno-unused-command-line-argument",
        ],
    )

    return test_name

def test_auto_initialize_default_flags():
    test_name = "auto_initialize_default_test"

    auto_initialize_default_flags_verification_test(
        name = test_name,
        target_under_test = CC_LIB_NAME,
        expected_flags = [
            "-ftrivial-auto-var-init=zero",
            "-Wno-unused-command-line-argument",
            "-g",
        ],
    )

    return test_name

def test_independent_global_flags():
    test_name = "independent_gloabl_flags_test"

    independent_global_flags_verification_test(
        name = test_name,
        target_under_test = CC_LIB_NAME,
        expected_flags = [
            "-Wno-unused-command-line-argument",
            "-Wno-error=single-bit-bitfield-constant-conversion",
            "-Wno-error=unknown-warning-option",
        ],
    )

    return test_name

def cc_toolchain_features_env_based_flags_test_suite(name):
    native.cc_library(
        name = CC_LIB_NAME,
        srcs = ["foo.cc"],
        tags = ["manual"],
    )

    native.test_suite(
        name = name,
        tests = [
            test_auto_pattern_initialize_flags(),
            test_auto_uninitialize_flags(),
            test_auto_zero_initialize_flags(),
            test_auto_initialize_default_flags(),
            test_independent_global_flags(),
        ],
    )
