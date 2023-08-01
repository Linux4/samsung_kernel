cmd_arch/arm/kernel/entry-common.o := /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/scripts/gcc-wrapper.py /opt/toolchains/arm-eabi-4.7/bin/arm-eabi-gcc -Wp,-MD,arch/arm/kernel/.entry-common.o.d  -nostdinc -isystem /opt/toolchains/arm-eabi-4.7/bin/../lib/gcc/arm-eabi/4.7/include -I/home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include -Iarch/arm/include/generated  -Iinclude -I/home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/uapi -Iarch/arm/include/generated/uapi -I/home/longjian.cui/SM-E7000_CHN_CHC_Kernel/include/uapi -Iinclude/generated/uapi -include /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-msm/include  -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables -marm -D__LINUX_ARM_ARCH__=7 -march=armv7-a  -include asm/unified.h -msoft-float -gdwarf-2         -c -o arch/arm/kernel/entry-common.o arch/arm/kernel/entry-common.S

source_arch/arm/kernel/entry-common.o := arch/arm/kernel/entry-common.S

deps_arch/arm/kernel/entry-common.o := \
    $(wildcard include/config/need/ret/to/user.h) \
    $(wildcard include/config/function/tracer.h) \
    $(wildcard include/config/old/mcount.h) \
    $(wildcard include/config/frame/pointer.h) \
    $(wildcard include/config/function/graph/tracer.h) \
    $(wildcard include/config/dynamic/ftrace.h) \
    $(wildcard include/config/oabi/compat.h) \
    $(wildcard include/config/arm/thumb.h) \
    $(wildcard include/config/cpu/endian/be8.h) \
    $(wildcard include/config/aeabi.h) \
    $(wildcard include/config/alignment/trap.h) \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/unistd.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/uapi/asm/unistd.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/ftrace.h \
    $(wildcard include/config/arm/unwind.h) \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/unwind.h \
  arch/arm/kernel/entry-header.S \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/cpu/v6.h) \
    $(wildcard include/config/cpu/32v6k.h) \
    $(wildcard include/config/context/tracking.h) \
  include/linux/init.h \
    $(wildcard include/config/broken/rodata.h) \
    $(wildcard include/config/modules.h) \
  include/linux/compiler.h \
    $(wildcard include/config/sparse/rcu/pointer.h) \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
    $(wildcard include/config/kprobes.h) \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/lbdaf.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  include/uapi/linux/types.h \
  arch/arm/include/generated/asm/types.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/include/uapi/asm-generic/types.h \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  arch/arm/include/generated/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/uapi/asm-generic/bitsperlong.h \
  include/linux/linkage.h \
  include/linux/stringify.h \
  include/linux/export.h \
    $(wildcard include/config/have/underscore/symbol/prefix.h) \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/unused/symbols.h) \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/linkage.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/assembler.h \
    $(wildcard include/config/cpu/feroceon.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/cpu/use/domains.h) \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/ptrace.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/uapi/asm/ptrace.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/hwcap.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/uapi/asm/hwcap.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/domain.h \
    $(wildcard include/config/verify/permission/fault.h) \
    $(wildcard include/config/io/36.h) \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/opcodes-virt.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/opcodes.h \
    $(wildcard include/config/cpu/endian/be32.h) \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/asm-offsets.h \
  include/generated/asm-offsets.h \
  arch/arm/include/generated/asm/errno.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/include/uapi/asm-generic/errno.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/include/uapi/asm-generic/errno-base.h \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/thread_info.h \
    $(wildcard include/config/crunch.h) \
    $(wildcard include/config/arm/thumbee.h) \
  /home/longjian.cui/SM-E7000_CHN_CHC_Kernel/arch/arm/include/asm/fpstate.h \
    $(wildcard include/config/vfpv3.h) \
    $(wildcard include/config/iwmmxt.h) \
  arch/arm/kernel/calls.S \

arch/arm/kernel/entry-common.o: $(deps_arch/arm/kernel/entry-common.o)

$(deps_arch/arm/kernel/entry-common.o):
