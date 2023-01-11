#!/bin/sh

adb shell "echo 1 > /proc/tee/enable; cat /proc/tee/enable"
