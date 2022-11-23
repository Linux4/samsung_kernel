#!/bin/bash
#
# Copyright (c) 2019, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# This script is mainly to compile QSSI targets. For other targets, usage
# of regular "make" is recommended.
#
# To run this script, do the following:
#
#  source build/envsetup.sh
#  lunch <target>-userdebug
#  ./vendor/qcom/opensource/core-utils/build/build.sh <make options>
#
# Note: For QSSI targets, this script cannot be used to compile individual images
#

###########################
# Build.sh versioning:
###########################
# build.sh supports '--version' option, returns the version number.
# Version number is based on the features/commands supported by it.
# The file - './vendor/qcom/opensource/core-utils/build/build.sh.versioned' indicates that build.sh
# supports versioning. So, it's required to first check for this file's existence before
# calling with '--version', since this versioning support didn't exist from the beginning of this script.
#
# Version 0:
#     Supports just the basic make commands (passes on all args like -j32 to the make command).
# Version 1:
#     Supports dist command as well - needed for target-files/ota generation.
#     Usage: ./build.sh dist -j32
#     This triggers make dist for qssi and target lunch, generates target-files, merges them
#     and triggers ota generation.
# Version 2:
#     Supports custom copy paths for dynamic patition images when compiled with dist.
#     option : --dp_images_path=<custom-copy-path>
# Version 3:
#     Supports segmenting the build into qssi only, target only and merge only steps and
#     enabling users to call specific steps or full build by giving no separate steps.
#     options: --qssi_only, --target_only, --merge_only
#     Usage: ./build.sh dist -j32 --qssi_only (for only qssi build) or ./build.sh dist -j32 (for full build)
#     Note: qssi_only and target_only options can be given together but merge_only should not be combined with
#           any other options.
# Version 4:
#     Supports lunch qssi variant to build qssi only images.
#     enables users to build standalone qssi images independent of any targets
#     option(s): --qssi_only
#     Usage: ./build.sh dist -j32 --qssi_only or ./build.sh dist -j32. Either way the outcome will be the same
#     Note: --target_only and --merge_only options will throw an error with lunch qssi variant
# Version 5:
#     Supports --tech_package argument for checking backward compatibility of Android.bp Modules.
#     options: Comma(',') seperated tech_packages name and give :golden at the end if we want to generate
#     Golden abi-dumps.
#     Note: By default it will run in checking mode.
# Version 6:
#     Supports uploading build data for analysis. It is enabled by default.
#     Use options: --dca_disable To disable collecting build data.
#     Usage: ./build.sh dist -j32 --dca_disable
# Version 7:
#     Supports rebuilding sepolicy with vendor side otatools.
#     option : --rebuild_sepolicy_with_vendor_otatools=<path-to-vendor-otatools>
#
BUILD_SH_VERSION=7
if [ "$1" == "--version" ]; then
    return $BUILD_SH_VERSION
    # Above return will work only if someone source'ed this script (which is expected, need to source the script).
    # Add extra exit 0 to ensure script doesn't proceed further (if someone didn't use source but passed --version)
    exit 0
fi
###########################

#Sanitize host toolsi
LS=`which ls`
LS=${LS:-ls}
MV=`which mv`
MV=${MV:-mv}
RM=`which rm`
RM=${RM:-rm}
CAT=`which cat`
CAT=${CAT:-cat}
CUT=`which cut`
CUT=${CUT:-cut}
REV=`which rev`
REV=${REV:-rev}
SED=`which sed`
SED=${SED:-sed}
DIFF=`which diff`
DIFF=${DIFF:-diff}
ECHO=`which echo`
ECHO=${ECHO:-echo}
FIND=`which find`
FIND=${FIND:-find}
GREP=`which grep`
GREP=${GREP:-grep}

MAKE_ARGUMENTS=()
MERGE_ONLY=0
QSSI_ONLY=0
TARGET_ONLY=0
FULL_BUILD=0
LIST_TECH_PACKAGE=""

# set below flag to 0 to disable build performance data collection.
DCA_ENABLED=1
DCA_OUT="out/dca"

while [[ $# -gt 0 ]]
    do
    arg="$1"
    case $arg in
        *merge_only)
            MERGE_ONLY=1
            shift # go to next argument
            ;;
        *qssi_only)
            QSSI_ONLY=1
            shift
            ;;
        *target_only)
            TARGET_ONLY=1
            shift
            ;;
        *dca_disable)
            DCA_ENABLED=0
            shift
            ;;
        --tech_package*)
            LIST_TECH_PACKAGE="$LIST_TECH_PACKAGE$arg"
            shift
            ;;
        *)  # all other option
            MAKE_ARGUMENTS+=("$1") # save it in an array to pass to make later
            shift
            ;;
    esac
