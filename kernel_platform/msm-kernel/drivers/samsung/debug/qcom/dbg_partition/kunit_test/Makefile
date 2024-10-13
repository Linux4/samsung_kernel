ifneq ($(CONFIG_UML), y)
obj-$(CONFIG_SEC_QC_DBG_PARTITION_TEST_FOR_ON_DEVICE)	+= sec_qc_dbg_partition_test.o \
							   sec_qc_dbg_partition_test.dtb.o
else
obj-$(CONFIG_SEC_QC_DBG_PARTITION_TEST_FOR_ONLY_UML)	+= sec_qc_dbg_partition_test.o \
							   sec_qc_dbg_partition_test.dtb.o
endif
