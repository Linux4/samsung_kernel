#
# Makefile for Infineon Authenticate IC
#
obj-$(CONFIG_BATTERY_AUTH_SLE956681) += sec-auth-sle956681.o
sec-auth-sle956681-$(CONFIG_BATTERY_AUTH_SLE956681) += sec_auth_sle956681.o libs/authon-sdk/platform/helper.o libs/authon-sdk/platform/bitbang.o libs/authon-sdk/ecc/ecc.o libs/authon-sdk/host_auth/host_auth.o libs/authon-sdk/interface/interface.o libs/authon-sdk/interface/swi_crc/swi_crc.o libs/authon-sdk/interface/swi_crc_bus_command.o libs/authon-sdk/nvm/nvm.o libs/authon-sdk/authon_api.o

ccflags-y := -Wformat