obj-$(CONFIG_SCHED_EMS) += ems.o

ems-y += main.o multi_load.o hook.o profile.o perf_events.o
ems-$(CONFIG_SCHED_EMS_CORE_SELECT) += core-select-main.o core.o ontime.o sysbusy.o frt.o \
				       balance.o frt.o gsc.o tune.o
ems-$(CONFIG_SCHED_EMS_FREQ_SELECT) += freq-select-main.o cpufreq.o freqboost.o ego.o energy_table.o
ems-$(CONFIG_SCHED_EMS_IDLE_SELECT) += idle-select-main.o ecs.o ecs_dynamic.o halo.o freq_variant.o
ems-$(CONFIG_MHDVFS) += mhdvfs.o
ems-$(CONFIG_SCHED_EMS_PAGO) += pago.o
ems-$(CONFIG_SCHED_EMS_TASK_GROUP) += taskgroup.o
