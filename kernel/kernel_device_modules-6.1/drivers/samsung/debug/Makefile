# drivers/staging/samsung/Makefile
#
# Copyright (c) 2010-2016 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

# Samsung Debugging Feature
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug.o

sec_debug-y += \
	sec_debug_base.o \
	sec_debug_log.o \
	sec_debug_pmsg.o \
	sec_debug_init_log.o

sec_debug-$(CONFIG_SEC_DEBUG_DUMP_SINK)		+= sec_debug_dump_sink.o

ifdef CONFIG_SEC_DEBUG_AUTO_COMMENT
sec_debug-y += sec_debug_auto_comment.o
endif

ifdef CONFIG_SEC_DEBUG_HISTORY_LOG
sec_debug-y += sec_debug_hist.o
endif

obj-$(CONFIG_SEC_DEBUG_EXTRA_INFO)		+= sec_extra_info.o
sec_extra_info-y += \
	sec_debug_extra_info.o \
	sec_debug_hw_param.o

obj-$(CONFIG_SEC_DEBUG_SOFTDOG)		+= sec_debug_softdog.o

