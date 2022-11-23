#!/bin/bash
# MSM8916 KK kernel build script v0.5

SELINUX_DEFCONFIG=selinux_defconfig
SELINUX_LOG_DEFCONFIG=selinux_log_defconfig

BUILD_COMMAND=$1
if [ "$BUILD_COMMAND" == "kleos_eur" ]; then
        PRODUCT_NAME=kleoslte;
elif [ "$BUILD_COMMAND" == "rossa_cmcc" ]; then
        PRODUCT_NAME=rossalte;
elif [ "$BUILD_COMMAND" == "rossa_ctc" ]; then
        PRODUCT_NAME=rossaltectc;
elif [ "$BUILD_COMMAND" == "rossa_spr" ]; then
        PRODUCT_NAME=rossaltespr;
elif [ "$BUILD_COMMAND" == "rossa_tfn" ]; then
        PRODUCT_NAME=rossaltetfn;
elif [ "$BUILD_COMMAND" == "rossa_vzw" ]; then
        PRODUCT_NAME=rossaltevzw;
elif [ "$BUILD_COMMAND" == "rossa_aio" ]; then
        PRODUCT_NAME=rossalteaio;
elif [ "$BUILD_COMMAND" == "rossa_aus" ]; then
        PRODUCT_NAME=rossaltedv;
elif [ "$BUILD_COMMAND" == "fortuna_cmcc" ]; then
        PRODUCT_NAME=fortunalte;
elif [ "$BUILD_COMMAND" == "fortuna_ctc" ]; then
        PRODUCT_NAME=fortunaltectc;
elif [ "$BUILD_COMMAND" == "fortuna_cu" ]; then
        PRODUCT_NAME=fortunalte;
elif [ "$BUILD_COMMAND" == "fortuna3g_eur" ]; then
        PRODUCT_NAME=fortuna3g;
elif [ "$BUILD_COMMAND" == "fortuna_sea" ]; then
        PRODUCT_NAME=fortunalte;
elif [ "$BUILD_COMMAND" == "gpen_eur" ]; then
        PRODUCT_NAME=gpenlte;
elif [ "$BUILD_COMMAND" == "heat_eur" ]; then
        PRODUCT_NAME=heatlte;
elif [ "$BUILD_COMMAND" == "a3_chnopen" ]; then
        PRODUCT_NAME=a3ltechn;
elif [ "$BUILD_COMMAND" == "a3_chntw" ]; then
        PRODUCT_NAME=a3ltezt;
elif [ "$BUILD_COMMAND" == "a3_chnctc" ]; then
        PRODUCT_NAME=a3ltectc;
elif [ "$BUILD_COMMAND" == "a3_eur" ]; then
        PRODUCT_NAME=a3ltexx;
elif [ "$BUILD_COMMAND" == "a3_ltn" ]; then
        PRODUCT_NAME=a3lteub;
elif [ "$BUILD_COMMAND" == "a33g_eur" ]; then
        PRODUCT_NAME=a33gxx;
elif [ "$BUILD_COMMAND" == "o1_chnopen" ]; then
        PRODUCT_NAME=o1ltechn;
elif [ "$BUILD_COMMAND" == "vivalto_aus" ]; then
        PRODUCT_NAME=vivaltolte;
elif [ "$BUILD_COMMAND" == "vivalto_mea" ]; then
        PRODUCT_NAME=vivaltolte;
else
	#default product
	PRODUCT_NAME=$BUILD_COMMAND;
fi

BUILD_WHERE=$(pwd)
BUILD_KERNEL_DIR=$BUILD_WHERE
BUILD_ROOT_DIR=$BUILD_KERNEL_DIR/../..
BUILD_KERNEL_OUT_DIR=$BUILD_ROOT_DIR/android/out/target/product/$PRODUCT_NAME/obj/KERNEL_OBJ
PRODUCT_OUT=$BUILD_ROOT_DIR/android/out/target/product/$PRODUCT_NAME

SECURE_SCRIPT=$BUILD_ROOT_DIR/buildscript/tools/signclient.jar
BUILD_CROSS_COMPILE=$BUILD_ROOT_DIR/android/prebuilts/gcc/linux-x86/arm/arm-eabi-4.7/bin/arm-eabi-
BUILD_JOB_NUMBER=`grep processor /proc/cpuinfo|wc -l`

# Default Python version is 2.7
mkdir -p bin
ln -sf /usr/bin/python2.7 ./bin/python
export PATH=$(pwd)/bin:$PATH

KERNEL_DEFCONFIG=msm8916_sec_defconfig
DEBUG_DEFCONFIG=msm8916_sec_eng_defconfig

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
SEANDROID_OPTION=
SECURE_OPTION=
if [ "$2" == "-B" ]; then
	SECURE_OPTION=$2
elif [ "$2" == "-E" ]; then
	SEANDROID_OPTION=$2
else
	NO_JOB=
fi

if [ "$3" == "-B" ]; then
	SECURE_OPTION=$3
elif [ "$3" == "-E" ]; then
	SEANDROID_OPTION=$3
else
	NO_JOB=
fi

if [ "$BUILD_COMMAND" == "kleos_eur" ]; then
	SIGN_MODEL=SM-G510F_EUR_XX_ROOT0
elif [ "$BUILD_COMMAND" == "fortuna_cmcc" ]; then
	SIGN_MODEL=SM-G5308W_CHN_ZM_ROOT0
