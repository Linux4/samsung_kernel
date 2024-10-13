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
    ":cc_toolchain_constants.bzl",
    "generated_config_constants",
    "generated_sanitizer_constants",
)
load(
    "//build/bazel/rules/test_common:flags.bzl",
    "action_flags_absent_for_mnemonic_aosp_arm64_host_test",
    "action_flags_absent_for_mnemonic_aosp_arm64_test",
    "action_flags_absent_for_mnemonic_test",
    "action_flags_present_for_mnemonic_nonexclusive_test",
    "action_flags_present_only_for_mnemonic_aosp_arm64_host_test",
    "action_flags_present_only_for_mnemonic_aosp_arm64_test",
    "action_flags_present_only_for_mnemonic_test",
)

compile_action_mnemonic = "CppCompile"
link_action_mnemonic = "CppLink"

# Include these different file types to make sure that all actions types are
# triggered
test_srcs = [
    "foo.cpp",
    "bar.c",
    "baz.s",
    "blah.S",
]

def test_cfi_c_and_cpp_has_correct_flags():
    name = "cfi_c_and_cpp_has_correct_flags"
    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = ["android_cfi"],
        tags = ["manual"],
    )

    compile_test_name = name + "_compile_test"
    action_flags_present_for_mnemonic_nonexclusive_test(
        name = compile_test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_flags = generated_sanitizer_constants.CfiCFlags +
                         [generated_sanitizer_constants.CfiCrossDsoFlag],
    )

    link_test_name = name + "_link_test"
    action_flags_present_for_mnemonic_nonexclusive_test(
        name = link_test_name,
        target_under_test = name,
        mnemonics = [link_action_mnemonic],
        expected_flags = generated_sanitizer_constants.CfiLdFlags +
                         [generated_sanitizer_constants.CfiCrossDsoFlag],
    )

    return [
        compile_test_name,
        link_test_name,
    ]

def test_cfi_s_has_correct_flags():
    name = "cfi_s_has_correct_flags"
    native.cc_binary(
        name = name,
        srcs = ["baz.s", "blah.S"],
        features = ["android_cfi"],
        tags = ["manual"],
    )

    assemble_test_name = name + "_assemble_test"
    action_flags_present_only_for_mnemonic_test(
        name = assemble_test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_flags = generated_sanitizer_constants.CfiAsFlags,
    )

    compile_flags_absent_test_name = name + "_compile_flags_absent_test"
    action_flags_absent_for_mnemonic_test(
        name = compile_flags_absent_test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_absent_flags = generated_sanitizer_constants.CfiCFlags +
                                [generated_sanitizer_constants.CfiCrossDsoFlag],
    )

    return [
        assemble_test_name,
        compile_flags_absent_test_name,
    ]

def test_cross_dso_not_added_when_cfi_disabled():
    name = "cross_dso_not_added_when_cfi_disabled"
    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = ["android_cfi_cross_dso"],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_absent_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic, link_action_mnemonic],
        expected_absent_flags = [generated_sanitizer_constants.CfiCrossDsoFlag],
    )

    return test_name

def test_cross_dso_not_added_when_static_binary():
    name = "cross_dso_not_added_when_static_binary"
    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = [
            "android_cfi",
            "static_executable",
            "-dynamic_executable",
            "android_cfi_cross_dso",
        ],
        tags = ["manual"],
    )

    android_test_name = name + "_android_test"
    action_flags_absent_for_mnemonic_test(
        name = android_test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic, link_action_mnemonic],
        expected_absent_flags = [generated_sanitizer_constants.CfiCrossDsoFlag],
        target_compatible_with = ["//build/bazel/platforms/os:android"],
    )

    linux_test_name = name + "_linux_test"
    action_flags_absent_for_mnemonic_test(
        name = linux_test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic, link_action_mnemonic],
        expected_absent_flags = [generated_sanitizer_constants.CfiCrossDsoFlag],
        target_compatible_with = ["//build/bazel/platforms/os:linux_glibc"],
    )

    return [
        android_test_name,
        linux_test_name,
    ]

def test_cfi_assembly_support_has_correct_flags():
    name = "cfi_assembly_support_has_correct_flags"
    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = [
            "android_cfi",
            "android_cfi_assembly_support",
        ],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_present_only_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_flags = [generated_sanitizer_constants.CfiAssemblySupportFlag],
    )

    return test_name

def test_cfi_assembly_support_does_not_add_flags_for_s():
    name = "cfi_assembly_support_does_not_add_flags_for_s"
    native.cc_binary(
        name = name,
        srcs = ["baz.s", "blah.S"],
        features = [
            "android_cfi",
            "android_cfi_assembly_support",
        ],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_absent_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_absent_flags = [
            generated_sanitizer_constants.CfiAssemblySupportFlag,
        ],
    )

    return test_name

def test_cfi_exports_map_has_correct_flags():
    name = "cfi_exports_map_has_correct_flags"
    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = [
            "android_cfi",
            "android_cfi_exports_map",
        ],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_present_only_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [link_action_mnemonic],
        expected_flags = [
            generated_config_constants.VersionScriptFlagPrefix +
            generated_sanitizer_constants.CfiExportsMapPath +
            "/" +
            generated_sanitizer_constants.CfiExportsMapFilename,
        ],
    )

    return test_name

def test_cfi_cross_dso_has_correct_flags():
    name = "cfi_cross_dso_has_correct_flags"
    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = [
            "android_cfi",
            "android_cfi_cross_dso",
        ],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_present_only_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic, link_action_mnemonic],
        expected_flags = [generated_sanitizer_constants.CfiCrossDsoFlag],
    )

    return test_name

