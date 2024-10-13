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
    "action_flags_present_only_for_mnemonic_test",
)

# Include these different file types to make sure that all actions types are
# triggered
test_srcs = [
    "foo.cpp",
    "bar.c",
    "baz.s",
    "blah.S",
]

compile_action_mnemonic = "CppCompile"
link_action_mnemonic = "CppLink"

def test_thin_lto_feature_defaults():
    name = "thin_lto_feature"

    native.cc_binary(
        name = name,
        srcs = test_srcs,
        features = ["android_thin_lto"],
        tags = ["manual"],
    )

    test_name = name + "_test"
    test_names = [test_name]
    action_flags_present_only_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic, link_action_mnemonic],
        expected_flags = [
            "-flto=thin",
            "-fsplit-lto-unit",
        ],
    )
    limit_cross_tu_inline_test_name = "limit_cross_tu_inline_feature_enabled_by_default"
    test_names += [limit_cross_tu_inline_test_name]
    action_flags_present_only_for_mnemonic_test(
        name = limit_cross_tu_inline_test_name,
        target_under_test = name,
        mnemonics = [link_action_mnemonic],
        expected_flags = ["-Wl,-plugin-opt,-import-instr-limit=5"],
    )
    return test_names

def test_whole_program_vtables_feature():
    name = "whole_program_vtables_feature"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = test_srcs,
        features = [
            "android_thin_lto",
            "android_thin_lto_whole_program_vtables",
        ],
        tags = ["manual"],
    )
    action_flags_present_only_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [link_action_mnemonic],
        expected_flags = ["-fwhole-program-vtables"],
    )

    return test_name

def test_whole_program_vtables_requires_thinlto_feature():
    name = "whole_program_vtables_requires_thinlto"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = test_srcs,
        features = [
            "android_thin_lto_whole_program_vtables",
        ],
        tags = ["manual"],
    )
    action_flags_absent_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [
            compile_action_mnemonic,
            link_action_mnemonic,
        ],
        expected_absent_flags = ["-fwhole-program-vtables"],
    )

    return test_name

def test_limit_cross_tu_inline_requires_thinlto_feature():
    name = "limit_cross_tu_inline_requires_thinlto"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = test_srcs,
        features = [
            "android_thin_lto_limit_cross_tu_inline",
        ],
        tags = ["manual"],
    )
    action_flags_absent_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [
            link_action_mnemonic,
        ],
        expected_absent_flags = ["-Wl,-plugin-opt,-import-instr-limit=5"],
    )

    return test_name

def test_limit_cross_tu_inline_disabled_when_autofdo_enabled():
    name = "limit_cross_tu_inline_disabled_when_autofdo_enabled"
    test_name = name + "_test"

    native.cc_binary(
        name = name,
        srcs = test_srcs,
        features = [
            "android_thin_lto",
            "autofdo",
        ],
        tags = ["manual"],
    )
    action_flags_absent_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [
            link_action_mnemonic,
        ],
        expected_absent_flags = ["-Wl,-plugin-opt,-import-instr-limit=5"],
    )

    return test_name

def cc_toolchain_features_lto_test_suite(name):
    native.test_suite(
        name = name,
        tests = [
            test_whole_program_vtables_feature(),
            test_whole_program_vtables_requires_thinlto_feature(),
            test_limit_cross_tu_inline_requires_thinlto_feature(),
            test_limit_cross_tu_inline_disabled_when_autofdo_enabled(),
        ] + test_thin_lto_feature_defaults(),
    )
