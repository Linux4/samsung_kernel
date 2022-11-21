#
# Makefile for the VIPx common driver
#

obj-y						+= vertex-binary.o
obj-y						+= vertex-compat-ioctl.o
obj-y						+= vertex-ioctl.o
obj-y						+= vertex-debug.o
obj-y						+= vertex-device.o
obj-y						+= vertex-graph.o
obj-y						+= vertex-graphmgr.o
obj-y						+= vertex-interface.o
obj-y						+= vertex-io.o
obj-y						+= vertex-mailbox.o
obj-y						+= vertex-memory.o
obj-y						+= vertex-queue.o
obj-y						+= vertex-system.o
obj-y						+= vertex-taskmgr.o
obj-y						+= vertex-time.o
obj-y						+= vertex-core.o
obj-y						+= vertex-context.o
obj-y						+= vertex-util.o

obj-$(CONFIG_EXYNOS_VIPX_VERTEX_PLATFORM)	+= platform/

ccflags-y += -Idrivers/vision/vipx_vertex
