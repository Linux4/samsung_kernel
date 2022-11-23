obj-$(CONFIG_SB_CORE)	+= sb-core.o
sb-core-$(CONFIG_SB_CORE)	+= sb_core.o
sb-core-$(CONFIG_SB_PQUEUE)	+= sb_pqueue.o
sb-core-$(CONFIG_SB_NOTIFY)	+= sb_notify.o
sb-core-$(CONFIG_SB_SYSFS)	+= sb_sysfs.o
sb-core-$(CONFIG_SB_VOTE)	+= sb_vote.o

ifeq ($(CONFIG_SEC_KUNIT), y)
 sb-core-$(CONFIG_SB_PQUEUE)		+= kunit_test/sb_pqueue_test.o
 GCOV_PROFILE_sb_pqueue.o := $(CONFIG_SEC_KUNIT)
 sb-core-$(CONFIG_SB_NOTIFY)		+= kunit_test/sb_notify_test.o
 GCOV_PROFILE_sb_notify.o := $(CONFIG_SEC_KUNIT)
 sb-core-$(CONFIG_SB_SYSFS)		+= kunit_test/sb_sysfs_test.o
 GCOV_PROFILE_sb_sysfs.o := $(CONFIG_SEC_KUNIT)
 sb-core-$(CONFIG_SB_VOTE)		+= kunit_test/sb_vote_test.o
 GCOV_PROFILE_sb_vote.o := $(CONFIG_SEC_KUNIT)
endif

ccflags-y := -Wformat
