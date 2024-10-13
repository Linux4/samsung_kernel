load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "action_config",
    "tool",
    "tool_path",
)
load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(
    ":cc_toolchain_constants.bzl",
    "arch_to_variants",
    "variant_constraints",
    "variant_name",
    "x86_64_host_toolchains",
    "x86_64_musl_host_toolchains",
    "x86_host_toolchains",
    "x86_musl_host_toolchains",
    _actions = "actions",
    _enabled_features = "enabled_features",
    _flags = "flags",
    _generated_config_constants = "generated_config_constants",
    _generated_sanitizer_constants = "generated_sanitizer_constants",
)
load(
    ":cc_toolchain_features.bzl",
    "get_features",
    "int_overflow_ignorelist_filename",
    "int_overflow_ignorelist_path",
    "sanitizer_blocklist_dict",
)
load("//build/bazel/platforms/arch/variants:constants.bzl", _arch_constants = "constants")

# Clang-specific configuration.
_ClangVersionInfo = provider(fields = ["clang_version", "includes"])

CLANG_TOOLS = [
    "llvm-ar",
    "llvm-nm",
    "llvm-objcopy",
    "llvm-readelf",
    "llvm-strip",
    "clang-tidy",
    "clang-tidy.sh",
    "clang-tidy.real",
]

CLANG_TEST_TOOLS = [
    "llvm-readelf",
    "llvm-nm",
]

def clang_tool_output_group(tool_name):
    return tool_name.replace("-", "_").replace(".", "_")

_libclang_rt_prebuilt_map = {
    "android_arm": "libclang_rt.builtins-arm-android.a",
    "android_arm64": "libclang_rt.builtins-aarch64-android.a",
    "android_x86": "libclang_rt.builtins-i686-android.a",
    "android_x86_64": "libclang_rt.builtins-x86_64-android.a",
    "linux_bionic_x86_64": "libclang_rt.builtins-x86_64-android.a",
    "linux_glibc_x86": "libclang_rt.builtins-i386.a",
    "linux_glibc_x86_64": "libclang_rt.builtins-x86_64.a",
    "linux_musl_x86": "i686-unknown-linux-musl/lib/linux/libclang_rt.builtins-i386.a",
    "linux_musl_x86_64": "x86_64-unknown-linux-musl/lib/linux/libclang_rt.builtins-x86_64.a",
}

_libclang_rt_ubsan_minimal_prebuilt_map = {
    "android_arm": "libclang_rt.ubsan_minimal-arm-android.a",
    "android_arm64": "libclang_rt.ubsan_minimal-aarch64-android.a",
    "android_x86": "libclang_rt.ubsan_minimal-i686-android.a",
    "android_x86_64": "libclang_rt.ubsan_minimal-x86_64-android.a",
    "linux_bionic_x86_64": "libclang_rt.ubsan_minimal-x86_64-android.a",
    "linux_glibc_x86": "libclang_rt.ubsan_minimal-i386.a",
    "linux_glibc_x86_64": "libclang_rt.ubsan_minimal-x86_64.a",
    "linux_musl_x86": "i686-unknown-linux-musl/lib/linux/libclang_rt.ubsan_minimal-i386.a",
    "linux_musl_x86_64": "x86_64-unknown-linux-musl/lib/linux/libclang_rt.ubsan_minimal-x86_64.a",
}

def _is_relative_path(path, root):
    path_parts = paths.normalize(path).split("/")
    root_parts = paths.normalize(root).split("/")
    for i in range(len(root_parts)):
        if path_parts[i] != root_parts[i]:
            return False
    return True

def _clang_version_info_impl(ctx):
    clang_version = ctx.attr.clang_version[BuildSettingInfo].value
    clang_version_directory = paths.join(ctx.label.package, clang_version)
    clang_short_version = ctx.attr.clang_short_version[BuildSettingInfo].value

    all_files = {}  # a set to do fast prebuilt lookups later
    output_groups = {
        "compiler_clang_includes": [],
        "compiler_binaries": [],
        "linker_binaries": [],
        "ar_files": [],
        "clang_test_tools": [],
    }

    for file in ctx.files.clang_files:
        if not _is_relative_path(file.short_path, clang_version_directory):
            continue
        file_path = paths.relativize(file.short_path, clang_version_directory)
        all_files[file_path] = file

        file_path_parts = file_path.split("/")
        if file_path_parts[:2] == ["lib", "clang"] and file_path_parts[4] == "include":
            output_groups["compiler_clang_includes"].append(file)  # /lib/clang/*/include/**
        if file_path_parts[0] == "bin" and len(file_path_parts) == 2:
            output_groups["linker_binaries"].append(file)  # /bin/*
            if file.basename.startswith("clang"):
                output_groups["compiler_binaries"].append(file)  # /bin/clang*
            if file.basename == "llvm-ar":
                output_groups["ar_files"].append(file)  # /bin/llvm-ar
            if file.basename in CLANG_TEST_TOOLS:
                output_groups["clang_test_tools"].append(file)
            if file.basename in CLANG_TOOLS:
                output_groups[clang_tool_output_group(file.basename)] = [file]

    libclang_rt_prefix = "lib/clang/%s/lib/linux" % clang_short_version
    for arch, path in _libclang_rt_prebuilt_map.items():
        file_path = paths.join(libclang_rt_prefix, path)
        if file_path in all_files:
            output_groups["libclang_rt_builtins_" + arch] = [all_files[file_path]]
        else:
            fail("could not find libclang_rt_builtin for `%s` at path `%s`" % (arch, file_path))
    for arch, path in _libclang_rt_ubsan_minimal_prebuilt_map.items():
        file_path = paths.join(libclang_rt_prefix, path)
        if file_path in all_files:
            output_groups["libclang_rt_ubsan_minimal_" + arch] = [all_files[file_path]]
        else:
            fail("could not find libclang_rt_ubsan_minimal for `%s` at path `%s`" % (arch, file_path))

    return [
        _ClangVersionInfo(
            clang_version = clang_version,
            includes = [
                paths.join(clang_version_directory, "lib", "clang", clang_short_version, "include"),
            ],
        ),
        OutputGroupInfo(
            **output_groups
        ),
    ]

clang_version_info = rule(
    implementation = _clang_version_info_impl,
    attrs = {
        "clang_version": attr.label(mandatory = True),
        "clang_short_version": attr.label(mandatory = True),
        "clang_files": attr.label_list(
            allow_files = True,
        ),
    },
)

def _tool_paths(clang_version_info):
    return [
        tool_path(
            name = "gcc",
            path = clang_version_info.clang_version + "/bin/clang",
        ),
        tool_path(
            name = "ld",
            path = clang_version_info.clang_version + "/bin/ld.lld",
        ),
        tool_path(
            name = "ar",
            path = clang_version_info.clang_version + "/bin/llvm-ar",
        ),
        tool_path(
            name = "cpp",
            path = "/bin/false",
        ),
        tool_path(
            name = "gcov",
            path = clang_version_info.clang_version + "/bin/llvm-profdata",
        ),
        tool_path(
            name = "llvm-cov",
            path = clang_version_info.clang_version + "/bin/llvm-cov",
        ),
        tool_path(
            name = "nm",
            path = clang_version_info.clang_version + "/bin/llvm-nm",
        ),
        tool_path(
            name = "objcopy",
            path = clang_version_info.clang_version + "/bin/llvm-objcopy",
        ),
        tool_path(
            name = "objdump",
            path = clang_version_info.clang_version + "/bin/llvm-objdump",
        ),
        # Soong has a wrapper around strip.
        # https://cs.android.com/android/platform/superproject/+/master:build/soong/cc/strip.go;l=62;drc=master
        # https://cs.android.com/android/platform/superproject/+/master:build/soong/cc/builder.go;l=991-1025;drc=master
        tool_path(
            name = "strip",
            path = clang_version_info.clang_version + "/bin/llvm-strip",
        ),
        tool_path(
            name = "clang++",
            path = clang_version_info.clang_version + "/bin/clang++",
        ),
    ]

