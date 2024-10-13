static int sable_collection_off_set_param_cb(const char *val, const struct kernel_param *kp);
int (*fp_sable_collection_off_set_param_cb)(const char *val, const struct kernel_param *kp) = &sable_collection_off_set_param_cb;

static int sable_collection_off_get_param_cb(char *buffer, const struct kernel_param *kp);
int (*fp_sable_collection_off_get_param_cb)(const char *val, const struct kernel_param *kp) = &sable_collection_off_get_param_cb;

static int scsc_log_collector_collect(enum scsc_log_reason reason, u16 reason_code);
int (*fp_scsc_log_collector_collect)(enum scsc_log_reason reason, u16 reason_code) = &scsc_log_collector_collect;
