#!/bin/bash
# MSM8998 kernel build script v0.5

BUILD_COMMAND=$1
if [ "$BUILD_COMMAND" == "dreamqlte_usa_vzw" ]; then
	PRODUCT_NAME=dreamqltevzw;
	BOARD_NAME=SRPQC03A000KE;
	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "dream2qlte_usa_vzw" ]; then
	PRODUCT_NAME=dream2qltevzw;
	BOARD_NAME=SRPQC03A000KE;
	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "dreamqlte_usa_single" ]; then
	PRODUCT_NAME=dreamqltesingle;
	BOARD_NAME=SRPQC03A000KE;
	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "dream2qlte_usa_single" ]; then
	PRODUCT_NAME=dream2qltesingle;
	BOARD_NAME=SRPQC03A000KE;
	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "dreamqlte_chn_open" ]; then
	PRODUCT_NAME=dreamqltezc;
	BOARD_NAME=SRPQC03A000KE;
	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "dream2qlte_chn_open" ]; then
	PRODUCT_NAME=dream2qltezc;
	BOARD_NAME=SRPQC03A000KE;
	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "greatqlte_usa_single" ]; then
	PRODUCT_NAME=greatqltesq;
	BOARD_NAME=SRPQC03A000KE;
	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4llte_eur_open" ]; then
	PRODUCT_NAME=gts4lltexx;
#	BOARD_NAME=SRPQC03A000KE;
#	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4llte_eur_ldu" ]; then
        PRODUCT_NAME=gts4llteldu;
#       BOARD_NAME=SRPQC03A000KE;
#       KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4llte_usa_att" ]; then
        PRODUCT_NAME=gts4llteuc;
#       BOARD_NAME=SRPQC03A000KE;
#       KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4llte_usa_spr" ]; then
        PRODUCT_NAME=gts4lltespr;
#       BOARD_NAME=SRPQC03A000KE;
#       KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4llte_chn_open" ]; then
	PRODUCT_NAME=gts4lltezc;
#	BOARD_NAME=SRPQC03A000KE;
#	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4llte_chn_hk" ]; then
	PRODUCT_NAME=gts4lltezh;
#	BOARD_NAME=SRPQC03A000KE;
#	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4llte_kor_open" ]; then
	PRODUCT_NAME=gts4lltekx;
#	BOARD_NAME=SRPQC03A000KE;
#	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4llte_sea_open" ]; then
	PRODUCT_NAME=gts4lltedx;
#	BOARD_NAME=SRPQC03A000KE;
#	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4lwifi_eur_open" ]; then
	PRODUCT_NAME=gts4lwifixx;
#	BOARD_NAME=SRPQC03A000KE;
#	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4lwifi_eur_ldu" ]; then
        PRODUCT_NAME=gts4lwifildu;
#       BOARD_NAME=SRPQC03A000KE;
#       KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4lwifi_chn_open" ]; then
	PRODUCT_NAME=gts4lwifizc;
#	BOARD_NAME=SRPQC03A000KE;
#	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "gts4lwifi_cis_ser" ]; then
	PRODUCT_NAME=gts4lwifiser;
#	BOARD_NAME=SRPQC03A000KE;
#	KASLR_DEFCONFIG=kaslr_defconfig
elif [ "$BUILD_COMMAND" == "kellylte_chn_open" ]; then
          PRODUCT_NAME=kellylteopen;
		KASLR_DEFCONFIG=kaslr_defconfig
else
	#default product
        PRODUCT_NAME=dreamqltevzw;
	BOARD_NAME=SRPQC03A000KE;
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

KERNEL_DEFCONFIG=msm8998_sec_defconfig
DEBUG_DEFCONFIG=msm8998_sec_eng_defconfig
# SELINUX_DEFCONFIG=selinux_defconfig
# SELINUX_LOG_DEFCONFIG=selinux_log_defconfig

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

if [ "$BUILD_COMMAND" == "dreamqlte_usa_vzw" ]; then
	SIGN_MODEL=SM-G950U_NA_VZW_USA0
elif [ "$BUILD_COMMAND" == "dream2qlte_usa_vzw" ]; then
	SIGN_MODEL=SM-G955U_NA_VZW_USA0
elif [ "$BUILD_COMMAND" == "dreamqlte_usa_single" ]; then
	SIGN_MODEL=SM-G950U_NA_VZW_USA0
elif [ "$BUILD_COMMAND" == "dream2qlte_usa_single" ]; then
	SIGN_MODEL=SM-G955U_NA_VZW_USA0
elif [ "$BUILD_COMMAND" == "dreamqlte_chn_open" ]; then
	SIGN_MODEL=SM-G9500_CHN_CHC_CHN0
elif [ "$BUILD_COMMAND" == "dream2qlte_chn_open" ]; then
	SIGN_MODEL=SM-G9550_CHN_CHC_CHN0
elif [ "$BUILD_COMMAND" == "greatqlte_usa_single" ]; then
	SIGN_MODEL=SM-N950U_NA_VZW_USA0
else
	SIGN_MODEL=
fi

