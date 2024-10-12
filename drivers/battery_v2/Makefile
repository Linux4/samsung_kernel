MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_BATTERY_SAMSUNG)	+= sec_battery.o
obj-$(CONFIG_OF)		+= sec_adc.o
obj-$(CONFIG_BATTERY_CISD)	+= sec_cisd.o
obj-$(CONFIG_MULTI_CHARGING)	+= sec_multi_charger.o
obj-$(CONFIG_STEP_CHARGING)	+= sec_step_charging.o
obj-$(CONFIG_UPDATE_BATTERY_DATA)	+= sec_battery_data.o
obj-$(CONFIG_BATTERY_NOTIFIER)	+= battery_notifier.o

obj-$(CONFIG_FUELGAUGE_S2MU004)		+= s2mu004_fuelgauge.o
obj-$(CONFIG_FUELGAUGE_S2MU005)        += s2mu005_fuelgauge.o

obj-$(CONFIG_CHARGER_S2MU004)   += s2mu004_charger.o
obj-$(CONFIG_CHARGER_S2MU005)   += s2mu005_charger.o

obj-$(CONFIG_WIRELESS_CHARGER_BQ51221)	+= bq51221_charger.o
obj-$(CONFIG_WIRELESS_CHARGER_P9220)	+= p9220_charger.o
