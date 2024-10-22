$(info [bt_drv] linux_v2/makefile start)
# Support GKI mixed build
ifeq ($(DEVICE_MODULES_PATH),)
DEVICE_MODULES_PATH = $(DEVICE_MODULES_PATH)
else
LINUXINCLUDE := $(DEVCIE_MODULES_INCLUDE) $(LINUXINCLUDE)
endif

LOG_TAG := [BT_Drv][linux_v2]
ifneq ($(wildcard $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include),)
include $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include
extra_symbols := $(abspath $(OUT_DIR)/../vendor/mediatek/kernel_modules/connectivity/common/Module.symvers)
extra_symbols += $(abspath $(OUT_DIR)/../vendor/mediatek/kernel_modules/connectivity/connfem/Module.symvers)
extra_symbols += $(abspath $(OUT_DIR)/../vendor/mediatek/kernel_modules/connectivity/conninfra/Module.symvers)
else
extra_symbols := $(abspath $(O)/../vendor/mediatek/kernel_modules/connectivity/common/Module.symvers)
extra_symbols += $(abspath $(O)/../vendor/mediatek/kernel_modules/connectivity/connfem/Module.symvers)
extra_symbols += $(abspath $(O)/../vendor/mediatek/kernel_modules/connectivity/conninfra/Module.symvers)
endif

all: PRIVATE_LOG_TAG := $(LOG_TAG)
all: EXTRA_SYMBOLS += $(extra_symbols)
all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS) LOG_TAG=$(PRIVATE_LOG_TAG) KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)"

modules_install:
	$(MAKE) M=$(M) -C $(KERNEL_SRC) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

# Check coding style
# export IGNORE_CODING_STYLE_RULES := NEW_TYPEDEFS,LEADING_SPACE,CODE_INDENT,SUSPECT_CODE_INDENT
ccs:
	./util/checkpatch.pl -f ./sdio/btmtksdio.c
	./util/checkpatch.pl -f ./include/sdio/btmtk_sdio.h
	./util/checkpatch.pl -f ./include/btmtk_define.h
	./util/checkpatch.pl -f ./include/btmtk_drv.h
	./util/checkpatch.pl -f ./include/btmtk_chip_if.h
	./util/checkpatch.pl -f ./include/btmtk_main.h
	./util/checkpatch.pl -f ./include/btmtk_buffer_mode.h
	./util/checkpatch.pl -f ./include/uart/tty/btmtk_uart_tty.h
	./util/checkpatch.pl -f ./uart/btmtktty.c
	./util/checkpatch.pl -f ./include/btmtk_fw_log.h
	./util/checkpatch.pl -f ./include/btmtk_woble.h
	./util/checkpatch.pl -f ./include/uart/btmtk_uart.h
	./util/checkpatch.pl -f ./uart/btmtk_uart_main.c
	./util/checkpatch.pl -f ./include/usb/btmtk_usb.h
	./util/checkpatch.pl -f ./usb/btmtkusb.c
	./util/checkpatch.pl -f ./proj/btmtk_proj_ce.c
	./util/checkpatch.pl -f ./proj/btmtk_proj_sp.c
	./util/checkpatch.pl -f btmtk_fw_log.c
	./util/checkpatch.pl -f btmtk_main.c
	./util/checkpatch.pl -f btmtk_buffer_mode.c
	./util/checkpatch.pl -f btmtk_woble.c
	./util/checkpatch.pl -f btmtk_chip_reset.c
	./util/checkpatch.pl -f btmtk_queue.c
	./util/checkpatch.pl -f btmtk_char_dev.c
$(info [bt_drv] linux_v2/makefile end)
