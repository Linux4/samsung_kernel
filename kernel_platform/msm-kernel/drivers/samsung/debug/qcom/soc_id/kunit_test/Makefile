ifneq ($(CONFIG_UML), y)
  # Test file for device
	obj-$(CONFIG_SEC_QC_SOC_ID_TEST_FOR_ONLY_DEVICE)	+= sec_qc_soc_id_test.o \
								   sec_qc_soc_id_test.dtb.o
else
  # Test file for UML
	obj-$(CONFIG_SEC_QC_SOC_ID_TEST_FOR_ONLY_UML)		+= sec_qc_soc_id_test.o \
								   sec_qc_soc_id_test.dtb.o
endif
