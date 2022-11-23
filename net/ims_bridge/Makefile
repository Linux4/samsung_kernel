#
# Makefile for the ims bridge kernel modules
#
GCOV_PROFILE = ${CONFIG_SPRD_IMS_BRIDGE_GCOV}

obj-$(CONFIG_SPRD_IMS_BRIDGE) += ims_bridge.o

ims_bridge-y := imsbr_main.o imsbr_hooks.o imsbr_sipc.o imsbr_packet.o \
		imsbr_core.o imsbr_netlink.o imsbr_test.o
