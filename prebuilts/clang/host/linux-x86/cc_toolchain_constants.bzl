load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load("@soong_injection//cc_toolchain:config_constants.bzl", _generated_config_constants = "constants")
load("@soong_injection//cc_toolchain:sanitizer_constants.bzl", _generated_sanitizer_constants = "constants")

# This file uses structs to organize and control the visibility of symbols.

# Handcrafted default flags.
flags = struct(
    # =============
    # Compiler flags
    # =============
    compiler_flags = [
    ],
    # ============
    # Linker flags
    # ============
    host_non_windows_dynamic_executable_linker_flags = [
        "-pie",
    ],
    bionic_linker_flags = [
        # These are the linker flags for OSes that use Bionic: LinuxBionic, Android
        "-nostdlib",
        "-Wl,--gc-sections",
    ],
    bionic_static_executable_linker_flags = [
        "-Bstatic",
    ],
    bionic_dynamic_executable_linker_flags = [
        "-pie",
        "-Bdynamic",
        "-Wl,-z,nocopyreloc",
    ],
    # ===========
    # Other flags
    # ===========
    non_darwin_archiver_flags = [
        "--format=gnu",
    ],
    non_external_defines = [
        # These defines should only apply to targets which are not under
        # @external/. This can be controlled by adding "-non_external_compiler_flags"
        # to the features list for external/ packages.
        # This corresponds to special-casing in Soong (see "external/" in build/soong/cc/compiler.go).
        "-DANDROID_STRICT",
    ],
)

# Generated flags dumped from Soong's cc toolchain code.
generated_config_constants = _generated_config_constants
generated_sanitizer_constants = _generated_sanitizer_constants

# The set of C and C++ actions used in the Android build. There are other types
# of actions available in ACTION_NAMES, but those are not used in
# Android yet.
actions = struct(
    compile = [
        ACTION_NAMES.c_compile,
        ACTION_NAMES.cpp_compile,
        ACTION_NAMES.assemble,
        ACTION_NAMES.preprocess_assemble,
    ],
    c_and_cpp_compile = [
        ACTION_NAMES.c_compile,
        ACTION_NAMES.cpp_compile,
    ],
    c_compile = ACTION_NAMES.c_compile,
    cpp_compile = ACTION_NAMES.cpp_compile,
    # Assembler actions for .s and .S files.
    assemble = [
        ACTION_NAMES.assemble,
        ACTION_NAMES.preprocess_assemble,
    ],
    # Link actions
    link = [
        ACTION_NAMES.cpp_link_executable,
        ACTION_NAMES.cpp_link_dynamic_library,
        ACTION_NAMES.cpp_link_nodeps_dynamic_library,
    ],
    # Differentiate archive actions from link actions
    archive = [
        ACTION_NAMES.cpp_link_static_library,
    ],
    cpp_link_dynamic_library = ACTION_NAMES.cpp_link_dynamic_library,
    cpp_link_nodeps_dynamic_library = ACTION_NAMES.cpp_link_nodeps_dynamic_library,
    cpp_link_static_library = ACTION_NAMES.cpp_link_static_library,
    cpp_link_executable = ACTION_NAMES.cpp_link_executable,
    strip = ACTION_NAMES.strip,
)

bionic_crt = struct(
    # crtbegin and crtend libraries for compiling cc_library_shared and
    # cc_binary against the Bionic runtime
    shared_library_crtbegin = "//bionic/libc:crtbegin_so",
    shared_library_crtend = "//bionic/libc:crtend_so",
    shared_binary_crtbegin = "//bionic/libc:crtbegin_dynamic",
    static_binary_crtbegin = "//bionic/libc:crtbegin_static",
    binary_crtend = "//bionic/libc:crtend_android",
)

musl_crt = struct(
    # crtbegin and crtend libraries for compiling cc_library_shared and
    # cc_binary against Musl libc
    shared_library_crtbegin = "//external/musl:libc_musl_crtbegin_so",
    shared_library_crtend = "//external/musl:libc_musl_crtend_so",
    shared_binary_crtbegin = "//external/musl:libc_musl_crtbegin_dynamic",
    static_binary_crtbegin = "//external/musl:libc_musl_crtbegin_static",
    binary_crtend = "//external/musl:libc_musl_crtend",
)

