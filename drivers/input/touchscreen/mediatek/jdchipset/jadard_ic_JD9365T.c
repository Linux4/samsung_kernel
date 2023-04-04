#include "jadard_platform.h"
#include "jadard_common.h"
#include "jadard_module.h"
#include "jadard_ic_JD9365T.h"

extern struct jadard_module_fp g_module_fp;
extern struct jadard_ts_data *pjadard_ts_data;
extern struct jadard_ic_data *pjadard_ic_data;
extern struct jadard_report_data *pjadard_report_data;
extern struct jadard_common_variable g_common_variable;

#ifdef JD_AUTO_UPGRADE_FW
extern uint8_t *g_jadard_fw;
extern uint32_t g_jadard_fw_ver;
extern uint32_t g_jadard_fw_cid_ver;
#endif

#ifdef CONFIG_TOUCHSCREEN_JADARD_DEBUG
extern int *jd_diag_mutual;
#endif

static void jd9365t_ChipInfoInit(void)
{
	/* Initial section info */
	g_jd_section_info.section_info_ready            = false;
	g_jd_section_info.section_info_ready_addr       = JD9365T_MEMORY_ADDR_ERAM;

	g_jd_section_info.dsram_num_start_addr          = JD9365T_MEMORY_ADDR_ERAM + 2;
	g_jd_section_info.dsram_section_info_start_addr = JD9365T_MEMORY_ADDR_ERAM + 4;

	g_jd_section_info.esram_num_start_addr          = (g_jd_section_info.dsram_section_info_start_addr +
														JD9365T_MAX_DSRAM_NUM * sizeof(struct JD9365T_INFO_CONTENT));
	g_jd_section_info.esram_section_info_start_addr = g_jd_section_info.esram_num_start_addr + 4;
}

static bool jd9365t_DoneStatusIsHigh(uint8_t usValue, uint8_t DoneStatus)
{
	return ((usValue & DoneStatus) == DoneStatus);
}

static bool jd9365t_DoneStatusIsLow(uint8_t usValue, uint8_t BusyStatus, uint8_t DoneStatus)
{
	return ((usValue & BusyStatus) == DoneStatus);
}

static int jd9365t_Read_BackDoor_RegSingle(uint32_t addr, uint8_t *rdata)
{
	int ReCode;
	uint8_t addrBuf[JD_SIX_SIZE];
	uint8_t readBuf[JD_ONE_SIZE];

	addrBuf[0] = 0xF3;
	addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[5] = 0x01;

	ReCode = jadard_bus_read(addrBuf, sizeof(addrBuf), readBuf, sizeof(readBuf), JADARD_BUS_RETRY_TIMES);
	*rdata = readBuf[0];

	return ReCode;
}

static int jd9365t_Write_BackDoor_RegSingle(uint32_t addr, uint8_t wdata)
{
	uint8_t addrBuf[JD_SIX_SIZE];
	uint8_t writeBuf[JD_ONE_SIZE];

	addrBuf[0] = 0xF2;
	addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[5] = 0x01;
	writeBuf[0] = wdata;

	return jadard_bus_write(addrBuf, sizeof(addrBuf), writeBuf, sizeof(writeBuf), JADARD_BUS_RETRY_TIMES);
}

static int jd9365t_Read_BackDoor_RegMulti(uint32_t addr, uint8_t *rdata, uint16_t rlen)
{
	uint8_t addrBuf[JD_SIX_SIZE];

	addrBuf[0] = 0xF3;
	addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[5] = 0x01;

	return jadard_bus_read(addrBuf, sizeof(addrBuf), rdata, rlen, JADARD_BUS_RETRY_TIMES);
}

static int jd9365t_Write_BackDoor_RegMulti(uint32_t addr, uint8_t *wdata, uint16_t wlen)
{
	uint8_t addrBuf[JD_SIX_SIZE];

	addrBuf[0] = 0xF2;
	addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[5] = 0x01;

	return jadard_bus_write(addrBuf, sizeof(addrBuf), wdata, wlen, JADARD_BUS_RETRY_TIMES);
}

static int jd9365t_Read_FW_RegSingleI2c(uint32_t addr, uint8_t *rdata)
{
	int ReCode;
	uint8_t addrBuf[JD_SIX_SIZE];
	uint8_t readBuf[JD_ONE_SIZE];

	addrBuf[0] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[1] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[2] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[3] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[4] = 0x00;
	addrBuf[5] = 0x01;

	ReCode = jadard_bus_read(addrBuf, sizeof(addrBuf), readBuf, sizeof(readBuf), JADARD_BUS_RETRY_TIMES);
	*rdata = readBuf[0];

	return ReCode;
}

static int jd9365t_Write_FW_RegSingleI2c(uint32_t addr, uint8_t wdata)
{
	uint8_t addrBuf[JD_SIX_SIZE];
	uint8_t writeBuf[JD_ONE_SIZE];

	addrBuf[0] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[1] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[2] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[3] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[4] = 0x00;
	addrBuf[5] = 0x01;
	writeBuf[0] = wdata;

	return jadard_bus_write(addrBuf, sizeof(addrBuf), writeBuf, sizeof(writeBuf), JADARD_BUS_RETRY_TIMES);
}

static int jd9365t_Read_FW_RegMultiI2c(uint32_t addr, uint8_t *rdata, uint16_t rlen)
{
	uint8_t addrBuf[JD_SIX_SIZE];

	addrBuf[0] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[1] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[2] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[3] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[4] = (uint8_t)((rlen & 0xFF00) >> 8);
	addrBuf[5] = (uint8_t)((rlen & 0x00FF) >> 0);

	return jadard_bus_read(addrBuf, sizeof(addrBuf), rdata, rlen, JADARD_BUS_RETRY_TIMES);
}

static int jd9365t_Write_FW_RegMultiI2c(uint32_t addr, uint8_t *wdata, uint16_t wlen)
{
	uint8_t addrBuf[JD_SIX_SIZE];

	addrBuf[0] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[1] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[2] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[3] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[4] = (uint8_t)((wlen & 0xFF00) >> 8);
	addrBuf[5] = (uint8_t)((wlen & 0x00FF) >> 0);

	return jadard_bus_write(addrBuf, sizeof(addrBuf), wdata, wlen, JADARD_BUS_RETRY_TIMES);
}

static int jd9365t_Read_FW_RegSingleSpi(uint32_t addr, uint8_t *rdata)
{
	int ReCode;
	uint8_t addrBuf[JD_SEVEN_SIZE];
	uint8_t readBuf[JD_ONE_SIZE];

	addrBuf[0] = 0xF3;
	addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[5] = 0x00;
	addrBuf[6] = 0x01;

	ReCode = jadard_bus_read(addrBuf, sizeof(addrBuf), readBuf, sizeof(readBuf), JADARD_BUS_RETRY_TIMES);
	*rdata = readBuf[0];

	return ReCode;
}

static int jd9365t_Write_FW_RegSingleSpi(uint32_t addr, uint8_t wdata)
{
	uint8_t addrBuf[JD_SEVEN_SIZE];
	uint8_t writeBuf[JD_ONE_SIZE];

	addrBuf[0] = 0xF2;
	addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[5] = 0x00;
	addrBuf[6] = 0x01;
	writeBuf[0] = wdata;

	return jadard_bus_write(addrBuf, sizeof(addrBuf), writeBuf, sizeof(writeBuf), JADARD_BUS_RETRY_TIMES);
}

static int jd9365t_Read_FW_RegMultiSpi(uint32_t addr, uint8_t *rdata, uint16_t rlen)
{
	uint8_t addrBuf[JD_SEVEN_SIZE];

	addrBuf[0] = 0xF3;
	addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[5] = (uint8_t)((rlen & 0xFF00) >> 8);
	addrBuf[6] = (uint8_t)((rlen & 0x00FF) >> 0);

	return jadard_bus_read(addrBuf, sizeof(addrBuf), rdata, rlen, JADARD_BUS_RETRY_TIMES);
}

static int jd9365t_Write_FW_RegMultiSpi(uint32_t addr, uint8_t *wdata, uint16_t wlen)
{
	uint8_t addrBuf[JD_SEVEN_SIZE];

	addrBuf[0] = 0xF2;
	addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
	addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
	addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
	addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
	addrBuf[5] = (uint8_t)((wlen & 0xFF00) >> 8);
	addrBuf[6] = (uint8_t)((wlen & 0x00FF) >> 0);

	return jadard_bus_write(addrBuf, sizeof(addrBuf), wdata, wlen, JADARD_BUS_RETRY_TIMES);
}

static int jd9365t_ReadRegSingle(uint32_t addr, uint8_t *rdata)
{
	int ReCode;

	jd9365t_EnterBackDoor(NULL);

	if (g_jd9365t_chip_info.back_door_mode) {
		ReCode = jd9365t_Read_BackDoor_RegSingle(addr, rdata);
	} else {
		if ((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) {
			ReCode = jd9365t_Read_FW_RegSingleSpi(addr, rdata);
		} else {
			ReCode = jd9365t_Read_FW_RegSingleI2c(addr, rdata);
		}
	}

	jd9365t_ExitBackDoor();
	return ReCode;
}

static int jd9365t_WriteRegSingle(uint32_t addr, uint8_t wdata)
{
	int ReCode;

	jd9365t_EnterBackDoor(NULL);

	if (g_jd9365t_chip_info.back_door_mode) {
		ReCode = jd9365t_Write_BackDoor_RegSingle(addr, wdata);
	} else {
		if ((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) {
			ReCode = jd9365t_Write_FW_RegSingleSpi(addr, wdata);
		} else {
			ReCode = jd9365t_Write_FW_RegSingleI2c(addr, wdata);
		}
	}

	jd9365t_ExitBackDoor();
	return ReCode;
}

static int jd9365t_ReadRegMulti(uint32_t addr, uint8_t *rdata, uint16_t rlen)
{
	int ReCode;

	jd9365t_EnterBackDoor(NULL);
	if (g_jd9365t_chip_info.back_door_mode) {
		ReCode = jd9365t_Read_BackDoor_RegMulti(addr, rdata, rlen);
	} else {
		if ((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) {
			ReCode = jd9365t_Read_FW_RegMultiSpi(addr, rdata, rlen);
		} else {
			ReCode = jd9365t_Read_FW_RegMultiI2c(addr, rdata, rlen);
		}
	}

	jd9365t_ExitBackDoor();
	return ReCode;
}

static int jd9365t_WriteRegMulti(uint32_t addr, uint8_t *wdata, uint16_t wlen)
{
	int ReCode;

	jd9365t_EnterBackDoor(NULL);
	if (g_jd9365t_chip_info.back_door_mode) {
		ReCode = jd9365t_Write_BackDoor_RegMulti(addr, wdata, wlen);
	} else {
		if ((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) {
			ReCode = jd9365t_Write_FW_RegMultiSpi(addr, wdata, wlen);
		} else {
			ReCode = jd9365t_Write_FW_RegMultiI2c(addr, wdata, wlen);
		}
	}

	jd9365t_ExitBackDoor();
	return ReCode;
}

static int jd9365t_GetID(uint16_t *pRomID)
{
	int ReCode;
	uint8_t rbuf[JD_TWO_SIZE];

	ReCode = jd9365t_ReadRegMulti((uint32_t)JD9365T_SOC_REG_ADDR_CHIP_ID2, rbuf, sizeof(rbuf));

	if (ReCode < 0)
		return ReCode;

	*pRomID = (uint16_t)((rbuf[1] << 8) + rbuf[0]);

	if (*pRomID == JD9365T_ID) {
		JD_I("%s: IC ID: %04x\n", __func__, *pRomID);
	} else {
		JD_E("%s: Error IC ID: %04x\n", __func__, *pRomID);
	}

	return ReCode;
}

int jd9365t_EnterBackDoor(uint16_t *pRomID)
{
	int ReCode;
	uint8_t addrBuf[JD_FIVE_SIZE];
	uint8_t writeBuf[JD_ONE_SIZE];
	addrBuf[0] = 0xF2;
	addrBuf[1] = 0xAA;
	addrBuf[2] = 0x55;
	addrBuf[3] = 0x0F;
	addrBuf[4] = 0xF0;
	writeBuf[0] = 0x68;
	ReCode = jadard_bus_write(addrBuf, sizeof(addrBuf), writeBuf, sizeof(writeBuf), JADARD_BUS_RETRY_TIMES);
	if (ReCode < 0) {
		JD_E("%s: EnterBackDoor fail\n", __func__);
	} else {
		g_jd9365t_chip_info.back_door_mode = true;
		if (pRomID) {
			ReCode = jd9365t_GetID(pRomID);
		}
	}
	return ReCode;
}
int jd9365t_ExitBackDoor(void)
{
	int ReCode;
	uint8_t addrBuf[JD_FIVE_SIZE];
	uint8_t writeBuf[JD_ONE_SIZE];
	addrBuf[0] = 0xF2;
	addrBuf[1] = 0xAA;
	addrBuf[2] = 0x88;
	addrBuf[3] = 0x00;
	addrBuf[4] = 0x00;
	writeBuf[0] = 0x00;
	ReCode = jadard_bus_write(addrBuf, sizeof(addrBuf), writeBuf, sizeof(writeBuf), JADARD_BUS_RETRY_TIMES);
	if (ReCode < 0) {
		JD_E("%s: ExitBackDoor fail\n", __func__);
	} else {
		g_jd9365t_chip_info.back_door_mode = false;
	}
	return ReCode;
}

/* static int jd9365t_DisableSleepOutInt(void)
{
	return jd9365t_WriteRegSingle((uint32_t)JD9365T_SOC_REG_ADDR_SLPOUT_INT_EN,
									(uint8_t)SOC_RELATED_SETTING_DISABLE_SLEEP_OUT_INT);
}

static int jd9365t_EnableSleepOutInt(void)
{
	return jd9365t_WriteRegSingle((uint32_t)JD9365T_SOC_REG_ADDR_SLPOUT_INT_EN,
									(uint8_t)SOC_RELATED_SETTING_ENABLE_SLEEP_OUT_INT);
} */

static void jd9365t_PinReset(void)
{
#ifdef JD_RST_PIN_FUNC
	JD_I("%s: Now reset the Touch chip\n", __func__);
	jadard_gpio_set_value(pjadard_ts_data->rst_gpio, 1);
	mdelay(10);
	jadard_gpio_set_value(pjadard_ts_data->rst_gpio, 0);
	mdelay(10);
	jadard_gpio_set_value(pjadard_ts_data->rst_gpio, 1);
	mdelay(100);
#endif
	/* EnterBackDoor */
	jd9365t_EnterBackDoor(NULL);
}

static int jd9365t_StartMCUClock(void)
{
	int ReCode;

	ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_SOC_REG_ADDR_CPU_CLK_STOP,
										(uint8_t)JD9365T_SOC_PASSWORD_START_MCU_CLOCK);

	if (ReCode < 0) {
		JD_E("%s: Start MCU clock fail\n", __func__);
	}

	return ReCode;
}

static int jd9365t_StopMCUClock(void)
{
	int ReCode;

	ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_SOC_REG_ADDR_CPU_CLK_STOP,
										(uint8_t)JD9365T_SOC_PASSWORD_STOP_MCU_CLOCK);

	if (ReCode < 0) {
		JD_E("%s: Stop MCU clock fail\n", __func__);
	}

	return ReCode;
}

static int jd9365t_StartMCU(void)
{
	int ReCode;

	ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_SOC_REG_ADDR_PRAM_PROG,
										(uint8_t)JD9365T_SOC_PASSWORD_START_MCU);

	if (ReCode < 0) {
		JD_E("%s: Start MCU fail\n", __func__);
	}

	return ReCode;
}

static int jd9365t_StopMCU(void)
{
	int ReCode;

	ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_SOC_REG_ADDR_PRAM_PROG,
										(uint8_t)JD9365T_SOC_PASSWORD_STOP_MCU);

	if (ReCode < 0) {
		JD_E("%s: Stop MCU fail\n", __func__);
	}

	return ReCode;
}

