#
# Makefile for the CMUCAL drivers.
#

# Object files in subdirectories

obj-$(CONFIG_CAL_IF)			+= cmupmucal.o
cmupmucal-y					+= cal-if.o cmucal.o ra.o vclk.o pll_spec.o cmucal-debug.o
cmupmucal-$(CONFIG_PMUCAL)			+= pmucal_system.o pmucal_local.o pmucal_cpu.o pmucal_rae.o pmucal_powermode.o
#obj-$(CONFIG_PMUCAL_DBG)		+= pmucal_dbg.o
#obj-$(CONFIG_FLEXPMU)			+= pmucal_powermode.o
cmupmucal-$(CONFIG_CP_PMUCAL)	+= pmucal_cp.o
cmupmucal-$(CONFIG_GNSS_PMUCAL)	+= pmucal_gnss.o
cmupmucal-$(CONFIG_SHUB_PMUCAL)	+= pmucal_shub.o
cmupmucal-$(CONFIG_SOC_S5E3830)	+= s5e3830/cal_data.o
cmupmucal-$(CONFIG_ACPM_DVFS)	+= acpm_dvfs.o fvmap.o
#obj-$(CONFIG_CMUCAL)			+= cmucal.o ra.o vclk.o pll_spec.o
#obj-$(CONFIG_CMUCAL_DEBUG)		+= cmucal-debug.o
#obj-$(CONFIG_CMU_EWF)			+= cmu_ewf.o

