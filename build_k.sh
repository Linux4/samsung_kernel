##!/bin/bash

export ARCH=arm
export CROSS_COMPILE=/home/opensource.m/toolchains/arm-eabi-4.7/bin/arm-eabi-
export VARIANT_DEFCONFIG=msm8926-sec_ms01lte_skt_defconfig

make msm8926-sec_defconfig
make