static int jd9365t_ResetSOC(void)
{
	int ReCode;

	ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_SOC_REG_ADDR_RGU_0,
										(uint8_t)JD9365T_SOC_PASSWORD_SOC_RESET);

	if (ReCode < 0) {
		JD_E("%s: Reset SOC fail\n", __func__);
	} else {
		g_jd9365t_chip_info.back_door_mode = false;
	}

	return ReCode;
}

static int jd9365t_DisableTouchScan(void)
{
	return jd9365t_WriteRegSingle((uint32_t)JD9365T_STC1_REG_ADDR_AFE_SCAN_CTRL,
									(uint8_t)JD9365T_STC1_RELATED_SETTING_STC_SCAN_DISABLE);
}

static int jd9365t_EnableTouchScan(void)
{
	return jd9365t_WriteRegSingle((uint32_t)JD9365T_STC1_REG_ADDR_AFE_SCAN_CTRL,
									(uint8_t)JD9365T_STC1_RELATED_SETTING_STC_SCAN_ENABLE);
}

static int jd9365t_DisableRTCRun(void)
{
	return jd9365t_WriteRegSingle((uint32_t)JD9365T_TIMER_REG_ADDR_RTC_CONFIG,
									(uint8_t)JD9365T_TIMER_RELATED_SETTING_DISABLE_RTC_RUN);
}

static int jd9365t_EnableRTCRun(void)
{
	return jd9365t_WriteRegSingle((uint32_t)JD9365T_TIMER_REG_ADDR_RTC_CONFIG,
									(uint8_t)JD9365T_TIMER_RELATED_SETTING_ENABLE_RTC_RUN);
}

static int jd9365t_SetFlashSPISpeed(uint8_t flash_spi_speed_level)
{
	int ReCode;

	ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_BAUD_RATE, flash_spi_speed_level);

	if (ReCode < 0) {
		JD_E("%s: Set flash SPI speed fail\n", __func__);
	}

	return ReCode;
}

static int jd9365t_GetFlashSPIFStatus(uint16_t usTimeOut, uint8_t *rdata)
{
	int ReCode;
	uint32_t i = 0;
	uint8_t usValue = (uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_SPIF_BUSY;
	bool spif_busy_status_done = false;

	do
	{
		/* Read SPI status until done */
		mdelay(2);
		ReCode = jd9365t_ReadRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_STATUS, &usValue);

		if (ReCode < 0) {
			JD_E("%s: Read flash SPI status fail\n", __func__);
			return ReCode;
		} else {
			i += 2;
		}

		spif_busy_status_done = jd9365t_DoneStatusIsHigh(usValue,
															(uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_SPIF_DONE);

		/* Read SPI data when rdata was not NULL */
		if (rdata != NULL) {
			ReCode = jd9365t_ReadRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_RDATA, rdata);
			if (ReCode <0) {
				JD_E("%s: Read SPI RDATA fail\n", __func__);
				return ReCode;
			}
		}
	} while ((i < usTimeOut) && !spif_busy_status_done);

	if (i >= usTimeOut) {
		JD_E("%s: Get Flash SPI status timeout\n", __func__);
		return JD_TIME_OUT;
	}

	return ReCode;
}

static int jd9365t_ICSetExFlashCSNOutDisable(void)
{
	return jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_CSN_OUT,
									(uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_CSN_H);
}

static int jd9365t_ICSetExFlashCSNOutEnable(void)
{
	return jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_CSN_OUT,
									(uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_CSN_L);
}

static int jd9365t_ICWriteExFlashFlow(uint8_t cmd, uint32_t addr, uint16_t addr_len)
{
	int ReCode, i;
	uint8_t wBuf[JD_THREE_SIZE];
	uint8_t dummy;

	/* Set SPI frequency to 12MHz */
	wBuf[0] = (uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_SPEED_12MHz;

	ReCode = jd9365t_WriteRegMulti((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_BAUD_RATE, wBuf, 1);
	if (ReCode < 0) {
		JD_E("%s: Set SPI frequency to 12MHz fail\n", __func__);
		return ReCode;
	}

	wBuf[0] = (uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_CSN_L; /* Set CSN out enable [CSN = L] */
	wBuf[1] = 0x01; /* set SPI enable */
	wBuf[2] = cmd; /* set Flash command */

	ReCode = jd9365t_WriteRegMulti((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_CSN_OUT, wBuf, sizeof(wBuf));
	if (ReCode < 0) {
		JD_E("%s: SPI setting fail\n", __func__);
		return ReCode;
	}

	/* Read SPI status until done */
	ReCode = jd9365t_GetFlashSPIFStatus(50, &dummy);
	if (ReCode < 0) {
		JD_E("%s: Get flash SPI status fail\n", __func__);
		return ReCode;
	}

	for (i = 0; i < addr_len; i++) {
		ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_WDATA,
											(uint8_t)(addr >> ((addr_len - i - 1) * 8)));
		if (ReCode < 0) {
			JD_E("%s: Set flash SPI addrress fail\n", __func__);
			return ReCode;
		}

		ReCode = jd9365t_GetFlashSPIFStatus(50, &dummy);
		if (ReCode < 0) {
			JD_E("%s: Get flash SPI status fail\n", __func__);
			return ReCode;
		}
	}

	/* Set CSN out disable [CSN = H] */
	ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_CSN_OUT,
										(uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_CSN_H);
	if (ReCode < 0) {
		JD_E("%s: Set CSN high fail\n", __func__);
	}

	return ReCode;
}

static int jd9365t_ICReadExFlashFlow(uint8_t cmd, uint32_t addr, uint16_t addr_len,
										uint8_t *rdata, uint16_t rdata_len)
{
	int ReCode, i;
	uint8_t wBuf[JD_THREE_SIZE];
	uint8_t dummy;

	/* Set SPI frequency to 12MHz */
	wBuf[0] = (uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_SPEED_12MHz;

	ReCode = jd9365t_WriteRegMulti((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_BAUD_RATE, wBuf, 1);
	if (ReCode < 0) {
		JD_E("%s: Set SPI frequency to 12MHz fail\n", __func__);
		return ReCode;
	}

	/* Set CSN out enable [CSN = L] */
	wBuf[0] = (uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_CSN_L;
	wBuf[1] = 0x01; /* set SPI enable */
	wBuf[2] = cmd; /* set Flash command */

	ReCode = jd9365t_WriteRegMulti((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_CSN_OUT, wBuf, sizeof(wBuf));
	if (ReCode < 0) {
		JD_E("%s: SPI setting fail\n", __func__);
		return ReCode;
	}

	/* Read SPI status until done */
	ReCode = jd9365t_GetFlashSPIFStatus(50, &dummy);
	if (ReCode < 0) {
		JD_E("%s: Get flash SPI status fail\n", __func__);
		return ReCode;
	}

	for (i = 0; i < addr_len; i++) {
		ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_WDATA,
											(uint8_t)(addr >> ((addr_len - i - 1) * 8)));
		if (ReCode < 0) {
			JD_E("%s: Set flash SPI addrress fail\n", __func__);
			return ReCode;
		}

		ReCode = jd9365t_GetFlashSPIFStatus(50, &dummy);
		if (ReCode < 0) {
			JD_E("%s: Get flash SPI status fail\n", __func__);
			return ReCode;
		}
	}

	for (i = 0; i < rdata_len; i++) {
		/* set Dummy data */
		ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_WDATA, (uint8_t)(i + 1));
		if (ReCode < 0) {
			JD_E("%s: Set flash SPI read addrress fail\n", __func__);
			return ReCode;
		}

		/* Read SPI status until done & read SPI data */
		ReCode = jd9365t_GetFlashSPIFStatus(50, rdata + i);
		if (ReCode < 0) {
			JD_E("%s: Get flash SPI status fail\n", __func__);
			return ReCode;
		}
	}

	/* Set CSN out disable [CSN = H] */
	ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_CSN_OUT,
										(uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_CSN_H);
	if (ReCode < 0) {
		JD_E("%s: Set CSN high fail\n", __func__);
	}

	return ReCode;
}

static int jd9365t_ICGetExFlashStatus(uint16_t usTimeOut)
{
	int ReCode;
	uint32_t i = 0;
	uint8_t usValue = (uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_FLASH_BUSY;
	bool flash_busy_status_done = false;

	do
	{
		/* Set CSN out enable [CSN = L] */
		ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_CSN_OUT,
											(uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_CSN_L);
		if (ReCode < 0) {
			JD_E("%s: Set CSN low fail\n", __func__);
			return ReCode;
		}

		/* Set read flash status start */
		ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_DMA_2WFLASHEN_1WRITE_0START,
											(uint8_t)JD9365T_DMA_RELATED_SETTING_READ_FLASH_STATUS);
		if (ReCode < 0) {
			JD_E("%s: Set read flash status start fail\n", __func__);
			return ReCode;
		}

		/* Wait Flash Status Register 1 ready */
		mdelay(5);

		/* Set CSN out disable [CSN = H] */
		ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_SPI_CSN_OUT,
											(uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_CSN_H);
		if (ReCode < 0) {
			JD_E("%s: Set CSN high fail\n", __func__);
			return ReCode;
		}

		/* Read Ex-Flash status until done */
		ReCode = jd9365t_ReadRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_DMA_BUSY_OR_START, &usValue);
		if (ReCode < 0) {
			JD_E("%s: Read Ex-Flash status fail\n", __func__);
			return ReCode;
		} else {
			i += 5;
		}

		flash_busy_status_done = jd9365t_DoneStatusIsLow(usValue,
															(uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_FLASH_BUSY,
															(uint8_t)JD9365T_MASTER_SPI_RELATED_SETTING_FLASH_DONE);
	} while ((i < usTimeOut) && !flash_busy_status_done);

	if (i >= usTimeOut) {
		JD_E("%s: Get Ex-Flash status timeout\n", __func__);
		return JD_TIME_OUT;
	}

	return ReCode;
}

static int jd9365t_EraseSector(uint32_t addr)
{
	int ReCode;

	/* Write enable */
	ReCode = jd9365t_ICWriteExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_WRITE_ENABLE, 0, 0);
	if (ReCode < 0) {
		JD_E("%s: Set write enable fail\n", __func__);
		return ReCode;
	}

	/* Erase cmd */
	ReCode = jd9365t_ICWriteExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_ERASE_SECTOR, addr, 3);
	if (ReCode < 0) {
		JD_E("%s: Set erase cmd fail\n", __func__);
		return ReCode;
	}

	msleep((uint16_t)JD9365T_FLASH_RUN_TIME_ERASE_SECTOR_TIME);

	/* Read Ex-Flash status until done */
	ReCode = jd9365t_ICGetExFlashStatus(50);
	if (ReCode < 0) {
		JD_E("%s: Read Ex-Flash status fail\n", __func__);
	}

	return ReCode;
}

static int jd9365t_EraseBlock_32K(uint32_t addr)
{
	int ReCode;

	/* Write enable */
	ReCode = jd9365t_ICWriteExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_WRITE_ENABLE, 0, 0);
	if (ReCode < 0) {
		JD_E("%s: Set write enable fail\n", __func__);
		return ReCode;
	}

	/* Erase cmd */
	ReCode = jd9365t_ICWriteExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_ERASE_BLOCK_32K, addr, 3);
	if (ReCode < 0) {
		JD_E("%s: Set erase 32k cmd fail\n", __func__);
		return ReCode;
	}

	msleep((uint16_t)JD9365T_FLASH_RUN_TIME_ERASE_BLOCK_32K_TIME);

	/* Read Ex-Flash status until done */
	ReCode = jd9365t_ICGetExFlashStatus(50);
	if (ReCode < 0) {
		JD_E("%s: Read Ex-Flash status fail\n", __func__);
	}
	return ReCode;
}

static int jd9365t_EraseBlock_64K(uint32_t addr)
{
	int ReCode;

	/* Write enable */
	ReCode = jd9365t_ICWriteExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_WRITE_ENABLE, 0, 0);
	if (ReCode < 0) {
		JD_E("%s: Set write enable fail\n", __func__);
		return ReCode;
	}

	/* Erase cmd */
	ReCode = jd9365t_ICWriteExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_ERASE_BLOCK_64K, addr, 3);
	if (ReCode < 0) {
		JD_E("%s: Set erase 64k cmd fail\n", __func__);
		return ReCode;
	}

	msleep((uint16_t)JD9365T_FLASH_RUN_TIME_ERASE_BLOCK_64K_TIME);

	/* Read Ex-Flash status until done */
	ReCode = jd9365t_ICGetExFlashStatus(50);
	if (ReCode < 0) {
		JD_E("%s: Read Ex-Flash status fail\n", __func__);
	}
	return ReCode;
}

