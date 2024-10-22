#!/bin/bash

cd kernel
python kernel_device_modules-6.1/scripts/gen_build_config.py --kernel-defconfig mediatek-bazel_defconfig --kernel-defconfig-overlays "" --kernel-build-config-overlays "" -m user -o ../out/target/product/gts10uwifi/obj/KERNEL_OBJ/build.config

export DEVICE_MODULES_DIR="kernel_device_modules-6.1"
export BUILD_CONFIG="../out/target/product/gts10uwifi/obj/KERNEL_OBJ/build.config"
export OUT_DIR="../out/target/product/gts10uwifi/obj/KLEAF_OBJ"
export DIST_DIR="../out/target/product/gts10uwifi/obj/KLEAF_OBJ/dist"
export DEFCONFIG_OVERLAYS=""
export PROJECT="mgk_64_k61"
export MODE="user"

./kernel_device_modules-6.1/build.sh
