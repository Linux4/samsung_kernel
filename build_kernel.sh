#!/bin/bash

export ARCH=arm
export PATH=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin:$PATH

mkdir out

make -C $(pwd) O=out CROSS_COMPILE=arm-linux-androideabi- j4primelte_sea_open_defconfig
make -j64 -C $(pwd) O=out CROSS_COMPILE=arm-linux-androideabi-
 
cp out/arch/arm/boot/zImage $(pwd)/arch/arm/boot/zImage
