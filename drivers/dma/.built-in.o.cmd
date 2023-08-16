cmd_drivers/dma/built-in.o :=  arm-eabi-ld -EL    -r -o drivers/dma/built-in.o drivers/dma/dmaengine.o drivers/dma/of-dma.o drivers/dma/pl330.o 
