/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for inspection functions
 *
 *  Copyright (C) 2022 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_platform.h"
//#include "himax_common.h"
#include "himax_ic_core.h"

/*#define HX_GAP_TEST*/
/*#define HX_INSP_LP_TEST*/
/*#define HX_ACT_IDLE_TEST*/

/*#define HX_INSPT_DBG*/

// +P86801AA1 peiyuexiang.wt,add,20230703,himax openshort
#define HX_RSLT_OUT_PATH "/data/vendor/fac_sources/"
#define HX_RSLT_OUT_FILE "hx_test_result.txt"
// -P86801AA1 peiyuexiang.wt,add,20230703,himax openshort
#define PI(x...) pr_cont(x)
#define HX_SZ_ICID 60

#if defined(HX_EXCP_RECOVERY)
extern u8 HX_EXCP_RESET_ACTIVATE;
#endif

#define BS_RAWDATA     8
#define BS_NOISE       8
#define BS_OPENSHORT   0
#define	BS_LPWUG       1
#define	BS_LP_dile  1
#define	BS_ACT_IDLE    1

/* skip notch & dummy */
#define SKIP_NOTCH_START    5
#define SKIP_NOTCH_END      10
/* TX+SKIP_NOTCH_START */
#define SKIP_DUMMY_START    23
/* TX+SKIP_NOTCH_END*/
#define SKIP_DUMMY_END      28


#define	NOISEFRAME                      60
#define NORMAL_IDLE_RAWDATA_NOISEFRAME  10
#define LP_RAWDATAFRAME              1
#define LP_NOISEFRAME                1
#define LP_IDLE_RAWDATAFRAME         1
#define LP_IDLE_NOISEFRAME           1

#define OTHERSFRAME		2

#define	UNIFMAX			500


/*Himax MP Password*/
#define	PWD_OPEN_START          0x77
#define	PWD_OPEN_END            0x88
#define	PWD_SHORT_START         0x11
#define	PWD_SHORT_END           0x33
#define	PWD_RAWDATA_START       0x00
#define	PWD_RAWDATA_END         0x99
#define	PWD_NOISE_START         0x00
#define	PWD_NOISE_END           0x99
#define	PWD_SORTING_START       0xAA
#define	PWD_SORTING_END         0xCC


#define PWD_ACT_IDLE_START      0x22
#define PWD_ACT_IDLE_END        0x44



#define PWD_LP_START         0x55
#define PWD_LP_END           0x66

#define PWD_LP_IDLE_START    0x50
#define PWD_LP_IDLE_END      0x60

/*Himax DataType*/
#define DATA_SORTING            0x0A
#define DATA_OPEN               0x0B
#define DATA_MICRO_OPEN         0x0C
#define DATA_SHORT              0x0A
#define DATA_RAWDATA            0x0A
#define DATA_NOISE              0x0F
#define DATA_BACK_NORMAL        0x00
#define DATA_LP_RAWDATA      0x0C
#define DATA_LP_NOISE        0x0F
#define DATA_ACT_IDLE_RAWDATA   0x0A
#define DATA_ACT_IDLE_NOISE     0x0F
#define DATA_LP_IDLE_RAWDATA 0x0A
#define DATA_LP_IDLE_NOISE   0x0F

/*Himax Data Ready Password*/
#define	Data_PWD0       0xA5
#define	Data_PWD1       0x5A

/* ASCII format */
#define ASCII_LF        (0x0A)
#define ASCII_CR        (0x0D)
#define ASCII_COMMA     (0x2C)
#define ASCII_ZERO      (0x30)
#define CHAR_EL         '\0'
#define CHAR_NL         '\n'
#define ACSII_SPACE     (0x20)
/* INSOECTION Setting */

void himax_inspection_init(void);
extern int *g_test_item_flag;
extern int HX_CRITERIA_ITEM;
extern int *g_test_item_flag;
extern char *g_himax_inspection_mode[];

/*Need to map *g_himax_inspection_mode[]*/
enum THP_INSPECTION_ENUM {
	HX_OPEN,
	HX_MICRO_OPEN,
	HX_SHORT,
	HX_SC,
	HX_WT_NOISE,
	HX_ABS_NOISE,
	HX_RAWDATA,
	HX_BPN_RAWDATA,
	HX_SORTING,

	HX_GAPTEST_RAW,
	/*HX_GAPTEST_RAW_X,*/
	/*HX_GAPTEST_RAW_Y,*/