static int jd9365t_EraseChip(void)
{
	int ReCode;

	/* Write enable */
	ReCode = jd9365t_ICWriteExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_WRITE_ENABLE, 0, 0);
	if (ReCode < 0) {
		JD_E("%s: Set write enable fail\n", __func__);
		return ReCode;
	}

	/* Erase cmd */
	ReCode = jd9365t_ICWriteExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_ERASE_CHIP, 0, 0);
	if (ReCode < 0) {
		JD_E("%s: Set erase chip cmd fail\n", __func__);
		return ReCode;
	}

	msleep((uint16_t)JD9365T_FLASH_RUN_TIME_ERASE_CHIP_TIME);

	/* Read Ex-Flash status until done */
	ReCode = jd9365t_ICGetExFlashStatus(50);
	if (ReCode < 0) {
		JD_E("%s: Read Ex-Flash status fail\n", __func__);
	}
	return ReCode;
}

static int jd9365t_EraseChipFlash(uint32_t addr, uint32_t len)
{
	int ReCode, i;
	int sector_start = 0;
	int sector_end = 0;

	if ((addr + len) > (JD9365T_SIZE_DEF_BLOCK_64K * 2)) {
		ReCode = jd9365t_EraseChip();
		if (ReCode < 0) {
			JD_E("%s: Erase flash timeout error\n", __func__);
			return ReCode;
		}
	} else {
		sector_start = (int)(addr >> 12);

		if (((addr + len) & 0xFFF) == 0) {
			sector_end = (int)((addr + len) >> 12);
		} else {
			sector_end = (int)((addr + len) >> 12) + 1;
		}

		for (i = sector_start; i < sector_end;/* Acc in loop */)
		{
			if ((i == 0 || i == 16) && (sector_end - i) >= 16) {
				/* 64KB BLOCK ERASE */
				ReCode = jd9365t_EraseBlock_64K((uint32_t)(i << 12));
				if (ReCode < 0) {
					JD_E("%s: Erase 64KB block timeout error\n", __func__);
					return ReCode;
				} else {
					i += 16;
					JD_D("%s: EraseStart %08x\n", __func__, (uint32_t)(i << 12));
					JD_D("%s: Size 64KB\n", __func__);
					JD_D("%s: Erase 64KB block finish\n", __func__);
				}
			} else if ((i == 0 || i == 8 || i == 16 || i == 24) && (sector_end - i) >= 8) {
				/* 32KB BLOCK ERASE */
				ReCode = jd9365t_EraseBlock_32K((uint32_t)(i << 12));
				if (ReCode < 0) {
					JD_E("%s: Erase 32KB block timeout error\n", __func__);
					return ReCode;
				} else {
					i += 8;
					JD_D("%s: EraseStart %08x\n", __func__, (uint32_t)(i << 12));
					JD_D("%s: Size 32KB\n", __func__);
					JD_D("%s: Erase 32KB block finish\n", __func__);
				}
			} else {
				/* SECTOR ERASE */
				ReCode = jd9365t_EraseSector((uint32_t)(i << 12));
				if (ReCode < 0) {
					JD_E("%s: Erase sector timeout error\n", __func__);
					return ReCode;
				} else {
					i++;
					JD_D("%s: EraseStart %08x\n", __func__, (uint32_t)(i << 12));
					JD_D("%s: Size 4KB\n", __func__);
					JD_D("%s: Erase KB block finish\n", __func__);
				}
			}
		}
	}

	return ReCode;
}

static bool jd9365t_GetSectionFirstValueStatus(uint16_t usTimeOut)
{
	uint32_t i = 0;
	uint8_t rbuf[JD_TWO_SIZE];
	bool section_first_value_status_done = false;

	do
	{
		/* Pulling section ready == JD9365T_SECTION_INFO_READY_VALUE */
		mdelay(2);
		if (jd9365t_ReadRegMulti(g_jd_section_info.section_info_ready_addr, rbuf, sizeof(rbuf)) < 0) {
			JD_E("%s: Read section info fail\n", __func__);
			return false;
		} else {
			i += 2;
		}

		if (((rbuf[1] << 8) + rbuf[0]) == JD9365T_SECTION_INFO_READY_VALUE) {
			section_first_value_status_done = true;
		} else {
			section_first_value_status_done = false;
		}
	} while ((i < usTimeOut) && !section_first_value_status_done);

	if (i >= usTimeOut) {
		JD_E("%s: Get section status timeout\n", __func__);
		return false;
	}

	return section_first_value_status_done;
}

static int jd9365t_GetSectionInfo(uint32_t start_addr, uint8_t section_num, struct JD9365T_INFO_CONTENT *info_content)
{
	int ReCode, i;
	uint8_t info_content_size = (uint8_t)sizeof(struct JD9365T_INFO_CONTENT);
	uint8_t rbuf[section_num * info_content_size];

	ReCode = jd9365t_ReadRegMulti(start_addr, rbuf, sizeof(rbuf));
	if (ReCode < 0) {
		JD_E("%s: Read section info fail\n", __func__);
		return ReCode;
	}

	if (info_content_size == 8) {
		for (i = 0; i < section_num; i++) {
			info_content[i].info_content_addr = (uint32_t)(rbuf[i * info_content_size + 0] +
														(rbuf[i * info_content_size + 1] << 8) +
														(rbuf[i * info_content_size + 2] << 16) +
														(rbuf[i * info_content_size + 3] << 24));

			info_content[i].info_content_len = (uint32_t)(rbuf[i * info_content_size + 4] +
														(rbuf[i * info_content_size + 5] << 8) +
														(rbuf[i * info_content_size + 6] << 16) +
														(rbuf[i * info_content_size + 7] << 24));
		}
	} else {
		JD_E("%s: info_content_size was not 8, must to check flow\n", __func__);
	}

	return ReCode;
}

static void jd9365t_ReadSectionInfo(bool reinit_config)
{
	int ReCode, i;
	struct JD9365T_INFO_CONTENT dsram_info_content[JD9365T_MAX_DSRAM_NUM];
	struct JD9365T_INFO_CONTENT esram_info_content[JD9365T_MAX_ESRAM_NUM];
	uint8_t rbuf[JD_SIX_SIZE];

	/* 1. Get section ready */
	if (jd9365t_GetSectionFirstValueStatus(50) == true) {
		/* 2. Get section address */
		jd9365t_GetSectionInfo(g_jd_section_info.dsram_section_info_start_addr,
								JD9365T_PSRAM_SORT_PARA + 1, &dsram_info_content[0]);

		jd9365t_GetSectionInfo(g_jd_section_info.esram_section_info_start_addr,
								JD9365T_ESRAM_COORDINATE_REPORT + 1, &esram_info_content[0]);

		/* 2.1 Dump section info */
		for (i = 0; i <= JD9365T_PSRAM_SORT_PARA; i++) {
			/* Mapping to enum JD9365T_DSRAM_SECTION_INFO_ORDER */
			JD_D("%s: Dsram section(%d) Address: 0x%04x, Length: %d\n", __func__, i,
				dsram_info_content[i].info_content_addr,
				dsram_info_content[i].info_content_len);
		}

		for (i = 0; i <= JD9365T_ESRAM_COORDINATE_REPORT; i++) {
			/* Mapping to enum JD9365T_ESRAM_SECTION_INFO_ORDER */
			JD_D("%s: Esram section(%d) Address: 0x%04x, Length: %d\n", __func__, i,
				esram_info_content[i].info_content_addr,
				esram_info_content[i].info_content_len);
		}

		/* 3.1 Set Coordinate addr. and length */
		g_jd9365t_chip_info.esram_info_content_addr.coordinate_report =
			esram_info_content[JD9365T_ESRAM_COORDINATE_REPORT].info_content_addr;
		pjadard_report_data->touch_data_size = esram_info_content[JD9365T_ESRAM_COORDINATE_REPORT].info_content_len;

		/* 3.2 Set ADC number and resolution */
		ReCode = jd9365t_ReadRegMulti(dsram_info_content[JD9365T_DSRAM_FW_CFG].info_content_addr,
										rbuf, sizeof(rbuf));
		if (ReCode < 0) {
			JD_E("%s: Update global config fail\n", __func__);
		} else {
			/* Update global config */
			if ((pjadard_ic_data) && (pjadard_ts_data) && (pjadard_ts_data->pdata)) {
				pjadard_ic_data->JD_X_NUM = rbuf[4];
				pjadard_ic_data->JD_Y_NUM = rbuf[5];
				pjadard_ts_data->pdata->abs_x_min = 0;
				pjadard_ts_data->pdata->abs_x_max = (int)((rbuf[1] << 8) + rbuf[0]);
				pjadard_ts_data->pdata->abs_y_min = 0;
				pjadard_ts_data->pdata->abs_y_max = (int)((rbuf[3] << 8) + rbuf[2]);
				pjadard_ic_data->JD_X_RES = pjadard_ts_data->pdata->abs_x_max;
				pjadard_ic_data->JD_Y_RES = pjadard_ts_data->pdata->abs_y_max;

				if (reinit_config) {
#ifdef CONFIG_TOUCHSCREEN_JADARD_DEBUG
					if (jd_diag_mutual) {
						kfree(jd_diag_mutual);
						jd_diag_mutual = NULL;
					}

					/* Reallocate mutual data memory */
					jd_diag_mutual = kzalloc(pjadard_ic_data->JD_X_NUM *
											pjadard_ic_data->JD_Y_NUM * sizeof(int), GFP_KERNEL);
					if (jd_diag_mutual == NULL) {
						JD_E("%s: mutual buffer allocate failed\n", __func__);
					}
#endif
					/* Touch input config reinit */
					input_unregister_device(pjadard_ts_data->input_dev);

					if (jadard_input_register(pjadard_ts_data)) {
						JD_E("%s: Unable to register %s input device\n",
						  __func__, pjadard_ts_data->input_dev->name);
					}
				}
			} else {
				JD_E("%s: Can`t update global config, pjadard_ic_data/pjadard_ts_data/pdata was null\n", __func__);
			}

			/* Record JD9365T_DSRAM_HOST_ADDR */
			g_jd9365t_chip_info.dsram_host_addr.fw_cid_version =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 12;
			g_jd9365t_chip_info.dsram_host_addr.fw_version =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 16;
			g_jd9365t_chip_info.dsram_host_addr.game_mode_en =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 30;
			g_jd9365t_chip_info.dsram_host_addr.usb_en =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 32;
			g_jd9365t_chip_info.dsram_host_addr.gesture_en =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 34;
			g_jd9365t_chip_info.dsram_host_addr.high_sensitivity_en =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 36;
			g_jd9365t_chip_info.dsram_host_addr.border_en =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 38;
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
			g_jd9365t_chip_info.dsram_host_addr.mpap_pw =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 40;
			g_jd9365t_chip_info.dsram_host_addr.mpap_handshake =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 42;
			g_jd9365t_chip_info.dsram_host_addr.mpap_keep_frame =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 43;
			g_jd9365t_chip_info.dsram_host_addr.mpap_skip_frame =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 52;
			g_jd9365t_chip_info.dsram_host_addr.mpap_mux_switch =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 53;
			g_jd9365t_chip_info.dsram_host_addr.oscd_on =
				dsram_info_content[JD9365T_DSRAM_HOST].info_content_addr + 54;
			/* Set mapa diff array addr */
			g_jd9365t_chip_info.dsram_host_addr.mpap_diff_array =
				dsram_info_content[JD9365T_DSRAM_MPAP_DIFF_ARRAY].info_content_addr;
#endif
			/* Record JD9365T_DSRAM_DEBUG_ADDR */
			g_jd9365t_chip_info.dsram_debug_addr.output_data_addr =
				dsram_info_content[JD9365T_DSRAM_DEBUG].info_content_addr + 28;
			g_jd9365t_chip_info.dsram_debug_addr.output_data_sel =
				dsram_info_content[JD9365T_DSRAM_DEBUG].info_content_addr + 34;
			g_jd9365t_chip_info.dsram_debug_addr.output_data_handshake =
				dsram_info_content[JD9365T_DSRAM_DEBUG].info_content_addr + 36;
			g_jd9365t_chip_info.dsram_debug_addr.freq_band =
				dsram_info_content[JD9365T_DSRAM_DEBUG].info_content_addr + 40;

			/* Get esram_output_buf.output_buf.data address */
			ReCode = jd9365t_ReadRegMulti(g_jd9365t_chip_info.dsram_debug_addr.output_data_addr, rbuf, 4);
			if (ReCode < 0) {
				JD_E("%s: Get output buf data fail\n", __func__);
			} else {
				g_jd9365t_chip_info.dsram_debug_addr.output_data_addr = ( (rbuf[3] << 24) |
																		   (rbuf[2] << 16) |
																		   (rbuf[1] <<  8) |
																		   (rbuf[0]) );
			}

			JD_I("%s: Section info was ready\n", __func__);
		}
	} else {
		JD_E("%s: Section info was not ready\n", __func__);
	}
}

