obj-$(CONFIG_TROUT_FM) := trout_fm.o

trout_fm-$(CONFIG_RF_2829) += trout_rf_2829.o
trout_fm-$(CONFIG_RF_55) += trout_rf_55.o
trout_fm-$(CONFIG_RF_SHARK) += trout_rf_shark.o
trout_fm-$(CONFIG_INTERFACE_SHARED) += trout_shared.o
trout_fm-$(CONFIG_INTERFACE_ONCHIP) += trout_onchip.o

trout_fm-objs := trout_fm_ctrl.o
trout_fm-objs += $(trout_fm-y)

