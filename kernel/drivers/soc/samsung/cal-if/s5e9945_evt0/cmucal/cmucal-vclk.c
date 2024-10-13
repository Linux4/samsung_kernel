#include "../../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"
#include "cmucal-vclklut.h"

enum clk_id cmgp_usi0_ipclkport_clk[] = {
	CMGP_MUX_CLK_CMGP_USI0,
	CMGP_DIV_CLK_CMGP_USI0,
};

enum clk_id cmgp_usi1_ipclkport_clk[] = {
	CMGP_MUX_CLK_CMGP_USI1,
	CMGP_DIV_CLK_CMGP_USI1,
};

enum clk_id cmgp_usi2_ipclkport_clk[] = {
	CMGP_MUX_CLK_CMGP_USI2,
	CMGP_DIV_CLK_CMGP_USI2,
};

enum clk_id cmgp_usi3_ipclkport_clk[] = {
	CMGP_MUX_CLK_CMGP_USI3,
	CMGP_DIV_CLK_CMGP_USI3,
};

enum clk_id cmgp_usi4_ipclkport_clk[] = {
	CMGP_MUX_CLK_CMGP_USI4,
	CMGP_DIV_CLK_CMGP_USI4,
};

enum clk_id cmgp_usi5_ipclkport_clk[] = {
	CMGP_MUX_CLK_CMGP_USI5,
	CMGP_DIV_CLK_CMGP_USI5,
};

enum clk_id cmgp_usi6_ipclkport_clk[] = {
	CMGP_MUX_CLK_CMGP_USI6,
	CMGP_DIV_CLK_CMGP_USI6,
};

enum clk_id cmgp_spi_i2c0_ipclkport_clk[] = {
	CMGP_MUX_CLK_CMGP_SPI_I2C0,
	CMGP_DIV_CLK_CMGP_SPI_I2C0,
};

enum clk_id cmgp_spi_i2c1_ipclkport_clk[] = {
	CMGP_MUX_CLK_CMGP_SPI_I2C1,
	CMGP_DIV_CLK_CMGP_SPI_I2C1,
};

enum clk_id peric0_usi04_ipclkport_clk[] = {
	PERIC0_MUX_CLK_PERIC0_USI04,
	PERIC0_DIV_CLK_PERIC0_USI04,
};

enum clk_id peric1_usi07_ipclkport_clk[] = {
	PERIC1_MUX_CLK_PERIC1_USI07,
	PERIC1_DIV_CLK_PERIC1_USI07,
};

enum clk_id peric1_usi08_ipclkport_clk[] = {
	PERIC1_MUX_CLK_PERIC1_USI07,
	PERIC1_DIV_CLK_PERIC1_USI07,
};

enum clk_id peric1_usi09_ipclkport_clk[] = {
	PERIC1_MUX_CLK_PERIC1_USI07,
	PERIC1_DIV_CLK_PERIC1_USI07,
};

enum clk_id peric1_usi10_ipclkport_clk[] = {
	PERIC1_MUX_CLK_PERIC1_USI07,
	PERIC1_DIV_CLK_PERIC1_USI07,
};

enum clk_id peric1_usi07_spi_i2c_ipclkport_clk[] = {
	PERIC1_MUX_CLK_PERIC1_USI07_SPI_I2C,
	PERIC1_DIV_CLK_PERIC1_USI07_SPI_I2C,
};

enum clk_id peric1_usi08_spi_i2c_ipclkport_clk[] = {
	PERIC1_MUX_CLK_PERIC1_USI08_SPI_I2C,
	PERIC1_DIV_CLK_PERIC1_USI08_SPI_I2C,
};

enum clk_id peric2_usi00_ipclkport_clk[] = {
	PERIC2_MUX_CLK_PERIC2_USI00,
	PERIC2_DIV_CLK_PERIC2_USI00,
};

enum clk_id peric2_usi01_ipclkport_clk[] = {
	PERIC2_MUX_CLK_PERIC2_USI01,
	PERIC2_DIV_CLK_PERIC2_USI01,
};

enum clk_id peric2_usi02_ipclkport_clk[] = {
	PERIC2_MUX_CLK_PERIC2_USI02,
	PERIC2_DIV_CLK_PERIC2_USI02,
};

