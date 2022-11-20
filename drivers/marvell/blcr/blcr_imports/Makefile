#$(obj)/imports.c : $(srctree)/System.map
#		cd $(obj)/..;./blcr_symb.sh  $< 2>/dev/null	

clean-files = imports.c

obj-m    := blcr_imports.o
blcr_imports-objs := imports.o module.o

$(obj)/imports.o $(obj)/module.o : | $(obj)/imports.c

EXTRA_CFLAGS := -I$(obj)/.. -I$(obj)/../include

#.PHONY: all clean
#all:
#	$(MAKE) -C $(KDIR) M=$(PWD) CROSS_COMPILE=arm-eabi- ARCH=arm modules

#clean:
#	rm -rf *.o *.ko *.mod.c .*.cmd ./.tmp* *~ modules.order Module.symvers

