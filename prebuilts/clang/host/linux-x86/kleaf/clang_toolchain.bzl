# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Defines a cc toolchain for kernel build, based on clang."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "CPP_TOOLCHAIN_TYPE")
load(
    "@kernel_toolchain_info//:dict.bzl",
    "VARS",
)
load(":architecture_constants.bzl", "SUPPORTED_ARCHITECTURES")
load(":clang_config.bzl", "clang_config")

def _clang_toolchain_internal(
        name,
        clang_version,
        target_cpu,
        target_os,
        clang_pkg,
        linker_files = None,
        sysroot_label = None,
        sysroot_path = None,
        ndk_triple = None,
        extra_compatible_with = None):
    """Defines a cc toolchain for kernel build, based on clang.

    Args:
        name: name of the toolchain
        clang_version: value of `CLANG_VERSION`, e.g. `r475365b`.
        target_cpu: CPU that the toolchain cross-compiles to
        target_os: OS that the toolchain cross-compiles to
        clang_pkg: Label to any target in the clang toolchain package.

            This is used as an anchor to locate other targets in the package.
            Name of the label is ignored.
        linker_files: Additional dependencies to the linker
        sysroot_label: Label to a list of files from sysroot
        sysroot_path: Path to sysroot
        ndk_triple: `NDK_TRIPLE`.
        extra_compatible_with: Extra `exec_compatible_with` / `target_compatible_with`.
    """

    if sysroot_path == None:
        sysroot_path = "/dev/null"

    sysroot_labels = []
    if sysroot_label != None:
        sysroot_labels.append(sysroot_label)

    if linker_files == None:
        linker_files = []

    if extra_compatible_with == None:
        extra_compatible_with = []

    clang_pkg = native.package_relative_label(clang_pkg)
    clang_includes = clang_pkg.relative(":includes")

    # Technically we can split the binaries into those for compiler, linker
    # etc., but since these binaries are usually updated together, it is okay
    # to use a superset here.
    clang_all_binaries = clang_pkg.relative(":binaries")

    # Individual binaries
    # From _setup_env.sh
    #  HOSTCC=clang
    #  HOSTCXX=clang++
    #  CC=clang
    #  LD=ld.lld
    #  AR=llvm-ar
    #  NM=llvm-nm
    #  OBJCOPY=llvm-objcopy
    #  OBJDUMP=llvm-objdump
    #  OBJSIZE=llvm-size
    #  READELF=llvm-readelf
    #  STRIP=llvm-strip
    #
    # Note: ld.lld does not recognize --target etc. from android.bzl,
    # so just use clang directly
    clang = clang_pkg.relative(":bin/clang")
    clang_plus_plus = clang_pkg.relative(":bin/clang++")
    ld = clang
    strip = clang_pkg.relative(":bin/llvm-strip")
    ar = clang_pkg.relative(":bin/llvm-ar")
    objcopy = clang_pkg.relative(":bin/llvm-objcopy")
    # cc_* rules doesn't seem to need nm, obj-dump, size, and readelf

    native.filegroup(
        name = name + "_compiler_files",
        srcs = [
            clang_all_binaries,
            clang_includes,
        ] + sysroot_labels,
    )

    native.filegroup(
        name = name + "_linker_files",
        srcs = [clang_all_binaries] + sysroot_labels + linker_files,
    )

    native.filegroup(
        name = name + "_all_files",
        srcs = [
            clang_all_binaries,
            name + "_compiler_files",
            name + "_linker_files",
        ],
    )

    clang_config(
        name = name + "_clang_config",
        clang_version = clang_version,
        sysroot = sysroot_path,
        target_cpu = target_cpu,
        target_os = target_os,
        ndk_triple = ndk_triple,
        toolchain_identifier = name + "_clang_id",
        clang = clang,
        ld = ld,
        clang_plus_plus = clang_plus_plus,
        strip = strip,
        ar = ar,
        objcopy = objcopy,
    )

    native.cc_toolchain(
        name = name + "_cc_toolchain",
        all_files = name + "_all_files",
        ar_files = clang_all_binaries,
        compiler_files = name + "_compiler_files",
        dwp_files = clang_all_binaries,
        linker_files = name + "_linker_files",
        objcopy_files = clang_all_binaries,
        strip_files = clang_all_binaries,
        supports_param_files = False,
        toolchain_config = name + "_clang_config",
        toolchain_identifier = name + "_clang_id",
    )

    native.toolchain(
        name = name,
        exec_compatible_with = [
            "@platforms//os:linux",
            "@platforms//cpu:x86_64",
        ] + extra_compatible_with,
        target_compatible_with = [
            "@platforms//os:{}".format(target_os),
            "@platforms//cpu:{}".format(target_cpu),
        ] + extra_compatible_with,
        toolchain = name + "_cc_toolchain",
        toolchain_type = CPP_TOOLCHAIN_TYPE,
    )

