# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-$(CONFIG_POWER_SUPPLY_DEBUG) := -DDEBUG

#GCOV_PROFILE := $(CONFIG_SPRD_SUPPLY_GCOV)

# HS03 code for SR-SL6215-01-181 by gaochao at 20210724 start
obj-$(CONFIG_BATTERY_ID_VIA_ADC) += battery_id_via_adc.o
# HS03 code for SR-SL6215-01-181 by gaochao at 20210724 end

power_supply-y				:= power_supply_core.o
power_supply-$(CONFIG_SYSFS)		+= power_supply_sysfs.o
power_supply-$(CONFIG_LEDS_TRIGGERS)	+= power_supply_leds.o

obj-$(CONFIG_POWER_SUPPLY)	+= power_supply.o
obj-$(CONFIG_GENERIC_ADC_BATTERY)	+= generic-adc-battery.o

obj-$(CONFIG_PDA_POWER)		+= pda_power.o
obj-$(CONFIG_APM_POWER)		+= apm_power.o
obj-$(CONFIG_AXP20X_POWER)	+= axp20x_usb_power.o
obj-$(CONFIG_MAX8925_POWER)	+= max8925_power.o
obj-$(CONFIG_WM831X_BACKUP)	+= wm831x_backup.o
obj-$(CONFIG_WM831X_POWER)	+= wm831x_power.o
obj-$(CONFIG_WM8350_POWER)	+= wm8350_power.o
obj-$(CONFIG_TEST_POWER)	+= test_power.o

