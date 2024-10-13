OUTPUT="/data/vendor/log/chub/result_pr"
LISTNUM=21

PASS=0
FAIL=0

function reset_test() {
    echo @@ reset test @@
    echo @@ reset test @@ >> $OUTPUT
    rm /data/vendor/log/chub/chub_bin* > /dev/null 2>&1
    rm /data/vendor/log/chub/tmp > /dev/null 2>&1
    sleep 1
    echo 1 > /sys/class/nanohub/nanohub/reset
    sleep 2
    tmp=""
    tmp=$(ls /data/vendor/log/chub/chub_bin*)
    /system/bin/sensortest -e 1 200000 > /data/vendor/log/chub/tmp &
    sleep 2
    kill -9 `ps -ef | grep sensortest | awk '{print $2}'` > /dev/null 2>&1
    if [ -z "$tmp" ] || [ -z "$(cat /data/vendor/log/chub/tmp)" ]
    then
       FAIL=$(($FAIL + 1))
        echo reset test fail >> $OUTPUT
        echo reset test fail
    else
       PASS=$(($PASS + 1))
        echo reset test pass >> $OUTPUT
        echo reset test pass
    fi
    rm /data/vendor/log/chub/chub_bin* > /dev/null 2>&1
    rm /data/vendor/log/chub/tmp > /dev/null 2>&1
    printf "\n"
}

function crash_test() {
    echo @@ crash test @@
    echo @@ crash test @@ >> $OUTPUT
    for i in 17 18 21 22 23 24 25
    do
        echo "utc $i"
        echo "utc $i" >> $OUTPUT
        echo $i > /sys/class/nanohub/nanohub/utc
        sleep 1
       nanoapp_cmd config gyro true 100 0 > /dev/null # dummy command to crash
        sleep 5
        dmesg | grep contexthub_reset | sed '$!d'

        rm /data/vendor/log/chub/tmp > /dev/null 2>&1
        tmp=""
        tmp=$(cat /sys/class/nanohub/nanohub/alive)
        /system/bin/sensortest -e 1 200000 > /data/vendor/log/chub/tmp &
        sleep 2
        kill -9 `ps -ef | grep sensortest | awk '{print $2}'` > /dev/null 2>&1
        if [ "$tmp" != "chub alive" ] || [ -z "$(cat /data/vendor/log/chub/tmp)" ]
        then
           FAIL=$(($FAIL + 1))
            echo crash_test at $i fail
            echo crash_test at $i fail >> $OUTPUT
            echo 1 > /sys/class/nanohub/nanohub/reset
            sleep 2
            #echo 99 > /sys/class/nanohub/nanohub/utc #kernel panic
       else
           PASS=$(($PASS + 1))
           echo crash_test at $i pass
           echo crash_test at $i pass >> $OUTPUT
        fi

        printf "\n"
    done
    rm /data/vendor/log/chub/chub_bin* > /dev/null 2>&1
    rm /data/vendor/log/chub/tmp > /dev/null 2>&1
    echo @@ crash test end @@
    echo @@ crash test end @@ >> $OUTPUT
    printf "\n"
}

function sensor_test() {
    echo @@ physical sensor test @@
    echo @@ physical sensor test @@ >> $OUTPUT
    /system/bin/sensortest -l > /data/vendor/log/chub/tmp
    sleep 1
    TMP=$(wc -l < /data/vendor/log/chub/tmp)
    cat /data/vendor/log/chub/tmp >> $OUTPUT
    if [ $TMP != $LISTNUM ]
    then
       PASS=$(($PASS + 1))
        echo sensor list pass
    else
       FAIL=$(($FAIL + 1))
        echo sensor list fail!
    fi
    for sensor in 1 2 4 5 6 8
    do
       echo test sensor $sensor
        rm /data/vendor/log/chub/tmp > /dev/null 2>&1
        rm /data/vendor/log/chub/tmp2 > /dev/null 2>&1
       tmp=""
       tmp2=""

       /system/bin/sensortest -e $sensor 200000 > /data/vendor/log/chub/tmp &
       sleep 2
       kill -9 `ps -ef | grep sensortest | awk '{print $2}'` 2> /dev/null
        cat /data/vendor/log/chub/tmp
       /system/bin/sensortest -b $sensor 200000 1000000 > /data/vendor/log/chub/tmp2 &
       sleep 2
       kill -9 `ps -ef | grep sensortest | awk '{print $2}'` 2> /dev/null
       cat /data/vendor/log/chub/tmp2

       if [ -z "$(cat /data/vendor/log/chub/tmp)" ] || [ -z "$(cat /data/vendor/log/chub/tmp2)" ]
       then
           FAIL=$(($FAIL + 1))
            echo sensor test $sensor fail
            echo sensor test $sensor fail >> $OUTPUT
       else
           PASS=$(($PASS + 1))
            echo sensor test $sensor pass
            echo sensor test $sensor pass >> $OUTPUT
        fi
       rm /data/vendor/log/chub/tmp > /dev/null 2>&1
       rm /data/vendor/log/chub/tmp2 > /dev/null 2>&1
        printf "\n"
    done
    echo @@ sensor test end @@
    echo @@ sensor test end @@ >> $OUTPUT
    printf "\n"
}

function error_test() {
        echo @@ error_test @@
        echo @@ error_test @@ >> $OUTPUT
        for i in 10 11 15 28
        do
           echo utc $i
            echo $i > /sys/class/nanohub/nanohub/utc
            rm /data/vendor/log/chub/tmp > /dev/null 2>&1
            sleep 10
            tmp=""
            tmp=$(cat /sys/class/nanohub/nanohub/alive)
           /system/bin/sensortest -e 1 200000 > /data/vendor/log/chub/tmp &
            sleep 2
            kill -9 `ps -ef | grep sensortest | awk '{print $2}'` > /dev/null 2>&1
            if [ "$tmp" != "chub alive" ] || [ -z "$(cat /data/vendor/log/chub/tmp)" ]
            then
               FAIL=$(($FAIL + 1))
                echo error_test at $i fail
               echo $tmp
               cat /data/vendor/log/chub/tmp
                echo error_test at $i fail >> $OUTPUT
                echo 1 > /sys/class/nanohub/nanohub/reset
                sleep 2
                echo 99 > /sys/class/nanohub/nanohub/utc
            else
               PASS=$(($PASS + 1))
               echo error_test at $i pass
               echo error_test at $i pass >> $OUTPUT
            fi
        printf "\n"
        done
       rm /data/vendor/log/chub/tmp > /dev/null 2>&1
        echo @@ error_test end @@
        echo @@ error_test end @@ >> $OUTPUT
        printf "\n"
        sleep 5
}

SET=$(seq 1 1000)
for i in $SET
do
    PASS=0
    FAIL=0
    echo ~~~~ $i run ~~~~
    echo ~~~~ $i run ~~~~ >> $OUTPUT
    reset_test
    crash_test
    error_test
    sensor_test
    echo ~~~~ test result ~~~~
    echo pass: $PASS
    echo fail: $FAIL
    echo ~~~~~~~~~~~~~~~~~~~~~
    rm /data/vendor/log/chub/chub_bin* > /dev/null 2>&1
    sleep 5
done
