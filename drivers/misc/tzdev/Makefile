subdir-ccflags-y += -I$(srctree)/drivers/misc/tzdev/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/tzdev

obj-$(CONFIG_TZDEV)			+= lib/
obj-$(CONFIG_TZDEV)			+= core/
obj-$(CONFIG_TZDEV)			+= teec/
obj-$(CONFIG_TZDEV)			+= debug/
obj-$(CONFIG_TZDEV)			+= extensions/

obj-$(CONFIG_TZDEV_DEBUG)		+= tests/

ccflags-$(CONFIG_TZ_PMF) += -DCONFIG_TZ_PMF
