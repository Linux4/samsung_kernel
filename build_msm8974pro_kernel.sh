#!/bin/bash
# MSM8974 JBP kernel build script v0.4

BUILD_TOP_DIR=..
BUILD_KERNEL_DIR=$(pwd)

BUILD_CROSS_COMPILE=../prebuilts/gcc/linux-x86/arm/arm-eabi-4.7/bin/arm-eabi-
BUILD_JOB_NUMBER=`grep processor /proc/cpuinfo|wc -l`

# Default Python version is 2.7
mkdir -p bin
ln -sf /usr/bin/python2.7 ./bin/python
export PATH=$(pwd)/bin:$PATH

KERNEL_DEFCONFIG=msm8974_sec_defconfig
DEBUG_DEFCONFIG=msm8974_sec_eng_defconfig
SELINUX_DEFCONFIG=selinux_defconfig
SELINUX_LOG_DEFCONFIG=selinux_log_defconfig

#sed -i.bak "s/CONFIG_MODVERSIONS=y/CONFIG_MODVERSIONS=n/g" ${BUILD_KERNEL_DIR}/arch/arm/configs/${KERNEL_DEFCONFIG}

while getopts "w:t:" flag; do
	case $flag in
		w)
			BUILD_OPTION_HW_REVISION=$OPTARG
			echo "-w : "$BUILD_OPTION_HW_REVISION""
			;;
		t)
			TARGET_BUILD_VARIANT=$OPTARG
			echo "-t : "$TARGET_BUILD_VARIANT""
			;;
		*)
			echo "wrong 2nd param : "$OPTARG""
			exit -1
			;;
	esac
done

shift $((OPTIND-1))

BUILD_COMMAND=$1
MODEL=${BUILD_COMMAND%%_*}
TEMP=${BUILD_COMMAND#*_}
REGION=${TEMP%%_*}
CARRIER=${TEMP##*_}

VARIANT=k${CARRIER}
DTS_NAMES=msm8974pro-ac-sec-k-
#DTS_NAMES=msm8974pro-ac-sec
PROJECT_NAME=${VARIANT}
VARIANT_DEFCONFIG=msm8974pro_sec_${MODEL}_${CARRIER}_defconfig
case $1 in
		clean)
		#echo "Clean..."
		echo "Not support... remove kernel out directory by yourself"
		#make -C $BUILD_KERNEL_DIR clean
		#make -C $BUILD_KERNEL_DIR distclean
		#rm $BUILD_KERNEL_OUT_DIR -rf
		exit 1
		;;
		
		*)
		BUILD_KERNEL_OUT_DIR=$BUILD_TOP_DIR/okernel
		PRODUCT_OUT=$BUILD_TOP_DIR/okernel
		
		BOARD_KERNEL_BASE=0x00000000
		BOARD_KERNEL_PAGESIZE=2048
		BOARD_KERNEL_TAGS_OFFSET=0x01E00000
		BOARD_RAMDISK_OFFSET=0x02000000
		BOARD_KERNEL_CMDLINE="console=ttyHSL0,115200,n8 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x37 ehci-hcd.park=3"
		#BOARD_KERNEL_CMDLINE="console=ttyHSL0,115200,n8 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x37 ehci-hcd.park=3"
		mkdir -p $BUILD_KERNEL_OUT_DIR
		;;

esac

KERNEL_ZIMG=$BUILD_KERNEL_OUT_DIR/arch/arm/boot/zImage
DTC=$BUILD_KERNEL_OUT_DIR/scripts/dtc/dtc

FUNC_APPEND_DTB()
{
	if ! [ -d $BUILD_KERNEL_OUT_DIR/arch/arm/boot ] ; then
		echo "error no directory : "$BUILD_KERNEL_OUT_DIR/arch/arm/boot""
		exit -1
	else
		echo "rm files in : "$BUILD_KERNEL_OUT_DIR/arch/arm/boot/*-zImage""
		rm $BUILD_KERNEL_OUT_DIR/arch/arm/boot/*-zImage
		echo "rm files in : "$BUILD_KERNEL_OUT_DIR/arch/arm/boot/*.dtb""
		rm $BUILD_KERNEL_OUT_DIR/arch/arm/boot/*.dtb
	fi
	
	for DTS_FILE in `ls ${BUILD_KERNEL_DIR}/arch/arm/boot/dts/msm8974pro/${DTS_NAMES}*.dts`
	do
		DTB_FILE=${DTS_FILE%.dts}.dtb
		DTB_FILE=$BUILD_KERNEL_OUT_DIR/arch/arm/boot/${DTB_FILE##*/}
		ZIMG_FILE=${DTB_FILE%.dtb}-zImage
		
		echo ""
		echo "dts : $DTS_FILE"
		echo "dtb : $DTB_FILE"
		echo "out : $ZIMG_FILE"
		echo ""
		
		$DTC -p 1024 -O dtb -o $DTB_FILE $DTS_FILE
		cat $KERNEL_ZIMG $DTB_FILE > $ZIMG_FILE
	done
}

INSTALLED_DTIMAGE_TARGET=${BUILD_KERNEL_OUT_DIR}/dt.img
DTBTOOL=$BUILD_TOP_DIR/kernel/tools/dtbTool

