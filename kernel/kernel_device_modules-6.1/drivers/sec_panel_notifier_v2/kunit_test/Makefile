ifneq ($(CONFIG_UML), y)
  # Test file for device
  obj-$(CONFIG_SEC_PANEL_NOTIFIER_V2_TEST_FOR_ON_DEVICE) += sec_panel_notifier_v2_test.o
else
  # Test file for UML
  obj-$(CONFIG_SEC_PANEL_NOTIFIER_V2_TEST_FOR_ONLY_UML) += sec_panel_notifier_v2_test.o
endif
