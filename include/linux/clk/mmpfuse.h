#ifndef __MMP_FUSE_H
#define __MMP_FUSE_H

extern unsigned int get_chipprofile(void);
extern unsigned int get_iddq_105(void);
extern unsigned int get_iddq_130(void);
extern unsigned int get_skusetting(void);
extern unsigned int get_chipfab(void);
extern int get_fuse_suspd_voltage(void); /* unit: mV */

enum fab {
	TSMC = 0,
	SEC,
	FAB_MAX,
};

#endif /* __MMP_FUSE_H */
