#!/bin/bash
PLATFORM=$1
PROJECT=$2
REV_NUM=$3
dws_file_path=drivers/misc/mediatek/dws/${PLATFORM}/${PROJECT}.dws
dws_dtsi_file_folder=arch/arm64/boot/dts/mediatek/
dct_para='cust_dtsi'

#need to input project name
if [ -z "$PROJECT" ]
  then
    echo "Please indicate a project name!"
    exit
fi
if [ -z "$PLATFORM" ]
  then
    echo "Please indicate a platform!"
    exit
fi
if [ -z "$REV_NUM" ]
  then
    echo "Please assign revison number for dtsi file! ex.:00, 01, 02, etc"
    exit
fi

#check dws file exist or not
if [ -f "$dws_file_path" ]; then
    echo "$dws_file_path exist"
else
    echo "$dws_file_path does not exist"
	exit
fi
#generate custom file from dws
python tools/dct/DrvGen.py ${dws_file_path} ./ ./ ${dct_para}
mv ./cust.dtsi ${dws_dtsi_file_folder}/cust_${PROJECT}-gpio${REV_NUM}.dtsi
