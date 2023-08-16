cmd_arch/arm/mm/tlb-v7.o := arm-eabi-gcc -Wp,-MD,arch/arm/mm/.tlb-v7.o.d  -nostdinc -isystem /home/amardeep.s/OSLV/Google/ARM_4.8/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/../lib/gcc/arm-eabi/4.8/include -I/home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include -Iarch/arm/include/generated  -Iinclude -I/home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/uapi -Iarch/arm/include/generated/uapi -I/home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/include/uapi -Iinclude/generated/uapi -include /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-exynos/include -Iarch/arm/plat-samsung/include  -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -marm -D__LINUX_ARM_ARCH__=7 -march=armv7-a  -include asm/unified.h -msoft-float -gdwarf-2      -Wa,-march=armv7-a   -c -o arch/arm/mm/tlb-v7.o arch/arm/mm/tlb-v7.S

source_arch/arm/mm/tlb-v7.o := arch/arm/mm/tlb-v7.S

deps_arch/arm/mm/tlb-v7.o := \
    $(wildcard include/config/arm/errata/720789.h) \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \
  include/linux/init.h \
    $(wildcard include/config/broken/rodata.h) \
    $(wildcard include/config/deferred/initcalls.h) \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/hotplug.h) \
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
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/include/uapi/asm-generic/types.h \
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
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/linkage.h \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/assembler.h \
    $(wildcard include/config/cpu/feroceon.h) \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/cpu/use/domains.h) \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/ptrace.h \
    $(wildcard include/config/arm/thumb.h) \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/uapi/asm/ptrace.h \
    $(wildcard include/config/cpu/endian/be8.h) \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/hwcap.h \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/uapi/asm/hwcap.h \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/domain.h \
    $(wildcard include/config/io/36.h) \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/opcodes-virt.h \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/opcodes.h \
    $(wildcard include/config/cpu/endian/be32.h) \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/asm-offsets.h \
  include/generated/asm-offsets.h \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/page.h \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/cpu/copy/v4wt.h) \
    $(wildcard include/config/cpu/copy/v4wb.h) \
    $(wildcard include/config/cpu/copy/feroceon.h) \
    $(wildcard include/config/cpu/copy/fa.h) \
    $(wildcard include/config/cpu/sa1100.h) \
    $(wildcard include/config/cpu/xscale.h) \
    $(wildcard include/config/cpu/xsc3.h) \
    $(wildcard include/config/cpu/copy/v6.h) \
    $(wildcard include/config/kuser/helpers.h) \
    $(wildcard include/config/arm/lpae.h) \
    $(wildcard include/config/have/arch/pfn/valid.h) \
  include/asm-generic/getorder.h \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/tlbflush.h \
    $(wildcard include/config/smp/on/up.h) \
    $(wildcard include/config/cpu/tlb/v4wt.h) \
    $(wildcard include/config/cpu/tlb/fa.h) \
    $(wildcard include/config/cpu/tlb/v4wbi.h) \
    $(wildcard include/config/cpu/tlb/feroceon.h) \
    $(wildcard include/config/cpu/tlb/v4wb.h) \
    $(wildcard include/config/cpu/tlb/v6.h) \
    $(wildcard include/config/cpu/tlb/v7.h) \
    $(wildcard include/config/tima/rkp/lazy/mmu.h) \
    $(wildcard include/config/arm/errata/798181.h) \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/glue.h \
  arch/arm/mm/proc-macros.S \
    $(wildcard include/config/cpu/dcache/writethrough.h) \
    $(wildcard include/config/pm/sleep.h) \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/thread_info.h \
    $(wildcard include/config/crunch.h) \
    $(wildcard include/config/arm/thumbee.h) \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/fpstate.h \
    $(wildcard include/config/vfpv3.h) \
    $(wildcard include/config/iwmmxt.h) \

arch/arm/mm/tlb-v7.o: $(deps_arch/arm/mm/tlb-v7.o)

$(deps_arch/arm/mm/tlb-v7.o):
