#!/bin/bash


export CROSS_COMPILE=../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.7/bin/arm-eabi-
export ARCH=arm

make goyavewifi_defconfig
make -j



