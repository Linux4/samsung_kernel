ccflags-y += -I$(src)			# needed for trace events

obj-y					+= ion/

obj-$(CONFIG_ASHMEM)			+= ashmem.o
obj-$(CONFIG_ANDROID_VSOC)		+= vsoc.o
obj-$(CONFIG_ANDROID_LOW_MEMORY_KILLER)        += lowmemorykiller.o
# HS03 code for SR-SL6215-01-558 by shixuanxuan at 20210814 start
obj-$(CONFIG_PWR_ON_ALARM)		+= power_on_alarm-dev.o
# HS03 code for SR-SL6215-01-558 by shixuanxuan at 20210814 end

obj-$(CONFIG_SAMSUNG_LMKD_DEBUG)		+= lmkd_debug.o
