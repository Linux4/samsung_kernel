ifneq ($(CONFIG_UML), y)
# Test file for device
obj-$(SEC_REBOOT_CMD_TEST_FOR_ON_DEVICE)	+= sec_reboot_cmd_test.o \
						   sec_reboot_cmd_test.dtb.o
else
# Test file for UML
obj-$(CONFIG_SEC_REBOOT_CMD_TEST_FOR_ONLY_UML)	+= sec_reboot_cmd_test.o \
						   sec_reboot_cmd_test.dtb.o
endif