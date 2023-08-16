cmd_drivers/irqchip/built-in.o :=  arm-eabi-ld -EL    -r -o drivers/irqchip/built-in.o drivers/irqchip/irqchip.o drivers/irqchip/exynos-combiner.o drivers/irqchip/irq-gic.o 