def clang_toolchain(
        name,
        clang_version,
        clang_pkg,
        target_cpu,
        target_os,
        extra_compatible_with = None):
    """Declare a clang toolchain for the given OS-architecture.

    The toolchain should be under `prebuilts/clang/host/linux-x86`.

    Args:
        name: name of the toolchain
        clang_version: nonconfigurable. version of the toolchain
        clang_pkg: Label to any target in the clang toolchain package.

            This is used as an anchor to locate other targets in the package.
            Name of the label is ignored.
        target_cpu: nonconfigurable. CPU of the toolchain
        target_os: nonconfigurable. OS of the toolchain
        extra_compatible_with: nonconfigurable. extra `exec_compatible_with` and `target_compatible_with`
    """

    if sorted(ARCH_CONFIG.keys()) != sorted(SUPPORTED_ARCHITECTURES):
        fail("FATAL: ARCH_CONFIG is not up-to-date with SUPPORTED_ARCHITECTURES!")

    extra_kwargs = ARCH_CONFIG[(target_os, target_cpu)]

    _clang_toolchain_internal(
        name = name,
        clang_version = clang_version,
        target_os = target_os,
        target_cpu = target_cpu,
        clang_pkg = clang_pkg,
        extra_compatible_with = extra_compatible_with,
        **extra_kwargs
    )

# Keys: (target_os, target_cpu)
# Values: arguments to clang_toolchain()
ARCH_CONFIG = {
    ("linux", "x86_64"): dict(
        linker_files = [
            # From _setup_env.sh, HOSTLDFLAGS
            Label("//prebuilts/kernel-build-tools:linux-x86-libs"),
        ],
        # From _setup_env.sh
        # sysroot_flags+="--sysroot=${ROOT_DIR}/build/kernel/build-tools/sysroot "
        sysroot_label = Label("//build/kernel:sysroot"),
        sysroot_path = "build/kernel/build-tools/sysroot",
    ),
    ("android", "arm64"): dict(
        ndk_triple = VARS.get("AARCH64_NDK_TRIPLE"),
        # From _setup_env.sh: when NDK triple is set,
        # --sysroot=${NDK_DIR}/toolchains/llvm/prebuilt/linux-x86_64/sysroot
        sysroot_label = "@prebuilt_ndk//:sysroot" if "AARCH64_NDK_TRIPLE" in VARS else None,
        sysroot_path = "external/prebuilt_ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot" if "AARCH64_NDK_TRIPLE" in VARS else None,
    ),
    ("android", "arm"): dict(
        ndk_triple = VARS.get("ARM_NDK_TRIPLE"),
        # From _setup_env.sh: when NDK triple is set,
        # --sysroot=${NDK_DIR}/toolchains/llvm/prebuilt/linux-x86_64/sysroot
        sysroot_label = "@prebuilt_ndk//:sysroot" if "ARM_NDK_TRIPLE" in VARS else None,
        sysroot_path = "external/prebuilt_ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot" if "AARCH64_NDK_TRIPLE" in VARS else None,
    ),
    ("android", "x86_64"): dict(
        ndk_triple = VARS.get("X86_64_NDK_TRIPLE"),
        # From _setup_env.sh: when NDK triple is set,
        # --sysroot=${NDK_DIR}/toolchains/llvm/prebuilt/linux-x86_64/sysroot
        sysroot_label = "@prebuilt_ndk//:sysroot" if "X86_64_NDK_TRIPLE" in VARS else None,
        sysroot_path = "external/prebuilt_ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot" if "X86_64_NDK_TRIPLE" in VARS else None,
    ),
    ("android", "i386"): dict(
        # i386 uses the same NDK_TRIPLE as x86_64
        ndk_triple = VARS.get("X86_64_NDK_TRIPLE"),
        # From _setup_env.sh: when NDK triple is set,
        # --sysroot=${NDK_DIR}/toolchains/llvm/prebuilt/linux-x86_64/sysroot
        sysroot_label = "@prebuilt_ndk//:sysroot" if "X86_64_NDK_TRIPLE" in VARS else None,
        sysroot_path = "external/prebuilt_ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot" if "X86_64_NDK_TRIPLE" in VARS else None,
    ),
    ("android", "riscv64"): dict(
        # TODO(b/271919464): We need NDK_TRIPLE for riscv
    ),
}
