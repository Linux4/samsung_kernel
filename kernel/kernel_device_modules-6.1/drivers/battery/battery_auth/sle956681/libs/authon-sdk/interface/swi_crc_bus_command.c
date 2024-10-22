/**
 * \copyright
 * MIT License
 *
 * Copyright (c) 2024 Infineon Technologies AG
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE
 *
 * \endcopyright
 */
/**
 * @file   swi_crc_bus_command.c
 * @date   January, 2021
 * @brief  Implementation of SWI bus command
 */

#include "../authon_config.h"
#include "swi_crc_bus_command.h"
#include "../authon_status.h"
#include "swi_crc/swi_crc.h"
#include "../platform/bitbang.h"
#include "register.h"
#include "interface.h"
#include "../../authon-sdk/platform/helper.h"
//#include "../crypto/crypto_lib.h"
#include "../../authon-sdk/ecc/ecc.h"

/**
* @brief Generates SWI bus command EINT
* @param ubIsIntReceived interrupt status received. 
*/
void inline Send_EINT(uint8_t *ubIsIntReceived){
	uint32_t swi_cmd;
	uint8_t  cmd_array[3];
	uint16_t crc_16;

	swi_cmd = (SWI_BC << 8) + SWI_EINT;
	// send command
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// compute CRC
	cmd_array[0] = swi_cmd & 0xff;
	cmd_array[1] = (swi_cmd >> 8) & 0xff;
	cmd_array[2] = (swi_cmd >> 16) & 0x1;
	crc_16 = crc16_gen(cmd_array, 3);

	// send CRC0
	swi_cmd = (SWI_WD << 8) + (crc_16 & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// send CRC1
	swi_cmd = (SWI_WD << 8) + ((crc_16 >> 8) & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, TRUE, FALSE, ubIsIntReceived );

}

/**
* @brief Generates SWI bus command RBL
* @param ub_rbl_numb read burst length 
*/
void inline Send_RBL(uint8_t ub_rbl_numb){
	switch(ub_rbl_numb){
		case 0:
			Swi_SendRawWordNoIrq(SWI_BC, SWI_RBL0);
			break;
		case 1:
			Swi_SendRawWordNoIrq(SWI_BC, SWI_RBL1);
			break;
		case 2:
			Swi_SendRawWordNoIrq(SWI_BC, SWI_RBL2);
			break;
		case 3:
			Swi_SendRawWordNoIrq(SWI_BC, SWI_RBL3);
			break;
		default: break;
	}
}

/**
* @brief Generates SWI bus command DISS
*/
void inline Send_DISS(void){
	Swi_SendRawWordNoIrq(SWI_BC, SWI_DISS);
}

/**
* @brief Generates SWI bus command DIP0 and and check if having interrupt response
* @param ubIsIntReceived Interrupt received status
*/
void inline Send_DIP0(uint8_t *ubIsIntReceived){
	uint32_t swi_cmd;
	uint8_t  cmd_array[3];
	uint16_t crc_16;

	swi_cmd = (SWI_BC << 8) + SWI_DIP0;
	// send command
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// compute CRC
	cmd_array[0] = swi_cmd & 0xff;
	cmd_array[1] = (swi_cmd >> 8) & 0xff;
	cmd_array[2] = (swi_cmd >> 16) & 0x1;
	crc_16 = crc16_gen(cmd_array, 3);

	// send CRC0
	swi_cmd = (SWI_WD << 8) + (crc_16 & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// send CRC1
	swi_cmd = (SWI_WD << 8) + ((crc_16 >> 8) & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, TRUE, TRUE, ubIsIntReceived );

}

/**
* @brief Send SWI bus command DIP1 and check if having interrupt response
* @param ubIsIntReceived Interrupt received status
*/
void inline Send_DIP1(uint8_t *ubIsIntReceived){
	uint32_t swi_cmd;
	uint8_t  cmd_array[3];
	uint16_t crc_16;

	swi_cmd = (SWI_BC << 8) + SWI_DIP1;
	// send command
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// compute CRC
	cmd_array[0] = swi_cmd & 0xff;
	cmd_array[1] = (swi_cmd >> 8) & 0xff;
	cmd_array[2] = (swi_cmd >> 16) & 0x1;
	crc_16 = crc16_gen(cmd_array, 3);

	// send CRC0
	swi_cmd = (SWI_WD << 8) + (crc_16 & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// send CRC1
	swi_cmd = (SWI_WD << 8) + ((crc_16 >> 8) & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, TRUE, TRUE, ubIsIntReceived );
}

/**
* @brief Send SWI bus command DIE0
*/
void inline Send_DIE0(void){
	Swi_SendRawWordNoIrq(SWI_BC, SWI_DIE0);
}

/**
* @brief Send SWI bus command DIE1
*/
void inline Send_DIE1(void){
	Swi_SendRawWordNoIrq(SWI_BC, SWI_DIE1);
}

/**
* @brief Send SWI bus command DI00 and check if having interrupt response
* @param ubIsIntReceived Interrupt received status
*/
void inline Send_DI00(uint8_t *ubIsIntReceived){
	uint32_t swi_cmd;
	uint8_t  cmd_array[3];
	uint16_t crc_16;

	swi_cmd = (SWI_BC << 8) + SWI_DI00;
	// send command
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// compute CRC
	cmd_array[0] = swi_cmd & 0xff;
	cmd_array[1] = (swi_cmd >> 8) & 0xff;
	cmd_array[2] = (swi_cmd >> 16) & 0x1;
	crc_16 = crc16_gen(cmd_array, 3);

	// send CRC0
	swi_cmd = (SWI_WD << 8) + (crc_16 & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// send CRC1
	swi_cmd = (SWI_WD << 8) + ((crc_16 >> 8) & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, TRUE, TRUE, ubIsIntReceived );
}

/**
* @brief Send SWI bus command DI01 and check if having interrupt response
* @param ubIsIntReceived Interrupt received status
*/
void inline Send_DI01(uint8_t *ubIsIntReceived){
	uint32_t swi_cmd;
	uint8_t  cmd_array[3];
	uint16_t crc_16;

	swi_cmd = (SWI_BC << 8) + SWI_DI01;
	// send command
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// compute CRC
	cmd_array[0] = swi_cmd & 0xff;
	cmd_array[1] = (swi_cmd >> 8) & 0xff;
	cmd_array[2] = (swi_cmd >> 16) & 0x1;
	crc_16 = crc16_gen(cmd_array, 3);

	// send CRC0
	swi_cmd = (SWI_WD << 8) + (crc_16 & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// send CRC1
	swi_cmd = (SWI_WD << 8) + ((crc_16 >> 8) & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, TRUE, TRUE, ubIsIntReceived );
}

/**
* @brief Send SWI bus command DI10 and check if having interrupt response
* @param ubIsIntReceived Interrupt received status
*/
void inline Send_DI10(uint8_t *ubIsIntReceived){
	uint32_t swi_cmd;
	uint8_t  cmd_array[3];
	uint16_t crc_16;

	swi_cmd = (SWI_BC << 8) + SWI_DI10;
	// send command
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// compute CRC
	cmd_array[0] = swi_cmd & 0xff;
	cmd_array[1] = (swi_cmd >> 8) & 0xff;
	cmd_array[2] = (swi_cmd >> 16) & 0x1;
	crc_16 = crc16_gen(cmd_array, 3);

	// send CRC0
	swi_cmd = (SWI_WD << 8) + (crc_16 & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// send CRC1
	swi_cmd = (SWI_WD << 8) + ((crc_16 >> 8) & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, TRUE, TRUE, ubIsIntReceived );
}

/**
* @brief Send SWI bus command DI11 and check if having interrupt response
* @param ubIsIntReceived Interrupt received status
*/
void inline Send_DI11(uint8_t *ubIsIntReceived){
	uint32_t swi_cmd;
	uint8_t  cmd_array[3];
	uint16_t crc_16;

	swi_cmd = (SWI_BC << 8) + SWI_DI11;
	// send command
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// compute CRC
	cmd_array[0] = swi_cmd & 0xff;
	cmd_array[1] = (swi_cmd >> 8) & 0xff;
	cmd_array[2] = (swi_cmd >> 16) & 0x1;
	crc_16 = crc16_gen(cmd_array, 3);

	// send CRC0
	swi_cmd = (SWI_WD << 8) + (crc_16 & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// send CRC1
	swi_cmd = (SWI_WD << 8) + ((crc_16 >> 8) & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, TRUE, TRUE, ubIsIntReceived );
}

/**
* @brief Send SWI bus command ERA
* @param ubAddr higher byte of device address
*/
void inline Send_EDA(uint8_t ubAddr){
	Swi_SendRawWordNoIrq(SWI_EDA, ubAddr);
}

/**
* @brief Send SWI bus command SDA
* @param ubAddr Lower byte of device address
*/
void inline Send_SDA(uint8_t ubAddr){
	Swi_SendRawWordNoIrq(SWI_SDA, ubAddr);
}

/**
* @brief Send SWI bus command ERA
* @param ubAddr Higher byte of device address
*/
void inline Send_ERA(uint8_t ubAddr){
	Swi_SendRawWordNoIrq(SWI_ERA, ubAddr);
}

/**
* @brief Send SWI bus command WRA
* @param ubAddr Lower byte of device address
*/
void inline Send_WRA(uint8_t ubAddr){
	Swi_SendRawWordNoIrq(SWI_WRA, ubAddr);
}

/**
* @brief Send SWI bus command RRA
* @param ubAddr Lower byte of register address
* @param rd_len read byte length
* @param ub_data data to be store after read
*/
uint16_t inline Send_RRA(uint8_t ubAddr, uint16_t rd_len, uint8_t *ub_data){
	
	uint16_t ret=INF_SWI_INIT;
	uint32_t g_ulBaudStop = Get_SWIStopBaud();
	
	Swi_SendRawWordNoIrq(SWI_RRA, ubAddr);
	ret = Swi_ReceiveData( ub_data, rd_len);
	udelay(g_ulBaudStop);

	return ret;
}

/**
* @brief Send SWI bus command WD
* @param data_len write byte length
* @param ub_data data to be written
*/
uint16_t inline Send_WD(uint16_t data_len, uint8_t *ub_data){
	uint16_t i;
	uint32_t swi_cmd;
	uint8_t  cmd_array[3*50]; // max wd length was ECCC, 21 bytes
	uint16_t crc_16;
	uint16_t j;

	j = 0;
	for(i=0; i<data_len; i++){
		swi_cmd = (SWI_WD << 8) + ub_data[i];
		// send command
		swi_cmd = Swi_ComputeParity(swi_cmd);
		swi_cmd = Swi_AddInvertFlag(swi_cmd);
		Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

		// compute CRC
		cmd_array[j++] = swi_cmd & 0xff;
		cmd_array[j++] = (swi_cmd >> 8) & 0xff;
		cmd_array[j++] = (swi_cmd >> 16) & 0x1;
	}
	crc_16 = crc16_gen(cmd_array, j);

	// send CRC0
	swi_cmd = (SWI_WD << 8) + (crc_16 & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// send CRC1
	swi_cmd = (SWI_WD << 8) + ((crc_16 >> 8) & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );
	return INF_SWI_SUCCESS;
}

/**
* @brief Send SWI bus command BRES
*/
uint16_t inline Send_BRES(void){

	Swi_SendRawWordNoIrq(SWI_BC, SWI_BRES);
	udelay(Get_PowerdownTime());
	return INF_SWI_SUCCESS;
}

/**
* @brief Send SWI bus command PDWN
*/
uint16_t inline Send_PDWN(void){

	Swi_SendRawWordNoIrq(SWI_BC, SWI_PDWN);
	udelay(Get_PowerdownTime());
	return INF_SWI_SUCCESS;

}

/**
* @brief Send SWI bus command HRREQ
*/
uint16_t inline Send_HRREQ(void)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_HRREQ);
	return INF_SWI_SUCCESS;
}

/**
* @brief Send SWI bus command HRRES
*/
uint16_t inline Send_HRRES(uint8_t *ub_data)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_HRRES);
	Send_ERA(SWI_ERA_DUMMYADDR);
	return Send_RRA(SWI_WRARRA_DUMMYADDR, MAC_BYTE_LEN, ub_data);

}

/**
* @brief Send SWI bus command WDA
* @param ubAddr New device address
*/
uint16_t inline Send_WDA(uint16_t ubAddr){

	uint8_t data_wr[2];

	Swi_SendRawWordNoIrq(SWI_BC, SWI_WDA);
	Send_ERA(SWI_ERA_DUMMYADDR);
	Send_WRA(SWI_WRARRA_DUMMYADDR);
	data_wr[0] = HiByte(ubAddr);
	data_wr[1] = LoByte(ubAddr);
	Send_WD(2,data_wr);
	delay_ms(NVM_PROGRAMMING_TIME);
	return INF_SWI_SUCCESS;
}

/**
* @brief Send SWI bus command MACS
* @param uw_mac_addr NVM address to be MAC
*/
uint16_t inline Send_MACS(uint16_t uw_mac_addr)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_MACS);
	Send_ERA(HiByte(uw_mac_addr));
	Send_WRA(LoByte(uw_mac_addr));
	
	return INF_SWI_SUCCESS;
}

/**
* @brief Send SWI bus command MACR
* @param ub_data read response data
*/
uint16_t inline Send_MACR(uint8_t *ub_data)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_MACR);
	Send_ERA(SWI_ERA_DUMMYADDR);
	return Send_RRA(SWI_WRARRA_DUMMYADDR, MAC_BYTE_LEN, ub_data);
}

