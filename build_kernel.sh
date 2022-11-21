
#!/bin/bash

export PATH=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PATH

mkdir out

make -C $(pwd) O=$(pwd)/out ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- KCFLAGS=-mno-android sdm660-perf_defconfig
make -C $(pwd) O=$(pwd)/out ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- KCFLAGS=-mno-android

cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image

