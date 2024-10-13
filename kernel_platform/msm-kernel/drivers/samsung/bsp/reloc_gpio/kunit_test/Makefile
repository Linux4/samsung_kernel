ifneq ($(CONFIG_UML), y)
  # Test file for device
	obj-$(CONFIG_SEC_RELOC_GPIO_TEST_FOR_ON_DEVICE)	+= sec_reloc_gpio_test.o \
							   sec_reloc_gpio_test.dtb.o
else
  # Test file for UML
	obj-$(CONFIG_SEC_RELOC_GPIO_TEST_FOR_ONLY_UML)	+= sec_reloc_gpio_test.o \
							   sec_reloc_gpio_test.dtb.o
endif
