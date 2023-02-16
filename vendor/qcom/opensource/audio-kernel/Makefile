M=$(PWD)
AUDIO_ROOT=$(KERNEL_SRC)/$(M)

KBUILD_OPTIONS+=  AUDIO_ROOT=$(AUDIO_ROOT)

$(shell rm -fr $(AUDIO_ROOT)/soc/core.h)
$(shell ln -s $(KERNEL_SRC)/drivers/pinctrl/core.h $(AUDIO_ROOT)/soc/core.h)
$(shell rm -fr $(AUDIO_ROOT)/include/soc/internal.h)
$(shell ln -s $(KERNEL_SRC)/drivers/base/regmap/internal.h $(AUDIO_ROOT)/include/soc/internal.h)
$(shell rm -fr $(AUDIO_ROOT)/soc/pinctrl-utils.h)
$(shell ln -s $(KERNEL_SRC)/drivers/pinctrl/pinctrl-utils.h $(AUDIO_ROOT)/soc/pinctrl-utils.h)

# Include Makefile.include for Samsung specific configurations
$(info "run Makefile.include in audio-kernel for Samsung boards")
include $(AUDIO_ROOT)/Makefile.include

all: modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)

