#!/bin/bash


export PLATFORM_VERSION=7
export ANDROID_MAJOR_VERSION=n
export ARCH=arm64

make ARCH=arm64 zerolte_02_defconfig
make ARCH=arm64 -j64
