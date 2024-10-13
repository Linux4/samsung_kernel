M=$(abspath $(lastword $(MAKEFILE_LIST)))
SSG_MODULE_ROOT=$(dir $(M))
INC=-I/$(M)/linux/*
KBUILD_OPTIONS+=SSG_MODULE_ROOT=$(M)

all: modules

clean:
	rm -f *.cmd *.d *.mod *.o *.ko *.mod.c *.mod.o Module.symvers modules.order

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(INC) $@ $(KBUILD_OPTIONS)
