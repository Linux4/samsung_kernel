M=$(PWD)
SSG_MODULE_ROOT=$(KERNEL_SRC)/$(M)
INC=-I/$(M)/linux/*
KBUILD_OPTIONS+=SSG_MODULE_ROOT=$(SSG_MODULE_ROOT)

all: modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(INC) $@ $(KBUILD_OPTIONS)