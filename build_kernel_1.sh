export ARCH=arm64
export CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-

make -C $(pwd) O=$(pwd)/out ARCH=arm64 KCFLAGS=-mno-android a8sqlte_kor_single_defconfig
make -C $(pwd) O=$(pwd)/out ARCH=arm64 KCFLAGS=-mno-android

