#!/bin/bash

export ARCH=arm64
export PLATFORM_VERSION=8.1
export ANDROID_MAJOR_VERSION=OO81

make ARCH=arm64 mt6757-j7maxlte_defconfig
make ARCH=arm64 -j16
