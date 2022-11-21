cmd_drivers/misc/mediatek/base/power/leakage_table_v2/mtk_static_power_mt6739.o := /home/oslv_team/PROD_GANGA10/GALAXY_A01_CORE/FLUMEN/Mojito/MT6739/android//prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androidkernel-gcc -Wp,-MD,drivers/misc/mediatek/base/power/leakage_table_v2/.mtk_static_power_mt6739.o.d  -nostdinc -isystem /home/oslv_team/PROD_GANGA10/GALAXY_A01_CORE/FLUMEN/Mojito/MT6739/android/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/../lib/gcc/arm-linux-androideabi/4.9.x/include -I./arch/arm/include -I./arch/arm/include/generated  -I./drivers/misc/mediatek/include -I./include -I./arch/arm/include/uapi -I./arch/arm/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -Werror-implicit-function-declaration -Wno-format-security -std=gnu89 -fno-PIE -fno-dwarf2-cfi-asm -fno-omit-frame-pointer -mapcs -mno-sched-prolog -fno-ipa-sra -mabi=aapcs-linux -mfpu=vfp -marm -D__LINUX_ARM_ARCH__=7 -march=armv7-a -msoft-float -Uarm -fno-delete-null-pointer-checks -Os -Wno-maybe-uninitialized --param=allow-store-data-races=0 -DCC_HAVE_ASM_GOTO -Wframe-larger-than=1500 -fstack-protector-strong -Wno-unused-but-set-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-var-tracking-assignments -g -fno-inline-functions-called-once -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fno-merge-all-constants -fmerge-constants -fno-stack-check -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -Werror=date-time -Werror -I./drivers/misc/mediatek/include -I./drivers/misc/mediatek/include/mt-plat/mt6739/include/ -I./drivers/misc/mediatek/include/mt-plat/ -I./drivers/mmc/host/mediatek/mt6739 -I./drivers/misc/mediatek/base/power/include/ -I./drivers/misc/mediatek/base/power/include/leakage_table_v2/    -DKBUILD_BASENAME='"mtk_static_power_mt6739"'  -DKBUILD_MODNAME='"mtk_static_power_mt6739"' -c -o drivers/misc/mediatek/base/power/leakage_table_v2/.tmp_mtk_static_power_mt6739.o drivers/misc/mediatek/base/power/leakage_table_v2/mtk_static_power_mt6739.c

source_drivers/misc/mediatek/base/power/leakage_table_v2/mtk_static_power_mt6739.o := drivers/misc/mediatek/base/power/leakage_table_v2/mtk_static_power_mt6739.c

deps_drivers/misc/mediatek/base/power/leakage_table_v2/mtk_static_power_mt6739.o := \
  include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
    $(wildcard include/config/retpoline.h) \
    $(wildcard include/config/gcov/kernel.h) \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  include/linux/stringify.h \
  drivers/misc/mediatek/base/power/include/mtk_common_static_power.h \
    $(wildcard include/config/mach/mt6759.h) \
    $(wildcard include/config/mach/mt6763.h) \
    $(wildcard include/config/mach/mt6758.h) \
    $(wildcard include/config/mach/mt6739.h) \
    $(wildcard include/config/mach/mt6771.h) \
    $(wildcard include/config/mach/mt6775.h) \
    $(wildcard include/config/mach/mt6768.h) \
    $(wildcard include/config/mach/mt6785.h) \
    $(wildcard include/config/mach/mt6885.h) \
    $(wildcard include/config/mach/mt6765.h) \
  drivers/misc/mediatek/base/power/include/leakage_table_v2/mtk_static_power.h \
  include/linux/types.h \
    $(wildcard include/config/have/uid16.h) \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/lbdaf.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  include/uapi/linux/types.h \
  arch/arm/include/uapi/asm/types.h \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  arch/arm/include/generated/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/uapi/asm-generic/bitsperlong.h \
  include/uapi/linux/posix_types.h \
  include/linux/stddef.h \
  include/uapi/linux/stddef.h \
  arch/arm/include/uapi/asm/posix_types.h \
  include/uapi/asm-generic/posix_types.h \
  drivers/misc/mediatek/base/power/include/leakage_table_v2/mtk_static_power_mt6739.h \

drivers/misc/mediatek/base/power/leakage_table_v2/mtk_static_power_mt6739.o: $(deps_drivers/misc/mediatek/base/power/leakage_table_v2/mtk_static_power_mt6739.o)

$(deps_drivers/misc/mediatek/base/power/leakage_table_v2/mtk_static_power_mt6739.o):