/**
* @brief Send SWI bus command MACK
* @param ub_data pointer for mac result of kill password
*/
uint16_t inline Send_MACK(uint8_t *ub_data)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_MACK);
	Send_ERA(SWI_ERA_DUMMYADDR);
	Send_WRA(SWI_WRARRA_DUMMYADDR);

	Send_WD(MAC_BYTE_LEN, ub_data);
	delay_ms(MACK_PROGRAMMING_TIME);
	
	return INF_SWI_SUCCESS;
}

/**
* @brief Send SWI bus command MACCR5
* @param ub_data pointer for mac result of reset password
*/
uint16_t inline Send_MACCR5(uint8_t *ub_data)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_MACCR5);
	Send_ERA(SWI_ERA_DUMMYADDR);
	Send_WRA(SWI_WRARRA_DUMMYADDR);
	delay_ms(MACCR_PROGRAMMING_TIME);
	return Send_WD(MAC_BYTE_LEN, ub_data);
}

/**
* @brief Send SWI bus command ECCS1
*/
uint16_t inline Send_ECCS1(void)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_ECCS1);
	return INF_SWI_SUCCESS;
}

/**
* @brief Send SWI bus command ECCS2
*/
uint16_t inline Send_ECCS2(void)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_ECCS2);
	return INF_SWI_SUCCESS;
}

