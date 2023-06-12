#include "dimming_core.h"
#include "dimming_gamma_curve.h"

#define DEBUG_AID_DIMMING

int target_luminance;
static struct gray_scale gray_scale_lut[GRAY_SCALE_MAX];
struct gamma_correction *gamma_correction_lut;
struct mtp_data *mtp;

static int NR_LUMINANCE;
static int NR_TP;
static int TP_VT = 0;
static int TP_2ND = -1;
static int TP_3RD = -1;
static int TP_V255 = -1;

static int *calc_gray_order;
static int *calc_vout_order;

char(*volt_name)[10];
int *ivolt;

static char *tag_name[] = {
	"[DIMMING_INFO]",
	"[MTP_OFFSET_INPUT]",
	"[MTP_OFFSET_OUTPUT]"
};

static char *color_name[] = {
	"RED", "GREEN", "BLUE",
};

struct gamma_curve gamma_curve_lut[] = {
#ifndef GENERATE_GAMMA_CURVE
	{ 0, NULL },
	{ 160, gamma_curve_1p60 },
	{ 165, gamma_curve_1p65 },
	{ 170, gamma_curve_1p70 },
	{ 175, gamma_curve_1p75 },
	{ 180, gamma_curve_1p80 },
	{ 185, gamma_curve_1p85 },
	{ 190, gamma_curve_1p90 },
	{ 195, gamma_curve_1p95 },
	{ 200, gamma_curve_2p00 },
	{ 205, gamma_curve_2p05 },
	{ 210, gamma_curve_2p10 },
	{ 212, gamma_curve_2p12 },
	{ 213, gamma_curve_2p13 },
	{ 215, gamma_curve_2p15 },
	{ 220, gamma_curve_2p20 },
	{ 225, gamma_curve_2p25 },
#else
	{ 0, NULL },
	{ 160, NULL },
	{ 165, NULL },
	{ 170, NULL },
	{ 175, NULL },
	{ 180, NULL },
	{ 185, NULL },
	{ 190, NULL },
	{ 195, NULL },
	{ 200, NULL },
	{ 205, NULL },
	{ 210, NULL },
	{ 212, NULL },
	{ 213, NULL },
	{ 215, NULL },
	{ 220, NULL },
	{ 225, NULL },
#endif	/* GENERATE_GAMMA_CURVE */
};

static s64 disp_pow(s64 num, u32 digits)
{
	s64 res = num;
	u32 i;

	if (digits == 0)
		return 1;

	for (i = 1; i < digits; i++)
		res *= num;
	return res;
}

static s64 disp_round(s64 num, u32 digits)
{
	int sign = (num < 0 ? -1 : 1);

	num *= sign;
	num *= disp_pow(10, digits + 1);
	num += 5;
	do_div(num, (u32)disp_pow(10, digits + 1));

	return sign * num;
}

/*
static s64 scale_up(s64 num) {
	return (num * (1LL << BIT_SHIFT));
}
static s64 scale_down(s64 num) {
	return (num / (1LL << BIT_SHIFT));
}
*/

static s64 scale_down_round(s64 num, u32 digits) {
	int sign = (num < 0 ? -1 : 1);
	u64 rem;

	num *= sign;
	rem = do_div(num, (1U << BIT_SHIFT));

	rem *= disp_pow(10, digits + 1);
	rem >>= BIT_SHIFT;
	rem += 5;
	do_div(rem, (u32)disp_pow(10, digits + 1));

	return sign * (num + rem);
}

static s64 scale_down_rem(s64 num, u32 digits)
{
	int sign = (num < 0 ? -1 : 1);
	u64 rem;

	num *= sign;
	rem = do_div(num, (1U << BIT_SHIFT));

	rem *= disp_pow(10, digits + 1);
	rem >>= BIT_SHIFT;
	rem += 5;
	do_div(rem, 10);

	/* round up and minus in remainder */
	if (rem >= (u64)disp_pow(10, digits))
		rem -= (u64)disp_pow(10, digits);

	return sign * rem;
}

#if BIT_SHIFT > 26
static int msb64(s64 num)
{
	int i;
	for (i = 63; i >= 0; i--)
	if (num & (1ULL << i))
		return i;

	return 0;
}
#endif

