#!/bin/bash


export PLATFORM_VERSION=10
export ANDROID_MAJOR_VERSION=q
export ARCH=arm64

make ARCH=arm64 m30sdd_defconfig
make ARCH=arm64 -j64

