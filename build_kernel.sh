#!/bin/bash

export ARCH=arm64
export CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
mkdir output

make -C $(pwd) O=output msm8976_sec_defconfig VARIANT_DEFCONFIG=msm8976_sec_gts28velte_eur_defconfig SELINUX_DEFCONFIG=selinux_defconfig
make -j64 -C $(pwd) O=output

cp output/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image