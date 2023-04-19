pablo-libs-objs := pablo-lib.o \
		pablo-debug.o \
		pablo-mem.o \
		pablo-kernel-variant.o \
		pablo-work.o \
		pablo-hw-api-common.o \
		pablo-fpsimdmacro.o \
		pablo-fpsimd.o \
		pablo-uh_call.o \
		pablo-binary.o \
		pablo-framemgr.o \
		pablo-irq.o \
		pablo-mmio.o \
		pmio-cache.o \
		pablo-stripe.o \
		pablo-internal-subdev-ctrl.o

pablo-libs-$(CONFIG_PABLO_SOCKET_LAYER) += pablo-sock.o
pablo-libs-$(CONFIG_PABLO_OBTE_SUPPORT) += pablo-obte.o

obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += pablo-libs.o
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_LIB_VOTF) += votf/
# TODO: deprecate CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_VOTF
obj-$(CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_VOTF) += votf/

ccflags-y := -DDYNAMIC_DEBUG_MODULE
