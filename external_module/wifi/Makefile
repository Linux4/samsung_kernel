export ARCH ?= arm
BUILD ?= debug
ccflags-y += -DCONFIG_MACH_SAMSUNG

ifneq ($(KERNELRELEASE),)
# Inside the kernel build system

EXTRA_CFLAGS += -I$(KBUILD_EXTMOD)

SRC = wlan_core.c wlan_fifo.c wlan_msg_q.c wlan_cmd.c wlan_npi.c wlan_hostapd_conf.c wlan_cfg80211.c wlan_tcp.c wlan_wapi.c wlan_qos.c

MODULE:=sprdwl.ko

obj-m := $(MODULE:.ko=.o)
$(MODULE:.ko=-y) := $(SRC:.c=.o)

$(MODULE:.ko=-objs) := $(SRC:.c=.o) 

else
# Outside the kernel build system
#
#

# Get any user defined KDIR-<names> or maybe even a hardcoded KDIR
-include KDIR_CONFIGURATION

# Define host system directory
KDIR-$(shell uname -m):=/lib/modules/$(shell uname -r)/build

ifeq ($(ARCH), arm)
	# when compiling for ARM we're cross compiling
	export CROSS_COMPILE ?= arm-none-linux-gnueabi-
	CONFIG ?= arm
else
	# Compiling for the host
	CONFIG ?= $(shell uname -m)
endif

# default cpu to select
CPU ?= $(shell uname -m)

# look up KDIR based om CPU selection
KDIR ?= $(KDIR-$(CPU))
export CONFIG_ITTIAM := m
ifeq ($(KDIR),)
$(error No KDIR found for platform $(CPU))
endif

all:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR)

kernelrelease:
	$(MAKE) -C $(KDIR) kernelrelease

clean:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) clean

endif
