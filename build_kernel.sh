#!/bin/bash

export ARCH=arm
export CROSS_COMPILE=../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-

mkdir output

make -C $(pwd) O=output ARCH=arm CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi- msm8916_sec_defconfig VARIANT_DEFCONFIG=msm8916_sec_j3lte_vzw_defconfig SELINUX_DEFCONFIG=selinux_defconfig
make -C $(pwd) O=output ARCH=arm CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-

cp output/arch/arm/boot/zImage $(pwd)/arch/arm/boot/zImage
