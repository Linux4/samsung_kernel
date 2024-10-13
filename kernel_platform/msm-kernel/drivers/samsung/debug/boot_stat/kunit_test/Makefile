ifneq ($(CONFIG_UML), y)
	# Test file for device
	obj-$(SEC_BOOT_STAT_TEST_FOR_ON_DEVICE)		+= sec_boot_stat_main_test.o \
							   sec_boot_stat_proc_test.o
else
	# Test file for UML
	obj-$(CONFIG_SEC_BOOT_STAT_TEST_FOR_ONLY_UML)	+= sec_boot_stat_main_test.o \
							   sec_boot_stat_proc_test.o
endif