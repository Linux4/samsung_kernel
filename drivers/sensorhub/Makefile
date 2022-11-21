#
# Makefile for the sensor hub.
#
ifdef CONFIG_SENSORS_SSP_NOBLELTE
	obj-$(CONFIG_SENSORS_SSP_BBD)	+= brcm/
	obj-$(CONFIG_SENSORS_SSP_STM)	+= stm/
else
	ifdef CONFIG_SENSORS_SSP_ZENLTE
		obj-$(CONFIG_SENSORS_SSP_BBD)	+= brcm/
		obj-$(CONFIG_SENSORS_SSP_STM)	+= stm/
	else
		ifdef CONFIG_SENSORS_SSP_VLTE
			obj-$(CONFIG_SENSORS_SSP_BBD)	+= brcm/
			obj-$(CONFIG_SENSORS_SSP_STM)	+= stm/
		else
			obj-$(CONFIG_SENSORS_SSP_BBD)	+= brcm_legacy/
			obj-$(CONFIG_SENSORS_SSP_STM)	+= stm_legacy/
		endif
	endif
endif

obj-$(CONFIG_SENSORS_SSP_STM32)	+= stm32/
obj-$(CONFIG_SENSORS_SSP_ATMEL)	+= atmel/
