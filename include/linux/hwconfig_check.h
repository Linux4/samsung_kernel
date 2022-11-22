#ifndef __HWCONFIG_CHECK_H
#define __HWCONFIG_CHECK_H

bool get_hwconfig_sleep_flag(void);
bool get_hwconfig_resume_flag(void);
int hwconfig_check_sleep_conti_reg(void);
int hwconfig_check_resume_conti_reg(void);
int hwconfig_check_sleep_sep_reg(void);
int hwconfig_check_resume_sep_reg(void);
int hwconfig_check_sleep_regu_volt(void);
int hwconfig_check_resume_regu_volt(void);
#endif/*__HWCONFIG_CHECK_H*/
