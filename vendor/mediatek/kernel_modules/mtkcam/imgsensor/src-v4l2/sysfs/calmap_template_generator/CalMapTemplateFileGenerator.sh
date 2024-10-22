#!/bin/bash

if [ "$1" == "-h" ]; then
    echo "Usage: CalMapTemplateFileGenerator.sh [rom] [position] [name]"
    echo -e "Generate the template files of cal map, if they do not already exist.\n"
    echo "[Arguments]"
    echo "  Use lowercase only!"
    echo "  rom      = [eeprom, otp]"
    echo "  position = [rear, front, rear2, rear3, rear4]"
    echo "  name     = [gw3, hi2021q, 4ha, ...]\n"
    echo "[Output folder]"
    echo -e "  current working directory"
    echo "[Output files]"
    echo "  header file: imgsensor_rom_position_name.h"
    echo -e "  c file: imgsensor_rom_position_name.c\n"
    echo "[Example]"
    echo "  ./CalMapTemplateFileGenerator.sh eeprom rear gw3"
        exit 0
fi

if (( $# != 3 ))
then
    echo "Invalid argument"
    echo "Try 'CalMapTemplateFileGenerator.sh -h' for more information."
    exit 1
fi

# TODO: Edit cal map information. Use lowercase only!
EEPROM_OTP=$1
SENSOR_POSITION=$2
SENSOR_NAME=$3

EEPROM_OTP_STR="eepromotp"
SENSOR_POSITION_STR="sensorposition"
SENSOR_NAME_STR="sensorname"

BASE_DIR=$(dirname "$0")
TEMPLATE_C_FILE_NAME="$BASE_DIR/imgsensor_eepromotp_sensorposition_sensorname.c"
TEMPLATE_HEADER_FILE_NAME="$BASE_DIR/imgsensor_eepromotp_sensorposition_sensorname.h"

OUT_C_FILE_NAME="./imgsensor_${EEPROM_OTP}_${SENSOR_POSITION}_${SENSOR_NAME}.c"
OUT_HEADER_FILE_NAME="./imgsensor_${EEPROM_OTP}_${SENSOR_POSITION}_${SENSOR_NAME}.h"

if [ -f ${OUT_C_FILE_NAME} ]
then
    echo ${OUT_C_FILE_NAME} already exists!
    exit 1
fi

if [ -f ${OUT_HEADER_FILE_NAME} ]
then
    echo ${OUT_HEADER_FILE_NAME} already exists!
    exit 1
fi

# Copy files
cp ${TEMPLATE_C_FILE_NAME} ${OUT_C_FILE_NAME}
cp ${TEMPLATE_HEADER_FILE_NAME} ${OUT_HEADER_FILE_NAME}

declare -A CHANGE_LIST
# Replace strings
CHANGE_LIST[$EEPROM_OTP_STR]=$EEPROM_OTP
CHANGE_LIST[$SENSOR_POSITION_STR]=$SENSOR_POSITION
CHANGE_LIST[$SENSOR_NAME_STR]=$SENSOR_NAME

for ORI_STR in "${!CHANGE_LIST[@]}"
do
sed -i s/${ORI_STR}/${CHANGE_LIST[$ORI_STR]}/g ${OUT_HEADER_FILE_NAME} ${OUT_C_FILE_NAME}
sed -i s/${ORI_STR^^}/${CHANGE_LIST[$ORI_STR]^^}/g ${OUT_HEADER_FILE_NAME} ${OUT_C_FILE_NAME}
done


echo ${OUT_C_FILE_NAME} is generated!
echo ${OUT_HEADER_FILE_NAME} is generated!
echo -e "\nThis program only changes name of file and variable."
echo "You need to edit the address of cal map."
