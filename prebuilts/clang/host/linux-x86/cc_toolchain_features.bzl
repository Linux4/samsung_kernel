"""Feature definitions for Android's C/C++ toolchain.

This top level list of features are available through the get_features function.
"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "feature",
    "feature_set",
    "flag_group",
    "flag_set",
    "variable_with_value",
    "with_feature_set",
)
load("//build/bazel/product_config:product_variables_providing_rule.bzl", "ProductVariablesInfo")
load(
    ":cc_toolchain_constants.bzl",
    _actions = "actions",
    _arches = "arches",
    _c_std_versions = "c_std_versions",
    _cpp_std_versions = "cpp_std_versions",
    _default_c_std_version = "default_c_std_version",
    _default_cpp_std_version = "default_cpp_std_version",
    _default_cpp_std_version_no_gnu = "default_cpp_std_version_no_gnu",
    _experimental_cpp_std_version = "experimental_cpp_std_version",
    _experimental_cpp_std_version_no_gnu = "experimental_cpp_std_version_no_gnu",
    _flags = "flags",
    _generated_config_constants = "generated_config_constants",
    _generated_sanitizer_constants = "generated_sanitizer_constants",
    _oses = "oses",
)
load("//build/bazel/rules/common:api.bzl", "api")
load("@soong_injection//api_levels:platform_versions.bzl", "platform_versions")

def is_os_device(os):
    return os == _oses.Android

def is_os_bionic(os):
    return os == _oses.Android or os == _oses.LinuxBionic

def _sdk_version_features_between(start, end):
    return ["sdk_version_" + str(i) for i in range(start, end + 1)]

def _sdk_version_features_before(version):
    return _sdk_version_features_between(1, version - 1)

def _get_sdk_version_features(os_is_device, target_arch):
    if not os_is_device:
        return []

    default_sdk_version = "10000"
    sdk_feature_prefix = "sdk_version_"
    all_sdk_versions = [default_sdk_version]
    for level in api.api_levels.values():
        all_sdk_versions.append(str(level))

    # Explicitly support internal branch state where the platform sdk version has
    # finalized, but the sdk is still active, so it's represented by a 9000-ish
    # value in api_levels.
    platform_sdk_version = str(platform_versions.platform_sdk_version)
    if platform_sdk_version not in all_sdk_versions:
        all_sdk_versions.append(platform_sdk_version)

    flag_prefix = "--target="
    if target_arch == _arches.X86:
        flag_prefix += "i686-linux-android"
    elif target_arch == _arches.X86_64:
        flag_prefix += "x86_64-linux-android"
    elif target_arch == _arches.Arm:
        flag_prefix += _generated_config_constants.ArmClangTriple
    elif target_arch == _arches.Arm64:
        flag_prefix += "aarch64-linux-android"
    else:
        fail("Unknown target arch %s" % (target_arch))

    features = [feature(
        name = "sdk_version_default",
        enabled = True,
        implies = [sdk_feature_prefix + default_sdk_version],
    )]
    features.extend([
        feature(name = sdk_feature_prefix + sdk_version, provides = ["sdk_version"])
        for sdk_version in all_sdk_versions
    ])
    features.append(feature(
        name = "sdk_version_flag",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = _actions.compile + _actions.link,
                flag_groups = [
                    flag_group(
                        flags = [flag_prefix + sdk_version],
                    ),
                ],
                with_features = [
                    with_feature_set(
                        features = [sdk_feature_prefix + sdk_version],
                    ),
                ],
            )
            for sdk_version in all_sdk_versions
        ],
    ))
    return features

def _get_c_std_features():
    features = []
    features.append(feature(
        # The default cpp_std feature. Remember to disable
        # this feature if enabling another cpp_std feature.
        name = "cpp_std_default",
        enabled = True,
        implies = [_default_cpp_std_version],
    ))
    features.append(feature(
        # The default c_std feature. Remember to disable
        # this feature if enabling another cpp_std feature.
        name = "c_std_default",
        enabled = True,
        implies = [_default_c_std_version],
    ))

    features.append(feature(
        name = "cpp_std_default_no_gnu",
        implies = [_default_cpp_std_version_no_gnu],
    ))
    features.append(feature(
        name = "c_std_default_no_gnu",
        implies = [_default_cpp_std_version_no_gnu],
    ))
    features.append(feature(
        name = "cpp_std_experimental",
        implies = [_experimental_cpp_std_version],
    ))
    features.append(feature(
        name = "c_std_experimental",
        implies = [_experimental_cpp_std_version],
    ))
    features.append(feature(
        name = "cpp_std_experimental_no_gnu",
        implies = [_experimental_cpp_std_version_no_gnu],
    ))
    features.append(feature(
        name = "c_std_experimental_no_gnu",
        implies = [_experimental_cpp_std_version_no_gnu],
    ))
    features.extend([
        feature(name = std_version, provides = ["cpp_std"])
        for std_version in _cpp_std_versions
    ])
    features.extend([
        feature(name = std_version, provides = ["c_std"])
        for std_version in _c_std_versions
    ])
    features.append(feature(
        name = "cpp_std_flag",
        enabled = True,
        # Create the -std flag group for each of the std versions,
        # enabled with with_feature_set.
        flag_sets = [
            flag_set(
                actions = [_actions.cpp_compile],
                flag_groups = [
                    flag_group(
                        flags = ["-std=" + std_version],
                    ),
                ],
                with_features = [
                    with_feature_set(
                        features = [std_version],
                    ),
                ],
            )
            for std_version in _cpp_std_versions
        ],
    ))
    features.append(feature(
        name = "c_std_flag",
        enabled = True,
        # Create the -std flag group for each of the std versions,
        # enabled with with_feature_set.
        flag_sets = [
            flag_set(
                actions = [_actions.c_compile],
                flag_groups = [
                    flag_group(
                        flags = ["-std=" + std_version],
                    ),
                ],
                with_features = [
                    with_feature_set(
                        features = [std_version],
                    ),
                ],
            )
            for std_version in _c_std_versions
        ],
    ))
    return features

def _env_based_common_global_cflags(ctx):
    flags = []

    # The logic comes from https://cs.android.com/android/platform/superproject/+/master:build/soong/cc/config/global.go;l=332;drc=af32e1ba3ffca6b552ac1ff6d14e5c3a5148cb80
    auto_pattern_initialize = ctx.attr._auto_pattern_initialize[BuildSettingInfo].value
    auto_uninitialize = ctx.attr._auto_uninitialize[BuildSettingInfo].value
    if ctx.attr._auto_zero_initialize[BuildSettingInfo].value:
        flags.extend(["-ftrivial-auto-var-init=zero"])
    elif auto_pattern_initialize:
        flags.extend(["-ftrivial-auto-var-init=pattern"])
    elif auto_uninitialize:
        flags.extend(["-ftrivial-auto-var-init=uninitialized"])
    else:
        # Default to zero initialization.
        flags.extend(["-ftrivial-auto-var-init=zero"])

    if ctx.attr._use_ccache[BuildSettingInfo].value or (not auto_pattern_initialize and not auto_uninitialize):
        flags.extend(["-Wno-unused-command-line-argument"])

    if ctx.attr._llvm_next[BuildSettingInfo].value:
        flags.extend(["-Wno-error=single-bit-bitfield-constant-conversion"])

    if ctx.attr._allow_unknown_warning_option[BuildSettingInfo].value:
        flags.extend(["-Wno-error=unknown-warning-option"])

    clang_debug_env_value = ctx.attr._clang_default_debug_level[BuildSettingInfo].value
    if clang_debug_env_value == "":
        flags.extend(["-g"])
    elif clang_debug_env_value == "debug_level_g":
        flags.extend(["-g"])
    elif clang_debug_env_value == "debug_level_0":
        flags.extend(["-g0"])
    elif clang_debug_env_value == "debug_level_1":
        flags.extend(["-g1"])
    elif clang_debug_env_value == "debug_level_2":
        flags.extend(["-g2"])
    elif clang_debug_env_value == "debug_level_3":
        flags.extend(["-g3"])

    return flags

def _compiler_flag_features(ctx, target_arch, target_os, flags = []):
    os_is_device = is_os_device(target_os)
    compiler_flags = []

    # Combine the toolchain's provided flags with the default ones.
    compiler_flags.extend(flags)
    compiler_flags.extend(_flags.compiler_flags)
    compiler_flags.extend(_generated_config_constants.CommonGlobalCflags)
    compiler_flags.extend(_env_based_common_global_cflags(ctx))

    if os_is_device:
        compiler_flags.extend(_generated_config_constants.DeviceGlobalCflags)
    else:
        compiler_flags.extend(_generated_config_constants.HostGlobalCflags)

    # Default compiler flags for assembly sources.
    asm_only_flags = _generated_config_constants.CommonGlobalAsflags

    # Default C++ compile action only flags (No C)
    cpp_only_flags = []
    cpp_only_flags.extend(_generated_config_constants.CommonGlobalCppflags)
    if os_is_device:
        cpp_only_flags.extend(_generated_config_constants.DeviceGlobalCppflags)
    else:
        cpp_only_flags.extend(_generated_config_constants.HostGlobalCppflags)

    # Default C compile action only flags (No C++)
    c_only_flags = []
    c_only_flags.extend(_generated_config_constants.CommonGlobalConlyflags)

    # Flags that only apply in the external/ directory.
    non_external_flags = _flags.non_external_defines

    features = []

    # TODO: disabled on windows
    features.append(feature(
        name = "pic",
        enabled = False,
        flag_sets = [
            flag_set(
                actions = _actions.compile,
                flag_groups = [
                    flag_group(
                        flags = ["-fPIC"],
                    ),
                ],
                with_features = [
                    with_feature_set(
                        features = ["linker_flags"],
                        not_features = ["pie"],
                    ),
                ],
            ),
        ],
    ))

    features.append(feature(
        name = "pie",
        enabled = False,
        flag_sets = [
            flag_set(
                actions = _actions.compile,
                flag_groups = [
                    flag_group(
                        flags = ["-fPIE"],
                    ),
                ],
                with_features = [
                    with_feature_set(
                        features = ["linker_flags"],
                        not_features = ["pic"],
                    ),
                ],
            ),
        ],
    ))

    features.append(feature(
        name = "non_external_compiler_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = _actions.compile,
                flag_groups = [
                    flag_group(
                        flags = non_external_flags,
                    ),
                ],
                with_features = [
                    with_feature_set(
                        not_features = ["external_compiler_flags"],
                    ),
                ],
            ),
        ],
    ))

    # bpf only needs the flag below instead of all the flags in
    # common_compiler_flags
    features.append(feature(
        name = "bpf_compiler_flags",
        enabled = False,
        flag_sets = [
            flag_set(
                actions = _actions.compile,
                flag_groups = [
                    flag_group(
                        flags = ["-fdebug-prefix-map=/proc/self/cwd="],
                    ),
                ],
            ),
        ],
    ))

    if target_arch == _arches.Arm:
        features.append(feature(
            name = "arm_isa_arm",
            enabled = False,
            flag_sets = [
                flag_set(
                    actions = _actions.compile,
                    flag_groups = [
                        flag_group(
                            flags = ["-fstrict-aliasing"],
                        ),
                    ],
                ),
            ],
        ))

        features.append(feature(
            name = "arm_isa_thumb",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = _actions.compile,
                    flag_groups = [
                        flag_group(
                            flags = _generated_config_constants.ArmThumbCflags,
                        ),
                    ],
                    with_features = [
                        with_feature_set(
                            not_features = ["arm_isa_arm"],
                        ),
                    ],
                ),
            ],
        ))

    # Must follow arm_isa_thumb for flag ordering
    features.append(feature(
        name = "common_compiler_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = _actions.compile,
                flag_groups = [
                    flag_group(
                        flags = compiler_flags,
                    ),
                ],
            ),
        ],
    ))

    features.append(feature(
        name = "asm_compiler_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = _actions.assemble,
                flag_groups = [
                    flag_group(
                        flags = asm_only_flags,
                    ),
                ],
            ),
        ],
    ))

    features.append(feature(
        name = "cpp_compiler_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = [_actions.cpp_compile],
                flag_groups = [
                    flag_group(
                        flags = cpp_only_flags,
                    ),
                ],
            ),
        ],
    ))

    if c_only_flags:
        features.append(feature(
            name = "c_compiler_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [_actions.c_compile],
                    flag_groups = [
                        flag_group(
                            flags = c_only_flags,
                        ),
                    ],
                ),
            ],
        ))

    features.append(feature(
        name = "external_compiler_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = _actions.compile,
                flag_groups = [
                    flag_group(
                        flags = _generated_config_constants.ExternalCflags,
                    ),
                ],
                with_features = [
                    with_feature_set(
                        not_features = ["non_external_compiler_flags"],
                    ),
                ],
            ),
        ],
    ))

    features.append(feature(
        name = "warnings_as_errors",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = _actions.compile,
                flag_groups = [
                    flag_group(
                        flags = ["-Werror"],
                    ),
                ],
            ),
        ],
    ))

    # The user_compile_flags feature is used by Bazel to add --copt, --conlyopt,
    # and --cxxopt values. Any features added above this call will thus appear
    # earlier in the commandline than the user opts (so users could override
    # flags set by earlier features). Anything after the user options are
    # effectively non-overridable by users.
    features.append(feature(
        name = "user_compile_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = _actions.compile,
                flag_groups = [
                    flag_group(
                        expand_if_available = "user_compile_flags",
                        flags = ["%{user_compile_flags}"],
                        iterate_over = "user_compile_flags",
                    ),
                ],
            ),
        ],
    ))

    return features

def _get_no_override_compile_flags(target_os):
    features = []

    # These cannot be overriden by the user.
    features.append(feature(
        name = "no_override_clang_global_copts",
        enabled = True,
        flag_sets = [
            flag_set(
                # We want this to apply to all actions except assembly
                # primarily to match Soong's semantics
                actions = [a for a in _actions.compile if a not in _actions.assemble],
                flag_groups = [
                    flag_group(
                        flags = _generated_config_constants.NoOverrideGlobalCflags,
                    ),
                ],
            ),
        ],
    ))

    if target_os != _oses.Darwin:
        # These cannot be overriden by the user.
        features.append(feature(
            name = "no_override_clang_external_global_copts",
            enabled = True,
            flag_sets = [
                flag_set(
                    # We want this to apply to all actions except assembly
                    # primarily to match Soong's semantics
                    actions = [a for a in _actions.compile if a not in _actions.assemble],
                    flag_groups = [
                        flag_group(
                            flags = _generated_config_constants.NoOverrideExternalGlobalCflags,
                        ),
                    ],
                ),
            ],
            requires = [
                feature_set(features = [
                    "external_compiler_flags",
                ]),
            ],
        ))

    return features

def _rtti_features(rtti_toggle):
    if not rtti_toggle:
        return []

    rtti_flag_feature = feature(
        name = "rtti_flag",
        flag_sets = [
            flag_set(
                actions = [_actions.cpp_compile],
                flag_groups = [
                    flag_group(
                        flags = ["-frtti"],
                    ),
                ],
                with_features = [
                    with_feature_set(features = ["rtti"]),
                ],
            ),
            flag_set(
                actions = [_actions.cpp_compile],
                flag_groups = [
                    flag_group(
                        flags = ["-fno-rtti"],
                    ),
                ],
                with_features = [
                    with_feature_set(not_features = ["rtti"]),
                ],
            ),
        ],
        enabled = True,
    )
    rtti_feature = feature(
        name = "rtti",
        enabled = False,
    )
    return [rtti_flag_feature, rtti_feature]

# TODO(b/202167934): Darwin does not support pack dynamic relocations
def _pack_dynamic_relocations_features(target_os):
    sht_relr_flags = flag_set(
        actions = _actions.link,
        flag_groups = [
            flag_group(
                flags = ["-Wl,--pack-dyn-relocs=android+relr"],
            ),
        ],
        with_features = [
            with_feature_set(
                features = ["linker_flags"],
                not_features = ["disable_pack_relocations"] + _sdk_version_features_before(30),
            ),
        ],
    )
    android_relr_flags = flag_set(
        actions = _actions.link,
        flag_groups = [
            flag_group(
                flags = ["-Wl,--pack-dyn-relocs=android+relr", "-Wl,--use-android-relr-tags"],
            ),
        ],
        with_features = [
            with_feature_set(
                features = ["linker_flags", version],
                not_features = ["disable_pack_relocations"],
            )
            for version in _sdk_version_features_between(28, 29)
        ],
    )
    relocation_packer_flags = flag_set(
        actions = _actions.link,
        flag_groups = [
            flag_group(
                flags = ["-Wl,--pack-dyn-relocs=android"],
            ),
        ],
        with_features = [
            with_feature_set(
                features = ["linker_flags", version],
                not_features = ["disable_pack_relocations"],
            )
            for version in _sdk_version_features_between(23, 27)
        ],
    )

    if is_os_bionic(target_os):
        pack_dyn_relr_flag_sets = [
            sht_relr_flags,
            android_relr_flags,
            relocation_packer_flags,
        ]
    else:
        pack_dyn_relr_flag_sets = []

    pack_dynamic_relocations_feature = feature(
        name = "pack_dynamic_relocations",
        enabled = True,
        flag_sets = pack_dyn_relr_flag_sets,
    )
    disable_pack_relocations_feature = feature(
        # this will take precedence over the pack_dynamic_relocations feature
        # because each flag_set in that feature explicitly disallows the
        # disable_dynamic_relocations feature
        name = "disable_pack_relocations",
        flag_sets = [
            flag_set(
                actions = _actions.link,
                flag_groups = [
                    flag_group(
                        flags = ["-Wl,--pack-dyn-relocs=none"],
                    ),
                ],
                with_features = [
                    with_feature_set(
                        features = ["linker_flags"],
                    ),
                ],
            ),
        ],
        enabled = False,
    )
    return [
        pack_dynamic_relocations_feature,
        disable_pack_relocations_feature,
    ]

# TODO(b/202167934): Darwin by default disallows undefined symbols, to allow, -Wl,undefined,dynamic_lookup
def _undefined_symbols_feature(target_os):
    return _linker_flag_feature(
        "no_undefined_symbols",
        flags = ["-Wl,--no-undefined"],
        enabled = is_os_bionic(target_os) or target_os == _oses.LinuxMusl,
    )

def _dynamic_linker_flag_feature(target_os, arch_is_64_bit):
    flags = []
    if is_os_device(target_os):
        # TODO: handle bootstrap partition, asan
        dynamic_linker_path = "/system/bin/linker"
        if arch_is_64_bit:
            dynamic_linker_path += "64"
        flags.append("-Wl,-dynamic-linker," + dynamic_linker_path)
    elif is_os_bionic(target_os) or target_os == _oses.LinuxMusl:
        flags.append("-Wl,--no-dynamic-linker")
    return _binary_linker_flag_feature(name = "dynamic_linker", flags = flags) if len(flags) else []

# TODO(b/202167934): Darwin uses @loader_path in place of $ORIGIN
def _rpath_features(target_os, arch_is_64_bit):
    runtime_library_search_directories_flag_sets = [
        flag_set(
            actions = _actions.link,
            flag_groups = [
                flag_group(
                    iterate_over = "runtime_library_search_directories",
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-Wl,-rpath,$EXEC_ORIGIN/%{runtime_library_search_directories}",
                            ],
                            expand_if_true = "is_cc_test",
                        ),
                        flag_group(
                            flags = [
                                "-Wl,-rpath,$ORIGIN/%{runtime_library_search_directories}",
                            ],
                            expand_if_false = "is_cc_test",
                        ),
                    ],
                    expand_if_available =
                        "runtime_library_search_directories",
                ),
            ],
            with_features = [
                with_feature_set(features = ["static_link_cpp_runtimes"]),
            ],
        ),
        flag_set(
            actions = _actions.link,
            flag_groups = [
                flag_group(
                    iterate_over = "runtime_library_search_directories",
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-Wl,-rpath,$ORIGIN/%{runtime_library_search_directories}",
                            ],
                        ),
                    ],
                    expand_if_available =
                        "runtime_library_search_directories",
                ),
            ],
            with_features = [
                with_feature_set(
                    not_features = ["static_link_cpp_runtimes", "disable_rpath"],
                ),
            ],
        ),
    ]

    if (not is_os_device(target_os)) and arch_is_64_bit:
        runtime_library_search_directories_flag_sets.append(flag_set(
            actions = _actions.link,
            flag_groups = [
                flag_group(
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-Wl,-rpath,$ORIGIN/../lib64",
                                "-Wl,-rpath,$ORIGIN/lib64",
                            ],
                        ),
                    ],
                ),
            ],
            with_features = [
                with_feature_set(not_features = ["static_link_cpp_runtimes"]),
            ],
        ))

    runtime_library_search_directories_feature = feature(
        name = "runtime_library_search_directories",
        flag_sets = runtime_library_search_directories_flag_sets,
    )

    disable_rpath_feature = feature(
        name = "disable_rpath",
        enabled = False,
    )
    return [runtime_library_search_directories_feature, disable_rpath_feature]

def _use_libcrt_feature(path):
    if not path:
        return None
    return _flag_feature("use_libcrt", actions = _actions.link, flags = [
        path.path,
        "-Wl,--exclude-libs=" + path.basename,
    ])

def _flag_feature(name, actions = None, flags = None, enabled = True):
    if not flags or not actions:
        return None
    return feature(
        name = name,
        enabled = enabled,
        flag_sets = [
            flag_set(
                actions = actions,
                flag_groups = [
                    flag_group(flags = flags),
                ],
            ),
        ],
    )

def _linker_flag_feature(name, flags = [], enabled = True):
    return _flag_feature(name, actions = _actions.link, flags = flags, enabled = enabled)

def _archiver_flag_feature(name, flags = [], enabled = True):
    return _flag_feature(name, actions = _actions.archive, flags = flags, enabled = enabled)

def _binary_linker_flag_feature(name, flags = [], enabled = True):
    return _flag_feature(name, actions = [_actions.cpp_link_executable], flags = flags, enabled = enabled)

def _toolchain_include_feature(system_includes = []):
    flags = []
    for include in system_includes:
        flags.append("-isystem")
        flags.append(include)
    if not flags:
        return None
    return feature(
        name = "toolchain_include_directories",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = _actions.compile,
                flag_groups = [
                    flag_group(
                        flags = flags,
                    ),
                ],
            ),
        ],
    )

def _stub_library_feature():
    return feature(
        name = "stub_library",
        enabled = False,
        flag_sets = [
            flag_set(
                actions = [_actions.c_compile],
                flag_groups = [
                    flag_group(
                        # Ensures that the stub libraries are always compiled with default visibility
                        flags = _generated_config_constants.StubLibraryCompilerFlags + ["-fvisibility=default"],
                    ),
                ],
            ),
        ],
    )

def _flatten(xs):
    ret = []
    for x in xs:
        if type(x) == "list":
            ret.extend(x)
        else:
            ret.append(x)
    return ret

def _additional_archiver_flags(target_os):
    archiver_flags = []
    if target_os != _oses.Darwin:
        archiver_flags.extend(_flags.non_darwin_archiver_flags)
    return archiver_flags

# Additional linker flags that are dependent on a host or device target.
def _additional_linker_flags(os_is_device):
    linker_flags = []
    if os_is_device:
        linker_flags.extend(_generated_config_constants.DeviceGlobalLldflags)
        linker_flags.extend(_flags.bionic_linker_flags)
    else:
        linker_flags.extend(_generated_config_constants.HostGlobalLldflags)
    return linker_flags

def _static_binary_linker_flags(os_is_device):
    linker_flags = []
    if os_is_device:
        linker_flags.extend(_flags.bionic_static_executable_linker_flags)
    return linker_flags

def _shared_binary_linker_flags(target_os):
    linker_flags = []
    if is_os_device(target_os):
        linker_flags.extend(_flags.bionic_dynamic_executable_linker_flags)
    elif target_os != _oses.Windows:
        linker_flags.extend(_flags.host_non_windows_dynamic_executable_linker_flags)
    return linker_flags

# Legacy features moved from their hardcoded Bazel's Java implementation
# to Starlark.
#
# These legacy features must come before all other features.
def _get_legacy_features_begin():
    features = [
        # Legacy features omitted from this list, since they're not used in
        # Android builds currently, or is alternatively supported through rules
        # directly (e.g. stripped_shared_library for debug symbol stripping).
        #
        # thin_lto: Do not add, as it may break some features. replaced by _get_thinlto_features()
        #
        # runtime_library_search_directories: replaced by custom _rpath_feature().
        #
        # Compile related features:
        #
        # random_seed
        # legacy_compile_flags
        # per_object_debug_info
        # pic
        # force_pic_flags
        #
        # Optimization related features:
        #
        # fdo_instrument
        # fdo_optimize
        # cs_fdo_instrument
        # cs_fdo_optimize
        # fdo_prefetch_hints
        # propeller_optimize
        #
        # Interface libraries related features:
        #
        # supports_interface_shared_libraries
        # build_interface_libraries
        # dynamic_library_linker_tool
        #
        # Coverage:
        #
        # coverage
        # llvm_coverage_map_format
        # gcc_coverage_map_format
        #
        # Others:
        #
        # symbol_counts
        # static_libgcc
        # fission_support
        # static_link_cpp_runtimes
        #
        # ------------------------
        #
        feature(
            name = "autofdo",
            flag_sets = [
                flag_set(
                    actions = _actions.compile,
                    flag_groups = [
                        flag_group(
                            # https://cs.android.com/android/platform/superproject/+/master:build/soong/cc/afdo.go;l=35;drc=7a8362c252b152f806fc303c414ff1418672b067
                            flags = [
                                "-funique-internal-linkage-names",
                                "-fprofile-sample-accurate",
                                "-fprofile-sample-use=%{fdo_profile_path}",
                            ],
                            expand_if_available = "fdo_profile_path",
                        ),
                    ],
                ),
            ],
            provides = ["profile"],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=98;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "dependency_file",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = _actions.compile,
                    flag_groups = [
                        flag_group(
                            expand_if_available = "dependency_file",
                            flags = [
                                "-MD",
                                "-MF",
                                "%{dependency_file}",
                            ],
                        ),
                    ],
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=186;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "preprocessor_defines",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = _actions.compile,
                    flag_groups = [
                        flag_group(
                            iterate_over = "preprocessor_defines",
                            flags = ["-D%{preprocessor_defines}"],
                        ),
                    ],
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=207;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "includes",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = _actions.compile,
                    flag_groups = [
                        flag_group(
                            expand_if_available = "includes",
                            iterate_over = "includes",
                            flags = ["-include", "%{includes}"],
                        ),
                    ],
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=232;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "include_paths",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = _actions.compile,
                    flag_groups = [
                        flag_group(
                            iterate_over = "quote_include_paths",
                            flags = ["-iquote", "%{quote_include_paths}"],
                        ),
                        flag_group(
                            iterate_over = "include_paths",
                            flags = ["-I", "%{include_paths}"],
                        ),
                        flag_group(
                            iterate_over = "system_include_paths",
                            flags = ["-isystem", "%{system_include_paths}"],
                        ),
                        flag_group(
                            flags = ["-F%{framework_include_paths}"],
                            iterate_over = "framework_include_paths",
                        ),
                    ],
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=476;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "shared_flag",
            flag_sets = [
                flag_set(
                    actions = [
                        _actions.cpp_link_dynamic_library,
                        _actions.cpp_link_nodeps_dynamic_library,
                    ],
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-shared",
                            ],
                        ),
                    ],
                ),
            ],
        ),

        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=492;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "linkstamps",
            flag_sets = [
                flag_set(
                    actions = _actions.link,
                    flag_groups = [
                        flag_group(
                            expand_if_available = "linkstamp_paths",
                            iterate_over = "linkstamp_paths",
                            flags = ["%{linkstamp_paths}"],
                        ),
                    ],
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=512;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "output_execpath_flags",
            flag_sets = [
                flag_set(
                    actions = _actions.link,
                    flag_groups = [
                        flag_group(
                            expand_if_available = "output_execpath",
                            flags = [
                                "-o",
                                "%{output_execpath}",
                            ],
                        ),
                    ],
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=592;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "library_search_directories",
            flag_sets = [
                flag_set(
                    actions = _actions.link,
                    flag_groups = [
                        flag_group(
                            expand_if_available = "library_search_directories",
                            iterate_over = "library_search_directories",
                            flags = ["-L%{library_search_directories}"],
                        ),
                    ],
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=612;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "archiver_flags",
            flag_sets = [
                flag_set(
                    actions = ["c++-link-static-library"],
                    flag_groups = [
                        flag_group(
                            flags = ["crsPD"],
                        ),
                        flag_group(
                            expand_if_available = "output_execpath",
                            flags = ["%{output_execpath}"],
                        ),
                    ],
                ),
                flag_set(
                    actions = ["c++-link-static-library"],
                    flag_groups = [
                        flag_group(
                            expand_if_available = "libraries_to_link",
                            iterate_over = "libraries_to_link",
                            flag_groups = [
                                flag_group(
                                    expand_if_equal = variable_with_value(
                                        name = "libraries_to_link.type",
                                        value = "object_file",
                                    ),
                                    flags = ["%{libraries_to_link.name}"],
                                ),
                            ],
                        ),
                        flag_group(
                            expand_if_equal = variable_with_value(
                                name = "libraries_to_link.type",
                                value = "object_file_group",
                            ),
                            iterate_over = "libraries_to_link.object_files",
                            flags = ["%{libraries_to_link.object_files}"],
                        ),
                    ],
                ),
            ],
        ),
        feature(
            name = "archive_with_prebuilt_flags",
            flag_sets = [
                flag_set(
                    actions = ["c++-link-static-library"],
                    flag_groups = [
                        flag_group(
                            flags = ["cqsL"],
                        ),
                        flag_group(
                            expand_if_available = "output_execpath",
                            flags = ["%{output_execpath}"],
                        ),
                    ],
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=653;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "libraries_to_link",
            flag_sets = [
                flag_set(
                    actions = _actions.link,
                    flag_groups = ([
                        flag_group(
                            expand_if_true = "thinlto_param_file",
                            flags = ["-Wl,@%{thinlto_param_file}"],
                        ),
                        flag_group(
                            expand_if_available = "libraries_to_link",
                            iterate_over = "libraries_to_link",
                            flag_groups = (
                                [
                                    flag_group(
                                        expand_if_equal = variable_with_value(
                                            name = "libraries_to_link.type",
                                            value = "object_file_group",
                                        ),
                                        expand_if_false = "libraries_to_link.is_whole_archive",
                                        flags = ["-Wl,--start-lib"],
                                    ),
                                    flag_group(
                                        expand_if_equal = variable_with_value(
                                            name = "libraries_to_link.type",
                                            value = "static_library",
                                        ),
                                        expand_if_true = "libraries_to_link.is_whole_archive",
                                        flags = ["-Wl,--whole-archive"],
                                    ),
                                    flag_group(
                                        expand_if_equal = variable_with_value(
                                            name = "libraries_to_link.type",
                                            value = "object_file_group",
                                        ),
                                        iterate_over = "libraries_to_link.object_files",
                                        flags = ["%{libraries_to_link.object_files}"],
                                    ),
                                    flag_group(
                                        expand_if_equal = variable_with_value(
                                            name = "libraries_to_link.type",
                                            value = "object_file",
                                        ),
                                        flags = ["%{libraries_to_link.name}"],
                                    ),
                                    flag_group(
                                        expand_if_equal = variable_with_value(
                                            name = "libraries_to_link.type",
                                            value = "interface_library",
                                        ),
                                        flags = ["%{libraries_to_link.name}"],
                                    ),
                                    flag_group(
                                        expand_if_equal = variable_with_value(
                                            name = "libraries_to_link.type",
                                            value = "static_library",
                                        ),
                                        flags = ["%{libraries_to_link.name}"],
                                    ),
                                    flag_group(
                                        expand_if_equal = variable_with_value(
                                            name = "libraries_to_link.type",
                                            value = "dynamic_library",
                                        ),
                                        flags = ["-l%{libraries_to_link.name}"],
                                    ),
                                    flag_group(
                                        expand_if_equal = variable_with_value(
                                            name = "libraries_to_link.type",
                                            value = "versioned_dynamic_library",
                                        ),
                                        flags = ["-l:%{libraries_to_link.name}"],
                                    ),
                                    flag_group(
                                        expand_if_equal = variable_with_value(
                                            name = "libraries_to_link.type",
                                            value = "static_library",
                                        ),
                                        expand_if_true = "libraries_to_link.is_whole_archive",
                                        flags = ["-Wl,--no-whole-archive"],
                                    ),
                                    flag_group(
                                        expand_if_equal = variable_with_value(
                                            name = "libraries_to_link.type",
                                            value = "object_file_group",
                                        ),
                                        expand_if_false = "libraries_to_link.is_whole_archive",
                                        flags = ["-Wl,--end-lib"],
                                    ),
                                ]
                            ),
                        ),
                    ]),
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=842;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "user_link_flags",
            flag_sets = [
                flag_set(
                    actions = _actions.link,
                    flag_groups = [
                        flag_group(
                            expand_if_available = "user_link_flags",
                            iterate_over = "user_link_flags",
                            flags = ["%{user_link_flags}"],
                        ),
                    ],
                ),
            ],
        ),
        feature(
            name = "strip_debug_symbols",
            flag_sets = [
                flag_set(
                    actions = _actions.link,
                    flag_groups = [
                        flag_group(
                            expand_if_available = "strip_debug_symbols",
                            flags = ["-Wl,-S"],
                        ),
                    ],
                ),
            ],
            enabled = False,
        ),
        feature(
            name = "llvm_coverage_map_format",
            flag_sets = [
                flag_set(
                    actions = _actions.compile,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fprofile-instr-generate=/data/misc/trace/clang-%%p-%%m.profraw",
                                "-fcoverage-mapping",
                                "-Wno-pass-failed",
                                "-D__ANDROID_CLANG_COVERAGE__",
                            ],
                        ),
                    ],
                ),
                flag_set(
                    actions = [_actions.c_compile, _actions.cpp_compile],
                    flag_groups = [flag_group(flags = ["-Wno-frame-larger-than="])],
                ),
                # TODO(b/233660582): support "-Wl,--wrap,open" and libprofile-clang-extras
                flag_set(
                    actions = _actions.link,
                    flag_groups = [flag_group(flags = ["-fprofile-instr-generate"])],
                ),
                flag_set(
                    actions = _actions.link,
                    flag_groups = [flag_group(flags = ["-Wl,--wrap,open"])],
                    with_features = [
                        with_feature_set(
                            features = ["android_coverage_lib"],
                        ),
                    ],
                ),
            ],
            requires = [feature_set(features = ["coverage"])],
        ),
        feature(name = "coverage"),
        feature(name = "android_coverage_lib"),
    ]

    return features

# Legacy features moved from their hardcoded Bazel's Java implementation
# to Starlark.
#
# These legacy features must come after all other features.
def _get_legacy_features_end():
    # Omitted legacy (unused or re-implemented) features:
    #
    # fully_static_link
    # user_compile_flags
    # sysroot
    features = [
        feature(
            name = "linker_param_file",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = _actions.link + _actions.archive,
                    flag_groups = [
                        flag_group(
                            expand_if_available = "linker_param_file",
                            flags = ["@%{linker_param_file}"],
                        ),
                    ],
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=1511;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "compiler_input_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = _actions.compile,
                    flag_groups = [
                        flag_group(
                            expand_if_available = "source_file",
                            flags = ["-c", "%{source_file}"],
                        ),
                    ],
                ),
            ],
        ),
        # https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java;l=1538;drc=6d03a2ecf25ad596446c296ef1e881b60c379812
        feature(
            name = "compiler_output_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = _actions.compile,
                    flag_groups = [
                        flag_group(
                            expand_if_available = "output_assembly_file",
                            flags = ["-S"],
                        ),
                        flag_group(
                            expand_if_available = "output_preprocess_file",
                            flags = ["-E"],
                        ),
                        flag_group(
                            expand_if_available = "output_file",
                            flags = ["-o", "%{output_file}"],
                        ),
                    ],
                ),
            ],
        ),
    ]

    return features

def _link_crtbegin(crt_files):
    # in practice, either all of these are supported for a toolchain or none of them do
    if crt_files.shared_library_crtbegin == None or crt_files.shared_binary_crtbegin == None or crt_files.static_binary_crtbegin == None:
        return []

    features = [
        feature(
            # User facing feature
            name = "link_crt",
            enabled = True,
            implies = ["link_crtbegin", "link_crtend"],
        ),
        feature(
            name = "link_crtbegin",
            enabled = True,
        ),
        feature(
            name = "link_crtbegin_so",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [_actions.cpp_link_dynamic_library],
                    flag_groups = [
                        flag_group(
                            flags = [crt_files.shared_library_crtbegin.path],
                        ),
                    ],
                    with_features = [
                        with_feature_set(
                            features = ["link_crt", "link_crtbegin"],
                        ),
                    ],
                ),
            ],
        ),
        feature(
            name = "link_crtbegin_dynamic",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [_actions.cpp_link_executable],
                    flag_groups = [
                        flag_group(
                            flags = [crt_files.shared_binary_crtbegin.path],
                        ),
                    ],
                    with_features = [
                        with_feature_set(
                            features = [
                                "dynamic_executable",
                                "link_crt",
                                "link_crtbegin",
                            ],
                        ),
                    ],
                ),
            ],
        ),
        feature(
            name = "link_crtbegin_static",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [_actions.cpp_link_executable],
                    flag_groups = [
                        flag_group(
                            flags = [crt_files.static_binary_crtbegin.path],
                        ),
                    ],
                    with_features = [
                        with_feature_set(
                            features = [
                                "link_crt",
                                "link_crtbegin",
                                "static_executable",
                            ],
                        ),
                    ],
                ),
            ],
        ),
    ]

    return features

def _link_crtend(crt_files):
    # in practice, either all of these are supported for a toolchain or none of them do
    if crt_files.shared_library_crtend == None or crt_files.binary_crtend == None:
        return None

    return [
        feature(
            name = "link_crtend",
            enabled = True,
        ),
        feature(
            name = "link_crtend_so",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [_actions.cpp_link_dynamic_library],
                    flag_groups = [
                        flag_group(
                            flags = [crt_files.shared_library_crtend.path],
                        ),
                    ],
                    with_features = [
                        with_feature_set(
                            features = ["link_crt", "link_crtend"],
                        ),
                    ],
                ),
            ],
        ),
        feature(
            name = "link_crtend_binary",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [_actions.cpp_link_executable],
                    flag_groups = [
                        flag_group(
                            flags = [crt_files.binary_crtend.path],
                        ),
                    ],
                    with_features = [
                        with_feature_set(
                            features = ["link_crt", "link_crtend"],
                        ),
                    ],
                ),
            ],
        ),
    ]

# TODO(b/276932249): Restrict for Fuzzer when we have Fuzzer in Bazel
def _get_thinlto_features():
    features = [
        feature(
            name = "android_thin_lto",
            enabled = False,
            flag_sets = [
                flag_set(
                    actions = _actions.compile + _actions.link + _actions.assemble,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-flto=thin",
                                "-fsplit-lto-unit",
                            ],
                        ),
                    ],
                ),
            ],
        ),
        feature(
            name = "android_thin_lto_whole_program_vtables",
            enabled = False,
            requires = [feature_set(features = ["android_thin_lto"])],
            flag_sets = [
                flag_set(
                    actions = _actions.link,
                    flag_groups = [
                        flag_group(
                            flags = ["-fwhole-program-vtables"],
                        ),
                    ],
                ),
            ],
        ),
        # See Soong code:
        # https://cs.android.com/android/platform/superproject/+/master:build/soong/cc/lto.go;l=133;drc=2c435a00ff73dc485855824ee49d2dec1a01e592
        feature(
            name = "android_thin_lto_limit_cross_tu_inline",
            enabled = True,
            requires = [feature_set(features = ["android_thin_lto"])],
            flag_sets = [
                flag_set(
                    actions = _actions.link,
                    flag_groups = [
                        flag_group(
                            flags = ["-Wl,-plugin-opt,-import-instr-limit=5"],
                        ),
                    ],
                    with_features = [
                        with_feature_set(
                            not_features = [
                                # TODO(b/267220812): Update for PGO
                                "autofdo",
                            ],
                        ),
                    ],
                ),
            ],
        ),
        # Used for CFI
        # TODO(b/283951987): Remove after full LTO support is removed in Soong
        feature(
            name = "android_full_lto",
            enabled = False,
            flag_sets = [
                flag_set(
                    actions = _actions.compile + _actions.link + _actions.assemble,
                    flag_groups = [
                        flag_group(
                            flags = ["-flto"],
                        ),
                    ],
                ),
            ],
        ),
    ]
    return features

def _make_flag_set(actions, flags, with_features = [], with_not_features = []):
    return flag_set(
        actions = actions,
        flag_groups = [
            flag_group(
                flags = flags,
            ),
        ],
        with_features = [
            with_feature_set(
                features = with_features,
                not_features = with_not_features,
            ),
        ],
    )

def _get_misc_sanitizer_features():
    return [
        # New sanitizers must imply this feature
        feature(
            name = "sanitizers_enabled",
            enabled = False,
        ),
    ]

# TODO(b/276756817): Restrict for VNDK when we have VNDK in Bazel
# TODO(b/276756319): Restrict for riscv64 when we have riscv64 in Bazel
# TODO(b/276932249): Restrict for Fuzzer when we have Fuzzer in Bazel
# TODO(b/276931992): Restrict for Asan when we have Asan in Bazel
# TODO(b/283951987): Switch to thin LTO when possible
def _get_cfi_features(target_arch, target_os):
    if target_os in [_oses.Windows, _oses.Darwin, _oses.LinuxMusl]:
        return []
    features = [
        feature(
            name = "android_cfi_cross_dso",
            enabled = True,
            requires = [feature_set(features = ["android_cfi"])],
            flag_sets = [
                _make_flag_set(
                    actions = _actions.c_and_cpp_compile + _actions.link,
                    flags = [_generated_sanitizer_constants.CfiCrossDsoFlag],
                    with_features = ["dynamic_executable"],
                    with_not_features = ["static_executable"],
                ),
            ],
        ),
    ]

    features.append(
        feature(
            name = "android_cfi",
            enabled = False,
            flag_sets = [
                _make_flag_set(
                    _actions.c_and_cpp_compile,
                    _generated_sanitizer_constants.CfiCFlags,
                ),
                _make_flag_set(
                    _actions.link,
                    _generated_sanitizer_constants.CfiLdFlags,
                ),
                _make_flag_set(
                    _actions.assemble,
                    _generated_sanitizer_constants.CfiAsFlags,
                ),
            ],
            implies = ["android_full_lto", "sanitizers_enabled"] + (
                ["arm_isa_thumb"] if target_arch == _arches.Arm else []
            ),
        ),
    )

    features.append(
        feature(
            name = "android_cfi_assembly_support",
            enabled = False,
            requires = [feature_set(features = ["android_cfi"])],
            flag_sets = [
                _make_flag_set(
                    _actions.c_and_cpp_compile,
                    [_generated_sanitizer_constants.CfiAssemblySupportFlag],
                ),
            ],
        ),
    )

    features.append(
        feature(
            name = "android_cfi_exports_map",
            enabled = False,
            requires = [feature_set(features = ["android_cfi"])],
            flag_sets = [
                _make_flag_set(
                    _actions.link,
                    [
                        _generated_config_constants.VersionScriptFlagPrefix +
                        _generated_sanitizer_constants.CfiExportsMapPath +
                        "/" +
                        _generated_sanitizer_constants.CfiExportsMapFilename,
                    ],
                ),
            ],
        ),
    )

    features.append(
        feature(
            name = "android_cfi_visibility_default",
            enabled = True,
            requires = [feature_set(features = ["android_cfi"])],
            flag_sets = [
                flag_set(
                    actions = _actions.c_and_cpp_compile,
                    flag_groups = [
                        flag_group(
                            flags = [
                                _generated_config_constants.VisibilityDefaultFlag,
                            ],
                        ),
                    ],
                    with_features = [
                        with_feature_set(
                            not_features = ["visibility_hidden"],
                        ),
                    ],
                ),
            ],
        ),
    )

    return features

def _get_visibiility_hidden_feature():
    return [
        feature(
            name = "visibility_hidden",
            enabled = False,
            flag_sets = [
                _make_flag_set(
                    _actions.c_and_cpp_compile,
                    [_generated_config_constants.VisibilityHiddenFlag],
                ),
            ],
        ),
    ]

def _sanitizer_flag_feature(name, actions, flags):
    return feature(
        name = name,
        enabled = True,
        flag_sets = [
            flag_set(
                actions = actions,
                flag_groups = [
                    flag_group(
                        flags = flags,
                    ),
                ],
                # Any new sanitizers that are enabled need to have a
                # with_feature_set added here.
                with_features = [
                    with_feature_set(
                        features = ["sanitizers_enabled"],
                    ),
                ],
            ),
        ],
    )

def _host_or_device_specific_sanitizer_feature(target_os):
    if is_os_device(target_os):
        return _sanitizer_flag_feature(
            "sanitizer_device_only_flags",
            _actions.compile,
            _generated_sanitizer_constants.DeviceOnlySanitizeFlags,
        )
    return _sanitizer_flag_feature(
        "sanitizer_host_only_flags",
        _actions.compile,
        _generated_sanitizer_constants.HostOnlySanitizeFlags,
    )

def _exclude_ubsan_rt_feature(path):
    if not path:
        return None
    return feature(
        name = "ubsan_exclude_rt",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = _actions.link,
                flag_groups = [
                    flag_group(
                        flags = ["-Wl,--exclude-libs=" + path.basename],
                    ),
                ],
            ),
        ],
    )

int_overflow_ignorelist_path = "build/soong/cc/config"
int_overflow_ignorelist_filename = "integer_overflow_blocklist.txt"

def _get_unsupported_integer_check_with_feature_sets(feature_check):
    integer_sanitizers = [
        "implicit-unsigned-integer-truncation",
        "implicit-signed-integer-truncation",
        "implicit-integer-sign-change",
        "integer-divide-by-zero",
        "signed-integer-overflow",
        "unsigned-integer-overflow",
        "implicit-integer-truncation",
        "implicit-integer-arithmetic-value-change",
        "integer",
        "integer_overflow",
    ]
    if feature_check in integer_sanitizers:
        integer_sanitizers.remove(feature_check)

    return [
        with_feature_set(
            features = ["ubsan_" + integer_sanitizer],
            not_features = ["ubsan_" + feature_check],
        )
        for integer_sanitizer in integer_sanitizers
    ]

# The key of this dict is the package path to the blocklist, while the value is
# its filename.
# TODO: b/286872909 - When the blocking bug is complete, put these in their
#                     respective packages
sanitizer_blocklist_dict = {
    "external/libavc": "libavc_blocklist.txt",
    "external/libaom": "libaom_blocklist.txt",
    "external/libvpx": "libvpx_blocklist.txt",
    "frameworks/base/libs/androidfw": "libandroidfw_blocklist.txt",
    "external/libhevc": "libhevc_blocklist.txt",
    "external/libexif": "libexif_blocklist.txt",
    "external/libopus": "libopus_blocklist.txt",
    "external/libmpeg2": "libmpeg2dec_blocklist.txt",
    "external/expat": "libexpat_blocklist.txt",
    "external/flac/src/libFLAC": "libFLAC_blocklist.txt",
    "system/extras/toolchain-extras": "libprofile_clang_extras_blocklist.txt",
}

def _get_sanitizer_blocklist_features():
    features = []
    for blocklist in sanitizer_blocklist_dict.items():
        # Format the blocklist name to be used in a feature name
        blocklist_feature_name_suffix = blocklist[1].lower().replace(".", "_")
        features.append(
            feature(
                name = "sanitizer_blocklist_" + blocklist_feature_name_suffix,
                enabled = False,
                requires = [
                    feature_set(features = ["sanitizers_enabled"]),
                ],
                flag_sets = [
                    flag_set(
                        actions = _actions.c_and_cpp_compile,
                        flag_groups = [
                            flag_group(
                                flags = [
                                    "%s%s/%s" % (
                                        _generated_sanitizer_constants.SanitizeIgnorelistPrefix,
                                        blocklist[0],
                                        blocklist[1],
                                    ),
                                ],
                            ),
                        ],
                    ),
                ],
            ),
        )
    return features

minimal_runtime_flags = [
    "-fsanitize-minimal-runtime",
    "-fno-sanitize-trap=integer,undefined",
    "-fno-sanitize-recover=integer,undefined",
]

def _get_ubsan_features(target_os, libclang_rt_ubsan_minimal):
    if target_os in [_oses.Windows, _oses.Darwin]:
        return []

    ALL_UBSAN_ACTIONS = _actions.compile + _actions.link + _actions.assemble

    ubsan_features = [
        feature(
            name = "ubsan_enabled",
            enabled = False,
            implies = ["sanitizers_enabled"],
        ),
    ]

    ubsan_features.append(
        feature(
            name = "ubsan_integer_overflow",
            enabled = False,
            flag_sets = [
                flag_set(
                    actions = ALL_UBSAN_ACTIONS,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fsanitize=unsigned-integer-overflow",
                                "-fsanitize=signed-integer-overflow",
                            ],
                        ),
                    ],
                ),
                flag_set(
                    actions = _actions.c_and_cpp_compile,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fsanitize-ignorelist=%s/%s" % (
                                    int_overflow_ignorelist_path,
                                    int_overflow_ignorelist_filename,
                                ),
                            ],
                        ),
                    ],
                ),
            ],
            implies = ["ubsan_minimal_runtime", "ubsan_enabled"],
        ),
    )

    ubsan_checks = [
        "alignment",
        "bool",
        "builtin",
        "bounds",
        "array-bounds",
        "local-bounds",
        "enum",
        "float-cast-overflow",
        "float-divide-by-zero",
        "function",
        "implicit-unsigned-integer-truncation",
        "implicit-signed-integer-truncation",
        "implicit-integer-sign-change",
        "integer-divide-by-zero",
        "nonnull-attribute",
        "null",
        "nullability-arg",
        "nullability-assign",
        "nullability-return",
        "objc-cast",
        "object-size",
        "pointer-overflow",
        "return",
        "returns-nonnull-attribute",
        "shift",
        "shift-base",
        "shift-exponent",
        "unsigned-shift-base",
        "signed-integer-overflow",
        "unreachable",
        "unsigned-integer-overflow",
        "vla-bound",
        "vptr",

        # check groups
        "undefined",
        "implicit-integer-truncation",
        "implicit-integer-arithmetic-value-change",
        "implicit-conversion",
        "integer",
        "nullability",
    ]
    ubsan_features += [
        feature(
            name = "ubsan_" + check_name,
            enabled = False,
            flag_sets = [
                flag_set(
                    actions = ALL_UBSAN_ACTIONS,
                    flag_groups = [
                        flag_group(
                            flags = ["-fsanitize=%s" % check_name],
                        ),
                    ],
                ),
            ],
            implies = ["ubsan_minimal_runtime", "ubsan_enabled"],
        )
        for check_name in ubsan_checks
    ]

    ubsan_features.append(
        feature(
            name = "ubsan_no_sanitize_link_runtime",
            enabled = target_os in [
                _oses.Android,
                _oses.LinuxBionic,
                _oses.LinuxMusl,
            ],
            flag_sets = [
                flag_set(
                    actions = _actions.link,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fno-sanitize-link-runtime",
                            ],
                        ),
                    ],
                    with_features = [
                        with_feature_set(
                            features = ["ubsan_enabled"],
                        ),
                    ],
                ),
            ],
        ),
    )

    # non-Bionic toolchain prebuilts are missing UBSan's vptr and function san.
    # Musl toolchain prebuilts have vptr and function sanitizers, but enabling
    # them implicitly enables RTTI which causes RTTI mismatch issues with
    # dependencies.
    ubsan_features.append(
        feature(
            name = "ubsan_disable_unsupported_non_bionic_checks",
            enabled = target_os not in [_oses.LinuxBionic, _oses.Android],
            flag_sets = [
                flag_set(
                    actions = _actions.c_and_cpp_compile,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fno-sanitize=function,vptr",
                            ],
                        ),
                    ],
                    with_features = [
                        with_feature_set(
                            features = ["ubsan_integer_overflow"],
                        ),
                    ] + [
                        with_feature_set(features = ["ubsan_" + check_name])
                        for check_name in ubsan_checks
                    ],
                ),
            ],
        ),
    )

    ubsan_features += [
        _host_or_device_specific_sanitizer_feature(target_os),
        _exclude_ubsan_rt_feature(libclang_rt_ubsan_minimal),
    ]

    ubsan_features.append(
        feature(
            name = "ubsan_minimal_runtime",
            enabled = False,
            flag_sets = [
                flag_set(
                    actions = _actions.c_and_cpp_compile,
                    flag_groups = [
                        flag_group(
                            flags = minimal_runtime_flags,
                        ),
                    ],
                ),
            ],
        ),
    )

    ubsan_features.append(
        feature(
            name = "ubsan_disable_unsupported_integer_checks",
            enabled = True,
            flag_sets = [
                # TODO(b/119329758): If this bug is fixed, this shouldn't be
                #                    needed anymore
                flag_set(
                    actions = _actions.c_and_cpp_compile,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fno-sanitize=implicit-integer-sign-change",
                            ],
                        ),
                    ],
                    with_features = _get_unsupported_integer_check_with_feature_sets(
                        "implicit-integer-sign-change",
                    ),
                ),
                # TODO(b/171275751): If this bug is fixed, this shouldn't be
                #                    needed anymore
                flag_set(
                    actions = _actions.c_and_cpp_compile,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fno-sanitize=unsigned-shift-base",
                            ],
                        ),
                    ],
                    with_features = _get_unsupported_integer_check_with_feature_sets(
                        "unsigned-shift-base",
                    ),
                ),
            ],
        ),
    )

    return ubsan_features

def _manual_binder_interface_feature():
    return feature(
        name = "do_not_check_manual_binder_interfaces",
        enabled = False,
        flag_sets = [
            flag_set(
                actions = _actions.compile,
                flag_groups = [
                    flag_group(
                        flags = [
                            "-DDO_NOT_CHECK_MANUAL_BINDER_INTERFACES",
                        ],
                    ),
                ],
            ),
        ],
    )

# Create the full list of features.
def get_features(
        ctx,
        builtin_include_dirs,
        crt_files):
    target_os = ctx.attr.target_os
    target_arch = ctx.attr.target_arch
    target_flags = ctx.attr.target_flags
    compile_only_flags = ctx.attr.compiler_flags
    linker_only_flags = ctx.attr.linker_flags
    deviceMaxPageSize = ctx.attr._product_variables[ProductVariablesInfo].DeviceMaxPageSizeSupported
    if deviceMaxPageSize and (target_arch == "arm" or target_arch == "arm64"):
        linker_only_flags = ctx.attr.linker_flags + \
                            ["-Wl,-z,max-page-size=" + deviceMaxPageSize]

    libclang_rt_builtin = ctx.file.libclang_rt_builtin
    libclang_rt_ubsan_minimal = ctx.file.libclang_rt_ubsan_minimal
    rtti_toggle = ctx.attr.rtti_toggle

    os_is_device = is_os_device(target_os)
    arch_is_64_bit = target_arch.endswith("64")

    # Aggregate all features in order.
    # Note that the feature-list helper methods called below may return empty
    # lists, depending on whether these features should be enabled. These are still
    # listed in the below stanza as-is to preserve ordering.
    features = [
        # Do not depend on Bazel's built-in legacy features and action configs:
        feature(name = "no_legacy_features"),

        # This must always come first, after no_legacy_features.
        _link_crtbegin(crt_files),

        # Explicitly depend on a subset of legacy configs:
        _get_legacy_features_begin(),

        # get_c_std_features must come before _compiler_flag_features and user
        # compile flags, as build targets may use copts/cflags to explicitly
        # change the -std version to overwrite the defaults or c{,pp}_std attribute
        # value.
        _get_c_std_features(),
        # Features tied to sdk version
        _get_sdk_version_features(os_is_device, target_arch),
        _compiler_flag_features(ctx, target_arch, target_os, target_flags + compile_only_flags),
        _rpath_features(target_os, arch_is_64_bit),
        # Optimization
        _get_thinlto_features(),
        # Sanitizers
        _get_cfi_features(target_arch, target_os),
        _get_ubsan_features(target_os, libclang_rt_ubsan_minimal),
        _get_sanitizer_blocklist_features(),
        _get_misc_sanitizer_features(),
        # Misc features
        _get_visibiility_hidden_feature(),
        # RTTI
        _rtti_features(rtti_toggle),
        _use_libcrt_feature(libclang_rt_builtin),
        # Shared compile/link flags that should also be part of the link actions.
        _linker_flag_feature("linker_target_flags", flags = target_flags),
        # Link-only flags.
        _linker_flag_feature("linker_flags", flags = linker_only_flags + _additional_linker_flags(os_is_device)),
        _archiver_flag_feature("additional_archiver_flags", flags = _additional_archiver_flags(target_os)),
        _undefined_symbols_feature(target_os),
        _dynamic_linker_flag_feature(target_os, arch_is_64_bit),
        _binary_linker_flag_feature("dynamic_executable", flags = _shared_binary_linker_flags(target_os)),
        # distinct from other static flags as it can be disabled separately
        _binary_linker_flag_feature("static_flag", flags = ["-static"], enabled = False),
        # default for executables is dynamic linking
        _binary_linker_flag_feature("static_executable", flags = _static_binary_linker_flags(os_is_device), enabled = False),
        _manual_binder_interface_feature(),
        _pack_dynamic_relocations_features(target_os),
        # System include directories features
        _toolchain_include_feature(system_includes = builtin_include_dirs),
        # Compiling stub.c sources to stub libraries
        _stub_library_feature(),
        _get_legacy_features_end(),
        # Flags that cannot be overridden must be at the end
        _get_no_override_compile_flags(target_os),
        # This must always come last.
        _link_crtend(crt_files),
    ]

    return _flatten([f for f in features if f != None])
