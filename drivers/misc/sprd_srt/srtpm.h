#ifndef _SRTPM_ANDROID_H_
#define _SRTPM_ANDROID_H_

#include <linux/cdev.h>
#include <linux/semaphore.h>

#define SRTPM_DEVICE_NODE_NAME      "SrtPM"
#define SRTPM_DEVICE_FILE_NAME      "SrtPM"
#define SRTPM_DEVICE_PROC_NAME      "SrtPM"
#define SRTPM_DEVICE_CLASS_NAME     "SrtPM"

struct srtpm_android_dev {
	int val;
	struct semaphore sem;
	struct cdev dev;
};

#endif
