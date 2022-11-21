int sensorhub_device_probe(struct platform_device *pdev);
void sensorhub_device_shutdown(struct platform_device *pdev);
int sensorhub_device_prepare(struct device *dev);
void sensorhub_device_complete(struct device *dev);