subdir-ccflags-y += -Idrivers/staging/android/ion/
subdir-ccflags-y += -I$(src)/core/include
subdir-ccflags-y += -I$(src)/bridge/
subdir-ccflags-y += -I$(src)/drv/
subdir-ccflags-$(CONFIG_FB_SCX30G) += -I$(src)/drv/shark/

#NOTE: drv/sprd_interface.c must be compiled first
obj-y += drv/
obj-y += core/
obj-y += bridge/