static int jd9365t_ClearSectionFirstValue(void)
{
	return jd9365t_WriteRegSingle(g_jd_section_info.section_info_ready_addr, 0x00);
}

static int jd9365t_SetCRCInitial(void)
{
	uint8_t wBuf[JD_ONE_SIZE];

	wBuf[0] = 0x01;
	return jd9365t_WriteRegMulti((uint32_t)JD9365T_FLASH_REG_ADDR_CRC_INIT, wBuf, sizeof(wBuf));
}

static int jd9365t_SetCRCInitialValue(void)
{
	uint8_t wBuf[JD_TWO_SIZE];

	wBuf[(uint32_t)JD9365T_CRC_CODE_POSITION_HIGH_BYTE] =
		(uint8_t)((JD9365T_CRC_INFO_CRC_INITIAL_VALUE & 0xFF00) >> 8);
	wBuf[(uint32_t)JD9365T_CRC_CODE_POSITION_LOW_BYTE] =
		(uint8_t)((JD9365T_CRC_INFO_CRC_INITIAL_VALUE & 0x00FF) >> 0);

	return jd9365t_WriteRegMulti((uint32_t)JD9365T_FLASH_REG_ADDR_CRC_INIT_CODE1, wBuf, sizeof(wBuf));
}

static int jd9365t_SetCRCEnable(bool enable)
{
	return jd9365t_WriteRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_CRC_ENABLE, enable);
}

static int jd9365t_GetCRCResult(uint16_t *crc)
{
	int ReCode;
	uint8_t rBuf[JD_TWO_SIZE];

	ReCode = jd9365t_ReadRegMulti((uint32_t)JD9365T_FLASH_REG_ADDR_CRC_CHK1, rBuf, sizeof(rBuf));
	if (ReCode < 0) {
		*crc = 0;
	} else {
		*crc = (uint16_t)((rBuf[(uint32_t)JD9365T_CRC_CODE_POSITION_HIGH_BYTE] << 8) +
							rBuf[(uint32_t)JD9365T_CRC_CODE_POSITION_LOW_BYTE]);
	}

	return ReCode;
}

static void jd9365t_GetDataCRC16(uint8_t *buf, uint32_t buf_len, uint32_t start_index,
									uint32_t len, uint16_t *crc, uint16_t PolynomialCRC16)
{
	uint32_t i, j;
	uint16_t sum = *crc;

	for (i = 0; i < len; i++) {
		if ((start_index + i) >= buf_len) {
			sum ^= (uint16_t)(0xFF << 8);
		} else {
			sum ^= (uint16_t)(buf[start_index + i] << 8);
		}

		for (j = 0; j < 8; j++) {
			if ((sum & 0x8000) == 0x8000)
				sum = (uint16_t)((sum << 1) ^ PolynomialCRC16);
			else
				sum <<= 1;
		}
	}

	*crc = sum;
}

static uint32_t jd9365t_Pow(uint8_t base, uint32_t pow)
{
	uint32_t i;
	uint32_t sum = 1;

	if (pow > 0) {
		for (i = 0; i < pow; i++) {
			sum *= base;
		}
	}

	return sum;
}

static int jd9365t_JEDEC_ID(uint8_t *id)
{
	int ReCode;
	uint8_t rdbuf[JD_THREE_SIZE];

	ReCode = jd9365t_ICReadExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_READ_ID, 0, 0, rdbuf, sizeof(rdbuf));
	if (ReCode < 0) {
		JD_E("%s: Read external flash fail\n", __func__);
		return ReCode;
	}

	if (id != NULL) {
		*id = rdbuf[2];
	}

	JD_I("Flash RDID: 0x%02x%02x%02x\n", rdbuf[0], rdbuf[1], rdbuf[2]);

	if ((rdbuf[0] == 0x00) || (rdbuf[0] == 0xFF)) {
		JD_E("%s: Flash was not exist\n", __func__);
	} else {
		JD_I("Flash capacity: %d KB\n", jd9365t_Pow(2, rdbuf[2] - 10));
	}

	/* Read flash status1 status*/
	ReCode = jd9365t_ICReadExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_STATUS_REGISTER1, 0, 0, rdbuf, 1);
	if (ReCode < 0) {
		JD_E("%s: Read external flash status1 fail\n", __func__);
		return ReCode;
	}

	JD_I("Flash Status1: 0x%02x\n", rdbuf[0]);

	/* Read flash status2 status*/
	ReCode = jd9365t_ICReadExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_STATUS_REGISTER2, 0, 0, rdbuf, 1);
	if (ReCode < 0) {
		JD_E("%s: Read external flash status2 fail\n", __func__);
		return ReCode;
	}

	JD_I("Flash Status2: 0x%02x\n", rdbuf[0]);

	return ReCode;
}

static int jd9365t_CheckSize(uint8_t data, uint32_t length)
{
	int ReCode = JD_NO_ERR;

	if (jd9365t_Pow(2, data) < length) {
		ReCode = JD_WRITE_OVERFLOW;
		JD_E("%s: Write size was overflow\n", __func__);
	}

	return ReCode;
}

static int jd9365t_GetDMAStatus(uint16_t usTimeOut)
{
	int ReCode;
	uint32_t i = 0;
	uint8_t usValue = (uint8_t)JD9365T_DMA_RELATED_SETTING_DMA_BUSY;
	bool dma_busy_status_done = false;

	do
	{
		/* Pulling DMA busy = JD9365T_DMA_RELATED_SETTING_DMA_DONE */
		mdelay(2);
		ReCode = jd9365t_ReadRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_DMA_BUSY_OR_START, &usValue);

		if (ReCode < 0) {
			JD_E("%s: Read DMA_BUSY_OR_START register fail\n", __func__);
			return ReCode;
		} else {
			i += 2;
		}

		dma_busy_status_done = jd9365t_DoneStatusIsLow(usValue, (uint8_t)JD9365T_DMA_RELATED_SETTING_DMA_BUSY,
														(uint8_t)JD9365T_DMA_RELATED_SETTING_DMA_DONE);
	} while ((i < usTimeOut) && !dma_busy_status_done);

	if (i >= usTimeOut) {
		JD_E("%s: Get DMA status timeout\n", __func__);
		return JD_TIME_OUT;
	}

	return ReCode;
}

static int jd9365t_GetPageProgramStatus(uint16_t usTimeOut)
{
	int ReCode;
	uint32_t i = 0;
	uint8_t usValue = (uint8_t)JD9365T_DMA_RELATED_SETTING_PAGE_PROGRAM_FLASH;
	bool dma_page_program_status_done = false;

	do
	{
		/* Pulling DMA Page Program = JD9365T_DMA_RELATED_SETTING_PAGE_PROGRAM_DONE */
		mdelay(2);
		ReCode = jd9365t_ReadRegSingle((uint32_t)JD9365T_FLASH_REG_ADDR_DMA_2WFLASHEN_1WRITE_0START, &usValue);

		if (ReCode < 0) {
			JD_E("%s: Read DMA_2WFLASHEN_1WRITE_0START register fail\n", __func__);
			return ReCode;
		} else {
			i += 2;
		}

		dma_page_program_status_done = jd9365t_DoneStatusIsLow(usValue, (uint8_t)JD9365T_DMA_RELATED_SETTING_PAGE_PROGRAM_FLASH,
																(uint8_t)JD9365T_DMA_RELATED_SETTING_PAGE_PROGRAM_DONE);
	} while ((i < usTimeOut) && !dma_page_program_status_done);

	if (i >= usTimeOut) {
		JD_E("%s: Get DMA status timeout\n", __func__);
		return JD_TIME_OUT;
	}

	return ReCode;
}

static void jd9365t_ReArrangePageProgramInfo(uint32_t ori_addr, uint32_t ori_len , uint32_t *page_addr , uint32_t *page_len)
{
	*page_addr = ori_addr - (ori_addr % JD9365T_SIZE_DEF_PAGE_SIZE);
	*page_len = ((ori_len + (ori_addr % JD9365T_SIZE_DEF_PAGE_SIZE)) % JD9365T_SIZE_DEF_PAGE_SIZE) > 0 ?
				((((ori_len + (ori_addr % JD9365T_SIZE_DEF_PAGE_SIZE)) >> 8) + 1) << 8) :
				(ori_len + (ori_addr % JD9365T_SIZE_DEF_PAGE_SIZE));
}

static int jd9365t_SetDMAStart(uint32_t flash_addr, uint32_t dma_size, uint32_t target_addr, uint8_t dma_cmd)
{
	int ReCode;
	uint8_t wBuf[JD_SIX_SIZE + JD_SEVEN_SIZE];

	if (dma_cmd == (uint8_t)JD9365T_DMA_RELATED_SETTING_WRITE_TO_FLASH) {
		ReCode = jd9365t_ICWriteExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_WRITE_ENABLE, 0, 0);
		if (ReCode < 0) {
			JD_E("%s: Set write enable fail\n", __func__);
			return ReCode;
		}
	}

	if ((dma_cmd == (uint8_t)JD9365T_DMA_RELATED_SETTING_READ_FROM_FLASH) ||
		(dma_cmd == (uint8_t)JD9365T_DMA_RELATED_SETTING_WRITE_TO_FLASH)) {
		ReCode = jd9365t_ICSetExFlashCSNOutEnable();
		if (ReCode < 0) {
			JD_E("%s: Set CSN enable fail\n", __func__);
			return ReCode;
		}
	}

	wBuf[0] =  (uint8_t)((flash_addr & 0x000000FF) >> 0);  /* Set flash_addr 0byte */
	wBuf[1] =  (uint8_t)((flash_addr & 0x0000FF00) >> 8);  /* Set flash_addr 1byte */
	wBuf[2] =  (uint8_t)((flash_addr & 0x00FF0000) >> 16); /* Set flash_addr 2byte */
	wBuf[3] =  (uint8_t)((flash_addr & 0xFF000000) >> 24); /* Set flash_addr 3byte */

	wBuf[4] =  (uint8_t)((dma_size & 0x000000FF) >> 0);  /* Set dma_size 0byte */
	wBuf[5] =  (uint8_t)((dma_size & 0x0000FF00) >> 8);  /* Set dma_size 1byte */
	wBuf[6] =  (uint8_t)((dma_size & 0x00FF0000) >> 16); /* Set dma_size 2byte */
	wBuf[7] =  (uint8_t)((dma_size & 0xFF000000) >> 24); /* Set dma_size 3byte */

	wBuf[8] =  (uint8_t)((target_addr & 0x000000FF) >> 0);  /* Set target_addr 0byte */
	wBuf[9] =  (uint8_t)((target_addr & 0x0000FF00) >> 8);  /* Set target_addr 1byte */
	wBuf[10] = (uint8_t)((target_addr & 0x00FF0000) >> 16); /* Set target_addr 2byte */
	wBuf[11] = (uint8_t)((target_addr & 0xFF000000) >> 24); /* Set target_addr 3byte */
	wBuf[12] = dma_cmd; /* dma_1write_0start */

	ReCode = jd9365t_WriteRegMulti((uint32_t)JD9365T_FLASH_REG_ADDR_DMA_FLASH_ADDR0, wBuf, sizeof(wBuf));
	if (ReCode < 0) {
		JD_E("%s: Set DMA parameters fail\n", __func__);
		return ReCode;
	}

	if (dma_cmd == (uint8_t)JD9365T_DMA_RELATED_SETTING_PAGE_PROGRAM_FLASH) {
		/* Pulling DMA Page Program = JD9365T_DMA_RELATED_SETTING_PAGE_PROGRAM_DONE */
		mdelay(1);
		ReCode = jd9365t_GetPageProgramStatus(50);
		if (ReCode < 0) {
			JD_E("%s: Get DMA page program status fail\n", __func__);
			return ReCode;
		}
	} else {
		/* Pulling DMA busy = JD9365T_DMA_RELATED_SETTING_DMA_DONE */
		ReCode = jd9365t_GetDMAStatus(50);
		if (ReCode < 0) {
			JD_E("%s: Get DMA status fail\n", __func__);
			return ReCode;
		}
	}

	if ((dma_cmd == (uint8_t)JD9365T_DMA_RELATED_SETTING_READ_FROM_FLASH) ||
		(dma_cmd == (uint8_t)JD9365T_DMA_RELATED_SETTING_WRITE_TO_FLASH)) {
		ReCode = jd9365t_ICSetExFlashCSNOutDisable();
		if (ReCode < 0) {
			JD_E("%s: Set CSN disable fail\n", __func__);
		}
	}

	return ReCode;
}

