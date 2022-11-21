#!/bin/sh

adb shell "while ( true ) do cat /proc/tee/log; done" | tee tee.dmp
