
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include "sii9136.h"
#include "sii9136_driver.h"
#include "sii9136_hdcp.h"
#include "sii9136_reg.h"
#include "def.h"


#ifdef DEV_SUPPORT_HDCP

#define AKSV_SIZE              5
#define NUM_OF_ONES_IN_KSV    20

bool HDCP_TxSupports;
bool HDCP_Started;
byte HDCP_LinkProtectionLevel;

static bool AreAKSV_OK(void);

#ifdef CHECKREVOCATIONLIST
static bool CheckRevocationList(void);
#endif

#ifdef READKSV
static bool GetKSV(void);
static bool IsRepeater(void);
#endif


//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   HDCP_Init()
// PURPOSE      :   Tests Tx and Rx support of HDCP. If found, checks if
//                  and attempts to set the security level accordingly.
// INPUT PARAMS :   None
// OUTPUT PARAMS:   None
// GLOBALS USED :	HDCP_TxSupports - initialized to FALSE, set to TRUE if supported by this device
//					HDCP_Started - initialized to FALSE;
//					HDCP_LinkProtectionLevel - initialized to (EXTENDED_LINK_PROTECTION_NONE | LOCAL_LINK_PROTECTION_NONE);
// RETURNS      :   None
//////////////////////////////////////////////////////////////////////////////
void HDCP_Init (void)
{
	pr_info("%s()\n", __func__);

	HDCP_TxSupports = FALSE;
	HDCP_Started = FALSE;
	HDCP_LinkProtectionLevel = EXTENDED_LINK_PROTECTION_NONE | LOCAL_LINK_PROTECTION_NONE;

	// This is TX-related... need only be done once.
    if (!IsHDCP_Supported())
    {
		DEV_DBG("HDCP -> TX does not support HDCP\n");
		return;
	}

	// This is TX-related... need only be done once.
    if (!AreAKSV_OK())
    {
        DEV_DBG("HDCP -> Illegal AKSV\n");
        return;
    }

	// Both conditions above must pass or authentication will never be attempted. 
	DEV_DBG("HDCP -> Supported by TX\n");
	HDCP_TxSupports = TRUE;

}

