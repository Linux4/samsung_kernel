ifneq ($(CONFIG_UML), y)
  # Test file for device
	obj-$(CONFIG_SEC_CRASHKEY_LONG_TEST_FOR_ON_DEVICE)	+= sec_crashkey_long_test.o \
								   sec_crashkey_long_test.dtb.o
else
  # Test file for UML
	obj-$(CONFIG_SEC_CRASHKEY_LONG_TEST_FOR_ONLY_UML)	+= sec_crashkey_long_test.o \
								   sec_crashkey_long_test.dtb.o
endif
