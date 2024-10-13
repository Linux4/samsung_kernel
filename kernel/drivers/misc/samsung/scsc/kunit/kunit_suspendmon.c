static int suspendmon_suspend(struct scsc_mif_abs *mif, void *data);
int (*fp_suspendmon_suspend)(struct scsc_mif_abs *mif, void *data) = &suspendmon_suspend;

static void suspendmon_resume(struct scsc_mif_abs *mif, void *data);
void (*fp_suspendmon_resume)(struct scsc_mif_abs *mif, void *data) = &suspendmon_resume;
