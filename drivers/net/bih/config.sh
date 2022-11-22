#!/bin/sh
v4addr=192.168.1.34
v6addr=2001:da8:bf:1010::c0a8:122
v6gw=2001:da8:bf:1010::c0a8:101
nameserver=2001:da8:bf:1010::1
pool=192.168.3.40-192.168.3.100
#map="192.168.1.35 2001:da8:bf:1010::c0a8:123"
#map="192.168.1.200 2001:da8:bf:1010::c0a8:1c8"
#map="192.168.1.40 2001:da8:bf:1010::c0a8:128"
#################################################
dev=`ifconfig|grep ^eth|awk '{print $1}'|tail -1`
if [ "X$v6addr" != "X" ] ; then
        ifconfig $dev inet6 add $v6addr/64
fi
if [ "X$v4addr" != "X" ] ; then
        ifconfig $dev $v4addr
fi
if [ "X$v6gw" != "X" ] ; then
        route -Ainet6 add default gw $v6gw
fi
if [ "X$nameserver" != "X" ] ; then
        echo "nameserver $nameserver" > /etc/resolv.conf
fi
rmmod bih > /dev/null 2>&1
insmod `pwd`/kernel-module/bih.ko
if [ "X$pool" != "X" ] ; then
        echo ADD $pool > /proc/bih/pool
fi
if [ "X$map" != "X" ] ; then
        echo ADD $map > /proc/bih/map
fi
route add default dev $dev
echo "1" > /proc/sys/net/ipv6/conf/all/forwarding
echo "BIS" > /proc/bih/mode