done
set -- "${MAKE_ARGUMENTS[@]}" # restore the argument list ($@) to be set to MAKE_ARGUMENTS

# If none of the discrete options are passed, this is a full build
if [[ "$MERGE_ONLY" != 1 && "$QSSI_ONLY" != 1 && "$TARGET_ONLY" != 1 && "$TARGET_PRODUCT" != "qssi" ]]; then
    FULL_BUILD=1
fi

if [[ "$MERGE_ONLY" == 1 ]]; then
    if [[ "$QSSI_ONLY" == 1 || "$TARGET_ONLY" == 1 ]]; then
        echo "merge_only cannot be passed along with qssi_only or target_only options"
        exit 1
    fi
fi

if [[ "$TARGET_PRODUCT" == "qssi" ]]; then
    if [[ "$MERGE_ONLY" == 1 || "$TARGET_ONLY" == 1 ]]; then
        echo "merge_only and target_only options aren't supported for lunch qssi variant"
        exit 1
    fi
    QSSI_ONLY=1
fi

QSSI_TARGETS_LIST=("holi" "taro" "kalama" "parrot" "lahaina" "sdm710" "sdm845" "msmnile" "sm6150" "kona" "atoll" "trinket" "lito" "bengal" "qssi" "qssi_32" "qssi_32go" "bengal_32" "bengal_32go" "msm8937_lily")
QSSI_TARGET_FLAG=0
SKIP_ABI_CHECKS=true


case "$TARGET_PRODUCT" in
    *_32)
        TARGET_QSSI="qssi_32"
        ;;
    *_32go)
        TARGET_QSSI="qssi_32go"
        ;;
    *_lily)
        TARGET_QSSI="qssi_32go"
        ;;
    *)
        TARGET_QSSI="qssi"
        ;;
esac

NON_AB_TARGET_LIST=("qssi_32go" "bengal_32go" "msm8937_lily")
for NON_AB_TARGET in "${NON_AB_TARGET_LIST[@]}"
do
    if [ "$TARGET_PRODUCT" == "$NON_AB_TARGET" ]; then
        log "${TARGET_PRODUCT} found in Non-A/B Target List"
        ENABLE_AB=false
        break
    fi
done

# Default A/B configuration flag for all QSSI targets (not used for legacy targets).
ENABLE_AB=${ENABLE_AB:-true}
ARGS="$@"
QSSI_ARGS="$ARGS ENABLE_AB=$ENABLE_AB"

#This flag control system_ext logical partition enablement
SYSTEMEXT_SEPARATE_PARTITION_ENABLE=true
QSSI_ARGS="$QSSI_ARGS SYSTEMEXT_SEPARATE_PARTITION_ENABLE=$SYSTEMEXT_SEPARATE_PARTITION_ENABLE"

# OTA/Dist related variables
#This flag control dynamic partition enablement
BOARD_DYNAMIC_PARTITION_ENABLE=false

# Virtual-AB feature flag
ENABLE_VIRTUAL_AB=false

# OTA/Dist related variaibles
QSSI_OUT="out/target/product/$TARGET_QSSI"
DIST_COMMAND="dist"
DIST_ENABLED=false
QSSI_ARGS_WITHOUT_DIST=""
DIST_DIR="out/dist"
MERGED_TARGET_FILES="$DIST_DIR/merged-qssi_${TARGET_PRODUCT}-target_files.zip"
MERGED_OTA_ZIP="$DIST_DIR/merged-qssi_${TARGET_PRODUCT}-ota.zip"
DIST_ENABLED_TARGET_LIST=("holi" "taro" "kalama" "parrot" "lahaina" "kona" "sdm710" "sdm845" "msmnile" "sm6150" "trinket" "lito" "bengal" "atoll" "qssi" "qssi_32" "qssi_32go" "bengal_32" "bengal_32go" "msm8937_lily")
VIRTUAL_AB_ENABLED_TARGET_LIST=("kona" "lito" "taro" "kalama" "parrot" "lahaina")
DYNAMIC_PARTITION_ENABLED_TARGET_LIST=("holi" "taro" "kalama" "parrot" "lahaina" "kona" "msmnile" "sdm710" "lito" "trinket" "atoll" "qssi" "qssi_32" "qssi_32go" "bengal" "bengal_32" "bengal_32go" "sm6150" "msm8937_lily")
DYNAMIC_PARTITIONS_IMAGES_PATH=$OUT
DP_IMAGES_OVERRIDE=false