static s64 disp_div64(s64 num, s64 den)
{
#if BIT_SHIFT > 26
	if (num > ((1LL << (63 - BIT_SHIFT)) - 1)) {
		int bit_shift = 62 - msb64(num);

		pr_err("out of range num %lld\n", num);
		return ((num << bit_shift) / den) >> bit_shift;
	}
#endif

	int sign_num = (num < 0 ? -1 : 1);
	int sign_den = (den < 0 ? -1 : 1);
	int sign = sign_num * sign_den;

	num *= sign_num;
	den *= sign_den;

	if (den >= (1U << 31))
		pr_err("%s, out of range denominator %lld\n", __func__, den);

	do_div(num, (u32)den);

	return sign * num;
}

#ifdef DIMMING_CALC_PRECISE
static s64 disp_div64_round(s64 num, s64 den, u32 digits)
{
	int sign_num = (num < 0 ? -1 : 1);
	int sign_den = (den < 0 ? -1 : 1);
	int sign = sign_num * sign_den;
	s64 rem;

	num *= sign_num;
	den *= sign_den;

	rem = do_div(num, (u32)den);
	rem *= disp_pow(10, digits + 2);
	do_div(rem, (u32)den);
	rem += 6;
	do_div(rem, (u32)disp_pow(10, digits + 2));
	
	return sign * (num + rem);
}
#endif


/*
* generate_order() is an helper function can make an order
* that mixed an initial order value and range based order values.
* @s : initial value
* @from - starting of the range
* @to - end of the range including 'to'.
* @return - an allocated array will be set as like { s, [from, to] }.
*/
static int *generate_order(int s, int from, int to) {
	int i, len = abs(from - to) + 2;
	int sign = ((to - from) < 0) ? -1 : 1;
	int *t_order = (int *)kmalloc(len * sizeof(int), GFP_KERNEL);

	t_order[0] = s;
	t_order[1] = from;
	for (i = 2; i < len; i++)
		t_order[i] = t_order[i - 1] + sign;

	return t_order;
}

#ifdef GENERATE_GAMMA_CURVE
static void generate_gamma_curve(int gamma, int *gamma_curve)
{
	double gamma_f = (double)gamma / 100;

	pr_info("%s, generate %d gamma\n", __func__, gamma);
	for (int i = 0; i < GRAY_SCALE_MAX; i++) {
		gamma_curve[i] = (int)(pow(((double)i / 255), gamma_f) * (double)(1 << BIT_SHIFT) + 0.5f);
		pr_info("%d ", gamma_curve[i]);
		if (!((i + 1) % 8))
			pr_info("\n");
	}
}

void remove_gamma_curve(void)
{
	int index;

	for (index = 0; index < ARRAY_SIZE(gamma_curve_lut); index++)
	if (gamma_curve_lut[index].gamma_curve_table) {
		free(gamma_curve_lut[index].gamma_curve_table);
		gamma_curve_lut[index].gamma_curve_table = NULL;
	}
}
#endif	/* GENERATE_GAMMA_CURVE */

int find_gamma_curve(int gamma)
{
	int index;
	for (index = 0; index < ARRAY_SIZE(gamma_curve_lut); index++)
	if (gamma_curve_lut[index].gamma == gamma)
		break;

	if (index == ARRAY_SIZE(gamma_curve_lut)) {
		pr_err("%s, gamma %d curve not found\n", __func__, gamma);
		return -1;
	}

#ifdef GENERATE_GAMMA_CURVE
	if (!gamma_curve_lut[index].gamma_curve_table) {
		pr_info("%s, generate gamma curve %d\n", __func__, gamma);
		int *gamma_curve_table = (int *)kmalloc(GRAY_SCALE_MAX * sizeof(int), GFP_KERNEL);
		generate_gamma_curve(gamma, gamma_curve_table);
		gamma_curve_lut[index].gamma_curve_table = gamma_curve_table;
	}
#endif

	return index;
}

enum gamma_degree gamma_value_to_degree(int gamma)
{
	int g_curve_index;
	g_curve_index = find_gamma_curve(gamma);
	if (g_curve_index < 0) {
		pr_err("%s, gamma %d not exist\n", __func__, gamma);
		return GAMMA_NONE;
	}

	return (enum gamma_degree)g_curve_index;
}

