cmd_drivers/leds/built-in.o :=  arm-eabi-ld -EL    -r -o drivers/leds/built-in.o drivers/leds/led-core.o drivers/leds/led-class.o drivers/leds/leds-max77804.o drivers/leds/leds-an30259a.o 
