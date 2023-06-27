#!/bin/bash

#export ARCH=arm
#export CROSS_COMPILE=/opt/toolchains/arm-eabi-4.7/bin/arm-eabi-

#make ARCH=arm msm8226-sec_defconfig VARIANT_DEFCONFIG=msm8926-#sec_ms01lte_ktt_defconfig
#make ARCH=arm

export ARCH=arm
export CROSS_COMPILE=../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.7/bin/arm-eabi-

mkdir output

make -C $(pwd) O=output ARCH=arm CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.7/bin/arm-eabi- msm8226-sec_defconfig VARIANT_DEFCONFIG=msm8926-sec_ms01lte_ktt_defconfig SELINUX_DEFCONFIG=selinux_defconfig
make -C $(pwd) O=output ARCH=arm CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.7/bin/arm-eabi-

cp output/arch/arm/boot/zImage $(pwd)/arch/arm/boot/zImage

