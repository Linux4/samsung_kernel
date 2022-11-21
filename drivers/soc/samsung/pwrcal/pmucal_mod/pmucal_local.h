#ifndef __PMUCAL_LOCAL_H__
#define __PMUCAL_LOCAL_H__
#include "pmucal_common.h"

/* In Exynos, the number of MAX_POWER_DOMAIN is less than 15 */
#define PMUCAL_MAX_PARENT_PD		15

/* will be a member of struct exynos_pm_domain */
struct pmucal_pd {
	u32 id;
	char *name;
	struct pmucal_seq *on;
	struct pmucal_seq *save;
	struct pmucal_seq *off;
	struct pmucal_seq *status;
	u32 num_on;
	u32 num_save;
	u32 num_off;
	u32 num_status;
};

/* Chip-independent enumeration for local power domains
 * All power domains should be described in here.
 */
enum pmucal_local_pdnum {
	PD_G3D = 0,
	PD_CAM0,
	PD_CAM1,
	PD_ISP,
	PD_ISP0,
	PD_ISP1,
	PD_MFC,
	PD_MSCL,
	PD_MFCMSCL,
	PD_AUD,
	PD_DISP,
	PD_DISP0,
	PD_DISP1,
	PD_DISPAUD,
	PMUCAL_NUM_PDS,
};

/* APIs to be supported to PWRCAL interface */
extern int pmucal_local_enable(unsigned int pd_id);
extern int pmucal_local_disable(unsigned int pd_id);
extern int pmucal_local_is_enabled(unsigned int pd_id);
extern int pmucal_local_init(void);

#endif
