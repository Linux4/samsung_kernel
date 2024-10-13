XXD := /usr/bin/xxd
SED := /bin/sed

#Translate .dat file to .h to cover the case which can not use request_firmware(Recovery Mode)
CLEAR_TMP := $(shell rm -f B6_S6E3FAC_AMF670GN01_PDF_DATA)
COPY_TO_HERE := $(shell cp -vf $(DISPLAY_BLD_DIR)/msm/samsung/panel_data_file/B6_S6E3FAC_AMF670GN01.dat B6_S6E3FAC_AMF670GN01_PDF_DATA)
DATA_TO_HEX := $(shell $(XXD) -i B6_S6E3FAC_AMF670GN01_PDF_DATA > $(DISPLAY_BLD_DIR)/msm/samsung/B6_S6E3FAC_AMF670GN01/B6_S6E3FAC_AMF670GN01_PDF.h)
ADD_NULL_CHR := $(shell $(SED) -i -e 's/\([0-9a-f]\)$$/\0, 0x00/' $(DISPLAY_BLD_DIR)/msm/samsung/B6_S6E3FAC_AMF670GN01/B6_S6E3FAC_AMF670GN01_PDF.h)