/*
* @ g_curve : gamma curve
* @ idx : gray scale level 0 ~ 255.
* @ return ((idx/255)^g_curve)*(2^22)
*/
int gamma_curve_value(enum gamma_degree g_curve, int idx)
{
	int gamma;
	if (g_curve == GAMMA_NONE ||
		idx >= GRAY_SCALE_MAX)
		return -1;

	gamma = gamma_curve_lut[(int)g_curve].gamma;

#ifdef GENERATE_GAMMA_CURVE
	if (!gamma_curve_lut[(int)g_curve].gamma_curve_table) {
		pr_info("%s, generate gamma curve %d\n", __func__, gamma);
		int *gamma_curve_table = (int *)kmalloc(GRAY_SCALE_MAX * sizeof(int), GFP_KERNEL);
		generate_gamma_curve(gamma, gamma_curve_table);
		gamma_curve_lut[(int)g_curve].gamma_curve_table = gamma_curve_table;
	}
#endif

	return gamma_curve_lut[(int)g_curve].gamma_curve_table[idx];
}

void print_mtp_offset(struct dimming_info *dim_info)
{
	int i;

	pr_info("%s\n", tag_name[TAG_MTP_OFFSET_INPUT]);
	for (i = 0; i < dim_info->nr_tp; i++) {
		pr_info("%-4d %-4d %-4d\n",
				dim_info->mtp.mtp_offset[i][RED],
				dim_info->mtp.mtp_offset[i][GREEN],
				dim_info->mtp.mtp_offset[i][BLUE]);
	}
	pr_info("\n");
}

void print_mtp_output(struct gamma_correction *lut)
{
	int i, ilum;
	char linebuf[256], *p;

	if (NR_TP * 15 >= ARRAY_SIZE(linebuf)) {
		pr_err("%s, exceed linebuf size (%d)\n",
				__func__, 15 * NR_TP);
		return;
	}
	pr_info("%s\n", tag_name[TAG_MTP_OFFSET_OUTPUT]);
	for (ilum = 0; ilum < NR_LUMINANCE; ilum++) {
		p = linebuf;
		for (i = 1; i < NR_TP; i++) {
			p += sprintf(p, "%-3d %-3d %-3d ",
					lut[ilum].mtp_offset[i][RED],
					lut[ilum].mtp_offset[i][GREEN],
					lut[ilum].mtp_offset[i][BLUE]);
		}
		pr_info("%s\n", linebuf);
	}
	pr_info("\n");
}

void print_gray_scale_mtp_vout(struct mtp_data *mtp)
{
	int i;

	pr_info("MTP REFERENCE VRETOUT\n");
	for (i = 0; i < NR_TP; i++)
		pr_info("%2lld.%04lld  %2lld.%04lld  %2lld.%04lld\n",
				scale_down_round(mtp->mtp_vout[i][RED], 4), scale_down_rem(mtp->mtp_vout[i][RED], 4),
				scale_down_round(mtp->mtp_vout[i][GREEN], 4), scale_down_rem(mtp->mtp_vout[i][GREEN], 4),
				scale_down_round(mtp->mtp_vout[i][BLUE], 4), scale_down_rem(mtp->mtp_vout[i][BLUE], 4));
}

void print_gray_scale_vout()
{
	int i;

	pr_info("GRAY SCALE OUTPUT VOLTAGE\n");
	for (i = 0; i < GRAY_SCALE_MAX; i++) {
		pr_info("%2lld.%04lld  %2lld.%04lld  %2lld.%04lld\n",
				scale_down_round(gray_scale_lut[i].vout[RED], 4), scale_down_rem(gray_scale_lut[i].vout[RED], 4),
				scale_down_round(gray_scale_lut[i].vout[GREEN], 4), scale_down_rem(gray_scale_lut[i].vout[GREEN], 4),
				scale_down_round(gray_scale_lut[i].vout[BLUE], 4), scale_down_rem(gray_scale_lut[i].vout[BLUE], 4));
	}
}

