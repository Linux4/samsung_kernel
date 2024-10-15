#define sprintf(args...) (0)
#define cpufreq_cpu_get(args...)	((void*)0)
#define cpufreq_cpu_out(args...)	(0)

static int platform_mif_set_affinity_cpu(struct scsc_mif_abs *interface, u8 cpu);
int (*fp_platform_mif_set_affinity_cpu)(struct scsc_mif_abs *interface, u8 cpu) = &platform_mif_set_affinity_cpu;

static int platform_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config);
int (*fp_platform_mif_pm_qos_add_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config) = &platform_mif_pm_qos_add_request;

static int platform_mif_pm_qos_update_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config);
int (*fp_platform_mif_pm_qos_update_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config) = &platform_mif_pm_qos_update_request;

static int platform_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req);
int (*fp_platform_mif_pm_qos_remove_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req) = &platform_mif_pm_qos_remove_request;
