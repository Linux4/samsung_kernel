static int logring_collect(struct scsc_log_collector_client *collect_client, size_t size);
int (*fp_logring_collect)(struct scsc_log_collector_client *collect_client, size_t size) = &logring_collect;

static int scsc_reset_all_droplevels_to_set_param_cb(const char *val, const struct kernel_param *kp);
int (*fp_scsc_reset_all_droplevels_to_set_param_cb)(const char *val, const struct kernel_param *kp) = &scsc_reset_all_droplevels_to_set_param_cb;
