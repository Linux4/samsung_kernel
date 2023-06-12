#ifndef __RT4832_H__
#define __RT4832_H__

void rt4832_dsv_ctrl(int enable);
void rt4832_dsv_toggle_ctrl(void);

extern unsigned short mt6328_set_register_value(PMU_FLAGS_LIST_ENUM flagname, unsigned int val);

#endif
