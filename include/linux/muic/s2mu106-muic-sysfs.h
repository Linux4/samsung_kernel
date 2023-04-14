#ifndef __S2MU106_MUIC_SYSFS_H__
#define __S2MU106_MUIC_SYSFS_H__

extern void hv_muic_change_afc_voltage(int tx_data);
int s2mu106_muic_init_sysfs(struct s2mu106_muic_data *muic_data);
void s2mu106_muic_deinit_sysfs(struct s2mu106_muic_data *muic_data);

#endif
