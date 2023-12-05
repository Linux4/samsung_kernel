# drivers/samsung/pm/Makefile
#
# Copyright (c) 2010-2022 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2
#
obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

# Samsung Mobile Power Management Debugging Support
obj-$(CONFIG_SEC_PM_DEBUG)		+= sec_pm_debug.o
obj-$(CONFIG_SEC_PM_QOS_MONITOR)	+= sec_pm_qos_monitor.o
obj-$(CONFIG_RTC_AUTO_PWRON)	+= sec_pon_alarm.o
obj-$(CONFIG_SEC_WAKEUP_CPU_ALLOCATOR)		+= sec_wakeup_cpu_allocator.o
obj-$(CONFIG_SEC_PM_CPUFREQ)            += sec_pm_cpufreq.o

ccflags-y := -Wformat
