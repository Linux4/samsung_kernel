obj-$(CONFIG_DIRECT_CHARGING)		+= sec_direct_charger.o

ccflags-y := -Wformat

obj-$(CONFIG_SEC_PD)				+= sec_pd.o

obj-$(CONFIG_BATTERY_SAMSUNG)	+= sec-battery.o
sec-battery-$(CONFIG_BATTERY_SAMSUNG)		+= sec_battery.o sec_battery_vote.o sec_battery_thermal.o sec_battery_sysfs.o sec_battery_dt.o sec_battery_ttf.o sec_adc.o
sec-battery-$(CONFIG_BATTERY_CISD)	+= sec_cisd.o
sec-battery-$(CONFIG_STEP_CHARGING)		+= sec_step_charging.o
sec-battery-$(CONFIG_WIRELESS_AUTH)		+= sec_battery_misc.o
sec-battery-$(CONFIG_WIRELESS_CHARGER_MFC)	+= sec_battery_wc.o

obj-$(CONFIG_UPDATE_BATTERY_DATA)	+= sec_battery_data.o

ccflags-y := -Wformat
