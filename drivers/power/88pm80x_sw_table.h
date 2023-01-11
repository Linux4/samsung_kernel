#ifndef __88PM80X_TABLE_H
#define __88PM80X_TABLE_H

/* -25C ~ 60C: multipled by 10000 */
extern const int temperature_table[];
extern const int ocv_dischg[];
extern const int ocv_chg[];
/*
 * The rtot(Rtotal) of battery is much different in different temperature,
 * so we introduced data in different temperature, the typical sample point
 * of temperature are -5/10/25/40 C.
 * For charging case, we support the data of charging current of 1200 mA
 * in those temperatures, so we have 4 table for it.
 * For discharging case, we have data 500/1000/1500 mA discharging current
 * case, then we have 12 tables for it.
 */
extern const int rtot_tm20_i0p1[];
extern const int rtot_tm20_i0p3[];
extern const int rtot_tm20_i0p5[];
extern const int rtot_tm20_i0p7[];
extern const int rtot_tm20_i1p0[];
extern const int rtot_tm20_i1p5[];
extern const int rtot_tm5_i0p1[];
extern const int rtot_tm5_i0p3[];
extern const int rtot_tm5_i0p5[];
extern const int rtot_tm5_i0p7[];
extern const int rtot_tm5_i1p0[];
extern const int rtot_tm5_i1p5[];
extern const int rtot_tm5_i0p13c[];
extern const int rtot_tm5_i0p3c[];
extern const int rtot_tm5_i0p5c[];
extern const int rtot_tm5_i0p7c[];
extern const int rtot_tm5_i1p0c[];
extern const int rtot_t10_i0p1[];
extern const int rtot_t10_i0p3[];
extern const int rtot_t10_i0p5[];
extern const int rtot_t10_i0p7[];
extern const int rtot_t10_i1p0[];
extern const int rtot_t10_i1p5[];
extern const int rtot_t10_i0p13c[];
extern const int rtot_t10_i0p3c[];
extern const int rtot_t10_i0p5c[];
extern const int rtot_t10_i0p7c[];
extern const int rtot_t10_i1p0c[];
extern const int rtot_t25_i0p1[];
extern const int rtot_t25_i0p3[];
extern const int rtot_t25_i0p5[];
extern const int rtot_t25_i0p7[];
extern const int rtot_t25_i1p0[];
extern const int rtot_t25_i1p5[];
extern const int rtot_t25_i0p13c[];
extern const int rtot_t25_i0p3c[];
extern const int rtot_t25_i0p5c[];
extern const int rtot_t25_i0p7c[];
extern const int rtot_t25_i1p0c[];
extern const int rtot_t40_i0p1[];
extern const int rtot_t40_i0p3[];
extern const int rtot_t40_i0p5[];
extern const int rtot_t40_i0p7[];
extern const int rtot_t40_i1p0[];
extern const int rtot_t40_i1p5[];
extern const int rtot_t40_i0p13c[];
extern const int rtot_t40_i0p3c[];
extern const int rtot_t40_i0p5c[];
extern const int rtot_t40_i0p7c[];
extern const int rtot_t40_i1p0c[];
extern const int *dis_chg_rtot[6][5];
extern const int dischg_ib[6];
extern const int *chg_rtot[5][4];
extern const int chg_ib[5];
#endif
