# Copyright (C) 2023 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#
#
#LINUXINCLUDE += -include $(srctree)/kernel/sched/sched.h
#LINUXINCLUDE += -include $(srctree)/drivers/android/binder_internal.h
#ccflags-y += -DDEBUG_PGBOOST_ENABLED	# dump more logs for debug
#ccflags-y += -DTEST_PGBOOST_ENABLED	# check behaviors manually
ccflags-y += -I$(src)              	# needed for trace events
ccflags-y += -I$(srctree)/mm
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pgboost
obj-$(CONFIG_MTK_PGBOOST) += pgboost.o
pgboost-objs += src/pgboost.o
pgboost-objs += src/ksym.o