u64 mtp_offset_to_vregout(int v, enum color c)
{
	s64 num, den, vreg, res = 0;

	if (v < 0 || v >= NR_TP) {
		pr_err("%s, invalid tp %d\n", __func__, v);
		return -EINVAL;
	}

	den = (s64)mtp->v_coeff[v].denominator;

	if (v == TP_VT) {
		s32 offset = mtp->mtp_base[v][c] + mtp->mtp_offset[v][c];
		if (offset > 0xF) {
			pr_err("%s, error : ivt (%d) out of range(0x0 ~ 0xF)\n", __func__, offset);
			return -EINVAL;
		}
		vreg = mtp->vregout;
		num = vreg * (s64)mtp->vt_coeff[offset].numerator;
		res = mtp->vregout - disp_div64(num, den);
	}
	else if (v == TP_V255) {
		vreg = mtp->vregout;
		num = vreg * ((s64)mtp->v_coeff[v].numerator + mtp->mtp_base[v][c] + mtp->mtp_offset[v][c]);
		res = mtp->vregout - disp_div64(num, den);
	}
	else if (v == TP_2ND) {
		vreg = mtp->vregout - mtp->mtp_vout[v + 1][c];
		num = vreg * ((s64)mtp->v_coeff[v].numerator + mtp->mtp_base[v][c] + mtp->mtp_offset[v][c]);
		res = mtp->vregout - disp_div64(num, den);
	}
	else {
		vreg = mtp->mtp_vout[TP_VT][c] - mtp->mtp_vout[v + 1][c];
		num = vreg * ((s64)mtp->v_coeff[v].numerator + mtp->mtp_base[v][c] + mtp->mtp_offset[v][c]);
		res = mtp->mtp_vout[TP_VT][c] - disp_div64(num, den);
	}

	if (res < 0) {
		pr_debug("%s, %s %s vreg %lld, num %lld, den %lld, res %lld\n",
			__func__, volt_name[(int)v], color_name[(int)c], vreg, num, den, res);
		pr_err("%s, res %lld should not be minus value\n", __func__, res);
	}
	return res;
}

s32 mtp_vregout_to_offset(int v, enum color c, int luminance_index)
{
	s64 num, den, res;
	u32 upper = ((v == TP_V255) ? 511 : 255);
	s64(*mtp_vout)[MAX_COLOR];
	s32(*rgb_offset)[MAX_COLOR];

	if (v < 0 || v >= NR_TP) {
		pr_err("%s, invalid tp %d\n", __func__, v);
		return -EINVAL;
	}

	if (luminance_index < 0 || luminance_index >= NR_LUMINANCE) {
		pr_err("%s, out of range luminance index (%d)\n", __func__, luminance_index);
		return 0;
	}

	mtp_vout = gamma_correction_lut[luminance_index].mtp_vout;
	rgb_offset = gamma_correction_lut[luminance_index].rgb_color_offset;

	if (v == TP_V255) {
		num = (mtp->vregout - mtp_vout[v][c]) * (s64)mtp->v_coeff[v].denominator;
		den = mtp->vregout;
	}
	else if (v == TP_2ND) {
		num = (mtp->vregout - mtp_vout[v][c]) * (s64)mtp->v_coeff[v].denominator;
		den = mtp->vregout - mtp_vout[v + 1][c];
	}
	else {
		num = (mtp_vout[TP_VT][c] - mtp_vout[v][c]) * (s64)mtp->v_coeff[v].denominator;
		den = mtp_vout[TP_VT][c] - mtp_vout[v + 1][c];
	}

#ifdef DIMMING_CALC_PRECISE
	/* To get precise value, use disp_div64_round() function */
	num -= den * (mtp->v_coeff[v].numerator);
	num = disp_round(num, 7) - den * (s64)mtp->mtp_offset[v][c];
	res = disp_div64_round(num, den, 2);
	//pr_info("lum %d, %s %s  num %lld, den %lld, res %lld\n", gamma_correction_lut[luminance_index].luminance, volt_name[v], color_name[c], num, den, res);
#else
	num -= den * ((s64)mtp->v_coeff[v].numerator + (s64)mtp->mtp_offset[v][c]);
	res = disp_div64(num, den);
#endif
	if (v >= TP_3RD)
		res += rgb_offset[v][c];

	/*
	pr_debug("luminance %3d %5s %5s num %lld\t den %lld\t rgb_offset %d\t res %lld\n",
	gamma_correction_lut[luminance_index].luminance, volt_name[v], color_name[c], num, den, rgb_offset[v][c], res);
	*/

	if (res < 0)
		res = 0;
	if (res > upper)
		res = upper;

	return (s32)res;
}

