# Tab A9 code for AX6739A-2638 by lina at 20230808 start
ccflags-y += \
        -DAW_HAVE_SNK \
        -DAW_HAVE_SRC \
        -DAW_HAVE_DRP \
        -DAW_HAVE_ACCMODE \
        -DAW_HAVE_EXT_MSG \
        -DAW_HAVE_VDM \
        -DAW_HAVE_DP \
        -DAW_HAVE_VBUS_ONLY
# Tab A9 code for AX6739A-2638 by lina at 20230808 end

obj-$(CONFIG_TCPC_AW35615) += aw35615_whole.o

aw35615_whole-objs := \
        aw35615_driver.o \
        aw35615_global.o \
        platform_helpers.o \
        dfs.o \
        core.o \
        PDPolicy.o \
        timer.o \
        PDProtocol.o \
        TypeC.o \
        Port.o \
        vendor_info.o \
        modules/dpm.o \
        modules/observer.o \
        vdm/bitfield_translators.o \
        vdm/vdm.o \
        vdm/vdm_callbacks.o \
        vdm/DisplayPort/dp.o \
