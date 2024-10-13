obj-$(CONFIG_64BIT)		+= iomap.o
obj-$(CONFIG_ARCH_SCX35)	+= arch_init.o arch_misc.o adi.o adc.o glb.o boot_mode.o pin_switch.o
obj-$(CONFIG_SPRD_MAILBOX) += mailbox.o
obj-$(CONFIG_SC_INTERNAL_WATCHDOG) += sys_reset.o
obj-$(CONFIG_BUS_MONITOR_DEBUG)	+= busmonitor.o
obj-$(CONFIG_SC_VIBRATOR)	+= vibrator.o
#obj-$(CONFIG_GPS_LSI_S5N6420)	+= sec_gps_s5n6420.o
ifneq ($(CONFIG_64BIT),y)
obj-y	+= timer_sc8830.o
else
obj-y	+= timer_sharkl64.o
obj-$(CONFIG_SPRD_AVS_DEBUG) += avs_debug.o
endif
ifneq ($(CONFIG_DMA_SPRD),y)
	obj-$(CONFIG_ARCH_SCX35)	+=dma_r4p0.o
endif
obj-$(CONFIG_SC_INTERNAL_ADI) += adie_irq.o
obj-$(CONFIG_SPRD_DEBUG) += sprd_debug.o
ifeq ($(CONFIG_ARCH_WHALE),y)
obj-$(CONFIG_DJTAG)             += sprd_bm_djtag.o
obj-$(CONFIG_BUS_MONITOR_PERF) += sprd_bm_perf.o
endif
obj-$(CONFIG_SPRD_EIRQSOFF_DEBUG) += trace_eirqsoff.o
obj-$(CONFIG_SPRD_DEBUG_DMALOG) += sprd_debug_dmalog.o
obj-$(CONFIG_SPRD_POWEROFF) += sprd_poweroff.o
obj-$(CONFIG_PM) += pm.o
ifeq ($(CONFIG_PM),y)
ifeq ($(CONFIG_ARCH_WHALE),y)
	obj-y += pm-arm64.o  standby-arm64.o pm_debug_arm64.o
else
	obj-$(CONFIG_ARCH_SCX35) += pm-scx35.o standby-scx35.o pm_debug_scx35.o
endif
endif
obj-$(CONFIG_ARCH_SCX20) += dump_lowpower_regs.o
obj-$(CONFIG_CPU_FREQ) += cpufreq-scx35.o
obj-$(CONFIG_SPRD_VIBRATOR_2723) += sprd_vibrator_2723.o
obj-$(CONFIG_SPRD_VIBRATOR_2731) += sprd_vibrator_2731.o
obj-$(CONFIG_SPRD_SYSDUMP)  += sysdump.o
obj-$(CONFIG_SPRD_SYSDUMP)  += dumpinfo.o


ifneq ($(CONFIG_64BIT),y)

ifeq ($(CONFIG_CPU_IDLE),y)
	obj-$(CONFIG_ARCH_SCX35) += cpuidle-scx35.o
endif
obj-y += irq.o reserve.o common.o
obj-$(CONFIG_ARCH_SCX35) += clock-sc8830_dt.o
obj-$(CONFIG_ARCH_SCX35) += io-sc8830.o pin_switch.o

ifeq ($(CONFIG_ARCH_SCX30G),y)
obj-$(CONFIG_PM_DEVFREQ) += dmc_freq_28nm.o dmc_dfs_test.o
obj-$(CONFIG_SCX35_DMC_FREQ_AP) += dmc_freq_ap_28nm.o
	CFLAGS_dmc_freq_ap_28nm.o += -fPIC
else
ifeq ($(CONFIG_ARCH_SCX35L),y)
obj-y += dmc_misc.o
obj-y += dmc_freq_28nm.o
else
obj-$(CONFIG_PM_DEVFREQ) += dmc_freq.o
obj-$(CONFIG_SCX35_DMC_FREQ_AP) += dmc_freq_ap.o
endif
endif

obj-$(CONFIG_FIX_V7TAGRAM_BUG) += fix_v7_tag_ram_bug.o
obj-$(CONFIG_SPRD_DCDC_DEBUG) += dcdc_debug.o
obj-$(CONFIG_SMP) += platsmp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU) += hotplug.o
ifeq ($(CONFIG_BCMDHD),m)
obj-y += dhd_adapter.o
endif

obj-$(CONFIG_SPRD_MEM_POOL) += sprd_mem_pool.o
obj-$(CONFIG_SPRD_AUX_DEBUG) += aux_debug.o

endif

obj-$(CONFIG_SEC_DEBUG64) += sec_debug64.o sec-reboot.o
obj-$(CONFIG_SEC_LOG64) += sec_log64.o sec_getlog64.o

obj-$(CONFIG_SEC_DEBUG) += sec_common.o sec_debug.o sec_gaf.o sec_getlog.o sec-reboot.o
obj-$(CONFIG_SEC_LOG) += sec_log.o
obj-$(CONFIG_SEC_BSP) += sec_bsp.o
obj-$(CONFIG_SEC_THERMISTOR) += sec_thermistor.o sc7723s_thermistor.o