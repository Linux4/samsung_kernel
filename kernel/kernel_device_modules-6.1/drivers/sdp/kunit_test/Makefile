ifneq ($(CONFIG_UML), y)
  # Test file for device
  obj-$(CONFIG_ADAPTIVE_MIPI_V2_TEST_FOR_ON_DEVICE) += adaptive_mipi_v2_test.o
else
  # Test file for UML
  obj-$(CONFIG_ADAPTIVE_MIPI_V2_TEST_FOR_ONLY_UML) += adaptive_mipi_v2_test.o
endif