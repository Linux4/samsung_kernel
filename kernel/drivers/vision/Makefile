#
# Makefile for the npu device driver
#

ccflags-y += -DNPU_DD_SOURCE_VERSION=$(shell ((find $(srctree)/drivers/vision/npu/ \( -name '*.h' -o -name '*.c' \) -print0 | xargs -0 cat ) | wc -l))

ccflags-y += -I$(srctree)/drivers/vision -Wformat
ccflags-y += -I$(srctree)/drivers/vision/npu -Wformat
ccflags-y += -I$(srctree)/drivers/vision/npu/include -Wformat
ifeq ($(CONFIG_DSP_USE_VS4L),y)
ccflags-y += -I$(srctree)/drivers/vision/npu/dl -Wformat
endif
ccflags-y += -I$(srctree)/drivers/vision/npu/interface/hardware

ifeq ($(CONFIG_NPU_STM),y)
ccflags-y += -I$(srctree)/drivers/hwtracing/coresight
endif

ifeq ($(CONFIG_NPU_USE_MBR),y)
ccflags-y += -I$(srctree)/drivers/vision/dsp -Wformat
endif

ifeq ($(CONFIG_NPU_BUILD_BUILTIN),y)
obj-y					+= npu/npu-util-autosleepthr.o
obj-y					+= npu/npu-util-liststatemgr.o
obj-y					+= npu/npu-util-msgidgen.o
obj-y					+= npu/npu-util-regs.o
obj-y					+= npu/npu-util-llq.o
obj-y					+= npu/npu-util-common.o
obj-y					+= npu/npu-llc.o
obj-y					+= npu/npu-log.o
obj-y					+= npu/npu-ver.o
obj-y					+= npu/npu-debug.o
obj-y					+= npu/npu-device.o
ifneq ($(CONFIG_NPU_KUNIT_TEST),y)
obj-y					+= npu/npu-core.o
endif
obj-y					+= npu/npu-clock.o
obj-y					+= npu/npu-vertex.o
obj-y					+= npu/npu-binary.o
obj-y					+= npu/npu-queue.o
obj-y					+= npu/npu-qos.o
obj-y					+= npu/npu-system.o
obj-y					+= npu/npu-syscall.o
obj-y					+= npu/npu-memory.o
obj-y					+= npu/npu-protodrv.o
obj-y					+= npu/npu-session.o
ifeq ($(CONFIG_DEBUG_FS),y)
obj-y					+= npu/npu-fw-test-handler.o
endif
obj-y					+= npu/npu-queue.o
obj-y					+= npu/npu-if-session-protodrv.o
obj-y					+= npu/npu-if-protodrv-mbox2.o
obj-y					+= npu/npu-sessionmgr.o
obj-y					+= npu/npu-config.o
obj-y					+= npu/npu-profile-v2.o
obj-y					+= npu/npu-hw-device.o
obj-y					+= npu/npu-bts.o
obj-y					+= npu/npu-dvfs.o
obj-y					+= npu/npu-dtm.o
obj-y					+= npu/npu-precision.o
ifeq ($(CONFIG_EXYNOS_UNPU_DRIVER),y)
obj-y					+= npu/unpu.o
endif
ifeq ($(CONFIG_NPU_USE_FENCE_SYNC),y)
obj-y					+= npu/npu-fence.o
obj-y					+= npu/npu-fence-sync.o
endif

obj-y	        += npu/npu-util-statekeeper.o

obj-$(CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS)	        += npu/npu-util-memdump.o

obj-$(CONFIG_NPU_SCHEDULER_GOVERNOR)	+= npu/npu-scheduler.o\
					npu/ npu-scheduler-governor.o

obj-$(CONFIG_NPU_GOVERNOR_SIMPLE_EXYNOS)	+= npu/npu-scheduler-governor-simple-exynos.o

obj-$(CONFIG_NPU_GOVERNOR_EXYNOS_INTERACTIVE)	+= npu/npu-scheduler-governor-exynos-interactive.o

obj-$(CONFIG_NPU_GOVERNOR_USERSPACE)	+= npu/npu-scheduler-governor-userspace.o

obj-$(CONFIG_EXYNOS_NPU_PUBLISH_NPU_BUILD_VER)	+= npu/generated/npu-ver-info.o

ifeq ($(CONFIG_NPU_INTERFACE),y)
obj-y					+= npu/interface/npu-interface.o
obj-y					+= npu/interface/mailbox_ipc.o
endif

obj-$(CONFIG_NPU_STM)			+= npu/npu-stm.o

obj-$(CONFIG_NPU_PM_SLEEP_WAKEUP)	+= npu/npu-pm.o

