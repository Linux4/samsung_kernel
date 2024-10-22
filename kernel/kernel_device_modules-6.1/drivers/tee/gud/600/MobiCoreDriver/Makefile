# Copyright (c) 2013-2022 TRUSTONIC LIMITED
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

#
# Makefile for the Kinibi core driver
#

GUD_ROOT_FOLDER := $(DEVICE_MODULES_PATH)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)

# add our modules to kernel.
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += mcDrvModule.o

mcDrvModule-y := \
	admin.o \
	client.o \
	clientlib.o \
	clock.o \
	fastcall.o \
	ffa_dummy.o \
	iwp.o \
	logging.o \
	main.o \
	mcp.o \
	mmu.o \
	nq.o \
	protocol.o \
	protocol_be.o \
	protocol_fe.o \
	session.o \
	teeclientapi.o \
	user.o \
	vlx_be.o \
	vlx_common.o \
	vlx_fe.o \
	xen_be.o \
	xen_common.o \
	xen_fe.o

# add our ffa modules to kernel.
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += mcDrvModule-ffa.o

mcDrvModule-ffa-y := \
	admin.o \
	client.o \
	clientlib.o \
	clock.o \
	fastcall.o \
	ffa.o \
	iwp.o \
	logging.o \
	main.o \
	mcp.o \
	mmu.o \
	nq.o \
	protocol.o \
	protocol_be.o \
	protocol_fe.o \
	session.o \
	teeclientapi.o \
	user.o \
	vlx_be.o \
	vlx_common.o \
	vlx_fe.o \
	xen_be.o \
	xen_common.o \
	xen_fe.o

# Release mode by default
ccflags-y += -DNDEBUG
ccflags-y += -Wno-declaration-after-statement
ccflags-y += $(KBUILD_CFLAGS_TRUSTONIC)

ccflags-$(CONFIG_TRUSTONIC_TEE_DEBUG) += -DDEBUG

# VLX Driver includes
KBUILD_CFLAGS += -I${VLM_VDRIVERS}/drivers/vlx

# MobiCore Driver includes
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver
