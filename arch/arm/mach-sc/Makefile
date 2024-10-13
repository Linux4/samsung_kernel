obj-y += irq.o arch_init.o arch_lock.o \
         reserve.o efuse.o common.o glb.o

obj-$(CONFIG_SC_INTERNAL_ADI) += adi.o adie_irq.o
obj-$(CONFIG_SC_INTERNAL_ADC) += adc.o
obj-$(CONFIG_SC_INTERNAL_WATCHDOG) += watchdog.o
ifeq ($(CONFIG_OF),y)
obj-$(CONFIG_ARCH_SCX35) += clock-sc8830_dt.o
else
obj-$(CONFIG_ARCH_SCX35) += clock-sc8830.o
endif
obj-$(CONFIG_ARCH_SCX35) += devices-sc8830.o io-sc8830.o arch_misc.o pin_switch.o
obj-$(CONFIG_ARCH_SCX35) += timer_sc8830.o dma_r4p0.o
obj-$(CONFIG_MACH_SCX35_DT) += mach-scx35-dt.o

ifeq ($(CONFIG_ARCH_SCX30G),y)
obj-$(CONFIG_PM_DEVFREQ) += dmc_freq_28nm.o dmc_dfs_test.o
obj-$(CONFIG_SCX35_DMC_FREQ_AP) += dmc_freq_ap_28nm.o
	CFLAGS_dmc_freq_ap_28nm.o += -fPIC
else
obj-$(CONFIG_PM_DEVFREQ) += dmc_freq.o
obj-$(CONFIG_SCX35_DMC_FREQ_AP) += dmc_freq_ap.o
endif

obj-$(CONFIG_PM) += pm.o
obj-$(CONFIG_FIX_V7TAGRAM_BUG) += fix_v7_tag_ram_bug.o
obj-$(CONFIG_CPU_FREQ) += cpufreq-scx35.o
ifeq ($(CONFIG_BUS_MONITOR),y)
obj-$(CONFIG_ARCH_SCX35) += bm_sc8830.o
endif

ifeq ($(CONFIG_CPU_IDLE),y)
	obj-$(CONFIG_ARCH_SCX35) += cpuidle-scx35.o
endif

ifeq ($(CONFIG_PM),y)
	obj-$(CONFIG_ARCH_SCX35) += pm-scx35.o standby-scx35.o pm_debug_scx35.o
endif
obj-$(CONFIG_SPRD_DCDC_DEBUG) += dcdc_debug.o
obj-$(CONFIG_SMP) += platsmp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU) += hotplug.o
obj-$(CONFIG_SC_VIBRATOR) += vibrator.o
obj-$(CONFIG_SPRD_VIBRATOR_2723) += sprd_vibrator_2723.o
obj-$(CONFIG_SIPC) += sipc.o
ifeq ($(CONFIG_BCM4343),y)
obj-y += board-tshark-wifi.o
endif

obj-$(CONFIG_SEC_DEBUG) += sec_common.o sec_debug.o sec_gaf.o sec_getlog.o sec-reboot.o
obj-$(CONFIG_SEC_LOG) += sec_log.o
obj-$(CONFIG_SEC_BSP) += sec_bsp.o

