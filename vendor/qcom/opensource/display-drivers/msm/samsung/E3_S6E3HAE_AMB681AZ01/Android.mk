XXD := /usr/bin/xxd
SED := /bin/sed

#Translate .dat file to .h to cover the case which can not use request_firmware(Recovery Mode)
CLEAR_TMP := $(shell rm -f E3_S6E3HAE_AMB681AZ01_PDF_DATA)
COPY_TO_HERE := $(shell cp -vf $(DISPLAY_BLD_DIR)/msm/samsung/panel_data_file/E3_S6E3HAE_AMB681AZ01.dat E3_S6E3HAE_AMB681AZ01_PDF_DATA)
DATA_TO_HEX := $(shell $(XXD) -i E3_S6E3HAE_AMB681AZ01_PDF_DATA > $(DISPLAY_BLD_DIR)/msm/samsung/E3_S6E3HAE_AMB681AZ01/E3_S6E3HAE_AMB681AZ01_PDF.h)
ADD_NULL_CHR := $(shell $(SED) -i -e 's/\([0-9a-f]\)$$/\0, 0x00/' $(DISPLAY_BLD_DIR)/msm/samsung/E3_S6E3HAE_AMB681AZ01/E3_S6E3HAE_AMB681AZ01_PDF.h)
