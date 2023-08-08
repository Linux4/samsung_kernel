#EXTRA_CFLAGS= -I$(shell pwd)
ccflags-y += -I$(shell pwd)

obj-y += snd-soc-tas256x.o

snd-soc-tas256x-y := os_layer/src/tas256x-codec.o \
	os_layer/src/tas256x-regmap.o \
	physical_layer/src/tas256x.o \
	logical_layer/src/tas256x-logic.o

snd-soc-tas256x-$(CONFIG_TAS256X_MISC) += misc/tas256x-misc.o
snd-soc-tas256x-$(CONFIG_TAS25XX_ALGO) += algo/src/tas25xx-algo-intf.o \
	algo/src/tas25xx-calib.o \
	algo/src/tas25xx-algo-common.o

snd-soc-tas256x-$(CONFIG_PLATFORM_QCOM) += algo/platform/qcom/tas25xx-algo-qdsp-intf.o

snd-soc-tas256x-$(CONFIG_TAS25XX_CALIB_VAL_BIG) += algo/src/tas25xx-calib-validation.o
