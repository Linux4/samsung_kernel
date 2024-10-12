# SPDX-License-Identifier: GPL-2.0
#
# Makefile for misc devices that really don't fit anywhere else.
#

obj-$(CONFIG_IBM_ASM)		+= ibmasm/
obj-$(CONFIG_AD525X_DPOT)	+= ad525x_dpot.o
obj-$(CONFIG_AD525X_DPOT_I2C)	+= ad525x_dpot-i2c.o
obj-$(CONFIG_AD525X_DPOT_SPI)	+= ad525x_dpot-spi.o
obj-$(CONFIG_INTEL_MID_PTI)	+= pti.o
obj-$(CONFIG_ATMEL_SSC)		+= atmel-ssc.o
obj-$(CONFIG_ATMEL_TCLIB)	+= atmel_tclib.o
obj-$(CONFIG_DUMMY_IRQ)		+= dummy-irq.o
obj-$(CONFIG_ICS932S401)	+= ics932s401.o
obj-$(CONFIG_LKDTM)		+= lkdtm.o
obj-$(CONFIG_TIFM_CORE)       	+= tifm_core.o
obj-$(CONFIG_TIFM_7XX1)       	+= tifm_7xx1.o
obj-$(CONFIG_PHANTOM)		+= phantom.o
obj-$(CONFIG_QCOM_COINCELL)	+= qcom-coincell.o
obj-$(CONFIG_SENSORS_BH1770)	+= bh1770glc.o
obj-$(CONFIG_SENSORS_APDS990X)	+= apds990x.o
obj-$(CONFIG_SGI_IOC4)		+= ioc4.o
obj-$(CONFIG_ENCLOSURE_SERVICES) += enclosure.o
obj-$(CONFIG_KGDB_TESTS)	+= kgdbts.o
obj-$(CONFIG_SGI_XP)		+= sgi-xp/
obj-$(CONFIG_SGI_GRU)		+= sgi-gru/
obj-$(CONFIG_CS5535_MFGPT)	+= cs5535-mfgpt.o
obj-$(CONFIG_HP_ILO)		+= hpilo.o
obj-$(CONFIG_APDS9802ALS)	+= apds9802als.o
obj-$(CONFIG_ISL29003)		+= isl29003.o
obj-$(CONFIG_ISL29020)		+= isl29020.o
obj-$(CONFIG_SENSORS_TSL2550)	+= tsl2550.o
obj-$(CONFIG_DS1682)		+= ds1682.o
obj-$(CONFIG_TI_DAC7512)	+= ti_dac7512.o
obj-$(CONFIG_C2PORT)		+= c2port/
obj-$(CONFIG_HMC6352)		+= hmc6352.o
obj-y				+= eeprom/
obj-y				+= cb710/
obj-$(CONFIG_SPEAR13XX_PCIE_GADGET)	+= spear13xx_pcie_gadget.o
obj-$(CONFIG_VMWARE_BALLOON)	+= vmw_balloon.o
obj-$(CONFIG_PCH_PHUB)		+= pch_phub.o
obj-y				+= ti-st/
obj-y				+= lis3lv02d/
obj-$(CONFIG_USB_SWITCH_FSA9480) += fsa9480.o
obj-$(CONFIG_ALTERA_STAPL)	+=altera-stapl/
obj-$(CONFIG_INTEL_MEI)		+= mei/
obj-$(CONFIG_VMWARE_VMCI)	+= vmw_vmci/
obj-$(CONFIG_LATTICE_ECP3_CONFIG)	+= lattice-ecp3-config.o
obj-$(CONFIG_SRAM)		+= sram.o
obj-$(CONFIG_SRAM_EXEC)		+= sram-exec.o
obj-y				+= mic/
obj-$(CONFIG_GENWQE)		+= genwqe/
obj-$(CONFIG_ECHO)		+= echo/
obj-$(CONFIG_VEXPRESS_SYSCFG)	+= vexpress-syscfg.o
obj-$(CONFIG_CXL_BASE)		+= cxl/
obj-$(CONFIG_SPRD_VSP)          += sprd_vsp/
obj-$(CONFIG_SPRD_VPU)          += sprd_vpu/
obj-$(CONFIG_SPRD_JPG)          += sprd_jpg/
obj-$(CONFIG_ASPEED_LPC_CTRL)	+= aspeed-lpc-ctrl.o
obj-$(CONFIG_ASPEED_LPC_SNOOP)	+= aspeed-lpc-snoop.o
obj-$(CONFIG_PCI_ENDPOINT_TEST)	+= pci_endpoint_test.o
obj-y				+= sprdwcn/
obj-m				+= sprd_wcn_n6pro/
obj-$(CONFIG_UID_SYS_STATS)	+= uid_sys_stats.o
obj-$(CONFIG_SPRD_UID)		+= sprd_uid.o
obj-$(CONFIG_MEMORY_STATE_TIME)	+= memory_state_time.o
obj-y				+= sprd_apdu/
# Tab A8 code for SR-AX6300-01-207 by wangdeyan at 20210812 start
obj-$(CONFIG_GRDM_POWER)          += grdm/
# Tab A8 code for SR-AX6300-01-207 by wangdeyan at 20210812 end

