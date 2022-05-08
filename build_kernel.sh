export PLATFORM_VERSION=12
export ARCH=arm64

make ARCH=arm64 exynos9820-m62xx_defconfig
make ARCH=arm64 -j64
