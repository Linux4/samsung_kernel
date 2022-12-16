
export PLATFORM_VERSION=9
export ANDROID_MAJOR_VERSION=p
export ARCH=arm64

make ARCH=arm64 exynos7870-j5y17lte_defconfig
make ARCH=arm64 -j64
