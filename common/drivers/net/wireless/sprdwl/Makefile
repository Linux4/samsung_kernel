ccflags-y += -DDEBUG
#ccflags-y += -DDUMP_TRANSMIT_PACKET
#ccflags-y += -DDUMP_RECEIVE_PACKET
#ccflags-y += -DDUMP_COMMAND
#ccflags-y += -DDUMP_COMMAND_RESPONSE
#ccflags-y += -DDUMP_EVENT

ccflags-y += -DCONFIG_SPRDWL_ENHANCED_PM

obj-$(CONFIG_SPRDWL) += sprdwl.o

sprdwl-objs:= main.o cfg80211.o sipc.o \
	      hostap_conf.o npi.o wifi_nvm.o \
	      wapi.o \
	      $(sprdwl-y)
