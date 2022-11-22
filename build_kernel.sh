#!/bin/sh

export CROSS_COMPILE=/opt/toolchains/arm-eabi-4.6/bin/arm-eabi-

make ARCH=arm -j8 ANDROID_MAJOR_VERSION=m klimtwifi_00_defconfig
make ARCH=arm -j8 ANDROID_MAJOR_VERSION=m