static int jd9365t_SetDMAByteMode(void)
{
	uint8_t wBuf[JD_ONE_SIZE];

	wBuf[0] = (uint8_t)JD9365T_DMA_RELATED_SETTING_TRANSFER_DATA_1_BYTE_MODE;

	return jd9365t_WriteRegMulti((uint32_t)JD9365T_FLASH_REG_ADDR_DMA_BYTE_MODE, wBuf, sizeof(wBuf));
}

static uint8_t jd9365t_GetMoveInfoNumber(uint8_t *pFileData)
{
	if (pFileData[JD9365T_HW_HEADER_LEN] == 0xFF) {
		return 0;
	} else {
		return (uint8_t)((pFileData[JD9365T_HW_HEADER_LEN] - JD9365T_HW_HEADER_CRC_NUMBER) /
							JD9365T_HW_HEADER_MOVE_INFO_NUMBER);
	}
}

static void jd9365t_GetHeaderInfo(uint8_t *pFileData, struct JD9365T_MOVE_INFO *move_info, int move_info_sz)
{
	int i;
	int header_shift = JD9365T_HW_HEADER_CRC_NUMBER + JD9365T_HW_HEADER_INITIAL_NUMBER;

	for (i = 0; i < move_info_sz; i++) {
		move_info[i].mov_mem_cmd =
			pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 0];

		move_info[i].fl_st_addr =
			(uint32_t)((pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 1] << 16) +
						(pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 2] << 8) +
						(pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 3] << 0));

		move_info[i].fl_len =
			(uint16_t)((pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 4] << 8) +
						(pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 5] << 0) + 1);

		move_info[i].to_mem_st_addr =
			(uint32_t)((pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 6] << 24) +
						(pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 7] << 16) +
						(pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 8] << 8) +
						(pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 9] << 0));

		move_info[i].to_mem_crc =
			(uint16_t)((pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 10] << 8) +
						(pFileData[header_shift + i * JD9365T_HW_HEADER_MOVE_INFO_NUMBER + 11] << 0));
	}
}

static int jd9365t_ReadFlash(uint32_t addr, uint8_t *rdata, uint32_t rlen)
{
	int ReCode;
	uint16_t packsize = (uint16_t)JD9365T_FLASH_PACK_READ_SIZE;
	uint32_t pos = 0;
	uint16_t packlen = 0;
	/* MCU aram config */
	uint16_t ramsize = 2048;
	uint32_t ramaddr = 0;
	uint16_t ramlen = 0;
	/* DMA config */
	uint32_t flashaddr = addr;
	uint32_t targetaddr = (uint32_t)JD9365T_MEMORY_ADDR_ARAM;
	/* Read buf */
	uint8_t rBuf[packsize];

	/* Set CRC initial value */
	ReCode = jd9365t_SetCRCInitialValue();
	if (ReCode < 0) {
		JD_E("%s: Set CRC initial value fail\n", __func__);
		return ReCode;
	}

	/* Set CRC Initial */
	ReCode = jd9365t_SetCRCInitial();
	if (ReCode < 0) {
		JD_E("%s: Set CRC initial fail\n", __func__);
		return ReCode;
	}

	/* Read start */
	while (rlen > 0) {
		if (rlen > ramsize)
			ramlen = ramsize;
		else
			ramlen = (uint16_t)rlen;

		/* Set next ram addr */
		rlen -= ramlen;
		ramaddr = (uint32_t)JD9365T_MEMORY_ADDR_ARAM;

		/* Set DMA Start & Pulling DMA busy = JD9365T_DMA_RELATED_SETTING_DMA_DONE */
		JD_D("%s: Flash DMA start\n", __func__);

		ReCode = jd9365t_SetDMAStart(flashaddr, ramlen, targetaddr,
										(uint8_t)JD9365T_DMA_RELATED_SETTING_READ_FROM_FLASH);
		if (ReCode < 0) {
			JD_E("%s: DMA start error\n", __func__);
			return ReCode;
		} else {
			JD_D("%s: DMA start finish\n", __func__);
		}

		/* Read from ram */
		while (ramlen > 0) {
			if (ramlen > packsize)
				packlen = packsize;
			else
				packlen = (uint16_t)ramlen;

			/* Read Data from ARAM */
			ReCode = jd9365t_ReadRegMulti(ramaddr, rBuf, packlen);
			if (ReCode < 0) {
				JD_E("%s: Read ARAM data fail\n", __func__);
				return ReCode;
			} else {
				memcpy(rdata + pos, rBuf, packlen);
			}

			/* Set next packet */
			ramlen -= packlen;
			pos += packlen;
			ramaddr += packlen;
			flashaddr += packlen;
		}

		JD_D("%s: Read flash data %d KB...\n", __func__, (uint16_t)((flashaddr - addr) / 1024));
	}

	return ReCode;
}

static int jd9365t_PageProgramFlash(uint32_t addr, uint32_t wlen, uint8_t *data, uint32_t data_len)
{
	int ReCode, i;
	uint16_t packsize = (uint16_t)JD9365T_FLASH_PACK_WRITE_SIZE;
	uint32_t pos = addr;
	uint16_t packlen = 0;
	/* MCU aram config */
	uint16_t ramsize = 2048;
	uint32_t ramaddr = 0;
	uint16_t ramlen = 0;
	/* DMA config */
#if defined(CONFIG_TOUCHSCREEN_JADARD_DEBUG)
	uint32_t start_flashaddr = addr;
#endif
	uint32_t flashaddr = 0;
	uint16_t dmasize = 0;
	uint32_t targetaddr = (uint32_t)JD9365T_MEMORY_ADDR_ARAM;
	/* Write buf */
	uint8_t wBuf[packsize];

	/* Set CRC initial value */
	ReCode = jd9365t_SetCRCInitialValue();
	if (ReCode < 0) {
		JD_E("%s: Set CRC initial value fail\n", __func__);
		return ReCode;
	}

	/* Set CRC Initial */
	ReCode = jd9365t_SetCRCInitial();
	if (ReCode < 0) {
		JD_E("%s: Set CRC initial fail\n", __func__);
		return ReCode;
	}

	/* Write start */
	while (wlen > 0) {
		if (wlen > ramsize)
			ramlen = ramsize;
		else
			ramlen = (uint16_t)wlen;

		flashaddr = addr;
		dmasize = ramlen;
		/* Set next ram addr */
		wlen -= ramlen;
		ramaddr = (uint32_t)JD9365T_MEMORY_ADDR_ARAM;

		/* Write to ram */
		while (ramlen > 0) {
			if (ramlen > packsize)
				packlen = packsize;
			else
				packlen = (uint16_t)ramlen;

			for (i = 0; i < packlen; i++) {
				if ((pos + i) >= data_len)
					wBuf[i] = 0xFF;
				else
					wBuf[i] = data[pos + i];
			}

			/* Write Data to ARAM */
			ReCode = jd9365t_WriteRegMulti(ramaddr, wBuf, packlen);
			if (ReCode < 0) {
				JD_E("%s: Write data to ARAM fail\n", __func__);
				return ReCode;
			}

			/* Set next packet */
			ramlen -= packlen;
			pos += packlen;
			ramaddr += packlen;
			addr += packlen;
		}

		/* Set DMA Start & Pulling DMA busy = JD9365T_DMA_RELATED_SETTING_DMA_DONE */
		ReCode = jd9365t_SetDMAStart(flashaddr, dmasize, targetaddr,
										(uint8_t)JD9365T_DMA_RELATED_SETTING_PAGE_PROGRAM_FLASH);
		if (ReCode < 0) {
			JD_E("%s: DMA start error\n", __func__);
			return ReCode;
		} else {
			JD_D("%s: Flash DMA start\n", __func__);
		}

		/* Read Ex-Flash status until done */
		// ReCode = jd9365t_ICGetExFlashStatus(50);
		// if (ReCode < 0) {
			// E("%s: Read Ex-Flash status fail\n", __func__);
			// return ReCode;
		// } else {
			// D("%s: DMA status ok\n", __func__);
		// }

		JD_D("%s: Write flash data %d KB...\n", __func__, (uint16_t)((flashaddr - start_flashaddr) / 1024));
	}

	ReCode = jd9365t_ICWriteExFlashFlow((uint8_t)JD9365T_EX_FLASH_ADDR_WRITE_DISABLE, 0, 0);
	if (ReCode < 0) {
		JD_E("%s: Write flash disable fail\n", __func__);
	}

	return ReCode;
}

static int jd9365t_CheckFlashContent(uint8_t *pFileData, uint32_t addr, uint32_t len)
{
	int ReCode, i;
	uint32_t error_count = 0;
	uint8_t *pData = kzalloc(len * sizeof(uint8_t), GFP_KERNEL);

	ReCode = jd9365t_ReadFlash(addr, pData, len);
	if (ReCode < 0) {
		kfree(pData);
		return ReCode;
	}

	for (i = 0; i < len; i++) {
		if (pFileData[addr + i] != pData[i]) {
			JD_E("%s: Error data[%d]=0x%02x, Current data[%d]=0x%02x\n", __func__, i, pData[i], i, pFileData[addr + i]);
			error_count++;
			if (error_count > 5) {
				kfree(pData);
				return ReCode;
			}
		}
	}

	kfree(pData);
	return ReCode;
}

static int jd9365t_HostReadFlash(uint32_t ReadAddr, uint8_t *pDataBuffer, uint32_t ReadLen)
{
	int ReCode;
	uint16_t romid = 0;
	uint16_t crc, softcrc;

	/* 1. Enter backdoor & 2. Read IC ID */
	ReCode = jd9365t_EnterBackDoor(&romid);
	if (ReCode < 0)
		return ReCode;

	/* 3.1 IC RTC run off */
	ReCode = jd9365t_DisableRTCRun();
	if (ReCode < 0)
		return ReCode;

	/* 3.2 IC touch scan off */
	ReCode = jd9365t_DisableTouchScan();
	if (ReCode < 0)
		return ReCode;

	/* 3.3 Set DMA 1-byte mode */
	ReCode = jd9365t_SetDMAByteMode();
	if (ReCode < 0)
		return ReCode;

	/* 4. Stop MCU clock */
	ReCode = jd9365t_StopMCUClock();
	if (ReCode < 0) {
		return ReCode;
	}

	/* 5. Set Flash SPI speed & 6. Read Flash id */
	ReCode = jd9365t_JEDEC_ID(NULL);
	if (ReCode < 0) {
		return ReCode;
	}

	/* 7. Read data from flash */
	JD_I("%s: Read flash start\n", __func__);
	ReCode = jd9365t_ReadFlash(ReadAddr, pDataBuffer, ReadLen);
	if (ReCode < 0) {
		JD_E("%s: Read flash error\n", __func__);
		return ReCode;
	} else {
		JD_I("%s: Read flash finish\n", __func__);
	}

	/* 8-1. Get IC crc */
	ReCode = jd9365t_GetCRCResult(&crc);
	if (ReCode < 0) {
		JD_E("%s: Read flash ecc fail\n", __func__);
		return ReCode;
	}

	/* 8-2. Calculate software crc */
	softcrc = JD9365T_CRC_INFO_CRC_INITIAL_VALUE;
	jd9365t_GetDataCRC16(pDataBuffer, ReadLen, 0, ReadLen, &softcrc, JD9365T_CRC_INFO_PolynomialCRC16);

	/* 8-3. Compare IC and software crc */
	if (softcrc != crc) {
		JD_E("%s: softEcc is %04x, but read IC crc is %04x\n", __func__, softcrc, crc);
	}

	/* 9.1 IC RTC run on */
	ReCode = jd9365t_EnableRTCRun();
	if (ReCode < 0)
		return ReCode;

	/* 9.2 IC touch scan on */
	ReCode = jd9365t_EnableTouchScan();
	if (ReCode < 0)
		return ReCode;

	/* 9.3 start MCU clock */
	ReCode = jd9365t_StartMCUClock();
	if (ReCode < 0)
		return ReCode;

	return ReCode;
}

