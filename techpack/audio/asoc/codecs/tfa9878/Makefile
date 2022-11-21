# ifeq ($(CONFIG_SND_SOC_TFA9878), y)
# TFA audio amplifier - TFA9878

TFA_KERNEL_MODULE        =1

TFA_DEBUG                =1
TFADSP_DEBUG             =1
TFA_VERSION              =tfa9878
TFADSP_32BITS            =1
TFA_USE_DIRECT_API_CALL  =1
TFADSP_DSP_MSG_PACKET_STRATEGY=1
TFADSP_DSP_BUFFER_POOL   =1
TFADSP_CONFIGURE_AT_FIRST_DEVICE=1
#TFA_CUSTOM_FORMAT_AT_RESPONSE=1
#TFA_PROFILE_ON_DEVICE    =1
TFA_MIXER_ON_DEVICE      =1
TFA_NO_SND_FORMAT_CHECK  =1
TFA_FIXED_RATE_FOR_DSP   =1
TFA_VOID_APIV_IN_FILE    =1
#TFA_VOID_LIBV_CHECK      =1
TFA_SET_EXT_INTERNALLY   =1
#TFA_USE_WAITQUEUE_SEQ    =1
#TFA_MUTE_DURING_SWITCHING_PROFILE=1
#TFA_WAIT_CAL_IN_WORKQUEUE=1
TFA_RAMPDOWN_BEFORE_MUTE =1
TFA_RECONFIG_WITHOUT_RESET=1
TFA_USE_MANSTATE_TO_CHECK_COLD=1
#TFA_PRELOAD_SETTING_AT_PROBING=1
#TFA_CHANGE_PCM_FORMAT    =1
TFA_MUTE_CONTROL         =1
TFA_PAUSE_CONTROL        =1
#TFA_DISABLE_INT_VDDS     =1
#TFA_STOP_AT_START_FAILURE=1
TFA_BYPASS_AT_START_FAILURE=1
TFA_REDUCE_BYPASS_GAIN   =1
TFA_SET_IPC_PORT_ID      =1
LIMIT_CAL_FROM_DTS       =1
TFA_BLACKBOX_LOGGING     =1
TFA_USE_TFA_CLASS        =1
TFA_USE_TFACAL_NODE      =1
#TFA_USE_TFALOG_NODE      =1
TFA_USE_TFAVVAL_NODE     =1
#TFA_STEREO_NODE          =1
TFA_READ_REFERENCE_TEMP  =1
TFA_USE_POWER_SUPPLY     =1
#TFA_USE_THERMAL_SENSOR   =1

TFA98XX_FLAGS            := -DUSE_TFA9878 -DQPLATFORM

# cc flags
#ccflags-y              := -DDEBUG
ccflags-y               += -Werror
ifdef TFA_DEBUG
ccflags-y               += -DTFA_DEBUG
endif
ifdef TFADSP_DEBUG
ccflags-y               += -DTFADSP_DEBUG
endif
ifdef TFADSP_32BITS
ccflags-y               += -DTFADSP_32BITS
endif
ifdef TFA_USE_DIRECT_API_CALL
ccflags-y               += -DTFA_USE_DIRECT_API_CALL
endif
ifdef TFADSP_DSP_MSG_PACKET_STRATEGY
ccflags-y               += -DTFADSP_DSP_MSG_PACKET_STRATEGY
endif
ifdef TFADSP_DSP_BUFFER_POOL
ccflags-y               += -DTFADSP_DSP_BUFFER_POOL
endif
ifdef TFADSP_CONFIGURE_AT_FIRST_DEVICE
ccflags-y               += -DTFADSP_CONFIGURE_AT_FIRST_DEVICE
endif
ifdef TFA_CUSTOM_FORMAT_AT_RESPONSE
ccflags-y               += -DTFA_CUSTOM_FORMAT_AT_RESPONSE
endif
ifdef TFA_PROFILE_ON_DEVICE
ccflags-y               += -DTFA_PROFILE_ON_DEVICE
endif
ifdef TFA_MIXER_ON_DEVICE
ccflags-y               += -DTFA_MIXER_ON_DEVICE
endif
ifdef TFA_NO_SND_FORMAT_CHECK
ccflags-y               += -DTFA_NO_SND_FORMAT_CHECK
endif
ifdef TFA_FIXED_RATE_FOR_DSP
ccflags-y               += -DTFA_FIXED_RATE_FOR_DSP
endif
ifdef TFA_VOID_APIV_IN_FILE
ccflags-y               += -DTFA_VOID_APIV_IN_FILE
endif
ifdef TFA_VOID_LIBV_CHECK
ccflags-y               += -DTFA_VOID_LIBV_CHECK
endif
ifdef TFA_SET_EXT_INTERNALLY
ccflags-y               += -DTFA_SET_EXT_INTERNALLY
endif
ifdef TFA_USE_WAITQUEUE_SEQ
ccflags-y               += -DTFA_USE_WAITQUEUE_SEQ
endif
ifdef TFA_MUTE_DURING_SWITCHING_PROFILE
ccflags-y               += -DTFA_MUTE_DURING_SWITCHING_PROFILE
endif
ifdef TFA_WAIT_CAL_IN_WORKQUEUE
ccflags-y               += -DTFA_WAIT_CAL_IN_WORKQUEUE
endif
ifdef TFA_RAMPDOWN_BEFORE_MUTE
ccflags-y               += -DTFA_RAMPDOWN_BEFORE_MUTE
endif
ifdef TFA_RECONFIG_WITHOUT_RESET
ccflags-y               += -DTFA_RECONFIG_WITHOUT_RESET
endif
ifdef TFA_USE_MANSTATE_TO_CHECK_COLD
ccflags-y               += -DTFA_USE_MANSTATE_TO_CHECK_COLD
endif
ifdef TFA_PRELOAD_SETTING_AT_PROBING
ccflags-y               += -DTFA_PRELOAD_SETTING_AT_PROBING
endif
ifdef TFA_CHANGE_PCM_FORMAT
ccflags-y               += -DTFA_CHANGE_PCM_FORMAT
endif
ifdef TFA_MUTE_CONTROL
ccflags-y               += -DTFA_MUTE_CONTROL
endif
ifdef TFA_PAUSE_CONTROL
ccflags-y               += -DTFA_PAUSE_CONTROL
endif
ifdef TFA_DISABLE_INT_VDDS
ccflags-y               += -DTFA_DISABLE_INT_VDDS
endif
ifdef TFA_STOP_AT_START_FAILURE
ccflags-y               += -DTFA_STOP_AT_START_FAILURE
endif
ifdef TFA_BYPASS_AT_START_FAILURE
ccflags-y               += -DTFA_BYPASS_AT_START_FAILURE
endif
ifdef TFA_REDUCE_BYPASS_GAIN
ccflags-y               += -DTFA_REDUCE_BYPASS_GAIN
endif
ifdef TFA_SET_IPC_PORT_ID
ccflags-y               += -DTFA_SET_IPC_PORT_ID
endif
ifdef LIMIT_CAL_FROM_DTS
ccflags-y               += -DLIMIT_CAL_FROM_DTS
endif
ifdef TFA_BLACKBOX_LOGGING
ccflags-y               += -DTFA_BLACKBOX_LOGGING
endif
ifdef TFA_USE_TFACAL_NODE
ccflags-y               += -DTFA_USE_TFACAL_NODE
endif
ifdef TFA_USE_TFALOG_NODE
ccflags-y               += -DTFA_USE_TFALOG_NODE
endif
ifdef TFA_USE_TFAVVAL_NODE
ccflags-y               += -DTFA_USE_TFAVVAL_NODE
endif
ifdef TFA_STEREO_NODE
ccflags-y               += -DTFA_STEREO_NODE
endif
ifdef TFA_READ_REFERENCE_TEMP
ccflags-y               += -DTFA_READ_REFERENCE_TEMP
endif
ifdef TFA_USE_POWER_SUPPLY
ccflags-y               += -DTFA_USE_POWER_SUPPLY
endif
ifdef TFA_USE_THERMAL_SENSOR
ccflags-y               += -DTFA_USE_THERMAL_SENSOR
endif

