adb wait-for-device
adb shell setenforce 0
adb shell /data/gatord &
adb shell setenforce 1
adb forward tcp:8080 tcp:8080
adb shell "echo 1 > /sys/kernel/debug/tracing/atrace_gator"
adb shell "/system/bin/atrace --set_tags gfx input audio dalvik view wm"
