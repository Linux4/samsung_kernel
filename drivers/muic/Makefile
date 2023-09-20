#
# Makefile for muic devices
#

obj-y					+= muic-core.o
obj-$(CONFIG_MUIC_NOTIFIER)		+= muic_notifier.o
obj-$(CONFIG_MUIC_FSA9480)		+= fsa9480.o
obj-$(CONFIG_MUIC_S2MM001)		+= s2mm001.o
obj-$(CONFIG_MUIC_SM5703)		+= sm5703-muic.o
obj-$(CONFIG_MUIC_S2MU005)		+= s2mu005-muic.o

# MUIC_UNIVERSAL
obj-$(CONFIG_MUIC_UNIVERSAL)		+= universal/muic_task.o
obj-$(CONFIG_MUIC_UNIVERSAL)		+= universal/muic_state.o
obj-$(CONFIG_MUIC_UNIVERSAL)		+= universal/muic_apis.o
obj-$(CONFIG_MUIC_UNIVERSAL)		+= universal/muic_sysfs.o
obj-$(CONFIG_MUIC_UNIVERSAL)		+= universal/muic_debug.o
obj-$(CONFIG_MUIC_UNIVERSAL)		+= universal/muic_dt.o
obj-$(CONFIG_MUIC_UNIVERSAL)		+= universal/muic_i2c.o
obj-$(CONFIG_MUIC_UNIVERSAL)		+= universal/muic_regmap.o
obj-$(CONFIG_MUIC_UNIVERSAL)		+= universal/muic_vps.o
obj-$(CONFIG_MUIC_UNIVERSAL_SM5703)	+= universal/muic_regmap_sm5703.o
