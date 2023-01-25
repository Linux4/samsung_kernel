#ifndef ___SENSORS_HEAD_H_INCLUDED
#define ___SENSORS_HEAD_H_INCLUDED
#include <linux/sensors_core.h>

#define PROBE_SUCCESS 1
#define PROBE_FAIL	  0
extern unsigned int board_hw_revision;


int hscd_get_magnetic_field_data(int *xyz);
void hscd_activate(int flgatm, int flg, int dtime);
int hscd_open(void);

void bma222_activate(int flgatm, int flg, int dtime);
int bma222_get_acceleration_data(int *xyz);
int bma222_open(void);

void bma222e_activate(int flgatm, int flg, int dtime);
int bma222e_get_acceleration_data(int *xyz);
int bma222e_open(void);

void bma254_activate(int flgatm, int flg, int dtime);
int bma254_get_acceleration_data(int *xyz);
int bma254_open(void);

void k3dh_activate(int flgatm, int flg, int dtime);
int k3dh_get_acceleration_data(int *xyz);
int k3dh_io_open(void);

int hscd_self_test_A(void);
int hscd_self_test_B(void);
#endif

