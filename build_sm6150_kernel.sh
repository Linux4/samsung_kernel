#!/bin/bash

BUILD_COMMAND=$1
PRODUCT_OUT=$2
PRODUCT_NAME=$3


if  [ "${SEC_BUILD_CONF_USE_DMVERITY}" == "true" ] ; then
	DMVERITY_DEFCONFIG=dmverity_defconfig
fi

#if  [ "${SEC_BUILD_CONF_KERNEL_KASLR}" == "true" ] ; then
#	KASLR_DEFCONFIG=kaslr_defconfig
#fi

MODEL=${BUILD_COMMAND%%_*}

        
echo PRODUCT_NAME=$PRODUCT_NAME
echo MODEL=$MODEL
echo DMVERITY_DEFCONFIG=$DMVERITY_DEFCONFIG
echo KASLR_DEFCONFIG=$KASLR_DEFCONFIG

BUILD_WHERE=$(pwd)
BUILD_KERNEL_DIR=$BUILD_WHERE
BUILD_ROOT_DIR=$BUILD_KERNEL_DIR/../../..
BUILD_KERNEL_OUT_DIR=$PRODUCT_OUT/obj/KERNEL_OBJ

SECURE_SCRIPT=$BUILD_ROOT_DIR/buildscript/tools/signclient.jar
BUILD_CROSS_COMPILE=$BUILD_ROOT_DIR/android/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
KERNEL_LLVM_BIN=$BUILD_ROOT_DIR/android/vendor/qcom/proprietary/llvm-arm-toolchain-ship/6.0/bin/clang
CLANG_TRIPLE=aarch64-linux-gnu-
KERNEL_MAKE_ENV="DTC_EXT=$BUILD_KERNEL_DIR/tools/dtc CONFIG_BUILD_ARM64_DT_OVERLAY=y" 

BUILD_JOB_NUMBER=`grep processor /proc/cpuinfo|wc -l`

KERNEL_DEFCONFIG=sm6150_sec_defconfig
DEBUG_DEFCONFIG=sm6150_sec_eng_defconfig
SELINUX_DEFCONFIG=selinux_defconfig
SELINUX_LOG_DEFCONFIG=selinux_log_defconfig

MODEL=${BUILD_COMMAND%%_*}
TEMP=${BUILD_COMMAND#*_}
REGION=${TEMP%%_*}
CARRIER=${TEMP##*_}

VARIANT=${CARRIER}
PROJECT_NAME=${VARIANT}
if [ "${REGION}" == "${CARRIER}" ]; then
	VARIANT_DEFCONFIG=sm6150_sec_${MODEL}_${CARRIER}_defconfig
else
	VARIANT_DEFCONFIG=sm6150_sec_${MODEL}_${REGION}_${CARRIER}_defconfig
fi

BUILD_CONF_PATH=$BUILD_ROOT_DIR/buildscript/build_conf/${MODEL}
BUILD_CONF=${BUILD_CONF_PATH}/common_build_conf.${MODEL}

BOARD_KERNEL_PAGESIZE=4096
mkdir -p $BUILD_KERNEL_OUT_DIR

KERNEL_ZIMG=$BUILD_KERNEL_OUT_DIR/arch/arm64/boot/Image.gz-dtb

FUNC_BUILD_KERNEL()
{
	echo ""
	echo "=============================================="
	echo "START : FUNC_BUILD_KERNEL"
	echo "=============================================="
	echo ""
	echo "build project="$PROJECT_NAME""
	echo "build common config="$KERNEL_DEFCONFIG ""
	echo "build variant config="$VARIANT_DEFCONFIG ""
	echo "build secure option="$SECURE_OPTION ""
	echo "build SEANDROID option="$SEANDROID_OPTION ""
	echo "build selinux defconfig="$SELINUX_DEFCONFIG ""
	echo "build selinux log defconfig="$SELINUX_LOG_DEFCONFIG ""
	echo "build dmverity defconfig="$DMVERITY_DEFCONFIG""
	echo "build kaslr defconfig="$KASLR_DEFCONFIG""
	echo "build PRODUCT_OUT="$PRODUCT_OUT ""

        if [ "$BUILD_COMMAND" == "" ]; then
                SECFUNC_PRINT_HELP;
                exit -1;
        fi


    chmod u+w $PRODUCT_OUT/mkbootimg_ver_args.txt     
    echo '--cmdline "console=ttyMSM0,115200,n8 androidboot.console=ttyMSM0 earlycon=msm_serial_dm,0xc1b0000 boot_cpus=0-3 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x37 ehci-hcd.park=3 lpm_levels.sleep_disabled=1 sched_enable_hmp=1 sched_enable_power_aware=1 androidboot.selinux=permissive service_locator.enable=1" --base 0x00000000 --pagesize 4096 --os_version 8.1.0 --os_patch_level 2018-04-01 --ramdisk_offset 0x02000000 --tags_offset 0x01E00000 '   >  $PRODUCT_OUT/mkbootimg_ver_args.txt   
    
    echo ----------------------------------------------
    echo info $PRODUCT_OUT/mkbootimg_ver_args.txt
    echo ----------------------------------------------
    cat  $PRODUCT_OUT/mkbootimg_ver_args.txt
    echo ----------------------------------------------
    
	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER $KERNEL_MAKE_ENV ARCH=arm64 \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE \
			REAL_CC=$KERNEL_LLVM_BIN \
			CLANG_TRIPLE=$CLANG_TRIPLE \
			$KERNEL_DEFCONFIG \
			VARIANT_DEFCONFIG=$VARIANT_DEFCONFIG \
			DEBUG_DEFCONFIG=$DEBUG_DEFCONFIG \
			SELINUX_DEFCONFIG=$SELINUX_DEFCONFIG \
			SELINUX_LOG_DEFCONFIG=$SELINUX_LOG_DEFCONFIG \
			DMVERITY_DEFCONFIG=$DMVERITY_DEFCONFIG \
			KASLR_DEFCONFIG=$KASLR_DEFCONFIG || exit -1

	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER $KERNEL_MAKE_ENV ARCH=arm64 \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE \
			REAL_CC=$KERNEL_LLVM_BIN \
			CLANG_TRIPLE=$CLANG_TRIPLE  || exit -1

#	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER $KERNEL_MAKE_ENV ARCH=arm64 \
#			CROSS_COMPILE=$BUILD_CROSS_COMPILE dtbs || exit -1            
	$BUILD_KERNEL_DIR/tools/mkdtimg create $PRODUCT_OUT/dtbo.img --page_size=4096 $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/samsung/sm6150-sec-${MODEL}-${REGION}-overlay*.dtbo

    rsync -cv $KERNEL_ZIMG $PRODUCT_OUT/kernel
    
    ls -al $PRODUCT_OUT/kernel
    
	echo ""
	echo "================================="
	echo "END   : FUNC_BUILD_KERNEL"
	echo "================================="
	echo ""
}

SECFUNC_PRINT_HELP()
{
	echo -e '\E[33m'
	echo "Help"
	echo "$0 \$1"
	echo "  \$1 : "
	for model in `ls -1 ../../../model/build_conf | sed -e "s/.\+\///"`; do
		for build_conf in `find ../../../model/build_conf/$model/ -name build_conf.${model}_* | sed -e "s/.\+build_conf\.//"`; do
			echo "      $build_conf"
		done
	done
	echo -e '\E[0m'
}

(
	FUNC_BUILD_KERNEL
)
