ifneq ($(CONFIG_UML), y)
obj-$(CONFIG_SEC_QC_RBCMD_MAIN_TEST_FOR_ON_DEVICE)	+= sec_qc_rbcmd_test.o \
							   sec_qc_rbcmd_test.dtb.o\
							   sec_qc_rbcmd_te01.o \
							   sec_qc_rbcmd_te01.dtb.o \
							   sec_qc_rbcmd_command_test.o
else
obj-$(CONFIG_SEC_QC_RBCMD_MAIN_TEST_FOR_ONLY_UML)	+= sec_qc_rbcmd_test.o \
							   sec_qc_rbcmd_test.dtb.o \
							   sec_qc_rbcmd_te01.o \
							   sec_qc_rbcmd_te01.dtb.o \
							   sec_qc_rbcmd_command_test.o
endif
