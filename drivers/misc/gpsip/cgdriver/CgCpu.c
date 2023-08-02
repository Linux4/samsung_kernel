/******************************************************************************
 *                                LEGAL NOTICE                                *
 *                                                                            *
 *  USE OF THIS SOFTWARE (including any copy or compiled version thereof) AND *
 *  DOCUMENTATION IS SUBJECT TO THE SOFTWARE LICENSE AND RESTRICTIONS AND THE *
 *  WARRANTY DISLCAIMER SET FORTH IN LEGAL_NOTICE.TXT FILE. IF YOU DO NOT     *
 *  FULLY ACCEPT THE TERMS, YOU MAY NOT INSTALL OR OTHERWISE USE THE SOFTWARE *
 *  OR DOCUMENTATION.                                                         *
 *  NOTWITHSTANDING ANYTHING TO THE CONTRARY IN THIS NOTICE, INSTALLING OR    *
 *  OTHERISE USING THE SOFTWARE OR DOCUMENTATION INDICATES YOUR ACCEPTANCE OF *
 *  THE LICENSE TERMS AS STATED.                                              *
 *                                                                            *
 ******************************************************************************/
/* Version: 1.8.9\3686 */
/* Build  : 13 */
/* Date   : 12/08/2012 */

/**
	\file
	\brief CellGuide Common code
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide

*/

#include <linux/module.h>
#include <linux/kernel.h>
#include "CgxDriverCore.h"
#include "CgCpu.h"
// Swap helper functions
TCgByteSwapType CgGetByteSwapTypeU32(TCgByteOrder aFrom, TCgByteOrder aTo);
void * CgGetSwapFunctionU32(TCgByteSwapType type);


static const U32 fullMask[32] = {
	0x00000001, 0x00000003, 0x00000007, 0x0000000F,
	0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
	0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
	0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
	0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
	0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
	0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
	0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF	};

U32 CgGetU32Mask(U32 bitsInU32)
{
	return fullMask[bitsInU32];
}

TCgReturnCode CgSetRegisterBits(volatile U32 *aReg, TCgBitField aBitFieldDef, U32 aValue)
{

	U32 value = aValue & fullMask[aBitFieldDef.length-1];
	U32 mask = (fullMask[aBitFieldDef.length-1]) & ~value;

	// clear
	*aReg &= ~(mask << aBitFieldDef.start);
	// set
	*aReg |= (value << aBitFieldDef.start);
	return ECgOk;
}


TCgReturnCode CgGetRegisterBits(volatile U32 aReg, TCgBitField aBitFieldDef, U32 *apValue)
{

	*apValue = ((aReg) >> aBitFieldDef.start) & fullMask[aBitFieldDef.length-1];

	return ECgOk;
}


TCgByteSwapType CgGetByteSwapTypeU32(TCgByteOrder aFrom, TCgByteOrder aTo)
{
	switch (aFrom){
		case ECG_BYTE_ORDER_1234 :
			switch( aTo ){
				case ECG_BYTE_ORDER_1234 : return ECG_SWAP_NONE;
				case ECG_BYTE_ORDER_2143 : return ECG_SWAP_BYTES;
				case ECG_BYTE_ORDER_3412 : return ECG_SWAP_WORDS;
				case ECG_BYTE_ORDER_4321 : return ECG_SWAP_BYTES_AND_WORDS;
				}
			break;
		case ECG_BYTE_ORDER_2143 :
			switch( aTo ){
				case ECG_BYTE_ORDER_1234 : return ECG_SWAP_BYTES;
				case ECG_BYTE_ORDER_2143 : return ECG_SWAP_NONE;
				case ECG_BYTE_ORDER_3412 : return ECG_SWAP_BYTES_AND_WORDS;
				case ECG_BYTE_ORDER_4321 : return ECG_SWAP_WORDS;
				}
			break;
		case ECG_BYTE_ORDER_3412 :
			switch( aTo ) {
				case ECG_BYTE_ORDER_1234 : return ECG_SWAP_WORDS;
				case ECG_BYTE_ORDER_2143 : return ECG_SWAP_BYTES_AND_WORDS;
				case ECG_BYTE_ORDER_3412 : return ECG_SWAP_NONE;
				case ECG_BYTE_ORDER_4321 : return ECG_SWAP_BYTES;
				}
			break;
		case ECG_BYTE_ORDER_4321 :
			switch( aTo ) {
				case ECG_BYTE_ORDER_1234 : return ECG_SWAP_BYTES_AND_WORDS;
				case ECG_BYTE_ORDER_2143 : return ECG_SWAP_WORDS;
				case ECG_BYTE_ORDER_3412 : return ECG_SWAP_BYTES;
				case ECG_BYTE_ORDER_4321 : return ECG_SWAP_NONE;
				}
			break;
		default :
			return ECG_SWAP_NONE;
			break;
			}

	return ECG_SWAP_NONE;
}



TCgReturnCode CgSwapBytesInBlock(void * aSource,void * aDest, long aSizeInBytes, TCgByteOrder aFrom, TCgByteOrder aTo)
{
	TCgReturnCode rc = ECgOk;

	long index = 0, sizeInDword = 0;
	U32 * source = NULL, * dest = NULL;
	TCgByteSwapType swapType = CgGetByteSwapTypeU32(aFrom, aTo);

	sizeInDword = aSizeInBytes / 4;
	source =	(U32*)aSource;
	dest =		(U32*)aDest;

	switch (swapType) {
		case ECG_SWAP_NONE				:
			break;

		case ECG_SWAP_BYTES				:
			for ( ; index < sizeInDword; index ++ )
				dest[index] = CgSwapBytesU32(source[index]);
			break;

		case ECG_SWAP_WORDS				:
			for ( ; index < sizeInDword; index ++ )
				dest[index] = CgSwapWordsU32(source[index]);
			break;

		case ECG_SWAP_BYTES_AND_WORDS	:
			for ( ; index < sizeInDword; index ++ )
				dest[index] = CgSwapBytesAndWordsU32(source[index]);
			break;
		}


	return rc;
}

TCgReturnCode CgCpuDelay(unsigned long aCount)
{
	volatile unsigned long cnt = 0;
	volatile unsigned long z =2 ;
	for(cnt = 0; cnt<aCount; cnt++) {
		z *=2;
		}
	return ECgOk;
}

TCgReturnCode CgCpuRFControlWriteByte(unsigned char aValue)
{
        printk(" CgCpuControlWriteByte.\n");
             if(aValue == 1)
                  CgxDriverRFPowerUp();
             else if(aValue == 0)
                  CgxDriverRFPowerDown();
             else
                  printk(" invalid arg.\n");
      return ECgOk;
}

#ifdef CG_STORE_ERROR_LINE
	BOOL cgRetCode_store_error_line(const char * aFileName, long lineNumber, TCgReturnCode aRc) {
		return TRUE;
		}
#endif
