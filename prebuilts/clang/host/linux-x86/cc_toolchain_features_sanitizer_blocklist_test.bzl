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

# TODO: b/286872909 - Remove this test when blocking bug is complete
load(
    "//build/bazel/rules/test_common:flags.bzl",
    "action_flags_absent_for_mnemonic_test",
    "action_flags_present_only_for_mnemonic_test",
)

def _test_sanitizer_blocklist_applied_when_ubsan_enabled():
    name = "sanitizer_blocklist_applied_when_ubsan_enabled"
    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        features = [
            "ubsan_undefined",
            "sanitizer_blocklist_libavc_blocklist_txt",
        ],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_present_only_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = ["CppCompile"],
        expected_flags = [
            "-fsanitize-ignorelist=external/libavc/libavc_blocklist.txt",
        ],
    )

    return test_name

def _test_sanitizer_blocklist_applied_when_cfi_enabled():
    name = "sanitizer_blocklist_applied_when_cfi_enabled"
    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        features = [
            "android_cfi",
            "sanitizer_blocklist_libavc_blocklist_txt",
        ],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_present_only_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = ["CppCompile"],
        expected_flags = [
            "-fsanitize-ignorelist=external/libavc/libavc_blocklist.txt",
        ],
    )

    return test_name

def _test_sanitizer_blocklist_not_applied_when_sanitizers_disabled():
    name = "sanitizer_blocklist_not_applied_when_sanitizers_disabled"
    native.cc_binary(
        name = name,
        srcs = ["foo.cpp"],
        features = [
            "sanitizer_blocklist_libavc_blocklist_txt",
        ],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_absent_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = ["CppCompile"],
        expected_absent_flags = [
            "-fsanitize-ignorelist=external/libavc/libavc_blocklist.txt",
        ],
    )

    return test_name

def cc_toolchain_features_sanitizer_blocklist_test_suite(name):
    native.test_suite(
        name = name,
        tests = [
            _test_sanitizer_blocklist_applied_when_ubsan_enabled(),
            _test_sanitizer_blocklist_applied_when_cfi_enabled(),
            _test_sanitizer_blocklist_not_applied_when_sanitizers_disabled(),
        ],
    )
