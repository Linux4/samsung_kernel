#!/bin/bash
export CROSS_COMPILE=../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-
mkdir out
chmod -R 777 out

make -C $(pwd) O=out -j32 ARCH=arm CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi- VARIANT_DEFCONFIG=msm8952_sec_c5lte_chn_hk_defconfig msm8952_sec_defconfig SELINUX_DEFCONFIG=selinux_defconfig
make -C $(pwd) O=out -j32 ARCH=arm CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-
			
cp out/arch/arm/boot/Image $(pwd)/arch/arm/boot/Image
cp out/arch/arm/boot/zImage $(pwd)/arch/arm/boot/zImage