lkdtm-$(CONFIG_LKDTM)		+= lkdtm_core.o
lkdtm-$(CONFIG_LKDTM)		+= lkdtm_bugs.o
lkdtm-$(CONFIG_LKDTM)		+= lkdtm_heap.o
lkdtm-$(CONFIG_LKDTM)		+= lkdtm_perms.o
lkdtm-$(CONFIG_LKDTM)		+= lkdtm_refcount.o
lkdtm-$(CONFIG_LKDTM)		+= lkdtm_rodata_objcopy.o
lkdtm-$(CONFIG_LKDTM)		+= lkdtm_usercopy.o
obj-y 				+= sprd_audio/
obj-y				+= sprd_map/
obj-$(CONFIG_SPRD_SOCID)	+= sprd_soc_id.o
# HS03 code for SR-SL6215-01-233 by sunxunou at 2021/08/02 start
obj-y                           += huaqin/
# HS03 code for SR-SL6215-01-233 by sunxunou at 2021/08/02 end
# Tab A8 code for SR-AX6300-01-78 by xiongxiaoliang at 2021/08/09 start
obj-$(CONFIG_INPUT_SAR5) += sar/sx933x/
# Tab A8 code for SR-AX6300-01-78 by xiongxiaoliang at 2021/08/09 end
# Tab A8 code for SR-AX6300-01-87 by xiongxiaoliang at 2021/08/12 start
obj-$(CONFIG_INPUT_HALL) += hall/
# Tab A8 code for SR-AX6300-01-87 by xiongxiaoliang at 2021/08/12 end

# HS03 code for SR-SL6215-01-165 by shibinbin at 2021/08/09 start
obj-$(CONFIG_AW9610X_SAR)    += hs03_sar/
# HS03 code for SR-SL6215-01-165 by shibinbin at 2021/08/09 end

#Tab A8 code for SR-AX6300-01-81 by mayuhang at 2021/8/12 start
obj-$(CONFIG_INPUT_HX9031A) += sar/hx9031/
#Tab A8 code for SR-AX6300-01-81 by mayuhang at 2021/8/12 end
#Tab A8 code for AX6300DEV-457 by mayuhang at 2021/9/18 start
obj-y += sensors_class/
#Tab A8 code for AX6300DEV-457 by mayuhang at 2021/9/18 end
# Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 start
obj-$(CONFIG_INPUT_HX9031A) += sar/
# Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 end
# HS03 code for P211007-03235 by zhangziyi at 2021/10/12 start
obj-$(CONFIG_LED_SC27XX_VIBRA) += vibrator/
# HS03 code for P211007-03235 by zhangziyi at 2021/10/12 end
# Tab A7 T618 code for SR-AX6189A-01-178 by zhangziyi at 2021/12/19 start
obj-$(CONFIG_AW96103A_SAR) += aw96103A_sar/
# Tab A7 T618 code for SR-AX6189A-01-178 by zhangziyi at 2021/12/19 end
KCOV_INSTRUMENT_lkdtm_rodata.o	:= n
CFLAGS_lkdtm_rodata.o		+= $(DISABLE_LTO)

OBJCOPYFLAGS :=
OBJCOPYFLAGS_lkdtm_rodata_objcopy.o := \
	--rename-section .text=.rodata,alloc,readonly,load
targets += lkdtm_rodata.o lkdtm_rodata_objcopy.o
$(obj)/lkdtm_rodata_objcopy.o: $(obj)/lkdtm_rodata.o FORCE
	$(call if_changed,objcopy)