obj-y					+= npu/dsp-dhcp.o
ifeq ($(CONFIG_DSP_USE_VS4L),y)
obj-y					+= npu/dsp-binary.o
obj-y					+= npu/dsp-kernel.o
obj-y					+= npu/dsp-util.o
obj-y					+= npu/dl/dsp-common.o
obj-y					+= npu/dl/dsp-dl-engine.o
obj-y					+= npu/dl/dsp-dl-linker.o
obj-y					+= npu/dl/dsp-dl-out-manager.o
obj-y					+= npu/dl/dsp-elf-loader.o
obj-y					+= npu/dl/dsp-gpt-manager.o
obj-y					+= npu/dl/dsp-hash.o
obj-y					+= npu/dl/dsp-lib-manager.o
obj-y					+= npu/dl/dsp-list.o
obj-y					+= npu/dl/dsp-llstack.o
obj-y					+= npu/dl/dsp-pm-manager.o
obj-y					+= npu/dl/dsp-rule-reader.o
obj-y					+= npu/dl/dsp-string-tree.o
obj-y					+= npu/dl/dsp-sxml.o
obj-y					+= npu/dl/dsp-tlsf-allocator.o
obj-y					+= npu/dl/dsp-xml-parser.o
endif
else
npu-objs				+= npu/npu-util-autosleepthr.o
npu-objs				+= npu/npu-util-liststatemgr.o
npu-objs				+= npu/npu-util-msgidgen.o
npu-objs				+= npu/npu-util-regs.o
npu-objs				+= npu/npu-util-llq.o
ifneq ($(CONFIG_NPU_KUNIT_TEST),y)
npu-objs				+= npu/npu-core.o
endif
npu-objs				+= npu/npu-clock.o
npu-objs				+= npu/npu-util-common.o
npu-objs				+= npu/npu-llc.o
npu-objs				+= npu/npu-log.o
npu-objs				+= npu/npu-ver.o
npu-objs				+= npu/npu-debug.o
npu-objs				+= npu/npu-device.o
npu-objs				+= npu/npu-vertex.o
npu-objs				+= npu/npu-binary.o
npu-objs				+= npu/npu-queue.o
npu-objs				+= npu/npu-qos.o
npu-objs				+= npu/npu-system.o
npu-objs				+= npu/npu-syscall.o
npu-objs				+= npu/npu-memory.o
npu-objs				+= npu/npu-protodrv.o
npu-objs				+= npu/npu-session.o
ifeq ($(CONFIG_DEBUG_FS),y)
npu-objs				+= npu/npu-fw-test-handler.o
endif
npu-objs				+= npu/npu-queue.o
npu-objs				+= npu/npu-if-session-protodrv.o
npu-objs				+= npu/npu-if-protodrv-mbox2.o
npu-objs				+= npu/npu-sessionmgr.o
npu-objs				+= npu/npu-config.o
npu-objs				+= npu/npu-profile-v2.o
npu-objs				+= npu/npu-hw-device.o
npu-objs				+= npu/npu-bts.o
npu-objs				+= npu/npu-dvfs.o
npu-objs				+= npu/npu-dtm.o
npu-objs				+= npu/npu-precision.o
ifeq ($(CONFIG_EXYNOS_UNPU_DRIVER),y)
npu-objs				+= npu/unpu.o
endif
ifeq ($(CONFIG_NPU_USE_FENCE_SYNC),y)
npu-objs				+= npu/npu-fence.o
npu-objs				+= npu/npu-fence-sync.o
endif

ifeq ($(CONFIG_NPU_INTERFACE),y)
npu-objs				+= npu/interface/hardware/npu-interface.o
npu-objs				+= npu/interface/hardware/mailbox_ipc.o
endif

npu-objs				+= npu/npu-util-statekeeper.o

ifeq ($(CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS),y)
npu-objs				+= npu/npu-util-memdump.o
endif

ifeq ($(CONFIG_NPU_GOVERNOR),y)
npu-objs				+= npu/npu-governor.o
endif

ifeq ($(CONFIG_NPU_SCHEDULER_GOVERNOR),y)
npu-objs				+= npu/npu-scheduler.o
npu-objs				+= npu/npu-scheduler-governor.o
ifeq ($(CONFIG_NPU_GOVERNOR_SIMPLE_EXYNOS),y)
npu-objs				+= npu/npu-scheduler-governor-simple-exynos.o
endif
ifeq ($(CONFIG_NPU_GOVERNOR_EXYNOS_INTERACTIVE),y)
npu-objs				+= npu/npu-scheduler-governor-exynos-interactive.o
endif
ifeq ($(CONFIG_NPU_GOVERNOR_USERSPACE),y)
npu-objs				+= npu/npu-scheduler-governor-userspace.o
endif
endif

ifeq ($(CONFIG_EXYNOS_NPU_PUBLISH_NPU_BUILD_VER),y)
npu-objs				+= npu/generated/npu-ver-info.o
endif

ifeq ($(CONFIG_NPU_STM),y)
npu-objs				+= npu/npu-stm.o
endif

ifeq ($(CONFIG_NPU_AFM),y)
npu-objs				+= npu/npu-afm.o
npu-objs				+= npu/npu-afm-debug.o
endif

ifeq ($(CONFIG_NPU_PM_SLEEP_WAKEUP),y)
npu-objs				+= npu/npu-pm.o
endif

npu-objs				+= npu/dsp-dhcp.o
ifeq ($(CONFIG_DSP_USE_VS4L),y)
npu-objs				+= npu/dsp-binary.o
npu-objs				+= npu/dsp-kernel.o
npu-objs				+= npu/dsp-util.o
npu-objs				+= npu/dl/dsp-common.o
npu-objs				+= npu/dl/dsp-dl-engine.o
npu-objs				+= npu/dl/dsp-dl-linker.o
npu-objs				+= npu/dl/dsp-dl-out-manager.o
npu-objs				+= npu/dl/dsp-elf-loader.o
npu-objs				+= npu/dl/dsp-gpt-manager.o
npu-objs				+= npu/dl/dsp-hash.o
npu-objs				+= npu/dl/dsp-lib-manager.o
npu-objs				+= npu/dl/dsp-list.o
npu-objs				+= npu/dl/dsp-llstack.o
npu-objs				+= npu/dl/dsp-pm-manager.o
npu-objs				+= npu/dl/dsp-rule-reader.o
npu-objs				+= npu/dl/dsp-string-tree.o
npu-objs				+= npu/dl/dsp-sxml.o
npu-objs				+= npu/dl/dsp-tlsf-allocator.o
npu-objs				+= npu/dl/dsp-xml-parser.o
endif

obj-$(CONFIG_EXYNOS_NPU)		+= npu.o
endif
