#!/bin/bash

function chceckDevice() {
	isDevice="false";
	numberPlugged=0;

	for deviceList in `adb devices | grep -v "List" | awk '{print $1}'`
	do
		if [[ $deviceList = $idDevice ]]
		then
			isDevice="true";
			break
		fi

		numberPlugged=$(($numberPlugged+1));
	done

	if [[ $isDevice = "true" ]]
	then
		echo "Device is plugged.";
	elif [[ $numberPlugged = 1 ]]
	then
		idDevice=`adb devices | grep -v "List" | awk '{print $1}'`;
	else
		echo "Invalit id device. Plugged device doesn't exists or you must run script with -d id device.";
		exit 1;
	fi
}

function chceckFile() {
	if [[ -e $fileName ]]
	then
		printf "File is exists. Remove this file? [y/n]: ";
		read item;
		
		if [[ $item = "y" || $item = "Y" ]]
		then
			rm $fileName;
			touch $fileName;
		fi
	else
		touch $fileName;
	fi
}

function printHelp() {
	echo "============================================================";
	echo "|			HELP 				   |";
	echo "============================================================";
	echo "|  -i - number of iterations script			   |";
	echo "|  -d - id plugged devices to generate logs and run script |";
	echo "|  -s - number sleep script				   |";
	echo "|  -f - file name to save logs from device		   |";
	echo "|  -h - print help					   |";
	echo "|  -t - calculate run time script			   |";
	echo "|  -m - pameters:					   |";
	echo "| 	a - all, l -LABB, r - rotate screen		   |";
	echo "| 	b - slide brightness, f - change focus		   |";
	echo "| 	t - switch to torch, c- slide colors		   |";
	echo "| 	k - color compensation		   |";
	echo "| 	o - turn on / off projector			   |";
	echo "|  -crtl+c - stop script and kill generate log		   |";
	echo "============================================================";
}

function generateOptionsFile(){
	option1="=========================================================\n";
	option2="| \t\tSCRIPT OPTION \t\t\t\t|\n";
	option3="=========================================================\n";
	option4="|  -i "$numberIterations" \t\t\t\t\t\t|\n";
	option5="|  -d "$idDevice" \t\t\t\t\t|\n";
	option6="|  -s "$numberSleep" \t\t\t\t\t\t|\n";
	option7="|  -f "$fileName" \t\t\t\t\t|\n";
	option8="|  -m "$parameters" \t\t\t\t\t\t\t|\n";
	option9="=========================================================\n";

	option=${option1}${option2}${option3}${option4}${option5}${option6}${option7}${option8}${option9};

	echo -e $option >> $fileName;
}

function generateLog(){
	while true; do adb -s $idDevice shell dmesg -c >> $fileName; sleep 1; done &
	PID=$!;
}

function crtl_c(){
	echo -e "\E[31mUser click crtl+c and stop script.\033[0m";
	kill $PID;
	echo -e "\E[32mDONE!\033[0m";
	exit 1;
}

start=`date +%s`;

if ! [[ $@ ]]
then
	printHelp;
	exit 1;
fi

while getopts ":i:d:f:s:m:th" options;
do
	case $options in
		i)
			if [[ $OPTARG =~ ^-?[0-9]+$ ]]
			then
				numberIterations=$OPTARG;
			else
				echo "Invalid argument -i. This argument accept numbers.";
			fi
			;;
		d)
			idDevice=$OPTARG;
			chceckDevice;
			;;
		f)
			fileName=$OPTARG;
			chceckFile;
			;;
		s)
			if [[ $OPTARG =~ ^-?[0-9]+$ || $OPTARG =~ ^-?[0-9]+.+[0-9]+$ ]]
			then
				numberSleep=$OPTARG;

			else
				echo "Invalid argument -s. This argument accept numbers.";
			fi
			;;
		t)
			timer="true";
			;;
		m)
			if [[ $OPTARG =~ "a" || $OPTARG =~ "b" || $OPTARG =~ "l" || $OPTARG =~ "r" || $OPTARG =~ "f" || $OPTARG =~ "t" || $OPTARG =~ "c" || $OPTARG =~ "o" || $OPTARG =~ "k" ]]
			then
				parameters=$OPTARG;
			fi
			;;
		h)
			printHelp;
			exit 1;
			;;
		?)
			echo "Invalid option" $options "Run scrip with option -h (help).";
			exit 1;
			;;
	esac
done

if [[ $idDevice = NULL || $idDevice = "" ]]
then
	chceckDevice;
fi

