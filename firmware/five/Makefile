# FIVE TA
five_fwlist := \
    five/five.mdt \
    five/five.b00 \
    five/five.b01 \
    five/five.b02 \
    five/five.b03 \
    five/five.b04 \
    five/five.b05 \
    five/five.b06 \
    five/five.b07

five_fwdir := firmware/five
five_signed_fwdir := firmware/five/signed
five_objs := $(addprefix $(obj)/,$(five_fwlist))

# Tools
five_split_tool := $(srctree)/tools/pil_splitter.py
ifeq ($(SIGNCLIENT_JAR),)
	five_sign_tool := $(srctree)/tools/signclient.jar
else
	five_sign_tool := $(SIGNCLIENT_JAR)
endif

ifeq (exist, $(shell [ -e $(five_split_tool) ] && echo exist))
# Setup five_sign_runtype and five_sign_model
include $(srctree)/firmware/five/sign_mode.mk

$(five_objs): $(five_fwdir)/five.signed

$(five_fwdir)/five.signed: $(five_split_tool) $(five_signed_fwdir)/five.mbn
	python $(five_split_tool) $(five_signed_fwdir)/five.mbn $(five_fwdir)/five
	touch $@
	# Remove five_split_tool for OSRC
	rm -rf $(five_split_tool)

$(five_signed_fwdir)/five.mbn: $(five_sign_tool) $(srctree)/firmware/five/five.mbn
	mkdir -p $(five_signed_fwdir)
	java -jar $(five_sign_tool) -model $(five_sign_model) \
		-runtype $(five_sign_runtype) -input $(srctree)/firmware/five/five.mbn \
		-output $(five_signed_fwdir)/five.mbn

five_fwabs := $(addprefix $(objtree)/,$(filter-out /%,$(fwdir)))$(filter /%,$(fwdir))

$(patsubst %,$(obj)/%.gen.S, $(five_fwlist)): %: $(wordsize_deps)
	$(call cmd,fwbin,$(five_fwabs)/$(patsubst $(obj)/%.gen.S,%,$@))

# The .o files depend on the binaries directly; the .S files don't.
$(patsubst %,$(obj)/%.gen.o, $(five_fwlist)): $(obj)/%.gen.o: $(fwdir)/%

obj-y += $(patsubst %,%.gen.o, $(five_fwlist))

clean-files += $(five_objs) $(five_fwdir)/five.signed
clean-dirs += $(five_signed_fwdir)

else
fw-external-$(CONFIG_FIVE_TEE_DRIVER) += five/five.b00 five/five.b01 \
	five/five.b02 five/five.b03 five/five.b04 five/five.b05 five/five.b06 \
	five/five.b07 five/five.mdt
endif
