pablo-self-tests-objs := punit-test.o \
					punit-test-hw-ip.o \
					punit-test-file-io.o \
					punit-test-crta-mock.o \
					punit-test-hw-mock.o \
					punit-test-criteria.o \
					punit-test-param.o \
					punit-power-test.o \
					punit-group-test.o \
					punit-phy-test.o \
					punit-dvfs-test.o

pablo-self-tests-$(CONFIG_PABLO_V11_0_0)+= punit-hw-byrp-test.o \
					punit-hw-rgbp-test.o \
					punit-hw-yuvp-test.o \
					punit-hw-mcsc-test.o \
					punit-hw-mcfp-test.o \
					punit-hw-lme-test.o \
					punit-hw-cstat-test-v2_0.o \
					punit-hw-cstat-test.o \
					punit-hw-byrp2rgbp-test.o \
					punit-hw-byrp2mcsc-test.o

pablo-self-tests-$(CONFIG_PABLO_V11_20_0)+= punit-hw-byrp-test.o \
					punit-hw-rgbp-test.o \
					punit-hw-yuvp-test.o \
					punit-hw-mcsc-test.o \
					punit-hw-mcfp-test.o \
					punit-hw-lme-test.o \
					punit-hw-cstat-test-v2_0.o \
					punit-hw-cstat-test.o \
					punit-hw-byrp2rgbp-test.o \
					punit-hw-byrp2mcsc-test.o

pablo-self-tests-$(CONFIG_PABLO_V11_30_0)+= punit-hw-byrp-test.o \
					punit-hw-rgbp-test.o \
					punit-hw-yuvp-test.o \
					punit-hw-mcsc-test.o \
					punit-hw-mcfp-test.o \
					punit-hw-lme-test.o \
					punit-hw-cstat-test-v2_0.o \
					punit-hw-cstat-test.o \
					punit-hw-byrp2rgbp-test.o \
					punit-hw-byrp2mcsc-test.o

pablo-self-tests-$(CONFIG_PABLO_V12_0_0)+= punit-hw-byrp-test.o \
					punit-hw-rgbp-test.o \
					punit-hw-yuvp-test.o \
					punit-hw-shrp-test.o \
					punit-hw-mcsc-test.o \
					punit-hw-mcfp-test.o \
					punit-hw-lme-test.o \
					punit-hw-cstat-test-v3_0.o \
					punit-hw-cstat-test.o \
					punit-hw-byrp2rgbp-test.o \
					punit-hw-byrp2mcsc-test.o \
					punit-hw-mcfp2dlfe-test.o

pablo-self-tests-$(CONFIG_PABLO_V12_1_0)+= punit-hw-byrp-test.o \
					punit-hw-rgbp-test.o \
					punit-hw-yuvp-test.o \
					punit-hw-shrp-test.o \
					punit-hw-mcsc-test.o \
					punit-hw-mcfp-test.o \
					punit-hw-lme-test.o \
					punit-hw-cstat-test-v3_0.o \
					punit-hw-cstat-test.o \
					punit-hw-byrp2rgbp-test.o \
					punit-hw-byrp2mcsc-test.o \
					punit-hw-mcfp2dlfe-test.o

pablo-self-tests-$(CONFIG_PABLO_V12_20_0)+= punit-hw-byrp-test.o \
					punit-hw-rgbp-test.o \
					punit-hw-yuvp-test.o \
					punit-hw-mcsc-test.o \
					punit-hw-mcfp-test.o \
					punit-hw-lme-test.o \
					punit-hw-cstat-test-v2_20.o \
					punit-hw-cstat-test.o \
					punit-hw-byrp2rgbp-test.o \
					punit-hw-byrp2mcsc-test.o

pablo-self-tests-$(CONFIG_PABLO_DLFE)+= punit-hw-dlfe-test.o
pablo-self-tests-$(CONFIG_PABLO_YUVSC)+= punit-hw-yuvsc-test.o

pablo-self-tests-objs += sensor/cis/punit-cis-test.o
pablo-self-tests-objs += sensor/cis/punit-test-cis-result.o

obj-$(CONFIG_PABLO_UNIT_TEST) += pablo-self-tests.o
