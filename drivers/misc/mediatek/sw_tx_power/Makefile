# include $(srctree)/drivers/misc/mediatek/Makefile.custom

$(warning([teddy] MTK_PLATFORM=$(MTK_PLATFORM) CINFIG=$(CONFIG_MTK_PLATFORM))

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sw_tx_power/$(MTK_PLATFORM)

obj-y += sw_tx_power_eint.o
obj-y += sw_tx_power_sysfs.o

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
