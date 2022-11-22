# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-$(CONFIG_POWER_SUPPLY_DEBUG) := -DDEBUG
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb20
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_throttling

power_supply-y				:= power_supply_core.o
power_supply-$(CONFIG_SYSFS)		+= power_supply_sysfs.o
power_supply-$(CONFIG_LEDS_TRIGGERS)	+= power_supply_leds.o

obj-$(CONFIG_POWER_SUPPLY)	+= power_supply.o
obj-$(CONFIG_GENERIC_ADC_BATTERY)	+= generic-adc-battery.o

obj-$(CONFIG_PDA_POWER)		+= pda_power.o
obj-$(CONFIG_APM_POWER)		+= apm_power.o
#ifdef CONFIG_HS03S_SUPPORT
# HS03s for SR-AL5625-01-251 by wenyaqi at 20210425 start
obj-$(CONFIG_BATTERY_ID_ADC) += battery_id_adc.o
# HS03s for SR-AL5625-01-251 by wenyaqi at 20210425 end
#else
obj-$(CONFIG_BATTERY_ID_ADC) += battery_id_adc.o
#endif
obj-$(CONFIG_AXP20X_POWER)	+= axp20x_usb_power.o
obj-$(CONFIG_MAX8925_POWER)	+= max8925_power.o
obj-$(CONFIG_WM831X_BACKUP)	+= wm831x_backup.o
obj-$(CONFIG_WM831X_POWER)	+= wm831x_power.o
obj-$(CONFIG_WM8350_POWER)	+= wm8350_power.o
obj-$(CONFIG_TEST_POWER)	+= test_power.o

obj-$(CONFIG_BATTERY_88PM860X)	+= 88pm860x_battery.o
obj-$(CONFIG_CHARGER_ADP5061)	+= adp5061.o
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
obj-$(CONFIG_MANAGER_SBS)	+= sbs-manager.o
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
obj-$(CONFIG_CHARGER_MANAGER)	+= charger-manager.o
obj-$(CONFIG_CHARGER_LTC3651)	+= ltc3651-charger.o
obj-$(CONFIG_CHARGER_MAX14577)	+= max14577_charger.o
obj-$(CONFIG_CHARGER_DETECTOR_MAX14656)	+= max14656_charger_detector.o
obj-$(CONFIG_CHARGER_MAX77693)	+= max77693_charger.o
obj-$(CONFIG_CHARGER_MAX8997)	+= max8997_charger.o
obj-$(CONFIG_CHARGER_MAX8998)	+= max8998_charger.o
obj-$(CONFIG_CHARGER_QCOM_SMBB)	+= qcom_smbb.o
obj-$(CONFIG_CHARGER_BQ2415X)	+= bq2415x_charger.o
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
obj-$(CONFIG_CHARGER_BQ25601) += bq25601.o
obj-$(CONFIG_CHARGER_RT9458) += rt9458.o
obj-$(CONFIG_CHARGER_RT9465) += rt9465.o
obj-$(CONFIG_CHARGER_RT9466) += rt9466.o
obj-$(CONFIG_CHARGER_RT9467) += rt9467.o
obj-$(CONFIG_CHARGER_RT9471) += rt9471.o
# HS03s for SR-AL5625-01-509 by wenyaqi at 20210527 start
obj-$(CONFIG_CHARGER_BQ2560X) += bq2560x_charger.o
# HS03s for SR-AL5625-01-509 by wenyaqi at 20210527 end
# HS03s for SR-AL5625-01-510 by wenyaqi at 20210531 start
obj-$(CONFIG_CHARGER_SGM41511) += sgm41511_charger.o
# HS03s for SR-AL5625-01-510 by wenyaqi at 20210531 end
# hs03s_NM code for SR-AL5625-01-646 by shixuanxuan at 20220414 start
obj-$(CONFIG_CHARGER_SGM41513) += sgm41513_charger.o
# hs03s_NM code for SR-AL5625-01-646 by shixuanxuan at 20220414 end
# HS03s for SR-AL5625-01-543 by wenyaqi at 20210611 start
obj-$(CONFIG_CHARGER_ETA6953) += eta6953_charger.o
# HS03s for SR-AL5625-01-543 by wenyaqi at 20210611 end
# HS03s for SR-AL5625-01-542 by wenyaqi at 20210609 start
obj-$(CONFIG_CHARGER_SY6974) += sy6974_charger.o
# HS03s for SR-AL5625-01-542 by wenyaqi at 20210609 end
obj-$(CONFIG_CHARGER_MT6360) += mt6360_charger.o
obj-$(CONFIG_BATTERY_MT6357) += mt6357_battery.o
mt6357_battery-objs := mtk_battery_coulomb_service.o mtk_battery.o mtk_battery_algo.o  mt6357-gauge.o mtk_battery_daemon.o
obj-$(CONFIG_BATTERY_MT6359) += mt6359_battery.o
mt6359_battery-objs := mtk_battery_coulomb_service.o mtk_battery.o mtk_battery_algo.o mtk_battery_daemon.o mt6359-gauge.o
obj-$(CONFIG_CHARGER_TYPE_MT6357) += mt6357-charger-type.o

#ifdef CONFIG_HS03S_SUPPORT
obj-$(CONFIG_MTK_CHARGER) += charger_class.o mtk_basic_charger.o mtk_charger.o mtk_charger_algorithm_class.o mtk_charger_intf.o adapter_class.o mtk_pd_adapter.o mtk_pd.o mtk_pd_hal.o mtk_pulse_charger.o
obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT) += mtk_pe2.o mtk_pe2_hal.o
obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT) += mtk_pe.o mtk_pe_hal.o
obj-$(CONFIG_MTK_PUMP_EXPRESS_40_SUPPORT) += mtk_pe4.o mtk_pe4_hal.o
# HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start
obj-$(CONFIG_AFC_CHARGER) += afc_charger_intf.o
# HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end

#else

# TabA7 Lite code for OT8-89 add USB temp protection by wenyaqi at 20201213 start
# obj-$(CONFIG_MTK_CHARGER) += charger_class.o mtk_basic_charger.o mtk_charger.o mtk_charger_algorithm_class.o mtk_charger_intf.o adapter_class.o mtk_pd_adapter.o mtk_pd.o mtk_pd_hal.o mtk_pulse_charger.o vbus_ctrl.o
# TabA7 Lite code for OT8-89 add USB temp protection by wenyaqi at 20201213 end
# TabA7 Lite code for OT8-2093 PD and PE init before mtk_charger by wenyaqi at 20210128 start
obj-$(CONFIG_MTK_CHARGER) += mtk_pd_adapter.o mtk_pd.o mtk_pd_hal.o
obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT) += mtk_pe2.o mtk_pe2_hal.o
obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT) += mtk_pe.o mtk_pe_hal.o
obj-$(CONFIG_MTK_PUMP_EXPRESS_40_SUPPORT) += mtk_pe4.o mtk_pe4_hal.o
obj-$(CONFIG_MTK_CHARGER) += charger_class.o mtk_basic_charger.o mtk_charger.o mtk_charger_algorithm_class.o mtk_charger_intf.o adapter_class.o mtk_pulse_charger.o vbus_ctrl.o
# TabA7 Lite code for OT8-2093 PD and PE init before mtk_charger by wenyaqi at 20210128 end
# TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201201 start
obj-$(CONFIG_AFC_CHARGER) += afc_charger_intf.o
# TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201201 end

#endif
obj-$(CONFIG_CHARGER_MT6357) += mt6357-pulse-charger.o
obj-$(CONFIG_CHARGER_SM5714) += sm/sm5714/