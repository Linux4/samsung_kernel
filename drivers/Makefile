# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel device drivers.
#
# 15 Sep 2000, Christoph Hellwig <hch@infradead.org>
# Rewritten to use lists instead of if-statements.
#

obj-y				+= irqchip/
obj-y				+= bus/

obj-$(CONFIG_GENERIC_PHY)	+= phy/

# GPIO must come after pinctrl as gpios may need to mux pins etc
obj-$(CONFIG_PINCTRL)		+= pinctrl/
obj-$(CONFIG_GPIOLIB)		+= gpio/
obj-y				+= pwm/

obj-y				+= pci/

obj-$(CONFIG_PARISC)		+= parisc/
obj-$(CONFIG_RAPIDIO)		+= rapidio/
obj-y				+= video/
obj-y				+= idle/

# IPMI must come before ACPI in order to provide IPMI opregion support
obj-y				+= char/ipmi/

obj-$(CONFIG_ACPI)		+= acpi/
obj-$(CONFIG_SFI)		+= sfi/
# PnP must come after ACPI since it will eventually need to check if acpi
# was used and do nothing if so
obj-$(CONFIG_PNP)		+= pnp/
obj-y				+= amba/

obj-y				+= clk/
# Many drivers will want to use DMA so this has to be made available
# really early.
obj-$(CONFIG_DMADEVICES)	+= dma/

# SOC specific infrastructure drivers.
obj-y				+= soc/

obj-$(CONFIG_VIRTIO)		+= virtio/
obj-$(CONFIG_XEN)		+= xen/

# regulators early, since some subsystems rely on them to initialize
obj-$(CONFIG_REGULATOR)		+= regulator/

# reset controllers early, since gpu drivers might rely on them to initialize
obj-$(CONFIG_RESET_CONTROLLER)	+= reset/

# tty/ comes before char/ so that the VT console is the boot-time
# default.
obj-y				+= tty/
obj-y				+= char/

# iommu/ comes before gpu as gpu are using iommu controllers
obj-$(CONFIG_IOMMU_SUPPORT)	+= iommu/

# gpu/ comes after char for AGP vs DRM startup and after iommu
obj-y				+= gpu/

obj-$(CONFIG_CONNECTOR)		+= connector/

# i810fb and intelfb depend on char/agp/
obj-$(CONFIG_FB_I810)           += video/fbdev/i810/
obj-$(CONFIG_FB_INTEL)          += video/fbdev/intelfb/

obj-$(CONFIG_PARPORT)		+= parport/
obj-$(CONFIG_NVM)		+= lightnvm/
obj-$(CONFIG_MEMORY)		+= memory/
obj-y				+= base/ block/ misc/ mfd/ nfc/
obj-$(CONFIG_LIBNVDIMM)		+= nvdimm/
obj-$(CONFIG_DAX)		+= dax/
obj-$(CONFIG_DMA_SHARED_BUFFER) += dma-buf/
obj-$(CONFIG_NUBUS)		+= nubus/
obj-y				+= macintosh/
obj-$(CONFIG_IDE)		+= ide/
obj-y				+= scsi/
obj-y				+= nvme/
obj-$(CONFIG_ATA)		+= ata/
obj-$(CONFIG_TARGET_CORE)	+= target/
obj-$(CONFIG_MTD)		+= mtd/
obj-$(CONFIG_SPI)		+= spi/
obj-$(CONFIG_SPMI)		+= spmi/
obj-$(CONFIG_HSI)		+= hsi/
obj-$(CONFIG_SLIMBUS)		+= slimbus/
obj-y				+= net/
obj-$(CONFIG_ATM)		+= atm/
obj-$(CONFIG_FUSION)		+= message/
obj-y				+= firewire/
obj-$(CONFIG_UIO)		+= uio/
obj-$(CONFIG_VFIO)		+= vfio/
obj-y				+= cdrom/
obj-y				+= auxdisplay/
obj-$(CONFIG_PCCARD)		+= pcmcia/
obj-$(CONFIG_DIO)		+= dio/
obj-$(CONFIG_SBUS)		+= sbus/
obj-$(CONFIG_ZORRO)		+= zorro/
obj-$(CONFIG_ATA_OVER_ETH)	+= block/aoe/
obj-$(CONFIG_PARIDE) 		+= block/paride/
obj-$(CONFIG_TC)		+= tc/
obj-$(CONFIG_UWB)		+= uwb/
obj-$(CONFIG_USB_PHY)		+= usb/
obj-$(CONFIG_USB)		+= usb/
obj-$(CONFIG_SWITCH)	+= switch/
obj-$(CONFIG_USB_SUPPORT)	+= usb/
obj-$(CONFIG_PCI)		+= usb/
obj-$(CONFIG_USB_GADGET)	+= usb/
obj-$(CONFIG_OF)		+= usb/
obj-$(CONFIG_SERIO)		+= input/serio/
obj-$(CONFIG_GAMEPORT)		+= input/gameport/
obj-$(CONFIG_INPUT)		+= input/
obj-$(CONFIG_RTC_LIB)		+= rtc/
obj-y				+= i2c/ media/
obj-y				+= i3c/
obj-$(CONFIG_PPS)		+= pps/
obj-y				+= ptp/
obj-$(CONFIG_W1)		+= w1/
obj-y				+= power/
obj-$(CONFIG_HWMON)		+= hwmon/
obj-$(CONFIG_THERMAL)		+= thermal/
obj-$(CONFIG_WATCHDOG)		+= watchdog/
obj-$(CONFIG_MD)		+= md/
obj-$(CONFIG_BT)		+= bluetooth/
obj-$(CONFIG_ACCESSIBILITY)	+= accessibility/
obj-$(CONFIG_ISDN)		+= isdn/
obj-$(CONFIG_EDAC)		+= edac/
obj-$(CONFIG_EISA)		+= eisa/
obj-$(CONFIG_PM_OPP)		+= opp/
obj-$(CONFIG_CPU_FREQ)		+= cpufreq/
obj-$(CONFIG_CPU_IDLE)		+= cpuidle/
obj-y				+= mmc/
obj-$(CONFIG_MEMSTICK)		+= memstick/
obj-$(CONFIG_NEW_LEDS)		+= leds/
obj-$(CONFIG_INFINIBAND)	+= infiniband/
obj-$(CONFIG_SGI_SN)		+= sn/
obj-y				+= firmware/
obj-$(CONFIG_CRYPTO)		+= crypto/
obj-$(CONFIG_SUPERH)		+= sh/
ifndef CONFIG_ARCH_USES_GETTIMEOFFSET
obj-y				+= clocksource/
endif
obj-$(CONFIG_DCA)		+= dca/
obj-$(CONFIG_HID)		+= hid/
obj-$(CONFIG_PPC_PS3)		+= ps3/
obj-$(CONFIG_OF)		+= of/
obj-$(CONFIG_SSB)		+= ssb/
obj-$(CONFIG_BCMA)		+= bcma/
obj-$(CONFIG_VHOST_RING)	+= vhost/
obj-$(CONFIG_VHOST)		+= vhost/
obj-$(CONFIG_VLYNQ)		+= vlynq/
obj-$(CONFIG_STAGING)		+= staging/
obj-y				+= platform/

