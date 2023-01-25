#
# Makefile for the Cypress touchkey driver
#

ifdef CONFIG_SEC_PRODUCT_8960
obj-$(CONFIG_KEYBOARD_CYPRESS_TOUCH_236) += cypress-touchkey-236_msm8960.o issp_main.o issp_routines.o issp_driver_routines_msm8960.o
else
obj-$(CONFIG_KEYBOARD_CYPRESS_TOUCH_236) += cypress-touchkey-236.o issp_main.o issp_routines.o issp_driver_routines.o
endif