static int jd9365t_HostWriteFlash(uint32_t WriteAddr, uint8_t *pFileData, uint32_t FileSize)
{
	int ReCode;
	uint32_t PageProgram_addr = 0;
    uint32_t PageProgram_len = 0;
	uint16_t romid = 0;
	uint16_t crc, softcrc;
	uint8_t id;
	uint8_t MoveInfoNumber;
	struct JD9365T_MOVE_INFO *MoveInfo = NULL;

	/* 0. Check WriteAddr is a multiple of 256 */
	if (WriteAddr % JD9365T_SIZE_DEF_PAGE_SIZE != 0) {
		JD_E("%s: WriteAddr: 0x%08x was not a multiple of 256\n", __func__, WriteAddr);
		return JD_CHECK_DATA_ERROR;
	}

	/* 1. Enter backdoor & 2. Read IC ID */
	ReCode = jd9365t_EnterBackDoor(&romid);
	if (ReCode < 0)
		return ReCode;

	/* 3.1 IC RTC run off */
	ReCode = jd9365t_DisableRTCRun();
	if (ReCode < 0)
		return ReCode;

	/* 3.2 IC touch scan off */
	ReCode = jd9365t_DisableTouchScan();
	if (ReCode < 0)
		return ReCode;

	/* 3.3 Set DMA 1-byte mode */
	ReCode = jd9365t_SetDMAByteMode();
	if (ReCode < 0)
		return ReCode;

	/* 3.4 Clear section first value */
	ReCode = jd9365t_ClearSectionFirstValue();
	if (ReCode < 0)
		return ReCode;

	/* 4. Stop MCU clock */
	ReCode = jd9365t_StopMCUClock();
	if (ReCode < 0) {
		return ReCode;
	}

	/* 5. Set Flash SPI speed & Read Flash id */
	ReCode = jd9365t_JEDEC_ID(&id);
	if (ReCode < 0) {
		return ReCode;
	}

	/* 6. Check flash size is enough */
	ReCode = jd9365t_CheckSize(id, FileSize);
	if (ReCode < 0) {
		return ReCode;
	}

	/* 7. Erase flash */
	JD_I("%s: Erase flash start, Start Address: 0x%08x, Size: %d\n", __func__, WriteAddr, FileSize);
	ReCode = jd9365t_EraseChipFlash(WriteAddr, FileSize);
	if (ReCode < 0) {
		JD_E("%s: Flash erase fail\n", __func__);
		return ReCode;
	} else {
		JD_I("%s: Erase Flash completed\n", __func__);
	}

	/* 8. Set CRC disable */
	ReCode =  jd9365t_SetCRCEnable(false);
	if (ReCode < 0) {
		JD_E("%s: CRC disable fail\n", __func__);
		return ReCode;
	}

	/* 9. Set Flash SPI speed */
	ReCode = jd9365t_SetFlashSPISpeed(pFileData[JD9365T_LOAD_CODE_SPI_FREQ]);
	if (ReCode < 0) {
		return ReCode;
	}

	/* 10. Get header info */
	MoveInfoNumber = jd9365t_GetMoveInfoNumber(pFileData);
	if (MoveInfoNumber == 0) {
		JD_E("%s: Header information error, please check FW bin!\n", __func__);
		return JD_CHECK_DATA_ERROR;
	}

	MoveInfo = kzalloc(MoveInfoNumber * sizeof(struct JD9365T_MOVE_INFO), GFP_KERNEL);
	if (MoveInfo == NULL) {
		JD_E("%s: Memory allocate fail\n", __func__);
		return JD_MEM_ALLOC_FAIL;
	}
	jd9365t_GetHeaderInfo(pFileData, MoveInfo, MoveInfoNumber);

	/* 10-1. Write data to flash */
	JD_I("%s: Write flash start\n", __func__);
	jd9365t_ReArrangePageProgramInfo(WriteAddr, (uint32_t)(JD9365T_HW_HEADER_MAX_SIZE + MoveInfo[0].fl_len),
									&PageProgram_addr, &PageProgram_len);
	ReCode = jd9365t_PageProgramFlash(PageProgram_addr, PageProgram_len, pFileData, FileSize);
	kfree(MoveInfo);
	if (ReCode < 0) {
		JD_E("%s: Write flash fail\n", __func__);
		return ReCode;
	} else {
		JD_I("%s: Write flash finish\n", __func__);
	}

	/* 11-1. Get IC crc */
	ReCode = jd9365t_GetCRCResult(&crc);
	if (ReCode < 0) {
		JD_E("%s: Read flash ecc fail\n", __func__);
		return ReCode;
	}

	/* 11-2. calculate software crc */
	softcrc = JD9365T_CRC_INFO_CRC_INITIAL_VALUE;
	jd9365t_GetDataCRC16(pFileData, FileSize, PageProgram_addr, PageProgram_len, &softcrc, JD9365T_CRC_INFO_PolynomialCRC16);

	/* 11-3. Compare IC and software crc */
	if (softcrc != crc) {
		JD_E("%s: softEcc is %04x, but read IC crc is %04x\n", __func__, softcrc, crc);
	} else {
		JD_I("%s: CRC check pass\n", __func__);
	}

	/* 11-4. Write data to flash2 */
	JD_I("%s: Write flash start\n", __func__);
	jd9365t_ReArrangePageProgramInfo(JD9365T_HW_HEADER_MAX_SIZE + JD9365T_PRAM_MAX_SIZE,
									FileSize - (JD9365T_HW_HEADER_MAX_SIZE + JD9365T_PRAM_MAX_SIZE),
									&PageProgram_addr, &PageProgram_len);
	ReCode = jd9365t_PageProgramFlash(PageProgram_addr, PageProgram_len, pFileData, FileSize);
	if (ReCode < 0) {
		JD_E("%s: Write flash2 fail\n", __func__);
		return ReCode;
	} else {
		JD_I("%s: Write flash2 finish\n", __func__);
	}

	/* 11-5. Get IC crc */
	ReCode = jd9365t_GetCRCResult(&crc);
	if (ReCode < 0) {
		JD_E("%s: Read flash2 ecc fail\n", __func__);
		return ReCode;
	}

	/* 11-6. calculate software crc */
	softcrc = JD9365T_CRC_INFO_CRC_INITIAL_VALUE;
	jd9365t_GetDataCRC16(pFileData, FileSize, PageProgram_addr, PageProgram_len, &softcrc, JD9365T_CRC_INFO_PolynomialCRC16);

	/* 11-7. Compare IC and software crc-2 */
	if (softcrc != crc) {
		JD_E("%s: softEcc-2 is %04x, but read IC crc-2 is %04x\n", __func__, softcrc, crc);
	} else {
		JD_I("%s: CRC-2 check pass\n", __func__);
	}

	/* 12. Read back Flash content [option] */
	ReCode = jd9365t_CheckFlashContent(pFileData, 0, 256);
	if (ReCode < 0)
		return ReCode;

	/* 13. Reset IC touch soc */
	ReCode = jd9365t_ResetSOC();
	if (ReCode < 0) {
		JD_E("%s: Reset touch soc fail\n", __func__);
		return ReCode;
	} else {
		JD_I("%s: Reset touch soc\n", __func__);
	}

	/* Wait flash reload to Pram */
	msleep(100);

	/* 14. Enter back door & read IC id */
	ReCode = jd9365t_EnterBackDoor(&romid);
	if (ReCode < 0)
		return ReCode;

	/* 15. Set Flash SPI speed */
	ReCode = jd9365t_SetFlashSPISpeed(pFileData[JD9365T_LOAD_CODE_SPI_FREQ]);
	if (ReCode < 0) {
		return ReCode;
	}

	/* 16. Reload section info. */
	/*hs03s code for DEVAL5626-1004 by huangzhongjie at 20220720 start*/
	jd9365t_ReadSectionInfo(true);
	/*hs03s code for DEVAL5626-1004 by huangzhongjie at 20220720 end*/

	return ReCode;
}

#ifdef JD_ZERO_FLASH
static int jd9365t_ResetMCU(void)
{
	int ReCode;

	ReCode = jd9365t_WriteRegSingle((uint32_t)JD9365T_SOC_REG_ADDR_CPU_SOFT_RESET,
										(uint8_t)JD9365T_SOC_PASSWORD_MCU_RESET);

	if (ReCode < 0) {
		JD_E("%s: Reset MCU fail\n", __func__);
	}

	return ReCode;
}

static int jd9365t_WritePram(uint32_t addr, uint8_t *pData, uint32_t len)
{
	int ReCode;
	uint16_t packsize = (uint16_t)JD9365T_FLASH_PACK_WRITE_SIZE;
	uint32_t pos = 0;
	uint16_t packlen = 0;
	/* MCU aram config */
	uint32_t ramaddr = addr;
	uint32_t ramlen = len;

	/* Write start */
	if (len > 0) {
		JD_I("%s: Start write ram addr: 0x%08x\n", __func__, ramaddr);
		JD_I("%s: Write pram data %d bytes\n", __func__, ramlen);

		/* Write to ram */
		while (ramlen > 0) {
			if (ramlen > packsize)
				packlen = packsize;
			else
				packlen = (uint16_t)ramlen;

			/* Write Data to PRAM/ARAM */
			ReCode = jd9365t_WriteRegMulti(ramaddr, pData + pos, packlen);
			if (ReCode < 0) {
				JD_E("%s: Write data to PRAM/ARAM fail\n", __func__);
				return ReCode;
			}

			/* Set next packet */
			ramlen -= packlen;
			pos += packlen;
			ramaddr += packlen;
		}
	}

	/* Set CRC initial value */
	ReCode = jd9365t_SetCRCInitialValue();
	if (ReCode < 0) {
		JD_E("%s: Set CRC initial value fail\n", __func__);
		return ReCode;
	}

	/* Set CRC Initial */
	ReCode = jd9365t_SetCRCInitial();
	if (ReCode < 0) {
		JD_E("%s: Set CRC initial fail\n", __func__);
		return ReCode;
	}

	/* Set DMA Start & Pulling DMA busy = JD9365T_DMA_RELATED_SETTING_DMA_DONE */
	ReCode = jd9365t_SetDMAStart(addr, len, addr, (uint8_t)JD9365T_DMA_RELATED_SETTING_WRITE_TO_PRAM);
	if (ReCode < 0) {
		JD_E("%s: DMA start error\n", __func__);
		return ReCode;
	} else {
		JD_I("%s: DMA start finish\n", __func__);
	}

	return ReCode;
}

