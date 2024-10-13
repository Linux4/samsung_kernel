ifneq ($(CONFIG_UML), y)
obj-$(CONFIG_SEC_QC_PARAM_TEST_FOR_ON_DEVICE) += sec_qc_param_test.o \
						 sec_qc_param_test.dtb.o
else
obj-$(CONFIG_SEC_QC_PARAM_TEST_FOR_ONLY_UML)  += sec_qc_param_test.o \
						 sec_qc_param_test.dtb.o
endif
