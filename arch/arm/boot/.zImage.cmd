cmd_arch/arm/boot/zImage := /opt/toolchains/arm-eabi-4.7/bin/arm-eabi-objcopy -O binary -R .comment -S  arch/arm/boot/compressed/vmlinux arch/arm/boot/zImage
