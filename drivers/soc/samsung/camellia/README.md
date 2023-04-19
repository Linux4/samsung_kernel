

[![CircleCI](https://circleci.sec.samsung.net/gh/Camellia/ree-camellia-driver.svg?style=svg)](https://circleci.sec.samsung.net/gh/Camellia/ree-camellia-driver)

# REE Camellia Device Driver

This repository provides sources for the REE Camellia Device Driver.
REE Camellia Device Driver is responsible for
- providing device file interface for Camellia REE to SEE communication via mailbox drivers
- managing shared memory between REE and SEE.


## Build REE Camellia Device Driver (Only for `Olympus` device)
To build only module with android kernel codes without building kernel, use:
```
ANDROID_DIR="<PATH_TO_REE_PRODUCT_REPO>" \
PATH=$PATH:$ANDROID_DIR/prebuilts-master/clang/host/linux-x86/clang-r383902/bin \
MODULE_BUILD_CONFIG="CC=clang NM=llvm-nm LD=ld.lld EXTRA_CFLAGS=-I$ANDROID_DIR/kernel" \
REE_KERNELDIR=$ANDROID_DIR/KERNEL_OBJ/eng/kernel \
ARCH=arm64 \
CROSS_COMPILE=aarch64-linux-android- \
CLANG_TRIPLE=aarch64-linux-gnu- \
./camellia_build.sh -m
```

To build only module with android kernel codes for emulator without building kernel, use:
```
ANDROID_DIR="<PATH_TO_AOSP_KERNEL>" \
PATH=$ANDROID_DIR/prebuilts-master/clang/host/linux-x86/clang-r416183/bin:$PATH \
MODULE_BUILD_CONFIG="CC=clang NM=llvm-nm LD=ld.lld EXTRA_CFLAGS=-I$ANDROID_DIR/common" \
REE_KERNELDIR=$ANDROID_DIR/out/android11-5.4/common \
ARCH=x86_64 \
CROSS_COMPILE=x86_64-linux-android- \
CLANG_TRIPLE=x86_64-linux-gnu- \
./camellia_build.sh -m
```

## camellia_build.sh script
```
./camellia_build.sh -h
Usage: camellia_build.sh [options]
options:
	-k	build linux kernel, pointed by REE_KERNELDIR environment variable
	-m	build Camellia ree kernel module
	-t	Use this option together with -k, to enable kernel build with
		KUnit support, or with -m, to enable Camellia KUnit test module
		build.
	-d	deploy Camellia kernel modules to vm via ssh
	-c	perform make clean operation
	-h	display this help message
```

## Fix codes according to Coding Standard
Sources in this repository are for a device driver, so, they should comply with Linux Kernel Coding standard.
The following command can change codes according to the standard automatically,
```
$ ./formmater.sh
```
Note that `astyle` should be installed before executing `formatter.sh`.
You can install it by using the instruction below:
```
sudo apt install astyle
```
Note that you should check all coding styles manually by using `checkpatch.pl`.
For example,
```
 $ ~/kernel/scripts/checkpatch.pl --file *.c *.h
```