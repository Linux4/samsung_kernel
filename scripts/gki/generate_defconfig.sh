#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2020, The Linux Foundation. All rights reserved.

# Script to generate a defconfig variant based on the input

usage() {
	echo "Usage: $0 <platform_defconfig_variant>"
	echo "Variants: <platform>-gki_defconfig, <platform>-qgki_defconfig, <platform>-consolidate_defconfig and <platform>-qgki-debug_defconfig"
	echo "Example: $0 lahaina-gki_defconfig"
	exit 1
}

if [ -z "$1" ]; then
	echo "Error: Failed to pass input argument"
	usage
fi

SCRIPTS_ROOT=$(readlink -f $(dirname $0)/)

TEMP_DEF_NAME=`echo $1 | sed -r "s/_defconfig$//"`
DEF_VARIANT=`echo ${TEMP_DEF_NAME} | sed -r "s/.*-//"`
PLATFORM_NAME=`echo ${TEMP_DEF_NAME} | sed -r "s/-.*$//"`

PLATFORM_NAME=`echo $PLATFORM_NAME | sed "s/vendor\///g"`

REQUIRED_DEFCONFIG=`echo $1 | sed "s/vendor\///g"`

# We should be in the kernel root after the envsetup
if [[  "${REQUIRED_DEFCONFIG}" != *"gki"* ]]; then
	source ${SCRIPTS_ROOT}/envsetup.sh $PLATFORM_NAME generic_defconfig
else
	source ${SCRIPTS_ROOT}/envsetup.sh $PLATFORM_NAME
fi

KERN_MAKE_ARGS="ARCH=$ARCH \
		CROSS_COMPILE=$CROSS_COMPILE \
		REAL_CC=$REAL_CC \
		CLANG_TRIPLE=$CLANG_TRIPLE \
		HOSTCC=$HOSTCC \
		HOSTLD=$HOSTLD \
		HOSTAR=$HOSTAR \
		LD=$LD \
		"

# Allyes fragment temporarily created on GKI config fragment
QCOM_GKI_ALLYES_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_ALLYES_GKI.config

if [[ "${REQUIRED_DEFCONFIG}" == *"gki"* ]]; then
if [ ! -f "${QCOM_GKI_FRAG}" ]; then
	echo "Error: Invalid input"
	usage
fi
fi

FINAL_DEFCONFIG_BLEND=""

#Samsung defconfigs
SAMSUNG_DEFCONFIG_BLEND=""
TEMP_VARIANT_NAME=`echo $2 | sed -r "s/.*vendor//"`
VARIANT_DEFCONFIG=${CONFIGS_DIR}${TEMP_VARIANT_NAME}

TEMP_VARIANT_GKI_NAME=`echo $3 | sed -r "s/.*vendor//"`
VARIANT_GKI_DEFCONFIG=${CONFIGS_DIR}${TEMP_VARIANT_GKI_NAME}

SAMSUNG_DEFCONFIG_BLEND="${VARIANT_DEFCONFIG}"

if [[ "${PROJECT_FULLNAME}" == "b2q"* ]]; then
if [[ "${SEC_BUILD_OPTION_HW_REVISION}" == "00" ]]; then
	echo "" >> ${VARIANT_DEFCONFIG}
	echo "# WIFI HW option set 00" >> ${VARIANT_DEFCONFIG}
	echo "# CONFIG_CNSS_QCA6490 is not set" >> ${VARIANT_DEFCONFIG}
	echo "CONFIG_CNSS_QCA6390=y" >> ${VARIANT_DEFCONFIG}
	echo "" >> ${VARIANT_GKI_DEFCONFIG}
	echo "# WIFI HW option set 00" >> ${VARIANT_GKI_DEFCONFIG}
	echo "# CONFIG_CNSS_QCA6490 is not set" >> ${VARIANT_GKI_DEFCONFIG}
	echo "CONFIG_CNSS_QCA6390=y" >> ${VARIANT_GKI_DEFCONFIG}
fi
fi

if [[ "${TARGET_BUILD_VARIANT}" == "eng" ]]; then
	SAMSUNG_DEFCONFIG_BLEND+=" $SEC_ENG_FRAG"
elif [[ "${TARGET_BUILD_VARIANT}" == "userdebug" ]]; then
	SAMSUNG_DEFCONFIG_BLEND+=" $SEC_USERDEBUG_FRAG"
fi

SAMSUNG_DEFCONFIG_BLEND+=" $SEC_COMMON_FRAG"

echo "Samsung generate_defconfig config ${SAMSUNG_DEFCONFIG_BLEND}"
if [[ "${REQUIRED_DEFCONFIG}" == *"qgki"* ]]; then
	FINAL_DEFCONFIG_BLEND+=" $SAMSUNG_DEFCONFIG_BLEND"
fi

case "$REQUIRED_DEFCONFIG" in
	${PLATFORM_NAME}-qgki-debug_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $QCOM_DEBUG_FRAG"
		;&	# Intentional fallthrough
	${PLATFORM_NAME}-qgki-consolidate_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $QCOM_CONSOLIDATE_FRAG"
		;&	# Intentional fallthrough
	${PLATFORM_NAME}-qgki_defconfig )
		# DEBUG_FS fragment.
		FINAL_DEFCONFIG_BLEND+=" $QCOM_DEBUG_FS_FRAG"

		FINAL_DEFCONFIG_BLEND+=" $QCOM_QGKI_FRAG"
		${SCRIPTS_ROOT}/fragment_allyesconfig.sh $QCOM_GKI_FRAG $QCOM_GKI_ALLYES_FRAG
		FINAL_DEFCONFIG_BLEND+=" $QCOM_GKI_ALLYES_FRAG "
		;;
	${PLATFORM_NAME}-gki_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $VARIANT_GKI_DEFCONFIG"
		FINAL_DEFCONFIG_BLEND+=" $SEC_GKI_COMMON_FRAG"
		FINAL_DEFCONFIG_BLEND+=" $QCOM_GKI_FRAG "
		;;
	${PLATFORM_NAME}-debug_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $QCOM_GENERIC_DEBUG_FRAG "
		;&
	${PLATFORM_NAME}_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $QCOM_GENERIC_PERF_FRAG "
		;;
esac

FINAL_DEFCONFIG_BLEND+=${BASE_DEFCONFIG}

# Reverse the order of the configs for the override to work properly
# Correct order is base_defconfig GKI.config QGKI.config consolidate.config debug.config
FINAL_DEFCONFIG_BLEND=`echo "${FINAL_DEFCONFIG_BLEND}" | awk '{ for (i=NF; i>1; i--) printf("%s ",$i); print $1; }'`

echo "defconfig blend for $REQUIRED_DEFCONFIG: $FINAL_DEFCONFIG_BLEND"

MAKE_ARGS=$KERN_MAKE_ARGS \
MAKE_PATH=${MAKE_PATH} \
	${KERN_SRC}/scripts/kconfig/merge_config.sh $FINAL_DEFCONFIG_BLEND
${MAKE_PATH}make $KERN_MAKE_ARGS savedefconfig
mv defconfig $CONFIGS_DIR/$REQUIRED_DEFCONFIG

# Cleanup the allyes config fragment and other generated files
rm -rf $QCOM_GKI_ALLYES_FRAG .config include/config/ include/generated/ arch/$ARCH/include/generated/