# Set tools used for all actions in Android's C++ builds.
def _create_action_configs(tool_paths, target_os):
    action_configs = []

    tool_name_to_tool = {}
    for tool_path in tool_paths:
        tool_name_to_tool[tool_path.name] = tool(path = tool_path.path)

    # use clang for assembler actions
    # https://cs.android.com/android/_/android/platform/build/soong/+/a14b18fb31eada7b8b58ecd469691c20dcb371b3:cc/builder.go;l=616;drc=769a51cc6aa9402c1c55e978e72f528c26b6a48f
    for action_name in _actions.assemble:
        action_configs.append(action_config(
            action_name = action_name,
            enabled = True,
            tools = [tool_name_to_tool["gcc"]],
            implies = [
                "user_compile_flags",
                "compiler_input_flags",
                "compiler_output_flags",
            ],
        ))

    # use clang++ for compiling C++
    # https://cs.android.com/android/_/android/platform/build/soong/+/a14b18fb31eada7b8b58ecd469691c20dcb371b3:cc/builder.go;l=627;drc=769a51cc6aa9402c1c55e978e72f528c26b6a48f
    action_configs.append(action_config(
        action_name = _actions.cpp_compile,
        enabled = True,
        tools = [tool_name_to_tool["clang++"]],
        implies = [
            "user_compile_flags",
            "compiler_input_flags",
            "compiler_output_flags",
        ],
    ))

    # use clang for compiling C
    # https://cs.android.com/android/_/android/platform/build/soong/+/a14b18fb31eada7b8b58ecd469691c20dcb371b3:cc/builder.go;l=623;drc=769a51cc6aa9402c1c55e978e72f528c26b6a48f
    action_configs.append(action_config(
        action_name = _actions.c_compile,
        enabled = True,
        # this is clang, but needs to be called gcc for legacy reasons.
        # to avoid this, we need to set `no_legacy_features`: b/201257475
        # http://google3/third_party/bazel/src/main/java/com/google/devtools/build/lib/rules/cpp/CcModule.java;l=1106-1122;rcl=398974497
        # http://google3/third_party/bazel/src/main/java/com/google/devtools/build/lib/rules/cpp/CcModule.java;l=1185-1187;rcl=398974497
        tools = [tool_name_to_tool["gcc"]],
        implies = [
            "user_compile_flags",
            "compiler_input_flags",
            "compiler_output_flags",
        ],
    ))

    rpath_features = []
    if target_os not in ("android", "windows"):
        rpath_features.append("runtime_library_search_directories")

    # use clang++ for dynamic linking
    # https://cs.android.com/android/_/android/platform/build/soong/+/a14b18fb31eada7b8b58ecd469691c20dcb371b3:cc/builder.go;l=790;drc=769a51cc6aa9402c1c55e978e72f528c26b6a48f
    for action_name in [_actions.cpp_link_dynamic_library, _actions.cpp_link_nodeps_dynamic_library]:
        action_configs.append(action_config(
            action_name = action_name,
            enabled = True,
            tools = [tool_name_to_tool["clang++"]],
            implies = [
                "shared_flag",
                "linkstamps",
                "output_execpath_flags",
                "library_search_directories",
                "libraries_to_link",
                "user_link_flags",
                "linker_param_file",
            ] + rpath_features,
        ))

    # use clang++ for linking cc executables
    action_configs.append(action_config(
        action_name = _actions.cpp_link_executable,
        enabled = True,
        tools = [tool_name_to_tool["clang++"]],
        implies = [
            "linkstamps",
            "output_execpath_flags",
            "library_search_directories",
            "libraries_to_link",
            "user_link_flags",
            "linker_param_file",
        ] + rpath_features,
    ))

    # use llvm-ar for creating static archives
    action_configs.append(action_config(
        action_name = _actions.cpp_link_static_library,
        enabled = True,
        tools = [tool_name_to_tool["ar"]],
        implies = [
            "linker_param_file",
        ],
    ))

    # unused, but Bazel complains if there isn't an action config for strip
    action_configs.append(action_config(
        action_name = _actions.strip,
        enabled = True,
        tools = [tool_name_to_tool["strip"]],
        # This doesn't imply any feature, because Bazel currently mimics
        # Soong by running strip actions in a rule (stripped_shared_library).
    ))

    # use llvm-objcopy to remove addrsig sections from partially linked objects
    action_configs.append(action_config(
        action_name = "objcopy",
        enabled = True,
        tools = [tool_name_to_tool["objcopy"]],
    ))

    return action_configs

