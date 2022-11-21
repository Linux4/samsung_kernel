
ssize_t mDNIeTuning_show(struct device *dev, struct device_attribute *attr, char *buf) ;
ssize_t mDNIeTuning_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
ssize_t mDNIeScenario_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t mDNIeScenario_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
ssize_t mDNIeOutdoor_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t mDNIeOutdoor_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
ssize_t mDNIeNegative_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t mDNIeNegative_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
extern int is_poweron;
extern int wakeup_brightness;