/*
* ########### BASE LUMINANCE GRAY SCALE TABLE FUNCTIONS ###########
*/

/*
* sams as excel macro function min_diff_gray().
*/
static int find_gray_scale_gamma_value(struct gray_scale *gray_scale_lut, u64 gamma_value)
{
	int l = 0, m, r = GRAY_SCALE_MAX - 1;
	s64 delta_l, delta_r;
	if (unlikely(!gray_scale_lut)) {
		pr_err("%s, gray_scale_lut not exist\n", __func__);
		return -EINVAL;
	}

	if ((gray_scale_lut[l].gamma_value > gamma_value) ||
		(gray_scale_lut[r].gamma_value < gamma_value)) {
		pr_err("%s, out of range([l:%d, r:%d], [%lld, %lld]) candela(%lld)\n",
			__func__, l, r, gray_scale_lut[l].gamma_value,
			gray_scale_lut[r].gamma_value, gamma_value);
		return -EINVAL;
	}

	while (l <= r) {
		m = l + (r - l) / 2;
		if (gray_scale_lut[m].gamma_value == gamma_value)
			return m;
		if (gray_scale_lut[m].gamma_value < gamma_value)
			l = m + 1;
		else
			r = m - 1;
	}
	delta_l = gamma_value - gray_scale_lut[r].gamma_value;
	delta_r = gray_scale_lut[l].gamma_value - gamma_value;

	return delta_l <= delta_r ? r : l;
}

static s64 interpolate(s64 from, s64 to, int cur_step, int total_step)
{
	s64 num = (from - to) * (s64)cur_step;
	s64 den = (s64)total_step;
	num = to + disp_div64(num, den);

	return num;
}

static int generate_gray_scale(struct gray_scale *gray_scale_lut)
{
	int i, c, iv = 0, iv_upper = 0, iv_lower = 0, ret = 0, cur_step, total_step = 0;
	s64 vout, v_upper, v_lower;

	for (i = 0; i < GRAY_SCALE_MAX; i++) {
		if (i == ivolt[iv]) {
			iv++;
			iv_upper = ivolt[iv];
			iv_lower = ivolt[iv - 1];
			total_step = iv_upper - iv_lower;
			continue;
		}

		cur_step = total_step - i + iv_lower;
		for (c = 0; c < MAX_COLOR; c++) {
			v_upper = (s64)gray_scale_lut[iv_upper].vout[c];
			v_lower = (s64)gray_scale_lut[iv_lower].vout[c];
			vout = interpolate(v_lower, v_upper, cur_step, total_step);

			pr_debug("lower %3d, upper %3d, cur_step %3d, total_step %3d, vout[%d]\t from %2lld.%04lld, vout %2lld.%04lld, to %2lld.%04lld\n",
					iv_lower, iv_upper, cur_step, total_step, c,
					scale_down_round(v_lower, 4), scale_down_rem(v_lower, 4),
					scale_down_round(vout, 4), scale_down_rem(vout, 4),
					scale_down_round(v_upper, 4), scale_down_rem(v_upper, 4));

			if (vout < 0) {
				pr_warn("%s, from %2lld.%4lld, to %2lld.%4lld (idx %-3d %-3d), cur_steps %-3d, total_steps %-3d\n",
					__func__, scale_down_round(v_lower, 4), scale_down_rem(v_lower, 4),
					scale_down_round(v_upper, 4), scale_down_rem(v_upper, 4),
					iv_lower, iv_upper, cur_step, total_step);
				ret = -EINVAL;
			}
			gray_scale_lut[i].vout[c] = vout;
		}
	}

	for (i = 0; i < GRAY_SCALE_MAX; i++) {
		/* base luminance gamma curve value */
		gray_scale_lut[i].gamma_value = target_luminance * (u64)gamma_curve_value(mtp->g_curve_degree, i);
		pr_debug("%-4d gamma %3lld.%02lld\n", i, scale_down_round(gray_scale_lut[i].gamma_value, 2), scale_down_rem(gray_scale_lut[i].gamma_value, 2));
	}
#ifdef DEBUG_AID_DIMMING
	print_gray_scale_vout();
#endif

	return ret;
}

