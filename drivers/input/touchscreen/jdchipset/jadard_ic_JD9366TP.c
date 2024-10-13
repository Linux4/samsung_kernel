#include "jadard_platform.h"
#include "jadard_common.h"
#include "jadard_module.h"
#include "jadard_ic_JD9366TP.h"

extern struct jadard_module_fp g_module_fp;
extern struct jadard_ts_data *pjadard_ts_data;
extern struct jadard_ic_data *pjadard_ic_data;
extern struct jadard_report_data *pjadard_report_data;
extern struct jadard_common_variable g_common_variable;
extern char *jd_panel_maker_list[JD_PANEL_MAKER_LIST_SIZE];

#ifdef JD_AUTO_UPGRADE_FW
extern uint8_t *g_jadard_fw;
extern uint32_t g_jadard_fw_ver;
extern uint32_t g_jadard_fw_cid_ver;
#endif

#ifdef CONFIG_TOUCHSCREEN_JADARD_DEBUG
extern int *jd_diag_mutual;
#endif

static void jd9366tp_ChipInfoInit(void)
{
    /* Initial section info */
    g_jd9366tp_section_info.section_info_ready            = false;
    g_jd9366tp_section_info.section_info_ready_addr       = JD9366TP_MEMORY_ADDR_ERAM;

    g_jd9366tp_section_info.dsram_num_start_addr          = JD9366TP_MEMORY_ADDR_ERAM + 2;
    g_jd9366tp_section_info.dsram_section_info_start_addr = JD9366TP_MEMORY_ADDR_ERAM + 4;

    g_jd9366tp_section_info.esram_num_start_addr          = (g_jd9366tp_section_info.dsram_section_info_start_addr +
                                                             JD9366TP_MAX_DSRAM_NUM * sizeof(struct JD9366TP_INFO_CONTENT));
    g_jd9366tp_section_info.esram_section_info_start_addr = g_jd9366tp_section_info.esram_num_start_addr + 4;
}

static bool jd9366tp_DoneStatusIsHigh(uint8_t usValue, uint8_t DoneStatus)
{
    return ((usValue & DoneStatus) == DoneStatus);
}

static bool jd9366tp_DoneStatusIsLow(uint8_t usValue, uint8_t BusyStatus, uint8_t DoneStatus)
{
    return ((usValue & BusyStatus) == DoneStatus);
}

static int jd9366tp_Read_BackDoor_RegSingle(uint32_t addr, uint8_t *rdata)
{
    int ReCode;
    uint8_t addrBuf[JD_SIX_SIZE];
    uint8_t readBuf[JD_ONE_SIZE];

    addrBuf[0] = 0xF3;
    addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
    addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
    addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
    addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
    addrBuf[5] = 0x03;

    ReCode = jadard_bus_read(addrBuf, sizeof(addrBuf), readBuf, sizeof(readBuf), JADARD_BUS_RETRY_TIMES);
    *rdata = readBuf[0];

    return ReCode;
}

static int jd9366tp_Write_BackDoor_RegSingle(uint32_t addr, uint8_t wdata)
{
    uint8_t addrBuf[JD_SIX_SIZE];
    uint8_t writeBuf[JD_ONE_SIZE];

    addrBuf[0] = 0xF2;
    addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
    addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
    addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
    addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
    addrBuf[5] = 0x03;
    writeBuf[0] = wdata;

    return jadard_bus_write(addrBuf, sizeof(addrBuf), writeBuf, sizeof(writeBuf), JADARD_BUS_RETRY_TIMES);
}

static int jd9366tp_Read_BackDoor_RegMulti(uint32_t addr, uint8_t *rdata, uint16_t rlen)
{
    uint8_t addrBuf[JD_SIX_SIZE];

    addrBuf[0] = 0xF3;
    addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
    addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
    addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
    addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
    addrBuf[5] = 0x03;

    return jadard_bus_read(addrBuf, sizeof(addrBuf), rdata, rlen, JADARD_BUS_RETRY_TIMES);
}

static int jd9366tp_Write_BackDoor_RegMulti(uint32_t addr, uint8_t *wdata, uint16_t wlen)
{
    uint8_t addrBuf[JD_SIX_SIZE];

    addrBuf[0] = 0xF2;
    addrBuf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
    addrBuf[2] = (uint8_t)((addr & 0x00FF0000) >> 16);
    addrBuf[3] = (uint8_t)((addr & 0x0000FF00) >> 8);
    addrBuf[4] = (uint8_t)((addr & 0x000000FF) >> 0);
    addrBuf[5] = 0x03;

    return jadard_bus_write(addrBuf, sizeof(addrBuf), wdata, wlen, JADARD_BUS_RETRY_TIMES);
}

static int jd9366tp_Read_FW_RegSingleI2c(uint32_t addr, uint8_t *rdata)
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

static int jd9366tp_Write_FW_RegSingleI2c(uint32_t addr, uint8_t wdata)
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

static int jd9366tp_Read_FW_RegMultiI2c(uint32_t addr, uint8_t *rdata, uint16_t rlen)
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

static int jd9366tp_Write_FW_RegMultiI2c(uint32_t addr, uint8_t *wdata, uint16_t wlen)
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

static int jd9366tp_Read_FW_RegSingleSpi(uint32_t addr, uint8_t *rdata)
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

static int jd9366tp_Write_FW_RegSingleSpi(uint32_t addr, uint8_t wdata)
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

static int jd9366tp_Read_FW_RegMultiSpi(uint32_t addr, uint8_t *rdata, uint16_t rlen)
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

static int jd9366tp_Write_FW_RegMultiSpi(uint32_t addr, uint8_t *wdata, uint16_t wlen)
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

static int jd9366tp_ReadRegSingle(uint32_t addr, uint8_t *rdata)
{
    int ReCode;

    if (!pjadard_ts_data->diag_thread_active)
        jd9366tp_EnterBackDoor(NULL);

    if (g_jd9366tp_chip_info.back_door_mode) {
    #ifdef JD_READ_SLAVE_BY_MASTER
        if (((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) &&
            (pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE == false) &&
            ((addr & JD9366TP_SLAVE_ADDR_OFFSET) == JD9366TP_SLAVE_ADDR_OFFSET)) {
            ReCode = jd9366tp_read_slave_by_master(addr, rdata , 1);
        } else {
            ReCode = jd9366tp_Read_BackDoor_RegSingle(addr, rdata);
        }
    #else
        ReCode = jd9366tp_Read_BackDoor_RegSingle(addr, rdata);
    #endif
    } else {
        if ((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) {
        #ifdef JD_READ_SLAVE_BY_MASTER
            if (((addr & JD9366TP_SLAVE_ADDR_OFFSET) == JD9366TP_SLAVE_ADDR_OFFSET) &&
                (pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE == false)) {
                ReCode = jd9366tp_read_slave_by_master(addr, rdata, 1);
            } else {
                ReCode = jd9366tp_Read_FW_RegSingleSpi(addr, rdata);
            }
        #else
            ReCode = jd9366tp_Read_FW_RegSingleSpi(addr, rdata);
        #endif
        } else {
            ReCode = jd9366tp_Read_FW_RegSingleI2c(addr, rdata);
        }
    }

    if (!pjadard_ts_data->diag_thread_active)
        jd9366tp_ExitBackDoor();

    return ReCode;
}

static int jd9366tp_WriteRegSingle(uint32_t addr, uint8_t wdata)
{
    int ReCode;

    if (!pjadard_ts_data->diag_thread_active)
        jd9366tp_EnterBackDoor(NULL);

    if (g_jd9366tp_chip_info.back_door_mode) {
        ReCode = jd9366tp_Write_BackDoor_RegSingle(addr, wdata);
    } else {
        if ((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) {
            ReCode = jd9366tp_Write_FW_RegSingleSpi(addr, wdata);
        } else {
            ReCode = jd9366tp_Write_FW_RegSingleI2c(addr, wdata);
        }
    }

    if (!pjadard_ts_data->diag_thread_active)
        jd9366tp_ExitBackDoor();

    return ReCode;
}

static int jd9366tp_ReadRegMulti(uint32_t addr, uint8_t *rdata, uint16_t rlen)
{
    int ReCode;

#ifndef JD_I2C_SINGLE_MODE
    if ((addr >= JD9366TP_MEMORY_ADDR_ERAM) &&
        (addr < (JD9366TP_MEMORY_ADDR_ERAM + JD9366TP_MEMORY_ERAM_SIZE))) {
    #ifdef JD_READ_SLAVE_BY_MASTER
        if (((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) &&
            (pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE == false) &&
            ((addr & JD9366TP_SLAVE_ADDR_OFFSET) == JD9366TP_SLAVE_ADDR_OFFSET)) {
            ReCode = jd9366tp_read_slave_by_master(addr, rdata, rlen);
        } else {
            ReCode = jd9366tp_Read_BackDoor_RegMulti(addr, rdata, rlen);
        }
    #else
        ReCode = jd9366tp_Read_BackDoor_RegMulti(addr, rdata, rlen);
    #endif
    } else {
#endif
        if (!pjadard_ts_data->diag_thread_active)
            jd9366tp_EnterBackDoor(NULL);

        if (g_jd9366tp_chip_info.back_door_mode) {
        #ifdef JD_READ_SLAVE_BY_MASTER
            if (((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) &&
                (pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE == false) &&
                ((addr & JD9366TP_SLAVE_ADDR_OFFSET) == JD9366TP_SLAVE_ADDR_OFFSET)) {
                ReCode = jd9366tp_read_slave_by_master(addr, rdata, rlen);
            } else {
                ReCode = jd9366tp_Read_BackDoor_RegMulti(addr, rdata, rlen);
            }
        #else
            ReCode = jd9366tp_Read_BackDoor_RegMulti(addr, rdata, rlen);
        #endif
        } else {
            if ((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) {
            #ifdef JD_READ_SLAVE_BY_MASTER
                if (((addr & JD9366TP_SLAVE_ADDR_OFFSET) == JD9366TP_SLAVE_ADDR_OFFSET) &&
                    (pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE == false)) {
                    ReCode = jd9366tp_read_slave_by_master(addr, rdata, rlen);
                } else {
                    ReCode = jd9366tp_Read_FW_RegMultiSpi(addr, rdata, rlen);
                }
            #else
                ReCode = jd9366tp_Read_FW_RegMultiSpi(addr, rdata, rlen);
            #endif
            } else {
                ReCode = jd9366tp_Read_FW_RegMultiI2c(addr, rdata, rlen);
            }
        }

        if (!pjadard_ts_data->diag_thread_active)
            jd9366tp_ExitBackDoor();
#ifndef JD_I2C_SINGLE_MODE
    }
#endif

    return ReCode;
}

static int jd9366tp_WriteRegMulti(uint32_t addr, uint8_t *wdata, uint16_t wlen)
{
    int ReCode;

    if (!pjadard_ts_data->diag_thread_active)
        jd9366tp_EnterBackDoor(NULL);

    if (g_jd9366tp_chip_info.back_door_mode) {
        ReCode = jd9366tp_Write_BackDoor_RegMulti(addr, wdata, wlen);
    } else {
        if ((pjadard_ts_data != NULL) && (pjadard_ts_data->spi != NULL)) {
            ReCode = jd9366tp_Write_FW_RegMultiSpi(addr, wdata, wlen);
        } else {
            ReCode = jd9366tp_Write_FW_RegMultiI2c(addr, wdata, wlen);
        }
    }

    if (!pjadard_ts_data->diag_thread_active)
        jd9366tp_ExitBackDoor();

    return ReCode;
}

#ifdef JD_READ_SLAVE_BY_MASTER
static void jd9366tp_SetSortingMode_by_master(uint8_t *value, uint8_t value_len)
{
    uint32_t AddrOffset;

    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;

    jd9366tp_WriteRegSingle(g_jd9366tp_chip_info.dsram_host_addr.mpap_pw_sync + AddrOffset, 0x00);
    jd9366tp_WriteRegSingle(g_jd9366tp_chip_info.dsram_host_addr.mpap_sync_status + AddrOffset, 0x00);
    /* Write Master then Master transfer to Slave */
    jd9366tp_WriteRegMulti(g_jd9366tp_chip_info.dsram_host_addr.mpap_pw, value, value_len);
    jd9366tp_WriteRegSingle(g_jd9366tp_chip_info.dsram_host_addr.mpap_pw_sync + JD9366TP_MASTER_ADDR_OFFSET, 0x01);
}

static int jd9366tp_read_slave_by_master(uint32_t addr, uint8_t *rdata, uint16_t rlen)
{
    int ReCode;
    int retry;
    uint8_t os_test_mode_password[JD_TWO_SIZE];
    uint8_t address[JD_FOUR_SIZE];
    uint16_t packsize = 32;
    uint32_t pos = 0;
    uint16_t packlen = 0;
    uint32_t ramaddr = addr & 0xFEFFFFFF;
    uint32_t ramlen = rlen;
	ReCode = -1;
    if (rlen > 0) {
        JD_I("%s: Start read slave ram addr: 0x%08x\n", __func__, addr);
        JD_I("%s: Read slave ram data %d bytes\n", __func__, ramlen);

        /* Read from ram */
        while (ramlen > 0) {
            if (ramlen > packsize)
                packlen = packsize;
            else
                packlen = (uint16_t)ramlen;

            retry = 0;
            address[0] = (uint8_t)((ramaddr & 0x000000FF) >> 0);
            address[1] = (uint8_t)((ramaddr & 0x0000FF00) >> 8);
            address[2] = (uint8_t)((ramaddr & 0x00FF0000) >> 16);
            address[3] = (uint8_t)((ramaddr & 0xFF000000) >> 24);
            jd9366tp_WriteRegMulti(g_jd9366tp_chip_info.dsram_debug_addr.ip_stress_addr, address, sizeof(address));
            jd9366tp_WriteRegSingle(g_jd9366tp_chip_info.dsram_debug_addr.otp_len, packlen);

            os_test_mode_password[0] = 0x01;
            os_test_mode_password[1] = 0xFD;
            jd9366tp_SetSortingMode_by_master(os_test_mode_password, sizeof(os_test_mode_password));

            do {
                mdelay(50);

                jd9366tp_ReadRegMulti(g_jd9366tp_chip_info.dsram_host_addr.mpap_pw,
                                      os_test_mode_password, sizeof(os_test_mode_password));
                if ((os_test_mode_password[0] == 0xFF) && (os_test_mode_password[1] == 0xFF)) {
                    break;
                }
            } while (retry++ < 20);

            ReCode = jd9366tp_ReadRegMulti(g_jd9366tp_chip_info.esram_info_content_addr.slave_data_addr, rdata + pos, packlen);

            /* Set next packet */
            ramlen -= packlen;
            pos += packlen;
            ramaddr += packlen;
        }
    }

    return ReCode;
}
#endif

static int jd9366tp_GetID(uint16_t *pRomID, uint32_t offset)
{
    int ReCode;
    uint8_t rbuf[JD_TWO_SIZE];

    ReCode = jd9366tp_ReadRegMulti((uint32_t)(JD9366TP_SOC_REG_ADDR_CHIP_ID2 + offset), rbuf, sizeof(rbuf));

    if (ReCode < 0)
        return ReCode;

    *pRomID = (uint16_t)((rbuf[1] << 8) + rbuf[0]);

    if (*pRomID == JD9366TP_ID) {
        if (JD9366TP_SLAVE_ADDR_OFFSET == offset)
            JD_I("%s: Slave IC ID: %04x\n", __func__, *pRomID);
        else
            JD_I("%s: Master IC ID: %04x\n", __func__, *pRomID);
    } else {
        if (JD9366TP_SLAVE_ADDR_OFFSET == offset)
            JD_E("%s: Error Slave IC ID: %04x\n", __func__, *pRomID);
        else
            JD_E("%s: Error Master IC ID: %04x\n", __func__, *pRomID);

        ReCode = JD_CHIP_ID_ERROR;
    }

    return ReCode;
}

static int jd9366tp_EnterBackDoor(uint16_t *pRomID)
{
    int ReCode;
#ifndef JD_I2C_SINGLE_MODE
    uint8_t addrBuf[JD_FIVE_SIZE];
    uint8_t writeBuf[JD_ONE_SIZE];

    #define POR_BACKDOOR

    #if defined (POR_BACKDOOR)
        addrBuf[0] = 0xF2;
        addrBuf[1] = 0xAA;
        addrBuf[2] = 0xF0;
        addrBuf[3] = 0x0F;
        addrBuf[4] = 0x55;
        writeBuf[0] = 0x68;
    #else
        addrBuf[0] = 0xF2;
        addrBuf[1] = 0xAA;
        addrBuf[2] = 0x55;
        addrBuf[3] = 0x0F;
        addrBuf[4] = 0xF0;
        writeBuf[0] = 0x68;
    #endif

    ReCode = jadard_bus_write(addrBuf, sizeof(addrBuf), writeBuf, sizeof(writeBuf), JADARD_BUS_RETRY_TIMES);

    if (ReCode < 0) {
        JD_E("%s: EnterBackDoor fail\n", __func__);
    } else {
        g_jd9366tp_chip_info.back_door_mode = true;
        /* If pRomID != NULL, Read IC ID */
        if (pRomID) {
            ReCode = jd9366tp_GetID(pRomID, JD9366TP_MASTER_ADDR_OFFSET);
        }
    }
    return ReCode;
#else
    g_jd9366tp_chip_info.back_door_mode = false;
    /* If pRomID != NULL, Read IC ID */
    if (pRomID) {
        ReCode = jd9366tp_GetID(pRomID, JD9366TP_MASTER_ADDR_OFFSET);
    } else {
        ReCode = JD_NO_ERR;
    }

    return ReCode;
#endif
}

static int jd9366tp_ExitBackDoor(void)
{
#ifndef JD_I2C_SINGLE_MODE
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
        g_jd9366tp_chip_info.back_door_mode = false;
    }

    return ReCode;
#else
    return JD_NO_ERR;
#endif
}

static void jd9366tp_PinReset(void)
{
#ifdef JD_RST_PIN_FUNC
    JD_I("%s: Now reset the Touch chip\n", __func__);

    if (gpio_is_valid(pjadard_ts_data->rst_gpio)) {
        if (gpio_request(pjadard_ts_data->rst_gpio, "jadard_reset_gpio")) {
            JD_E("unable to request rst-gpio [%d]\n", pjadard_ts_data->rst_gpio);
            goto err_gpio;
        }

        if (gpio_direction_output(pjadard_ts_data->rst_gpio, 1)) {
            JD_E("unable to set direction for rst-gpio [%d]\n", pjadard_ts_data->rst_gpio);
            gpio_free(pjadard_ts_data->rst_gpio);
            goto err_gpio;
        }

        jadard_gpio_set_value(pjadard_ts_data->rst_gpio, 1);
        mdelay(10);
        jadard_gpio_set_value(pjadard_ts_data->rst_gpio, 0);
        mdelay(10);
        jadard_gpio_set_value(pjadard_ts_data->rst_gpio, 1);
        mdelay(100);

        gpio_free(pjadard_ts_data->rst_gpio);
    } else {
        JD_E("rst-gpio [%d] is not valid\n", pjadard_ts_data->rst_gpio);
    }

err_gpio:
#endif
    /* EnterBackDoor */
    jd9366tp_EnterBackDoor(NULL);
}

static int jd9366tp_StartMCUClock(uint32_t offset)
{
    int ReCode;

    ReCode = jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_SOC_REG_ADDR_CPU_CLK_STOP + offset),
                                        (uint8_t)JD9366TP_SOC_PASSWORD_START_MCU_CLOCK);

    if (ReCode < 0) {
        JD_E("%s: Start MCU clock fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_StopMCUClock(uint32_t offset)
{
#ifndef JD_I2C_SINGLE_MODE
    int ReCode;

    ReCode = jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_SOC_REG_ADDR_CPU_CLK_STOP + offset),
                                        (uint8_t)JD9366TP_SOC_PASSWORD_STOP_MCU_CLOCK);

    if (ReCode < 0) {
        JD_E("%s: Stop MCU clock fail\n", __func__);
    }

    return ReCode;
#else
    return JD_NO_ERR;
#endif
}

static int jd9366tp_StartMCU(uint32_t offset)
{
    int ReCode;

    ReCode = jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_SOC_REG_ADDR_PRAM_PROG + offset),
                                        (uint8_t)JD9366TP_SOC_PASSWORD_START_MCU);

    if (ReCode < 0) {
        JD_E("%s: Start MCU fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_StopMCU(uint32_t offset)
{
    int ReCode;

    ReCode = jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_SOC_REG_ADDR_PRAM_PROG + offset),
                                        (uint8_t)JD9366TP_SOC_PASSWORD_STOP_MCU);

    if (ReCode < 0) {
        JD_E("%s: Stop MCU fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_ResetSOC(uint32_t offset)
{
    int ReCode;
	int retry = 0;

	do {
		ReCode = jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_SOC_REG_ADDR_RGU_0 + offset),
                                        (uint8_t)JD9366TP_SOC_PASSWORD_SOC_RESET);
    } while ((++retry < 5) && (ReCode < 0));

    if (ReCode < 0) {
        JD_E("%s: Reset SOC fail\n", __func__);
    } else {
        g_jd9366tp_chip_info.back_door_mode = false;
    }

    return ReCode;
}

static int jd9366tp_ResetMCU(uint32_t offset)
{
    int ReCode;

    ReCode = jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_SOC_REG_ADDR_CPU_SOFT_RESET + offset),
                                        (uint8_t)JD9366TP_SOC_PASSWORD_MCU_RESET);

    if (ReCode < 0) {
        JD_E("%s: Reset MCU fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_PorInit(uint32_t offset)
{
    int ReCode;

    ReCode = jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_SOC_REG_ADDR_POR_INIT + offset),
                                        (uint8_t)JD9366TP_SOC_PASSWORD_POR_CLEAR);

    if (ReCode < 0) {
        JD_E("%s: Por init fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_DisableTouchScan(uint32_t offset)
{
    return jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_STC1_REG_ADDR_AFE_SCAN_CTRL + offset),
                                    (uint8_t)JD9366TP_STC1_RELATED_SETTING_STC_SCAN_DISABLE);
}

static int jd9366tp_EnableTouchScan(uint32_t offset)
{
    return jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_STC1_REG_ADDR_AFE_SCAN_CTRL + offset),
                                    (uint8_t)JD9366TP_STC1_RELATED_SETTING_STC_SCAN_ENABLE);
}

static int jd9366tp_DisableRTCRun(uint32_t offset)
{
    return jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_TIMER_REG_ADDR_RTC_CONFIG + offset),
                                    (uint8_t)JD9366TP_TIMER_RELATED_SETTING_DISABLE_RTC_RUN);
}

