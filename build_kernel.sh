#!/bin/bash

export PATH=$(pwd)/toolchain/clang/host/linux-x86/clang-r450784e/bin:$PATH
export PATH=$(pwd)/toolchain/build/kernel/build-tools/path/linux-x86/:$PATH
export HOSTCFLAGS="--sysroot=$(pwd)/toolchain/build/kernel/build-tools/sysroot -I$(pwd)/toolchain/prebuilts/kernel-build-tools/linux-x86/include"
export HOSTLDFLAGS="--sysroot=$(pwd)/toolchain/build/kernel/build-tools/sysroot  -Wl,-rpath,$(pwd)/toolchain/prebuilts/kernel-build-tools/linux-x86/lib64 -L $(pwd)/toolchain/prebuilts/kernel-build-tools/linux-x86/lib64 -fuse-ld=lld --rtlib=compiler-rt"

export DTC_FLAGS="-@"
export PLATFORM_VERSION=14
export LLVM=1
export DEPMOD=depmod
export ARCH=arm64
export TARGET_SOC=s5e3830
make exynos850-a14nsxx_defconfig
make
