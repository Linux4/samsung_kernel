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

load("//build/bazel/rules/cc:cc_binary.bzl", "cc_binary")
load(
    "//build/bazel/rules/test_common:flags.bzl",
    "action_flags_absent_for_mnemonic_test",
    "action_flags_present_only_for_mnemonic_test",
)

_SHT_RELR_ARGS = ["-Wl,--pack-dyn-relocs=android+relr"]
_ANDROID_RELR_ARGS = ["-Wl,--pack-dyn-relocs=android+relr", "-Wl,--use-android-relr-tags"]
_RELR_PACKER_ARGS = ["-Wl,--pack-dyn-relocs=android"]
_DISABLE_RELOC_ARGS = ["-Wl,--pack-dyn-relocs=none"]

# Include these different file types to make sure that all actions types are
# triggered
test_srcs = [
    "foo.cpp",
    "bar.c",
    "baz.s",
    "blah.S",
]

def _create_relr_targets(name, sdk_version, extra_target_features = []):
    name = "relocation_features_" + name
    cc_binary(
        name = name,
        srcs = test_srcs,
        min_sdk_version = sdk_version,
        features = extra_target_features,
        tags = ["manual"],
    )
    return name

def _test_relocation_feature_flags(*, name, sdk_version, flags):
    name = _create_relr_targets(name, sdk_version)
    android_test_name = name + "_android_test"
    linux_test_name = name + "_linux_test"
    target_under_test = name + "_unstripped"
    action_flags_present_only_for_mnemonic_test(
        name = android_test_name,
        target_under_test = target_under_test,
        mnemonics = ["CppLink"],
        expected_flags = flags,
        target_compatible_with = [
            "//build/bazel/platforms/os:android",
        ],
    )
    action_flags_absent_for_mnemonic_test(
        name = linux_test_name,
        target_under_test = target_under_test,
        mnemonics = ["CppLink"],
        expected_absent_flags = flags,
        target_compatible_with = [
            "//build/bazel/platforms/os:linux",
        ],
    )
    return [
        android_test_name,
        linux_test_name,
    ]

def _test_no_relocation_features(*, name, sdk_version, extra_target_features = []):
    name = _create_relr_targets(name, sdk_version, extra_target_features)
    android_test_name = name + "_android_test"
    linux_test_name = name + "_linux_test"
    target_under_test = name + "_unstripped"
    action_flags_absent_for_mnemonic_test(
        name = android_test_name,
        target_under_test = target_under_test,
        mnemonics = ["CppLink"],
        expected_absent_flags = _SHT_RELR_ARGS + _ANDROID_RELR_ARGS + _RELR_PACKER_ARGS,
        target_compatible_with = [
            "//build/bazel/platforms/os:android",
        ],
    )
    action_flags_absent_for_mnemonic_test(
        name = linux_test_name,
        target_under_test = target_under_test,
        mnemonics = ["CppLink"],
        expected_absent_flags = _SHT_RELR_ARGS + _ANDROID_RELR_ARGS + _RELR_PACKER_ARGS,
        target_compatible_with = [
            "//build/bazel/platforms/os:linux",
        ],
    )
    return [
        android_test_name,
        linux_test_name,
    ]

def _test_disable_relocation_features(*, name, sdk_version, extra_target_features):
    name = _create_relr_targets(name, sdk_version, extra_target_features)
    android_test_name = name + "_android_test"
    linux_test_name = name + "_linux_test"
    target_under_test = name + "_unstripped"
    action_flags_present_only_for_mnemonic_test(
        name = android_test_name,
        target_under_test = target_under_test,
        mnemonics = ["CppLink"],
        expected_flags = _DISABLE_RELOC_ARGS,
        target_compatible_with = [
            "//build/bazel/platforms/os:android",
        ],
    )
    action_flags_present_only_for_mnemonic_test(
        name = linux_test_name,
        target_under_test = target_under_test,
        mnemonics = ["CppLink"],
        expected_flags = _DISABLE_RELOC_ARGS,
        target_compatible_with = [
            "//build/bazel/platforms/os:linux",
        ],
    )
    return [
        android_test_name,
        linux_test_name,
    ] + _test_no_relocation_features(
        name = name + "_no_relr_features",
        sdk_version = sdk_version,
        extra_target_features = extra_target_features,
    )

def _generate_relocation_feature_tests():
    return (
        _test_relocation_feature_flags(
            name = "sdk_version_30",
            sdk_version = "30",
            flags = _SHT_RELR_ARGS,
        ) +
        _test_relocation_feature_flags(
            name = "sdk_version_29",
            # this sdk_version is too low for sht_relr
            sdk_version = "29",
            flags = _ANDROID_RELR_ARGS,
        ) +
        _test_relocation_feature_flags(
            name = "sdk_version_27",
            # this sdk_version is too low for android_relr
            sdk_version = "27",
            flags = _RELR_PACKER_ARGS,
        ) +
        _test_relocation_feature_flags(
            name = "sdk_version_24",
            sdk_version = "24",
            flags = _RELR_PACKER_ARGS,
        ) +
        _test_no_relocation_features(
            name = "sdk_version_22",
            # this sdk_version is too low for relocation_packer
            sdk_version = "22",
        ) +
        _test_disable_relocation_features(
            name = "sdk_version_30_disable_pack_relocations",
            sdk_version = "30",
            extra_target_features = ["disable_pack_relocations"],
        ) +
        _test_disable_relocation_features(
            name = "sdk_version_29_disable_pack_relocations",
            sdk_version = "29",
            extra_target_features = ["disable_pack_relocations"],
        ) +
        _test_disable_relocation_features(
            name = "sdk_version_27_disable_pack_relocations",
            sdk_version = "27",
            extra_target_features = ["disable_pack_relocations"],
        ) +
        _test_disable_relocation_features(
            name = "sdk_version_24_disable_pack_relocations",
            sdk_version = "24",
            extra_target_features = ["disable_pack_relocations"],
        ) +
        _test_disable_relocation_features(
            name = "sdk_version_22_disable_pack_relocations",
            sdk_version = "22",
            extra_target_features = ["disable_pack_relocations"],
        )
    )

def cc_toolchain_features_pack_relocation_test_suite(name):
    native.test_suite(
        name = name,
        tests = _generate_relocation_feature_tests(),
    )
