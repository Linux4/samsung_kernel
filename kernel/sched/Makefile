# SPDX-License-Identifier: GPL-2.0
ifdef CONFIG_FUNCTION_TRACER
CFLAGS_REMOVE_clock.o = $(CC_FLAGS_FTRACE)
endif

# These files are disabled because they produce non-interesting flaky coverage
# that is not a function of syscall inputs. E.g. involuntary context switches.
KCOV_INSTRUMENT := n

ifneq ($(CONFIG_SCHED_OMIT_FRAME_POINTER),y)
# According to Alan Modra <alan@linuxcare.com.au>, the -fno-omit-frame-pointer is
# needed for x86 only.  Why this used to be enabled for all architectures is beyond
# me.  I suspect most platforms don't need this, but until we know that for sure
# I turn this off for IA-64 only.  Andreas Schwab says it's also needed on m68k
# to get a correct value for the wait-channel (WCHAN in ps). --davidm
CFLAGS_core.o := $(PROFILING) -fno-omit-frame-pointer
endif

obj-y += core.o loadavg.o clock.o cputime.o
GCOV_PROFILE_core.o :=$(CONFIG_SPRD_SCHED_GCOV)
obj-y += idle_task.o fair.o rt.o deadline.o
GCOV_PROFILE_idle_task.o :=$(CONFIG_SPRD_SCHED_GCOV)
GCOV_PROFILE_fair.o :=$(CONFIG_SPRD_SCHED_GCOV)
GCOV_PROFILE_rt.o :=$(CONFIG_SPRD_SCHED_GCOV)
GCOV_PROFILE_deadline.o :=$(CONFIG_SPRD_SCHED_GCOV)
obj-y += wait.o wait_bit.o swait.o completion.o idle.o
GCOV_PROFILE_idle.o :=$(CONFIG_SPRD_SCHED_GCOV)
obj-$(CONFIG_SMP) += cpupri.o cpudeadline.o topology.o stop_task.o
GCOV_PROFILE_topology.o :=$(CONFIG_SPRD_SCHED_GCOV)
obj-$(CONFIG_GENERIC_ARCH_TOPOLOGY) += energy.o
GCOV_PROFILE_energy.o :=$(CONFIG_SPRD_SCHED_GCOV)
obj-$(CONFIG_SCHED_WALT) += walt.o
GCOV_PROFILE_walt.o :=$(CONFIG_SPRD_SCHED_GCOV)
obj-$(CONFIG_SCHED_AUTOGROUP) += autogroup.o
obj-$(CONFIG_SCHEDSTATS) += stats.o
obj-$(CONFIG_SCHED_DEBUG) += debug.o
obj-$(CONFIG_SCHED_TUNE) += tune.o
GCOV_PROFILE_tune.o :=$(CONFIG_SPRD_SCHED_GCOV)
obj-$(CONFIG_CGROUP_CPUACCT) += cpuacct.o
obj-$(CONFIG_CPU_FREQ) += cpufreq.o
GCOV_PROFILE_cpufreq.o :=$(CONFIG_SPRD_SCHED_GCOV)
obj-$(CONFIG_CPU_FREQ_GOV_SCHEDUTIL) += cpufreq_schedutil.o
GCOV_PROFILE_cpufreq_schedutil.o :=$(CONFIG_SPRD_SCHED_GCOV)
obj-$(CONFIG_MEMBARRIER) += membarrier.o
obj-$(CONFIG_PSI) += psi.o
obj-$(CONFIG_SPRD_CORE_CTL) += sprd_core_ctl.o
GCOV_PROFILE_sprd_core_ctl.o := $(CONFIG_SPRD_CORE_CTL_GCOV)
