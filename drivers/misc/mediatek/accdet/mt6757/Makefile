ifeq ($(CONFIG_MTK_TC10_FEATURE), y)
ifeq ($(CONFIG_SEC_JACK_EXTENSION),y)
	obj-y := accdet_tc10.o sec_jack_ex.o
else
	obj-y := accdet_tc10.o
endif
else
	obj-$(CONFIG_MTK_ACCDET) := accdet.o
endif
obj-$(CONFIG_TS3A225E_ACCDET) += ts3a225e.o