	HX_ACT_IDLE_NOISE,
	HX_ACT_IDLE_RAWDATA,
	HX_ACT_IDLE_BPN_RAWDATA,
/*LPWUG test must put after Normal test*/
	HX_LP_WT_NOISE,
	HX_LP_ABS_NOISE,
	HX_LP_RAWDATA,
	HX_LP_BPN_RAWDATA,

	HX_LP_IDLE_NOISE,
	HX_LP_IDLE_RAWDATA,
	HX_LP_IDLE_BPN_RAWDATA,

	HX_BACK_NORMAL,/*Must put in the end*/
};


enum HX_CRITERIA_ENUM {
	IDX_RAWMIN = 0,
	IDX_RAWMAX,
	IDX_BPN_RAWMIN,
	IDX_BPN_RAWMAX,
	IDX_SCMIN,
	IDX_SCMAX,
	IDX_SC_GOLDEN,
	IDX_SHORTMIN,
	IDX_SHORTMAX,
	IDX_OPENMIN,
	IDX_OPENMAX,
	IDX_M_OPENMIN,
	IDX_M_OPENMAX,
	IDX_WT_NOISEMIN,
	IDX_WT_NOISEMAX,
	IDX_ABS_NOISEMIN,
	IDX_ABS_NOISEMAX,
	IDX_SORTMIN,
	IDX_SORTMAX,

	IDX_GAP_HOR_RAWMAX,
	IDX_GAP_HOR_RAWMIN,
	IDX_GAP_VER_RAWMAX,
	IDX_GAP_VER_RAWMIN,

	IDX_ACT_IDLE_NOISE_MIN,
	IDX_ACT_IDLE_NOISE_MAX,
	IDX_ACT_IDLE_RAWDATA_MIN,
	IDX_ACT_IDLE_RAWDATA_MAX,
	IDX_ACT_IDLE_RAW_BPN_MIN,
	IDX_ACT_IDLE_RAW_BPN_MAX,

	IDX_LP_WT_NOISEMIN,
	IDX_LP_WT_NOISEMAX,
	IDX_LP_NOISE_ABS_MIN,
	IDX_LP_NOISE_ABS_MAX,
	IDX_LP_RAWDATA_MIN,
	IDX_LP_RAWDATA_MAX,
	IDX_LP_RAW_BPN_MIN,
	IDX_LP_RAW_BPN_MAX,

	IDX_LP_IDLE_NOISE_MIN,
	IDX_LP_IDLE_NOISE_MAX,
	IDX_LP_IDLE_RAWDATA_MIN,
	IDX_LP_IDLE_RAWDATA_MAX,
	IDX_LP_IDLE_RAW_BPN_MIN,
	IDX_LP_IDLE_RAW_BPN_MAX,
};

enum HX_INSPT_SETTING_IDX {
	RAW_BS_FRAME = 0,
	NOISE_BS_FRAME,
	ACT_IDLE_BS_FRAME,
	LP_BS_FRAME,
	LP_IDLE_BS_FRAME,

	NFRAME,
	IDLE_NFRAME,
	LP_RAW_NFRAME,
	LP_NOISE_NFRAME,
	LP_IDLE_RAW_NFRAME,
	LP_IDLE_NOISE_NFRAME,
};

enum HX_INSP_TEST_ERR_ENUM {
	/* OK */
	HX_INSP_OK = 0,

	/* FAIL */
	HX_INSP_FAIL = 1,

	/* Memory allocate errors */
	HX_INSP_MEMALLCTFAIL = 1 << 1,

	/* Abnormal screen state */
	HX_INSP_ESCREEN = 1 << 2,

	/* Out of specification */
	HX_INSP_ESPEC = 1 << 3,

	/* Criteria file error*/
	HX_INSP_EFILE = 1 << 4,

	/* Switch mode error*/
	HX_INSP_ESWITCHMODE = 1 << 5,

	/* Get raw data errors */
	HX_INSP_EGETRAW = 1 << 6,
};

/* Error code of Inspection */
enum HX_INSP_ITEM_ERR_ENUM {
	/* Sensor open error */
	HX_EOPEN = 1 << HX_OPEN,

	/* Sensor micro open error */
	HX_EMOPEN = 1 << HX_MICRO_OPEN,

	/* Sensor short error */
	HX_ESHORT = 1 << HX_SHORT,

