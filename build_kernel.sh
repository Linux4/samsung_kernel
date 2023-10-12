
#!/bin/bash


export PLATFORM_VERSION=7
export ANDROID_MAJOR_VERSION=n
export ARCH=arm

make ARCH=arm j3xlteatt_00_defconfig
make ARCH=arm -j64
