static void platform_mif_module_probe_registered_clients(struct scsc_mif_abs *mif_abs);
void (*fp_platform_mif_module_probe_registered_clients)(struct scsc_mif_abs *mif_abs) = &platform_mif_module_probe_registered_clients;

static int platform_mif_module_probe(struct platform_device *pdev);
int (*fp_platform_mif_module_probe)(struct platform_device *pdev) = &platform_mif_module_probe;

static int platform_mif_module_remove(struct platform_device *pdev);
int (*fp_platform_mif_module_remove)(struct platform_device *pdev) = &platform_mif_module_remove;

static int platform_mif_module_suspend(struct device *dev);
int (*fp_platform_mif_module_suspend)(struct device *dev) = &platform_mif_module_suspend;

static int platform_mif_module_resume(struct device *dev);
int (*fp_platform_mif_module_resume)(struct device *dev) = &platform_mif_module_resume;