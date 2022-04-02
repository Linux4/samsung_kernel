# drivers/samsung/debug/Makefile
#
# Copyright (c) 2010-2019 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2
#

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

ccflags-y := -Wformat

# Samsung TN Debugging Features
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug_base.o
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug_test.o
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug_ksym.o
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug_mode.o
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug_memtab.o
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug_summary.o
obj-$(CONFIG_SEC_DEBUG_HIST_LOG) 	+= sec_debug_hist.o
obj-$(CONFIG_SEC_DEBUG_RESET_REASON)	+= sec_debug_reset_reason.o
obj-$(CONFIG_SEC_DEBUG_EXTRA_INFO)	+= sec_debug_extra_info.o
obj-$(CONFIG_SEC_DEBUG_HW_PARAM)	+= sec_debug_hw_param.o
obj-$(CONFIG_SEC_DEBUG_AUTO_COMMENT)	+= sec_debug_auto_comment.o
obj-$(CONFIG_SEC_DEBUG_LAST_KMSG)	+= sec_debug_last_kmsg.o
obj-$(CONFIG_SEC_DEBUG_FIRST2M_LOG)	+= sec_debug_first2M_log.o
obj-$(CONFIG_SEC_DEBUG_INIT_LOG)	+= sec_debug_init_log.o
obj-$(CONFIG_SEC_DEBUG_WATCHDOGD_FOOTPRINT)	+= sec_debug_wdd_info.o
obj-$(CONFIG_SEC_DEBUG_FREQ)		+= sec_debug_freq.o
obj-$(CONFIG_SEC_DEBUG_COMPLETE_HINT)	+= sec_debug_complete_hint.o
obj-$(CONFIG_SEC_DEBUG_DTASK)		+= sec_debug_dtask.o
obj-$(CONFIG_SEC_DEBUG_SOFTDOG)   	+= sec_debug_softdog.o
obj-$(CONFIG_SEC_DEBUG_SHOW_USER_STACK)	+= sec_debug_show_user_stack.o