void HDCP_CheckStatus (byte InterruptStatusImage)
{
	byte QueryData;
	byte LinkStatus;
	byte RegImage;
	byte NewLinkProtectionLevel;
#ifdef READKSV
	byte RiCnt;
#endif

	if (HDCP_TxSupports == TRUE)
	{
		if ((HDCP_LinkProtectionLevel == (EXTENDED_LINK_PROTECTION_NONE | LOCAL_LINK_PROTECTION_NONE)) && (HDCP_Started == FALSE))
		{
			QueryData = ReadByteTPI(TPI_HDCP_QUERY_DATA_REG);

			if (QueryData & PROTECTION_TYPE_MASK)   // Is HDCP avaialable
			{
				HDCP_On();
			}
		}

		// Check if Link Status has changed:
		if (InterruptStatusImage & SECURITY_CHANGE_EVENT)
		{
			TPI_DEBUG_PRINT (("HDCP -> "));

			LinkStatus = ReadByteTPI(TPI_HDCP_QUERY_DATA_REG);
			LinkStatus &= LINK_STATUS_MASK;

			ClearInterrupt(SECURITY_CHANGE_EVENT);

			switch (LinkStatus)
			{
				case LINK_STATUS_NORMAL:
					TPI_DEBUG_PRINT (("Link = Normal\n"));
					break;

				case LINK_STATUS_LINK_LOST:
					TPI_DEBUG_PRINT (("Link = Lost\n"));
					RestartHDCP();
					break;

				case LINK_STATUS_RENEGOTIATION_REQ:
					TPI_DEBUG_PRINT (("Link = Renegotiation Required\n"));
					HDCP_Off();
					HDCP_On();
					break;

				case LINK_STATUS_LINK_SUSPENDED:
					TPI_DEBUG_PRINT (("Link = Suspended\n"));
					HDCP_On();
					break;
			}
		}

		// Check if HDCP state has changed:
		if (InterruptStatusImage & HDCP_CHANGE_EVENT)
		{
			RegImage = ReadByteTPI(TPI_HDCP_QUERY_DATA_REG);

			NewLinkProtectionLevel = RegImage & (EXTENDED_LINK_PROTECTION_MASK | LOCAL_LINK_PROTECTION_MASK);
			if (NewLinkProtectionLevel != HDCP_LinkProtectionLevel)
			{
				TPI_DEBUG_PRINT (("HDCP -> "));

				HDCP_LinkProtectionLevel = NewLinkProtectionLevel;

				switch (HDCP_LinkProtectionLevel)
				{
					case (EXTENDED_LINK_PROTECTION_NONE | LOCAL_LINK_PROTECTION_NONE):
						TPI_DEBUG_PRINT (("Protection = None\n"));
						RestartHDCP();
						break;

					case LOCAL_LINK_PROTECTION_SECURE:

#ifdef USE_BLACK_MODE
  						mdelay(10); // Wait about half a frame for compliance.
						SetAudioMute(AUDIO_MUTE_NORMAL);
#ifndef USE_DE_GENERATOR
						SetInputColorSpace (INPUT_COLOR_SPACE_RGB);
#else
						SetInputColorSpace (INPUT_COLOR_SPACE_YCBCR422);
#endif
#else	//AV MUTE
						ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, AV_MUTE_MASK, AV_MUTE_NORMAL);
#endif
						TPI_DEBUG_PRINT (("Protection = Local, Video Unmuted\n"));
						break;

					case (EXTENDED_LINK_PROTECTION_SECURE | LOCAL_LINK_PROTECTION_SECURE):
						TPI_DEBUG_PRINT (("Protection = Extended\n"));
#ifdef READKSV
 						if (IsRepeater())
						{
							RiCnt = ReadIndexedRegister(INDEXED_PAGE_0, 0x25);
							while (RiCnt > 0x70)  // Frame 112
							{
								RiCnt = ReadIndexedRegister(INDEXED_PAGE_0, 0x25);
							}
							ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, 0x06, 0x06);
							GetKSV();
							RiCnt = ReadByteTPI(TPI_SYSTEM_CONTROL_DATA_REG);
							ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, 0x08, 0x00);
						}
#endif
						// Trace of the KSV List Forwarding Status - For Bug#17892
						{
						byte	ksv = ReadByteTPI(TPI_KSV_FIFO_FIRST_STATUS_REG);

						if (ksv & KSV_FIFO_FIRST_YES)
							TPI_DEBUG_PRINT (("KSV Fwd: ksv_fifo_first is TRUE.   "));
						else
							TPI_DEBUG_PRINT (("KSV Fwd: ksv_fifo_first is FALSE.  ");)
						ksv = ReadByteTPI(TPI_KSV_FIFO_LAST_STATUS_REG);
						if (ksv & KSV_FIFO_LAST_YES)
							TPI_DEBUG_PRINT (("KSV Fwd: ksv_fifo_last is TRUE.  Count = %d\n", (int)(ksv & KSV_FIFO_BYTES_MASK)));
						else
							TPI_DEBUG_PRINT (("KSV Fwd: ksv_fifo_last is FALSE\n"));
						}
						break;

					default:
						TPI_DEBUG_PRINT (("Protection = Extended but not Local?\n"));
						RestartHDCP();
						break;
				}
			}

			ClearInterrupt(HDCP_CHANGE_EVENT);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   IsHDCP_Supported()
