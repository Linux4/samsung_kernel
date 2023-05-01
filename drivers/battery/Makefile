ifeq ($(CONFIG_SEC_PRODUCT_8930),y)
obj-$(CONFIG_BATTERY_SAMSUNG)   += sec_battery_8930.o sec_fuelgauge_8930.o
else
obj-$(CONFIG_BATTERY_SAMSUNG)	+= sec_battery.o sec_fuelgauge.o
endif

obj-$(CONFIG_FUELGAUGE_MAX17042)	+= max17042_fuelgauge.o
ifeq ($(CONFIG_SEC_PRODUCT_8930),y)
obj-$(CONFIG_FUELGAUGE_MAX17048)        += max17048_fuelgauge_8930.o
else
obj-$(CONFIG_FUELGAUGE_MAX17048)	+= max17048_fuelgauge.o
endif
obj-$(CONFIG_FUELGAUGE_MAX17050)	+= max17050_fuelgauge.o

obj-$(CONFIG_CHARGER_MAX8903)	+= max8903_charger.o
obj-$(CONFIG_CHARGER_SMB328)	+= smb328_charger.o
obj-$(CONFIG_CHARGER_SMB347)	+= smb347_charger.o
ifeq ($(CONFIG_SEC_PRODUCT_8930),y)
obj-$(CONFIG_CHARGER_SMB358)	+= smb358_charger.o sec_charger_8930.o
else
obj-$(CONFIG_CHARGER_SMB358)	+= smb358_charger.o sec_charger.o
endif
ifeq ($(CONFIG_SEC_PRODUCT_8930),y)
obj-$(CONFIG_CHARGER_BQ24157)	+= bq24157_charger.o sec_charger_8930.o
else
obj-$(CONFIG_CHARGER_BQ24157)	+= bq24157_charger.o sec_charger.o
endif
obj-$(CONFIG_CHARGER_BQ24190)	+= bq24190_charger.o
obj-$(CONFIG_CHARGER_BQ24191)	+= bq24190_charger.o
ifeq ($(CONFIG_SEC_PRODUCT_8930),y)
obj-$(CONFIG_CHARGER_MAX77XXX)  += max77693_charger_8930.o
else
obj-$(CONFIG_CHARGER_MAX77XXX)  += max77693_charger.o
endif
