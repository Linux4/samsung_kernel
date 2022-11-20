# auto-detect subdirs
ifeq ($(CONFIG_ARCH_SDM845), y)
include $(srctree)/techpack/audio/config/sdm845auto.conf
export
endif
ifeq ($(CONFIG_ARCH_SDM670), y)
include $(srctree)/techpack/audio/config/sdm670auto_static.conf
export
endif
ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
include $(srctree)/techpack/audio/config/sdxpoorwillsauto.conf
export
endif
ifeq ($(CONFIG_ARCH_SM8150), y)
include $(srctree)/techpack/audio/config/sm8150auto.conf
export
endif
ifeq ($(CONFIG_ARCH_SDMSHRIKE), y)
include $(srctree)/techpack/audio/config/sm8150auto.conf
export
endif
ifeq ($(CONFIG_ARCH_SM6150), y)
ifeq ($(filter y,$(CONFIG_SEC_A70Q_PROJECT)),y)
include $(srctree)/techpack/audio/config/sm6150_a70q.conf
else ifeq ($(filter y,$(CONFIG_SEC_A70S_PROJECT)),y)
include $(srctree)/techpack/audio/config/sm6150_a70s.conf
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),a90q r1q))
include $(srctree)/techpack/audio/config/sm7150_a90q.conf
else ifeq ($(filter y,$(CONFIG_SEC_A70SQ_PROJECT)),y)
include $(srctree)/techpack/audio/config/sm7150_a70sq.conf
else ifeq ($(filter y,$(CONFIG_SEC_A71_PROJECT)),y)
include $(srctree)/techpack/audio/config/sm7150_a71.conf
else ifeq ($(filter y,$(CONFIG_SEC_A60Q_PROJECT)),y)
include $(srctree)/techpack/audio/config/sm6150_a60q.conf
else ifeq ($(filter y,$(CONFIG_SEC_M40_PROJECT)),y)
include $(srctree)/techpack/audio/config/sm6150_m40.conf
else
include $(srctree)/techpack/audio/config/sm6150auto.conf
endif
export
endif

# Use USERINCLUDE when you must reference the UAPI directories only.
USERINCLUDE     += \
                -I$(srctree)/techpack/audio/include/uapi \

# Use LINUXINCLUDE when you must reference the include/ directory.
# Needed to be compatible with the O= option
LINUXINCLUDE    += \
                -I$(srctree)/techpack/audio/include/uapi \
                -I$(srctree)/techpack/audio/include

ifeq ($(CONFIG_ARCH_SDM845), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm845autoconf.h
endif
ifeq ($(CONFIG_ARCH_SDM670), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm670autoconf.h
endif
ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdxpoorwillsautoconf.h
endif
ifeq ($(CONFIG_ARCH_SM8150), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150autoconf.h
endif
ifeq ($(CONFIG_ARCH_SDMSHRIKE), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150autoconf.h
endif
ifeq ($(CONFIG_ARCH_SM6150), y)
ifeq ($(filter y,$(CONFIG_SEC_A70Q_PROJECT)),y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm6150_a70q.h
else ifeq ($(filter y,$(CONFIG_SEC_A70S_PROJECT)),y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm6150_a70s.h					
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),a90q r1q))
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm7150_a90q.h
else ifeq ($(filter y,$(CONFIG_SEC_A70SQ_PROJECT)),y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm7150_a70sq.h
else ifeq ($(filter y,$(CONFIG_SEC_A71_PROJECT)),y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm7150_a71.h
else ifeq ($(filter y,$(CONFIG_SEC_A60Q_PROJECT)),y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm6150_a60q.h
else ifeq ($(filter y,$(CONFIG_SEC_M40_PROJECT)),y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm6150_m40.h
else
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm6150autoconf.h
endif
endif

obj-y += soc/
obj-y += dsp/
obj-y += ipc/
obj-y += asoc/
