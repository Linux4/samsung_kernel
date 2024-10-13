# Makefile for the focaltech touchscreen drivers.



obj-$(CONFIG_TOUCHSCREEN_FT8057P_O8) += focaltech_tp.o
focaltech_tp-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o

focaltech_tp-y += focaltech_flash_spi.o

focaltech_tp-y += focaltech_spi_v2.o

focaltech_tp-y += focaltech_test/focaltech_test.o
focaltech_tp-y += focaltech_test/focaltech_test_ini.o
focaltech_tp-y += focaltech_test/supported_ic/focaltech_test_ft8057p.o