elif [ "$BUILD_COMMAND" == "fortuna_ctc" ]; then
	SIGN_MODEL=SM-G5309W_CHN_CTC_ROOT0
elif [ "$BUILD_COMMAND" == "fortuna_sea" ]; then
	SIGN_MODEL=SM-G530F_SEA_DX_ROOT0
elif [ "$BUILD_COMMAND" == "heat_eur" ]; then
	SIGN_MODEL=SM-G357FZ_EUR_XX_ROOT0
else
	SIGN_MODEL=
fi

MODEL=${BUILD_COMMAND%%_*}
TEMP=${BUILD_COMMAND#*_}
REGION=${TEMP%%_*}
CARRIER=${TEMP##*_}

VARIANT=k${CARRIER}
PROJECT_NAME=${VARIANT}
VARIANT_DEFCONFIG=msm8916_sec_${MODEL}_${CARRIER}_defconfig

CERTIFICATION=NONCERT

case $1 in
		clean)
		echo "Not support... remove kernel out directory by yourself"
		exit 1
		;;

		*)

		BOARD_KERNEL_BASE=0x80000000
		BOARD_KERNEL_PAGESIZE=2048
		BOARD_KERNEL_TAGS_OFFSET=0x01E00000
		BOARD_RAMDISK_OFFSET=0x02000000
		#BOARD_KERNEL_CMDLINE="console=ttyHSL0,115200,n8 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x37 ehci-hcd.park=3"
		BOARD_KERNEL_CMDLINE="console=ttyHSL0,115200,n8 androidboot.console=ttyHSL0 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x3F ehci-hcd.park=3 androidboot.bootdevice=7824900.sdhci"
#		BOARD_KERNEL_CMDLINE="console=ttyHSL0,115200,n8 androidboot.hardware=qcom androidboot.bootdevice=soc.0/7824900.sdhci user_debug=31 msm_rtb.filter=0x3F ehci-hcd.park=3 video=vfb:640x400,bpp=32,memsize=3072000 earlyprintk"
		mkdir -p $BUILD_KERNEL_OUT_DIR
		;;

esac

KERNEL_ZIMG=$BUILD_KERNEL_OUT_DIR/arch/arm/boot/zImage
DTC=$BUILD_KERNEL_OUT_DIR/scripts/dtc/dtc

FUNC_CLEAN_DTB()
{
	if ! [ -d $BUILD_KERNEL_OUT_DIR/arch/arm/boot/dts ] ; then
		echo "no directory : "$BUILD_KERNEL_OUT_DIR/arch/arm/boot/dts""
	else
		echo "rm files in : "$BUILD_KERNEL_OUT_DIR/arch/arm/boot/dts/*.dtb""
		rm $BUILD_KERNEL_OUT_DIR/arch/arm/boot/dts/*.dtb
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

	echo "$DTBTOOL -o $INSTALLED_DTIMAGE_TARGET -s $BOARD_KERNEL_PAGESIZE \
		-p $BUILD_KERNEL_OUT_DIR/scripts/dtc/ $BUILD_KERNEL_OUT_DIR/arch/arm/boot/dts/"
		$DTBTOOL -o $INSTALLED_DTIMAGE_TARGET -s $BOARD_KERNEL_PAGESIZE \
		-p $BUILD_KERNEL_OUT_DIR/scripts/dtc/ $BUILD_KERNEL_OUT_DIR/arch/arm/boot/dts/

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

	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER ARCH=arm \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE \
			VARIANT_DEFCONFIG=$VARIANT_DEFCONFIG \
			DEBUG_DEFCONFIG=$DEBUG_DEFCONFIG $KERNEL_DEFCONFIG\
			SELINUX_DEFCONFIG=$SELINUX_DEFCONFIG \
			SELINUX_LOG_DEFCONFIG=$SELINUX_LOG_DEFCONFIG || exit -1

	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER ARCH=arm \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE || exit -1

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
	echo " for KLEOS EUR OPEN use kleos_eur"
	echo " for ROSSA CHN CMCC use rossa_cmcc"
	echo " for ROSSA CHN CTC use rossa_ctc"
	echo " for ROSSA CHN CTC use rossa_spr"
	echo " for ROSSA CHN CTC use rossa_tfn"
	echo " for ROSSA CHN CTC use rossa_vzw"
	echo " for ROSSA USA AIO use rossa_aio"
	echo " for ROSSA AUS XSA use rossa_aus"
	echo " for FORTUNA CHN CMCC use fortuna_cmcc"
	echo " for FORTUNA CHN CTC use fortuna_ctc"
	echo " for FORTUNA CHN CU use fortuna_cu"
	echo " for FORTUNA SEA OPEN use fortuna_sea"
	echo " for FORTUNA3G EUR OPEN use fortuna3g_eur"
	echo " for GPEN_EUR_OPEN use gpen_eur"
	echo " for HEAT EUR OPEN use heat_eur"
	echo " for A3 CHN OPEN use a3_chnopen"
	echo " for A3 CHN CTC use a3_chntw"
	echo " for A3 CHN CTC use a3_chnctc"
	echo " for A3 EUR OPEN use a3lte_eur"
	echo " for A33G EUR OPEN use a33g_eur"
	echo " for O1 CHN OPEN use o1_chnopen"
	echo " for VIVALTO_AUS_XSA use vivalto_aus"
	echo " for VIVALTO_MEA use vivalto_mea"
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
