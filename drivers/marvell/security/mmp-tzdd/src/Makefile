
# debug for TZDD
TZDD_DBG := N

# debug proc fs
TZDD_DBG_PROC_FS := Y

# debug for OSA
OSA_DEBUG := Y

# measure performance
TZDD_PERF_REC := N

# sharedMem context checking
TZDD_SHAREDMEM_CONTEXT_CHECKING := N

obj-m := tzdd.o
tzdd-objs := \
	osa/osa.o \
	osa/osa_atomic.o \
	osa/osa_dbg.o \
	osa/osa_delay.o \
	osa/osa_irq.o \
	osa/osa_list.o \
	osa/osa_mem.o \
	osa/osa_sync.o \
	osa/osa_thread.o \
	osa/osa_sched.o \
	\
	main/tzdd_main_core.o \
	main/tzdd_main.o \
	main/tzdd_pt.o \
	main/tzdd_pt_core.o \
	main/teec_client_api.o \
	main/teec_cb.o \
	main/teec_time.o \
	\
	msgm/ntw/tee_msgm_handle_params.o \
	msgm/ntw/tee_msgm_close_ss.o \
	msgm/ntw/tee_msgm_open_ss.o \
	msgm/ntw/tee_msgm_map_shm.o \
	msgm/ntw/tee_msgm_unmap_shm.o \
	msgm/ntw/tee_msgm_inv_op.o \
	msgm/ntw/tee_msgm_can_op.o \
	msgm/ntw/tee_msgm_ntw.o \
	\
	comm/tee_cm.o \
	comm/tee_cm_smi.o \
	comm/ntw/tee_cm_init_ntw.o \
	comm/ntw/tee_cm_misc_ntw.o \
	\
	memm/tee_memm.o

ifeq ($(TZDD_DBG), Y)
ccflags-y += -DTZDD_DEBUG
endif

ifeq ($(OSA_DEBUG), Y)
ccflags-y += -DOSA_DEBUG
endif

ifeq ($(TZDD_PERF_REC), Y)
ccflags-y += -DTEE_PERF_MEASURE -I$(CWD)/perf/ntw
tzdd-objs += perf/tee_perf.o
endif

ifeq ($(TZDD_DBG_PROC_FS), Y)
ccflags-y += -DTEE_DEBUG_ENALBE_PROC_FS_LOG
tzdd-objs += dbg/ntw/tee_dbg_log.o
endif

ifeq ($(TZDD_SHAREDMEM_CONTEXT_CHECKING), Y)
EXTRA_CFLAGS += -DSHAREDMEM_CONTEXT_CHECKING
endif

ccflags-y += -Wall -O2 \
				-I$(src)/osa/ \
				-I$(src)/osa/include \
				-I$(src)/../inc \
				-I$(src)/inc \
				-I$(src)/comm/ntw \
				-I$(src)/main \
				-I$(src)/memm