obj-$(CONFIG_MACH_SPX15) += board-spx15.o board_common_battery.o
obj-$(CONFIG_MACH_SPX35EB) += board-sp8830eb.o board_common_battery.o
obj-$(CONFIG_MACH_SP8835EB) += board-sp8835eb.o board_common_battery.o
obj-$(CONFIG_MACH_SPX15FPGA) += board-sp8830fpga.o pinmap-sp8830fpga.o board_common_battery.o
obj-$(CONFIG_MACH_SPX35EC) += board-sp8830ec.o board_common_battery.o
obj-$(CONFIG_MACH_SC9620OPENPHONE) += board-sc9620openphone.o board_common_battery.o
obj-$(CONFIG_MACH_SC9620OPENPHONE_ZT) += board-sc9620openphone_zt.o board_common_battery.o
obj-$(CONFIG_MACH_SC9620FPGA) += board-sc9620fpga.o board_common_battery.o
obj-$(CONFIG_MACH_SP8830GEA) += board-sp8830gea.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_SP8730SEA) += board-sp8730sea.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_SP7730GGA_LC) += board-sp7730gga_lc.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_SP7731GGA_LC) += board-sp7731gga_lc.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_SP7730GGA) += board-sp7730gga.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_SP7730GGAOPENPHONE) += board-sp7730ggaopenphone.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_SP7731GEAOPENPHONE) += board-sp7731geaopenphone.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_SP7731GEA_FWVGA) += board-sp7731gea_fwvga.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_SP7731GEA) += board-sp7731gea.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_SP7731GEA_LC) += board-sp7731gea_lc.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_TSHARKWSAMSUNG) += board-tsharkwsamsung.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_CORE3_W) += dma_r4p0.o board_common_battery.o syssleep.o
obj-$(CONFIG_MACH_GRANDNEOVE3G) += dma_r4p0.o board_common_battery.o syssleep.o
obj-$(CONFIG_MACH_SHANGHAI_W) += board-shanghai3g_native.o dma_r4p0.o board_common_battery.o
obj-$(CONFIG_MACH_YOUNG23GDTV) += dma_r4p0.o board_common_battery.o syssleep.o
obj-$(CONFIG_MACH_VIVALTO5MVE3G) += dma_r4p0.o board_common_battery.o syssleep.o
obj-$(CONFIG_MACH_GOYAVE3G) += dma_r4p0.o board_common_battery.o syssleep.o
obj-$(CONFIG_MACH_GOYAVEWIFI) += dma_r4p0.o board_common_battery.o syssleep.o
obj-$(CONFIG_MACH_J13G) += dma_r4p0.o board_common_battery.o syssleep.o
obj-$(CONFIG_MACH_KANAS_W) += board-kanas_w.o board_common_battery.o
obj-$(CONFIG_MACH_KANAS_TD) += board-kanas_td.o board_common_battery.o
obj-$(CONFIG_MACH_SP7715EATRISIM) += board-sp7715eatrisim.o board_common_battery.o
obj-$(CONFIG_MACH_SP7715EA) += board-sp7715ea.o board_common_battery.o
obj-$(CONFIG_MACH_SP7715EAOPENPHONE) += board-sp7715eaopenphone.o board_common_battery.o
obj-$(CONFIG_MACH_SP7715GATRISIM) += board-sp7715gatrisim.o board_common_battery.o
obj-$(CONFIG_MACH_SP7715GA) += board-sp7715ga.o board_common_battery.o
obj-$(CONFIG_MACH_SP6815GA) += board-sp6815ga.o board_common_battery.o
obj-$(CONFIG_MACH_SP8815GA) += board-sp8815ga.o board_common_battery.o
obj-$(CONFIG_MACH_SP8815GAOPENPHONE) += board-sp8815gaopenphone.o board_common_battery.o
obj-$(CONFIG_MACH_POCKET2) += board-pocket2.o board_common_battery.o
obj-$(CONFIG_MACH_CORSICA_VE) += board-corsica_ve.o board_common_battery.o
obj-$(CONFIG_MACH_YOUNG2) += board-young2.o board_common_battery.o
obj-$(CONFIG_MACH_HIGGS) += board-higgs.o board_common_battery.o
obj-$(CONFIG_MACH_VIVALTO) += board-vivalto.o board_common_battery.o
obj-$(CONFIG_MACH_SP5735EA) += board-sp5735ea.o board_common_battery.o
obj-$(CONFIG_MACH_SP5735C1EA) += board-sp5735c1ea.o board_common_battery.o
obj-$(CONFIG_MACH_SP8830GA) += board-sp8830ga.o board_common_battery.o
obj-$(CONFIG_MACH_SP7730GA) += board-sp7730ga.o board_common_battery.o
obj-$(CONFIG_MACH_SP7730EC) += board-sp7730ec.o board_common_battery.o
obj-$(CONFIG_MACH_SP7730ECTRISIM) += board-sp7730ectrisim.o board_common_battery.o
obj-$(CONFIG_SPRD_DEBUG) += sprd_debug.o
obj-$(CONFIG_SPRD_MEM_POOL) += sprd_mem_pool.o
obj-$(CONFIG_SPRD_AVS_DEBUG) += avs_debug.o
obj-$(CONFIG_SPRD_AUX_DEBUG) += aux_debug.o
obj-$(CONFIG_GPS_BCM47520) += sec_gps_bcm47520.o
obj-$(CONFIG_SEC_THERMISTOR) += sec_thermistor.o sc7723s_thermistor.o