/*
* ########### GAMMA CORRECTION TABLE FUNCTIONS ###########
*/
int find_luminance(struct gamma_correction *gamma_correction_lut, u32 luminance)
{
	int index;
	for (index = 0; index < NR_LUMINANCE; index++)
	if (gamma_correction_lut[index].luminance == luminance)
		return index;

	return -1;
}

void copy_mtp_output(struct gamma_correction *gamma_correction_lut, u32 luminance, u8 *output)
{
	int i, c, idst = 0;
	int ilum = find_luminance(gamma_correction_lut, luminance);

	for (i = TP_V255; i >= TP_VT; i--) {
		for (c = 0; c < MAX_COLOR; c++) {
			if (i == TP_V255) {
				output[idst++] = ((gamma_correction_lut[ilum].mtp_offset[i][c] & 0x0100) ? 1 : 0);
				output[idst++] = gamma_correction_lut[ilum].mtp_offset[i][c] & 0x00FF;
				if (gamma_correction_lut[ilum].mtp_offset[i][c] > 511) {
					pr_err("%s, error : exceed output range %s[%s] %d !!\n", __func__,
							volt_name[i], color_name[c],
							gamma_correction_lut[ilum].mtp_offset[i][c]);
				}
			} else {
				output[idst++] = gamma_correction_lut[ilum].mtp_offset[i][c] & 0x00FF;
				if (gamma_correction_lut[ilum].mtp_offset[i][c] > 255) {
					pr_err("%s, error : exceed output range %s[%s] %d !!\n", __func__,
							volt_name[i], color_name[c],
							gamma_correction_lut[ilum].mtp_offset[i][c]);
				}
			}
		}
	}

	return;
}

static void print_mtp_output_info(struct gamma_correction *gamma_correction_lut)
{
	int v;
	u32 luminance = gamma_correction_lut->luminance;
	u64 *L = gamma_correction_lut->L;
	u32 *M_GRAY = gamma_correction_lut->M_GRAY;

	for (v = 0; v < NR_TP; v++) {
		pr_info("luminance %3d %5s L %3lld.%05lld\t M_GRAY %3u R %3lld.%05lld\t G %3lld.%05lld\t B %3lld.%05lld\t %3X %3X %3X \n", luminance, volt_name[v],
			scale_down_round(L[v], 5), scale_down_rem(L[v], 5), M_GRAY[v],
			scale_down_round(gamma_correction_lut->mtp_vout[v][RED], 5), scale_down_rem(gamma_correction_lut->mtp_vout[v][RED], 5),
			scale_down_round(gamma_correction_lut->mtp_vout[v][GREEN], 5), scale_down_rem(gamma_correction_lut->mtp_vout[v][GREEN], 5),
			scale_down_round(gamma_correction_lut->mtp_vout[v][BLUE], 5), scale_down_rem(gamma_correction_lut->mtp_vout[v][BLUE], 5),
			gamma_correction_lut->mtp_offset[v][RED],
			gamma_correction_lut->mtp_offset[v][GREEN],
			gamma_correction_lut->mtp_offset[v][BLUE]);
	}

	pr_info("\n");
}

