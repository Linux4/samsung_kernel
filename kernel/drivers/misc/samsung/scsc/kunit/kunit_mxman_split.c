int pmu_init_result = 0;
int fw_init_result = 0;
int allocator_init_result = 0;
int res_reset_result = 0;
int res_init_result = 0;
int wait_for_result = 1;

#define mxman_res_init_subsystem(args...) ((int)res_init_result)
#define mxman_res_deinit_subsystem(args...) ((int)res_init_result)
#define mxman_res_pmu_boot(args...) ((int)pmu_init_result)
#define mxman_res_pmu_init(args...) ((int)pmu_init_result)
#define mxman_res_pmu_deinit(args...) ((int)pmu_init_result)
#define mxman_res_pmu_reset(args...) ((int)pmu_init_result)
#define mxman_res_fw_init(args...) ((int)fw_init_result)
#define mxman_res_mappings_allocator_init(args...) ((int)allocator_init_result)
#define mxman_res_mappings_allocator_deinit(args...) ((int)allocator_init_result)
#define mxman_res_reset(args...) ((int)res_reset_result)
#define panicmon_init(args...) ((void *)0)
#define panicmon_deinit(args...) ((void *)0)
#define wait_for_completion_timeout(args...)	((int)wait_for_result)
#define mxmgmt_transport_send(args...) ((void *)0)
#define whdr_destroy(args...) ((void *)0)
#define scsc_log_collector_schedule_collection(args...) ((void *)0)
#define mxlogger_deinit(args...) ((void *)0)
#define mxlog_release(args...) ((void *)0)
#define srvman_notify_services(args...) ((u8)0)
#define srvman_set_error(args...) ((void *)0)
#define srvman_freeze_services(args...) ((void *)0)
#define srvman_clear_error(args...) ((void *)0)
#define complete(arg)	((void *)0)
#define scsc_lerna_init()	((void *)0)
#define whdr_crc_wq_start(arg)  ((void*)0)
#define whdr_crc_wq_stop(arg)  ((void*)0)

static int fw_runtime_flags_setter(const char *val, const struct kernel_param *kp);
static int fw_runtime_flags_setter_wpan(const char *val, const struct kernel_param *kp);
static int syserr_setter(const char *val, const struct kernel_param *kp);
static int mxman_minimoredump_collect_wpan(struct scsc_log_collector_client *collect_client, size_t size);

(*fp_fw_runtime_flags_setter)(const char *val, const struct kernel_param *kp) = &fw_runtime_flags_setter;
(*fp_fw_runtime_flags_setter_wpan)(const char *val, const struct kernel_param *kp) = &fw_runtime_flags_setter_wpan;
(*fp_syserr_setter)(const char *val, const struct kernel_param *kp) = &syserr_setter;
(*fp_mxman_minimoredump_collect_wpan)(struct scsc_log_collector_client *collect_client, size_t size) = &mxman_minimoredump_collect_wpan;

static char *chip_version(u32 rf_hw_ver);
char *kunit_chip_version(u32 rf_hw_ver)
{
	return chip_version(rf_hw_ver);
}

static void mxman_reset_chip(struct mxman *mxman);
void kunit_mxman_reset_chip(struct mxman *mxman)
{
	mxman_reset_chip(mxman);
}

static int mxman_start_boot(struct mxman *mxman, enum scsc_subsystem sub);
int kunit_mxman_start_boot(struct mxman *mxman, enum scsc_subsystem sub)
{
	return mxman_start_boot(mxman, sub);
}

static void print_panic_code_legacy(u16 code);
void kunit_print_panic_code_legacy(u16 code)
{
	print_panic_code_legacy(code);
}

static void print_panic_code(struct mxman *mxman);
void kunit_print_panic_code(struct mxman *mxman)
{
	print_panic_code(mxman);
}

static void process_panic_record(struct mxman *mxman, bool dump);
void kunit_process_panic_record(struct mxman *mxman, bool dump)
{
	process_panic_record(mxman, dump);
}

static void mxman_check_promote_syserr(struct mxman *mxman);
void kunit_mxman_check_promote_syserr(struct mxman *mxman)
{
	mxman_check_promote_syserr(mxman);
}

static void mxman_failure_work(struct work_struct *work);
void kunit_mxman_failure_work(struct work_struct *work)
{
	mxman_failure_work(work);
}

static int mxman_logring_register_observer(struct scsc_logring_mx_cb *mx_cb, char *name);
int kunit_mxman_logring_register_observer(struct scsc_logring_mx_cb *mx_cb, char *name)
{
	return mxman_logring_register_observer(mx_cb, name);
}

static int mxman_logring_unregister_observer(struct scsc_logring_mx_cb *mx_cb, char *name);
int kunit_mxman_logring_unregister_observer(struct scsc_logring_mx_cb *mx_cb, char *name)
{
	return mxman_logring_unregister_observer(mx_cb, name);
}

static int mxman_minimoredump_collect(struct scsc_log_collector_client *collect_client, size_t size);
int kunit_mxman_minimoredump_collect(struct scsc_log_collector_client *collect_client, size_t size)
{
	return mxman_minimoredump_collect(collect_client, size);
}

static void failure_wq_init(struct mxman *mxman);
void kunit_failure_wq_init(struct mxman *mxman)
{
	failure_wq_init(mxman);
}

static void failure_wq_stop(struct mxman *mxman);
void kunit_failure_wq_stop(struct mxman *mxman)
{
	failure_wq_stop(mxman);
}

static void failure_wq_deinit(struct mxman *mxman);
void kunit_failure_wq_deinit(struct mxman *mxman)
{
	failure_wq_deinit(mxman);
}

static void failure_wq_start(struct mxman *mxman);
void kunit_failure_wq_start(struct mxman *mxman)
{
	failure_wq_start(mxman);
}

static void syserr_recovery_wq_init(struct mxman *mxman);
void kunit_syserr_recovery_wq_init(struct mxman *mxman)
{
	syserr_recovery_wq_init(mxman);
}

static void syserr_recovery_wq_stop(struct mxman *mxman);
void kunit_syserr_recovery_wq_stop(struct mxman *mxman)
{
	syserr_recovery_wq_stop(mxman);
}

static void syserr_recovery_wq_deinit(struct mxman *mxman);
void kunit_syserr_recovery_wq_deinit(struct mxman *mxman)
{
	syserr_recovery_wq_deinit(mxman);
}

static void syserr_recovery_wq_start(struct mxman *mxman);
void kunit_syserr_recovery_wq_start(struct mxman *mxman)
{
	syserr_recovery_wq_start(mxman);
}

static void mxman_fail_level8(struct mxman *mxman, u16 scsc_panic_code, const char *reason, enum scsc_subsystem sub);
void kunit_mxman_fail_level8(struct mxman *mxman, u16 scsc_panic_code, const char *reason, enum scsc_subsystem sub)
{
	mxman_fail_level8(mxman, scsc_panic_code, reason, sub);
}

