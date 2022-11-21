#!/bin/sh

export ROOT=/home/cellguide
export CG_KERNEL_DIR=/home/cellguide/android/kernel/Telechips/tcc8900_2010_06_22/kernel
export ARCH=arm
export CROSS_COMPILE=/home/cellguide/android/froyo/prebuilt/linux-x86/toolchain/arm-eabi-4.3.1/bin/arm-eabi-
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
echo
make -f Makefile $*
