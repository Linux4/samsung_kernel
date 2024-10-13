ifneq ($(CONFIG_UML), y)
  # Test file for device
	obj-$(CONFIG_SEC_CRASHKEY_TEST_FOR_ON_DEVICE)	+= sec_crashkey_test.o \
							   sec_crashkey_test.dtb.o \
							   sec_crashkey_te01.o \
							   sec_crashkey_te01.dtb.o
else
  # Test file for UML
	obj-$(CONFIG_SEC_CRASHKEY_TEST_FOR_ONLY_UML)	+= sec_crashkey_test.o \
							   sec_crashkey_test.dtb.o \
							   sec_crashkey_te01.o \
							   sec_crashkey_te01.dtb.o
endif
