#ifndef __SPRD_SRT_H__
#define __SPRD_SRT_H__



#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG    "<4>SPRD_SRT "


#define SRT_DEBUG (0) /*log switch ,default 0*/
#if SRT_DEBUG
#define DEBUG_PRINT printk
#else
#define DEBUG_PRINT(...)
#endif

struct srtinfo_struct
{
	unsigned int value[5];
};
struct srt_dev {
	struct cdev cdev;
	int dev_major;
	struct class *pClass;
	struct device *pClassDev;
};
#endif
