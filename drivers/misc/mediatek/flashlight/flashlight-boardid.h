//flashlight-boardid.h

#ifndef _FLASHLIGHTBIDEV_H_
#define _FLASHLIGHTBIDEV_H_

#ifndef FLASHLIGHTBIDEV_MAJOR
#define FLASHLIGHTBIDEV_MAJOR 0  /* Preset main equipment number of mem */   //origin 254
#define FLASHLIGHTBIDEV_MIN 0  /* Preset secondary equipment number of mem */   //origin 254
#endif

#ifndef FLASHLIGHTBIDEV_NR_DEVS
#define FLASHLIGHTBIDEV_NR_DEVS 1    /* Number of equipment */  //origin 2
#endif

#ifndef FLASHLIGHTBIDEV_SIZE
#define FLASHLIGHTBIDEV_SIZE 4096
#endif

#ifndef use_udev
#define use_udev
#endif

#ifndef creat_show_store
#define creat_show_store
#endif

#ifndef proc_sys_creat
#define proc_sys_creat
#endif


extern char* boardid_get(void);
/*flashlightbi Equipment description structure */
struct flashlightbi_dev
{
  char *data;
  unsigned long size;
};

#endif /* _FLASHLIGHTBIDEV_H_ */


