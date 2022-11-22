subdir-ccflags-y := -Wformat
obj-$(CONFIG_MUIC_SM5714)	+= sm5714-muic.o sm5714-muic-afc.o
ifeq ($(CONFIG_MUIC_SUPPORT_PDIC), y)
obj-$(CONFIG_MUIC_SM5714)	+= sm5714-muic-pdic.o
endif
