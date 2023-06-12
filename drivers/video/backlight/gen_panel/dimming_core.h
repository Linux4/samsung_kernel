#ifndef _DIMMING_CORE_H_
#define _DIMMING_CORE_H_

#define DIMMING_CALC_PRECISE

#ifndef __KERNEL__
#include <stdio.h>
#include <stdlib.h>

#define EINVAL      22
#define pr_warn		printf
#define pr_info		printf
#define pr_err		printf
#define pr_debug		printf
#define unlikely	
#define __func__	__FUNCTION__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;

#define GFP_KERNEL	(0)


static inline u64 do_div(u64 &num, u32 den)
{
	u64 rem = num % den;
	num /= den;
	return rem;
}

static inline s64 do_div(s64 &num, u32 den)
{
	s64 rem = num % den;
	num /= den;
	return rem;
}

#define typeof	typeid
#define kfree		delete
#define kmalloc(size, flags)	malloc((size))
#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) > (b) ? (a) : (b))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#else
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#endif	/* _LINUX_ */

#define BIT_SHIFT	(22)
#define DIMMING_CORE_BIT_SHIFT	(BIT_SHIFT)
#define GRAY_SCALE_MAX	(256)
#define ERROR_DETECT_BOUND	(1)
#define DIMMING_CORE_DYNAMIC_NUM_TP

#if (BIT_SHIFT != 22) && (BIT_SHIFT != 24) && (BIT_SHIFT != 26)
#define GENERATE_GAMMA_CURVE
#endif

#ifdef GENERATE_GAMMA_CURVE
#include <math.h>
#endif	/* GENERATE_GAMMA_CURVE */

enum tag {
	TAG_DIMMING_INFO,
	TAG_MTP_OFFSET_INPUT,
	TAG_MTP_OFFSET_OUTPUT,
	MAX_INPUT_TAG,
};

enum {
	R_OFFSET,
	G_OFFSET,
	B_OFFSET,
	RGB_MAX_OFFSET,
};

enum color {
	RED,
	GREEN,
	BLUE,
	MAX_COLOR,
};

struct coeff_t {
	u32 numerator;
	u32 denominator;
};

enum gamma_degree {
	GAMMA_NONE = 0,
	GAMMA_1_60,
	GAMMA_1_65,
	GAMMA_1_70,
	GAMMA_1_75,
	GAMMA_1_80,
	GAMMA_1_85,
	GAMMA_1_90,
	GAMMA_1_95,
	GAMMA_2_00,
	GAMMA_2_05,
	GAMMA_2_10,
	GAMMA_2_12,
	GAMMA_2_13,
	GAMMA_2_15,
	GAMMA_2_20,
	GAMMA_2_25,
};

struct gamma_curve {
	int gamma;
	int *gamma_curve_table;
};

#ifdef DIMMING_CORE_DYNAMIC_NUM_TP

typedef s32(*s32_TP_COLORs)[MAX_COLOR];
typedef s64(*s64_TP_COLORs)[MAX_COLOR];
typedef u32(*u32_TP_COLORs)[MAX_COLOR];
typedef u64(*u64_TP_COLORs)[MAX_COLOR];

typedef s32(*s32_TPs);
typedef s64(*s64_TPs);
typedef u32(*u32_TPs);
typedef u64(*u64_TPs);

struct gamma_correction {
	/* input data from user setting */
	u32 luminance;
	u32 aor;
	u32 base_luminamce;
	enum gamma_degree g_curve_degree;
	s32 *gray_scale_offset;
	s32 (*rgb_color_offset)[MAX_COLOR];

	/* output(reversed distortion) data will be stored */
	u64 *L;
	u32 *M_GRAY;
	s64 (*mtp_vout)[MAX_COLOR];
	s32 (*mtp_offset)[MAX_COLOR];
	s32 (*mtp_output_from_panel)[MAX_COLOR];
};

struct gray_scale {
	u64 gamma_value;
	s64 vout[MAX_COLOR];
};

struct mtp_data {
	s64 vregout;
	s32 (*mtp_base)[MAX_COLOR];		/* from user setting */
	s32 (*mtp_offset)[MAX_COLOR];	/* from panel */
	s64 (*mtp_vout)[MAX_COLOR];		/* will be calculated */
	enum gamma_degree g_curve_degree;		/* from user setting */
	struct coeff_t *v_coeff;		/* from user setting */
	struct coeff_t vt_coeff[16];			/* from user setting */
};
#else
struct gamma_correction {
	/* input data from user setting */
	u32 luminance;
	u32 aor;
	u32 base_luminamce;
	enum gamma_degree g_curve_degree;
	s32 gray_scale_offset[MAX_VOLT];
	s32 rgb_color_offset[MAX_VOLT][MAX_COLOR];

	/* output(reversed distortion) data will be stored */
	u64 L[MAX_VOLT];
	u32 M_GRAY[MAX_VOLT];
	s64 mtp_vout[MAX_VOLT][MAX_COLOR];
	s32 mtp_offset[MAX_VOLT][MAX_COLOR];
	s32 mtp_output_from_panel[MAX_VOLT][MAX_COLOR];
};

struct gray_scale {
	u64 gamma_value;
	s64 vout[MAX_COLOR];
};

struct mtp_data {
	s64 vregout;
	s32 mtp_base[MAX_VOLT][MAX_COLOR];		/* from user setting */
	s32 mtp_offset[MAX_VOLT][MAX_COLOR];	/* from panel */
	s64 mtp_vout[MAX_VOLT][MAX_COLOR];		/* will be calculated */
	enum gamma_degree g_curve_degree;		/* from user setting */
	struct coeff_t v_coeff[MAX_VOLT];		/* from user setting */
	struct coeff_t vt_coeff[16];			/* from user setting */
};
#endif	/* DIMMING_CORE_DYNAMIC_NUM_TP */

struct dimming_info {
	int nr_tp;				/* number of turning point */
	int *tp;
	char (*tp_names)[10];
	int nr_luminance;		/* number of luminance */
	struct mtp_data mtp;
	struct gray_scale gray_scale_lut[GRAY_SCALE_MAX];
	struct gamma_correction *gamma_correction_lut;
	int target_luminance;
};


#define for_each_tp(pos) \
for ((pos) = 0; (pos) < (sz_tp); (pos)++)

#define for_each_color(pos) \
for ((pos) = 0; (pos) < (sz_tp); (pos)++)

void print_gray_scale_mtp_vout(struct mtp_data *mtp);
void print_gray_scale_vout(void);
int process_mtp(struct dimming_info *);
void print_mtp_offset(struct dimming_info *);
void copy_mtp_output(struct gamma_correction *, u32, u8 *);
int alloc_dimming_info(struct dimming_info *, int, int);

/* gamma curve functions */
void remove_gamma_curve(void);
int find_gamma_curve(int gamma);
enum gamma_degree gamma_value_to_degree(int gamma);
int gamma_curve_value(enum gamma_degree g_curve, int idx);

#endif	/* _DIMMING_CORE_H_ */
