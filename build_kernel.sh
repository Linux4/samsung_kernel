
#!/bin/bash

export PATH=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PATH
# export SEC_BUILD_OPTION_HW_REVISION=02

mkdir out

make -C $(pwd) O=$(pwd)/out ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- KCFLAGS=-mno-android VARIANT_DEFCONFIG=sdm660_sec_a9y18qlte_eur_open_defconfig sdm660_sec_defconfig SELINUX_DEFCONFIG=selinux_defconfig
make -j64 -C $(pwd) O=$(pwd)/out ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- KCFLAGS=-mno-android

cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image