OTATOOLS_DIR="$(mktemp --directory)"
MERGED_TARGET_FILES_DIR="$(mktemp --directory)"
cleanup() {
    rm -rf $OTATOOLS_DIR $MERGED_TARGET_FILES_DIR
}
trap cleanup EXIT

function log() {
    ${ECHO} "============================================"
    ${ECHO} "[build.sh]: $@"
    ${ECHO} "============================================"
}

for DP_TARGET in "${DYNAMIC_PARTITION_ENABLED_TARGET_LIST[@]}"
do
    if [ "$TARGET_PRODUCT" == "$DP_TARGET" ]; then
        log "${TARGET_PRODUCT} found in Dynamic Parition Enablement List"
        BOARD_DYNAMIC_PARTITION_ENABLE=true
        break
    fi
done

for VIRTUAL_AB_TARGET in "${VIRTUAL_AB_ENABLED_TARGET_LIST[@]}"
do
    if [ "$TARGET_PRODUCT" == "$VIRTUAL_AB_TARGET" ]; then
        ENABLE_VIRTUAL_AB=true
        break
    fi
done

# Pass Dynamic Partition and virtual-ab flags
QSSI_ARGS="$QSSI_ARGS BOARD_DYNAMIC_PARTITION_ENABLE=$BOARD_DYNAMIC_PARTITION_ENABLE ENABLE_VIRTUAL_AB=$ENABLE_VIRTUAL_AB"

# Set Shipping API level on target basis.
SHIPPING_API_P="28"
SHIPPING_API_Q="29"
SHIPPING_API_P_TARGET_LIST=("sdm845")
SHIPPING_API_LEVEL=$SHIPPING_API_Q
for P_API_TARGET in "${SHIPPING_API_P_TARGET_LIST[@]}"
do
    if [ "$TARGET_PRODUCT" == "$P_API_TARGET" ]; then
        SHIPPING_API_LEVEL=$SHIPPING_API_P
        break
    fi
done
QSSI_ARGS="$QSSI_ARGS SHIPPING_API_LEVEL=$SHIPPING_API_LEVEL"

for ARG in $QSSI_ARGS
do
    if [ "$ARG" == "$DIST_COMMAND" ]; then
        DIST_ENABLED=true
    elif [ "$ARG" == "BUILDING_WITH_VSDK=true" ]; then
        BUILDING_WITH_VSDK=true
    elif [[ "$ARG" == *"DISABLED_VSDK_SNAPSHOTS"* ]]; then
        DISABLED_VSDK_SNAPSHOTS_ARG=$ARG
    elif [[ "$ARG" == *"--dp_images_path"* ]]; then
        DP_IMAGES_OVERRIDE=true
        DYNAMIC_PARTITIONS_IMAGES_PATH=$(${ECHO} "$ARG" | ${CUT} -d'=' -f 2)
    elif [[ "$ARG" == *"--rebuild_sepolicy_with_vendor_otatools"* ]]; then
        REBUILD_SEPOLICY=true
        VENDOR_OTATOOLS=$(${ECHO} "$ARG" | ${CUT} -d'=' -f 2)
    else
        QSSI_ARGS_WITHOUT_DIST="$QSSI_ARGS_WITHOUT_DIST $ARG"
    fi
done