default_cpp_std_version = generated_config_constants.CppStdVersion
experimental_cpp_std_version = generated_config_constants.ExperimentalCppStdVersion
default_cpp_std_version_no_gnu = generated_config_constants.CppStdVersion.replace("gnu", "c")
experimental_cpp_std_version_no_gnu = generated_config_constants.ExperimentalCppStdVersion.replace("gnu", "c")
_cpp_std_versions = {
    "gnu++98": True,
    "gnu++11": True,
    "gnu++17": True,
    "gnu++20": True,
    "gnu++2a": True,
    "gnu++2b": True,
    "c++98": True,
    "c++11": True,
    "c++17": True,
    "c++2a": True,
}
_cpp_std_versions[default_cpp_std_version] = True
_cpp_std_versions[experimental_cpp_std_version] = True
_cpp_std_versions[default_cpp_std_version_no_gnu] = True
_cpp_std_versions[experimental_cpp_std_version_no_gnu] = True

cpp_std_versions = [k for k in _cpp_std_versions.keys()]

default_c_std_version = generated_config_constants.CStdVersion
experimental_c_std_version = generated_config_constants.ExperimentalCStdVersion
default_c_std_version_no_gnu = generated_config_constants.CStdVersion.replace("gnu", "c")
experimental_c_std_version_no_gnu = generated_config_constants.ExperimentalCStdVersion.replace("gnu", "c")

_c_std_versions = {
    "gnu99": True,
    "gnu11": True,
    "gnu17": True,
    "c99": True,
    "c11": True,
    "c17": True,
}
_c_std_versions[default_c_std_version] = True
_c_std_versions[experimental_c_std_version] = True
_c_std_versions[default_c_std_version_no_gnu] = True
_c_std_versions[experimental_c_std_version_no_gnu] = True

c_std_versions = [k for k in _c_std_versions.keys()]

# Added by linker.go for non-bionic, non-musl, non-windows toolchains.
# Should be added to host builds to match the default behavior of device builds.
device_compatibility_flags_non_windows = [
    "-ldl",
    "-lpthread",
    "-lm",
]

# Added by linker.go for non-bionic, non-musl, non-darwin toolchains.
# Should be added to host builds to match the default behavior of device builds.
device_compatibility_flags_non_darwin = ["-lrt"]

arches = struct(
    Arm = "arm",
    Arm64 = "arm64",
    X86 = "x86",
    X86_64 = "x86_64",
)

oses = struct(
    Android = "android",
    LinuxGlibc = "linux_glibc",
    LinuxBionic = "linux_bionic",
    LinuxMusl = "linux_musl",
    Darwin = "darwin",
    Windows = "windows",
)

def _variant_combinations(arch_variants = {}, cpu_variants = {}):
    combinations = []
    for arch in arch_variants:
        if "" not in cpu_variants:
            combinations.append(struct(arch_variant = arch, cpu_variant = ""))
        for cpu in cpu_variants:
            combinations.append(struct(arch_variant = arch, cpu_variant = cpu))
    return combinations

arch_to_variants = {
    arches.Arm: _variant_combinations(arch_variants = generated_config_constants.ArmArchVariantCflags, cpu_variants = generated_config_constants.ArmCpuVariantCflags),
    arches.Arm64: _variant_combinations(arch_variants = generated_config_constants.Arm64ArchVariantCflags, cpu_variants = generated_config_constants.Arm64CpuVariantCflags),
    arches.X86: _variant_combinations(arch_variants = generated_config_constants.X86ArchVariantCflags),
    arches.X86_64: _variant_combinations(arch_variants = generated_config_constants.X86_64ArchVariantCflags),
}

# enabled_features returns a list of enabled features for the given arch variant, defaults to empty list
def enabled_features(arch_variant, arch_variant_to_features = {}):
    if arch_variant == None:
        arch_variant = ""
    return arch_variant_to_features.get(arch_variant, [])

# variant_name creates a name based on a variant struct with arch_variant and cpu_variant
def variant_name(variant):
    ret = ""
    if variant.arch_variant:
        ret += "_" + variant.arch_variant
    if variant.cpu_variant:
        ret += "_" + variant.cpu_variant
    return ret

# variant_constraints gets constraints based on variant struct and arch_variant_features
def variant_constraints(variant, arch_variant_features = {}):
    ret = []
    if variant.arch_variant:
        ret.append("//build/bazel/platforms/arch/variants:" + variant.arch_variant)
    if variant.cpu_variant:
        ret.append("//build/bazel/platforms/arch/variants:" + variant.cpu_variant)
    features = enabled_features(variant.arch_variant, arch_variant_features)
    for feature in features:
        ret.append("//build/bazel/platforms/arch/variants:" + feature)
    return ret