def test_cfi_cross_dso_does_not_add_flags_for_s():
    name = "cfi_cross_dso_does_not_add_flags_for_s"
    native.cc_binary(
        name = name,
        srcs = ["baz.s", "blah.S"],
        features = [
            "android_cfi",
            "android_cfi_cross_dso",
        ],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_absent_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_absent_flags = [generated_sanitizer_constants.CfiCrossDsoFlag],
    )

    return test_name

# TODO(b/283951987): Swtich to thin LTO when possible
def test_cfi_implies_lto():
    name = "cfi_implies_lto"
    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = ["android_cfi"],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_present_for_mnemonic_nonexclusive_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_flags = ["-flto"],
    )

    return test_name

def test_cfi_implies_vis_default_if_not_hidden():
    name = "cfi_implies_vis_default_if_not_hidden"
    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = ["android_cfi"],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_present_only_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_flags = ["-fvisibility=default"],
    )

    return test_name

def test_cfi_does_not_add_vis_default_if_hidden():
    name = "cfi_does_not_add_vis_default_if_hidden"
    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = ["android_cfi", "visibility_hidden"],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_absent_for_mnemonic_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_absent_flags = ["-fvisibility=default"],
    )

    return test_name

def test_cfi_and_arm_uses_thumb():
    name = "cfi_and_arm_uses_thumb"
    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = ["android_cfi"],
        tags = ["manual"],
    )

    test_name = name + "_test"
    action_flags_present_for_mnemonic_nonexclusive_test(
        name = test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_flags = generated_config_constants.ArmThumbCflags,
        target_compatible_with = [
            "//build/bazel/platforms/arch:arm",
        ],
    )

    return test_name

# TODO(b/274924237): Uncomment after Darwin and Windows have toolchains
#def test_cfi_absent_on_unsupported_oses():
#    name = "cfi_absent_on_unsupported_oses"
#    native.cc_binary(
#        name = name,
#        srcs = ["foo.c", "bar.cpp"],
#        features = ["android_cfi"],
#        tags = ["manual"],
#    )
#
#    test_name = name + "_test"
#    action_flags_absent_for_mnemonic_test(
#        name = test_name,
#        target_under_test = name,
#        mnemonics = [compile_action_mnemonic],
#        expected_absent_flags = generated_sanitizer_constants.CfiCFlags,
#        target_compatible_with = [
#            "//build/bazel/platforms/os:linux_musl",
#            "//build/bazel/platforms/os:darwin",
#            "//build/bazel/platforms/os:windows",
#        ]
#    )
#
#    return test_name

def _test_host_only_and_device_only_features():
    name = "cfi_host_only_and_device_only_features"
    test_names = []

    native.cc_binary(
        name = name,
        srcs = ["foo.c", "bar.cpp"],
        features = ["android_cfi"],
        tags = ["manual"],
    )

    host_with_host_flags_test_name = name + "_host_flags_present_when_host_test"
    test_names += [host_with_host_flags_test_name]
    action_flags_present_only_for_mnemonic_aosp_arm64_host_test(
        name = host_with_host_flags_test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_flags = generated_sanitizer_constants.HostOnlySanitizeFlags,
    )

    device_with_host_flags_test_name = name + "_host_flags_absent_when_device_test"
    test_names += [device_with_host_flags_test_name]
    action_flags_absent_for_mnemonic_aosp_arm64_test(
        name = device_with_host_flags_test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_absent_flags = generated_sanitizer_constants.HostOnlySanitizeFlags,
    )

    device_with_device_flags_test_name = name + "_device_flags_present_when_device_test"
    test_names += [device_with_device_flags_test_name]
    action_flags_present_only_for_mnemonic_aosp_arm64_test(
        name = device_with_device_flags_test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_flags = generated_sanitizer_constants.DeviceOnlySanitizeFlags,
    )

    host_with_device_flags_test_name = name + "_device_flags_absent_when_host_test"
    test_names += [host_with_device_flags_test_name]
    action_flags_absent_for_mnemonic_aosp_arm64_host_test(
        name = host_with_device_flags_test_name,
        target_under_test = name,
        mnemonics = [compile_action_mnemonic],
        expected_absent_flags = generated_sanitizer_constants.DeviceOnlySanitizeFlags,
    )

    return test_names

def _test_device_only_and_host_only_features_absent_when_cfi_disabled():
    pass

def cc_toolchain_features_cfi_test_suite(name):
    individual_tests = [
        test_cross_dso_not_added_when_cfi_disabled(),
        test_cfi_assembly_support_has_correct_flags(),
        test_cfi_assembly_support_does_not_add_flags_for_s(),
        test_cfi_exports_map_has_correct_flags(),
        test_cfi_cross_dso_has_correct_flags(),
        test_cfi_cross_dso_does_not_add_flags_for_s(),
        test_cfi_implies_lto(),
        test_cfi_implies_vis_default_if_not_hidden(),
        test_cfi_does_not_add_vis_default_if_hidden(),
        test_cfi_and_arm_uses_thumb(),
        # TODO(b/274924237): Uncomment after Darwin and Windows have toolchains
        # test_cfi_absent_on_unsupported_oses(),
    ]
    native.test_suite(
        name = name,
        tests = individual_tests +
                test_cfi_c_and_cpp_has_correct_flags() +
                test_cfi_s_has_correct_flags() +
                test_cross_dso_not_added_when_static_binary() +
                _test_host_only_and_device_only_features(),
    )
