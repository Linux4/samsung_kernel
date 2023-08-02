This is what is needed, in order to build the cgvuart driver : 

1. 	edit bld.sh, and change the following exports:
	a. 	ROOT holds under it, "/android/ndk/" and "/android/sdk".
	b. 	CG_KERNEL_DIR holds under it, the kernel source tree
	c. 	Set ARCH=arm
	d. 	Set CROSS_COMPILE=<tool chain prefix>
		An example : $ export CROSS_COMPILE="/home/cellguide/android/froyo/prebuilt/linux-x86/toolchain/arm-eabi-4.3.1/bin/arm-eabi-"
2. 	Run bld.sh