MODEL=${BUILD_COMMAND%%_*}
TEMP=${BUILD_COMMAND#*_}
REGION=${TEMP%%_*}
CARRIER=${TEMP##*_}

VARIANT=${CARRIER}
PROJECT_NAME=${VARIANT}

if [ "$BUILD_OPTION_HW_REVISION" == "" ] ; then
    VARIANT_DEFCONFIG=msm8998_sec_${BUILD_COMMAND}_defconfig
else
    VARIANT_DEFCONFIG=msm8998_sec_${BUILD_COMMAND}_rev${BUILD_OPTION_HW_REVISION}_defconfig
fi

CERTIFICATION=NONCERT

case $1 in
		clean)
		echo "Not support... remove kernel out directory by yourself"
		exit 1
		;;

		*)

		BOARD_KERNEL_BASE=0x00000000
		BOARD_KERNEL_PAGESIZE=4096
		BOARD_KERNEL_TAGS_OFFSET=0x01E00000
		BOARD_RAMDISK_OFFSET=0x02000000
		BOARD_KERNEL_CMDLINE="console=ttyMSM0,115200,n8 androidboot.console=ttyMSM0 earlycon=msm_serial_dm,0xc1b0000 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x37 ehci-hcd.park=3 lpm_levels.sleep_disabled=1 sched_enable_hmp=1 sched_enable_power_aware=1 androidboot.selinux=permissive service_locator.enable=1"

		mkdir -p $BUILD_KERNEL_OUT_DIR
		;;

esac

KERNEL_ZIMG=$BUILD_KERNEL_OUT_DIR/arch/arm64/boot/Image.gz-dtb
DTC=$BUILD_KERNEL_OUT_DIR/scripts/dtc/dtc

FUNC_CLEAN_DTB()
{
	if ! [ -d $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/samsung ] ; then
		echo "no directory : "$BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/samsung""
	else
		echo "rm files in : "$BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/samsung/*.dtb""
		rm $BUILD_KERNEL_OUT_DIR/arch/arm64/boot/dts/samsung/*.dtb
	fi
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
	echo "build kaslr defconfig="$KASLR_DEFCONFIG""

        if [ "$BUILD_COMMAND" == "" ]; then
                SECFUNC_PRINT_HELP;
                exit -1;
        fi

	FUNC_CLEAN_DTB

	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER ARCH=arm64 \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE KCFLAGS=-mno-android \
			VARIANT_DEFCONFIG=$VARIANT_DEFCONFIG \
			DEBUG_DEFCONFIG=$DEBUG_DEFCONFIG $KERNEL_DEFCONFIG \
			SELINUX_DEFCONFIG=$SELINUX_DEFCONFIG \
			SELINUX_LOG_DEFCONFIG=$SELINUX_LOG_DEFCONFIG \
			KASLR_DEFCONFIG=$KASLR_DEFCONFIG || exit -1

	make -C $BUILD_KERNEL_DIR O=$BUILD_KERNEL_OUT_DIR -j$BUILD_JOB_NUMBER ARCH=arm64 \
			CROSS_COMPILE=$BUILD_CROSS_COMPILE KCFLAGS=-mno-android || exit -1


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
	MKBOOTIMGTOOL=$BUILD_KERNEL_DIR/tools/mkbootimg

	if ! [ -e $MKBOOTIMGTOOL ] ; then
		if ! [ -d $BUILD_ROOT_DIR/android/out/host/linux-x86/bin ] ; then
			mkdir -p $BUILD_ROOT_DIR/android/out/host/linux-x86/bin
		fi
		cp $BUILD_KERNEL_DIR/tools/mkbootimg $MKBOOTIMGTOOL
	fi

	echo "Making boot.img ..."
	echo "	$MKBOOTIMGTOOL --kernel $KERNEL_ZIMG \
			--ramdisk $PRODUCT_OUT/ramdisk.img \
			--output $PRODUCT_OUT/boot.img \
			--cmdline "$BOARD_KERNEL_CMDLINE" \
			--base $BOARD_KERNEL_BASE \
			--pagesize $BOARD_KERNEL_PAGESIZE \
			--ramdisk_offset $BOARD_RAMDISK_OFFSET \
			--tags_offset $BOARD_KERNEL_TAGS_OFFSET"



	$MKBOOTIMGTOOL --kernel $KERNEL_ZIMG \
			--ramdisk $PRODUCT_OUT/ramdisk.img \
			--output $PRODUCT_OUT/boot.img \
			--cmdline "$BOARD_KERNEL_CMDLINE" \
			--base $BOARD_KERNEL_BASE \
			--pagesize $BOARD_KERNEL_PAGESIZE \
			--ramdisk_offset $BOARD_RAMDISK_OFFSET \
			--tags_offset $BOARD_KERNEL_TAGS_OFFSET 

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
	echo "      dreamqlte_usa_vzw"
	echo "      dream2qlte_usa_vzw"
	echo "      dreamqlte_usa_single"
	echo "      dream2qlte_usa_single"
	echo "      dreamqlte_chn_open"
	echo "      dream2qlte_chn_open"
	echo "      greatqlte_usa_single"
	echo "      gts4llte_eur_open"
	echo "      gts4llte_eur_ldu"
	echo "      gts4llte_usa_att"
	echo "      gts4llte_usa_spr"
        echo "      gts4llte_chn_open"
        echo "      gts4llte_chn_hk"
	echo "      gts4llte_kor_open"
	echo "      gts4llte_sea_open"
	echo "      gts4lwifi_eur_open"
        echo "      gts4lwifi_eur_ldu"
        echo "      gts4lwifi_chn_open"
		echo "      gts4lwifi_cis_ser"
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

