obj-y += irq.o arch_init.o arch_lock.o \
         reserve.o efuse.o common.o glb.o

obj-$(CONFIG_SC_INTERNAL_ADI) += adi.o adie_irq.o
obj-$(CONFIG_SC_INTERNAL_ADC) += adc.o
obj-$(CONFIG_SC_INTERNAL_WATCHDOG) += watchdog.o

obj-$(CONFIG_ARCH_SCX35) += devices-sc8830.o io-sc8830.o  clock-sc8830.o arch_misc.o pin_switch.o syssleep.o
obj-$(CONFIG_ARCH_SCX35) += timer_sc8830.o
obj-$(CONFIG_PM_DEVFREQ) += dmc_freq.o

obj-$(CONFIG_PM) += pm.o
obj-$(CONFIG_FIX_V7TAGRAM_BUG) += fix_v7_tag_ram_bug.o
obj-$(CONFIG_CPU_FREQ) += cpufreq-scx35.o
ifeq ($(CONFIG_BUS_MONITOR),y)
obj-$(CONFIG_ARCH_SCX35) += bm_sc8830.o
endif

ifeq ($(CONFIG_CPU_IDLE),y)
	obj-$(CONFIG_ARCH_SCX35) += cpuidle-scx35.o
endif
obj-y += dev-bluetooth_bcm4330.o
ifeq ($(CONFIG_PM),y)
	obj-$(CONFIG_ARCH_SCX35) += pm-scx35.o standby-scx35.o pm_debug_scx35.o
endif
obj-$(CONFIG_SCX35_DMC_FREQ_AP) += dmc_freq_ap.o
obj-$(CONFIG_SPRD_DCDC_DEBUG) += dcdc_debug.o
obj-$(CONFIG_SMP) += platsmp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU) += hotplug.o
obj-$(CONFIG_SC_VIBRATOR) += vibrator.o
obj-$(CONFIG_SC_VIBRATOR_SS) += vibrator_ss.o
obj-$(CONFIG_SIPC) += sipc.o
obj-$(CONFIG_MACH_SPX35EB) += board-sp8830eb.o dma_r4p0.o
obj-$(CONFIG_MACH_SP8835EB) += board-sp8835eb.o dma_r4p0.o
obj-$(CONFIG_MACH_SPX15FPGA) += board-sp8830fpga.o pinmap-sp8830fpga.o dma_r4p0.o
obj-$(CONFIG_MACH_SPX35EC) += board-sp8830ec.o dma_r4p0.o
obj-$(CONFIG_MACH_SC9620OPENPHONE) += board-sc9620openphone.o dma_r4p0.o
obj-$(CONFIG_MACH_SC9620FPGA) += board-sc9620fpga.o dma_r4p0.o
obj-$(CONFIG_MACH_SP8830SSW) += board-sp8830ssw.o dma_r4p0.o
obj-$(CONFIG_MACH_SP7715EA) += board-sp7715ea.o dma_r4p0.o
obj-$(CONFIG_MACH_SP7715EAOPENPHONE) += board-sp7715eaopenphone.o dma_r4p0.o
obj-$(CONFIG_MACH_SP7715GA) += board-sp7715ga.o dma_r4p0.o
obj-$(CONFIG_MACH_SP6815GA) += board-sp6815ga.o dma_r4p0.o
obj-$(CONFIG_MACH_SP8815GA) += board-sp8815ga.o dma_r4p0.o
obj-$(CONFIG_MACH_POCKET2) += board-pocket23g.o dma_r4p0.o board-pocket23g-wifi.o board-pocket23g-gps.o board-pocket23g-battery.o
obj-$(CONFIG_MACH_YOUNG2) += board-young2.o dma_r4p0.o board-young2-wifi.o board-young2-gps.o board-young2-battery.o
obj-$(CONFIG_MACH_CORSICA_VE) += board-corsicave3g.o dma_r4p0.o board-corsicave3g-wifi.o board-corsicave3g-gps.o board-corsicave3g-battery.o
obj-$(CONFIG_MACH_VIVALTO) += board-vivalto3g.o dma_r4p0.o board-vivalto3g-wifi.o board-vivalto3g-gps.o board-vivalto3g-battery.o
obj-$(CONFIG_MACH_HIGGS) += board-higgs2g.o dma_r4p0.o board-higgs2g-wifi.o board-higgs2g-battery.o
obj-$(CONFIG_MACH_SP5735EA) += board-sp5735ea.o dma_r4p0.o
obj-$(CONFIG_MACH_SP8830GA) += board-sp8830ga.o dma_r4p0.o
obj-$(CONFIG_MACH_SP7730GA) += board-sp7730ga.o dma_r4p0.o
obj-$(CONFIG_MACH_SP7730EC) += board-sp7730ec.o dma_r4p0.o
obj-$(CONFIG_MACH_SP7730ECTRISIM) += board-sp7730ectrisim.o dma_r4p0.o
obj-$(CONFIG_SPRD_DEBUG) += sprd_debug.o
obj-$(CONFIG_SPRD_MEM_POOL) += sprd_mem_pool.o
obj-$(CONFIG_SPRD_AVS_DEBUG) += avs_debug.o
obj-$(CONFIG_SPRD_AUX_DEBUG) += aux_debug.o
obj-$(CONFIG_SPRD_SIMPLE_MUIC) += simple_muic.o ss_musb_info.o
obj-$(CONFIG_SEC_DEBUG) += sec_debug.o sec_getlog.o sec-common.o sec-reboot.o
obj-$(CONFIG_SEC_LOG) += sec-log.o

obj-$(CONFIG_SEC_THERMISTOR) += sec_thermistor.o
