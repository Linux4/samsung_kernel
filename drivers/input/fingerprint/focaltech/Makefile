#include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

# SVN revision and build date for driver version.
#ccflags-y += -D__FF_SVN_REV=\"$(strip $(shell svn info --show-item last-changed-revision $(PWD) 2>/dev/null))\"
ccflags-y += -D__FF_SVN_REV=\"7111\"
ccflags-y += -D__FF_BUILD_DATE=\"$(shell date +%Y%m%d)\"

ccflags-$(CONFIG_FINGERPRINT_ID_HI_FOCALTECH) += -DUSE_FP_ID_DUAL -DFP_ID_VALUE=1
ccflags-$(CONFIG_FINGERPRINT_ID_LO_FOCALTECH) += -DUSE_FP_ID_DUAL -DFP_ID_VALUE=0

#ccflags-y += -DUSE_ONE_BINARY

# Early-logging level for ff_hal, ff_ta and driver as well. Note that this
# logging level only takes effect before ff_config module is loaded.
#
# Options:
# 0 - FF_LOG_LEVEL_ALL, All
# 1 - FF_LOG_LEVEL_VBS, Verbose
# 2 - FF_LOG_LEVEL_DBG, Debug
# 3 - FF_LOG_LEVEL_INF, Info
# 4 - FF_LOG_LEVEL_WRN, Warning
# 5 - FF_LOG_LEVEL_ERR, Error
ccflags-y += -D__FF_EARLY_LOG_LEVEL=1

# Source files.
obj-y := focaltech_fp.o
focaltech_fp-objs := ff_ctl.o
