ifeq ($(CONFIG_SEC_A60Q_PROJECT),y)
five_old_signature := y
endif
ifeq ($(CONFIG_SEC_A70Q_PROJECT),y)
five_old_signature := y
endif
ifeq ($(CONFIG_SEC_A70S_PROJECT),y)
five_old_signature := y
endif
ifeq ($(CONFIG_SEC_A70SQ_PROJECT),y)
five_old_signature := y
endif
ifeq ($(CONFIG_SEC_M40_PROJECT),y)
five_old_signature := y
endif
ifeq ($(CONFIG_SEC_M41_PROJECT),y)
five_old_signature := y
endif
ifeq ($(CONFIG_SEC_M51_PROJECT),y)
five_old_signature := y
endif
ifeq ($(CONFIG_SEC_R1Q_PROJECT),y)
five_old_signature := y
endif

ifeq ($(five_old_signature),y)
five_sign_runtype := qc_secimg50_tzapp
five_sign_model := QSEE_SM6150_TA
else
five_sign_runtype := qc_secimg50_tzapp
five_sign_model := $(SEC_BUILD_CONF_SIGNER_MODEL_NAME)
ifndef SEC_BUILD_CONF_SIGNER_MODEL_NAME
  $(error "SEC_BUILD_CONF_SIGNER_MODEL_NAME isn't defined")
endif
endif
