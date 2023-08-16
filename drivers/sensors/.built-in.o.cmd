cmd_drivers/sensors/built-in.o :=  arm-eabi-ld -EL    -r -o drivers/sensors/built-in.o drivers/sensors/sensors_core.o drivers/sensors/max86900.o drivers/sensors/tmd3782.o 
