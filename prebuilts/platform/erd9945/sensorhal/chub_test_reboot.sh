echo ===================== REBOOT TEST ==================== >> /data/vendor/log/chub/result_reboot
sleep 3

tmp=$(cat /sys/class/nanohub/nanohub/alive)
echo $tmp >> /data/vendor/log/chub/result_reboot
if [[ $tmp != "chub alive" ]]; then
  echo $tmp
  echo 99 > /sys/class/nanohub/nanohub/utc
fi

echo sensor id: >> /data/vendor/log/chub/result_reboot
for i in 1 5 8 12 13
do
    echo $i > /sys/class/nanohub/nanohub/chipid 2>&1
    sleep 1
    cat /sys/class/nanohub/nanohub/chipid >> /data/vendor/log/chub/result_reboot 2>&1
done
nanotool -s accel:10 -c 1 | sed '1d' >> /data/vendor/log/chub/result_reboot
nanotool -s gyro:10 -c 1 | sed '1d' >> /data/vendor/log/chub/result_reboot
nanotool -s mag:10 -c 1 | sed '1d' >> /data/vendor/log/chub/result_reboot
echo ====================================================== >> /data/vendor/log/chub/result_reboot
