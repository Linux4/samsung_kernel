#!/bin/bash
# MSM8996 kernel build script v0.5

BUILD_COMMAND=$1
if [ "$BUILD_COMMAND" == "heroqlte_usa_acg" ]; then
        PRODUCT_NAME=heroqlteacg;
elif [ "$BUILD_COMMAND" == "heroqlte_usa_att" ]; then
        PRODUCT_NAME=heroqlteatt;
elif [ "$BUILD_COMMAND" == "heroqlte_usa_spr" ]; then
        PRODUCT_NAME=heroqltespr;
elif [ "$BUILD_COMMAND" == "heroqlte_usa_tmo" ]; then
        PRODUCT_NAME=heroqltetmo;
elif [ "$BUILD_COMMAND" == "heroqlte_usa_usc" ]; then
        PRODUCT_NAME=heroqlteusc;
elif [ "$BUILD_COMMAND" == "heroqlte_usa_vzw" ]; then
        PRODUCT_NAME=heroqltevzw;
elif [ "$BUILD_COMMAND" == "heroqlte_usa_mtr" ]; then
        PRODUCT_NAME=heroqltemtr;
elif [ "$BUILD_COMMAND" == "heroqlte_chn_open" ]; then
        PRODUCT_NAME=heroqltezc;
elif [ "$BUILD_COMMAND" == "heroqlte_jpn_kdi" ]; then
        PRODUCT_NAME=heroqltekdi;
elif [ "$BUILD_COMMAND" == "heroqlte_jpn_dcm" ]; then
        PRODUCT_NAME=heroqltedcm;
elif [ "$BUILD_COMMAND" == "heroqlte_usa_open" ]; then
        PRODUCT_NAME=heroqltesed;
elif [ "$BUILD_COMMAND" == "hero2qlte_usa_att" ]; then
        PRODUCT_NAME=hero2qlteatt;
elif [ "$BUILD_COMMAND" == "hero2qlte_usa_spr" ]; then
        PRODUCT_NAME=hero2qltespr;
elif [ "$BUILD_COMMAND" == "hero2qlte_usa_tmo" ]; then
        PRODUCT_NAME=hero2qltetmo;
elif [ "$BUILD_COMMAND" == "hero2qlte_usa_usc" ]; then
        PRODUCT_NAME=hero2qlteusc;
elif [ "$BUILD_COMMAND" == "hero2qlte_usa_vzw" ]; then
        PRODUCT_NAME=hero2qltevzw;
elif [ "$BUILD_COMMAND" == "hero2qlte_chn_open" ]; then
        PRODUCT_NAME=hero2qltezc;
elif [ "$BUILD_COMMAND" == "hero2qlte_jpn_kdi" ]; then
        PRODUCT_NAME=hero2qltekdi;
elif [ "$BUILD_COMMAND" == "hero2qlte_jpn_dcm" ]; then
        PRODUCT_NAME=hero2qltedcm;
elif [ "$BUILD_COMMAND" == "hero2qlte_usa_open" ]; then
        PRODUCT_NAME=hero2qltesed;
elif [ "$BUILD_COMMAND" == "aeroqlte_usa_vzw" ]; then
        PRODUCT_NAME=aeroqltevzw;
elif [ "$BUILD_COMMAND" == "poseidonlte_usa_att" ]; then
        PRODUCT_NAME=poseidonlteatt;
elif [ "$BUILD_COMMAND" == "veyronlte_ctc" ]; then
        PRODUCT_NAME=veyronltectc;
else
	#default product
        PRODUCT_NAME=heroqlteatt;
fi

BUILD_WHERE=$(pwd)
BUILD_KERNEL_DIR=$BUILD_WHERE
BUILD_ROOT_DIR=$BUILD_KERNEL_DIR/../..
BUILD_KERNEL_OUT_DIR=$BUILD_ROOT_DIR/android/out/target/product/$PRODUCT_NAME/obj/KERNEL_OBJ
PRODUCT_OUT=$BUILD_ROOT_DIR/android/out/target/product/$PRODUCT_NAME


SECURE_SCRIPT=$BUILD_ROOT_DIR/buildscript/tools/signclient.jar
BUILD_CROSS_COMPILE=$BUILD_ROOT_DIR/android/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
BUILD_JOB_NUMBER=`grep processor /proc/cpuinfo|wc -l`

# Default Python version is 2.7
mkdir -p bin
ln -sf /usr/bin/python2.7 ./bin/python
export PATH=$(pwd)/bin:$PATH

