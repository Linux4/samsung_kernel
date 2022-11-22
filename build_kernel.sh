#!/bin/bash

################################################################################
#
# Build Script
#
# Copyright Samsung Electronics(C), 2010
#
################################################################################

make pxa1088_cs053g_rev02_defconfig
make -j8
make modules
make INSTALL_MOD_PATH=../modules/ modules_install
