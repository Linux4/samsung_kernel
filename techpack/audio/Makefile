# auto-detect subdirs
ifeq ($(CONFIG_ARCH_SDM845), y)
include $(srctree)/techpack/audio/config/sdm845auto.conf
export
ifeq ($(CONFIG_ARCH_SDM660), y)
include $(srctree)/techpack/audio/config/sdm660auto_static.conf
export
endif
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
ifeq ($(filter y,$(CONFIG_SEC_BEYOND0QLTE_PROJECT) $(CONFIG_SEC_BEYOND1QLTE_PROJECT) $(CONFIG_SEC_BEYOND2QLTE_PROJECT) $(CONFIG_SEC_BEYOND2Q5G_PROJECT) $(CONFIG_SEC_BEYONDXQ_PROJECT)),y)
include $(srctree)/techpack/audio/config/sm8150_beyondq.conf
else ifeq ($(CONFIG_SEC_WINNERLTE_PROJECT), y)
include $(srctree)/techpack/audio/config/sm8150_winner.conf
else ifeq ($(filter y,$(CONFIG_SEC_WINNERX_PROJECT) $(CONFIG_SEC_ZODIAC_PROJECT)),y)
include $(srctree)/techpack/audio/config/sm8150_winner.conf
else ifeq ($(CONFIG_SEC_BLOOMQ_PROJECT), y)
include $(srctree)/techpack/audio/config/sm8150_bloom.conf
else ifeq ($(CONFIG_SEC_BOLTQ5G_PROJECT), y)
include $(srctree)/techpack/audio/config/sm8150_boltq5g.conf
else ifeq ($(filter y,$(CONFIG_SEC_D1Q_PROJECT) $(CONFIG_SEC_D2Q_PROJECT) $(CONFIG_SEC_D2XQ_PROJECT) $(CONFIG_SEC_D2XQ2_PROJECT)),y)
include $(srctree)/techpack/audio/config/sm8150_davinciq.conf
else ifeq ($(filter y,$(CONFIG_SEC_GTS6L_PROJECT) $(CONFIG_SEC_GTS6LWIFI_PROJECT) $(CONFIG_SEC_GTS6X_PROJECT)),y)
include $(srctree)/techpack/audio/config/sm8150_gts6l.conf
else ifeq ($(CONFIG_SEC_R3Q_PROJECT), y)
include $(srctree)/techpack/audio/config/sm8150_r3.conf
else ifeq ($(CONFIG_SEC_R5Q_PROJECT), y)
include $(srctree)/techpack/audio/config/sm8150_r5.conf
else ifeq ($(CONFIG_SEC_A82XQ_PROJECT), y)
include $(srctree)/techpack/audio/config/sm8150_a82.conf
else
include $(srctree)/techpack/audio/config/sm8150auto.conf
endif
export
endif
ifeq ($(CONFIG_ARCH_SDMSHRIKE), y)
include $(srctree)/techpack/audio/config/sm8150auto.conf
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
ifeq ($(CONFIG_ARCH_SDM660), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm660autoconf.h
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
ifeq ($(filter y,$(CONFIG_SEC_BEYOND0QLTE_PROJECT) $(CONFIG_SEC_BEYOND1QLTE_PROJECT) $(CONFIG_SEC_BEYOND2QLTE_PROJECT) $(CONFIG_SEC_BEYOND2Q5G_PROJECT) $(CONFIG_SEC_BEYONDXQ_PROJECT)),y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150_beyondq.h
else ifeq ($(CONFIG_SEC_WINNERLTE_PROJECT), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150_winner.h
else ifeq ($(filter y,$(CONFIG_SEC_WINNERX_PROJECT) $(CONFIG_SEC_ZODIAC_PROJECT)),y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150_winner.h
else ifeq ($(CONFIG_SEC_BLOOMQ_PROJECT) ,y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150_bloom.h
else ifeq ($(CONFIG_SEC_BOLTQ5G_PROJECT), y)
LINUXINCLUDE	+= \
                -include $(srctree)/techpack/audio/config/sm8150_boltq5g.h
else ifeq ($(filter y,$(CONFIG_SEC_D1Q_PROJECT) $(CONFIG_SEC_D2Q_PROJECT) $(CONFIG_SEC_D2XQ_PROJECT) $(CONFIG_SEC_D2XQ2_PROJECT)),y)
LINUXINCLUDE	+= \
                -include $(srctree)/techpack/audio/config/sm8150_davinciq.h
else ifeq ($(filter y,$(CONFIG_SEC_GTS6L_PROJECT) $(CONFIG_SEC_GTS6LWIFI_PROJECT) $(CONFIG_SEC_GTS6X_PROJECT)),y)
LINUXINCLUDE	+= \
                -include $(srctree)/techpack/audio/config/sm8150_gts6l.h
else ifeq ($(CONFIG_SEC_R3Q_PROJECT), y)
LINUXINCLUDE	+= \
                -include $(srctree)/techpack/audio/config/sm8150_r3.h
else ifeq ($(CONFIG_SEC_R5Q_PROJECT), y)
LINUXINCLUDE	+= \
                -include $(srctree)/techpack/audio/config/sm8150_r5.h
else ifeq ($(CONFIG_SEC_A82XQ_PROJECT), y)
LINUXINCLUDE	+= \
                -include $(srctree)/techpack/audio/config/sm8150_a82.h
else
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150autoconf.h
endif
endif
ifeq ($(CONFIG_ARCH_SDMSHRIKE), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150autoconf.h
endif

obj-y += soc/
obj-y += dsp/
obj-y += ipc/
obj-y += asoc/
