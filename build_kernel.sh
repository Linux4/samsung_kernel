#!/bin/bash

################################################################################
#
# Build Script
#
# Copyright Samsung Electronics(C), 2010
#
################################################################################

################################################################################
# Common Path
################################################################################

export CROSS_COMPILE=/opt/toolchains/arm-eabi-4.7/bin/arm-eabi-
export ARCH=arm
mkdir output
make -C $(pwd) O=output msm8916_sec_defconfig VARIANT_DEFCONFIG=msm8916_sec_heat_eur_defconfig SELINUX_DEFCONFIG=selinux_defconfig TIMA_DEFCONFIG=tima8916_defconfig 
make -C $(pwd) O=output 