obj-$(CONFIG_MAILBOX)		+= mailbox/
obj-$(CONFIG_HWSPINLOCK)	+= hwspinlock/
obj-$(CONFIG_REMOTEPROC)	+= remoteproc/
obj-$(CONFIG_RPMSG)		+= rpmsg/
obj-$(CONFIG_SOUNDWIRE)		+= soundwire/

obj-$(CONFIG_ENERGY_MODEL)	+= energy_model/

# Virtualization drivers
obj-$(CONFIG_VIRT_DRIVERS)	+= virt/
obj-$(CONFIG_HYPERV)		+= hv/

obj-$(CONFIG_PM_DEVFREQ)	+= devfreq/
obj-$(CONFIG_EXTCON)		+= extcon/
obj-$(CONFIG_IIO)		+= iio/
obj-$(CONFIG_VME_BUS)		+= vme/
obj-$(CONFIG_IPACK_BUS)		+= ipack/
obj-$(CONFIG_NTB)		+= ntb/
obj-$(CONFIG_FMC)		+= fmc/
obj-$(CONFIG_POWERCAP)		+= powercap/
obj-$(CONFIG_MCB)		+= mcb/
obj-$(CONFIG_PERF_EVENTS)	+= perf/
obj-$(CONFIG_RAS)		+= ras/
obj-$(CONFIG_THUNDERBOLT)	+= thunderbolt/
obj-$(CONFIG_CORESIGHT)		+= hwtracing/coresight/
obj-y				+= hwtracing/intel_th/
obj-$(CONFIG_STM)		+= hwtracing/stm/
obj-$(CONFIG_ANDROID)		+= android/
obj-$(CONFIG_NVMEM)		+= nvmem/
obj-$(CONFIG_FPGA)		+= fpga/
obj-$(CONFIG_FSI)		+= fsi/
obj-$(CONFIG_TEE)		+= tee/
obj-$(CONFIG_MULTIPLEXER)	+= mux/
obj-$(CONFIG_UNISYS_VISORBUS)	+= visorbus/
obj-$(CONFIG_SIOX)		+= siox/
obj-$(CONFIG_GNSS)		+= gnss/
obj-$(CONFIG_INTERCONNECT)     += interconnect/
obj-$(CONFIG_SEC_EXT)	+= samsung/

# grip sensor
obj-y				+= sensors/

# SPU signature verify
obj-$(CONFIG_SPU_VERIFY) += spu_verify/
obj-y += vibrator/common/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sti/abc/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += muic/common/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += input/input_boost/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += fingerprint/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += hwmon/sec_thermistor/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += vibrator/dc/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += muic/sm/sm5714/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/comm/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/comm/misc/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/debug/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/factory/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/factory/accelerometer/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/factory/gyroscope/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/factory/light/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/factory/magnetometer/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/factory/pressure/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/factory/proximity/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/factory/flip_cover_detector/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/others/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/sensor/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/sensor/physical_sensor/accelerometer/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/sensor/physical_sensor/gyroscope/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/sensor/physical_sensor/magnetometer/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/sensor/physical_sensor/proximity/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/sensorhub/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/sensormanager/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/utility/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += sensorhub/vendor/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += battery/fuelgauge/sm5714_fuelgauge/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += usb/typec/sm/sm5714/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += battery/charger/sm5714_charger/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
obj-y += battery/common/ # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
