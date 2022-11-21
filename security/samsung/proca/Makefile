SHELL := /bin/bash

# For kernel version 4.14 or lower
ifeq ($(CONFIG_MACH_MT6853),y)
$(obj)/proca_certificate-asn1.o: $(obj)/proca_certificate-asn1.c \
                               $(obj)/proca_certificate-asn1.h

$(obj)/proca_certificate.o: $(obj)/proca_certificate-asn1.h

obj-$(CONFIG_PROCA) += proca_identity.o proca_lsm.o proca_fcntl.o \
                               proca_task_descr.o proca_certificate-asn1.o \
                               proca_certificate.o proca_table.o \
                               proca_config.o
else
$(obj)/proca_certificate.asn1.o: $(obj)/proca_certificate.asn1.c \
                               $(obj)/proca_certificate.asn1.h

$(obj)/proca_certificate.o: $(obj)/proca_certificate.asn1.h


obj-$(CONFIG_PROCA) += proca_identity.o proca_lsm.o proca_fcntl.o \
                               proca_task_descr.o proca_certificate.asn1.o \
                               proca_certificate.o proca_table.o \
                               proca_config.o
endif

ccflags-y += -I$(srctree)/security/samsung/five/

# clean-files is given relative to the current directory,
# so don't use (obj)/ prefix here.
# Look at file ../../scripts/Makefile.clean for details.
ifeq ($(CONFIG_MACH_MT6853),y)
clean-files += proca_certificate-asn1.c proca_certificate-asn1.h
else
clean-files += proca_certificate.asn1.c proca_certificate.asn1.h
endif

subdir-ccflags-y += -Wformat