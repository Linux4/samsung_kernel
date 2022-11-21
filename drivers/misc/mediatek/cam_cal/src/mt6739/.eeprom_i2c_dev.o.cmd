cmd_drivers/misc/mediatek/cam_cal/src/mt6739/eeprom_i2c_dev.o := /home/oslv_team/PROD_GANGA10/GALAXY_A01_CORE/FLUMEN/Mojito/MT6739/android//prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androidkernel-gcc -Wp,-MD,drivers/misc/mediatek/cam_cal/src/mt6739/.eeprom_i2c_dev.o.d  -nostdinc -isystem /home/oslv_team/PROD_GANGA10/GALAXY_A01_CORE/FLUMEN/Mojito/MT6739/android/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/../lib/gcc/arm-linux-androideabi/4.9.x/include -I./arch/arm/include -I./arch/arm/include/generated  -I./drivers/misc/mediatek/include -I./include -I./arch/arm/include/uapi -I./arch/arm/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -Werror-implicit-function-declaration -Wno-format-security -std=gnu89 -fno-PIE -fno-dwarf2-cfi-asm -fno-omit-frame-pointer -mapcs -mno-sched-prolog -fno-ipa-sra -mabi=aapcs-linux -mfpu=vfp -marm -D__LINUX_ARM_ARCH__=7 -march=armv7-a -msoft-float -Uarm -fno-delete-null-pointer-checks -Os -Wno-maybe-uninitialized --param=allow-store-data-races=0 -DCC_HAVE_ASM_GOTO -Wframe-larger-than=1500 -fstack-protector-strong -Wno-unused-but-set-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-var-tracking-assignments -g -fno-inline-functions-called-once -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fno-merge-all-constants -fmerge-constants -fno-stack-check -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -Werror=date-time -Werror -I./drivers/misc/mediatek/include -I./drivers/misc/mediatek/include/mt-plat/mt6739/include/ -I./drivers/misc/mediatek/include/mt-plat/ -I./drivers/mmc/host/mediatek/mt6739 -I./drivers/misc/mediatek/imgsensor/inc -I./drivers/misc/mediatek/imgsensor/src/"mt6739"/camera_hw -I./drivers/misc/mediatek/cam_cal/inc -I./drivers/misc/mediatek/cam_cal/src/mt6739 -I./drivers/misc/mediatek/cam_cal/src/common/v1 -I./drivers/i2c/busses/    -DKBUILD_BASENAME='"eeprom_i2c_dev"'  -DKBUILD_MODNAME='"eeprom_i2c_dev"' -c -o drivers/misc/mediatek/cam_cal/src/mt6739/.tmp_eeprom_i2c_dev.o drivers/misc/mediatek/cam_cal/src/mt6739/eeprom_i2c_dev.c

source_drivers/misc/mediatek/cam_cal/src/mt6739/eeprom_i2c_dev.o := drivers/misc/mediatek/cam_cal/src/mt6739/eeprom_i2c_dev.c

deps_drivers/misc/mediatek/cam_cal/src/mt6739/eeprom_i2c_dev.o := \
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
  drivers/misc/mediatek/cam_cal/src/common/v1/eeprom_i2c_dev.h \
  drivers/misc/mediatek/imgsensor/inc/kd_camera_feature.h \
  drivers/misc/mediatek/imgsensor/inc/kd_camera_feature_id.h \
  drivers/misc/mediatek/imgsensor/inc/kd_camera_feature_enum.h \

drivers/misc/mediatek/cam_cal/src/mt6739/eeprom_i2c_dev.o: $(deps_drivers/misc/mediatek/cam_cal/src/mt6739/eeprom_i2c_dev.o)

$(deps_drivers/misc/mediatek/cam_cal/src/mt6739/eeprom_i2c_dev.o):