enum clk_id peric2_usi03_ipclkport_clk[] = {
	PERIC2_MUX_CLK_PERIC2_USI03,
	PERIC2_DIV_CLK_PERIC2_USI03,
};

enum clk_id peric2_usi05_ipclkport_clk[] = {
	PERIC2_MUX_CLK_PERIC2_USI05,
	PERIC2_DIV_CLK_PERIC2_USI05,
};

enum clk_id peric2_usi06_ipclkport_clk[] = {
	PERIC2_MUX_CLK_PERIC2_USI06,
	PERIC2_DIV_CLK_PERIC2_USI06,
};

enum clk_id peric2_usi11_ipclkport_clk[] = {
	PERIC2_MUX_CLK_PERIC2_USI11,
	PERIC2_DIV_CLK_PERIC2_USI11,
};

enum clk_id peric2_usi00_spi_i2c_ipclkport_clk[] = {
	PERIC2_MUX_CLK_PERIC2_USI00_SPI_I2C,
	PERIC2_DIV_CLK_PERIC2_USI00_SPI_I2C,
};

enum clk_id peric2_usi01_spi_i2c_ipclkport_clk[] = {
	PERIC2_MUX_CLK_PERIC2_USI01_SPI_I2C,
	PERIC2_DIV_CLK_PERIC2_USI01_SPI_I2C,
};

struct vclk_lut cmucal_vclk_div_clk_peri_usixx_usi_lut[] = {
	{400000, div_clk_peri_400_lut_params},
	{200000, div_clk_peri_200_lut_params},
	{133000, div_clk_peri_133_lut_params},
	{100000, div_clk_peri_100_lut_params},
	{80000, div_clk_peri_80_lut_params},
	{66000, div_clk_peri_66_lut_params},
	{57000, div_clk_peri_57_lut_params},
	{50000, div_clk_peri_50_lut_params},
	{44000, div_clk_peri_44_lut_params},
	{40000, div_clk_peri_40_lut_params},
	{36000, div_clk_peri_36_lut_params},
	{33000, div_clk_peri_33_lut_params},
	{30000, div_clk_peri_30_lut_params},
	{28000, div_clk_peri_28_lut_params},
	{26000, div_clk_peri_26_lut_params},
	{25000, div_clk_peri_25_lut_params},
	{13000, div_clk_peri_13_lut_params},
	{8600, div_clk_peri_8_lut_params},
	{6500, div_clk_peri_6_lut_params},
	{5200, div_clk_peri_5_lut_params},
	{4300, div_clk_peri_4_lut_params},
	{3700, div_clk_peri_3_lut_params},
};
/*================================ VCLK List =================================*/
struct vclk cmucal_vclk_list[] = {
	CMUCAL_VCLK(VCLK_CMGP_USI0, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi0_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CMGP_USI1, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi1_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CMGP_USI2, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi2_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CMGP_USI3, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi3_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CMGP_USI4, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi4_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CMGP_USI5, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi5_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CMGP_USI6, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi6_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CMGP_SPI_I2C0, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_spi_i2c0_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CMGP_SPI_I2C1, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_spi_i2c1_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC0_USI04, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric0_usi04_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC1_USI07, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric1_usi07_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC1_USI08, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric1_usi08_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC1_USI09, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric1_usi09_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC1_USI10, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric1_usi10_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC1_USI07_SPI_I2C, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric1_usi07_spi_i2c_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC1_USI08_SPI_I2C, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric1_usi08_spi_i2c_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC2_USI00, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric2_usi00_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC2_USI01, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric2_usi01_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC2_USI02, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric2_usi02_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC2_USI03, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric2_usi03_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC2_USI05, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric2_usi05_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC2_USI06, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric2_usi06_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC2_USI11, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric2_usi11_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC2_USI00_SPI_I2C, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric2_usi00_spi_i2c_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_PERIC2_USI01_SPI_I2C, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric2_usi01_spi_i2c_ipclkport_clk, NULL, NULL),
};
unsigned int cmucal_vclk_size = ARRAY_SIZE(cmucal_vclk_list);
