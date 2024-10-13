#!/bin/bash

USAGE="Usage: camellia_build.sh [options]
options:
	-k	build linux kernel, pointed by REE_KERNELDIR environment variable
	-m	build camellia ree kernel module
	-t	Use this option together with -k, to enable kernel build with
		KUnit support, or with -m, to enable Camelia KUnit test module
		build.
	-d	deploy camellia kernel modules to vm via ssh
	-c	perform make clean operation
	-h	display this help message

Note:
	1. To build the kernel, set REE_KERNELDIR environment variable to
	kernel tree root directory
	2. To deploy camellia module, set REE_VMPASSWD environment variable
	to virtual machine root password"

check_is_set() {
	if [ -z ${!1+x} ]; then
		echo "$1 not set"
		exit 1
	fi
}

build_kernel() {
	check_is_set REE_KERNELDIR
 	echo "Build kernel in: $REE_KERNELDIR ..."
 	cd $REE_KERNELDIR && \
 	sshpass -p $REE_VMPASSWD ssh -p 22222 root@localhost "lsmod" > ree_vm_lsmod.txt && \
 	make -C $REE_KERNELDIR defconfig && \
 	make -C $REE_KERNELDIR LSMOD="ree_vm_lsmod.txt" localmodconfig && \
	{ if [ -z ${!1+x} ]; then ./scripts/config --set-val $1 y; make -C $REE_KERNELDIR olddefconfig; fi; } && \
	make $CONFIG_OPT -j4
 	rm ree_vm_lsmod.txt

}

build_ree_modules() {
	check_is_set REE_KERNELDIR
	echo $1

	CONFIG_OPT=$1
	cd "${0%/*}"
	echo 'Build camellia ree modules...'
	make $CONFIG_OPT -C $REE_KERNELDIR M=`pwd` modules
}

clean_ree_modules() {
	check_is_set REE_KERNELDIR

	cd "${0%/*}"
	echo 'Make clean...'
	make -C $REE_KERNELDIR M=`pwd` clean
}

deploy_ree_modules() {
	check_is_set REE_VMPASSWD

	echo Deploy modules: *.ko
	cd "${0%/*}"
	sshpass -p $REE_VMPASSWD scp -P 22222 *.ko root@localhost:~
}

if [ "$#" -eq 0 ]; then
	echo "$USAGE"
	exit 0
fi

while getopts ":kmstdhc" arg; do
	case $arg in
		k)
			BUILD_KERNEL=1
			echo "BUILD_KERNEL"
			;;
		m)
			BUILD_MODULE=1
			MODULE_BUILD_CONFIG="${MODULE_BUILD_CONFIG} CONFIG_CAMELLIA_IWC=m"
			echo BUILD_MODULE
			echo $MODULE_BUILD_CONFIG
			;;
		s)
			BUILD_MODULE=1
			MODULE_BUILD_CONFIG="${MODULE_BUILD_CONFIG} CONFIG_EXYNOS_SSP_STUB=m CONFIG_CAMELLIA_IWC=m"
			echo BUILD_MODULE
			echo $MODULE_BUILD_CONFIG
			;;
		t)
			KERNEL_BUILD_CONFIG='CONFIG_KUNIT'
			MODULE_BUILD_CONFIG="${MODULE_BUILD_CONFIG} CONFIG_CAMELLIA_KUNIT_TESTS=m"
			echo MODE_TEST
			echo $MODULE_BUILD_CONFIG
			;;
		d)
			DEPLOY_MODULES=1
			echo DEPLOY_MODULES
			;;
		h)
			echo "$USAGE"
			exit 1
			;;

		c)
			CLEAN_MODULES=1
			echo "CLEAN_MODULES"
			;;
		*)
			echo "ERROR: Invalid option $OPTARG"
			echo "$USAGE"
			exit 1
			;;
	esac
done

if [[ $BUILD_KERNEL == 1 ]]; then
	build_kernel $KERNEL_BUILD_CONFIG || \
		{ echo "ERROR: Building kernel failed, abort."; exit 1; }
fi

if [[ $CLEAN_MODULES == 1 ]]; then
	clean_ree_modules || \
		{ echo "ERROR: Cleaning modules failed, abort."; exit 1; }
fi

if [[ $BUILD_MODULE == 1 ]]; then
	build_ree_modules "$MODULE_BUILD_CONFIG" || \
		{ echo "ERROR: Building modules failed, abort."; exit 1; }
fi

if [[ $DEPLOY_MODULES == 1 ]]; then
	deploy_ree_modules || \
		{ echo "ERROR: Deploying modules failed, abort."; exit 1; }
fi