obj-$(CONFIG_BATTERY_88PM860X)	+= 88pm860x_battery.o
obj-$(CONFIG_BATTERY_ACT8945A)	+= act8945a_charger.o
obj-$(CONFIG_BATTERY_AXP20X)	+= axp20x_battery.o
obj-$(CONFIG_CHARGER_AXP20X)	+= axp20x_ac_power.o
obj-$(CONFIG_BATTERY_CPCAP)	+= cpcap-battery.o
obj-$(CONFIG_BATTERY_DS2760)	+= ds2760_battery.o
obj-$(CONFIG_BATTERY_DS2780)	+= ds2780_battery.o
obj-$(CONFIG_BATTERY_DS2781)	+= ds2781_battery.o
obj-$(CONFIG_BATTERY_DS2782)	+= ds2782_battery.o
obj-$(CONFIG_BATTERY_GAUGE_LTC2941)	+= ltc2941-battery-gauge.o
obj-$(CONFIG_BATTERY_GOLDFISH)	+= goldfish_battery.o
obj-$(CONFIG_BATTERY_LEGO_EV3)	+= lego_ev3_battery.o
obj-$(CONFIG_BATTERY_PMU)	+= pmu_battery.o
obj-$(CONFIG_BATTERY_OLPC)	+= olpc_battery.o
obj-$(CONFIG_BATTERY_TOSA)	+= tosa_battery.o
obj-$(CONFIG_BATTERY_COLLIE)	+= collie_battery.o
obj-$(CONFIG_BATTERY_IPAQ_MICRO) += ipaq_micro_battery.o
obj-$(CONFIG_BATTERY_WM97XX)	+= wm97xx_battery.o
obj-$(CONFIG_BATTERY_SBS)	+= sbs-battery.o
obj-$(CONFIG_CHARGER_SBS)	+= sbs-charger.o
obj-$(CONFIG_BATTERY_BQ27XXX)	+= bq27xxx_battery.o
obj-$(CONFIG_BATTERY_BQ27XXX_I2C) += bq27xxx_battery_i2c.o
obj-$(CONFIG_BATTERY_BQ27XXX_HDQ) += bq27xxx_battery_hdq.o
obj-$(CONFIG_BATTERY_DA9030)	+= da9030_battery.o
obj-$(CONFIG_BATTERY_DA9052)	+= da9052-battery.o
obj-$(CONFIG_CHARGER_DA9150)	+= da9150-charger.o
obj-$(CONFIG_BATTERY_DA9150)	+= da9150-fg.o
obj-$(CONFIG_BATTERY_MAX17040)	+= max17040_battery.o
obj-$(CONFIG_BATTERY_MAX17042)	+= max17042_battery.o
obj-$(CONFIG_BATTERY_MAX1721X)	+= max1721x_battery.o
obj-$(CONFIG_BATTERY_Z2)	+= z2_battery.o
obj-$(CONFIG_BATTERY_RT5033)	+= rt5033_battery.o
obj-$(CONFIG_CHARGER_RT9455)	+= rt9455_charger.o
obj-$(CONFIG_BATTERY_S3C_ADC)	+= s3c_adc_battery.o
obj-$(CONFIG_BATTERY_TWL4030_MADC)	+= twl4030_madc_battery.o
obj-$(CONFIG_CHARGER_88PM860X)	+= 88pm860x_charger.o
obj-$(CONFIG_CHARGER_PCF50633)	+= pcf50633-charger.o
obj-$(CONFIG_BATTERY_JZ4740)	+= jz4740-battery.o
obj-$(CONFIG_BATTERY_RX51)	+= rx51_battery.o
obj-$(CONFIG_AB8500_BM)		+= ab8500_bmdata.o ab8500_charger.o ab8500_fg.o ab8500_btemp.o abx500_chargalg.o pm2301_charger.o
obj-$(CONFIG_CHARGER_CPCAP)	+= cpcap-charger.o
obj-$(CONFIG_CHARGER_ISP1704)	+= isp1704_charger.o
obj-$(CONFIG_CHARGER_MAX8903)	+= max8903_charger.o
obj-$(CONFIG_CHARGER_TWL4030)	+= twl4030_charger.o
obj-$(CONFIG_CHARGER_LP8727)	+= lp8727_charger.o
obj-$(CONFIG_CHARGER_LP8788)	+= lp8788-charger.o
obj-$(CONFIG_CHARGER_GPIO)	+= gpio-charger.o
obj-$(CONFIG_CHARGER_MANAGER)	+= charger-manager.o sprd_vote.o
GCOV_PROFILE_charger-manager.o := $(CONFIG_SPRD_SUPPLY_GCOV)
obj-$(CONFIG_CHARGER_LTC3651)	+= ltc3651-charger.o
obj-$(CONFIG_CHARGER_MAX14577)	+= max14577_charger.o
obj-$(CONFIG_CHARGER_DETECTOR_MAX14656)	+= max14656_charger_detector.o
obj-$(CONFIG_CHARGER_MAX77693)	+= max77693_charger.o
obj-$(CONFIG_CHARGER_MAX8997)	+= max8997_charger.o
obj-$(CONFIG_CHARGER_MAX8998)	+= max8998_charger.o
obj-$(CONFIG_CHARGER_QCOM_SMBB)	+= qcom_smbb.o
obj-$(CONFIG_CHARGER_BQ2415X)	+= bq2415x_charger.o
obj-$(CONFIG_CHARGER_AW32257)	+= aw32257_charger.o
obj-$(CONFIG_CHARGER_BQ24190)	+= bq24190_charger.o
obj-$(CONFIG_CHARGER_BQ24257)	+= bq24257_charger.o
obj-$(CONFIG_CHARGER_BQ24735)	+= bq24735-charger.o
obj-$(CONFIG_CHARGER_BQ25890)	+= bq25890_charger.o
obj-$(CONFIG_CHARGER_SMB347)	+= smb347-charger.o
obj-$(CONFIG_CHARGER_TPS65090)	+= tps65090-charger.o
obj-$(CONFIG_CHARGER_TPS65217)	+= tps65217_charger.o
obj-$(CONFIG_AXP288_FUEL_GAUGE) += axp288_fuel_gauge.o
obj-$(CONFIG_AXP288_CHARGER)	+= axp288_charger.o
obj-$(CONFIG_CHARGER_CROS_USBPD)	+= cros_usbpd-charger.o
obj-$(CONFIG_CHARGER_SC2731)	+= sc2731_charger.o
obj-$(CONFIG_FUEL_GAUGE_SC27XX)	+= sc27xx_fuel_gauge.o
GCOV_PROFILE_sc27xx_fuel_gauge.o := $(CONFIG_SPRD_SUPPLY_GCOV)
obj-$(CONFIG_CHARGER_SC2703)	+= sc2703-charger.o
GCOV_PROFILE_sc2703-charger.o := $(CONFIG_SPRD_SUPPLY_GCOV)
obj-$(CONFIG_CHARGER_FAN54015)	+= fan54015-charger.o
GCOV_PROFILE_fan54015-charger.o := $(CONFIG_SPRD_SUPPLY_GCOV)
obj-$(CONFIG_CHARGER_ETA6937)	+= eta6937-charger.o
GCOV_PROFILE_eta6937-charger.o := $(CONFIG_SPRD_SUPPLY_GCOV)
obj-$(CONFIG_CHARGER_BQ2560X)	+= bq2560x-charger.o
# HS03 code for SR-SL6215-01-178 by gaochao at 20210724 start
obj-$(CONFIG_CHARGER_SGM41511)	+= sgm41511-charger.o
# HS03 code for SR-SL6215-01-178 by gaochao at 20210724 end
# HS03 code for SL6215DEV-4004 by lina at 20220119 start
obj-$(CONFIG_CHARGER_SGM41513)	+= sgm41513-charger.o
# HS03 code for SL6215DEV-4004 by lina at 20220119 end
obj-$(CONFIG_CHARGER_BQ25890)	+= bq25890_charger.o
# Tab A8 code for AX6300DEV-11 by wenyaqi at 20210730 start
obj-$(CONFIG_CHARGER_SGM4154X)	+= sgm4154x_charger.o
obj-$(CONFIG_CHARGER_SY6970)	+= sy6970_charger.o
# Tab A8 code for AX6300DEV-11 by wenyaqi at 20210730 end
# Tab A7 Lite T618 code for SR-AX6189A-01-103 by shixuanxuan at 20220105 start
obj-$(CONFIG_CHARGER_SC89890H) += sc89890h_charger.o
# Tab A7 Lite T618 code for SR-AX6189A-01-103 by shixuanxuan at 20220105 end
obj-$(CONFIG_CHARGER_BQ2597X)	+= bq2597x-charger.o
obj-$(CONFIG_CHARGER_SC2720)	+= sc2720_charger.o
obj-$(CONFIG_CHARGER_SC2721)	+= sc2721_charger.o
obj-$(CONFIG_WIRELESS_CHARGER_NU1619)	+= nu1619_wireless_charger.o
obj-$(CONFIG_WIRELESS_CHARGER_SY65153)	+= sy65153_wireless_charger.o
obj-$(CONFIG_WIRELESS_CHARGE_PUMPS_CONVERTER_HL1506)	+= hl1506_charger.o
obj-$(CONFIG_FAST_CHARGER_SC2730) += sc2730_fast_charger.o
obj-$(CONFIG_CHARGER_BQ25910) += bq25910_charger.o
# HS03 code for SR-SL6215-01-177 by jiahao at 20210810 start
obj-$(CONFIG_CHARGER_RT9471)	+= rt9471_charger.o
# HS03 code for SR-SL6215-01-177 by jiahao at 20210810 end
