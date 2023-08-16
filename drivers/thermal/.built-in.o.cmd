cmd_drivers/thermal/built-in.o :=  arm-eabi-ld -EL    -r -o drivers/thermal/built-in.o drivers/thermal/thermal_sys.o drivers/thermal/exynos_thermal.o drivers/thermal/cal_tmu.o drivers/thermal/ipa.o 
