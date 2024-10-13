/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *            http://www.samsung.com
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#ifndef __SGPU_AFM_H__
#define __SGPU_AFM_H__

/* HTU */
#define OCPMCTL                         0x0
#define OCPDEBOUNCER                    0x10
#define OCPINTCTL                       0x20
#define OCPTOPPWRTHRESH                 0x48
#define OCPTHROTTCTL                    0x6c
#define OCPTHROTTCNTA                   0x80
#define OCPTHROTTPROFD                  0x84
#define OCPTHROTTCNTM                   0x118
#define GLOBALTHROTTEN                  0x200
#define OCPFILTER                       0x300

#define OCPMCTL_EN_VALUE                0x1
#define QACTIVEENABLE_CLK_VALUE         (0x1 << 31)
#define HIU1ST                          (0x1 << 5)

#define OCPTHROTTERRA_VALUE             (0x1 << 27)
#define TCRST_MASK                      (0x7 << 21)
#define TCRST_VALUE                     (0x5 << 21)
#define TRW_VALUE                       (0x2 << 3)
#define TEW_VALUE                       (0x0 << 2)

#define TDT_MASK                        (0xfff << 20)
#define TDT_VALUE                       (0x8 << 20)
#define TDC_MASK                        (0xfff << 4)
#define OCPTHROTTERRAEN_VALUE           (0x1 << 1)
#define TDCEN_VALUE                     (0x1 << 0)

#define IRP_MASK                        0x3f
#define IRP_SHIFT                       (24)

#define OCPTHROTTCNTM_EN_VALUE          (0x1 << 31)
#define EN_MASK                         (0x1)

#define RELEASE_MAX_LIMIT		0
#define SET_MAX_LIMIT			1

struct sgpu_afm_domain {
	struct amdgpu_device *adev;
	struct i2c_client               *i2c;
	struct work_struct              afm_work;
	struct delayed_work             release_dwork;
	struct delayed_work             register_dwork;
	/* AFM feature status */
	bool                            enabled;
	bool                            flag;
	/* AFM initial parameter */
	int                             irq;
	void __iomem			*base;
	unsigned long			max_freq;
	unsigned long			min_freq;
	unsigned int                    down_step;
	unsigned int                    release_duration;
	unsigned int                    register_duration;
	unsigned int                    s2mps_afm_offset;
	unsigned int                    interrupt_src;
	/* AFM profile data */
	unsigned int                    throttle_cnt;
	unsigned int                    profile_started;
	unsigned long			clipped_freq;
	unsigned int			total_trans;
	u64				*time_in_state;
	u64				last_time;
	unsigned int			last_level;
	/*GPU PMIC function */
	int (*pmic_read_reg)(struct i2c_client *i2c, u8 reg, u8 *val);
	int (*pmic_update_reg)(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
	int (*pmic_get_i2c)(struct i2c_client **i2c);
};

/* PMIC */
#define MAIN_PMIC			1
#define SUB_PMIC			2
#define EXTRA_PMIC			3

#define S2MPS_AFM_WARN_EN_SHIFT		7
#define S2MPS_AFM_WARN_LVL_MASK		0x1f
#define S2MPS_AFM_WARN_DEFAULT_LVL	0x0
#define S2MPS_AFM_WARN_LOWEST_LVL	S2MPS_AFM_WARN_LVL_MASK

#ifdef CONFIG_DRM_SGPU_AFM
extern int main_pmic_read_reg(struct i2c_client *i2c, u8 reg, u8 *val);
extern int main_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int main_pmic_get_i2c(struct i2c_client **i2c);
#ifdef CONFIG_SOC_S5E8845
extern int sub_pmic_read_reg(struct i2c_client *i2c, u8 reg, u8 *val);
extern int sub_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int sub_pmic_get_i2c(struct i2c_client **i2c);
#else
extern int sub3_pmic_read_reg(struct i2c_client *i2c, u8 reg, u8 *val);
extern int sub3_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int sub3_pmic_get_i2c(struct i2c_client **i2c);
#endif
int sgpu_afm_init(struct amdgpu_device *adev);
int sgpu_afm_get_irq(struct amdgpu_device *adev);
void sgpu_afm_suspend(struct amdgpu_device *adev);
void sgpu_afm_resume(struct amdgpu_device *adev);
#else
static inline int sgpu_afm_init(struct amdgpu_device *adev)
{
	return 0;
}
static inline int sgpu_afm_get_irq(struct amdgpu_device *adev)
{
	return 0;
}
#define sgpu_afm_suspend(adev) do {} while (0)
#define sgpu_afm_resume(adev) do {} while (0)
#endif /* CONFIG_DRM_SGPU_AFM */
#endif /* __SGPU_AFM_H__ */
