obj-$(CONFIG_FUELGAUGE_MAX77705)	+= max77705-fuelgauge.o
max77705-fuelgauge-$(CONFIG_FUELGAUGE_MAX77705)	+= max77705_fuelgauge.o

ifeq ($(CONFIG_SEC_KUNIT), y)
max77705-fuelgauge-$(CONFIG_FUELGAUGE_MAX77705_TEST)		+= kunit_test/max77705_fuelgauge_test.o
GCOV_PROFILE_max77705_fuelgauge.o := $(CONFIG_SEC_KUNIT)
endif
ccflags-y := -Wformat
