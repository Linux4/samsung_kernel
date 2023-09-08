obj-$(CONFIG_BATTERY_SAMSUNG)		+= sec_battery.o sec_battery_vote.o sec_battery_thermal.o sec_battery_dt.o sec_battery_ttf.o
obj-$(CONFIG_BATTERY_SAMSUNG)	+= sec_core_sysfs.o sec_battery_sysfs.o sec_device_sysfs.o sec_event_sysfs.o
obj-$(CONFIG_ENG_BATTERY_CONCEPT) += sec_eng_sysfs.o
obj-$(CONFIG_OF)			+= sec_adc.o
obj-$(CONFIG_MULTI_CHARGING)		+= sec_multi_charger.o
obj-$(CONFIG_DUAL_BATTERY)		+= sec_dual_battery.o
obj-$(CONFIG_STEP_CHARGING)		+= sec_step_charging.o
obj-$(CONFIG_BATTERY_CISD)		+= sec_cisd.o
obj-$(CONFIG_WIRELESS_AUTH)		+= sec_battery_misc.o
obj-$(CONFIG_UPDATE_BATTERY_DATA)	+= sec_battery_data.o

obj-$(CONFIG_PDIC_NOTIFIER)		+= sec_pd.o
obj-$(CONFIG_BATTERY_LOGGING)	+= battery_logger.o

GCOV_PROFILE_sec_battery.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_battery_thermal.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_battery_vote.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_cisd.o := $(CONFIG_SEC_KUNIT)
        GCOV_PROFILE_sec_adc.o := $(CONFIG_SEC_KUNIT)
        GCOV_PROFILE_sec_battery_sysfs.o := $(CONFIG_SEC_KUNIT)
        GCOV_PROFILE_sec_battery_ttf.o := $(CONFIG_SEC_KUNIT)
        GCOV_PROFILE_sec_step_charging.o := $(CONFIG_SEC_KUNIT)
       GCOV_PROFILE_sec_pd.o := $(CONFIG_SEC_KUNIT)
ccflags-y := -Wformat