static int generate_gamma_table(struct gamma_correction *gamma_correction_lut, struct gray_scale *gray_scale_lut)
{
	int i, v, c, ilum;
	enum gamma_degree g_curve_degree;

	if (!calc_gray_order) {
		pr_err("%s, calc_vout_order not exist!!\n", __func__);
		return -EINVAL;
	}

	if (!calc_vout_order) {
		pr_err("%s, calc_vout_order not exist!!\n", __func__);
		return -EINVAL;
	}

	for (ilum = 0; ilum < NR_LUMINANCE; ilum++) {
		u32 luminance = gamma_correction_lut[ilum].luminance;
		u64 *L = gamma_correction_lut[ilum].L;
		u32 *M_GRAY = gamma_correction_lut[ilum].M_GRAY;
		for (i = 0; i < NR_TP; i++) {
			v = calc_gray_order[i];
			g_curve_degree = gamma_correction_lut[ilum].g_curve_degree;

			/* TODO : need to optimize */
			if (v == TP_V255)
				L[v] = ((u64)gamma_correction_lut[ilum].base_luminamce << BIT_SHIFT);
			else if (v == TP_VT)
				L[v] = (luminance < 251) ? 0 : (L[TP_V255] * (u64)gamma_curve_value(g_curve_degree, ivolt[v]) >> BIT_SHIFT);
			else
				L[v] = (L[TP_V255] * (u64)gamma_curve_value(g_curve_degree, ivolt[v])) >> BIT_SHIFT;

			M_GRAY[v] = (v == TP_VT) ? 0 : find_gray_scale_gamma_value(gray_scale_lut, L[v]) +
				gamma_correction_lut[ilum].gray_scale_offset[v];

			if (M_GRAY[v] > 255)
				M_GRAY[v] = 255;

			for (c = 0; c < MAX_COLOR; c++)
				gamma_correction_lut[ilum].mtp_vout[v][c] =
				(v == TP_VT) ? mtp->mtp_vout[v][c] : gray_scale_lut[M_GRAY[v]].vout[c];
		}

		for (i = 0; i < NR_TP; i++) {
			v = calc_vout_order[i];
			for (c = 0; c < MAX_COLOR; c++) {
				gamma_correction_lut[ilum].mtp_offset[v][c] =
					(v == TP_VT) ? mtp->mtp_base[v][c] : mtp_vregout_to_offset(v, (enum color)c, ilum);
			}
		}
		print_mtp_output_info(&gamma_correction_lut[ilum]);
	}

	return 0;
}

int alloc_dimming_info(struct dimming_info *dim_info, int nr_tp, int nr_luminance)
{
	int i;

	dim_info->nr_tp = nr_tp;
	dim_info->nr_luminance = nr_luminance;

	dim_info->tp = (int *)kzalloc(sizeof(int) * nr_tp, GFP_KERNEL);
	dim_info->tp_names = (char (*)[10])kzalloc(sizeof(char) * nr_tp * 10, GFP_KERNEL);

	/* allocation of struct mtp_data members */
	dim_info->mtp.mtp_base = (s32 (*)[MAX_COLOR])kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	dim_info->mtp.mtp_offset = (s32 (*)[MAX_COLOR])kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	dim_info->mtp.mtp_vout = (s64 (*)[MAX_COLOR])kzalloc(sizeof(s64) * nr_tp * MAX_COLOR, GFP_KERNEL);
	dim_info->mtp.v_coeff = (struct coeff_t *)kzalloc(sizeof(struct coeff_t) * nr_tp, GFP_KERNEL);

	/* allocation of struct gamma_correction lut and members */
	dim_info->gamma_correction_lut =
		(struct gamma_correction *)kzalloc(sizeof(struct gamma_correction) * nr_luminance, GFP_KERNEL);
	for (i = 0; i < nr_luminance; i++) {
		dim_info->gamma_correction_lut[i].gray_scale_offset =
			(s32 *)kzalloc(sizeof(s32) * nr_tp, GFP_KERNEL);
		dim_info->gamma_correction_lut[i].rgb_color_offset =
			(s32 (*)[MAX_COLOR])kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
		dim_info->gamma_correction_lut[i].L = (u64 *)kzalloc(sizeof(u64) * nr_tp, GFP_KERNEL);
		dim_info->gamma_correction_lut[i].M_GRAY = (s32 *)kzalloc(sizeof(s32) * nr_tp, GFP_KERNEL);
		dim_info->gamma_correction_lut[i].mtp_vout =
			(s64 (*)[MAX_COLOR])kzalloc(sizeof(s64) * nr_tp * MAX_COLOR, GFP_KERNEL);
		dim_info->gamma_correction_lut[i].mtp_offset =
			(s32 (*)[MAX_COLOR])kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
		dim_info->gamma_correction_lut[i].mtp_output_from_panel =
			(s32 (*)[MAX_COLOR])kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	}

	return 0;
}