	/* Raw data error */
	HX_ERAW = 1 << HX_RAWDATA,

	/* Raw data BPN error */
	HX_EBPNRAW = 1 << HX_BPN_RAWDATA,

	/* Get SC errors */
	HX_ESC = 1 << HX_SC,

	/* Noise error */
	HX_WT_ENOISE = 1 << HX_WT_NOISE,

	/* Noise error */
	HX_ABS_ENOISE = 1 << HX_ABS_NOISE,

	/* Sorting error*/
	HX_ESORT = 1 << HX_SORTING,

	/* Raw Data GAP  */
	HX_EGAP_RAW = 1 << HX_GAPTEST_RAW,

	/* ACT_IDLE RAW ERROR */
	HX_EACT_IDLE_RAW = 1 << HX_ACT_IDLE_RAWDATA,

	/* ACT_IDLE NOISE ERROR */
	HX_EACT_IDLE_NOISE = 1 << HX_ACT_IDLE_NOISE,

	HX_EACT_IDLE_BPNRAW = 1 << HX_ACT_IDLE_BPN_RAWDATA,

	/* LPWUG RAW ERROR */
	HX_ELP_RAW = 1 << HX_LP_RAWDATA,

	/* LPWUG NOISE ERROR */
	HX_ELP_WT_NOISE = 1 << HX_LP_WT_NOISE,

	/* LPWUG NOISE ERROR */
	HX_ELP_ABS_NOISE = 1 << HX_LP_ABS_NOISE,

	/* LPWUG IDLE RAW ERROR */
	HX_ELP_IDLE_RAW = 1 << HX_LP_IDLE_RAWDATA,

	/* LPWUG IDLE NOISE ERROR */
	HX_ELP_IDLE_NOISE = 1 << HX_LP_IDLE_NOISE,
	HX_ELP_BPNRAW = 1 << HX_LP_BPN_RAWDATA,
	HX_ELP_IDLE_BPNRAW = 1 << HX_LP_IDLE_BPN_RAWDATA,
};

extern void himax_inspect_data_clear(void);

int hx_check_char_val(char input)
{
	int result = NO_ERR;

	if (input >= 'A' && input <= 'Z') {
		result = -1;
		goto END;
	}
	if (input >= 'a' && input <= 'z') {
		result = -1;
		goto END;
	}
	if (input >= '0' && input <= '9') {
		result = 1;
		goto END;
	}
END:
	return result;
}

#ifdef HX_INSPT_DBG
int hx_print_crtra_after_parsing(void)
{
	int i = 0, j = 0;
	int all_mut_len = hx_s_ic_data->tx_num*hx_s_ic_data->rx_num;

	for (i = 0; i < HX_CRITERIA_SIZE; i++) {
		I("Now is %s\n", g_hx_inspt_crtra_name[i]);
		if (g_inspt_crtra_flag[i] == 1) {
			for (j = 0; j < all_mut_len; j++) {
				PI("%d, ", g_inspection_criteria[i][j]);
				if (j % 16 == 15)
					PI("\n");
			}
		} else {
			I("No this Item in this criteria file!\n");
		}
		PI("\n");
	}

	return 0;
}
#endif

/* claculate 10's power function */
int himax_power_cal(int pow, int number)
{
	int i = 0;
	int result = 1;

	for (i = 0; i < pow; i++)
		result *= 10;
	result = result * number;

	return result;

}

/* String to int */
int hiamx_parse_str2int(char *str)
{
	int i = 0;
	int temp_cal = 0;
	int result = -948794;
	unsigned int str_len = strlen(str);
	int negtive_flag = 0;

	for (i = 0; i < str_len; i++) {
		if (i == 0)
			result = 0;
		if (str[i] != '-' && (str[i] > '9' || str[i] < '0')) {
			E("%s: Parsing fail!\n", __func__);
			result = -9487;
			negtive_flag = 0;
			break;
		}
		if (str[i] == '-') {
			negtive_flag = 1;
			continue;
		}
		temp_cal = str[i] - '0';
		result += himax_power_cal(str_len-i-1, temp_cal);
		/* str's the lowest char is the number's the highest number
		 * So we should reverse this number before using the power
		 * function
		 * -1: starting number is from 0 ex:10^0 = 1,10^1=10
		 */
	}

	if (negtive_flag == 1)
		result = 0 - result;

	return result;
}