/**
* @brief Send SWI bus command ECCC
* @param ub_data pointer to challenge data
*/
uint16_t inline Send_ECCC(uint8_t *ub_data)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_ECCC);
	Send_ERA(SWI_ERA_DUMMYADDR);
	Send_WRA(SWI_WRARRA_DUMMYADDR);
	return Send_WD(ECC_CHALLENGE_LEN, ub_data);
}

/**
* @brief Send SWI bus command ECCR
* @param ub_data pointer to response data
*/
uint16_t inline Send_ECCR(uint8_t *ub_data)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_ECCR);
	Send_ERA(SWI_ERA_DUMMYADDR);
	return Send_RRA(SWI_WRARRA_DUMMYADDR, ECC_RESPONSE_LEN, ub_data);
}

//#if (HOST_AUTHENTICATION_FEATURE_SUPPORTED==1)
/**
* @brief Send SWI bus command DRRES
* @param ub_data pointer to nouce B
*/
uint16_t inline Send_DRRES(uint8_t *ub_data)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_DRRES);
	Send_ERA(SWI_ERA_DUMMYADDR);
	Send_WRA(SWI_WRARRA_DUMMYADDR);
	Send_WD(MAC_BYTE_LEN, ub_data);
	return INF_SWI_SUCCESS;
}

