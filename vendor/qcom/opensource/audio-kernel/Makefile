M=$(PWD)
AUDIO_ROOT=$(KERNEL_SRC)/$(M)

KBUILD_OPTIONS+=  AUDIO_ROOT=$(AUDIO_ROOT)

# Include Makefile.include for Samsung specific configurations
$(info "run Makefile.include in audio-kernel for Samsung boards")
include $(AUDIO_ROOT)/Makefile.include

all: modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)

