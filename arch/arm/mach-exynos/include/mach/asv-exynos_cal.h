#ifndef _ASV_EXYNOS_CAL_H_
#define _ASV_EXYNOS_CAL_H_

#include <linux/io.h>
#include <mach/asv-exynos.h>

u32 re_err(void);

#define Assert(b) (!(b) ? re_err() : 0)

#if defined(CONFIG_SOC_EXYNOS3475)
enum SYSC_DVFS_SEL {
	SYSC_DVFS_CL1,
	SYSC_DVFS_CL0,
	SYSC_DVFS_INT,
	SYSC_DVFS_MIF,
	SYSC_DVFS_G3D,
	SYSC_DVFS_CAM,
	SYSC_DVFS_NUM
};
#endif

typedef unsigned long long      u64;
typedef unsigned int            u32;
typedef unsigned short          u16;
typedef unsigned char           u8;
typedef signed long long        s64;
typedef signed int              s32;
typedef signed short            s16;
typedef signed char             s8;

//#define __raw_writel(addr, data) (*(volatile u32 *)(addr) = (data))
//#define __raw_readl(addr) (*(volatile u32 *)(addr))
#define __getbits(addr, base, mask) ((__raw_readl(addr) >> (base)) & (mask))

void cal_init(void);
u32 cal_get_max_volt(u32 id);
s32 cal_get_min_lv(u32 id);
u32 cal_get_freq(u32 id, s32 level);
u32 cal_get_asv_grp(u32 id, s32 level);
u32 cal_get_volt(u32 id, s32 level);
u32 cal_get_abb(u32 id, s32 level);
u32 cal_get_ssa_volt(u32 id);
u32 cal_get_boost_volt(u32 id, s32 level);
bool cal_get_use_abb(u32 id);
void cal_set_abb(u32 id, u32 abb);

bool cal_use_dynimic_abb(u32 id);
bool cal_use_dynimic_ema(u32 id);
u32 cal_set_ema(u32 id, u32 volt);

#ifdef CONFIG_SOC_EXYNOS7420
u32 cal_get_asv_info(int id);
#else
extern inline u32 cal_get_asv_info(int id){return 0;};
extern u32 get_asv_ids(void);
extern u32 get_asv_hpm(void);
extern u32 get_asv_ro(void);
#endif

s32 cal_get_max_lv(u32 id);
s32 cal_get_min_lv(u32 id);
u32 cal_get_table_ver(void);

#endif

