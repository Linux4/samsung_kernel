#!/bin/bash

export CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androidkernel-
export CC=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androidkernel-gcc
export ARCH=arm

export KCFLAGS=-w
export CONFIG_SECTION_MISMATCH_WARN_ONLY=y

make -C $(pwd) O=$(pwd)/out KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y a02_defconfig
make -C $(pwd) O=$(pwd)/out KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y -j16

cp out/arch/arm/boot/zImage $(pwd)/arch/arm/boot/zImage