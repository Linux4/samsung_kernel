ifneq ($(CONFIG_UML), y)
# Test file for device
obj-$(CONFIG_SEC_UPLOAD_CAUSE_TEST_FOR_ON_DEVICE)	+= sec_upload_cause_test.o \
							   sec_upload_cause_test.dtb.o
else
# Test file for UML
obj-$(CONFIG_SEC_UPLOAD_CAUSE_TEST_FOR_ONLY_UML)	+= sec_upload_cause_test.o \
							   sec_upload_cause_test.dtb.o
endif