static int jd9365t_HostWritePram(uint32_t WriteAddr, uint8_t *pFileData, uint32_t FileSize)
{
	int ReCode, i;
	uint16_t romid = 0;
	uint16_t crc, softcrc;
	uint8_t MoveInfoNumber;
	struct JD9365T_MOVE_INFO *MoveInfo = NULL;

	/* 1. Enter backdoor & 2. Read IC ID */
	ReCode = jd9365t_EnterBackDoor(&romid);
	if (ReCode < 0)
		return ReCode;

	/* 3.1 IC RTC run off */
	ReCode = jd9365t_DisableRTCRun();
	if (ReCode < 0)
		return ReCode;

	/* 3.2 IC touch scan off */
	ReCode = jd9365t_DisableTouchScan();
	if (ReCode < 0)
		return ReCode;

	/* 3.3 Set DMA 1-byte mode */
	ReCode = jd9365t_SetDMAByteMode();
	if (ReCode < 0)
		return ReCode;

	/* 3.4 Clear section first value */
	ReCode = jd9365t_ClearSectionFirstValue();
	if (ReCode < 0)
		return ReCode;

	/* 4. Stop MCU */
	ReCode = jd9365t_StopMCU();
	if (ReCode < 0) {
		return ReCode;
	}

	/* 5. Stop MCU clock */
	ReCode = jd9365t_StopMCUClock();
	if (ReCode < 0) {
		return ReCode;
	}

	/* 6. Get header info.*/
	MoveInfoNumber = jd9365t_GetMoveInfoNumber(pFileData);
	MoveInfo = kzalloc(MoveInfoNumber * sizeof(struct JD9365T_MOVE_INFO), GFP_KERNEL);

	if (MoveInfo == NULL) {
		JD_E("%s: Memory allocate fail\n", __func__);
		return JD_MEM_ALLOC_FAIL;
	}

	jd9365t_GetHeaderInfo(pFileData, MoveInfo, MoveInfoNumber);

	/* 7. Write data to PRAM/DRAM */
	JD_I("%s: Write ram start\n", __func__);

	for (i = 0; i < MoveInfoNumber; i++) {
		if ((MoveInfo[i].fl_st_addr + MoveInfo[i].fl_len) > FileSize) {
			JD_E("%s: Write ram overflow\n", __func__);
			kfree(MoveInfo);
			return ReCode;
		}

		ReCode = jd9365t_WritePram(MoveInfo[i].to_mem_st_addr, pFileData + MoveInfo[i].fl_st_addr, MoveInfo[i].fl_len);
		if (ReCode < 0) {
			JD_E("%s: Write pram fail\n", __func__);
			kfree(MoveInfo);
			return ReCode;
		}

		/* 8-1. Get IC crc */
		ReCode = jd9365t_GetCRCResult(&crc);
		if (ReCode < 0) {
			JD_E("%s: Read crc fail\n", __func__);
			kfree(MoveInfo);
			return ReCode;
		}

		/* 8-2. Check IC crc */
		if (crc != MoveInfo[i].to_mem_crc) {
			JD_E("%s: Binary crc is %04x, but read IC crc is %04x\n", __func__, MoveInfo[i].to_mem_crc, crc);
			kfree(MoveInfo);
			return ReCode;
		} else {
			JD_I("%s: HW CRC check pass\n", __func__);
		}

		/* 8-3. Calculate software crc */
		softcrc = JD9365T_CRC_INFO_CRC_INITIAL_VALUE;
		jd9365t_GetDataCRC16(pFileData + MoveInfo[i].fl_st_addr, MoveInfo[i].fl_len, 0, MoveInfo[i].fl_len, &softcrc, JD9365T_CRC_INFO_PolynomialCRC16);

		/* 8-4. check software crc */
		if (softcrc != MoveInfo[i].to_mem_crc) {
			JD_E("%s: Binary crc is %04x, but soft crc is %04x\n", __func__, MoveInfo[i].to_mem_crc, softcrc);
			kfree(MoveInfo);
			return ReCode;
		} else {
			JD_I("%s: Software CRC check pass\n", __func__);
		}
	}

	kfree(MoveInfo);
	JD_I("%s: Write ram finish\n", __func__);

	/* 9. Software reset MCU */
	ReCode = jd9365t_ResetMCU();
	if (ReCode < 0) {
		return ReCode;
	} else {
		JD_I("%s: Reset touch mcu\n", __func__);
	}

	/* 10. Start MCU clock */
	ReCode = jd9365t_StartMCUClock();
	if (ReCode < 0) {
		return ReCode;
	} else {
		JD_I("%s: Start MCU clock\n", __func__);
	}

	/* 11. start MCU */
	ReCode = jd9365t_StartMCU();
	if (ReCode < 0) {
		return ReCode;
	} else {
		JD_I("%s: Start MCU\n", __func__);
	}

	/* 12. Reload section info. */
	/*hs03s code for DEVAL5626-1004 by huangzhongjie at 20220720 start*/
	jd9365t_ReadSectionInfo(false);
	/*hs03s code for DEVAL5626-1004 by huangzhongjie at 20220720 end*/

	return ReCode;
}
#endif

static int jd9365t_ReadPram(uint32_t addr, uint8_t *pData, uint32_t len)
{
	int ReCode;
	uint16_t packsize = (uint16_t)JD9365T_FLASH_PACK_READ_SIZE;
	uint32_t pos = 0;
	uint16_t packlen = 0;
	/* MCU ram config */
	uint32_t ramaddr = addr;
	uint32_t ramlen = len;
	/* Read buf */
	uint8_t rBuf[packsize];

	/* Read start */
	if (len > 0) {
		JD_I("%s: Start read ram addr: 0x%08x\n", __func__, ramaddr);
		JD_I("%s: Read ram data %d bytes\n", __func__, ramlen);

		/* Read from ram */
		while (ramlen > 0) {
			if (ramlen > packsize)
				packlen = packsize;
			else
				packlen = (uint16_t)ramlen;

			/* Read Data from ram */
			ReCode = jd9365t_ReadRegMulti(ramaddr, rBuf, packlen);
			if (ReCode < 0) {
				JD_E("%s: Read ram data fail\n", __func__);
				return ReCode;
			} else {
				memcpy(pData + pos, rBuf, packlen);
			}

			/* Set next packet */
			ramlen -= packlen;
			pos += packlen;
			ramaddr += packlen;
		}
	}

	/* Set CRC initial value */
	ReCode = jd9365t_SetCRCInitialValue();
	if (ReCode < 0) {
		JD_E("%s: Set CRC initial value fail\n", __func__);
		return ReCode;
	}

	/* Set CRC Initial */
	ReCode = jd9365t_SetCRCInitial();
	if (ReCode < 0) {
		JD_E("%s: Set CRC initial fail\n", __func__);
		return ReCode;
	}

	/* Set DMA Start & Pulling DMA busy = JD9365T_DMA_RELATED_SETTING_DMA_DONE */
	ReCode = jd9365t_SetDMAStart(addr, len, addr, (uint8_t)JD9365T_DMA_RELATED_SETTING_WRITE_TO_PRAM);
	if (ReCode < 0) {
		JD_E("%s: DMA start error\n", __func__);
		return ReCode;
	} else {
		JD_I("%s: DMA start finish\n", __func__);
	}

	return ReCode;
}

static int jd9365t_HostReadPram(uint32_t ReadAddr, uint8_t *pFileData, uint32_t ReadLen)
{
	int ReCode;
	uint16_t romid = 0;
	uint16_t crc, softcrc;

	/* 1. Enter backdoor & 2. Read IC ID */
	ReCode = jd9365t_EnterBackDoor(&romid);
	if (ReCode < 0)
		return ReCode;

	/* 3.1 Set DMA 1-byte mode */
	ReCode = jd9365t_SetDMAByteMode();
	if (ReCode < 0)
		return ReCode;

	/* 3.2 Stop MCU */
	ReCode = jd9365t_StopMCU();
	if (ReCode < 0) {
		return ReCode;
	}

	/* 4. Stop MCU clock */
	ReCode = jd9365t_StopMCUClock();
	if (ReCode < 0) {
		return ReCode;
	}

	/* 5. Read data from PRAM/DRAM */
	JD_I("%s: Read ram start\n", __func__);
	ReCode = jd9365t_ReadPram(ReadAddr, pFileData, ReadLen);
	if (ReCode < 0) {
		JD_E("%s: Read ram error\n", __func__);
		return ReCode;
	} else {
		JD_I("%s: Read ram finish\n", __func__);
	}

	/* 6-1. Get IC crc */
	ReCode = jd9365t_GetCRCResult(&crc);
	if (ReCode < 0) {
		JD_E("%s: Read flash ecc fail\n", __func__);
		return ReCode;
	}

	/* 6-2. Calculate software crc */
	softcrc = JD9365T_CRC_INFO_CRC_INITIAL_VALUE;
	jd9365t_GetDataCRC16(pFileData, ReadLen, ReadAddr, ReadLen, &softcrc, JD9365T_CRC_INFO_PolynomialCRC16);

	/* 6-3. Compare IC and software crc */
	if (softcrc != crc) {
		JD_E("%s: softEcc is %04x, but read IC crc is %04x\n", __func__, softcrc, crc);
	}

	/* 7. Start MCU clock */
	ReCode = jd9365t_StartMCUClock();
	if (ReCode >= 0) {
		JD_I("%s: Start MCU clock\n", __func__);
	}

	/* 8. start MCU */
	ReCode = jd9365t_StartMCU();
	if (ReCode < 0) {
		return ReCode;
	} else {
		JD_I("%s: Start MCU\n", __func__);
	}

	msleep(10);

	return ReCode;
}

static int jd9365t_ReadTpInfoBuf(uint8_t *rdata, uint16_t rlen)
{
	return jd9365t_ReadRegMulti(g_jd9365t_chip_info.esram_info_content_addr.coordinate_report, rdata, rlen);
}

static void module_jd9365t_chip_init(void)
{
	JD_I("%s\n", __func__);
	/* Jadard: Set FW and CFG Flash Address */
	g_common_variable.FW_SIZE        = JD9365T_SIZE;
	g_common_variable.RAM_LEN        = JD9365T_PRAM_MAX_SIZE;
	jd9365t_ChipInfoInit();

#ifndef JD_ZERO_FLASH
	/* Reload section info. */
	jd9365t_ReadSectionInfo(false);
	g_module_fp.fp_read_fw_ver();
#endif
}

static void module_jd9365t_PinReset(void)
{
	jd9365t_PinReset();
}

static void module_jd9365t_SoftReset(void)
{
	/* Reset IC touch soc */
	if (jd9365t_ResetSOC() < 0) {
		JD_E("%s: Reset touch soc fail\n", __func__);
		return;
	} else {
		JD_I("%s: Reset touch soc\n", __func__);
	}

	/* Wait flash reload to Pram, Add Flash */
	msleep(100);

	/* Enter back door */
	jd9365t_EnterBackDoor(NULL);
}

static int module_jd9365t_register_read(uint32_t ReadAddr, uint8_t *ReadData, uint32_t ReadLen)
{
	int ReCode;

	if (ReadLen & 0xFFFF0000) {
		ReCode = JD_READ_LEN_OVERFLOW;
	} else {
		ReCode = jd9365t_ReadRegMulti(ReadAddr, ReadData, (uint16_t)ReadLen);
	}

	return ReCode;
}

static int module_jd9365t_register_write(uint32_t WriteAddr, uint8_t *WriteData, uint32_t WriteLen)
{
	return jd9365t_WriteRegMulti(WriteAddr, WriteData, (uint16_t)WriteLen);
}

static int module_jd9365t_ram_read(uint32_t ReadAddr, uint8_t *ReadBuffer, uint32_t ReadLen)
{
	return jd9365t_HostReadPram(ReadAddr, ReadBuffer, ReadLen);
}

#ifdef JD_ZERO_FLASH
static int module_jd9365t_ram_write(uint32_t WriteAddr, uint8_t *WriteBuffer, uint32_t WriteLen)
{
	return jd9365t_HostWritePram(WriteAddr ,WriteBuffer, WriteLen);
}
#endif

static int module_jd9365t_flash_read(uint32_t ReadAddr, uint8_t *ReadBuffer, uint32_t ReadLen)
{
	return jd9365t_HostReadFlash(ReadAddr, ReadBuffer, ReadLen);
}

static int module_jd9365t_flash_write(uint32_t WriteAddr, uint8_t *WriteData, uint32_t WriteLen)
{
	return jd9365t_HostWriteFlash(WriteAddr, WriteData, WriteLen);
}

static int module_jd9365t_flash_erase(void)
{
	return jd9365t_EraseChip();
}

static void module_jd9365t_read_fw_ver(void)
{
	int ReCode_cid, ReCode_ver;
	uint8_t rdata_cid[JD_FOUR_SIZE], rdata_fw[JD_FOUR_SIZE];

	ReCode_cid = module_jd9365t_register_read(g_jd9365t_chip_info.dsram_host_addr.fw_cid_version, rdata_cid, sizeof(rdata_cid));
	ReCode_ver = module_jd9365t_register_read(g_jd9365t_chip_info.dsram_host_addr.fw_version, rdata_fw, sizeof(rdata_fw));

	if ((ReCode_cid < 0) || (ReCode_ver < 0)) {
		JD_E("%s: Read fw version fail\n", __func__);
		pjadard_ic_data->fw_ver = 0;
		pjadard_ic_data->fw_cid_ver = 0;
	} else {
		pjadard_ic_data->fw_ver = ( (rdata_fw[3] << 24) |
									(rdata_fw[2] << 16) |
									(rdata_fw[1] <<  8) |
									(rdata_fw[0]) );
		pjadard_ic_data->fw_cid_ver = ( (rdata_cid[3] << 24) |
										(rdata_cid[2] << 16) |
										(rdata_cid[1] <<  8) |
										(rdata_cid[0]) );
	}

	/*hs03s  code for SR-AL5625-01-96 by wangdeyan at 20210519 start*/
	mtp_fw_ver = pjadard_ic_data->fw_ver;
	/*hs03s  code for SR-AL5625-01-96 by wangdeyan at 20210519 end*/
	
	JD_I("FW_VER: %08x\n", pjadard_ic_data->fw_ver);
	JD_I("FW_CID_VER: %08x\n", pjadard_ic_data->fw_cid_ver);
}

#if defined(JD_AUTO_UPGRADE_FW)
static int module_jd9365t_read_fw_ver_bin(void)
{
	if (g_jadard_fw != NULL) {
		JD_I("Catch fw version in bin file\n");
		g_jadard_fw_ver = ( (g_jadard_fw[JD9365T_BIN_FW_VER + 0] << 16) |
							(g_jadard_fw[JD9365T_BIN_FW_VER + 1] <<  8) |
							(g_jadard_fw[JD9365T_BIN_FW_VER + 2]) );
		g_jadard_fw_cid_ver = ( (g_jadard_fw[JD9365T_BIN_FW_CID_VER + 0] << 24) |
								(g_jadard_fw[JD9365T_BIN_FW_CID_VER + 1] << 16) |
								(g_jadard_fw[JD9365T_BIN_FW_CID_VER + 2] <<  8) |
								(g_jadard_fw[JD9365T_BIN_FW_CID_VER + 3]) );
	} else {
		JD_I("FW data is null\n");
		return JD_FILE_OPEN_FAIL;
	}

	return JD_NO_ERR;
}
#endif