# objs
snd-soc-tfa98xx-objs    := tfa98xx.o
snd-soc-tfa98xx-objs    += tfa_container.o
snd-soc-tfa98xx-objs    += tfa_dsp.o
snd-soc-tfa98xx-objs    += tfa_init.o
snd-soc-tfa98xx-objs    += bigdata_tfa_sysfs_cb.o
ifdef TFA_DEBUG
snd-soc-tfa98xx-objs    += tfa_debug.o
endif
ifdef TFA_USE_TFA_CLASS
ifdef TFA_KERNEL_MODULE
snd-soc-tfa_sysfs-objs  += tfa_sysfs.o
else
snd-soc-tfa98xx-objs    += tfa_sysfs.o
endif
ifdef TFA_USE_TFACAL_NODE
ifdef TFA_KERNEL_MODULE
snd-soc-tfa_sysfs-objs  += tfa_cal.o
else
snd-soc-tfa98xx-objs    += tfa_cal.o
endif
endif
ifdef TFA_USE_TFALOG_NODE
ifdef TFA_KERNEL_MODULE
snd-soc-tfa_sysfs-objs  += tfa_log.o
else
snd-soc-tfa98xx-objs    += tfa_log.o
endif
endif
ifdef TFA_USE_TFAVVAL_NODE
ifdef TFA_KERNEL_MODULE
snd-soc-tfa_sysfs-objs  += tfa_vval.o
else
snd-soc-tfa98xx-objs    += tfa_vval.o
endif
endif
endif

# CFLAGS
CFLAGS_tfa98xx.o       += $(TFA98XX_FLAGS)
CFLAGS_tfa_container.o += $(TFA98XX_FLAGS)
CFLAGS_tfa_dsp.o       += $(TFA98XX_FLAGS)
CFLAGS_tfa_init.o      += $(TFA98XX_FLAGS)
ifdef TFA_DEBUG
CFLAGS_tfa_debug.o     += $(TFA98XX_FLAGS)
endif
ifdef TFA_USE_TFA_CLASS
CFLAGS_tfa_sysfs.o     += $(TFA98XX_FLAGS)
ifdef TFA_USE_TFACAL_NODE
CFLAGS_tfa_cal.o       += $(TFA98XX_FLAGS)
endif
ifdef TFA_USE_TFALOG_NODE
CFLAGS_tfa_log.o       += $(TFA98XX_FLAGS)
endif
ifdef TFA_USE_TFAVVAL_NODE
CFLAGS_tfa_vval.o      += $(TFA98XX_FLAGS)
endif
endif

ifdef TFA_KERNEL_MODULE
# module driver
obj-m   := snd-soc-tfa98xx.o
ifdef TFA_USE_TFA_CLASS
obj-m   += snd-soc-tfa_sysfs.o
endif
else
# built-in driver
obj-y	+= snd-soc-tfa98xx.o
endif
# endif

