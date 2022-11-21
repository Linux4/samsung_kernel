
#!/bin/bash


export PLATFORM_VERSION=8.1
export ANDROID_MAJOR_VERSION=o
export ARCH=arm64

make ARCH=arm64 j7xelte_eur_open_defconfig
make ARCH=arm64 -j64


