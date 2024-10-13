#!/vendor/bin/sh

usb_state_sysfs="/sys/class/android_usb/android0/state"
usb_state_val=""
usb_state_need_check=0
usb_state_check_interval=1

usb_state_sfr="/sys/devices/platform/17900000.usb/usb_state"

usb_connection_fail_sysfs="/sys/devices/platform/17900000.usb/usb_connection_fail"
usb_connection_fail_val=""

usb_log_dir="/data/vendor/log/usb"
date_format="+%y%m%d_%H%M%S"

#rm -rf $usb_log_dir
mkdir $usb_log_dir

while [ 1 ]
do
	usb_state_val=$(cat $usb_state_sysfs)
	echo "$(date $date_format): $usb_state_val" >> "$usb_log_dir/usb_connection_history.txt"
	if [ $usb_state_val = "DISCONNECTED" ]
	then
		usb_state_need_check=0
		usb_connection_fail_val=$(cat $usb_connection_fail_sysfs)
		if [ $usb_connection_fail_val = "1" ] # CONNECTION_FAIL_SET
		then
			cat $usb_state_sfr > "$usb_log_dir/$(date $date_format)_sfr.txt"
			dmesg > "$usb_log_dir/$(date $date_format)_kernel.txt"
			echo 2 > $usb_connection_fail_sysfs # CONNECTION_FAIL_DUMP
	fi
	else
		usb_state_need_check=1
	fi

	while [ $usb_state_need_check = 1 ]
	do
		usb_state_val=$(cat $usb_state_sysfs)

		if [ $usb_state_val = "DISCONNECTED" ]
		then
			echo "$(date $date_format): $usb_state_val" >> "$usb_log_dir/usb_connection_history.txt"
			cat $usb_state_sfr > "$usb_log_dir/$(date $date_format)_sfr.txt"
			dmesg > "$usb_log_dir/$(date $date_format)_kernel.txt"
			usb_state_need_check=0
			break;
		fi

		sleep $usb_state_check_interval
		#sleep 1s
	done

	#sleep 1s
	sleep $usb_state_check_interval
done

