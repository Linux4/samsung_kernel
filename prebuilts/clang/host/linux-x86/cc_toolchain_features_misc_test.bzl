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
load(
    "//build/bazel/rules/test_common:flags.bzl",
    "action_flags_absent_for_mnemonic_test",
    "action_flags_present_for_mnemonic_nonexclusive_test",
)

def _test_warnings_as_errors_feature():
    name = "warnings_as_errors_feature"
    test_name = name + "_test"
    native.cc_library(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        tags = ["manual"],
    )
    action_flags_present_for_mnemonic_nonexclusive_test(
        name = test_name,
        target_under_test = name,
        mnemonics = ["CppCompile"],
        expected_flags = ["-Werror"],
    )
    return test_name

def _test_warnings_as_errors_disabled_feature():
    name = "no_warnings_as_errors_feature"
    test_name = name + "_test"
    native.cc_library(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = ["-warnings_as_errors"],
        tags = ["manual"],
    )
    action_flags_absent_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = ["CppCompile"],
        expected_absent_flags = ["-Werror"],
    )
    return test_name

def cc_toolchain_features_test_suite(name):
    native.test_suite(
        name = name,
        tests = [
            _test_warnings_as_errors_feature(),
            _test_warnings_as_errors_disabled_feature(),
        ],
    )
