
obj-$(CONFIG_SDP) += sdp.o
sdp-y += adaptive_mipi_v2.o \
	sdp_debug.o

obj-$(CONFIG_KUNIT)			+= kunit_test/
GCOV_PROFILE_adaptive_mipi_v2.o		:= $(CONFIG_KUNIT)
