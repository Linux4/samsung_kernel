obj-$(CONFIG_BATTERY_SAMSUNG)	+= sec_battery.o
obj-$(CONFIG_STBC_SAMSUNG)      += stbc_charger.o\
				   stbc_fuelgauge.o
obj-$(CONFIG_OF)		+= sec_adc.o \
  				   sec_dt_init.o

obj-$(CONFIG_FUELGAUGE_ADC)	+= adc_fuelgauge.o
obj-$(CONFIG_FUELGAUGE_MAX17042)	+= max17042_fuelgauge.o
obj-$(CONFIG_FUELGAUGE_MAX17048)	+= max17048_fuelgauge.o
obj-$(CONFIG_FUELGAUGE_MAX17050)	+= max17050_fuelgauge.o
obj-$(CONFIG_FUELGAUGE_STC3115)		+= stc3115_fuelgauge.o
obj-$(CONFIG_FUELGAUGE_STC3117)		+= stc3117_fuelgauge.o sec_fuelgauge.o
obj-$(CONFIG_FUELGAUGE_SPRD2713)	+= sprd2713_fuelgauge.o sec_fuelgauge.o

obj-$(CONFIG_CHARGER_MAX8903)	+= max8903_charger.o
obj-$(CONFIG_CHARGER_SMB328)	+= smb328_charger.o
obj-$(CONFIG_CHARGER_SMB347)	+= smb347_charger.o
obj-$(CONFIG_CHARGER_SMB358)    += smb358_charger.o
obj-$(CONFIG_CHARGER_BQ24157)	+= bq24157_charger.o
obj-$(CONFIG_CHARGER_BQ24190)	+= bq24190_charger.o
obj-$(CONFIG_CHARGER_BQ24191)	+= bq24190_charger.o
obj-$(CONFIG_CHARGER_BQ24260)	+= bq24260_charger.o
obj-$(CONFIG_CHARGER_NCP1851)	+= ncp1851_charger.o
obj-$(CONFIG_CHARGER_TSU8111)	+= tsu8111_charger.o
obj-$(CONFIG_CHARGER_MAX14577)	+= max14577_charger.o
# For IF PMIC, sec_charger.o should be erased
obj-$(CONFIG_CHARGER_MAX77693)	+= max77693_charger.o
obj-$(CONFIG_CHARGER_SPRD2713)  += sprd2713_charger.o sec_charger.o

obj-$(CONFIG_CHARGER_SM5701)    += sm5701_charger.o sec_dt_init.o
obj-$(CONFIG_FUELGAUGE_SPRD4SAMSUNG27X3) += sprd27x3_fuelgauge4samsung.o sec_fuelgauge.o sprd_2713_fgu.o
obj-$(CONFIG_CHARGER_SPRD4SAMSUNG27X3) += sprd27x3_charger4samsung.o
obj-$(CONFIG_CHARGER_SM5414)   += sm5414_charger.o sec_charger.o sec_dt_init.o