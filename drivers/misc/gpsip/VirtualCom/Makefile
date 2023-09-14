###############################################################################
#                                LEGAL NOTICE                                 #
#                                                                             #
#  USE OF THIS SOFTWARE (including any copy or compiled version thereof) AND  #
#  DOCUMENTATION IS SUBJECT TO THE SOFTWARE LICENSE AND RESTRICTIONS AND THE  #
#  WARRANTY DISLCAIMER SET FORTH IN LEGAL_NOTICE.TXT FILE. IF YOU DO NOT      #
#  FULLY ACCEPT THE TERMS, YOU MAY NOT INSTALL OR OTHERWISE USE THE SOFTWARE  #
#  OR DOCUMENTATION.                                                          #
#  NOTWITHSTANDING ANYTHING TO THE CONTRARY IN THIS NOTICE, INSTALLING OR     #
#  OTHERISE USING THE SOFTWARE OR DOCUMENTATION INDICATES YOUR ACCEPTANCE OF  #
#  THE LICENSE TERMS AS STATED.                                               #
#                                                                             #
###############################################################################
# Version: 1.8.9/3686 
# Build  : 13 
# Date   : 12/08/2012 


ANDROID_NDK_ROOT := $(ROOT)/android/ndk
ANDROID_SDK_ROOT := $(ROOT)/android/sdk
PATH := $(PREBUILD)/bin:$(PATH)

obj-$(CONFIG_CGX_VIRTUALCOM)          := cgvuart.o

# C Object Files:
cgvuart-c-objs   += virt_uart.o
cgvuart-objs     += $(cgvuart-c-objs)

# EXTRA_CFLAGS    += -I$(PWD)


ifeq ($(KERNELRELEASE),)
# on first call from remote location we get into this path
# whilst on second call all is managed by the embedding kernel makefile

cgvuart-cfiles    = ${cgvuart-c-objs:.o=.c}

default:: kmod_build

kmod_build:: $(cgvuart-cfiles) $(cgvuart-hdrs)
	$(MAKE) -C $(CG_KERNEL_DIR) SUBDIRS=$(CURDIR) modules

# distclean:: clean

clean::
	rm -f *.o *.ko .*.cmd cgvuart.mod.c
	rm -rf .tmp_versions

tags::
	$(MAKE) -C $(CG_KERNEL_DIR) SUBDIRS=$(CURDIR) tags
endif

# ### EOF ###
