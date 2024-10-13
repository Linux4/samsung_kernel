mkdir output
make -C $(pwd) O=output goyave3g-dt_hw04_defconfig 
make -C $(pwd) O=output
cp $(pwd)/output/arch/arm/boot/zImage $(pwd)/arch/arm/boot/zImage
