log=@echo [$(shell date "+%Y-%m-%d %H:%M:%S")]

KERNEL_CONFIG?=goyavewifi_defconfig

.PHONY:help
help:
	$(hide)echo "======================================="
	$(hide)echo "= This file wraps the build of kernel and modules"
	$(hide)echo "= make kernel: only make the kernel."
	$(hide)echo "= make mali: only make mali."
	$(hide)echo "= make clean: clean the kernel and mali"
	$(hide)echo "======================================="

all: kernel mali

.PHONY: kernel
kernel:
	$(log) "making kernel [$(KERNEL_CONFIG)]..."

	@cd common && \
	make $(KERNEL_CONFIG) && \
	make
	$(log) "kernel [$(KERNEL_CONFIG)] done"

.PHONY: mali
mali:
	$(log) "making mali..."

	@cd mali && \
	./build.sh
	$(log) "mali done"

clean:
	@cd common && \
	make clean
	$(log) "Kernel cleaned."
	
	@cd mali && \
	make clean MALI_PLATFORM=sc8830 BUILD=no KDIR=../common
	$(log) "mali cleaned."
