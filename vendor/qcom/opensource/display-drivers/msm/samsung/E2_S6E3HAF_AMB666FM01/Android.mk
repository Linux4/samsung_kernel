XXD := /usr/bin/xxd
SED := /bin/sed

#Translate .dat file to .h to cover the case which can not use request_firmware(Recovery Mode)
CLEAR_TMP := $(shell rm -f E2_S6E3HAF_AMB666FM01_PDF_DATA)
COPY_TO_HERE := $(shell cp -vf $(DISPLAY_BLD_DIR)/msm/samsung/panel_data_file/E2_S6E3HAF_AMB666FM01.dat E2_S6E3HAF_AMB666FM01_PDF_DATA)
DATA_TO_HEX := $(shell $(XXD) -i E2_S6E3HAF_AMB666FM01_PDF_DATA > $(DISPLAY_BLD_DIR)/msm/samsung/E2_S6E3HAF_AMB666FM01/E2_S6E3HAF_AMB666FM01_PDF.h)
ADD_NULL_CHR := $(shell $(SED) -i -e 's/\([0-9a-f]\)$$/\0, 0x00/' $(DISPLAY_BLD_DIR)/msm/samsung/E2_S6E3HAF_AMB666FM01/E2_S6E3HAF_AMB666FM01_PDF.h)
