cmd_firmware/tsp_stm/stm_tb_seper_1.fw.gen.o := arm-eabi-gcc -Wp,-MD,firmware/tsp_stm/.stm_tb_seper_1.fw.gen.o.d  -nostdinc -isystem /home/amardeep.s/OSLV/Google/ARM_4.8/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/../lib/gcc/arm-eabi/4.8/include -I/home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include -Iarch/arm/include/generated  -Iinclude -I/home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/uapi -Iarch/arm/include/generated/uapi -I/home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/include/uapi -Iinclude/generated/uapi -include /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-exynos/include -Iarch/arm/plat-samsung/include  -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -marm -D__LINUX_ARM_ARCH__=7 -march=armv7-a  -include asm/unified.h -msoft-float -gdwarf-2         -c -o firmware/tsp_stm/stm_tb_seper_1.fw.gen.o firmware/tsp_stm/stm_tb_seper_1.fw.gen.S

source_firmware/tsp_stm/stm_tb_seper_1.fw.gen.o := firmware/tsp_stm/stm_tb_seper_1.fw.gen.S

deps_firmware/tsp_stm/stm_tb_seper_1.fw.gen.o := \
  /home/amardeep.s/OSLV/SM-G850F_SEA_LL_DX/Kernel/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \

firmware/tsp_stm/stm_tb_seper_1.fw.gen.o: $(deps_firmware/tsp_stm/stm_tb_seper_1.fw.gen.o)

$(deps_firmware/tsp_stm/stm_tb_seper_1.fw.gen.o):
