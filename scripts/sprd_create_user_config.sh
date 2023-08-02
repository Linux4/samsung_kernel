#!/bin/bash

FN="$2"

echo "==== create user kernel configs ===="

for line in `cat "$FN"`
do
prefix=`expr substr "$line" 1 3`

if [ "$prefix" = "DEF" ]; then
config=${line:11}
./kernel/scripts/config --file $1 -d $config

elif [ "$prefix" = "VAL" ]; then
len=`expr length $line`
idx=`expr index $line "="`
config=`expr substr "$line" 12 $[$idx-12]`
val=`expr substr "$line" $[$idx+1] $len`
./kernel/scripts/config --file $1 --set-val $config $val

elif [ "$prefix" = "STR" ]; then
len=`expr length $line`
idx=`expr index $line "="`
config=`expr substr "$line" 12 $[$idx-12]`
str=`expr substr "$line" $[$idx+1] $len`
./kernel/scripts/config --file $1 --set-str $config $str

elif [ "$prefix" = "ADD" ]; then
config=${line:11}
./kernel/scripts/config --file $1 -e $config

fi
done