KERNEL_DEFCONFIG=msm8996_sec_defconfig
DEBUG_DEFCONFIG=msm8996_sec_eng_defconfig
# SELINUX_DEFCONFIG=selinux_defconfig
# SELINUX_LOG_DEFCONFIG=selinux_log_defconfig
DMVERITY_DEFCONFIG=dmverity_defconfig

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
SECURE_OPTION=$2
SEANDROID_OPTION=$3

if [ "$BUILD_COMMAND" == "heroqlte_att" ]; then
	SIGN_MODEL=
else
	SIGN_MODEL=
fi

MODEL=${BUILD_COMMAND%%_*}
TEMP=${BUILD_COMMAND#*_}
REGION=${TEMP%%_*}
CARRIER=${TEMP##*_}

VARIANT=k${CARRIER}
PROJECT_NAME=${VARIANT}
if [ "$BUILD_COMMAND" == "heroqlte_chn_open" ] || [ "$BUILD_COMMAND" == "hero2qlte_chn_open" ]; then
	VARIANT_DEFCONFIG=msm8996_sec_${MODEL}_chnzc_defconfig
else
	if [ "$BUILD_COMMAND" == "heroqlte_usa_open" ] || [ "$BUILD_COMMAND" == "hero2qlte_usa_open" ]; then
		VARIANT_DEFCONFIG=msm8996_sec_${MODEL}_sed_defconfig
	else
		VARIANT_DEFCONFIG=msm8996_sec_${MODEL}_${CARRIER}_defconfig
	fi
fi

CERTIFICATION=NONCERT

case $1 in
		clean)
		echo "Not support... remove kernel out directory by yourself"
		exit 1
		;;
		
		*)
		
		BOARD_KERNEL_BASE=0x80000000
		BOARD_KERNEL_PAGESIZE=4096
		BOARD_KERNEL_TAGS_OFFSET=0x02000000
		BOARD_RAMDISK_OFFSET=0x02200000
		BOARD_KERNEL_CMDLINE="console=ttyHSL0,115200,n8 androidboot.console=ttyHSL0 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x37 ehci-hcd.park=3 lpm_levels.sleep_disabled=1 cma=16M@0-0xffffffff cnsscore.pcie_link_down_panic=1"
		
		mkdir -p $BUILD_KERNEL_OUT_DIR
		;;

esac

KERNEL_ZIMG=$BUILD_KERNEL_OUT_DIR/arch/arm64/boot/Image
DTC=$BUILD_KERNEL_OUT_DIR/scripts/dtc/dtc