static void module_jd9365t_mutual_data_set(uint8_t data_type)
{
	uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};
	uint8_t status[JD_TWO_SIZE];

	if ((data_type == JD_DATA_TYPE_RawData) ||
		(data_type == JD_DATA_TYPE_Difference) ||
		/*hs03s code for SR-AL5625-01-439 by yuanliding at 20210616 start*/
		(data_type == JD_DATA_TYPE_Baseline) ||
		(data_type == JD_DATA_TYPE_LISTEN) ||
		(data_type == JD_DATA_TYPE_LABEL)) {
		/*hs03s code for SR-AL5625-01-439 by yuanliding at 20210616 end*/
		wdata[0] = data_type - 1;
	} else {
		wdata[0] = JD_DATA_TYPE_RawData - 1;
	}

	module_jd9365t_register_write(g_jd9365t_chip_info.dsram_debug_addr.output_data_sel, wdata, sizeof(wdata));

	/* Clear handshark status for START */
	status[0] = 0xA5;
	status[1] = 0x5A;
	jd9365t_WriteRegMulti(g_jd9365t_chip_info.dsram_debug_addr.output_data_handshake, status, sizeof(status));
}

static int module_jd9365t_get_mutual_data(uint8_t data_type, uint8_t *rdata, uint16_t rlen)
{
	int ReCode;
	int DataCnt = 0;
	uint8_t status[JD_TWO_SIZE];

	do {
		mdelay(1);

		if (DataCnt++ > JD9365T_DATA_TIMEOUT) {
			JD_I("Scan data timeout, DataCnt = %d\n", DataCnt);
			return JD_TIME_OUT;
		}

		jd9365t_ReadRegMulti(g_jd9365t_chip_info.dsram_debug_addr.output_data_handshake, status, sizeof(status));
	} while (((status[1] << 8) + status[0]) != JD9365T_OUTPUT_DATA_HANDSHAKE_DONE);

	/* Get data from IC */
	switch (data_type) {
	case JD_DATA_TYPE_RawData:
	case JD_DATA_TYPE_Baseline:
	case JD_DATA_TYPE_Difference:
	/*hs03s code for SR-AL5625-01-439 by yuanliding at 20210616 start*/
	case JD_DATA_TYPE_LISTEN:
	case JD_DATA_TYPE_LABEL:
	/*hs03s code for SR-AL5625-01-439 by yuanliding at 20210616 end*/
		ReCode = jd9365t_ReadRegMulti(g_jd9365t_chip_info.dsram_debug_addr.output_data_addr, rdata, rlen);
		/* Clear handshark status */
		status[0] = 0xA5;
		status[1] = 0x5A;
		jd9365t_WriteRegMulti(g_jd9365t_chip_info.dsram_debug_addr.output_data_handshake, status, sizeof(status));
		break;
	default:
		JD_I("%s: Not support data type = %d\n", __func__, data_type);
		ReCode = -EINVAL;
	}

	return ReCode;
}

static bool module_jd9365t_get_touch_data(uint8_t *rdata, uint8_t rlen)
{
	if (jd9365t_ReadTpInfoBuf(rdata, rlen) < 0) {
		return false;
	} else {
		return true;
	}
}

/*hs03s code for SR-AL5625-01-305 by yuanliding at 20210521 start*/
static void module_jd9365t_set_high_sensitivity(bool enable)
{
	uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};

	if (enable) {
		wdata[0] = 0x3A;
		wdata[1] = 0xA3;
	}

	module_jd9365t_register_write(g_jd9365t_chip_info.dsram_host_addr.high_sensitivity_en, wdata, sizeof(wdata));
}
/*hs03s code for SR-AL5625-01-305 by yuanliding at 20210521 end*/

static void module_jd9365t_set_SMWP_enable(bool enable)
{
	uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};

	if (enable) {
		wdata[0] = 1;
	} else {
		wdata[0] = 0;
	}

	module_jd9365t_register_write(g_jd9365t_chip_info.dsram_host_addr.gesture_en, wdata, sizeof(wdata));
}

static void module_jd9365t_usb_detect_set(uint8_t *usb_status)
{
	uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};

	wdata[0] = usb_status[1];
	module_jd9365t_register_write(g_jd9365t_chip_info.dsram_host_addr.usb_en, wdata, sizeof(wdata));
}

static uint8_t module_jd9365t_get_freq_band(void)
{
	uint8_t rdata[JD_TWO_SIZE];

	module_jd9365t_register_read(g_jd9365t_chip_info.dsram_debug_addr.freq_band, rdata, sizeof(rdata));
	return rdata[0];
}

#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
static void module_jd9365t_APP_SetSortingMode(uint8_t *value, uint8_t value_len)
{
	module_jd9365t_register_write(g_jd9365t_chip_info.dsram_host_addr.mpap_pw, value, value_len);
}

static void module_jd9365t_APP_ReadSortingMode(uint8_t *pValue, uint8_t pValue_len)
{
	module_jd9365t_register_read(g_jd9365t_chip_info.dsram_host_addr.mpap_pw, pValue, pValue_len);
}

static void module_jd9365t_APP_GetLcdSleep(uint8_t *pStatus)
{
	jd9365t_ReadRegSingle((uint32_t)JD9365T_SOC_REG_ADDR_SLPOUT, pStatus);
}

static void module_jd9365t_APP_SetSortingSkipFrame(uint8_t value)
{
	jd9365t_WriteRegSingle(g_jd9365t_chip_info.dsram_host_addr.mpap_skip_frame, value);
}

static void module_jd9365t_APP_SetSortingKeepFrame(uint8_t value)
{
	jd9365t_WriteRegSingle(g_jd9365t_chip_info.dsram_host_addr.mpap_keep_frame, value);
}

static bool module_jd9365t_APP_ReadSortingBusyStatus(uint8_t mpap_handshake_finish)
{
	uint8_t pStatus = 0x00;

	jd9365t_ReadRegSingle(g_jd9365t_chip_info.dsram_host_addr.mpap_handshake, &pStatus);

	if (pStatus == mpap_handshake_finish)
		return true;
	else
		return false;
}

static void module_jd9365t_GetSortingDiffData(uint8_t *pValue, uint16_t pValue_len)
{
	module_jd9365t_register_read(g_jd9365t_chip_info.dsram_host_addr.mpap_diff_array, pValue, pValue_len);
}

static void module_jd9365t_OSCD_Off(void)
{
	uint8_t wBuf[JD_TWO_SIZE];

	wBuf[0] = 0x24;
	wBuf[1] = 0x02;
	module_jd9365t_register_write(g_jd9365t_chip_info.dsram_host_addr.oscd_on, wBuf, sizeof(wBuf));
}

static void module_jd9365t_OSCD_On(void)
{
	uint8_t wBuf[JD_TWO_SIZE];

	wBuf[0] = 0xFF;
	wBuf[1] = 0xFF;
	module_jd9365t_register_write(g_jd9365t_chip_info.dsram_host_addr.oscd_on, wBuf, sizeof(wBuf));
}

static void module_jd9365t_StartMCU(void)
{
	jd9365t_StartMCU();
}
#endif

static void jd9365t_func_reinit(void)
{
	JD_D("%s:Entering!\n", __func__);
	g_module_fp.fp_chip_init            = module_jd9365t_chip_init;
	g_module_fp.fp_pin_reset            = module_jd9365t_PinReset;
	g_module_fp.fp_soft_reset           = module_jd9365t_SoftReset;
	g_module_fp.fp_register_read        = module_jd9365t_register_read;
	g_module_fp.fp_register_write       = module_jd9365t_register_write;
	g_module_fp.fp_flash_read           = module_jd9365t_flash_read;
	g_module_fp.fp_flash_write          = module_jd9365t_flash_write;
	g_module_fp.fp_flash_erase          = module_jd9365t_flash_erase;
	g_module_fp.fp_ram_read             = module_jd9365t_ram_read;
#ifdef JD_ZERO_FLASH
	g_module_fp.fp_ram_write            = module_jd9365t_ram_write;
#endif
	g_module_fp.fp_read_fw_ver          = module_jd9365t_read_fw_ver;
#ifdef JD_AUTO_UPGRADE_FW
	g_module_fp.fp_read_fw_ver_bin      = module_jd9365t_read_fw_ver_bin;
#endif
	g_module_fp.fp_mutual_data_set      = module_jd9365t_mutual_data_set;
	g_module_fp.fp_get_mutual_data      = module_jd9365t_get_mutual_data;
	g_module_fp.fp_get_touch_data       = module_jd9365t_get_touch_data;
	g_module_fp.fp_set_high_sensitivity = module_jd9365t_set_high_sensitivity;
	g_module_fp.fp_set_SMWP_enable      = module_jd9365t_set_SMWP_enable;
	g_module_fp.fp_usb_detect_set       = module_jd9365t_usb_detect_set;
	g_module_fp.fp_get_freq_band        = module_jd9365t_get_freq_band;
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
	g_module_fp.fp_APP_SetSortingMode        = module_jd9365t_APP_SetSortingMode;
	g_module_fp.fp_APP_ReadSortingMode       = module_jd9365t_APP_ReadSortingMode;
	g_module_fp.fp_APP_GetLcdSleep           = module_jd9365t_APP_GetLcdSleep;
	g_module_fp.fp_APP_SetSortingSkipFrame   = module_jd9365t_APP_SetSortingSkipFrame;
	g_module_fp.fp_APP_SetSortingKeepFrame   = module_jd9365t_APP_SetSortingKeepFrame;
	g_module_fp.fp_APP_ReadSortingBusyStatus = module_jd9365t_APP_ReadSortingBusyStatus;
	g_module_fp.fp_GetSortingDiffData        = module_jd9365t_GetSortingDiffData;
	g_module_fp.fp_OSCD_Off                  = module_jd9365t_OSCD_Off;
	g_module_fp.fp_OSCD_On                   = module_jd9365t_OSCD_On;
	g_module_fp.fp_StartMCU                  = module_jd9365t_StartMCU;
#endif
}

static bool module_jd9365t_chip_detect(void)
{
	bool ret = false;
	uint16_t romid = 0;
	uint16_t CHIP_ID = JD9365T_ID;

	/* EnterBackDoor get IC ID */
	jd9365t_EnterBackDoor(&romid);

	if (romid == CHIP_ID) {
		JD_I("%s:detect IC JD%04XTD successfully\n", __func__, CHIP_ID);
		snprintf(pjadard_ic_data->chip_id, sizeof(pjadard_ic_data->chip_id), "JD%04XTD", CHIP_ID);
		jadard_mcu_cmd_struct_init();
		jd9365t_func_reinit();
		ret = true;
	} else {
		JD_E("%s:Read driver ID %04X not equal %04X\n", __func__, romid, CHIP_ID);
		JD_E("Could not find Jadard Chipset\n");
	}

	return ret;
}

#ifdef CONFIG_CHIP_DTCFG
static int jadard_jd9365t_probe(struct platform_device *pdev)
{
	JD_I("%s:Enter\n", __func__);
	g_module_fp.fp_chip_detect = module_jd9365t_chip_detect;

	return 0;
}

static int jadard_jd9365t_remove(struct platform_device *pdev)
{
	g_module_fp.fp_chip_detect = NULL;
	g_module_fp.fp_chip_init = NULL;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id jadard_jd9365t_mttable[] = {
	{ .compatible = "jadard,jd9365t"},
	{ },
};
#else
#define jadard_jd9365t_mttable NULL
#endif

static struct platform_driver jadard_jd9365t_driver = {
	.probe	= jadard_jd9365t_probe,
	.remove	= jadard_jd9365t_remove,
	.driver	= {
		.name           = "JADARD_JD9365T",
		.owner          = THIS_MODULE,
		.of_match_table = jadard_jd9365t_mttable,
	},
};

static int __init jadard_jd9365t_init(void)
{
	JD_I("%s\n", __func__);
	platform_driver_register(&jadard_jd9365t_driver);

	return 0;
}

static void __exit jadard_jd9365t_exit(void)
{
	platform_driver_unregister(&jadard_jd9365t_driver);
}

#else
static int jadard_jd9365t_probe(void)
{
	JD_I("%s:Enter\n", __func__);
	g_module_fp.fp_chip_detect = module_jd9365t_chip_detect;

	return 0;
}

static int jadard_jd9365t_remove(void)
{
	g_module_fp.fp_chip_detect = NULL;
	g_module_fp.fp_chip_init = NULL;

	return 0;
}

static int __init jadard_jd9365t_init(void)
{
	JD_I("%s\n", __func__);
	jadard_jd9365t_probe();

	return 0;
}

static void __exit jadard_jd9365t_exit(void)
{
	jadard_jd9365t_remove();
}
#endif

module_init(jadard_jd9365t_init);
module_exit(jadard_jd9365t_exit);

MODULE_DESCRIPTION("JADARD JD9365T touch driver");
MODULE_LICENSE("GPL");