x86_64_host_toolchains = [
    ("cc_toolchain_x86_64_linux_host", "@bazel_tools//tools/cpp:toolchain_type"),
    ("cc_toolchain_x86_64_linux_host_nocrt", "nocrt_toolchain"),
]
x86_host_toolchains = [
    ("cc_toolchain_x86_linux_host", "@bazel_tools//tools/cpp:toolchain_type"),
    ("cc_toolchain_x86_linux_host_nocrt", "nocrt_toolchain"),
]

x86_64_musl_host_toolchains = [
    ("cc_toolchain_x86_64_linux_musl_host", "@bazel_tools//tools/cpp:toolchain_type"),
    ("cc_toolchain_x86_64_linux_musl_host_nocrt", "nocrt_toolchain"),
]

x86_musl_host_toolchains = [
    ("cc_toolchain_x86_linux_musl_host", "@bazel_tools//tools/cpp:toolchain_type"),
    ("cc_toolchain_x86_linux_musl_host_nocrt", "nocrt_toolchain"),
]

libclang_rt_prebuilt_map = {
    "//build/bazel/platforms/os_arch:android_arm": "//prebuilts/clang/host/linux-x86:libclang_rt_builtins_android_arm",
    "//build/bazel/platforms/os_arch:android_arm64": "//prebuilts/clang/host/linux-x86:libclang_rt_builtins_android_arm64",
    "//build/bazel/platforms/os_arch:android_x86": "//prebuilts/clang/host/linux-x86:libclang_rt_builtins_android_x86",
    "//build/bazel/platforms/os_arch:android_x86_64": "//prebuilts/clang/host/linux-x86:libclang_rt_builtins_android_x86_64",
    "//build/bazel/platforms/os_arch:linux_bionic_x86_64": "//prebuilts/clang/host/linux-x86:libclang_rt_builtins_linux_bionic_x86_64",
    "//build/bazel/platforms/os_arch:linux_glibc_x86": "//prebuilts/clang/host/linux-x86:libclang_rt_builtins_linux_glibc_x86",
    "//build/bazel/platforms/os_arch:linux_glibc_x86_64": "//prebuilts/clang/host/linux-x86:libclang_rt_builtins_linux_glibc_x86_64",
    "//build/bazel/platforms/os_arch:linux_musl_x86": "//prebuilts/clang/host/linux-x86:libclang_rt_builtins_linux_musl_x86",
    "//build/bazel/platforms/os_arch:linux_musl_x86_64": "//prebuilts/clang/host/linux-x86:libclang_rt_builtins_linux_musl_x86_64",
    "//conditions:default": None,
}

libclang_ubsan_minimal_rt_prebuilt_map = {
    "//build/bazel/platforms/os_arch:android_arm": "//prebuilts/clang/host/linux-x86:libclang_rt_ubsan_minimal_android_arm",
    "//build/bazel/platforms/os_arch:android_arm64": "//prebuilts/clang/host/linux-x86:libclang_rt_ubsan_minimal_android_arm64",
    "//build/bazel/platforms/os_arch:android_x86": "//prebuilts/clang/host/linux-x86:libclang_rt_ubsan_minimal_android_x86",
    "//build/bazel/platforms/os_arch:android_x86_64": "//prebuilts/clang/host/linux-x86:libclang_rt_ubsan_minimal_android_x86_64",
    "//build/bazel/platforms/os_arch:linux_bionic_x86_64": "//prebuilts/clang/host/linux-x86:libclang_rt_ubsan_minimal_linux_bionic_x86_64",
    "//build/bazel/platforms/os_arch:linux_glibc_x86": "//prebuilts/clang/host/linux-x86:libclang_rt_ubsan_minimal_linux_glibc_x86",
    "//build/bazel/platforms/os_arch:linux_glibc_x86_64": "//prebuilts/clang/host/linux-x86:libclang_rt_ubsan_minimal_linux_glibc_x86_64",
    "//build/bazel/platforms/os_arch:linux_musl_x86": "//prebuilts/clang/host/linux-x86:libclang_rt_ubsan_minimal_linux_musl_x86",
    "//build/bazel/platforms/os_arch:linux_musl_x86_64": "//prebuilts/clang/host/linux-x86:libclang_rt_ubsan_minimal_linux_musl_x86_64",
    "//conditions:default": None,
}
