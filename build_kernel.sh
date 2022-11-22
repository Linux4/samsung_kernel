#!/bin/bash

export ARCH=arm
export CROSS_COMPILE=/home/opensource.m/toolchains/arm-eabi-4.7/bin/arm-eabi-

make msm8974_sec_defconfig VARIANT_DEFCONFIG=msm8974_sec_flte_skt_defconfig SELINUX_DEFCONFIG=selinux_defconfig
make
