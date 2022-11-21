#!/bin/bash

export CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-

mkdir out
export ARCH=arm64
make -C $(pwd) O=$(pwd)/out KCFLAGS=-mno-android gts4llte_eur_open_defconfig
make -j64 -C $(pwd) O=$(pwd)/out KCFLAGS=-mno-android

cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image