void prepare_dim_info(struct dimming_info *dim_info)
{
	int i;

	NR_TP = dim_info->nr_tp;
	NR_LUMINANCE = dim_info->nr_luminance;
	gamma_correction_lut = dim_info->gamma_correction_lut;
	mtp = &dim_info->mtp;
	volt_name = dim_info->tp_names;
	ivolt = dim_info->tp;
	target_luminance = dim_info->target_luminance;

	TP_VT = 0;
	TP_2ND = 1;
	TP_3RD = 2;
	TP_V255 = NR_TP - 1;

	pr_info("NR_TP %d\n", NR_TP);

	if (!calc_gray_order) {
		calc_gray_order = generate_order(NR_TP - 1, 0, NR_TP - 2);
#ifdef DEBUG_AID_DIMMING
		pr_info("[calc_gray_order]\n");
		for (i = 0; i < NR_TP; i++)
			pr_info("%-3d\n", calc_gray_order[i]);
#endif
	}

	if (!calc_vout_order) {
		calc_vout_order = generate_order(0, NR_TP - 1, 1);
#ifdef DEBUG_AID_DIMMING
		pr_info("[calc_vout_order]\n");
		for (i = 0; i < NR_TP; i++)
			pr_info("%-3d\n", calc_vout_order[i]);
#endif
	}
}


void print_mtp_data(struct dimming_info *dim_info)
{
	int i, c, ilum;
	static char buf[512], *p = buf;
	struct mtp_data *mtp = &dim_info->mtp;

	pr_info("###### PRINT MTP DATA #####\n");
	pr_info("vregout %lld\n", mtp->vregout);

	pr_info("MTP_BASE\n");
	for (i = 0; i < dim_info->nr_tp; i++) {
		pr_info("%-3X %-3X %-3X\n",
				dim_info->mtp.mtp_base[i][RED],
				dim_info->mtp.mtp_base[i][GREEN],
				dim_info->mtp.mtp_base[i][BLUE]);
	}

	pr_info("g_curve_degree %d\n", gamma_curve_lut[(int)mtp->g_curve_degree].gamma);

	pr_info("MTP COEFFICIENCY\n");
	for (i = 0; i < dim_info->nr_tp; i++)
		pr_info("num %-3d den %-3d\n", mtp->v_coeff[i].numerator, mtp->v_coeff[i].denominator);

	pr_info("MTP VT COEFFICIENCY\n");
	for (i = 0; i < ARRAY_SIZE(mtp->vt_coeff); i++)
		pr_info("%d\n", mtp->vt_coeff[i].numerator);

	for (ilum = 0; ilum < dim_info->nr_luminance; ilum++) {
		struct gamma_correction *arr = &dim_info->gamma_correction_lut[ilum];
		p = buf;
		p += sprintf(p, "%-3d\t %04X %3d %3d\t ",
				arr->luminance, arr->aor, arr->base_luminamce, gamma_curve_lut[arr->g_curve_degree].gamma);
		for (i = dim_info->nr_tp - 1; i >= 1; i--)
			p += sprintf(p, "%3d ", arr->gray_scale_offset[i]);
		p += sprintf(p, "\t ");

		for (i = dim_info->nr_tp - 1; i >= 2; i--)
			for (c = 0; c < MAX_COLOR; c++)
				p += sprintf(p, "%3d ", arr->rgb_color_offset[i][c]);
		pr_info("%s\n", buf);
	}
}

int process_mtp(struct dimming_info *dim_info)
{
	int i, c, v;

	prepare_dim_info(dim_info);
#ifdef DEBUG_AID_DIMMING
	print_mtp_data(dim_info);
#endif

	/* 1. Generate RGB voltage output table of 9 TP using MTP(TP_VT ~ TP_V255) of panel. */
	for (i = 0; i < NR_TP; i++) {
		v = calc_vout_order[i];
		for (c = 0; c < MAX_COLOR; c++) {
			mtp->mtp_vout[v][c] = mtp_offset_to_vregout(v, (enum color)c);
			gray_scale_lut[ivolt[v]].vout[c] = (v == TP_VT) ? (u64)mtp->vregout : mtp->mtp_vout[v][c];
		}
	}
#ifdef DEBUG_AID_DIMMING
	print_gray_scale_mtp_vout(mtp);
#endif

	/* 2. Fill in the all RGB voltate output of gray scale (0 ~ 255) by interpolation. */
	generate_gray_scale(gray_scale_lut);

	/* 3. Generate mtp offset of given luminances. */
	generate_gamma_table(gamma_correction_lut, gray_scale_lut);

#ifdef DEBUG_AID_DIMMING
	print_mtp_offset(dim_info);
	print_mtp_output(gamma_correction_lut);
#endif

	/* TODO : free allocated heap */

	return 0;
}