/**
* @brief Send SWI bus command HREQ1
* @param ub_data pointer to Tag A
*/
uint16_t inline Send_HREQ1(uint8_t *ub_data)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_HREQ1);
	Send_ERA(SWI_ERA_DUMMYADDR);
	Send_WRA(SWI_WRARRA_DUMMYADDR);
	return Send_WD(MAC_BYTE_LEN, ub_data);
}

/**
* @brief Send SWI bus command DRES1
* @param ub_data pointer to Tag B
*/
uint16_t inline Send_DRES1(uint8_t *ub_data)
{
	Swi_SendRawWordNoIrq(SWI_BC, SWI_DRES1);
	Send_ERA(SWI_ERA_DUMMYADDR);
	return Send_RRA(SWI_WRARRA_DUMMYADDR, MAC_BYTE_LEN, ub_data);
}
//#endif

uint16_t Get_error_code(uint8_t* error_code)
{
	uint16_t ret;

	ret = Intf_ReadRegister(ON_SFR_SWI_ERR_CODE_STS, error_code, 1);
	if (SDK_INTERFACE_SUCCESS!=ret ){
		PRINT("Read Register 0x%.4X failed. ret=%x\r\n", ON_SFR_SWI_ERR_CODE_STS, ret);
		return ret;
	}
	return INF_SWI_SUCCESS;
}
