ccflags-y += -I$(src)			# needed for trace events

obj-y					+= ion/
ifneq ($(HQ_FACTORY_BUILD), true)
obj-$(CONFIG_PWR_ON_ALARM)		+= power_on_alarm-dev.o
endif
obj-$(CONFIG_ASHMEM)			+= ashmem.o
obj-$(CONFIG_ANDROID_VSOC)		+= vsoc.o

obj-$(CONFIG_SAMSUNG_LMKD_DEBUG)		+= lmkd_debug.o