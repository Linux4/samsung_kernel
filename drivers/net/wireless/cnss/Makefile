# Makefile for CNSS platform driver

ifeq ($(CONFIG_GPIO_PCAL6416A),y)
	EXTRA_CFLAGS += -DUSE_GPIO_EXPANDER_WLEN_EN
endif

cnsscore-objs += cnss.o ../wcnss/qcomwlan_secif.o
obj-$(CONFIG_CNSS) += cnsscore.o
obj-$(CONFIG_WCNSS_MEM_PRE_ALLOC) += ../wcnss/wcnss_prealloc.o