// PURPOSE      :   Check Tx revision number to find if this Tx supports HDCP
//                  by reading the HDCP revision number from TPI register 0x30.
// INPUT PARAMS :   None
// OUTPUT PARAMS:   None
// GLOBALS USED :   None
// RETURNS      :   TRUE if Tx supports HDCP. FALSE if not.
//////////////////////////////////////////////////////////////////////////////
bool IsHDCP_Supported(void)
{

    byte HDCP_Rev;
	bool HDCP_Supported;

	pr_info("%s()\n", __func__);

	HDCP_Supported = TRUE;

	// Check Device ID
    HDCP_Rev = ReadByteTPI(TPI_HDCP_REVISION_DATA_REG);

    if (HDCP_Rev != (HDCP_MAJOR_REVISION_VALUE | HDCP_MINOR_REVISION_VALUE))
	{
    	HDCP_Supported = FALSE;
	}

#ifdef SiI_9022AYBT_DEVICEID_CHECK
	// Even if HDCP is supported check for incorrect Device ID
	HDCP_Rev = ReadByteTPI(TPI_AKSV_1_REG);
	if (HDCP_Rev == 0x90)
	{
		HDCP_Rev = ReadByteTPI(TPI_AKSV_2_REG);
		if (HDCP_Rev == 0x22)
		{
			HDCP_Rev = ReadByteTPI(TPI_AKSV_3_REG);
			if (HDCP_Rev == 0xA0)
			{
				HDCP_Rev = ReadByteTPI(TPI_AKSV_4_REG);
				if (HDCP_Rev == 0x00)
				{
					HDCP_Rev = ReadByteTPI(TPI_AKSV_5_REG);
					if (HDCP_Rev == 0x00)
					{
						HDCP_Supported = FALSE;
					}
				}
			}
		}
	}
#endif

	return HDCP_Supported;
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   HDCP_On()
// PURPOSE      :   Switch hdcp on.
// INPUT PARAMS :   None
// OUTPUT PARAMS:   None
// GLOBALS USED :   HDCP_Started set to TRUE
// RETURNS      :   None
//////////////////////////////////////////////////////////////////////////////
void HDCP_On(void)
{
	TPI_TRACE_PRINT((">>HDCP_On()\n"));
	TPI_DEBUG_PRINT(("HDCP Started\n"));

	WriteByteTPI(TPI_HDCP_CONTROL_DATA_REG, PROTECTION_LEVEL_MAX);

	HDCP_Started = TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   HDCP_Off()
// PURPOSE      :   Switch hdcp off.
// INPUT PARAMS :   None
// OUTPUT PARAMS:   None
// GLOBALS USED :   HDCP_Started set to FALSE
// RETURNS      :   None
//////////////////////////////////////////////////////////////////////////////
void HDCP_Off(void)
{
	TPI_TRACE_PRINT((">>HDCP_Off()\n"));
	TPI_DEBUG_PRINT(("HDCP Stopped\n"));
#ifdef USE_BLACK_MODE
	SetInputColorSpace(INPUT_COLOR_SPACE_BLACK_MODE);
	SetAudioMute(AUDIO_MUTE_MUTED);
#else	// AV MUTE
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, AV_MUTE_MASK, AV_MUTE_MUTED);
#endif
	WriteByteTPI(TPI_HDCP_CONTROL_DATA_REG, PROTECTION_LEVEL_MIN);

	HDCP_Started = FALSE;
	HDCP_LinkProtectionLevel = EXTENDED_LINK_PROTECTION_NONE | LOCAL_LINK_PROTECTION_NONE;
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  AreAKSV_OK()
// PURPOSE       :  Check if AKSVs contain 20 '0' and 20 '1'
// INPUT PARAMS  :  None
// OUTPUT PARAMS :  None
// GLOBALS USED  :  TBD
// RETURNS       :  TRUE if 20 zeros and 20 ones found in AKSV. FALSE OTHERWISE
//////////////////////////////////////////////////////////////////////////////
static bool AreAKSV_OK(void)
{
    byte B_Data[AKSV_SIZE];
    byte NumOfOnes = 0;

    byte i;
    byte j;

	pr_info("%s()\n", __func__);

    ReadBlockTPI(TPI_AKSV_1_REG, AKSV_SIZE, B_Data);

    for (i=0; i < AKSV_SIZE; i++)
    {
        for (j=0; j < BYTE_SIZE; j++)
        {
            if (B_Data[i] & 0x01)
            {
                NumOfOnes++;
            }
            B_Data[i] >>= 1;
        }
     }
     if (NumOfOnes != NUM_OF_ONES_IN_KSV)
        return FALSE;

    return TRUE;
}


#endif