if [[ $numberIterations = NULL ]]
then
	echo "Number iterqations is null. Please run script with -i and number option.";
fi

adb -s $idDevice remount;

if [[ $numberSleep = NULL || $numberSleep = "" ]]
then
	numberSleep=0.01;
fi

if [[ $fileName = NULL ]] || [[ $fileName = "" ]]
then
	fileName="log.txt";
	chceckFile;
fi

if [[ $parameters = NULL || $parameters = "" ]]
then
	parameters="a";
fi

echo -e "\E[32m============================================================\033[0m";
echo -e "\E[32m|		START Projector Test 			   |\033[0m";
echo -e "\E[32m============================================================\033[0m";
echo "Start script on device:" $idDevice;
echo "Start generate log and operations on device.";

generateOptionsFile;
generateLog;

for i in $(seq $numberIterations)
do
	echo -e "\E[31mNow is "$i" iteration.\033[0m";
   adb -s $idDevice shell date "+%F %T"

	#
	#   Turn on projector
	#
	echo -e "Turn on projector...\c";
	adb -s $idDevice shell "echo 1 > /sys/class/sec/sec_projector/proj_key"
	echo -e ".\c";	
	sleep $numberSleep
	echo -e ".\c";
	sleep $numberSleep
	echo -e ".\c";
	sleep $numberSleep
	echo -e "\E[32mOK\033[0m";

	#
	#   LABB
	#
	if [[ $parameters =~ "a" || $parameters =~ "l" ]]
	then
		echo -e "LABB...\c";
		adb -s $idDevice shell "echo 1 > /sys/class/sec/sec_projector/proj_labb"
		for i in $(seq 10 5 100)
		do
			echo -e ".\c";
    			adb -s $idDevice shell "echo $i > /sys/class/sec/sec_projector/labb_strength"
		done
		echo -e ".\c";
		adb -s $idDevice shell "echo 0 > /sys/class/sec/sec_projector/proj_labb"
		echo -e ".\c";
		sleep $numberSleep
		echo -e "\E[32mOK\033[0m";
	fi
	
	#
	#   ColorCompensation
	#
	

	if [[ $parameters =~ "a" || $parameters =~ "k" ]]
	then
		echo -e "ColorCompensation...\c";
		adb -s $idDevice shell "echo 61 > /sys/class/sec/sec_projector/projection_verify"
		echo -e ".\c";
		adb -s $idDevice shell "echo 0 > /sys/class/sec/sec_projector/caic"
		sleep $numberSleep
		echo -e ".\c";
		adb -s $idDevice shell "echo 11 > /sys/class/sec/sec_projector/led_duty"
		echo -e ".\c";
		adb -s $idDevice shell "echo 70 250 250 > /sys/class/sec/sec_projector/led_current"
		echo -e ".\c";
		adb -s $idDevice shell "echo 13 > /sys/class/sec/sec_projector/led_duty"
		echo -e ".\c";
		adb -s $idDevice shell "echo 70 70 250 > /sys/class/sec/sec_projector/led_current"
		echo -e ".\c";
		adb -s $idDevice shell "echo 12 > /sys/class/sec/sec_projector/led_duty"
		echo -e ".\c";
		adb -s $idDevice shell "echo 250 70 250 > /sys/class/sec/sec_projector/led_current"		
		echo -e ".\c";
		adb -s $idDevice shell "echo 14 > /sys/class/sec/sec_projector/led_duty"
		echo -e ".\c";
		adb -s $idDevice shell "echo 250 70 70 > /sys/class/sec/sec_projector/led_current"
		echo -e ".\c";
		adb -s $idDevice shell "echo 10 > /sys/class/sec/sec_projector/led_duty"
		echo -e ".\c";
		adb -s $idDevice shell "echo 250 250 70 > /sys/class/sec/sec_projector/led_current"
		echo -e ".\c";
		adb -s $idDevice shell "echo 15 > /sys/class/sec/sec_projector/led_duty"
		echo -e ".\c";
		adb -s $idDevice shell "echo 70 250 70 > /sys/class/sec/sec_projector/led_current"
		sleep $numberSleep
		echo -e ".\c";
		adb -s $idDevice shell "echo 1 > /sys/class/sec/sec_projector/caic"
		echo -e ".\c";
		adb -s $idDevice shell "echo 60 > /sys/class/sec/sec_projector/projection_verify"
		sleep $numberSleep
		echo -e "\E[32mOK\033[0m";
	fi

	#
	#   Rotate screen
	#
	if [[ $parameters =~ "a" || $parameters =~ "r" ]]
	then
		echo -e "Rotate screen...\c";
		for i in $(seq 0 5)
		do
			echo -e ".\c";
    			adb -s $idDevice shell "echo $i > /sys/class/sec/sec_projector/rotate_screen"
    			sleep $numberSleep
		done
		echo -e ".\c";
		sleep $numberSleep
		echo -e "\E[32mOK\033[0m";
	fi

	#
	#   Slide brightness
	#
	if [[ $parameters =~ "a" || $parameters =~ "b" ]]
	then
		echo -e "Slide brightness...\c";
		adb -s $idDevice shell "echo 3 > /sys/class/sec/sec_projector/brightness"
		echo -e ".\c";
		sleep $numberSleep
		adb -s $idDevice shell "echo 2 > /sys/class/sec/sec_projector/brightness"
		echo -e ".\c";
		sleep $numberSleep
		adb -s $idDevice shell "echo 1 > /sys/class/sec/sec_projector/brightness"
		echo -e ".\c";
		sleep $numberSleep
		echo -e "\E[32mOK\033[0m";
	fi

	#
	#   Change focus
	#
	if [[ $parameters =~ "a" || $parameters =~ "f" ]]
	then
		echo -e "Change focus...\c";
		for i in $(seq 5)
		do
			echo -e ".\c";
    			adb -s $idDevice shell "echo 5 > /sys/class/sec/sec_projector/proj_motor"
		done
		for i in $(seq 5)
		do
			echo -e ".\c";
    			adb -s $idDevice shell "echo 6 > /sys/class/sec/sec_projector/proj_motor"
		done
		sleep $numberSleep
		for i in $(seq 15)
		do
			echo -e ".\c";
    			adb -s $idDevice shell "echo 0 > /sys/class/sec/sec_projector/proj_motor"
		done
		for i in $(seq 15)
		do
			echo -e ".\c";
    			adb -s $idDevice shell "echo 1 > /sys/class/sec/sec_projector/proj_motor"
		done
		echo -e ".\c";
		sleep $numberSleep
		echo -e "\E[32mOK\033[0m";
	fi

	#
	#   Switch to torch
	#
	if [[ $parameters =~ "a" || $parameters =~ "t" ]]
	then
		echo -e "Switch to torch...\c";
		adb -s $idDevice shell "echo 1 > /sys/class/sec/sec_projector/projection_verify"
		echo -e ".\c";
		sleep $numberSleep
		echo -e "\E[32mOK\033[0m";
	fi

	#
	#   Slide brightness
	#
	if [[ $parameters =~ "a" || $parameters =~ "b" ]]
	then
		echo -e "Slide brightness...\c";
		adb -s $idDevice shell "echo 3 > /sys/class/sec/sec_projector/brightness"
		echo -e ".\c";
		sleep $numberSleep
		adb -s $idDevice shell "echo 2 > /sys/class/sec/sec_projector/brightness"
		echo -e ".\c";
		sleep $numberSleep
		adb -s $idDevice shell "echo 1 > /sys/class/sec/sec_projector/brightness"
		echo -e ".\c";
		sleep $numberSleep
		echo -e "\E[32mOK\033[0m";
	fi

	#
	#   Slide colors   
	#
	if [[ $parameters =~ "a" || $parameters =~ "c" ]]
	then
		echo -e "Slide colors...\c";
		adb -s $idDevice shell "echo 4 > /sys/class/sec/sec_projector/projection_verify"
		echo -e ".\c";
		sleep $numberSleep
		adb -s $idDevice shell "echo 5 > /sys/class/sec/sec_projector/projection_verify"
		echo -e ".\c";
		sleep $numberSleep
		adb -s $idDevice shell "echo 6 > /sys/class/sec/sec_projector/projection_verify"
		echo -e ".\c";
		sleep $numberSleep
		echo -e "\E[32mOK\033[0m";
	fi

    
	#
	#   Turn off projector
	#
	echo -e "Turn off projector...\c";
	adb -s $idDevice shell "echo 0 > /sys/class/sec/sec_projector/proj_key"
	echo -e "...\c";
	echo -e "\E[32mOK\033[0m";

    #
    # Get projector info
    #
    adb -s $idDevice shell cat /sys/class/sec/sec_projector/projection_verify

	#
	# crtl+c clicked to close script
	#
	trap crtl_c INT;
done

echo "Stop generate log and operations on device.";
kill $PID;
echo -e "\E[32mDONE!\033[0m";

if [[ $timer = "true" ]]
then
	end=`date +%s`;
	runtime=$(($end - $start));
	echo "Script run" $runtime "seconds.";
fi
