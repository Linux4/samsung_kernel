#!/bin/bash


export ANDROID_MAJOR_VERSION=o
export ARCH=arm64

make ARCH=arm64 j2corelte_01_defconfig
make ARCH=arm64 -j64