FUNC_BUILD_DTIMAGE_TARGET()
{
	echo ""
	echo "================================="
	echo "START : FUNC_BUILD_DTIMAGE_TARGET"
	echo "================================="
	echo ""
	echo "DT image target : $INSTALLED_DTIMAGE_TARGET"
	
	if ! [ -e $DTBTOOL ] ; then
		if ! [ -d $BUILD_TOP_DIR/out/host/linux-x86/bin ] ; then
			mkdir -p $BUILD_TOP_DIR/out/host/linux-x86/bin
		fi
		cp $BUILD_TOP_DIR/kernel/tools/dtbTool $DTBTOOL
	fi

	echo "$DTBTOOL -o $INSTALLED_DTIMAGE_TARGET -s $BOARD_KERNEL_PAGESIZE \
						-p $BUILD_KERNEL_OUT_DIR/scripts/dtc/ $BUILD_KERNEL_OUT_DIR/arch/arm/boot/"
	$DTBTOOL -o $INSTALLED_DTIMAGE_TARGET -s $BOARD_KERNEL_PAGESIZE \
						-p $BUILD_KERNEL_OUT_DIR/scripts/dtc/ $BUILD_KERNEL_OUT_DIR/arch/arm/boot/

	chmod a+r $INSTALLED_DTIMAGE_TARGET

	echo ""
	echo "================================="
	echo "END   : FUNC_BUILD_DTIMAGE_TARGET"
	echo "================================="
	echo ""
}

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

        if [ "$BUILD_COMMAND" == "" ]; then
                SECFUNC_PRINT_HELP;
                exit -1;
        fi

	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER ARCH=arm \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE \
			$KERNEL_DEFCONFIG VARIANT_DEFCONFIG=$VARIANT_DEFCONFIG \
			DEBUG_DEFCONFIG=$DEBUG_DEFCONFIG SELINUX_DEFCONFIG=$SELINUX_DEFCONFIG \
			SELINUX_LOG_DEFCONFIG=$SELINUX_LOG_DEFCONFIG || exit -1

	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER ARCH=arm \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE || exit -1

	FUNC_APPEND_DTB
	FUNC_BUILD_DTIMAGE_TARGET
	
	echo ""
	echo "================================="
	echo "END   : FUNC_BUILD_KERNEL"
	echo "================================="
	echo ""
}

FUNC_MKBOOTIMG()
{
	echo ""
	echo "==================================="
	echo "START : FUNC_MKBOOTIMG"
	echo "==================================="
	echo ""
	MKBOOTIMGTOOL=$BUILD_TOP_DIR/kernel/tools/mkbootimg

	if ! [ -e $MKBOOTIMGTOOL ] ; then
		if ! [ -d $BUILD_TOP_DIR/out/host/linux-x86/bin ] ; then
			mkdir -p $BUILD_TOP_DIR/out/host/linux-x86/bin
		fi
		cp $BUILD_TOP_DIR/kernel/tools/mkbootimg $MKBOOTIMGTOOL
	fi

	echo "Making boot.img ..."
	echo "	$MKBOOTIMGTOOL --kernel $KERNEL_ZIMG \
			--ramdisk $PRODUCT_OUT/ramdisk.img \
			--output $PRODUCT_OUT/boot.img \
			--cmdline "$BOARD_KERNEL_CMDLINE" \
			--base $BOARD_KERNEL_BASE \
			--pagesize $BOARD_KERNEL_PAGESIZE \
			--ramdisk_offset $BOARD_RAMDISK_OFFSET \
			--tags_offset $BOARD_KERNEL_TAGS_OFFSET \
			--dt $INSTALLED_DTIMAGE_TARGET"
			
	$MKBOOTIMGTOOL --kernel $KERNEL_ZIMG \
			--ramdisk $PRODUCT_OUT/ramdisk.img \
			--output $PRODUCT_OUT/boot.img \
			--cmdline "$BOARD_KERNEL_CMDLINE" \
			--base $BOARD_KERNEL_BASE \
			--pagesize $BOARD_KERNEL_PAGESIZE \
			--ramdisk_offset $BOARD_RAMDISK_OFFSET \
			--tags_offset $BOARD_KERNEL_TAGS_OFFSET \
			--dt $INSTALLED_DTIMAGE_TARGET
	
	cd $PRODUCT_OUT
	tar cvf boot_${MODEL}_${CARRIER}.tar boot.img
	
	if ! [ -d $BUILD_TOP_DIR/kernel/okernel ] ; then
		mkdir -p $BUILD_TOP_DIR/kernel/okernel
	else
		rm $BUILD_TOP_DIR/kernel/okernel/boot_${MODEL}_${REGION}_${CARRIER}.tar -f
	fi

	cd ~
	
	echo ""
	echo "==================================="
	echo "END   : FUNC_MKBOOTIMG"
	echo "==================================="
	echo ""	
}

SECFUNC_PRINT_HELP()
{
    echo -e '\E[33m'
    echo "Help"
    echo "$0 \$1"
    echo "  \$1 : "
        echo "  klte_eur"
        echo "  klte_att"
        echo "  klte_spr"
        echo "  klte_tmo"
        echo "  klte_vzw"
        echo "  klte_usc"
        echo -e '\E[0m'
}


# MAIN FUNCTION
rm -rf ./build.log
(
    START_TIME=`date +%s`

	FUNC_BUILD_KERNEL
	#FUNC_RAMDISK_EXTRACT_N_COPY
	FUNC_MKBOOTIMG

    END_TIME=`date +%s`
	
    let "ELAPSED_TIME=$END_TIME-$START_TIME"
    echo "Total compile time is $ELAPSED_TIME seconds"
) 2>&1	 | tee -a ./build.log