def _cc_toolchain_config_impl(ctx):
    clang_version_info = ctx.attr.clang_version[_ClangVersionInfo]
    tool_paths = _tool_paths(clang_version_info)

    action_configs = _create_action_configs(tool_paths, ctx.attr.target_os)

    builtin_include_dirs = []
    builtin_include_dirs.extend(clang_version_info.includes)

    # b/186035856: Do not add anything to this list.
    builtin_include_dirs.extend(_generated_config_constants.CommonGlobalIncludes)

    crt_files = struct(
        shared_library_crtbegin = ctx.file.shared_library_crtbegin,
        shared_library_crtend = ctx.file.shared_library_crtend,
        shared_binary_crtbegin = ctx.file.shared_binary_crtbegin,
        static_binary_crtbegin = ctx.file.static_binary_crtbegin,
        binary_crtend = ctx.file.binary_crtend,
    )

    features = get_features(
        ctx,
        builtin_include_dirs,
        crt_files,
    )

    # This is so that Bazel doesn't validate .d files against the set of headers
    # declared in BUILD files (Blueprint files don't contain that data). %workspace%/
    # will be replaced with the empty string by Bazel (essentially making it relative
    # to the build directory), so Bazel will skip the validations of all the headers
    # in .d files. For details please see CppCompileAction.validateInclude. We add
    # %workspace% after calling get_features so it won't be exposed as an -isystem flag.
    builtin_include_dirs.append("%workspace%/")

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        toolchain_identifier = ctx.attr.toolchain_identifier,
        tool_paths = tool_paths,
        features = features,
        action_configs = action_configs,
        cxx_builtin_include_directories = builtin_include_dirs,
        target_cpu = "_".join([ctx.attr.target_os, ctx.attr.target_arch]),
        # The attributes below are required by the constructor, but don't
        # affect actions at all.
        host_system_name = "__toolchain_host_system_name__",
        target_system_name = "__toolchain_target_system_name__",
        target_libc = "__toolchain_target_libc__",
        compiler = "__toolchain_compiler__",
        abi_version = "__toolchain_abi_version__",
        abi_libc_version = "__toolchain_abi_libc_version__",
    )

_cc_toolchain_config = rule(
    implementation = _cc_toolchain_config_impl,
    attrs = {
        "target_os": attr.string(mandatory = True),
        "target_arch": attr.string(mandatory = True),
        "toolchain_identifier": attr.string(mandatory = True),
        "clang_version": attr.label(mandatory = True, providers = [_ClangVersionInfo]),
        "target_flags": attr.string_list(default = []),
        "compiler_flags": attr.string_list(default = []),
        "linker_flags": attr.string_list(default = []),
        "libclang_rt_builtin": attr.label(allow_single_file = True),
        "libclang_rt_ubsan_minimal": attr.label(allow_single_file = True),
        # crtbegin and crtend libraries for compiling cc_library_shared and
        # cc_binary against the Bionic runtime
        "shared_library_crtbegin": attr.label(allow_single_file = True, cfg = "target"),
        "shared_library_crtend": attr.label(allow_single_file = True, cfg = "target"),
        "shared_binary_crtbegin": attr.label(allow_single_file = True, cfg = "target"),
        "static_binary_crtbegin": attr.label(allow_single_file = True, cfg = "target"),
        "binary_crtend": attr.label(allow_single_file = True, cfg = "target"),
        "rtti_toggle": attr.bool(default = True),
        "_auto_zero_initialize": attr.label(
            default = "//prebuilts/clang/host/linux-x86:auto_zero_initialize_env",
        ),
        "_auto_pattern_initialize": attr.label(
            default = "//prebuilts/clang/host/linux-x86:auto_pattern_initialize_env",
        ),
        "_auto_uninitialize": attr.label(
            default = "//prebuilts/clang/host/linux-x86:auto_uninitialize_env",
        ),
        "_use_ccache": attr.label(
            default = "//prebuilts/clang/host/linux-x86:use_ccache_env",
        ),
        "_llvm_next": attr.label(
            default = "//prebuilts/clang/host/linux-x86:llvm_next_env",
        ),
        "_allow_unknown_warning_option": attr.label(
            default = "//prebuilts/clang/host/linux-x86:allow_unknown_warning_option_env",
        ),
        "_product_variables": attr.label(
            default = "//build/bazel/product_config:product_vars",
        ),
        "_clang_default_debug_level": attr.label(
            default = "//prebuilts/clang/host/linux-x86:clang_default_debug_level",
        ),
    },
    provides = [CcToolchainConfigInfo],
)

# macro to expand feature flags for a toolchain
# we do not pass these directly to the toolchain so the order can
# be specified per toolchain
def expand_feature_flags(arch_variant, arch_variant_to_features = {}, flag_map = {}):
    flags = []
    features = _enabled_features(arch_variant, arch_variant_to_features)
    for feature in features:
        flags.extend(flag_map.get(feature, []))
    return flags

