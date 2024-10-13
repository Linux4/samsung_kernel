ifneq ($(CONFIG_UML), y)
# Test file for device
obj-$(CONFIG_SEC_PMSG_TEST_FOR_ON_DEVICE)	+= sec_pmsg_test.o \
						   sec_pmsg_test.dtb.o
else
# Test file for UML
obj-$(CONFIG_SEC_PMSG_TEST_FOR_ONLY_UML)	+= sec_pmsg_test.o \
						   sec_pmsg_test.dtb.o
endif
