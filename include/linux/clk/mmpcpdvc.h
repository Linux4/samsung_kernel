#ifndef __MMP_CPDVC_H
#define __MMP_CPDVC_H


#define MAX_CP_PPNUM 4

struct cpdvc_info {
	unsigned int cpfreq; /* Mhz */
	unsigned int cpvl; /* range from 0~7/0~15 */
};

struct cpmsa_dvc_info {
	struct cpdvc_info cpdvcinfo[MAX_CP_PPNUM];  /* we only use four CP PPs now as max */
	unsigned int msadvcvl; /* we only have one msa PP */
};

extern int fillcpdvcinfo(struct cpmsa_dvc_info *dvc_info);
extern int getcpdvcinfo(struct cpmsa_dvc_info *dvc_info);

#endif /* __MMP_SDH_TUNING_H */
