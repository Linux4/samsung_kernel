# Samsung sec ABC Feature
obj-$(CONFIG_SEC_ABC)		+= abc_common.o

# Samsung sec ABC_HUB Feature
obj-$(CONFIG_SEC_ABC_HUB)	+= abc_hub.o

# Samsung sec ABC_HUB Sub Module(Connect detect) Feature
obj-$(CONFIG_SEC_ABC_HUB_COND)	+= abc_hub_cond.o

# Samsung sec ABC_HUB Sub Module(Booting time check) Feature
obj-$(CONFIG_SEC_ABC_HUB_BOOTC)	+= abc_hub_bootc.o

# Samsung sec ABC KUnit Test Feature
obj-$(CONFIG_KUNIT) += abc-test.o

GCOV_PROFILE := $(CONFIG_KUNIT)
obj-$(CONFIG_KUNIT) += test/