static int jd9366tp_EnableRTCRun(uint32_t offset)
{
    return jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_TIMER_REG_ADDR_RTC_CONFIG + offset),
                                    (uint8_t)JD9366TP_TIMER_RELATED_SETTING_ENABLE_RTC_RUN);
}

static int jd9366tp_DisableWDTRun(uint32_t offset)
{
    int ReCode;

    ReCode = jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_TIMER_REG_ADDR_WDT_CONFIG + offset),
                                    (uint8_t)JD9366TP_TIMER_RELATED_SETTING_WDT_STOP);

    if (ReCode < 0) {
        JD_E("%s: Disable WDT run fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_SetTCBSpeed(uint32_t offset, uint8_t config)
{
    int ReCode;

    ReCode = jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_CASCADE_BUS_CONTROL_REG_ADDR_TCB_TX_CLK_SET + offset), config);

    if (ReCode < 0) {
        JD_E("%s: Set TCB speed fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_GetTCBSpeed(uint32_t offset, uint8_t *rdata)
{
    int ReCode;

    ReCode = jd9366tp_ReadRegSingle((uint32_t)(JD9366TP_CASCADE_BUS_CONTROL_REG_ADDR_TCB_TX_CLK_SET + offset), rdata);

    if (ReCode < 0) {
        JD_E("%s: Get TCB speed fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_ReadChipCascadeMode(uint8_t *is_cascade_mode_or_not, uint32_t offset)
{
    int ReCode;
    uint8_t pValue = JD9366TP_CHIP_SINGLE_MODE;

    ReCode = jd9366tp_ReadRegSingle(JD9366TP_CASCADE_BUS_CONTROL_REG_ADDR_MODE_FLAG + offset, &pValue);
    *is_cascade_mode_or_not = ((pValue & JD9366TP_CHIP_CASCADE_MODE) == JD9366TP_CHIP_CASCADE_MODE)
                                ? (uint8_t)JD_CASCADE_MODE_ENABLE : (uint8_t)JD_CASCADE_MODE_DISABLE;

    return ReCode;
}

static int jd9366tp_SetFlashSPISpeed(uint8_t flash_spi_speed_level)
{
    int ReCode;

    ReCode = jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_BAUD_RATE, flash_spi_speed_level & 0x07);

    if (ReCode < 0) {
        JD_E("%s: Set flash SPI speed fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_GetFlashSPIFStatus(uint16_t usTimeOut, uint8_t *rdata)
{
    int ReCode;
    uint32_t i = 0;
    uint8_t usValue = (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_SPIF_BUSY;
    bool spif_busy_status_done = false;

    do {
        /* Read SPI status until done */
        mdelay(2);
        ReCode = jd9366tp_ReadRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_STATUS, &usValue);

        if (ReCode < 0) {
            JD_E("%s: Read flash SPI status fail\n", __func__);
            return ReCode;
        } else {
            i += 2;
        }

        spif_busy_status_done = jd9366tp_DoneStatusIsHigh(usValue,
                                                            (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_SPIF_DONE);

        /* Read SPI data when rdata was not NULL */
        if (rdata != NULL) {
            ReCode = jd9366tp_ReadRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_RDATA, rdata);
            if (ReCode < 0) {
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

static int jd9366tp_ICSetExFlashCSNOutDisable(void)
{
    return jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_CSN_OUT,
                                    (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_CSN_H);
}

static int jd9366tp_ICSetExFlashCSNOutEnable(void)
{
    return jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_CSN_OUT,
                                    (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_CSN_L);
}

static int jd9366tp_ICWriteExFlashFlow(uint8_t cmd, uint32_t addr, uint16_t addr_len)
{
    int ReCode, i;
    uint8_t wBuf[JD_THREE_SIZE];
    uint8_t dummy;

    /* Set SPI frequency to 12MHz */
    wBuf[0] = (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_SPEED_12MHz;

    ReCode = jd9366tp_WriteRegMulti((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_BAUD_RATE, wBuf, 1);
    if (ReCode < 0) {
        JD_E("%s: Set SPI frequency to 12MHz fail\n", __func__);
        return ReCode;
    }

    wBuf[0] = (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_CSN_L; /* Set CSN out enable [CSN = L] */
    wBuf[1] = 0x01; /* set SPI enable */
    wBuf[2] = cmd; /* set Flash command */

    ReCode = jd9366tp_WriteRegMulti((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_CSN_OUT, wBuf, sizeof(wBuf));
    if (ReCode < 0) {
        JD_E("%s: SPI setting fail\n", __func__);
        return ReCode;
    }

    /* Read SPI status until done */
    ReCode = jd9366tp_GetFlashSPIFStatus(50, &dummy);
    if (ReCode < 0) {
        JD_E("%s: Get flash SPI status fail\n", __func__);
        return ReCode;
    }

    for (i = 0; i < addr_len; i++) {
        ReCode = jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_WDATA,
                                            (uint8_t)(addr >> ((addr_len - i - 1) * 8)));
        if (ReCode < 0) {
            JD_E("%s: Set flash SPI addrress fail\n", __func__);
            return ReCode;
        }

        ReCode = jd9366tp_GetFlashSPIFStatus(50, &dummy);
        if (ReCode < 0) {
            JD_E("%s: Get flash SPI status fail\n", __func__);
            return ReCode;
        }
    }

    /* Set CSN out disable [CSN = H] */
    ReCode = jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_CSN_OUT,
                                        (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_CSN_H);
    if (ReCode < 0) {
        JD_E("%s: Set CSN high fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_ICReadExFlashFlow(uint8_t cmd, uint32_t addr, uint16_t addr_len,
                                      uint32_t dummy_par, uint16_t dummy_par_len, uint8_t *rdata, uint16_t rdata_len)
{
    int ReCode, i;
    uint8_t wBuf[JD_THREE_SIZE];
    uint8_t dummy;

    /* Set SPI frequency to 12MHz */
    wBuf[0] = (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_SPEED_12MHz;

    ReCode = jd9366tp_WriteRegMulti((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_BAUD_RATE, wBuf, 1);
    if (ReCode < 0) {
        JD_E("%s: Set SPI frequency to 12MHz fail\n", __func__);
        return ReCode;
    }

    /* Set CSN out enable [CSN = L] */
    wBuf[0] = (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_CSN_L;
    wBuf[1] = 0x01; /* set SPI enable */
    wBuf[2] = cmd; /* set Flash command */

    ReCode = jd9366tp_WriteRegMulti((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_CSN_OUT, wBuf, sizeof(wBuf));
    if (ReCode < 0) {
        JD_E("%s: SPI setting fail\n", __func__);
        return ReCode;
    }

    /* Read SPI status until done */
    ReCode = jd9366tp_GetFlashSPIFStatus(50, &dummy);
    if (ReCode < 0) {
        JD_E("%s: Get flash SPI status fail\n", __func__);
        return ReCode;
    }

    for (i = 0; i < addr_len; i++) {
        ReCode = jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_WDATA,
                                            (uint8_t)(addr >> ((addr_len - i - 1) * 8)));
        if (ReCode < 0) {
            JD_E("%s(%d): Set flash SPI addrress fail\n", __func__, __LINE__);
            return ReCode;
        }

        ReCode = jd9366tp_GetFlashSPIFStatus(50, &dummy);
        if (ReCode < 0) {
            JD_E("%s(%d): Get flash SPI status fail\n", __func__, __LINE__);
            return ReCode;
        }
    }

    for (i = 0; i < dummy_par_len; i++) {
        ReCode = jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_WDATA, dummy_par);
        if (ReCode < 0) {
            JD_E("%s(%d): Set flash SPI addrress fail\n", __func__, __LINE__);
            return ReCode;
        }

        ReCode = jd9366tp_GetFlashSPIFStatus(50, &dummy);
        if (ReCode < 0) {
            JD_E("%s(%d): Get flash SPI status fail\n", __func__, __LINE__);
            return ReCode;
        }
    }

    for (i = 0; i < rdata_len; i++) {
        /* set Dummy data */
        ReCode = jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_WDATA, (uint8_t)(i + 1));
        if (ReCode < 0) {
            JD_E("%s(%d): Set flash SPI addrress fail\n", __func__, __LINE__);
            return ReCode;
        }

        /* Read SPI status until done & read SPI data */
        ReCode = jd9366tp_GetFlashSPIFStatus(50, rdata + i);
        if (ReCode < 0) {
            JD_E("%s(%d): Get flash SPI status fail\n", __func__, __LINE__);
            return ReCode;
        }
    }

    /* Set CSN out disable [CSN = H] */
    ReCode = jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_CSN_OUT,
                                        (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_CSN_H);
    if (ReCode < 0) {
        JD_E("%s: Set CSN high fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_ICGetExFlashStatus(uint16_t usTimeOut)
{
    int ReCode;
    uint32_t i = 0;
    uint8_t usValue = (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_FLASH_BUSY;
    bool flash_busy_status_done = false;

    do {
        /* Set CSN out enable [CSN = L] */
        ReCode = jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_CSN_OUT,
                                            (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_CSN_L);
        if (ReCode < 0) {
            JD_E("%s: Set CSN low fail\n", __func__);
            return ReCode;
        }

        /* Set read flash status start */
        ReCode = jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_DMA_2WFLASHEN_1WRITE_0START,
                                            (uint8_t)JD9366TP_DMA_RELATED_SETTING_READ_FLASH_STATUS);
        if (ReCode < 0) {
            JD_E("%s: Set read flash status start fail\n", __func__);
            return ReCode;
        }

        /* Wait Flash Status Register 1 ready */
        mdelay(5);

        /* Set CSN out disable [CSN = H] */
        ReCode = jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_SPI_CSN_OUT,
                                            (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_CSN_H);
        if (ReCode < 0) {
            JD_E("%s: Set CSN high fail\n", __func__);
            return ReCode;
        }

        /* Read Ex-Flash status until done */
        ReCode = jd9366tp_ReadRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_DMA_BUSY_OR_START, &usValue);
        if (ReCode < 0) {
            JD_E("%s: Read Ex-Flash status fail\n", __func__);
            return ReCode;
        } else {
            i += 5;
        }

        flash_busy_status_done = jd9366tp_DoneStatusIsLow(usValue,
                                                            (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_FLASH_BUSY,
                                                            (uint8_t)JD9366TP_MASTER_SPI_RELATED_SETTING_FLASH_DONE);
    } while ((i < usTimeOut) && !flash_busy_status_done);

    if (i >= usTimeOut) {
        JD_E("%s: Get Ex-Flash status timeout\n", __func__);
        return JD_TIME_OUT;
    }

    return ReCode;
}

static int jd9366tp_EraseSector(uint32_t addr)
{
    int ReCode;

    /* Write enable */
    ReCode = jd9366tp_ICWriteExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_WRITE_ENABLE, 0, 0);
    if (ReCode < 0) {
        JD_E("%s: Set write enable fail\n", __func__);
        return ReCode;
    }

    /* Erase cmd */
    ReCode = jd9366tp_ICWriteExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_ERASE_SECTOR, addr, 3);
    if (ReCode < 0) {
        JD_E("%s: Set erase cmd fail\n", __func__);
        return ReCode;
    }

    msleep((uint16_t)JD9366TP_FLASH_RUN_TIME_ERASE_SECTOR_TIME);

    /* Read Ex-Flash status until done */
    ReCode = jd9366tp_ICGetExFlashStatus(50);
    if (ReCode < 0) {
        JD_E("%s: Read Ex-Flash status fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_EraseBlock_32K(uint32_t addr)
{
    int ReCode;

    /* Write enable */
    ReCode = jd9366tp_ICWriteExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_WRITE_ENABLE, 0, 0);
    if (ReCode < 0) {
        JD_E("%s: Set write enable fail\n", __func__);
        return ReCode;
    }

    /* Erase cmd */
    ReCode = jd9366tp_ICWriteExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_ERASE_BLOCK_32K, addr, 3);
    if (ReCode < 0) {
        JD_E("%s: Set erase 32k cmd fail\n", __func__);
        return ReCode;
    }

    msleep((uint16_t)JD9366TP_FLASH_RUN_TIME_ERASE_BLOCK_32K_TIME);

    /* Read Ex-Flash status until done */
    ReCode = jd9366tp_ICGetExFlashStatus(50);
    if (ReCode < 0) {
        JD_E("%s: Read Ex-Flash status fail\n", __func__);
    }
    return ReCode;
}

static int jd9366tp_EraseBlock_64K(uint32_t addr)
{
    int ReCode;

    /* Write enable */
    ReCode = jd9366tp_ICWriteExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_WRITE_ENABLE, 0, 0);
    if (ReCode < 0) {
        JD_E("%s: Set write enable fail\n", __func__);
        return ReCode;
    }

    /* Erase cmd */
    ReCode = jd9366tp_ICWriteExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_ERASE_BLOCK_64K, addr, 3);
    if (ReCode < 0) {
        JD_E("%s: Set erase 64k cmd fail\n", __func__);
        return ReCode;
    }

    msleep((uint16_t)JD9366TP_FLASH_RUN_TIME_ERASE_BLOCK_64K_TIME);

    /* Read Ex-Flash status until done */
    ReCode = jd9366tp_ICGetExFlashStatus(50);
    if (ReCode < 0) {
        JD_E("%s: Read Ex-Flash status fail\n", __func__);
    }
    return ReCode;
}

static int jd9366tp_EraseChip(void)
{
    int ReCode;

    /* Write enable */
    ReCode = jd9366tp_ICWriteExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_WRITE_ENABLE, 0, 0);
    if (ReCode < 0) {
        JD_E("%s: Set write enable fail\n", __func__);
        return ReCode;
    }

    /* Erase cmd */
    ReCode = jd9366tp_ICWriteExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_ERASE_CHIP, 0, 0);
    if (ReCode < 0) {
        JD_E("%s: Set erase chip cmd fail\n", __func__);
        return ReCode;
    }

    msleep((uint16_t)JD9366TP_FLASH_RUN_TIME_ERASE_CHIP_TIME);

    /* Read Ex-Flash status until done */
    ReCode = jd9366tp_ICGetExFlashStatus(500);
    if (ReCode < 0) {
        JD_E("%s: Read Ex-Flash status fail\n", __func__);
    }
    return ReCode;
}

static int jd9366tp_EraseChipFlash(uint32_t addr, uint32_t len)
{
    int ReCode, i;
    int sector_start = 0;
    int sector_end = 0;

    if ((addr + len) > (JD9366TP_SIZE_DEF_BLOCK_64K * 2)) {
        ReCode = jd9366tp_EraseChip();
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

        for (i = sector_start; i < sector_end;/* Acc in loop */) {
            if ((i == 0 || i == 16) && (sector_end - i) >= 16) {
                /* 64KB BLOCK ERASE */
                ReCode = jd9366tp_EraseBlock_64K((uint32_t)(i << 12));
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
                ReCode = jd9366tp_EraseBlock_32K((uint32_t)(i << 12));
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
                ReCode = jd9366tp_EraseSector((uint32_t)(i << 12));
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

static bool jd9366tp_GetSectionFirstValueStatus(uint16_t usTimeOut)
{
    uint32_t i = 0;
    uint8_t rbuf[JD_TWO_SIZE];
    bool section_first_value_status_done = false;

    do {
        /* Pulling section ready == JD9366TP_SECTION_INFO_READY_VALUE */
        mdelay(2);
        if (jd9366tp_ReadRegMulti(g_jd9366tp_section_info.section_info_ready_addr, rbuf, sizeof(rbuf)) < 0) {
            JD_E("%s: Read section info fail\n", __func__);
            return false;
        } else {
            i += 2;
        }

        if (((rbuf[1] << 8) + rbuf[0]) == JD9366TP_SECTION_INFO_READY_VALUE) {
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

static int jd9366tp_GetSectionInfo(uint32_t start_addr, uint8_t section_num, struct JD9366TP_INFO_CONTENT *info_content)
{
    int ReCode, i;
    uint8_t info_content_size = (uint8_t)sizeof(struct JD9366TP_INFO_CONTENT);
    uint8_t *rbuf = NULL;
    int rbuf_len = section_num * info_content_size;

    rbuf = kzalloc(rbuf_len * sizeof(uint8_t), GFP_KERNEL);
    if (rbuf == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

    ReCode = jd9366tp_ReadRegMulti(start_addr, rbuf, rbuf_len);
    if (ReCode < 0) {
        JD_E("%s: Read section info fail\n", __func__);
        kfree(rbuf);
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

    kfree(rbuf);
    return ReCode;
}

static int jd9366tp_ReadSectionInfo(bool reinit_config)
{
    int ReCode, i;
    struct JD9366TP_INFO_CONTENT dsram_info_content[JD9366TP_MAX_DSRAM_NUM];
    struct JD9366TP_INFO_CONTENT esram_info_content[JD9366TP_MAX_ESRAM_NUM];
    uint8_t rbuf[JD_SIX_SIZE];
    bool reinit = false;

    /* 1. Get section ready */
    if (jd9366tp_GetSectionFirstValueStatus(50) == true) {
        /* 2. Get section address */
        jd9366tp_GetSectionInfo(g_jd9366tp_section_info.dsram_section_info_start_addr,
                                JD9366TP_REPAIR_MODE_SWITCH + 1, &dsram_info_content[0]);

        jd9366tp_GetSectionInfo(g_jd9366tp_section_info.esram_section_info_start_addr,
                                JD9366TP_ESRAM_DDREG_DATA_BUF + 1, &esram_info_content[0]);

        /* 2.1 Dump section info */
        for (i = 0; i <= JD9366TP_REPAIR_MODE_SWITCH; i++) {
            /* Mapping to enum JD9366TP_DSRAM_SECTION_INFO_ORDER */
            JD_D("%s: Dsram section(%d) Address: 0x%04x, Length: %d\n", __func__, i,
                dsram_info_content[i].info_content_addr,
                dsram_info_content[i].info_content_len);
        }

        for (i = 0; i <= JD9366TP_ESRAM_DDREG_DATA_BUF; i++) {
            /* Mapping to enum JD9366TP_ESRAM_SECTION_INFO_ORDER */
            JD_D("%s: Esram section(%d) Address: 0x%04x, Length: %d\n", __func__, i,
                esram_info_content[i].info_content_addr,
                esram_info_content[i].info_content_len);
        }

#ifdef JD_READ_SLAVE_BY_MASTER
        g_jd9366tp_chip_info.esram_info_content_addr.slave_data_addr =
            esram_info_content[JD9366TP_ESRAM_DDREG_DATA_BUF].info_content_addr;
#endif

        /* 3.1 Set Coordinate addr. and length */
        g_jd9366tp_chip_info.esram_info_content_addr.coordinate_report =
            esram_info_content[JD9366TP_ESRAM_COORDINATE_REPORT].info_content_addr;
        pjadard_report_data->touch_data_size = esram_info_content[JD9366TP_ESRAM_COORDINATE_REPORT].info_content_len;

        if (pjadard_report_data->touch_data_size > JD_TOUCH_DATA_SIZE) {
            JD_E("%s: FW report packet size %d is greater than Driver %d\n", __func__,
                pjadard_report_data->touch_data_size, JD_TOUCH_DATA_SIZE);
        }

        /* 3.2 Set ADC number and resolution */
        ReCode = jd9366tp_ReadRegMulti(dsram_info_content[JD9366TP_DSRAM_TOUCH_CFG].info_content_addr,
                                        rbuf, sizeof(rbuf));
        if (ReCode < 0) {
            JD_E("%s: Update global config fail\n", __func__);
        } else {
            /* Update global config */
            if ((pjadard_ic_data) && (pjadard_ts_data) && (pjadard_ts_data->pdata)) {
                if (reinit_config) {
                    pjadard_ic_data->JD_X_NUM = rbuf[4];
                    pjadard_ic_data->JD_Y_NUM = rbuf[5];
                    pjadard_ts_data->pdata->abs_x_min = 0;
                    pjadard_ts_data->pdata->abs_x_max = (int)((rbuf[1] << 8) + rbuf[0]);
                    pjadard_ts_data->pdata->abs_y_min = 0;
                    pjadard_ts_data->pdata->abs_y_max = (int)((rbuf[3] << 8) + rbuf[2]);

                    if (pjadard_ic_data->JD_X_RES != pjadard_ts_data->pdata->abs_x_max) {
                        pjadard_ic_data->JD_X_RES = pjadard_ts_data->pdata->abs_x_max;
                        reinit = true;
                    }

                    if (pjadard_ic_data->JD_Y_RES != pjadard_ts_data->pdata->abs_y_max) {
                        pjadard_ic_data->JD_Y_RES = pjadard_ts_data->pdata->abs_y_max;
                        reinit = true;
                    }

                    if (reinit) {
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
                }
            } else {
                JD_E("%s: Can`t update global config, pjadard_ic_data/pjadard_ts_data/pdata was null\n", __func__);
            }

            /* Record JD9366TP_DSRAM_HOST_ADDR */
            g_jd9366tp_chip_info.dsram_host_addr.fw_cid_version =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 12;
            g_jd9366tp_chip_info.dsram_host_addr.fw_version =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 16;
            g_jd9366tp_chip_info.dsram_host_addr.game_mode_en =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 30;
            g_jd9366tp_chip_info.dsram_host_addr.usb_en =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 32;
            g_jd9366tp_chip_info.dsram_host_addr.gesture_en =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 34;
            g_jd9366tp_chip_info.dsram_host_addr.high_sensitivity_en =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 36;
            g_jd9366tp_chip_info.dsram_host_addr.border_en =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 38;
            g_jd9366tp_chip_info.dsram_host_addr.proximity_en =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 44;
            g_jd9366tp_chip_info.dsram_host_addr.panel_maker =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 56;
            g_jd9366tp_chip_info.dsram_host_addr.panel_version =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 57;
            g_jd9366tp_chip_info.dsram_host_addr.earphone_en =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 58;
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
            g_jd9366tp_chip_info.dsram_host_addr.mpap_pw =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 40;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_handshake =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 42;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_keep_frame =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 43;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_skip_frame =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 52;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_mux_switch =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 53;
            g_jd9366tp_chip_info.dsram_host_addr.fw_dbic_en =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 54;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_pw_sync =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 76;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_sync_status =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 77;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_error_msg =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 78;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_bypass_main =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 84;
            /* Set mapa diff max/min addr */
            g_jd9366tp_chip_info.dsram_host_addr.mpap_diff_max =
                dsram_info_content[JD9366TP_DSRAM_MPAP_DIFF_MAX_M].info_content_addr;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_diff_max_s =
                dsram_info_content[JD9366TP_DSRAM_MPAP_DIFF_MAX_S].info_content_addr;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_diff_min =
                dsram_info_content[JD9366TP_DSRAM_MPAP_DIFF_MIN_M].info_content_addr;
            g_jd9366tp_chip_info.dsram_host_addr.mpap_diff_min_s =
                dsram_info_content[JD9366TP_DSRAM_MPAP_DIFF_MIN_S].info_content_addr;
#endif
            g_jd9366tp_chip_info.dsram_host_addr.data_output_v2 =
                dsram_info_content[JD9366TP_DSRAM_HOST].info_content_addr + 86;
            /* Record JD9366TP_DSRAM_DEBUG_ADDR */
            g_jd9366tp_chip_info.dsram_debug_addr.output_data_addr =
                dsram_info_content[JD9366TP_DSRAM_DEBUG].info_content_addr + 28;
            g_jd9366tp_chip_info.dsram_debug_addr.output_data_sel =
                dsram_info_content[JD9366TP_DSRAM_DEBUG].info_content_addr + 34;
            g_jd9366tp_chip_info.dsram_debug_addr.output_data_handshake =
                dsram_info_content[JD9366TP_DSRAM_DEBUG].info_content_addr + 36;
            g_jd9366tp_chip_info.dsram_debug_addr.freq_band =
                dsram_info_content[JD9366TP_DSRAM_DEBUG].info_content_addr + 40;
#ifdef JD_READ_SLAVE_BY_MASTER
            g_jd9366tp_chip_info.dsram_debug_addr.otp_len =
                dsram_info_content[JD9366TP_DSRAM_DEBUG].info_content_addr + 70;
            g_jd9366tp_chip_info.dsram_debug_addr.ip_stress_addr =
                dsram_info_content[JD9366TP_DSRAM_DEBUG].info_content_addr + 84;
#endif

            /* Get esram_output_buf.output_buf.data address */
            ReCode = jd9366tp_ReadRegMulti(g_jd9366tp_chip_info.dsram_debug_addr.output_data_addr, rbuf, 4);
            if (ReCode < 0) {
                JD_E("%s: Get output buf data fail\n", __func__);
            } else {
                g_jd9366tp_chip_info.dsram_debug_addr.output_data_addr = ((rbuf[3] << 24) |
                                                                         (rbuf[2] << 16) |
                                                                         (rbuf[1] << 8) |
                                                                         (rbuf[0]));
                JD_I("%s: Section info was ready\n", __func__);
            }
        }
    } else {
        JD_E("%s: Section info was not ready\n", __func__);
        ReCode = JD_APP_START_FAIL;
    }

    return ReCode;
}

static int jd9366tp_ClearSectionFirstValue(uint32_t offset)
{
    return jd9366tp_WriteRegSingle(g_jd9366tp_section_info.section_info_ready_addr + offset, 0x00);
}

static int jd9366tp_SetCRCInitial(void)
{
    uint8_t wBuf[JD_ONE_SIZE];

    wBuf[0] = 0x01;
    return jd9366tp_WriteRegMulti((uint32_t)JD9366TP_FLASH_REG_ADDR_CRC_INIT, wBuf, sizeof(wBuf));
}

static int jd9366tp_SetCRCInitialValue(void)
{
    uint8_t wBuf[JD_TWO_SIZE];

    wBuf[(uint32_t)JD9366TP_CRC_CODE_POSITION_HIGH_BYTE] =
        (uint8_t)((JD9366TP_CRC_INFO_CRC_INITIAL_VALUE & 0xFF00) >> 8);
    wBuf[(uint32_t)JD9366TP_CRC_CODE_POSITION_LOW_BYTE] =
        (uint8_t)((JD9366TP_CRC_INFO_CRC_INITIAL_VALUE & 0x00FF) >> 0);

    return jd9366tp_WriteRegMulti((uint32_t)JD9366TP_FLASH_REG_ADDR_CRC_INIT_CODE0, wBuf, sizeof(wBuf));
}

static int jd9366tp_SetCRCEnable(bool enable)
{
    return jd9366tp_WriteRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_CRC_ENABLE, enable);
}

static int jd9366tp_GetCRCResult(uint16_t *crc)
{
    int ReCode;
    uint8_t rBuf[JD_TWO_SIZE];

    ReCode = jd9366tp_ReadRegMulti((uint32_t)JD9366TP_FLASH_REG_ADDR_CRC_CHK0, rBuf, sizeof(rBuf));
    if (ReCode < 0) {
        *crc = 0;
    } else {
        *crc = (uint16_t)((rBuf[(uint32_t)JD9366TP_CRC_CODE_POSITION_HIGH_BYTE] << 8) +
                            rBuf[(uint32_t)JD9366TP_CRC_CODE_POSITION_LOW_BYTE]);
    }

    return ReCode;
}

static void jd9366tp_GetDataCRC16(uint8_t *buf, uint32_t buf_len, uint32_t start_index,
                                    uint32_t len, uint16_t *crc, uint16_t PolynomialCRC16)
{
    uint32_t i, j;
    uint16_t sum = *crc;

    if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
        start_index &= 0xF0FFFFFF;
    }

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

static uint32_t jd9366tp_Pow(uint8_t base, uint32_t pow)
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

static int jd9366tp_JEDEC_ID(uint8_t *id)
{
    int ReCode;
    uint8_t rdbuf[JD_THREE_SIZE];

    ReCode = jd9366tp_ICReadExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_READ_ID, 0, 0, 0, 0, rdbuf, sizeof(rdbuf));
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
        JD_I("Flash capacity: %d KB\n", jd9366tp_Pow(2, rdbuf[2] - 10));
    }

    /* Read flash status1 status*/
    ReCode = jd9366tp_ICReadExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_STATUS_REGISTER1, 0, 0, 0, 0, rdbuf, 2);
    if (ReCode < 0) {
        JD_E("%s: Read external flash status1 fail\n", __func__);
        return ReCode;
    }

    JD_I("Flash Status1: 0x%02x ,0x%02x\n", rdbuf[0], rdbuf[1]);

    /* Read flash status2 status*/
    ReCode = jd9366tp_ICReadExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_STATUS_REGISTER2, 0, 0, 0, 0, rdbuf, 2);
    if (ReCode < 0) {
        JD_E("%s: Read external flash status2 fail\n", __func__);
        return ReCode;
    }

    JD_I("Flash Status2: 0x%02x ,0x%02x\n", rdbuf[0], rdbuf[1]);

    return ReCode;
}

static int jd9366tp_CheckSize(uint8_t data, uint32_t length)
{
    int ReCode = JD_NO_ERR;

    if (jd9366tp_Pow(2, data) < length) {
        ReCode = JD_WRITE_OVERFLOW;
        JD_E("%s: Write size was overflow\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_GetDMAStatus(uint16_t usTimeOut)
{
    int ReCode;
    uint32_t i = 0;
    uint8_t usValue = (uint8_t)JD9366TP_DMA_RELATED_SETTING_DMA_BUSY;
    bool dma_busy_status_done = false;

    do {
        /* Pulling DMA busy = JD9366TP_DMA_RELATED_SETTING_DMA_DONE */
        mdelay(2);
        ReCode = jd9366tp_ReadRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_DMA_BUSY_OR_START, &usValue);

        if (ReCode < 0) {
            JD_E("%s: Read DMA_BUSY_OR_START register fail\n", __func__);
            return ReCode;
        } else {
            i += 2;
        }

        dma_busy_status_done = jd9366tp_DoneStatusIsLow(usValue, (uint8_t)JD9366TP_DMA_RELATED_SETTING_DMA_BUSY,
                                                        (uint8_t)JD9366TP_DMA_RELATED_SETTING_DMA_DONE);
    } while ((i < usTimeOut) && !dma_busy_status_done);

    if (i >= usTimeOut) {
        JD_E("%s: Get DMA status timeout\n", __func__);
        return JD_TIME_OUT;
    }

    return ReCode;
}

static int jd9366tp_GetPageProgramStatus(uint16_t usTimeOut)
{
    int ReCode;
    uint32_t i = 0;
    uint8_t usValue = (uint8_t)JD9366TP_DMA_RELATED_SETTING_PAGE_PROGRAM_FLASH;
    bool dma_page_program_status_done = false;

    do {
        /* Pulling DMA Page Program = JD9366TP_DMA_RELATED_SETTING_PAGE_PROGRAM_DONE */
        mdelay(2);
        ReCode = jd9366tp_ReadRegSingle((uint32_t)JD9366TP_FLASH_REG_ADDR_DMA_2WFLASHEN_1WRITE_0START, &usValue);

        if (ReCode < 0) {
            JD_E("%s: Read DMA_2WFLASHEN_1WRITE_0START register fail\n", __func__);
            return ReCode;
        } else {
            i += 2;
        }

        dma_page_program_status_done = jd9366tp_DoneStatusIsLow(usValue, (uint8_t)JD9366TP_DMA_RELATED_SETTING_PAGE_PROGRAM_FLASH,
                                                                (uint8_t)JD9366TP_DMA_RELATED_SETTING_PAGE_PROGRAM_DONE);
    } while ((i < usTimeOut) && !dma_page_program_status_done);

    if (i >= usTimeOut) {
        JD_E("%s: Get DMA status timeout\n", __func__);
        return JD_TIME_OUT;
    }

    return ReCode;
}

static void jd9366tp_ReArrangePageProgramInfo(uint32_t ori_addr, uint32_t ori_len , uint32_t *page_addr , uint32_t *page_len)
{
    *page_addr = ori_addr - (ori_addr % JD9366TP_SIZE_DEF_PAGE_SIZE);
    *page_len = ((ori_len + (ori_addr % JD9366TP_SIZE_DEF_PAGE_SIZE)) % JD9366TP_SIZE_DEF_PAGE_SIZE) > 0 ?
                ((((ori_len + (ori_addr % JD9366TP_SIZE_DEF_PAGE_SIZE)) >> 8) + 1) << 8) :
                (ori_len + (ori_addr % JD9366TP_SIZE_DEF_PAGE_SIZE));
}

static int jd9366tp_SetDMAStart(uint32_t flash_addr, uint32_t dma_size, uint32_t target_addr, uint8_t dma_cmd)
{
    int ReCode;
    uint8_t wBuf[JD_SIX_SIZE + JD_SEVEN_SIZE];

    if (dma_cmd == (uint8_t)JD9366TP_DMA_RELATED_SETTING_WRITE_TO_FLASH) {
        ReCode = jd9366tp_ICWriteExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_WRITE_ENABLE, 0, 0);
        if (ReCode < 0) {
            JD_E("%s: Set write enable fail\n", __func__);
            return ReCode;
        }
    }

    if ((dma_cmd == (uint8_t)JD9366TP_DMA_RELATED_SETTING_READ_FROM_FLASH) ||
        (dma_cmd == (uint8_t)JD9366TP_DMA_RELATED_SETTING_WRITE_TO_FLASH)) {
        ReCode = jd9366tp_ICSetExFlashCSNOutEnable();
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

    ReCode = jd9366tp_WriteRegMulti((uint32_t)JD9366TP_FLASH_REG_ADDR_DMA_FLASH_ADDR0, wBuf, sizeof(wBuf));
    if (ReCode < 0) {
        JD_E("%s: Set DMA parameters fail\n", __func__);
        return ReCode;
    }

    mdelay(50);

    if (dma_cmd == (uint8_t)JD9366TP_DMA_RELATED_SETTING_PAGE_PROGRAM_FLASH) {
        /* Pulling DMA Page Program = JD9366TP_DMA_RELATED_SETTING_PAGE_PROGRAM_DONE */
        mdelay(1);
        ReCode = jd9366tp_GetPageProgramStatus(100);
        if (ReCode < 0) {
            JD_E("%s: Get DMA page program status fail\n", __func__);
            return ReCode;
        }
    } else {
        /* Pulling DMA busy = JD9366TP_DMA_RELATED_SETTING_DMA_DONE */
        ReCode = jd9366tp_GetDMAStatus(50);
        if (ReCode < 0) {
            JD_E("%s: Get DMA status fail\n", __func__);
            return ReCode;
        }
    }

    if ((dma_cmd == (uint8_t)JD9366TP_DMA_RELATED_SETTING_READ_FROM_FLASH) ||
        (dma_cmd == (uint8_t)JD9366TP_DMA_RELATED_SETTING_WRITE_TO_FLASH)) {
        ReCode = jd9366tp_ICSetExFlashCSNOutDisable();
        if (ReCode < 0) {
            JD_E("%s: Set CSN disable fail\n", __func__);
        }
    }

    return ReCode;
}

static int jd9366tp_SetDMAByteMode(uint32_t offset)
{
    uint8_t wBuf[JD_ONE_SIZE];

    wBuf[0] = (uint8_t)JD9366TP_DMA_RELATED_SETTING_TRANSFER_DATA_1_BYTE_MODE;

    return jd9366tp_WriteRegMulti((uint32_t)(JD9366TP_FLASH_REG_ADDR_DMA_BYTE_MODE + offset), wBuf, sizeof(wBuf));
}

static int jd9366tp_AbortHostDMA(uint32_t offset)
{
    uint8_t wBuf[JD_ONE_SIZE];

    wBuf[0] = (uint8_t)JD9366TP_DMA_RELATED_SETTING_DMA_ABORT;

    return jd9366tp_WriteRegMulti((uint32_t)(JD9366TP_FLASH_REG_ADDR_DMA_0ABORT + offset), wBuf, sizeof(wBuf));
}

static uint8_t jd9366tp_GetMoveInfoNumber(uint8_t *pFileData)
{
    if (pFileData[JD9366TP_HW_HEADER_LEN] == 0xFF) {
        return 0;
    } else {
        return (uint8_t)((pFileData[JD9366TP_HW_HEADER_LEN] - JD9366TP_HW_HEADER_CRC_NUMBER) /
                            JD9366TP_HW_HEADER_MOVE_INFO_NUMBER);
    }
}

static void jd9366tp_GetHeaderInfo(uint8_t *pFileData, struct JD9366TP_MOVE_INFO *move_info, int move_info_sz)
{
    int i;
    int header_shift = JD9366TP_HW_HEADER_CRC_NUMBER + JD9366TP_HW_HEADER_INITIAL_NUMBER;

    for (i = 0; i < move_info_sz; i++) {
        move_info[i].mov_mem_cmd =
            pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 0];

        move_info[i].fl_st_addr =
            (uint32_t)((pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 1] << 16) +
                        (pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 2] << 8) +
                        (pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 3] << 0));

        move_info[i].fl_len =
            (uint32_t)((pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 4] << 8) +
                        (pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 5] << 0) + 1);

        move_info[i].to_mem_st_addr =
            (uint32_t)((pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 6] << 24) +
                        (pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 7] << 16) +
                        (pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 8] << 8) +
                        (pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 9] << 0));

        move_info[i].to_mem_crc =
            (uint16_t)((pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 10] << 8) +
                        (pFileData[header_shift + i * JD9366TP_HW_HEADER_MOVE_INFO_NUMBER + 11] << 0));
    }
}

static uint8_t jd9366tp_GetHeaderPsramIndex(struct JD9366TP_MOVE_INFO *move_info, int move_info_sz)
{
    int i;
    uint8_t index = 0;

    if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
        for (i = 0; i < move_info_sz; i++) {
            if ((move_info[i].to_mem_st_addr & 0xF0FFFFFF) == JD9366TP_HW_HEADER_PSRAM_START_ADDR) {
                index = (uint8_t)i;
                break;
            }
        }
    } else {
        for (i = 0; i < move_info_sz; i++) {
            if (move_info[i].to_mem_st_addr == JD9366TP_HW_HEADER_PSRAM_START_ADDR) {
                index = (uint8_t)i;
                break;
            }
        }
    }

    JD_I("%s: Header Psram Index = %d\n", __func__, index);
    return index;
}

static uint8_t jd9366tp_GetMoveInfoPsramNumber(struct JD9366TP_MOVE_INFO *move_info, int move_info_sz)
{
    int i;
    uint8_t num = 0;

    for (i = 0; i < move_info_sz; i++) {
        if ((move_info[i].to_mem_st_addr & 0xF0000000) == 0x00000000)
            num++;
    }

    return num;
}

static int jd9366tp_ReadFlash(uint32_t addr, uint8_t *rdata, uint32_t rlen)
{
    int ReCode;
    uint16_t packsize = (uint16_t)JD9366TP_FLASH_PACK_READ_SIZE;
    uint32_t pos = 0;
    uint16_t packlen = 0;
    /* MCU aram config */
    uint16_t ramsize = 2048;
    uint32_t ramaddr = 0;
    uint16_t ramlen = 0;
    /* DMA config */
    uint32_t flashaddr = addr;
    uint32_t targetaddr = (uint32_t)JD9366TP_MEMORY_ADDR_ARAM;
    /* Read buf */
    uint8_t *rBuf = NULL;

    rBuf = kzalloc(packsize * sizeof(uint8_t), GFP_KERNEL);
    if (rBuf == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

    /* Set CRC initial value */
    ReCode = jd9366tp_SetCRCInitialValue();
    if (ReCode < 0) {
        JD_E("%s: Set CRC initial value fail\n", __func__);
        kfree(rBuf);
        return ReCode;
    }

    /* Set CRC Initial */
    ReCode = jd9366tp_SetCRCInitial();
    if (ReCode < 0) {
        JD_E("%s: Set CRC initial fail\n", __func__);
        kfree(rBuf);
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
        ramaddr = (uint32_t)JD9366TP_MEMORY_ADDR_ARAM;

        /* Set DMA Start & Pulling DMA busy = JD9366TP_DMA_RELATED_SETTING_DMA_DONE */
        JD_D("%s: Flash DMA start\n", __func__);

        ReCode = jd9366tp_SetDMAStart(flashaddr, ramlen, targetaddr,
                                        (uint8_t)JD9366TP_DMA_RELATED_SETTING_READ_FROM_FLASH);
        if (ReCode < 0) {
            JD_E("%s: DMA start error\n", __func__);
            kfree(rBuf);
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
            ReCode = jd9366tp_ReadRegMulti(ramaddr, rBuf, packlen);
            if (ReCode < 0) {
                JD_E("%s: Read ARAM data fail\n", __func__);
                kfree(rBuf);
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

    kfree(rBuf);
    return ReCode;
}

static int jd9366tp_PageProgramFlash(uint32_t addr, uint32_t wlen, uint8_t *data, uint32_t data_len)
{
    int ReCode, i;
    uint16_t packsize = (uint16_t)JD9366TP_FLASH_PACK_WRITE_SIZE;
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
    uint32_t targetaddr = (uint32_t)JD9366TP_MEMORY_ADDR_ARAM;
    /* Write buf */
    uint8_t *wBuf = NULL;

    wBuf = kzalloc(packsize * sizeof(uint8_t), GFP_KERNEL);
    if (wBuf == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

    /* Set CRC initial value */
    ReCode = jd9366tp_SetCRCInitialValue();
    if (ReCode < 0) {
        JD_E("%s: Set CRC initial value fail\n", __func__);
        kfree(wBuf);
        return ReCode;
    }

    /* Set CRC Initial */
    ReCode = jd9366tp_SetCRCInitial();
    if (ReCode < 0) {
        JD_E("%s: Set CRC initial fail\n", __func__);
        kfree(wBuf);
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
        ramaddr = (uint32_t)JD9366TP_MEMORY_ADDR_ARAM;

        /* Write to ram */
        while (ramlen > 0) {
            if (ramlen > packsize)
                packlen = packsize;
            else
                packlen = (uint16_t)ramlen;

            for (i = 0; i < packlen; i++) {
                if ((pos + i) >= data_len)
                    wBuf[i] = 0xFF;
                else {
                    if (data_len > addr)
                        wBuf[i] = data[pos + i];
                    else
                        wBuf[i] = data[i];
                }
            }

            /* Write Data to ARAM */
            ReCode = jd9366tp_WriteRegMulti(ramaddr, wBuf, packlen);
            if (ReCode < 0) {
                JD_E("%s: Write data to ARAM fail\n", __func__);
                kfree(wBuf);
                return ReCode;
            }

            /* Set next packet */
            ramlen -= packlen;
            pos += packlen;
            ramaddr += packlen;
            addr += packlen;
        }

        /* Set DMA Start & Pulling DMA busy = JD9366TP_DMA_RELATED_SETTING_DMA_DONE */
        ReCode = jd9366tp_SetDMAStart(flashaddr, dmasize, targetaddr,
                                        (uint8_t)JD9366TP_DMA_RELATED_SETTING_PAGE_PROGRAM_FLASH);
        if (ReCode < 0) {
            JD_E("%s: DMA start error\n", __func__);
            kfree(wBuf);
            return ReCode;
        } else {
            JD_D("%s: Flash DMA start\n", __func__);
        }

        JD_D("%s: Write flash data %d KB...\n", __func__, (uint16_t)((flashaddr - start_flashaddr) / 1024));
    }

    /* ReCode = jd9366tp_ICWriteExFlashFlow((uint8_t)JD9366TP_EX_FLASH_ADDR_WRITE_DISABLE, 0, 0);
    if (ReCode < 0) {
        JD_E("%s: Write flash disable fail\n", __func__);
    } */

    kfree(wBuf);
    return ReCode;
}

static int jd9366tp_CheckFlashContent(uint8_t *pFileData, uint32_t addr, uint32_t len)
{
    int ReCode, i;
    uint32_t error_count = 0;
    uint8_t *pData = kzalloc(len * sizeof(uint8_t), GFP_KERNEL);

    if (pData == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

    ReCode = jd9366tp_ReadFlash(addr, pData, len);
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
                return JD_CHECK_DATA_ERROR;
            }
        }
    }

    kfree(pData);
    return ReCode;
}

static int jd9366tp_HostReadFlash(uint32_t ReadAddr, uint8_t *pDataBuffer, uint32_t ReadLen)
{
    int ReCode;
    uint8_t tcb_speed;
    uint16_t romid = 0;
    uint16_t crc, softcrc;
    uint32_t AddrOffset;

    /* 1. Enter backdoor & 2. Read IC ID */
    ReCode = jd9366tp_EnterBackDoor(&romid);
    if (ReCode < 0)
        return ReCode;

    /* 2.1 read IC is cascade or not */
    ReCode = jd9366tp_ReadChipCascadeMode(&(pjadard_ic_data->JD_MODULE_CASCADE_MODE), JD9366TP_MASTER_ADDR_OFFSET);
    if (ReCode < 0) {
        JD_E("%s: Read cascade mode fail\n", __func__);
        return ReCode;
    } else {
        JD_I("%s: Cascade mode %d\n", __func__, pjadard_ic_data->JD_MODULE_CASCADE_MODE);
    }

    /* 3.1 IC RTC run off */
    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;
    ReCode = jd9366tp_DisableRTCRun(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    /* 3.2 IC touch scan off */
    ReCode = jd9366tp_DisableTouchScan(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    /* 3.3 Set DMA 1-byte mode */
    ReCode = jd9366tp_SetDMAByteMode(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    /* 4.1 Stop WDT */
    ReCode = jd9366tp_DisableWDTRun(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    }

    /* 4.2 Stop MCU clock */
    ReCode = jd9366tp_StopMCUClock(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    }

    /* 4.3 Set TCB speed to 16M */
    jd9366tp_GetTCBSpeed(JD9366TP_MASTER_ADDR_OFFSET, &tcb_speed);
    ReCode = jd9366tp_SetTCBSpeed(AddrOffset, 0);
    if (ReCode < 0) {
         JD_E("%s: TCB spi 16M fail\n", __func__);
        return ReCode;
    }

    /* 4.4 read slave IC id */
    if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
        ReCode = jd9366tp_GetID(&romid, JD9366TP_SLAVE_ADDR_OFFSET);

        if (ReCode < 0) { /* Retry */
            msleep(10);
            ReCode = jd9366tp_GetID(&romid, JD9366TP_SLAVE_ADDR_OFFSET);

            if (ReCode < 0) {
                JD_E("%s: Check slave ICID fail\n", __func__);
                return ReCode;
            }
        }
    }

    /* 5. Set Flash SPI speed & 6. Read Flash id */
    ReCode = jd9366tp_JEDEC_ID(NULL);
    if (ReCode < 0) {
        return ReCode;
    }

    /* 7. Read data from flash */
    JD_I("%s: Read flash start\n", __func__);
    ReCode = jd9366tp_ReadFlash(ReadAddr, pDataBuffer, ReadLen);
    if (ReCode < 0) {
        JD_E("%s: Read flash error\n", __func__);
        return ReCode;
    } else {
        JD_I("%s: Read flash finish\n", __func__);
    }

    /* 8.1 Get IC crc */
    ReCode = jd9366tp_GetCRCResult(&crc);
    if (ReCode < 0) {
        JD_E("%s: Read flash ecc fail\n", __func__);
        return ReCode;
    }

    /* 8.2 Calculate software crc */
    softcrc = JD9366TP_CRC_INFO_CRC_INITIAL_VALUE;
    jd9366tp_GetDataCRC16(pDataBuffer, ReadLen, 0, ReadLen, &softcrc, JD9366TP_CRC_INFO_PolynomialCRC16);

    /* 8.3 Compare IC and software crc */
    if (softcrc != crc) {
        JD_E("%s: softEcc is %04x, but read IC crc is %04x\n", __func__, softcrc, crc);
    }

    /* 9.0 Reload TCB speed */
    ReCode = jd9366tp_SetTCBSpeed(AddrOffset, tcb_speed);
    if (ReCode < 0) {
         JD_E("%s: TCB spi reload fail\n", __func__);
        return ReCode;
    }

    /* 9.1 IC RTC run on */
    ReCode = jd9366tp_EnableRTCRun(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    /* 9.2 IC touch scan on */
    ReCode = jd9366tp_EnableTouchScan(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    /* 9.3 start MCU clock */
    ReCode = jd9366tp_StartMCUClock(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    return ReCode;
}

static int jd9366tp_HostWriteFlash(uint32_t WriteAddr, uint8_t *pFileData, uint32_t FileSize)
{
    int retry = 0;
    int i, ReCode;
    uint32_t AddrOffset;
    uint32_t PageProgram_addr = 0;
    uint32_t PageProgram_len = 0;
    uint16_t romid = 0;
    uint16_t crc, softcrc;
    uint8_t *crcbuffer = NULL;
    uint8_t id;
    uint8_t MoveInfoNumber;
    uint8_t MoveInfoPsramIndex = 0;
    uint8_t MoveInfoPsramNumber = 0;
    struct JD9366TP_MOVE_INFO *MoveInfo = NULL;

    do {
        /* 0. Check WriteAddr is a multiple of 256 */
        if (WriteAddr % JD9366TP_SIZE_DEF_PAGE_SIZE != 0) {
            JD_E("%s: WriteAddr: 0x%08x was not a multiple of 256\n", __func__, WriteAddr);
            return JD_CHECK_DATA_ERROR;
        }

        /* 1. Enter backdoor & 2. Read IC ID */
        ReCode = jd9366tp_EnterBackDoor(&romid);
        if (ReCode < 0)
            return ReCode;

        /* 2.1 read IC is cascade or not */
        ReCode = jd9366tp_ReadChipCascadeMode(&(pjadard_ic_data->JD_MODULE_CASCADE_MODE), JD9366TP_MASTER_ADDR_OFFSET);
        if (ReCode < 0) {
            JD_E("%s: Read cascade mode fail\n", __func__);
            return ReCode;
        } else {
            JD_I("%s: Cascade mode %d\n", __func__, pjadard_ic_data->JD_MODULE_CASCADE_MODE);
        }

        /* 3.1 IC RTC run off */
        AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                      ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;
        ReCode = jd9366tp_DisableRTCRun(AddrOffset);
        if (ReCode < 0)
            return ReCode;

        /* 3.2 IC touch scan off */
        ReCode = jd9366tp_DisableTouchScan(AddrOffset);
        if (ReCode < 0)
            return ReCode;

        /* 3.3 Set DMA 1-byte mode */
        ReCode = jd9366tp_SetDMAByteMode(AddrOffset);
        if (ReCode < 0)
            return ReCode;

        /* 3.4 Clear section first value */
        ReCode = jd9366tp_ClearSectionFirstValue(AddrOffset);
        if (ReCode < 0)
            return ReCode;

        /* 4.1 Stop WDT */
        ReCode = jd9366tp_DisableWDTRun(AddrOffset);
        if (ReCode < 0) {
            return ReCode;
        }

        /* 4.2 Stop MCU clock */
        ReCode = jd9366tp_StopMCUClock(AddrOffset);
        if (ReCode < 0) {
            return ReCode;
        }

        /* 4.3 Set TCB speed to 16M */
        ReCode = jd9366tp_SetTCBSpeed(AddrOffset, 0);
        if (ReCode < 0) {
             JD_E("%s: TCB spi 16M fail\n", __func__);
            return ReCode;
        }

        /* 4.4 read slave IC id */
        if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
            ReCode = jd9366tp_GetID(&romid, JD9366TP_SLAVE_ADDR_OFFSET);

            if (ReCode < 0) { /* Retry */
                msleep(10);
                ReCode = jd9366tp_GetID(&romid, JD9366TP_SLAVE_ADDR_OFFSET);

                if (ReCode < 0) {
                    JD_E("%s: Check slave ICID fail\n", __func__);
                    return ReCode;
                }
            }
        }

        /* 5. Abort Host DMA (inital DMA) */
        ReCode = jd9366tp_AbortHostDMA(AddrOffset);
        if (ReCode < 0) {
            return ReCode;
        }

        /* 6. Set Flash SPI speed & Read Flash id */
        ReCode = jd9366tp_JEDEC_ID(&id);
        if (ReCode < 0) {
            return ReCode;
        }

        /* 6.1 Check flash size is enough */
        ReCode = jd9366tp_CheckSize(id, FileSize);
        if (ReCode < 0) {
            return ReCode;
        }

        /* 7. Erase flash */
        JD_I("%s: Erase flash start, Start Address: 0x%08x, Size: %d\n", __func__, WriteAddr, FileSize);
        ReCode = jd9366tp_EraseChipFlash(WriteAddr, FileSize);
        if (ReCode < 0) {
            JD_E("%s: Flash erase fail\n", __func__);
            return ReCode;
        } else {
            JD_I("%s: Erase Flash completed\n", __func__);
        }

        /* 8. Set CRC disable */
        ReCode =  jd9366tp_SetCRCEnable(false);
        if (ReCode < 0) {
            JD_E("%s: CRC disable fail\n", __func__);
            return ReCode;
        }

        /* 9. Set Flash SPI speed */
        ReCode = jd9366tp_SetFlashSPISpeed(pFileData[JD9366TP_LOAD_CODE_SPI_FREQ]);
        if (ReCode < 0) {
            return ReCode;
        }

        /* 10. Get header info */
        MoveInfoNumber = jd9366tp_GetMoveInfoNumber(pFileData);
        if (MoveInfoNumber == 0) {
            JD_E("%s: Header information error, please check FW bin!\n", __func__);
            return JD_CHECK_DATA_ERROR;
        }

        MoveInfo = kzalloc(MoveInfoNumber * sizeof(struct JD9366TP_MOVE_INFO), GFP_KERNEL);
        if (MoveInfo == NULL) {
            JD_E("%s: Memory allocate fail\n", __func__);
            return JD_MEM_ALLOC_FAIL;
        }
        jd9366tp_GetHeaderInfo(pFileData, MoveInfo, MoveInfoNumber);

        /* 11. write data to flash-1 */
        JD_I("%s: Write flash start\n", __func__);
        MoveInfoPsramNumber = jd9366tp_GetMoveInfoPsramNumber(MoveInfo, MoveInfoNumber);
        MoveInfoPsramIndex = jd9366tp_GetHeaderPsramIndex(MoveInfo, MoveInfoNumber);
        for (i = 0; i < MoveInfoPsramNumber; i++) {
            jd9366tp_ReArrangePageProgramInfo((uint32_t)(MoveInfo[MoveInfoPsramIndex + i].fl_st_addr - JD9366TP_HW_HEADER_MAX_SIZE),
                                              (uint32_t)(JD9366TP_HW_HEADER_MAX_SIZE + MoveInfo[MoveInfoPsramIndex + i].fl_len),
                                              &PageProgram_addr, &PageProgram_len);
            ReCode = jd9366tp_PageProgramFlash(PageProgram_addr, PageProgram_len, pFileData, FileSize);
            if (ReCode < 0) {
                JD_E("%s: Write flash fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            } else {
                JD_I("%s: Write flash finish\n", __func__);
            }
            if (FileSize >= (PageProgram_addr + PageProgram_len)) {
                /* 12.1 get IC crc */
                ReCode = jd9366tp_GetCRCResult(&crc);
                if (ReCode < 0) {
                    JD_E("%s: Read flash ecc fail\n", __func__);
                    kfree(MoveInfo);
                    return ReCode;
                }

                /* 12.2 calculate software crc */
                softcrc = JD9366TP_CRC_INFO_CRC_INITIAL_VALUE;
                crcbuffer = kzalloc(PageProgram_len * sizeof(uint8_t), GFP_KERNEL);
                if (crcbuffer == NULL) {
                    JD_E("%s: Memory allocate fail\n", __func__);
                    kfree(MoveInfo);
                    return JD_MEM_ALLOC_FAIL;
                }
                memcpy(crcbuffer, pFileData + PageProgram_addr, PageProgram_len);
                jd9366tp_GetDataCRC16(crcbuffer, PageProgram_len, 0, PageProgram_len, &softcrc, JD9366TP_CRC_INFO_PolynomialCRC16);
                kfree(crcbuffer);

                /* 12.3 check IC and software crc */
                if (softcrc != crc) {
                    JD_E("%s: softEcc is %04x, but read IC crc is %04x\n", __func__, softcrc, crc);
                    kfree(MoveInfo);
                    ReCode = JD_CHECK_DATA_ERROR;
                    goto JD_UPGRADE_FW_RETRY;
                } else {
                    JD_I("%s: CRC check pass\n", __func__);
                }
            }
        }
        kfree(MoveInfo);

        /* 11. write data to flash-2 */
        JD_I("%s: Write flash2 start\n", __func__);
        jd9366tp_ReArrangePageProgramInfo(JD9366TP_HW_HEADER_MAX_SIZE + JD9366TP_PRAM_MAX_SIZE,
                                        FileSize - (JD9366TP_HW_HEADER_MAX_SIZE + JD9366TP_PRAM_MAX_SIZE),
                                        &PageProgram_addr, &PageProgram_len);
        ReCode = jd9366tp_PageProgramFlash(PageProgram_addr, PageProgram_len, pFileData, FileSize);
        if (ReCode < 0) {
            JD_E("%s: Write flash2 fail\n", __func__);
            return ReCode;
        } else {
            JD_I("%s: Write flash2 finish\n", __func__);
        }

        if (FileSize >= (PageProgram_addr + PageProgram_len)) {
            /* 12.1 get IC crc-2 */
            ReCode = jd9366tp_GetCRCResult(&crc);
            if (ReCode < 0) {
                JD_E("%s: Read flash2 ecc fail\n", __func__);
                return ReCode;
            }

            /* 12.2 calculate software crc */
            softcrc = JD9366TP_CRC_INFO_CRC_INITIAL_VALUE;
            crcbuffer = kzalloc(PageProgram_len * sizeof(uint8_t), GFP_KERNEL);
            if (crcbuffer == NULL) {
                JD_E("%s: Memory allocate fail\n", __func__);
                return JD_MEM_ALLOC_FAIL;
            }
            memcpy(crcbuffer, pFileData + PageProgram_addr, PageProgram_len);
            jd9366tp_GetDataCRC16(crcbuffer, PageProgram_len, 0, PageProgram_len, &softcrc, JD9366TP_CRC_INFO_PolynomialCRC16);
            kfree(crcbuffer);

            /* 12.3 check IC and software crc-2 */
            if (softcrc != crc) {
                JD_E("%s: softEcc-2 is %04x, but read IC crc-2 is %04x\n", __func__, softcrc, crc);
                ReCode = JD_CHECK_DATA_ERROR;
                goto JD_UPGRADE_FW_RETRY;
            } else {
                JD_I("%s: CRC-2 check pass\n", __func__);
            }
        }

        /* 13. Read back Flash content [option] */
        ReCode = jd9366tp_CheckFlashContent(pFileData, 0, 256);


    JD_UPGRADE_FW_RETRY:
        retry++;

        /* 14. Reset IC touch soc */
        ReCode = jd9366tp_ResetSOC(AddrOffset);
        if (ReCode < 0) {
            JD_E("%s: Reset touch soc fail\n", __func__);
            return ReCode;
        } else {
            JD_I("%s: Reset touch soc\n", __func__);
        }
    } while ((retry < 3) && (ReCode < 0));


    /* Wait flash reload to Pram */
    msleep(100);

    /* 15. Enter back door & read IC id */
    ReCode = jd9366tp_EnterBackDoor(&romid);
    if (ReCode < 0)
        return ReCode;

    /* 16. Set Flash SPI speed */
    ReCode = jd9366tp_SetFlashSPISpeed(pFileData[JD9366TP_LOAD_CODE_SPI_FREQ]);
    if (ReCode < 0) {
        return ReCode;
    }

    /* Reload section info. */
    ReCode = jd9366tp_ReadSectionInfo(true);

    return ReCode;
}

#ifdef JD_ZERO_FLASH
#ifdef JD_SET_VDDD
static void jd9366tp_SetVDDD(uint8_t parameter)
{
    uint8_t dd_reg_cmd;
    uint8_t dd_reg_page;
    uint8_t wdata[4];

    if (g_common_variable.dbi_dd_reg_mode == JD_DDREG_MODE_1) {
        /* Write password */
        dd_reg_page = 0x00;
        dd_reg_cmd = 0xDF;
        wdata[0] = JD9366TP_DD_PASSWORD_CODE1;
        wdata[1] = JD9366TP_DD_PASSWORD_CODE2;
        wdata[2] = JD9366TP_DD_PASSWORD_CODE3;
        if (g_module_fp.fp_dd_register_write(dd_reg_page, dd_reg_cmd, wdata, 3) < 0) {
            JD_E("%s: Write display register(%x) fail!\n", __func__, dd_reg_cmd);
        }

        /* Set page 0 */
        dd_reg_page = 0x00;
        dd_reg_cmd = 0xDE;
        wdata[0] = 0x00;
        if (g_module_fp.fp_dd_register_write(dd_reg_page, dd_reg_cmd, wdata, 1) < 0) {
            JD_E("%s: Write display register(%x) fail!\n", __func__, dd_reg_cmd);
        }

        /* Set VDDD */
        dd_reg_page = 0x00;
        dd_reg_cmd = 0xBC;
        wdata[0] = 0x2F;
        wdata[1] = (uint8_t)((parameter << 4));
        wdata[2] = (uint8_t)((parameter << 4) + (parameter & 0x0F));
        wdata[3] = 0x00;
        if (g_module_fp.fp_dd_register_write(dd_reg_page, dd_reg_cmd, wdata, 4) < 0) {
            JD_E("%s: Write display register(%x) fail!\n", __func__, dd_reg_cmd);
        }

        /* Clear password */
        dd_reg_page = 0x00;
        dd_reg_cmd = 0xDF;
        wdata[0] = 0x00;
        wdata[1] = 0x00;
        wdata[2] = 0x00;
        if (g_module_fp.fp_dd_register_write(dd_reg_page, dd_reg_cmd, wdata, 3) < 0) {
            JD_E("%s: Write display register(%x) fail!\n", __func__, dd_reg_cmd);
        }
    } else {
        JD_E("%s:Not support dbic dd register!\n", __func__);
    }
}
#endif

static int jd9366tp_EnableTCBwritePRAM(uint32_t offset)
{
    int ReCode;

    ReCode = jd9366tp_WriteRegSingle((uint32_t)(JD9366TP_CASCADE_BUS_CONTROL_REG_ADDR_TCB_PRAM_WRITE_CTRL + offset),
                                    (uint8_t)JD9366TP_TCB_PRAM_WRITE_CTRL_WRITABLE);

    if (ReCode < 0) {
        JD_E("%s: Enable TCB write PRAM fail\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_WritePram(uint32_t addr, uint8_t *pData, uint32_t len)
{
    int ReCode;
    uint16_t packsize = (uint16_t)JD9366TP_FLASH_PACK_WRITE_SIZE;
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
            ReCode = jd9366tp_WriteRegMulti(ramaddr, pData + pos, packlen);
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
    ReCode = jd9366tp_SetCRCInitialValue();
    if (ReCode < 0) {
        JD_E("%s: Set CRC initial value fail\n", __func__);
        return ReCode;
    }

    /* Set CRC Initial */
    ReCode = jd9366tp_SetCRCInitial();
    if (ReCode < 0) {
        JD_E("%s: Set CRC initial fail\n", __func__);
        return ReCode;
    }

    /* Set DMA Start & Pulling DMA busy = JD9366TP_DMA_RELATED_SETTING_DMA_DONE */
    ReCode = jd9366tp_SetDMAStart(addr, len, addr, (uint8_t)JD9366TP_DMA_RELATED_SETTING_WRITE_TO_PRAM);
    if (ReCode < 0) {
        JD_E("%s: DMA start error\n", __func__);
        return ReCode;
    } else {
        JD_I("%s: DMA start finish\n", __func__);
    }

    return ReCode;
}

static int jd9366tp_HostWritePram(uint32_t WriteAddr, uint8_t *pFileData, uint32_t FileSize)
{
    int ReCode, i;
    uint32_t AddrOffset;
    uint16_t romid = 0;
    uint16_t crc, softcrc;
    uint8_t MoveInfoNumber;
    uint8_t MoveInfoPsramIndex = 0;
    struct JD9366TP_MOVE_INFO *MoveInfo = NULL;
    bool pram_fail = false;

#ifdef JD_SET_VDDD
    /* 0. Set VDDD to 1.42v */
    jd9366tp_SetVDDD(5);
#endif

    /* 1. Enter backdoor & 2. Read IC ID */
    ReCode = jd9366tp_EnterBackDoor(&romid);
    if (ReCode < 0)
        return ReCode;

    /* 2.1 read IC is cascade or not */
    ReCode = jd9366tp_ReadChipCascadeMode(&(pjadard_ic_data->JD_MODULE_CASCADE_MODE), JD9366TP_MASTER_ADDR_OFFSET);
    if (ReCode < 0) {
        JD_E("%s: Read cascade mode fail\n", __func__);
        return ReCode;
    } else {
        JD_I("%s: Cascade mode %d\n", __func__, pjadard_ic_data->JD_MODULE_CASCADE_MODE);
    }

    /* 3.1 IC RTC run off */
    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;
    ReCode = jd9366tp_DisableRTCRun(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    /* 3.2 IC touch scan off */
    ReCode = jd9366tp_DisableTouchScan(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    /* 3.3 Set DMA 1-byte mode */
    ReCode = jd9366tp_SetDMAByteMode(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    /* 3.4 Clear section first value */
    ReCode = jd9366tp_ClearSectionFirstValue(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    /* 4.1 Stop WDT */
    ReCode = jd9366tp_DisableWDTRun(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    }

    /* 4.2 Stop MCU clock */
    ReCode = jd9366tp_StopMCUClock(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    }

    /* 4.3 Stop MCU */
    ReCode = jd9366tp_StopMCU(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    }

    /* 5.1 Set TCB speed to 16M */
    ReCode = jd9366tp_SetTCBSpeed(AddrOffset, 0);
    if (ReCode < 0) {
         JD_E("%s: TCB spi 16M fail\n", __func__);
        return ReCode;
    }

    /* 5.2 read slave IC id */
    // if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
        // ReCode = jd9366tp_GetID(&romid, JD9366TP_SLAVE_ADDR_OFFSET);

        // if (ReCode < 0) { /* Retry */
            // msleep(10);
            // ReCode = jd9366tp_GetID(&romid, JD9366TP_SLAVE_ADDR_OFFSET);

            // if (ReCode < 0) {
                // JD_E("%s: Check slave ICID fail\n", __func__);
                // return ReCode;
            // }
        // }
    // }

    /* 5.1 enable TCB write PRAM */
    ReCode = jd9366tp_EnableTCBwritePRAM(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    }

    /* 6. Get header info.*/
    MoveInfoNumber = jd9366tp_GetMoveInfoNumber(pFileData);
    MoveInfo = kzalloc(MoveInfoNumber * sizeof(struct JD9366TP_MOVE_INFO), GFP_KERNEL);

    if (MoveInfo == NULL) {
        JD_E("%s: Memory allocate fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

    jd9366tp_GetHeaderInfo(pFileData, MoveInfo, MoveInfoNumber);
    MoveInfoPsramIndex = jd9366tp_GetHeaderPsramIndex(MoveInfo, MoveInfoNumber);

    /* START: Check need upgrade fw */
    if (pjadard_ts_data->power_on_upgrade == false) {
        /* Check slave pram */
        if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
            /* Set CRC initial value */
            ReCode = jd9366tp_SetCRCInitialValue();
            if (ReCode < 0) {
                JD_E("%s: [Check slave ram]Set CRC initial value fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Set CRC Initial */
            ReCode = jd9366tp_SetCRCInitial();
            if (ReCode < 0) {
                JD_E("%s: [Check slave ram]Set CRC initial fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Set DMA Start & Pulling DMA busy = JD9365T_DMA_RELATED_SETTING_DMA_DONE */
            ReCode = jd9366tp_SetDMAStart((MoveInfo[MoveInfoPsramIndex].to_mem_st_addr & 0xF0FFFFFF) | 0x01000000,
                                           MoveInfo[MoveInfoPsramIndex].fl_len,
                                          (MoveInfo[MoveInfoPsramIndex].to_mem_st_addr & 0xF0FFFFFF) | 0x01000000,
                                          (uint8_t)JD9366TP_DMA_RELATED_SETTING_WRITE_TO_PRAM);
            if (ReCode < 0) {
                JD_E("%s: [Check slave ram]DMA start error\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            } else {
                JD_D("%s: [Check slave ram]DMA start finish\n", __func__);
            }

            /* Get IC crc */
            ReCode = jd9366tp_GetCRCResult(&crc);
            if (ReCode < 0) {
                JD_E("%s: [Check slave ram]Read crc fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Check IC crc */
            if (crc != MoveInfo[MoveInfoPsramIndex].to_mem_crc) {
                JD_I("%s: [Check slave ram]Binary crc is %04x, but read IC crc is %04x\n", __func__, MoveInfo[MoveInfoPsramIndex].to_mem_crc, crc);
                pram_fail = true;
            } else {
                JD_I("%s: [Check slave ram]HW CRC check pass\n", __func__);
            }
        }

        /* Check master pram */
        if (pram_fail == false) {
            /* Set CRC initial value */
            ReCode = jd9366tp_SetCRCInitialValue();
            if (ReCode < 0) {
                JD_E("%s: [Check master ram]Set CRC initial value fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Set CRC Initial */
            ReCode = jd9366tp_SetCRCInitial();
            if (ReCode < 0) {
                JD_E("%s: [Check master ram]Set CRC initial fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Set DMA Start & Pulling DMA busy = JD9365T_DMA_RELATED_SETTING_DMA_DONE */
            ReCode = jd9366tp_SetDMAStart(MoveInfo[MoveInfoPsramIndex].to_mem_st_addr & 0xF0FFFFFF,
                                          MoveInfo[MoveInfoPsramIndex].fl_len,
                                          MoveInfo[MoveInfoPsramIndex].to_mem_st_addr & 0xF0FFFFFF,
                                          (uint8_t)JD9366TP_DMA_RELATED_SETTING_WRITE_TO_PRAM);
            if (ReCode < 0) {
                JD_E("%s: [Check master ram]DMA start error\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            } else {
                JD_D("%s: [Check master ram]DMA start finish\n", __func__);
            }

            /* Get IC crc */
            ReCode = jd9366tp_GetCRCResult(&crc);
            if (ReCode < 0) {
                JD_E("%s: [Check master ram]Read crc fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Check IC crc */
            if (crc != MoveInfo[MoveInfoPsramIndex].to_mem_crc) {
                JD_I("%s: [Check master ram]Binary crc is %04x, but read IC crc is %04x\n", __func__, MoveInfo[MoveInfoPsramIndex].to_mem_crc, crc);
            } else {
                JD_I("%s: [Check master ram]HW CRC check pass, bypass upgrade flow\n", __func__);
                goto SKIP_UPGRADE_FW;
            }
        }
    }
    /* END: Check need upgrade fw */

    /* 7. Write data to PRAM/DRAM */
    JD_I("%s: Write ram start\n", __func__);

    for (i = 0; i < MoveInfoNumber; i++) {
        pjadard_ts_data->power_on_upgrade = false;

        if ((MoveInfo[i].fl_st_addr + MoveInfo[i].fl_len) > FileSize) {
            JD_E("%s: Write ram overflow\n", __func__);
            kfree(MoveInfo);
            return ReCode;
        }

        ReCode = jd9366tp_WritePram(MoveInfo[i].to_mem_st_addr, pFileData + MoveInfo[i].fl_st_addr, MoveInfo[i].fl_len);
        if (ReCode < 0) {
            JD_E("%s: Write pram fail\n", __func__);
            kfree(MoveInfo);
            return ReCode;
        }

        /* 8.1 Get IC crc */
        ReCode = jd9366tp_GetCRCResult(&crc);
        if (ReCode < 0) {
            JD_E("%s: Read crc fail\n", __func__);
            kfree(MoveInfo);
            return ReCode;
        }

        /* 8.2 Check IC crc */
        if (crc != MoveInfo[i].to_mem_crc) {
            JD_E("%s: Binary crc is %04x, but read IC crc is %04x\n", __func__, MoveInfo[i].to_mem_crc, crc);
            kfree(MoveInfo);
            return ReCode;
        } else {
            JD_I("%s: HW CRC check pass\n", __func__);
        }

        /* 8.3 Calculate software crc */
        softcrc = JD9366TP_CRC_INFO_CRC_INITIAL_VALUE;
        jd9366tp_GetDataCRC16(pFileData + MoveInfo[i].fl_st_addr, MoveInfo[i].fl_len, 0, MoveInfo[i].fl_len, &softcrc, JD9366TP_CRC_INFO_PolynomialCRC16);

        /* 8.4 check software crc */
        if (softcrc != MoveInfo[i].to_mem_crc) {
            JD_E("%s: Binary crc is %04x, but soft crc is %04x\n", __func__, MoveInfo[i].to_mem_crc, softcrc);
            kfree(MoveInfo);
            return ReCode;
        } else {
            JD_I("%s: Software CRC check pass\n", __func__);
        }
    }

SKIP_UPGRADE_FW:
    kfree(MoveInfo);
    JD_I("%s: Write ram finish\n", __func__);

    /* 9. Software reset MCU */
    ReCode = jd9366tp_ResetMCU(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    } else {
        JD_I("%s: Reset touch mcu\n", __func__);
    }

    /* 10. start MCU */
    ReCode = jd9366tp_StartMCU(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    } else {
        JD_I("%s: Start MCU\n", __func__);
    }

    /* 11. Start MCU clock */
    ReCode = jd9366tp_StartMCUClock(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    } else {
        JD_I("%s: Start MCU clock\n", __func__);
    }

    /* 12. Reload section info. */
    ReCode = jd9366tp_ReadSectionInfo(false);

    return ReCode;
}

#ifdef JD_ESD_CHECK
static int jd9366tp_esd_HostWritePram(uint32_t WriteAddr, uint8_t *pFileData, uint32_t FileSize)
{
    int ReCode, i;
    uint32_t AddrOffset;
    uint16_t romid = 0;
    uint16_t crc, softcrc;
    uint8_t MoveInfoNumber;
    uint8_t MoveInfoPsramIndex = 0;
    struct JD9366TP_MOVE_INFO *MoveInfo = NULL;
    bool pram_fail = false;

    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;
    jd9366tp_SetDMAByteMode(AddrOffset);

    /* 6. Get header info.*/
    MoveInfoNumber = jd9366tp_GetMoveInfoNumber(pFileData);
    MoveInfo = kzalloc(MoveInfoNumber * sizeof(struct JD9366TP_MOVE_INFO), GFP_KERNEL);

    if (MoveInfo == NULL) {
        JD_E("%s: Memory allocate fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

    jd9366tp_GetHeaderInfo(pFileData, MoveInfo, MoveInfoNumber);
    MoveInfoPsramIndex = jd9366tp_GetHeaderPsramIndex(MoveInfo, MoveInfoNumber);

    /* START: Check need upgrade fw */
    if (pjadard_ts_data->power_on_upgrade == false) {
JD_SLAVE_PRAM_CRC_RECHECK:
        /* Check slave pram */
        if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
            /* Set CRC initial value */
            ReCode = jd9366tp_SetCRCInitialValue();
            if (ReCode < 0) {
                JD_E("%s: [Check slave ram]Set CRC initial value fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Set CRC Initial */
            ReCode = jd9366tp_SetCRCInitial();
            if (ReCode < 0) {
                JD_E("%s: [Check slave ram]Set CRC initial fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Set DMA Start & Pulling DMA busy = JD9365T_DMA_RELATED_SETTING_DMA_DONE */
            ReCode = jd9366tp_SetDMAStart((MoveInfo[MoveInfoPsramIndex].to_mem_st_addr & 0xF0FFFFFF) | 0x01000000,
                                           MoveInfo[MoveInfoPsramIndex].fl_len,
                                          (MoveInfo[MoveInfoPsramIndex].to_mem_st_addr & 0xF0FFFFFF) | 0x01000000,
                                          (uint8_t)JD9366TP_DMA_RELATED_SETTING_WRITE_TO_PRAM);
            if (ReCode < 0) {
                JD_E("%s: [Check slave ram]DMA start error\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            } else {
                JD_D("%s: [Check slave ram]DMA start finish\n", __func__);
            }

            /* Get IC crc */
            ReCode = jd9366tp_GetCRCResult(&crc);
            if (ReCode < 0) {
                JD_E("%s: [Check slave ram]Read crc fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Check IC crc */
            if (crc != MoveInfo[MoveInfoPsramIndex].to_mem_crc) {
                JD_I("%s: [Check slave ram]Binary crc is %04x, but read IC crc is %04x\n", __func__, MoveInfo[MoveInfoPsramIndex].to_mem_crc, crc);

                if (pjadard_report_data->crc_start == 1) {
                    if (pjadard_report_data->crc_retry >= 2) {
                        pjadard_report_data->crc_retry = 0;
                        JD_I("%s: Upgrade slave\n", __func__);
                    } else {
                        pjadard_report_data->crc_retry++;
                        JD_I("%s: Recheck slave CRC\n", __func__);
                        mdelay(500);
                        goto JD_SLAVE_PRAM_CRC_RECHECK;
                    }
                }

                jadard_int_enable(false);
                pram_fail = true;
            } else {
                JD_I("%s: [Check slave ram]HW CRC check pass\n", __func__);
            }
        }

JD_MASTER_PRAM_CRC_RECHECK:
        /* Check master pram */
        if (pram_fail == false) {
            /* Set CRC initial value */
            ReCode = jd9366tp_SetCRCInitialValue();
            if (ReCode < 0) {
                JD_E("%s: [Check master ram]Set CRC initial value fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Set CRC Initial */
            ReCode = jd9366tp_SetCRCInitial();
            if (ReCode < 0) {
                JD_E("%s: [Check master ram]Set CRC initial fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Set DMA Start & Pulling DMA busy = JD9365T_DMA_RELATED_SETTING_DMA_DONE */
            ReCode = jd9366tp_SetDMAStart(MoveInfo[MoveInfoPsramIndex].to_mem_st_addr & 0xF0FFFFFF,
                                          MoveInfo[MoveInfoPsramIndex].fl_len,
                                          MoveInfo[MoveInfoPsramIndex].to_mem_st_addr & 0xF0FFFFFF,
                                          (uint8_t)JD9366TP_DMA_RELATED_SETTING_WRITE_TO_PRAM);
            if (ReCode < 0) {
                JD_E("%s: [Check master ram]DMA start error\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            } else {
                JD_D("%s: [Check master ram]DMA start finish\n", __func__);
            }

            /* Get IC crc */
            ReCode = jd9366tp_GetCRCResult(&crc);
            if (ReCode < 0) {
                JD_E("%s: [Check master ram]Read crc fail\n", __func__);
                kfree(MoveInfo);
                return ReCode;
            }

            /* Check IC crc */
            if (crc != MoveInfo[MoveInfoPsramIndex].to_mem_crc) {
                JD_I("%s: [Check master ram]Binary crc is %04x, but read IC crc is %04x\n", __func__, MoveInfo[MoveInfoPsramIndex].to_mem_crc, crc);
            
                if (pjadard_report_data->crc_start == 1) {
                    if (pjadard_report_data->crc_retry >= 2) {
                        pjadard_report_data->crc_retry = 0;
                        JD_I("%s: Upgrade master\n", __func__);
                    } else {
                        pjadard_report_data->crc_retry++;
                        JD_I("%s: Recheck master CRC\n", __func__);
                        mdelay(500);
                        goto JD_MASTER_PRAM_CRC_RECHECK;
                    }
                }

                jadard_int_enable(false);
            } else {
                JD_I("%s: [Check master ram]HW CRC check pass, bypass upgrade flow\n", __func__);
                
                if (pjadard_report_data->crc_start == 1) {
                    JD_I("%s: Clear crc_retry\n", __func__);
                    pjadard_report_data->crc_retry = 0;
                }

                kfree(MoveInfo);
                return JD_PRAM_CRC_PASS;
            }
        }
    }
    /* END: Check need upgrade fw */
    
    pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE = true;
    /* 1. Enter backdoor & 2. Read IC ID */
    ReCode = jd9366tp_EnterBackDoor(&romid);
    if (ReCode < 0) {
        kfree(MoveInfo);
        return ReCode;
    }

    /* 2.1 read IC is cascade or not */
    ReCode = jd9366tp_ReadChipCascadeMode(&(pjadard_ic_data->JD_MODULE_CASCADE_MODE), JD9366TP_MASTER_ADDR_OFFSET);
    if (ReCode < 0) {
        JD_E("%s: Read cascade mode fail\n", __func__);
        kfree(MoveInfo);
        return ReCode;
    } else {
        JD_I("%s: Cascade mode %d\n", __func__, pjadard_ic_data->JD_MODULE_CASCADE_MODE);
    }

    /* 3.1 IC RTC run off */
    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;
    ReCode = jd9366tp_DisableRTCRun(AddrOffset);
    if (ReCode < 0) {
       kfree(MoveInfo);
       return ReCode;
    }

    /* 3.2 IC touch scan off */
    ReCode = jd9366tp_DisableTouchScan(AddrOffset);
    if (ReCode < 0) {
        kfree(MoveInfo);
        return ReCode;
    }

    /* 3.3 Set DMA 1-byte mode */
    ReCode = jd9366tp_SetDMAByteMode(AddrOffset);
    if (ReCode < 0) {
        kfree(MoveInfo);
        return ReCode;
    }

    /* 3.4 Clear section first value */
    ReCode = jd9366tp_ClearSectionFirstValue(AddrOffset);
    if (ReCode < 0) {
        kfree(MoveInfo);
        return ReCode;
    }

    /* 4.1 Stop WDT */
    ReCode = jd9366tp_DisableWDTRun(AddrOffset);
    if (ReCode < 0) {
        kfree(MoveInfo);
        return ReCode;
    }

    /* 4.2 Stop MCU clock */
    ReCode = jd9366tp_StopMCUClock(AddrOffset);
    if (ReCode < 0) {
        kfree(MoveInfo);
        return ReCode;
    }

    /* 4.3 Stop MCU */
    ReCode = jd9366tp_StopMCU(AddrOffset);
    if (ReCode < 0) {
        kfree(MoveInfo);
        return ReCode;
    }

    /* 5.1 Set TCB speed to 16M */
    ReCode = jd9366tp_SetTCBSpeed(AddrOffset, 0);
    if (ReCode < 0) {
        JD_E("%s: TCB spi 16M fail\n", __func__);
        kfree(MoveInfo);
        return ReCode;
    }

    /* 5.2 read slave IC id */
    // if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
        // ReCode = jd9366tp_GetID(&romid, JD9366TP_SLAVE_ADDR_OFFSET);

        // if (ReCode < 0) { /* Retry */
            // msleep(10);
            // ReCode = jd9366tp_GetID(&romid, JD9366TP_SLAVE_ADDR_OFFSET);

            // if (ReCode < 0) {
                // JD_E("%s: Check slave ICID fail\n", __func__);
                // kfree(MoveInfo);
                // return ReCode;
            // }
        // }
    // }

    /* 5.1 enable TCB write PRAM */
    ReCode = jd9366tp_EnableTCBwritePRAM(AddrOffset);
    if (ReCode < 0) {
        kfree(MoveInfo);
        return ReCode;
    }

    /* 7. Write data to PRAM/DRAM */
    JD_I("%s: Write ram start\n", __func__);

    for (i = 0; i < MoveInfoNumber; i++) {
        pjadard_ts_data->power_on_upgrade = false;

        if ((MoveInfo[i].fl_st_addr + MoveInfo[i].fl_len) > FileSize) {
            JD_E("%s: Write ram overflow\n", __func__);
            kfree(MoveInfo);
            return ReCode;
        }

        ReCode = jd9366tp_WritePram(MoveInfo[i].to_mem_st_addr, pFileData + MoveInfo[i].fl_st_addr, MoveInfo[i].fl_len);
        if (ReCode < 0) {
            JD_E("%s: Write pram fail\n", __func__);
            kfree(MoveInfo);
            return ReCode;
        }

        /* 8.1 Get IC crc */
        ReCode = jd9366tp_GetCRCResult(&crc);
        if (ReCode < 0) {
            JD_E("%s: Read crc fail\n", __func__);
            kfree(MoveInfo);
            return ReCode;
        }

        /* 8.2 Check IC crc */
        if (crc != MoveInfo[i].to_mem_crc) {
            JD_E("%s: Binary crc is %04x, but read IC crc is %04x\n", __func__, MoveInfo[i].to_mem_crc, crc);
            kfree(MoveInfo);
            return ReCode;
        } else {
            JD_I("%s: HW CRC check pass\n", __func__);
        }

        /* 8.3 Calculate software crc */
        softcrc = JD9366TP_CRC_INFO_CRC_INITIAL_VALUE;
        jd9366tp_GetDataCRC16(pFileData + MoveInfo[i].fl_st_addr, MoveInfo[i].fl_len, 0, MoveInfo[i].fl_len, &softcrc, JD9366TP_CRC_INFO_PolynomialCRC16);

        /* 8.4 check software crc */
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
    ReCode = jd9366tp_ResetMCU(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    } else {
        JD_I("%s: Reset touch mcu\n", __func__);
    }

    /* 10. start MCU */
    ReCode = jd9366tp_StartMCU(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    } else {
        JD_I("%s: Start MCU\n", __func__);
    }

    /* 11. Start MCU clock */
    ReCode = jd9366tp_StartMCUClock(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    } else {
        JD_I("%s: Start MCU clock\n", __func__);
    }

    /* 12. Reload section info. */
    ReCode = jd9366tp_ReadSectionInfo(false);

    return ReCode;
}
#endif
#endif

static int jd9366tp_ReadPram(uint32_t addr, uint8_t *pData, uint32_t len)
{
    int ReCode;
    uint16_t packsize = (uint16_t)JD9366TP_FLASH_PACK_READ_SIZE;
    uint32_t pos = 0;
    uint16_t packlen = 0;
    /* MCU ram config */
    uint32_t ramaddr = addr;
    uint32_t ramlen = len;
    /* Read buf */
    uint8_t *rBuf = NULL;

    rBuf = kzalloc(packsize * sizeof(uint8_t), GFP_KERNEL);
    if (rBuf == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

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
            ReCode = jd9366tp_ReadRegMulti(ramaddr, rBuf, packlen);
            if (ReCode < 0) {
                JD_E("%s: Read ram data fail\n", __func__);
                kfree(rBuf);
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
    ReCode = jd9366tp_SetCRCInitialValue();
    if (ReCode < 0) {
        JD_E("%s: Set CRC initial value fail\n", __func__);
        kfree(rBuf);
        return ReCode;
    }

    /* Set CRC Initial */
    ReCode = jd9366tp_SetCRCInitial();
    if (ReCode < 0) {
        JD_E("%s: Set CRC initial fail\n", __func__);
        kfree(rBuf);
        return ReCode;
    }

    /* Set DMA Start & Pulling DMA busy = JD9366TP_DMA_RELATED_SETTING_DMA_DONE */
    ReCode = jd9366tp_SetDMAStart(addr, len, addr, (uint8_t)JD9366TP_DMA_RELATED_SETTING_WRITE_TO_PRAM);
    if (ReCode < 0) {
        JD_E("%s: DMA start error\n", __func__);
        kfree(rBuf);
        return ReCode;
    } else {
        JD_I("%s: DMA start finish\n", __func__);
    }

    kfree(rBuf);
    return ReCode;
}

static int jd9366tp_HostReadPram(uint32_t ReadAddr, uint8_t *pFileData, uint32_t ReadLen)
{
    int ReCode;
    uint8_t tcb_speed;
    uint16_t romid = 0;
    uint16_t crc, softcrc;
    uint32_t AddrOffset;

    /* 1. Enter backdoor & 2. Read IC ID */
    ReCode = jd9366tp_EnterBackDoor(&romid);
    if (ReCode < 0)
        return ReCode;

    /* 2.1 read IC is cascade or not */
    ReCode = jd9366tp_ReadChipCascadeMode(&(pjadard_ic_data->JD_MODULE_CASCADE_MODE), JD9366TP_MASTER_ADDR_OFFSET);
    if (ReCode < 0) {
        JD_E("%s: Read cascade mode fail\n", __func__);
        return ReCode;
    } else {
        JD_I("%s: Cascade mode %d\n", __func__, pjadard_ic_data->JD_MODULE_CASCADE_MODE);
    }

    /* 3.1 Set DMA 1-byte mode */
    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;

    ReCode = jd9366tp_SetDMAByteMode(AddrOffset);
    if (ReCode < 0)
        return ReCode;

    /* 4.0 Stop WDT */
    ReCode = jd9366tp_DisableWDTRun(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    }

    /* 4.1 Stop MCU clock */
    ReCode = jd9366tp_StopMCUClock(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    }

    /* 4.2 Stop MCU */
    ReCode = jd9366tp_StopMCU(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    }

    /* 4.3 Set TCB speed to 16M */
    jd9366tp_GetTCBSpeed(JD9366TP_MASTER_ADDR_OFFSET, &tcb_speed);
    ReCode = jd9366tp_SetTCBSpeed(AddrOffset, 0);
    if (ReCode < 0) {
         JD_E("%s: TCB spi 16M fail\n", __func__);
        return ReCode;
    }

    /* 4.4 read slave IC id */
    if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
        ReCode = jd9366tp_GetID(&romid, JD9366TP_SLAVE_ADDR_OFFSET);

        if (ReCode < 0) { /* Retry */
            msleep(10);
            ReCode = jd9366tp_GetID(&romid, JD9366TP_SLAVE_ADDR_OFFSET);

            if (ReCode < 0) {
                JD_E("%s: Check slave ICID fail\n", __func__);
                return ReCode;
            }
        }
    }

    /* 5. Read data from PRAM/DRAM */
    JD_I("%s: Read ram start\n", __func__);
    ReCode = jd9366tp_ReadPram(ReadAddr, pFileData, ReadLen);
    if (ReCode < 0) {
        JD_E("%s: Read ram error\n", __func__);
        return ReCode;
    } else {
        JD_I("%s: Read ram finish\n", __func__);
    }

    /* 6.1 Get IC crc */
    ReCode = jd9366tp_GetCRCResult(&crc);
    if (ReCode < 0) {
        JD_E("%s: Read flash ecc fail\n", __func__);
        return ReCode;
    }

    /* 6.2 Calculate software crc */
    softcrc = JD9366TP_CRC_INFO_CRC_INITIAL_VALUE;
    jd9366tp_GetDataCRC16(pFileData, ReadLen, ReadAddr, ReadLen, &softcrc, JD9366TP_CRC_INFO_PolynomialCRC16);

    /* 6.3 Compare IC and software crc */
    if (softcrc != crc) {
        JD_E("%s: softEcc is %04x, but read IC crc is %04x\n", __func__, softcrc, crc);
    }

    /* 7.1 Reload TCB speed */
    ReCode = jd9366tp_SetTCBSpeed(AddrOffset, tcb_speed);
    if (ReCode < 0) {
         JD_E("%s: TCB spi reload fail\n", __func__);
        return ReCode;
    }

    /* 7.2 start MCU */
    ReCode = jd9366tp_StartMCU(AddrOffset);
    if (ReCode < 0) {
        return ReCode;
    } else {
        JD_I("%s: Start MCU\n", __func__);
    }

    /* 8. Start MCU clock */
    ReCode = jd9366tp_StartMCUClock(AddrOffset);
    if (ReCode >= 0) {
        JD_I("%s: Start MCU clock\n", __func__);
    }

    msleep(10);

    return ReCode;
}

static int jd9366tp_ReadTpInfoBuf(uint8_t *rdata, uint16_t rlen)
{
    return jd9366tp_ReadRegMulti(g_jd9366tp_chip_info.esram_info_content_addr.coordinate_report, rdata, rlen);
}

static void module_jd9366tp_chip_init(void)
{
    int ReCode;

    JD_I("%s\n", __func__);
    /* Jadard: Set FW and CFG Flash Address */
    g_common_variable.FW_SIZE              = JD9366TP_SIZE;
    g_common_variable.RAM_LEN              = JD9366TP_PRAM_MAX_SIZE;
    pjadard_ic_data->JD_MASTER_ADDR_OFFSET = JD9366TP_MASTER_ADDR_OFFSET;
    pjadard_ic_data->JD_SLAVE_ADDR_OFFSET  = JD9366TP_SLAVE_ADDR_OFFSET;
    pjadard_ic_data->JD_BOTH_ADDR_OFFSET   = JD9366TP_BOTH_ADDR_OFFSET;
    jd9366tp_ChipInfoInit();

    /* Read cascade mode status */
    ReCode = jd9366tp_ReadChipCascadeMode(&(pjadard_ic_data->JD_MODULE_CASCADE_MODE), JD9366TP_MASTER_ADDR_OFFSET);
    if (ReCode < 0) {
        JD_E("%s: Read cascade mode fail\n", __func__);
    } else {
        JD_I("%s: Cascade mode %d\n", __func__, pjadard_ic_data->JD_MODULE_CASCADE_MODE);
    }

#ifndef JD_ZERO_FLASH
    /* Reload section info. */
    if (jd9366tp_ReadSectionInfo(false) < 0) {
        JD_E("Chip init fail\n");
    } else {
        g_module_fp.fp_read_fw_ver();
    }
#endif
}

static void module_jd9366tp_EnterBackDoor(void)
{
    jd9366tp_EnterBackDoor(NULL);
}

static void module_jd9366tp_ExitBackDoor(void)
{
    jd9366tp_ExitBackDoor();
}

static void module_jd9366tp_PinReset(void)
{
    jd9366tp_PinReset();
}

static void module_jd9366tp_SoftReset(void)
{
    uint32_t AddrOffset;

    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;

    /* Reset IC touch soc */
    if (jd9366tp_ResetSOC(AddrOffset) < 0) {
        JD_E("%s: Reset touch soc fail\n", __func__);
        return;
    } else {
        JD_I("%s: Reset touch soc\n", __func__);
    }

    /* Wait flash reload to Pram, Add Flash */
    msleep(100);

    /* Enter back door */
    jd9366tp_EnterBackDoor(NULL);
}

static void module_jd9366tp_ResetMCU(void)
{
    uint32_t AddrOffset;

    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;

    jd9366tp_ResetMCU(AddrOffset);
}

static void module_jd9366tp_PorInit(void)
{
    uint32_t AddrOffset;

    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;

    jd9366tp_PorInit(AddrOffset);
}

static int module_jd9366tp_register_read(uint32_t ReadAddr, uint8_t *ReadData, uint32_t ReadLen)
{
#ifdef JD_READ_SLAVE_BY_MASTER
    int i;
#endif
    int ReCode;

    if (ReadLen & 0xFFFF0000) {
        ReCode = JD_READ_LEN_OVERFLOW;
    } else {
#ifdef JD_READ_SLAVE_BY_MASTER
        if (((ReadAddr & 0x51000000) == 0x51000000) || ((ReadAddr & 0x51800000) == 0x51800000)) {
            for (i = 0; i < ReadLen; i++) {
                ReCode = jd9366tp_ReadRegSingle(ReadAddr + i, ReadData + i);
            }
        } else {
            ReCode = jd9366tp_ReadRegMulti(ReadAddr, ReadData, (uint16_t)ReadLen);
        }
#else
        ReCode = jd9366tp_ReadRegMulti(ReadAddr, ReadData, (uint16_t)ReadLen);
#endif

    }

    return ReCode;
}

static int module_jd9366tp_register_write(uint32_t WriteAddr, uint8_t *WriteData, uint32_t WriteLen)
{
    return jd9366tp_WriteRegMulti(WriteAddr, WriteData, (uint16_t)WriteLen);
}

static int module_jd9366tp_ram_read(uint32_t ReadAddr, uint8_t *ReadBuffer, uint32_t ReadLen)
{
    int ReCode;

    pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE = true;
    ReCode = jd9366tp_HostReadPram(ReadAddr, ReadBuffer, ReadLen);
    pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE = false;

    return ReCode;
}

#ifdef JD_ZERO_FLASH
static int module_jd9366tp_ram_write(uint32_t WriteAddr, uint8_t *WriteBuffer, uint32_t WriteLen)
{
    int ReCode;

    pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE = true;
    ReCode = jd9366tp_HostWritePram(WriteAddr, WriteBuffer, WriteLen);
    pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE = false;

    return ReCode;
}

#ifdef JD_ESD_CHECK
static int module_jd9366tp_esd_ram_write(uint32_t WriteAddr, uint8_t *WriteBuffer, uint32_t WriteLen)
{
    int ReCode;

    ReCode = jd9366tp_esd_HostWritePram(WriteAddr, WriteBuffer, WriteLen);
    pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE = false;

    return ReCode;
}
#endif
#endif

static int module_jd9366tp_flash_read(uint32_t ReadAddr, uint8_t *ReadBuffer, uint32_t ReadLen)
{
    int ReCode;

    pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE = true;
    ReCode = jd9366tp_HostReadFlash(ReadAddr, ReadBuffer, ReadLen);
    pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE = false;

    return ReCode;
}

static int module_jd9366tp_flash_write(uint32_t WriteAddr, uint8_t *WriteData, uint32_t WriteLen)
{
    int ReCode;

    pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE = true;
    ReCode = jd9366tp_HostWriteFlash(WriteAddr, WriteData, WriteLen);
    pjadard_ic_data->JD_DISABLE_MASTER_TO_SLAVE = false;

    return ReCode;
}

static int module_jd9366tp_flash_erase(void)
{
    return jd9366tp_EraseChip();
}

static void module_jd9366tp_read_fw_ver(void)
{
    int ReCode_cid, ReCode_ver, ReCode_maker, ReCode_panel, maker_id;
    uint8_t rdata_cid[JD_FOUR_SIZE], rdata_fw[JD_FOUR_SIZE], rdata_maker[JD_ONE_SIZE], rdata_panel[JD_ONE_SIZE];
    char *maker_name;

    ReCode_cid = module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.fw_cid_version, rdata_cid, sizeof(rdata_cid));
    ReCode_ver = module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.fw_version, rdata_fw, sizeof(rdata_fw));
    ReCode_maker = module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.panel_maker, rdata_maker, sizeof(rdata_maker));
    ReCode_panel = module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.panel_version, rdata_panel, sizeof(rdata_panel));

    if ((ReCode_cid < 0) || (ReCode_ver < 0) || (ReCode_maker < 0) || (ReCode_panel < 0)) {
        JD_E("%s: Read fw version fail\n", __func__);
        pjadard_ic_data->fw_ver = 0;
        pjadard_ic_data->fw_cid_ver = 0;
        scnprintf(pjadard_ic_data->panel_maker, sizeof(pjadard_ic_data->panel_maker), "%s", jd_panel_maker_list[JD_PANEL_MAKER_NUM]);
        pjadard_ic_data->panel_ver = 0;
    } else {
        pjadard_ic_data->fw_ver = ((rdata_fw[3] << 24) |
                                   (rdata_fw[2] << 16) |
                                   (rdata_fw[1] << 8) |
                                   (rdata_fw[0]));
        pjadard_ic_data->fw_cid_ver = ((rdata_cid[3] << 24) |
                                       (rdata_cid[2] << 16) |
                                       (rdata_cid[1] << 8) |
                                       (rdata_cid[0]));
        maker_id = (int)rdata_maker[0];
        pjadard_ic_data->panel_ver = rdata_panel[0];

        switch (maker_id) {
        case JD_PANEL_MAKER_CP:
            maker_name = "CP";
            break;
        case JD_PANEL_MAKER_COB:
            maker_name = "COB";
            break;
        default:
            if (maker_id < JD_PANEL_MAKER_NUM) {
                maker_name = jd_panel_maker_list[maker_id];
            } else {
                maker_name = jd_panel_maker_list[JD_PANEL_MAKER_NUM];
            }
            break;
        }
        scnprintf(pjadard_ic_data->panel_maker, sizeof(pjadard_ic_data->panel_maker), "%s", maker_name);
    }

    JD_I("FW_VER: %08x\n", pjadard_ic_data->fw_ver);
    JD_I("FW_CID_VER: %08x\n", pjadard_ic_data->fw_cid_ver);
    JD_I("PANEL_MAKER: %s\n", pjadard_ic_data->panel_maker);
    JD_I("PANEL_VER: %02x\n", pjadard_ic_data->panel_ver);
}

#if defined(JD_AUTO_UPGRADE_FW)
static int module_jd9366tp_read_fw_ver_bin(void)
{
    if (g_jadard_fw != NULL) {
        JD_I("Catch fw version in bin file\n");
        g_jadard_fw_ver = ((g_jadard_fw[JD9366TP_BIN_FW_VER + 0] << 16) |
                           (g_jadard_fw[JD9366TP_BIN_FW_VER + 1] << 8) |
                           (g_jadard_fw[JD9366TP_BIN_FW_VER + 2]));
        g_jadard_fw_cid_ver = ((g_jadard_fw[JD9366TP_BIN_FW_CID_VER + 0] << 24) |
                               (g_jadard_fw[JD9366TP_BIN_FW_CID_VER + 1] << 16) |
                               (g_jadard_fw[JD9366TP_BIN_FW_CID_VER + 2] << 8) |
                               (g_jadard_fw[JD9366TP_BIN_FW_CID_VER + 3]));
    } else {
        JD_I("FW data is null\n");
        return JD_FILE_OPEN_FAIL;
    }

    return JD_NO_ERR;
}
#endif

static void module_jd9366tp_mutual_data_set(uint8_t data_type)
{
    uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};
    uint8_t status[JD_TWO_SIZE];

    if ((data_type >= JD_DATA_TYPE_RawData) && (data_type <= JD_DATA_TYPE_RESERVE_F)) {
        wdata[0] = data_type - 1;
    } else {
        wdata[0] = JD_DATA_TYPE_RawData - 1;
    }

    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_debug_addr.output_data_sel, wdata, sizeof(wdata));

    if (pjadard_ts_data->debug_diag_apk_enable) {
        status[0] = 0x55;
        status[1] = 0xAA;
        jd9366tp_WriteRegMulti(g_jd9366tp_chip_info.dsram_host_addr.data_output_v2, status, sizeof(status));
    }
    else {
        status[0] = 0x00;
        status[1] = 0x00;
        jd9366tp_WriteRegMulti(g_jd9366tp_chip_info.dsram_host_addr.data_output_v2, status, sizeof(status));
        /* Clear handshark status for START */
        status[0] = 0xA5;
        status[1] = 0x5A;
        jd9366tp_WriteRegMulti(g_jd9366tp_chip_info.dsram_debug_addr.output_data_handshake, status, sizeof(status));
    }
}

static int module_jd9366tp_get_mutual_data(uint8_t data_type, uint8_t *rdata, uint16_t rlen)
{
    int ReCode;
    int DataCnt = 0;
    uint8_t status[JD_TWO_SIZE];

    do {
        mdelay(1);

        if (DataCnt++ > JD9366TP_DATA_TIMEOUT) {
            JD_I("Scan data timeout, DataCnt = %d\n", DataCnt);
            jd9366tp_EnterBackDoor(NULL);
            return JD_TIME_OUT;
        }

        jd9366tp_ReadRegMulti(g_jd9366tp_chip_info.dsram_debug_addr.output_data_handshake, status, sizeof(status));
    } while (((status[1] << 8) + status[0]) != JD9366TP_OUTPUT_DATA_HANDSHAKE_DONE);

    /* Get data from IC */
    if ((data_type >= JD_DATA_TYPE_RawData) && (data_type <= JD_DATA_TYPE_RESERVE_F)) {
        ReCode = jd9366tp_ReadRegMulti(g_jd9366tp_chip_info.dsram_debug_addr.output_data_addr, rdata, rlen);
        /* Clear handshark status */
        status[0] = 0xA5;
        status[1] = 0x5A;
        jd9366tp_WriteRegMulti(g_jd9366tp_chip_info.dsram_debug_addr.output_data_handshake, status, sizeof(status));
#ifdef JD_FPGA_WORKAROUND
        /* FPGA need delay after handshake */
        mdelay(50);
#endif
    } else {
        JD_E("%s: Not support data type = %d\n", __func__, data_type);
        ReCode = -EINVAL;
    }

    return ReCode;
}

static bool module_jd9366tp_get_touch_data(uint8_t *rdata, uint8_t rlen)
{
    if (jd9366tp_ReadTpInfoBuf(rdata, rlen) < 0) {
        return false;
    } else {
        return true;
    }
}

static int module_jd9366tp_read_mutual_data(uint8_t *rdata, uint16_t rlen)
{
    int ReCode;

    if (g_jd9366tp_chip_info.dsram_debug_addr.output_data_addr > g_jd9366tp_chip_info.esram_info_content_addr.coordinate_report) {
        ReCode = jd9366tp_ReadRegMulti(g_jd9366tp_chip_info.esram_info_content_addr.coordinate_report, rdata, rlen);
        if (ReCode >= 0)
            ReCode = JD_REPORT_DATA;
    }
    else
        ReCode = jd9366tp_ReadRegMulti(g_jd9366tp_chip_info.dsram_debug_addr.output_data_addr, rdata, rlen);

    return ReCode;
}

static void module_jd9366tp_set_high_sensitivity(bool enable)
{
    uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};
    if (enable) {
        wdata[0] = 0x3A;
        wdata[1] = 0xA3;
    }
    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.high_sensitivity_en, wdata, sizeof(wdata));
}

static void module_jd9366tp_set_rotate_border(uint16_t rotate)
{
    uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};
    wdata[0] = (uint8_t)((rotate & 0x00FF) >> 0);
    wdata[1] = (uint8_t)((rotate & 0xFF00) >> 8);
    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.border_en, wdata, sizeof(wdata));
}

#ifdef JD_EARPHONE_DETECT
static void module_jd9366tp_set_earphone_enable(uint8_t status)
{
    uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};

    switch (status) {
        case 1: /* earphone removed */
            pjadard_ts_data->earphone_enable &=  0xFF00;
            break;
        case 2: /* earphone detected */
            pjadard_ts_data->earphone_enable &=  0xFF00;
            pjadard_ts_data->earphone_enable |=  0x0001;
            break;
        case 3: /* type-c_earphone removed */
            pjadard_ts_data->earphone_enable &=  0x00FF;
            break;
        case 4: /* type-c_earphone detected */
            pjadard_ts_data->earphone_enable &=  0x00FF;
            pjadard_ts_data->earphone_enable |=  0x0100;
            break;
        default:
            JD_E("%s: unknown status\n", __func__);
            break;
    }

    wdata[0] = (uint8_t)((pjadard_ts_data->earphone_enable & 0x00FF) >> 0);
    wdata[1] = (uint8_t)((pjadard_ts_data->earphone_enable & 0xFF00) >> 8);
    JD_D("%s: earphone = %d, typec_earphone = %d\n", __func__, wdata[0], wdata[1]);

    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.earphone_en, wdata, sizeof(wdata));
}
#endif

static void module_jd9366tp_set_SMWP_enable(bool enable)
{
    uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};

    if (enable) {
        wdata[0] = 1;
    } else {
        wdata[0] = 0;
    }

    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.gesture_en, wdata, sizeof(wdata));
}

static void module_jd9366tp_usb_detect_set(uint8_t *usb_status)
{
    uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};

    wdata[0] = usb_status[1];
    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.usb_en, wdata, sizeof(wdata));
}

static uint8_t module_jd9366tp_get_freq_band(void)
{
    uint8_t rdata[JD_TWO_SIZE];

    module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_debug_addr.freq_band, rdata, sizeof(rdata));
    return rdata[0];
}

static void module_jd9366tp_set_virtual_proximity(bool enable)
{
    uint8_t wdata[JD_TWO_SIZE] = {0x00, 0x00};

    if (enable) {
        wdata[0] = 0x3A;
        wdata[1] = 0xA3;
    }
    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.proximity_en, wdata, sizeof(wdata));
}

#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
static void jd9366tp_SetMpapPwSync(uint8_t value, uint32_t offset)
{
    uint8_t wdata[JD_ONE_SIZE] = {0x00};

    wdata[0] = value;
    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.mpap_pw_sync + offset, wdata, sizeof(wdata));
}

static void module_jd9366tp_APP_SetSortingMode(uint8_t *value, uint8_t value_len)
{
    uint32_t AddrOffset = JD9366TP_MASTER_ADDR_OFFSET;

    /* Write Master then Master transfer to Slave */
    jd9366tp_SetMpapPwSync(0, AddrOffset);
    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.mpap_pw, value, value_len);
    msleep(50);
    jd9366tp_SetMpapPwSync(1, AddrOffset);
}

static void module_jd9366tp_APP_ReadSortingMode(uint8_t *pValue, uint8_t pValue_len)
{
    module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.mpap_pw, pValue, pValue_len);
}

static void module_jd9366tp_APP_GetLcdSleep(uint8_t *pStatus)
{
    uint8_t lcd_sleep_m = 0x00;
    uint8_t lcd_sleep_s = 0x00;

    if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
        jd9366tp_ReadRegSingle((uint32_t)(JD9366TP_SOC_REG_ADDR_SLPOUT + JD9366TP_MASTER_ADDR_OFFSET), &lcd_sleep_m);
        jd9366tp_ReadRegSingle((uint32_t)(JD9366TP_SOC_REG_ADDR_SLPOUT + JD9366TP_SLAVE_ADDR_OFFSET), &lcd_sleep_s);

        if (((lcd_sleep_m & 0x01) == (uint8_t)JD9366TP_SOC_PASSWORD_SLEEP_IN) &&
            ((lcd_sleep_s & 0x01) == (uint8_t)JD9366TP_SOC_PASSWORD_SLEEP_IN)) {
            *pStatus = (uint8_t)JD_LCD_STATUS_SLEEP_IN;
        } else if (((lcd_sleep_m & 0x01) == (uint8_t)JD9366TP_SOC_PASSWORD_SLEEP_OUT) &&
                   ((lcd_sleep_s & 0x01) == (uint8_t)JD9366TP_SOC_PASSWORD_SLEEP_OUT)) {
            *pStatus = (uint8_t)JD_LCD_STATUS_SLEEP_OUT;
        } else {
            *pStatus = (uint8_t)JD_LCD_STATUS_ERROR;
        }
    } else {
        jd9366tp_ReadRegSingle((uint32_t)JD9366TP_SOC_REG_ADDR_SLPOUT, &lcd_sleep_m);

        if ((lcd_sleep_m & 0x01) == (uint8_t)JD9366TP_SOC_PASSWORD_SLEEP_IN) {
            *pStatus = (uint8_t)JD_LCD_STATUS_SLEEP_IN;
        } else if ((lcd_sleep_m & 0x01) == (uint8_t)JD9366TP_SOC_PASSWORD_SLEEP_OUT) {
            *pStatus = (uint8_t)JD_LCD_STATUS_SLEEP_OUT;
        } else {
            *pStatus = (uint8_t)JD_LCD_STATUS_ERROR;
        }
    }
}

static void module_jd9366tp_APP_SetSortingSkipFrame(uint8_t value)
{
    uint32_t AddrOffset = JD9366TP_MASTER_ADDR_OFFSET;

    jd9366tp_WriteRegSingle(g_jd9366tp_chip_info.dsram_host_addr.mpap_skip_frame + AddrOffset, value);
}

static void module_jd9366tp_APP_SetSortingKeepFrame(uint8_t value)
{
    uint32_t AddrOffset = JD9366TP_MASTER_ADDR_OFFSET;

    jd9366tp_WriteRegSingle(g_jd9366tp_chip_info.dsram_host_addr.mpap_keep_frame + AddrOffset, value);
}

static bool module_jd9366tp_APP_ReadSortingBusyStatus(uint8_t mpap_handshake_finish, uint8_t *pStatus)
{
    jd9366tp_ReadRegSingle(g_jd9366tp_chip_info.dsram_host_addr.mpap_handshake, pStatus);

    if (*pStatus == mpap_handshake_finish)
        return true;
    else
        return false;
}

static void module_jd9366tp_GetSortingDiffDataMax(uint8_t *pValue, uint16_t pValue_len)
{
    uint16_t half_len = pValue_len / 2;

    if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
        module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.mpap_diff_max, pValue, half_len);
        module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.mpap_diff_max_s, pValue + half_len, half_len);
    } else {
        module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.mpap_diff_max, pValue, pValue_len);
    }
}

static void module_jd9366tp_GetSortingDiffDataMin(uint8_t *pValue, uint16_t pValue_len)
{
    uint16_t half_len = pValue_len / 2;

    if (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE) {
        module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.mpap_diff_min, pValue, half_len);
        module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.mpap_diff_min_s, pValue + half_len, half_len);
    } else {
        module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.mpap_diff_min, pValue, pValue_len);
    }
}

static void module_jd9366tp_Fw_DBIC_Off(void)
{
    uint8_t wBuf[JD_TWO_SIZE];
    uint32_t AddrOffset;

    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;

    wBuf[0] = 0x24;
    wBuf[1] = 0x02;
    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.fw_dbic_en + AddrOffset, wBuf, sizeof(wBuf));
}

static void module_jd9366tp_Fw_DBIC_On(void)
{
    uint8_t wBuf[JD_TWO_SIZE];
    uint32_t AddrOffset;

    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;

    wBuf[0] = 0xFF;
    wBuf[1] = 0xFF;
    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.fw_dbic_en + AddrOffset, wBuf, sizeof(wBuf));
}

static void module_jd9366tp_StartMCU(void)
{
    uint32_t AddrOffset;

    AddrOffset = (pjadard_ic_data->JD_MODULE_CASCADE_MODE == JD_CASCADE_MODE_ENABLE)
                  ? JD9366TP_BOTH_ADDR_OFFSET : JD9366TP_MASTER_ADDR_OFFSET;

    jd9366tp_StartMCU(AddrOffset);
}

static void module_jd9366tp_ReadMpapErrorMsg(uint8_t *pValue, uint8_t pValue_len)
{
    module_jd9366tp_register_read(g_jd9366tp_chip_info.dsram_host_addr.mpap_error_msg, pValue, pValue_len);
}

static void module_jd9366tp_SetMpBypassMain(void)
{
    uint8_t wBuf[JD_TWO_SIZE];

    wBuf[0] = 0xFF;
    wBuf[1] = 0xFF;
    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.mpap_bypass_main, wBuf, sizeof(wBuf));
}

static void module_jd9366tp_ClearMpBypassMain(void)
{
    uint8_t wBuf[JD_TWO_SIZE];

    wBuf[0] = 0x00;
    wBuf[1] = 0x00;
    module_jd9366tp_register_write(g_jd9366tp_chip_info.dsram_host_addr.mpap_bypass_main, wBuf, sizeof(wBuf));
}
#endif

static void jd9366tp_func_reinit(void)
{
    JD_D("%s:Entering!\n", __func__);
    g_module_fp.fp_EnterBackDoor        = module_jd9366tp_EnterBackDoor;
    g_module_fp.fp_ExitBackDoor         = module_jd9366tp_ExitBackDoor;
    g_module_fp.fp_pin_reset            = module_jd9366tp_PinReset;
    g_module_fp.fp_soft_reset           = module_jd9366tp_SoftReset;
    g_module_fp.fp_ResetMCU             = module_jd9366tp_ResetMCU;
    g_module_fp.fp_PorInit              = module_jd9366tp_PorInit;
    g_module_fp.fp_register_read        = module_jd9366tp_register_read;
    g_module_fp.fp_register_write       = module_jd9366tp_register_write;
    g_module_fp.fp_flash_read           = module_jd9366tp_flash_read;
    g_module_fp.fp_flash_write          = module_jd9366tp_flash_write;
    g_module_fp.fp_flash_erase          = module_jd9366tp_flash_erase;
    g_module_fp.fp_ram_read             = module_jd9366tp_ram_read;
#ifdef JD_ZERO_FLASH
    g_module_fp.fp_ram_write            = module_jd9366tp_ram_write;
#ifdef JD_ESD_CHECK
    g_module_fp.fp_esd_ram_write        = module_jd9366tp_esd_ram_write;
#endif
#endif
    g_module_fp.fp_read_fw_ver          = module_jd9366tp_read_fw_ver;
#ifdef JD_AUTO_UPGRADE_FW
    g_module_fp.fp_read_fw_ver_bin      = module_jd9366tp_read_fw_ver_bin;
#endif
    g_module_fp.fp_mutual_data_set      = module_jd9366tp_mutual_data_set;
    g_module_fp.fp_get_mutual_data      = module_jd9366tp_get_mutual_data;
    g_module_fp.fp_get_touch_data       = module_jd9366tp_get_touch_data;
    g_module_fp.fp_read_mutual_data     = module_jd9366tp_read_mutual_data;
    g_module_fp.fp_set_high_sensitivity = module_jd9366tp_set_high_sensitivity;
    g_module_fp.fp_set_rotate_border    = module_jd9366tp_set_rotate_border;
#ifdef JD_EARPHONE_DETECT
    g_module_fp.fp_set_earphone_enable  = module_jd9366tp_set_earphone_enable;
#endif
    g_module_fp.fp_set_SMWP_enable      = module_jd9366tp_set_SMWP_enable;
    g_module_fp.fp_usb_detect_set       = module_jd9366tp_usb_detect_set;
    g_module_fp.fp_get_freq_band        = module_jd9366tp_get_freq_band;
    g_module_fp.fp_set_virtual_proximity = module_jd9366tp_set_virtual_proximity;
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
    g_module_fp.fp_APP_SetSortingMode        = module_jd9366tp_APP_SetSortingMode;
    g_module_fp.fp_APP_ReadSortingMode       = module_jd9366tp_APP_ReadSortingMode;
    g_module_fp.fp_APP_GetLcdSleep           = module_jd9366tp_APP_GetLcdSleep;
    g_module_fp.fp_APP_SetSortingSkipFrame   = module_jd9366tp_APP_SetSortingSkipFrame;
    g_module_fp.fp_APP_SetSortingKeepFrame   = module_jd9366tp_APP_SetSortingKeepFrame;
    g_module_fp.fp_APP_ReadSortingBusyStatus = module_jd9366tp_APP_ReadSortingBusyStatus;
    /*
     * Using new function g_module_fp.fp_GetSortingDiffDataMax &
     *                    g_module_fp.fp_GetSortingDiffDataMin
     * when g_module_fp.fp_GetSortingDiffData = NULL
     */
    g_module_fp.fp_GetSortingDiffData        = NULL;
    g_module_fp.fp_GetSortingDiffDataMax     = module_jd9366tp_GetSortingDiffDataMax;
    g_module_fp.fp_GetSortingDiffDataMin     = module_jd9366tp_GetSortingDiffDataMin;
    g_module_fp.fp_Fw_DBIC_Off               = module_jd9366tp_Fw_DBIC_Off;
    g_module_fp.fp_Fw_DBIC_On                = module_jd9366tp_Fw_DBIC_On;
    g_module_fp.fp_StartMCU                  = module_jd9366tp_StartMCU;
    g_module_fp.fp_ReadMpapErrorMsg          = module_jd9366tp_ReadMpapErrorMsg;
    g_module_fp.fp_SetMpBypassMain           = module_jd9366tp_SetMpBypassMain;
    g_module_fp.fp_ClearMpBypassMain         = module_jd9366tp_ClearMpBypassMain;
#endif
}

static bool module_jd9366tp_chip_detect(void)
{
    int i = 0;
    bool ret = false;
    uint16_t romid = 0;
    uint16_t CHIP_ID = JD9366TP_ID;

    g_common_variable.dbi_dd_reg_mode = JD9366TP_DDREG_MODE;
    if (g_common_variable.dbi_dd_reg_mode != JD_DDREG_MODE_1) {
        JD_E("%s:Don't support this mode ,dbi_dd_reg_mode %02X not equal %02X\n",
            __func__, g_common_variable.dbi_dd_reg_mode, JD_DDREG_MODE_1);
        return false;
    }

    do {
        /* EnterBackDoor get IC ID */
        jd9366tp_EnterBackDoor(&romid);

        if (romid == CHIP_ID) {
            JD_I("%s: detect IC %s successfully\n", __func__, JD9366TP_OUTER_ID);
            scnprintf(pjadard_ic_data->chip_id, sizeof(pjadard_ic_data->chip_id), JD9366TP_OUTER_ID);
            jd9366tp_func_reinit();
            ret = true;
        } else {
            JD_E("%s:Read driver ID %04X not equal %04X\n", __func__, romid, CHIP_ID);
            msleep(500);
        }
    } while ((++i < 3) && (ret == false));

    return ret;
}

#ifdef CONFIG_CHIP_DTCFG
 int jadard_jd9366tp_probe(struct platform_device *pdev)
{
    struct jadard_support_chip *pChip = NULL;
    struct jadard_support_chip *ptr = NULL;

    JD_I("%s:Enter\n", __func__);

    pChip = kzalloc(sizeof(struct jadard_support_chip), GFP_KERNEL);
    if (pChip == NULL) {
        JD_E("%s: allocate jadard_ts_data failed\n", __func__);
        return -ENOMEM;
    }

    /* Store chip init and detect funtion */
    pChip->chip_init = module_jd9366tp_chip_init;
    pChip->chip_detect = module_jd9366tp_chip_detect;
    pChip->next = NULL;

    if (g_module_fp.head_support_chip == NULL) {
        g_module_fp.head_support_chip = pChip;
    } else {
        ptr = g_module_fp.head_support_chip;
        /* Find the last one */
        while (ptr->next != NULL) {
            ptr = ptr->next;
        }
        /* Link to the last */
        ptr->next = pChip;
    }

    return 0;
}

 int jadard_jd9366tp_remove(struct platform_device *pdev)
{
    struct jadard_support_chip *ptr = g_module_fp.head_support_chip;
    struct jadard_support_chip *pre_ptr = NULL;

    while (ptr != NULL) {
        if ((ptr->chip_init == module_jd9366tp_chip_init) &&
            (ptr->chip_detect == module_jd9366tp_chip_detect)) {
            if (ptr == g_module_fp.head_support_chip) {
                g_module_fp.head_support_chip = ptr->next;
            } else {
                pre_ptr->next = ptr->next;
            }
            kfree(ptr);
            break;
        }

        pre_ptr = ptr;
        ptr = ptr->next;
    }

    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id jadard_jd9366tp_mttable[] = {
    { .compatible = "jadard,jd9366tp"},
    { },
};
#else
#define jadard_jd9366tp_mttable NULL
#endif

static struct platform_driver jadard_jd9366tp_driver = {
    .probe  = jadard_jd9366tp_probe,
    .remove = jadard_jd9366tp_remove,
    .driver = {
        .name           = "JADARD_JD9366TP",
        .owner          = THIS_MODULE,
        .of_match_table = jadard_jd9366tp_mttable,
    },
};



#else
static int jadard_jd9366tp_probe(void)
{
    struct jadard_support_chip *pChip = NULL;
    struct jadard_support_chip *ptr = NULL;

    JD_I("%s:Enter\n", __func__);

    pChip = kzalloc(sizeof(struct jadard_support_chip), GFP_KERNEL);
    if (pChip == NULL) {
        JD_E("%s: allocate jadard_ts_data failed\n", __func__);
        return -ENOMEM;
    }

    /* Store chip init and detect funtion */
    pChip->chip_init = module_jd9366tp_chip_init;
    pChip->chip_detect = module_jd9366tp_chip_detect;
    pChip->next = NULL;

    if (g_module_fp.head_support_chip == NULL) {
        g_module_fp.head_support_chip = pChip;
    } else {
        ptr = g_module_fp.head_support_chip;
        /* Find the last one */
        while (ptr->next != NULL) {
            ptr = ptr->next;
        }
        /* Link to the last */
        ptr->next = pChip;
    }

    return 0;
}

static int jadard_jd9366tp_remove(void)
{
    struct jadard_support_chip *ptr = g_module_fp.head_support_chip;
    struct jadard_support_chip *pre_ptr = NULL;

    while (ptr != NULL) {
        if ((ptr->chip_init == module_jd9366tp_chip_init) &&
            (ptr->chip_detect == module_jd9366tp_chip_detect)) {
            if (ptr == g_module_fp.head_support_chip) {
                g_module_fp.head_support_chip = ptr->next;
            } else {
                pre_ptr->next = ptr->next;
            }
            kfree(ptr);
            break;
        }

        pre_ptr = ptr;
        ptr = ptr->next;
    }

    return 0;
}

 int  jadard_jd9366tp_init(void)
{
    JD_I("%s\n", __func__);
    jadard_jd9366tp_probe();
#if defined(__JADARD_KMODULE__)
    jadard_common_init();
#endif

    return 0;
}

 void  jadard_jd9366tp_exit(void)
{
	JD_I("%s\n", __func__);
    jadard_jd9366tp_remove();
#if defined(__JADARD_KMODULE__)
    jadard_common_exit();
#endif
}
#endif

late_initcall(jadard_jd9366tp_init);
module_exit(jadard_jd9366tp_exit);

MODULE_DESCRIPTION("JADARD JD9366TP touch driver");
MODULE_LICENSE("GPL");
