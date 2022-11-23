export PLATFORM_VERSION=12
export ARCH=arm64

make ARCH=arm64 f62_swa_ins_defconfig
make ARCH=arm64 -j64