# Macro to set up both the toolchain and the config.
def android_cc_toolchain(
        name,
        target_os = None,
        target_arch = None,
        clang_version = None,
        gcc_toolchain = None,
        # If false, the crt version and "normal" version of this toolchain are identical.
        crt = None,
        libclang_rt_builtin = None,
        libclang_rt_ubsan_minimal = None,
        target_flags = [],
        compiler_flags = [],
        linker_flags = [],
        toolchain_identifier = None,
        rtti_toggle = True):
    extra_linker_paths = []
    libclang_rt_path = None
    if libclang_rt_builtin:
        libclang_rt_path = libclang_rt_builtin
        extra_linker_paths.append(libclang_rt_path)
    libclang_rt_ubsan_minimal_path = None
    if libclang_rt_ubsan_minimal:
        libclang_rt_ubsan_minimal_path = libclang_rt_ubsan_minimal
    if gcc_toolchain:
        gcc_toolchain_path = "//%s:tools" % gcc_toolchain
        extra_linker_paths.append(gcc_toolchain_path)
    extra_linker_paths.append("//%s:%s" % (
        _generated_sanitizer_constants.CfiExportsMapPath,
        _generated_sanitizer_constants.CfiExportsMapFilename,
    ))
    extra_linker_paths.append("//%s:%s" % (
        _generated_sanitizer_constants.CfiBlocklistPath,
        _generated_sanitizer_constants.CfiBlocklistFilename,
    ))

    common_toolchain_config = dict(
        [
            ("target_os", target_os),
            ("target_arch", target_arch),
            ("clang_version", clang_version),
            ("libclang_rt_builtin", libclang_rt_path),
            ("libclang_rt_ubsan_minimal", libclang_rt_ubsan_minimal_path),
            ("target_flags", target_flags),
            ("compiler_flags", compiler_flags),
            ("linker_flags", linker_flags),
            ("rtti_toggle", rtti_toggle),
        ],
    )

    _cc_toolchain_config(
        name = "%s_nocrt_config" % name,
        toolchain_identifier = toolchain_identifier + "_nocrt",
        **common_toolchain_config
    )

    # Create the filegroups needed for sandboxing toolchain inputs to C++ actions.
    native.filegroup(
        name = "%s_compiler_clang_includes" % name,
        srcs = [clang_version],
        output_group = "compiler_clang_includes",
    )

    native.filegroup(
        name = "%s_compiler_binaries" % name,
        srcs = [clang_version],
        output_group = "compiler_binaries",
    )

    native.filegroup(
        name = "%s_linker_binaries" % name,
        srcs = [clang_version],
        output_group = "linker_binaries",
    )

    native.filegroup(
        name = "%s_ar_files" % name,
        srcs = [clang_version],
        output_group = "ar_files",
    )

    native.filegroup(
        name = "%s_compiler_files" % name,
        srcs = [
            "%s_compiler_binaries" % name,
            "%s_compiler_clang_includes" % name,
            "@//%s:%s" % (int_overflow_ignorelist_path, int_overflow_ignorelist_filename),
        ] + [
            "@//%s:%s" % (blocklist[0], blocklist[1])
            for blocklist in sanitizer_blocklist_dict.items()
        ],
    )

    native.filegroup(
        name = "%s_linker_files" % name,
        srcs = ["%s_linker_binaries" % name] + extra_linker_paths,
    )

    native.filegroup(
        name = "%s_all_files" % name,
        srcs = [
            "%s_compiler_files" % name,
            "%s_linker_files" % name,
            "%s_ar_files" % name,
        ],
    )

    native.cc_toolchain(
        name = name + "_nocrt",
        all_files = "%s_all_files" % name,
        as_files = "//:empty",  # Note the "//" prefix, see comment above
        ar_files = "%s_ar_files" % name,
        compiler_files = "%s_compiler_files" % name,
        dwp_files = ":empty",
        linker_files = "%s_linker_files" % name,
        objcopy_files = ":empty",
        strip_files = ":empty",
        supports_param_files = 0,
        toolchain_config = ":%s_nocrt_config" % name,
        toolchain_identifier = toolchain_identifier + "_nocrt",
    )

    if crt:
        # Write the toolchain config.
        _cc_toolchain_config(
            name = "%s_config" % name,
            toolchain_identifier = toolchain_identifier,
            shared_library_crtbegin = crt.shared_library_crtbegin,
            shared_library_crtend = crt.shared_library_crtend,
            shared_binary_crtbegin = crt.shared_binary_crtbegin,
            static_binary_crtbegin = crt.static_binary_crtbegin,
            binary_crtend = crt.binary_crtend,
            **common_toolchain_config
        )

        native.filegroup(
            name = "%s_crt_libs" % name,
            srcs = [
                crt.shared_library_crtbegin,
                crt.shared_library_crtend,
                crt.shared_binary_crtbegin,
                crt.static_binary_crtbegin,
                crt.binary_crtend,
            ],
        )

        native.filegroup(
            name = "%s_linker_files_with_crt" % name,
            srcs = [
                "%s_linker_files" % name,
                "%s_crt_libs" % name,
            ],
        )

        # Create the actual cc_toolchain.
        # The dependency on //:empty is intentional; it's necessary so that Bazel
        # can parse .d files correctly (see the comment in $TOP/BUILD)
        native.cc_toolchain(
            name = name,
            all_files = "%s_all_files" % name,
            as_files = "//:empty",  # Note the "//" prefix, see comment above
            ar_files = "%s_ar_files" % name,
            compiler_files = "%s_compiler_files" % name,
            dwp_files = ":empty",
            linker_files = "%s_linker_files_with_crt" % name,
            objcopy_files = ":empty",
            strip_files = ":empty",
            supports_param_files = 0,
            toolchain_config = ":%s_config" % name,
            toolchain_identifier = toolchain_identifier,
            exec_transition_for_inputs = False,
        )
    else:
        _cc_toolchain_config(
            name = "%s_config" % name,
            toolchain_identifier = toolchain_identifier,
            **common_toolchain_config
        )

        native.alias(
            name = name,
            actual = name + "_nocrt",
        )

