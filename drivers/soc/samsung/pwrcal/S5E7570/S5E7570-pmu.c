#include "../pwrcal.h"
#include "../pwrcal-env.h"
#include "../pwrcal-rae.h"
#include "../pwrcal-pmu.h"
#include "S5E7570-cmusfr.h"
#include "S5E7570-pmusfr.h"
#include "S5E7570-cmu.h"

static void dispaud_prev(int enable)
{
	pwrcal_setbit(CLKRUN_CMU_DISPAUD_SYS_PWR_REG, 0, 0);
	pwrcal_setbit(CLKSTOP_CMU_DISPAUD_SYS_PWR_REG, 0, 0);
	pwrcal_setbit(DISABLE_PLL_CMU_DISPAUD_SYS_PWR_REG, 0, 0);
	pwrcal_setf(RESET_LOGIC_DISPAUD_SYS_PWR_REG, 0, 0x3, 0);
	pwrcal_setf(RESET_CMU_DISPAUD_SYS_PWR_REG, 0, 0x3, 0);
}

static void g3d_prev(int enable)
{
	pwrcal_setbit(CLKRUN_CMU_G3D_SYS_PWR_REG, 0, 0);
	pwrcal_setbit(CLKSTOP_CMU_G3D_SYS_PWR_REG, 0, 0);
	pwrcal_setbit(DISABLE_PLL_CMU_G3D_SYS_PWR_REG, 0, 0);
	pwrcal_setf(RESET_LOGIC_G3D_SYS_PWR_REG, 0, 0x3, 0);
	pwrcal_setf(RESET_CMU_G3D_SYS_PWR_REG, 0, 0x3, 0);
}

static void isp_prev(int enable)
{
	pwrcal_setbit(CLKRUN_CMU_ISP_SYS_PWR_REG, 0, 0);
	pwrcal_setbit(CLKSTOP_CMU_ISP_SYS_PWR_REG, 0, 0);
	pwrcal_setbit(DISABLE_PLL_CMU_ISP_SYS_PWR_REG, 0, 0);
	pwrcal_setf(RESET_LOGIC_ISP_SYS_PWR_REG, 0, 0x3, 0);
	pwrcal_setf(RESET_CMU_ISP_SYS_PWR_REG, 0, 0x3, 0);
}

static void mfcmscl_prev(int enable)
{
	pwrcal_setbit(CLKRUN_CMU_MFCMSCL_SYS_PWR_REG, 0, 0);
	pwrcal_setbit(CLKSTOP_CMU_MFCMSCL_SYS_PWR_REG, 0, 0);
	pwrcal_setbit(DISABLE_PLL_CMU_MFCMSCL_SYS_PWR_REG, 0, 0);
	pwrcal_setf(RESET_LOGIC_MFCMSCL_SYS_PWR_REG, 0, 0x3, 0);
	pwrcal_setf(RESET_CMU_MFCMSCL_SYS_PWR_REG, 0, 0x3, 0);
}

static void dispaud_post(int enable)
{
	pwrcal_setbit(PAD_RETENTION_AUD_OPTION, 28, 1);
}

static void g3d_post(int enable)
{
}

static void isp_post(int enable)
{
}

static void mfcmscl_post(int enable)
{
}

static void dispaud_config(int enable)
{
	pwrcal_setf(DISPAUD_OPTION, 0, 0xFFFFFFFF, 0x2);
}

static void g3d_config(int enable)
{
	pwrcal_setf(G3D_OPTION, 0, 0xFFFFFFFF, 0x1);
}

static void isp_config(int enable)
{
	pwrcal_setf(ISP_OPTION, 0, 0xFFFFFFFF, 0x2);
}

static void mfcmscl_config(int enable)
{
	pwrcal_setf(MFCMSCL_OPTION, 0, 0xFFFFFFFF, 0x2);
}

BLKPWR(blkpwr_dispaud, DISPAUD_CONFIGURATION, 0, 0xF, DISPAUD_STATUS, 0, 0xF, dispaud_config, dispaud_prev, dispaud_post);
BLKPWR(blkpwr_g3d, G3D_CONFIGURATION, 0, 0xF, G3D_STATUS, 0, 0xF, g3d_config, g3d_prev, g3d_post);
BLKPWR(blkpwr_isp, ISP_CONFIGURATION, 0, 0xF, ISP_STATUS, 0, 0xF, isp_config, isp_prev, isp_post);
BLKPWR(blkpwr_mfcmscl, MFCMSCL_CONFIGURATION, 0, 0xF, MFCMSCL_STATUS, 0, 0xF, mfcmscl_config, mfcmscl_prev, mfcmscl_post);

struct cal_pd *pwrcal_blkpwr_list[4];
unsigned int pwrcal_blkpwr_size = 4;

static int blkpwr_init(void)
{
	pwrcal_blkpwr_list[0] = &blkpwr_blkpwr_dispaud;
	pwrcal_blkpwr_list[1] = &blkpwr_blkpwr_g3d;
	pwrcal_blkpwr_list[2] = &blkpwr_blkpwr_isp;
	pwrcal_blkpwr_list[3] = &blkpwr_blkpwr_mfcmscl;

	return 0;
}

struct cal_pd_ops cal_pd_ops = {
	.pd_control = blkpwr_control,
	.pd_status = blkpwr_status,
	.pd_init = blkpwr_init,
};
