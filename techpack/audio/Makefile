# auto-detect subdirs
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
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/audio/config/konaauto.conf
export
endif
ifeq ($(CONFIG_ARCH_SDM660), y)
include $(srctree)/techpack/audio/config/sdm660auto.conf
endif
ifeq ($(CONFIG_ARCH_LITO), y)
ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),a42xq a42xuq))
include $(srctree)/techpack/audio/config/lito_a42.conf
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),a52xq))
include $(srctree)/techpack/audio/config/lito_a52.conf
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),gts7xllite gts7xllitewifi))
include $(srctree)/techpack/audio/config/lito_gts7plite.conf
else
include $(srctree)/techpack/audio/config/litoauto.conf
endif
export
endif

# Use USERINCLUDE when you must reference the UAPI directories only.
USERINCLUDE     += \
                -I$(srctree)/techpack/audio/include/uapi \
                -I$(srctree)/techpack/audio/include

# Use LINUXINCLUDE when you must reference the include/ directory.
# Needed to be compatible with the O= option
LINUXINCLUDE    += \
                -I$(srctree)/techpack/audio/include/uapi \
                -I$(srctree)/techpack/audio/include

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
ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/konaautoconf.h
endif
ifeq ($(CONFIG_ARCH_SDM660), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm660autoconf.h
endif
ifeq ($(CONFIG_ARCH_LITO), y)
ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),a42xq a42xuq))
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/lito_a42.h
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),a52xq))
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/lito_a52.h
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),gts7xllite gts7xllitewifi))
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/lito_gts7plite.h
else
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/litoautoconf.h
endif
endif

obj-y += soc/
obj-y += dsp/
obj-y += ipc/
obj-y += asoc/
