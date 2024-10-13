#define exynos_s2mpu_backup_subsystem(args...) (0)

static int platform_mif_pmu_reset_release(struct scsc_mif_abs *interface);
int (*fp_platform_mif_pmu_reset_release)(struct scsc_mif_abs *interface) = &platform_mif_pmu_reset_release;

static int platform_mif_start_wait_for_cfg_ack_completion(struct scsc_mif_abs *interface);
int (*fp_platform_mif_start_wait_for_cfg_ack_completion)(struct scsc_mif_abs *interface) = &platform_mif_start_wait_for_cfg_ack_completion;