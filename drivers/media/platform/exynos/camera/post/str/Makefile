#
# Copyright (c) 2018 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

str-objs := pablo-str-core.o
str-objs += pablo-str-video.o
str-objs += pablo-str-hw.o
str-objs += pablo-str-cal.o

obj-$(CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_STR)	+= str.o
obj-y							+= sfr/

ccflags-y += -Idrivers/media/platform/exynos/camera/post/str
ccflags-y += -Idrivers/media/platform/exynos/camera/post/str/sfr
ccflags-y += -Idrivers/media/platform/exynos/camera/post/str/param