FUNC_CLEAN_DTB()
{
	if ! [ -d $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts ] ; then
		echo "no directory : "$BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts""
	else
		echo "rm files in : "$BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/*.dtb""
		rm -r $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/*.dtb
	fi
}

INSTALLED_DTIMAGE_TARGET=${PRODUCT_OUT}/dt.img
DTBTOOL=$BUILD_KERNEL_DIR/tools/dtbTool

FUNC_BUILD_DTIMAGE_TARGET()
{
	echo ""
	echo "================================="
	echo "START : FUNC_BUILD_DTIMAGE_TARGET"
	echo "================================="
	echo ""
	echo "DT image target : $INSTALLED_DTIMAGE_TARGET"
	
	if ! [ -e $DTBTOOL ] ; then
		if ! [ -d $BUILD_ROOT_DIR/android/out/host/linux-x86/bin ] ; then
			mkdir -p $BUILD_ROOT_DIR/android/out/host/linux-x86/bin
		fi
		cp $BUILD_ROOT_DIR/kernel/tools/dtbTool $DTBTOOL
	fi

	cp $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/samsung/*.dtb $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/.

	echo "$DTBTOOL -o $INSTALLED_DTIMAGE_TARGET -s $BOARD_KERNEL_PAGESIZE \
		-p $BUILD_KERNEL_OUT_DIR/scripts/dtc/ $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/"
		$DTBTOOL -o $INSTALLED_DTIMAGE_TARGET -s $BOARD_KERNEL_PAGESIZE \
		-p $BUILD_KERNEL_OUT_DIR/scripts/dtc/ $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/

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
	echo "build secure option="$SECURE_OPTION ""
	echo "build SEANDROID option="$SEANDROID_OPTION ""

        if [ "$BUILD_COMMAND" == "" ]; then
                SECFUNC_PRINT_HELP;
                exit -1;
        fi

	FUNC_CLEAN_DTB

	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER ARCH=arm64 \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE KCFLAGS=-mno-android \
			VARIANT_DEFCONFIG=$VARIANT_DEFCONFIG \
			TIMA_DEFCONFIG=tima_defconfig \
			DEBUG_DEFCONFIG=$DEBUG_DEFCONFIG $KERNEL_DEFCONFIG \
			SELINUX_DEFCONFIG=$SELINUX_DEFCONFIG \
			SELINUX_LOG_DEFCONFIG=$SELINUX_LOG_DEFCONFIG || exit -1

	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER ARCH=arm64 \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE KCFLAGS=-mno-android || exit -1

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
	MKBOOTIMGTOOL=$BUILD_ROOT_DIR/android/kernel/tools/mkbootimg

	if ! [ -e $MKBOOTIMGTOOL ] ; then
		if ! [ -d $BUILD_ROOT_DIR/android/out/host/linux-x86/bin ] ; then
			mkdir -p $BUILD_ROOT_DIR/android/out/host/linux-x86/bin
		fi
		cp $BUILD_ROOT_DIR/anroid/kernel/tools/mkbootimg $MKBOOTIMGTOOL
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

	if [ "$SEANDROID_OPTION" == "-E" ] ; then
		FUNC_SEANDROID
	fi

	if [ "$SECURE_OPTION" == "-B" ]; then
		FUNC_SECURE_SIGNING
	fi

	cd $PRODUCT_OUT
	tar cvf boot_${MODEL}_${CARRIER}_${CERTIFICATION}.tar boot.img

	cd $BUILD_ROOT_DIR
	if ! [ -d output ] ; then
		mkdir -p output
	fi

        echo ""
	echo "================================================="
        echo "-->Note, copy to $BUILD_TOP_DIR/../output/ directory"
	echo "================================================="
	cp $PRODUCT_OUT/boot_${MODEL}_${CARRIER}_${CERTIFICATION}.tar $BUILD_ROOT_DIR/output/boot_${MODEL}_${CARRIER}_${CERTIFICATION}.tar || exit -1
        cd ~

	echo ""
	echo "==================================="
	echo "END   : FUNC_MKBOOTIMG"
	echo "==================================="
	echo ""
}

FUNC_SEANDROID()
{
	echo -n "SEANDROIDENFORCE" >> $PRODUCT_OUT/boot.img
}

FUNC_SECURE_SIGNING()
{
	echo "java -jar $SECURE_SCRIPT -model $SIGN_MODEL -runtype ss_openssl_sha -input $PRODUCT_OUT/boot.img -output $PRODUCT_OUT/signed_boot.img"
	openssl dgst -sha256 -binary $PRODUCT_OUT/boot.img > sig_32
	java -jar $SECURE_SCRIPT -runtype ss_openssl_sha -model $SIGN_MODEL -input sig_32 -output sig_256
	cat $PRODUCT_OUT/boot.img sig_256 > $PRODUCT_OUT/signed_boot.img

	mv -f $PRODUCT_OUT/boot.img $PRODUCT_OUT/unsigned_boot.img
	mv -f $PRODUCT_OUT/signed_boot.img $PRODUCT_OUT/boot.img

	CERTIFICATION=CERT
}

SECFUNC_PRINT_HELP()
{
	echo -e '\E[33m'
	echo "Help"
	echo "$0 \$1 \$2 \$3"
	echo "  \$1 : "
	echo "      heroqlte_usa_acg"
	echo "      heroqlte_usa_att"
	echo "      heroqlte_usa_spr"
	echo "      heroqlte_usa_tmo"
	echo "      heroqlte_usa_usc"
	echo "      heroqlte_usa_vzw"
	echo "      heroqlte_usa_mtr"
	echo "      heroqlte_chn_open"
	echo "      heroqlte_jpn_kdi"
	echo "      heroqlte_jpn_dcm"
	echo "      heroqlte_usa_open"
	echo "      hero2qlte_usa_att"
	echo "      hero2qlte_usa_spr"
	echo "      hero2qlte_usa_tmo"
	echo "      hero2qlte_usa_usc"
	echo "      hero2qlte_usa_vzw"
	echo "      hero2qlte_chn_open"
	echo "      hero2qlte_jpn_kdi"
	echo "      hero2qlte_jpn_dcm"
	echo "      hero2qlte_usa_open"
	echo "      aeroqlte_usa_vzw"
	echo "      poseidonlte_usa_att"
	echo "  \$2 : "
	echo "      -B or Nothing  (-B : Secure Binary)"
	echo "  \$3 : "
	echo "      -E or Nothing  (-E : SEANDROID Binary)"
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