def _toolchain_name(arch, variant, nocrt = False):
    return "cc_toolchain_{arch}{variant}{nocrt}_def".format(
        arch = arch,
        variant = variant_name(variant),
        nocrt = "_nocrt" if nocrt else "",
    )

def toolchain_definition(arch, variant, nocrt = False):
    """Macro to create a toolchain with a standardized name

    The name used here must match that used in cc_register_toolchains.
    """
    name = _toolchain_name(arch, variant, nocrt)
    native.toolchain(
        name = name,
        exec_compatible_with = [
            "//build/bazel/platforms/arch:x86_64",
            "//build/bazel/platforms/os:linux",
        ],
        target_compatible_with = [
            "//build/bazel/platforms/arch:%s" % arch,
            "//build/bazel/platforms/os:android",
        ] + variant_constraints(
            variant,
            _arch_constants.AndroidArchToVariantToFeatures[arch],
        ),
        toolchain = ":cc_toolchain_{arch}{variant}{nocrt}".format(
            arch = arch,
            variant = variant_name(variant),
            nocrt = "_nocrt" if nocrt else "",
        ),
        toolchain_type = (
            ":nocrt_toolchain" if nocrt else "@bazel_tools//tools/cpp:toolchain_type"
        ),
    )

def cc_register_toolchains():
    """Register cc_toolchains for device and host platforms.

    This function ensures that generic (non-variant) device toolchains are
    registered last.
    """

    generic_toolchains = []
    arch_variant_toolchains = []
    cpu_variant_toolchains = []
    for arch, variants in arch_to_variants.items():
        for variant in variants:
            if not variant.arch_variant:
                generic_toolchains.append((arch, variant))
            elif not variant.cpu_variant:
                arch_variant_toolchains.append((arch, variant))
            else:
                cpu_variant_toolchains.append((arch, variant))

    target_toolchains = [
        _toolchain_name(arch, variant, nocrt = nocrt)
        for nocrt in [False, True]
        for arch, variant in (
            # Ordering is important here: more specific toolchains must be
            # registered before more generic toolchains
            cpu_variant_toolchains +
            arch_variant_toolchains +
            generic_toolchains
        )
    ]

    host_toolchains = [
        tc[0] + "_def"
        for tc in x86_64_host_toolchains + x86_host_toolchains +
                  x86_64_musl_host_toolchains + x86_musl_host_toolchains
    ]

    native.register_toolchains(*[
        "//prebuilts/clang/host/linux-x86:" + tc
        for tc in host_toolchains + target_toolchains
    ])