if [ "$BUILDING_WITH_VSDK" = true ]; then
  TARGET_BUILD_UNBUNDLED_IMAGE_ARG="TARGET_BUILD_UNBUNDLED_IMAGE=true"
  DISABLED_VSDK_SNAPSHOTS=(${DISABLED_VSDK_SNAPSHOTS_ARG//=/ })
  IFS=',';for i in `echo "${DISABLED_VSDK_SNAPSHOTS[1]}"`;
  do
      if [ "$i" == "java" ]; then
          TARGET_BUILD_UNBUNDLED_IMAGE_ARG=""
      fi
  done
  QSSI_ARGS="$QSSI_ARGS $TARGET_BUILD_UNBUNDLED_IMAGE_ARG"
  unset IFS;
fi

#Strip image_path if present
if [ "$DP_IMAGES_OVERRIDE" = true ]; then
    QSSI_ARGS=${QSSI_ARGS//"--dp_images_path=$DYNAMIC_PARTITIONS_IMAGES_PATH"/}
fi

if [ "$REBUILD_SEPOLICY" = true ]; then
    QSSI_ARGS=${QSSI_ARGS//"--rebuild_sepolicy_with_vendor_otatools=$VENDOR_OTATOOLS"/}
fi

# Check if dist is supported on this target (yet) or not, and override DIST_ENABLED flag.
IS_DIST_ENABLED_TARGET=false
for DIST_TARGET in "${DIST_ENABLED_TARGET_LIST[@]}"
do
    if [ "$TARGET_PRODUCT" == "$DIST_TARGET" ]; then
        IS_DIST_ENABLED_TARGET=true
        break
    fi
done

# Disable dist if it's not supported (yet).
if [ "$IS_DIST_ENABLED_TARGET" = false ] && [ "$DIST_ENABLED" = true ]; then
    QSSI_ARGS="$QSSI_ARGS_WITHOUT_DIST"
    DIST_ENABLED=false
    log "Dist not (yet) enabled for $TARGET_PRODUCT"
fi

function check_return_value () {
    retVal=$1
    command=$2
    if [ $retVal -ne 0 ]; then
        log "FAILED: $command"
        exit $retVal
    fi
}

function command () {
    command=$@
    log "Command: \"$command\""
    time $command
    retVal=$?
    check_return_value $retVal "$command"
}

function check_if_file_exists () {
    if [[ ! -f "$1" ]]; then
        log "Could not find the file: \"$1\", aborting.."
        exit 1
    fi
}

function generate_dynamic_partition_images () {
    log "Generate Dynamic Partition Images for ${TARGET_PRODUCT}"
    # Handling for Dist Enabled targets
    # super.img/super_empty generation
    if [ "$DIST_ENABLED" = true ]; then
       if [ "$DP_IMAGES_OVERRIDE" = true ]; then
           command "mkdir -p $DYNAMIC_PARTITIONS_IMAGES_PATH"
       fi
       if [ -f "$QSSI_OUT/vbmeta_system.img" ]; then
           command "cp $QSSI_OUT/vbmeta_system.img $OUT/"
       fi
       command "unzip -jo -DD $MERGED_TARGET_FILES IMAGES/*.img -x IMAGES/userdata.img -d $DYNAMIC_PARTITIONS_IMAGES_PATH"
       # TEMP FIX
       command "unzip $MERGED_TARGET_FILES IMAGES/* META/* */build.prop -d $MERGED_TARGET_FILES_DIR"
       # END TEMP FIX
       command "$OTATOOLS_DIR/bin/build_super_image --path $OTATOOLS_DIR $MERGED_TARGET_FILES_DIR $DYNAMIC_PARTITIONS_IMAGES_PATH/super.img"
       command "${RM} -rf ${MERGED_TARGET_FILES_DIR}"
    else
        command "cp $QSSI_OUT/vbmeta_system.img $OUT/"
        command "mkdir -p out/${TARGET_PRODUCT}_dpm"
        check_if_file_exists "$QSSI_OUT/dynamic_partition_metadata.txt"
        check_if_file_exists "$OUT/dynamic_partition_metadata.txt"
        MERGED_DPM_PATH="out/${TARGET_PRODUCT}_dpm/dynamic_partition_metadata.txt"
        MERGE_DYNAMIC_PARTITION_INFO_COMMAND="./build/tools/releasetools/merge_dynamic_partition_metadata.py \
            --qssi-dpm-file=$QSSI_OUT/dynamic_partition_metadata.txt \
            --target-dpm-file=$OUT/dynamic_partition_metadata.txt \
            --merged-dpm-file=$MERGED_DPM_PATH"
        # Temporarily change permission of merge_dynamic_partition_metadata.py till proper fix is merged.
        command "chmod 755 build/tools/releasetools/merge_dynamic_partition_metadata.py"
        command "$MERGE_DYNAMIC_PARTITION_INFO_COMMAND"
        command "./build/tools/releasetools/build_super_image.py $MERGED_DPM_PATH $OUT/super_empty.img"
    fi
}

function generate_ota_zip () {
    log "Processing dist/ota commands:"

    FRAMEWORK_TARGET_FILES="$(find $DIST_DIR -name "qssi*-target_files-*.zip" -print)"
    log "FRAMEWORK_TARGET_FILES=$FRAMEWORK_TARGET_FILES"
    check_if_file_exists "$FRAMEWORK_TARGET_FILES"

    VENDOR_TARGET_FILES="$(find $DIST_DIR -name "${TARGET_PRODUCT}*-target_files-*.zip" -print)"
    log "VENDOR_TARGET_FILES=$VENDOR_TARGET_FILES"
    check_if_file_exists "$VENDOR_TARGET_FILES"

    log "MERGED_TARGET_FILES=$MERGED_TARGET_FILES"

    check_if_file_exists "$DIST_DIR/merge_config_system_misc_info_keys"
    check_if_file_exists "$DIST_DIR/merge_config_system_item_list"
    check_if_file_exists "$DIST_DIR/merge_config_other_item_list"

#Remove the entries in merge config files in dist folder when disabling system_ext logical partition
    if [ "$SYSTEMEXT_SEPARATE_PARTITION_ENABLE" = false ]; then
        sed -i '/^SYSTEM_EXT/d' $DIST_DIR/merge_config_system_item_list
        sed -i '/^system_ext/d' $DIST_DIR/merge_config_system_misc_info_keys
    fi

    check_if_file_exists "$DIST_DIR/otatools.zip"
    log "Unpacking otatools.zip to $OTATOOLS_DIR"
    UNZIP_OTATOOLS_COMMAND="unzip -d $OTATOOLS_DIR $DIST_DIR/otatools.zip"
    command "$UNZIP_OTATOOLS_COMMAND"

    MERGE_TARGET_FILES_COMMAND="$OTATOOLS_DIR/bin/merge_target_files \
        --path $OTATOOLS_DIR \
        --framework-target-files $FRAMEWORK_TARGET_FILES \
        --vendor-target-files $VENDOR_TARGET_FILES \
        --output-target-files $MERGED_TARGET_FILES \
        --framework-misc-info-keys $DIST_DIR/merge_config_system_misc_info_keys \
        --framework-item-list $DIST_DIR/merge_config_system_item_list \
        --vendor-item-list $DIST_DIR/merge_config_other_item_list \
        --output-ota  $MERGED_OTA_ZIP --allow-duplicate-apkapex-keys"

    if [ "$ENABLE_AB" = false ]; then
        MERGE_TARGET_FILES_COMMAND="$MERGE_TARGET_FILES_COMMAND --rebuild_recovery"
    fi

    if [ "$REBUILD_SEPOLICY" = true ]; then
        MERGE_TARGET_FILES_COMMAND="$MERGE_TARGET_FILES_COMMAND --rebuild-sepolicy --vendor-otatools=$VENDOR_OTATOOLS"
    fi

    command "$MERGE_TARGET_FILES_COMMAND"
}

function run_qiifa_initialization() {
    QIIFA_IN_SCRIPT="$QCPATH/commonsys-intf/QIIFA-fwk/qiifa_initialization.py"
    QIIFA_TP_SCRIPT="$QCPATH/commonsys-intf/QIIFA-fwk/qiifa_techpackage_initialization.py"
    QIIFA_SCRIPT = ""
    if [[ -f $QIIFA_IN_SCRIPT ]];then
     QIIFA_SCRIPT=$QIIFA_IN_SCRIPT
    elif [[ -f $QIIFA_TP_SCRIPT ]];then
      QIIFA_SCRIPT=$QIIFA_TP_SCRIPT
    fi
    IFS=':' read -ra ADDR <<< "${LIST_TECH_PACKAGE:15}"
    if [[ -f $QIIFA_SCRIPT ]]; then
     command "python $QIIFA_SCRIPT ${ADDR[0]}"
    fi
}

function run_qiifa_for_techpackage () {
    QIIFA_SCRIPT="$QCPATH/commonsys-intf/QIIFA-fwk/qiifa_main.py"
    if [ -f $QIIFA_SCRIPT ]; then
     command "python $QIIFA_SCRIPT --create techpackage --enforced 1"
    fi
}

function run_qiifa () {
    IFS=':' read -ra ADDR <<< "${LIST_TECH_PACKAGE:15}"
    if [[ -n ${ADDR[1]} && "${ADDR[1]}" == "golden" ]]; then
      command "run_qiifa_for_techpackage"
    fi
    QIIFA_SCRIPT="$QCPATH/commonsys-intf/QIIFA-fwk/qiifa_main.py"
    if [ -f $QIIFA_SCRIPT ]; then
     command "python $QIIFA_SCRIPT --type all --enforced 1"
    fi
}

function build_qssi_only () {
    command "source build/envsetup.sh"
    command "python -B $QTI_BUILDTOOLS_DIR/build/makefile-violation-scanner.py"
    command "lunch ${TARGET_QSSI}-${TARGET_BUILD_VARIANT}"
    command "make $QSSI_ARGS"
    COMMONSYS_INTF_SCRIPT="$QTI_BUILDTOOLS_DIR/build/commonsys_intf_checker.py"
    if [ -f $COMMONSYS_INTF_SCRIPT ];then
      command "python $COMMONSYS_INTF_SCRIPT"
    fi
}

function build_target_only () {
    command "source build/envsetup.sh"
    command "lunch ${TARGET}-${TARGET_BUILD_VARIANT}"
    command "python -B $QTI_BUILDTOOLS_DIR/build/makefile-violation-scanner.py"
    QSSI_ARGS="$QSSI_ARGS SKIP_ABI_CHECKS=$SKIP_ABI_CHECKS"
    command "run_qiifa_initialization"
    command "make $QSSI_ARGS"
    if [ "$BUILDING_WITH_VSDK" = true ]; then
        command "cp vendor/qcom/otatools_snapshot/otatools.zip out/dist/otatools.zip"
    fi
    command "run_qiifa"
}

function merge_only () {
    # DIST/OTA specific operations:
    if [ "$DIST_ENABLED" = true ]; then
        generate_ota_zip
    fi
    # Handle dynamic partition case and generate images
    if [ "$BOARD_DYNAMIC_PARTITION_ENABLE" = true ]; then
        generate_dynamic_partition_images
    fi
}

function full_build () {
    build_qssi_only
    build_target_only
    # Copy Qssi system|product.img to target folder so that all images can be picked up from one folder
    command "cp $QSSI_OUT/system.img $OUT/"
    if [ -f  $QSSI_OUT/product.img ]; then
        command "cp $QSSI_OUT/product.img $OUT/"
    fi
    if [ -f  $QSSI_OUT/system_ext.img ]; then
        command "cp $QSSI_OUT/system_ext.img $OUT/"
    fi
    merge_only
}

function run_dca() {
    # Run the command in background and collect build data.
    DCA_SCRIPT="$QCPATH/common-noship/scripts/analytics_data_collection.sh"
    if [[ -f $DCA_SCRIPT ]]; then
        bash $DCA_SCRIPT > $DCA_OUT/dca.log 2>&1 &
    fi
}

# Check if qssi is supported on this target or not.
for QSSI_TARGET in "${QSSI_TARGETS_LIST[@]}"
do
    if [ "$TARGET_PRODUCT" == "$QSSI_TARGET" ]; then
        QSSI_TARGET_FLAG=1
        break
    fi
done

# For non-QSSI targets
if [ $QSSI_TARGET_FLAG -eq 0 ]; then
    log "${TARGET_PRODUCT} is not a QSSI target. Using legacy build process for compilation..."
    command "source build/envsetup.sh"
    command "make $ARGS"
else # For QSSI targets
    log "Building Android using build.sh for ${TARGET_PRODUCT}..."
    log "QSSI_ARGS=\"$QSSI_ARGS\""
    log "DIST_ENABLED=$DIST_ENABLED, ENABLE_AB=$ENABLE_AB"
    TARGET="$TARGET_PRODUCT"
    export TARGET_PARENT_VENDOR="$TARGET_PRODUCT"
    if [[ "$FULL_BUILD" -eq 1 ]]; then
        log "Executing a full build ..."
        full_build
    fi

    if [[ "$QSSI_ONLY" -eq 1 ]]; then
        log "Executing a QSSI only build ..."
        build_qssi_only
        if [[ "$TARGET_PRODUCT" == "qssi" ]]; then
            run_qiifa
        else
            log "Skipping QIIFA Validation for ${TARGET_PRODUCT}..."
        fi
    fi

    if [[ "$TARGET_ONLY" -eq 1 ]]; then
        log "Executing a target only build for $TARGET_PRODUCT ..."
        build_target_only
    fi

    if [[ "$MERGE_ONLY" -eq 1 ]]; then
        log "Executing a merge only operation ..."
        merge_only
    fi
    if [[ ${DCA_ENABLED} -eq 1 ]]; then
        log "Capturing build performance data..."
        mkdir -p $DCA_OUT
        run_dca
    fi
fi
