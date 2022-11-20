
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include "sii9136.h"
#include "sii9136_driver.h"
#include "sii9136_hdcp.h"
#include "sii9136_edid.h"
#include "sii9136_cec.h"
#include "sii9136_reg.h"
#include "sii9136_video_mode_table.h"
#include "def.h"

VideoFormat_t	video_information;
byte vid_mode;

// Checksums
byte g_audio_Checksum;	// Audio checksum

void SetFormat(byte *Data);

extern byte SI_CecHandler (byte, bool);

static void TxHW_Reset (void);
static bool StartTPI (void);
void WriteByteTPI(byte RegOffset, byte Data);

void ReadModifyWriteIndexedRegister(byte PageNum, byte RegOffset, byte Mask, byte Value);

static bool DisableInterrupts (byte);
static bool SetPreliminaryInfoFrames (void);

static void TxPowerState(byte powerState);

static void OnHdmiCableConnected (void);
static void OnHdmiCableDisconnected (void);

static void OnDownstreamRxPoweredDown (void);
static void OnDownstreamRxPoweredUp (void);


#define T_EN_TPI       	10
#define T_HPD_DELAY    	10

static bool tmdsPoweredUp;
static bool hdmiCableConnected;
static bool dsRxPoweredUp;
static byte txPowerState;

bool TPI_Init(void)
{
	tmdsPoweredUp = FALSE;
	hdmiCableConnected = FALSE;
	dsRxPoweredUp = FALSE;
	txPowerState = TX_POWER_STATE_D0;				// Chip powers up in D2 mode, but init to D0 for testing prupose.

#ifdef DEV_SUPPORT_EDID
		edidDataValid = FALSE;							// Move this into EDID_Init();
#endif

	
	TxHW_Reset();									// Toggle TX reset pin


	if (StartTPI()) 								// Enable HW TPI mode, check device ID
		{
		pr_info("%s()\n", __func__);

#ifdef DEV_SUPPORT_HDCP
			HDCP_Init();
#endif

#ifdef DEV_SUPPORT_CEC
			InitCPI();
#endif
	
#ifdef DEV_SUPPORT_EHDMI
			EHDMI_Init();
#endif
	
	
#if defined SiI9232_OR_SiI9236
			OnMobileHDCableDisconnected();					// Default to USB Mode.
#endif
	
			SI_CecInit();

			return TRUE;
		}


	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   ReadByteTPI ()
// PURPOSE      :   Read one byte from a given offset of the TPI interface.
// INPUT PARAMS :   Offset of TPI register to be read; A pointer to the variable
//                  where the data read will be stored
// OUTPUT PARAMS:   Data - Contains the value read from the register value
//                  specified as the first input parameter
// GLOBALS USED :   None
// RETURNS      :   TRUE
// NOTE         :   ReadByteTPI() is ported from the PC based FW to the uC
//                  version while retaining the same function interface. This
//                  will save the need to modify higher level I/O functions
//                  such as ReadSetWriteTPI(), ReadClearWriteTPI() etc.
//                  A dummy returned value (always TRUE) is provided for
//                  the same reason
//////////////////////////////////////////////////////////////////////////////
byte ReadByteTPI(byte RegOffset)
{
    return hdmi_read_reg(TX_SLAVE_ADDR, RegOffset);
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  WriteByteTPI ()
// PURPOSE       :  Write one byte to a given offset of the TPI interface.
// INPUT PARAMS  :  Offset of TPI register to write to; value to be written to
//                  that TPI retister
// OUTPUT PARAMS :  None
// GLOBALS USED  :  None
// RETURNS       :  VOID
//////////////////////////////////////////////////////////////////////////////
void WriteByteTPI(byte RegOffset, byte Data)
{
    hdmi_write_reg(TPI_BASE_ADDR, RegOffset, Data);
}

#define TPI_INTERNAL_PAGE_REG	0xBC
#define TPI_INDEXED_OFFSET_REG	0xBD
#define TPI_INDEXED_VALUE_REG	0xBE

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  ReadSetWriteTPI(byte Offset, byte Pattern)
// PURPOSE       :  Write "1" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Pattern"; Leave all other bits in "Offset"
//                  unchanged.
// INPUT PARAMS  :  Offset  :   TPI register offset
//                  Pattern :   GPIO bits that need to be set
// OUTPUT PARAMS :  None
// GLOBALS USED  :  None
// RETURNS       :  TRUE
//////////////////////////////////////////////////////////////////////////////
void ReadSetWriteTPI(byte Offset, byte Pattern)
{
    byte Tmp;

    Tmp = ReadByteTPI(Offset);

    Tmp |= Pattern;
    WriteByteTPI(Offset, Tmp);
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  ReadClearWriteTPI(byte Offset, byte Pattern)
// PURPOSE       :  Write "0" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Pattern"; Leave all other bits in "Offset"
//                  unchanged.
// INPUT PARAMS  :  Offset  :   TPI register offset
//                  Pattern :   "1" for each TPI register bit that needs to be
//                              cleared
// OUTPUT PARAMS :  None
// GLOBALS USED  :  None
// RETURNS       :  TRUE
//////////////////////////////////////////////////////////////////////////////
void ReadClearWriteTPI(byte Offset, byte Pattern)
{
    byte Tmp;

    Tmp = ReadByteTPI(Offset);

    Tmp &= ~Pattern;
    WriteByteTPI(Offset, Tmp);
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  ReadModifyWriteTPI(byte Offset, byte Mask, byte Value)
// PURPOSE       :  Write "Value" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Mask"; Leave all other bits in "Offset"
//                  unchanged.
// INPUT PARAMS  :  Offset  :   TPI register offset
//                  Mask    :   "1" for each TPI register bit that needs to be
//                              modified
//					Value   :   The desired value for the register bits in their
//								proper positions
// OUTPUT PARAMS :  None
// GLOBALS USED  :  None
// RETURNS       :  void
//////////////////////////////////////////////////////////////////////////////
void ReadModifyWriteTPI(byte Offset, byte Mask, byte Value)
{
    byte Tmp;

    Tmp = ReadByteTPI(Offset);
    Tmp &= ~Mask;
	Tmp |= (Value & Mask);
    WriteByteTPI(Offset, Tmp);
}


////////////////////////////////////////////////////////////////////////////////
// FUNCTION         :   ReadBlockTPI ()
// PURPOSE          :   Read NBytes from offset Addr of the TPI slave address
//                      into a byte Buffer pointed to by Data
// INPUT PARAMETERS :   TPI register offset, number of bytes to read and a
//                      pointer to the data buffer where the data read will be
//                      saved
// OUTPUT PARAMETERS:   pData - pointer to the buffer that will store the TPI
//                      block to be read
// RETURNED VALUE   :   VOID
// GLOBALS USED     :   None
////////////////////////////////////////////////////////////////////////////////
void ReadBlockTPI(byte TPI_Offset, word NBytes, byte * pData)
{
    hdmi_read_block_reg(TPI_BASE_ADDR, TPI_Offset, NBytes, pData);
}

////////////////////////////////////////////////////////////////////////////////
// FUNCTION         :   WriteBlockTPI ()
// PURPOSE          :   Write NBytes from a byte Buffer pointed to by Data to
//                      the TPI I2C slave starting at offset Addr
// INPUT PARAMETERS :   TPI register offset to start writing at, number of bytes
//                      to write and a pointer to the data buffer that contains
//                      the data to write
// OUTPUT PARAMETERS:   None.
// RETURNED VALUES  :   void
// GLOBALS USED     :   None
////////////////////////////////////////////////////////////////////////////////
void WriteBlockTPI(byte TPI_Offset, word NBytes, byte * pData)
{
    hdmi_write_block_reg(TPI_BASE_ADDR, TPI_Offset, NBytes, pData);
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   ReadIndexedRegister()
// PURPOSE      :   Read an indexed register value
//                  Write:
//                      1. 0xBC => Internal page num
//                      2. 0xBD => Indexed register offset
//                  Read:
//                      3. 0xBE => Returns the indexed register value
// INPUT PARAMS :   Internal page number, indexed register offset, pointer to
//                  buffer to store read value
// OUTPUT PARAMS:   Buffer that stores the read value
// GLOBALS USED :   None
// RETURNS      :   TRUE 
//////////////////////////////////////////////////////////////////////////////
#ifdef DEV_INDEXED_READ
byte ReadIndexedRegister(byte PageNum, byte RegOffset) {
    WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);		// Internal page
    WriteByteTPI(TPI_INDEXED_OFFSET_REG, RegOffset);	// Indexed register
    return ReadByteTPI(TPI_INDEXED_VALUE_REG); 		// Return read value
}
#endif

//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   WriteIndexedRegister()
// PURPOSE      :   Write a value to an indexed register
//                  Write:
//                      1. 0xBC => Internal page num
//                      2. 0xBD => Indexed register offset
//                      3. 0xBE => Set the indexed register value
// INPUT PARAMS :   Internal page number, indexed register offset, value
//                  to write to indexed register
// OUTPUT PARAMS:   None
// GLOBALS USED :   None
// RETURNS      :   TRUE 
//////////////////////////////////////////////////////////////////////////////
#ifdef DEV_INDEXED_WRITE
void WriteIndexedRegister(byte PageNum, byte RegOffset, byte RegValue) {
    WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);  // Internal page
    WriteByteTPI(TPI_INDEXED_OFFSET_REG, RegOffset);  // Indexed register
    WriteByteTPI(TPI_INDEXED_VALUE_REG, RegValue);    // Read value into buffer
}
#endif

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  ReadModifyWriteIndexedRegister(byte PageNum, byte Offset, byte Mask, byte Value)
// PURPOSE       :  Write "Value" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Mask"; Leave all other bits in "Offset"
//                  unchanged.
// INPUT PARAMS  :  Offset  :   TPI register offset
//                  Mask    :   "1" for each TPI register bit that needs to be
//                              modified
//					Value   :   The desired value for the register bits in their
//								proper positions
// OUTPUT PARAMS :  None
// GLOBALS USED  :  None
// RETURNS       :  void
//////////////////////////////////////////////////////////////////////////////
#ifdef DEV_INDEXED_RMW
void ReadModifyWriteIndexedRegister(byte PageNum, byte RegOffset, byte Mask, byte Value)
{
    byte Tmp;

    WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);
    WriteByteTPI(TPI_INDEXED_OFFSET_REG, RegOffset);
    Tmp = ReadByteTPI(TPI_INDEXED_VALUE_REG);

    Tmp &= ~Mask;
	Tmp |= (Value & Mask);

    WriteByteTPI(TPI_INDEXED_VALUE_REG, Tmp);
}
#endif


//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  InitVideo()
// PURPOSE       :  Set the 9022/4 to the video mode determined by GetVideoMode()
// INPUT PARAMS  :  Index of video mode to set; Flag that distinguishes between
//                  calling this function after power up and after input
//                  resolution change
// OUTPUT PARAMS :  None
// GLOBALS USED  :  VModesTable, VideoCommandImage
// RETURNS       :  TRUE
//////////////////////////////////////////////////////////////////////////////
bool InitVideo(byte Mode, byte TclkSel, byte _3D_Struct)
{

	byte V_Mode;
#ifdef DEEP_COLOR
	byte temp;
#endif
	byte B_Data[8];

#ifdef DEV_EMBEDDED
	byte EMB_Status;
#endif
#ifdef USE_DE_GENERATOR
	byte DE_Status;
#endif

#ifndef DEV_INDEXED_PLL
	byte Pattern;
#endif
	TPI_TRACE_PRINT((">>InitVideo()\n"));

	V_Mode = ConvertVIC_To_VM_Index(Mode, _3D_Struct & FOUR_LSBITS);	// convert 861-D VIC into VModesTable[] index.

#ifdef DEV_INDEXED_PLL
	SetPLL(TclkSel);
#else
	Pattern = (TclkSel << 6) & TWO_MSBITS;								// Use TPI 0x08[7:6] for 9022A/24A video clock multiplier
	ReadSetWriteTPI(TPI_PIX_REPETITION, Pattern);
#endif

	// Take values from VModesTable[]:

	B_Data[0] = VModesTable[V_Mode].PixClk & 0x00FF;			// write Pixel clock to TPI registers 0x00, 0x01
	B_Data[1] = (VModesTable[V_Mode].PixClk >> 8) & 0xFF;

	B_Data[2] = VModesTable[V_Mode].Tag.VFreq & 0x00FF;			// write Vertical Frequency to TPI registers 0x02, 0x03
	B_Data[3] = (VModesTable[V_Mode].Tag.VFreq >> 8) & 0xFF;

	B_Data[4] = VModesTable[V_Mode].Tag.Total.Pixels & 0x00FF;	// write total number of pixels to TPI registers 0x04, 0x05
	B_Data[5] = (VModesTable[V_Mode].Tag.Total.Pixels >> 8) & 0xFF;

	B_Data[6] = VModesTable[V_Mode].Tag.Total.Lines & 0x00FF;	// write total number of lines to TPI registers 0x06, 0x07
	B_Data[7] = (VModesTable[V_Mode].Tag.Total.Lines >> 8) & 0xFF;

	WriteBlockTPI(TPI_PIX_CLK_LSB, 8, B_Data);					// Write TPI Mode data.

	B_Data[0] = (VModesTable[V_Mode].PixRep) & LOW_BYTE;		// Set pixel replication field of 0x08
	B_Data[0] |= BIT_BUS_24;									// Set 24 bit bus

#ifndef DEV_INDEXED_PLL
	B_Data[0] |= (TclkSel << 6) & TWO_MSBITS;
#endif
#ifdef CLOCK_EDGE_FALLING
	B_Data[0] &= ~BIT_EDGE_RISE;								// Set to falling edge
#endif
#ifdef CLOCK_EDGE_RISING
	B_Data[0] |= BIT_EDGE_RISE;									// Set to rising edge
#endif
	WriteByteTPI(TPI_PIX_REPETITION, B_Data[0]);				// 0x08

#ifdef DEV_EMBEDDED
	EMB_Status = SetEmbeddedSync(Mode);
	EnableEmbeddedSync();
#endif

#ifdef USE_DE_GENERATOR
	ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);				// set 0x60[7] = 0 for External Sync
	DE_Status = SetDE(Mode);									// Call SetDE() with Video Mode as a parameter
	B_Data[0] = (((BITS_IN_YCBCR422 | BITS_IN_AUTO_RANGE) & ~BIT_EN_DITHER_10_8) & ~BIT_EXTENDED_MODE);	// 0x09
#else
	B_Data[0] = (((BITS_IN_RGB | BITS_IN_AUTO_RANGE) & ~BIT_EN_DITHER_10_8) & ~BIT_EXTENDED_MODE);		// 0x09
#endif

#ifdef DEEP_COLOR
	switch (video_information.color_depth) {
		case 0:  temp = 0x00; ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, 0x00); break;
		case 1:  temp = 0x80; ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2); break;
		case 2:  temp = 0xC0; ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2); break;
		case 3:  temp = 0x40; ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2); break;
		default: temp = 0x00; ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, 0x00); break;
		}
	B_Data[0] = ((B_Data[0] & 0x3F) | temp);
#endif

#ifdef DEV_EMBEDDED
	B_Data[0] = ((B_Data[0] & 0xFC) | 0x02);
#endif

	B_Data[1] = (BITS_OUT_RGB | BITS_OUT_AUTO_RANGE) & ~BIT_BT_709;
#ifdef DEEP_COLOR
	B_Data[1] = ((B_Data[1] & 0x3F) | temp);
#endif

#ifdef DEV_SUPPORT_EDID
	if (!IsHDMI_Sink()) {
		B_Data[1] = ((B_Data[1] & 0xFC) | 0x03);
		}
	else {
		// Set YCbCr color space depending on EDID
		if (EDID_Data.YCbCr_4_4_4) {
			B_Data[1] = ((B_Data[1] & 0xFC) | 0x01);
			}
		else {
			if (EDID_Data.YCbCr_4_2_2) {
				B_Data[1] = ((B_Data[1] & 0xFC) | 0x02);
				}
			}
		}
#else
	B_Data[1] = 0x00;
#endif

	SetFormat(B_Data);

	ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, BIT_2);	// Number HSync pulses from VSync active edge to Video Data Period should be 20 (VS_TO_VIDEO)

#ifdef SOURCE_TERMINATION_ON
	V_Mode = ReadIndexedRegister(INDEXED_PAGE_1, TMDS_CONT_REG);
	V_Mode = (V_Mode & 0x3F) | 0x25;
	WriteIndexedRegister(INDEXED_PAGE_1, TMDS_CONT_REG, V_Mode);
#endif

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   SetFormat(byte * Data)
// PURPOSE      :   
// INPUT PARAMS :   
// OUTPUT PARAMS:   
// GLOBALS USED :   
// RETURNS      :   
//////////////////////////////////////////////////////////////////////////////
void SetFormat(byte *Data)
{
	if (!IsHDMI_Sink())
	{
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_HDMI); // Set HDMI mode to allow color space conversion
	}

	WriteBlockTPI(TPI_INPUT_FORMAT_REG, 2, Data);   // Program TPI AVI Input and Output Format
	WriteByteTPI(TPI_END_RIGHT_BAR_MSB, 0x00);	    // Set last byte of TPI AVI InfoFrame for TPI AVI I/O Format to take effect

	if (!IsHDMI_Sink()) 
	{
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_DVI);
	}

#ifdef DEV_EMBEDDED
	EnableEmbeddedSync();							// Last byte of TPI AVI InfoFrame resets Embedded Sync Extraction
#endif
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   EnableEmbeddedSync
// PURPOSE      :
// INPUT PARAMS :
// OUTPUT PARAMS:
// GLOBALS USED :
// RETURNS      :
//////////////////////////////////////////////////////////////////////////////
void EnableEmbeddedSync( void )
{
    TPI_TRACE_PRINT((">>EnableEmbeddedSync()\n"));

	ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);	 // set 0x60[7] = 0 for DE mode
	WriteByteTPI(0x63, 0x30);
    ReadSetWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);       // set 0x60[7] = 1 for Embedded Sync
	ReadSetWriteTPI(TPI_DE_CTRL, BIT_6);
}


//////////////////////////////////////////////////////////////////////////////
// FUNCTION      : SetBasicAudio()
// PURPOSE       : Set the 9022/4 audio interface to basic audio.
// INPUT PARAMS  : None
// OUTPUT PARAMS : None
// GLOBALS USED  : None
// RETURNS       : void.
//////////////////////////////////////////////////////////////////////////////
void SetBasicAudio(void)
{

        TPI_TRACE_PRINT((">>SetBasicAudio()\n"));

#ifdef I2S_AUDIO
    WriteByteTPI(TPI_AUDIO_INTERFACE_REG,  AUD_IF_I2S);                             // 0x26
    WriteByteTPI(TPI_AUDIO_HANDLING, 0x08 | AUD_DO_NOT_CHECK);          // 0x25
#else
    WriteByteTPI(TPI_AUDIO_INTERFACE_REG, AUD_IF_SPDIF);                    // 0x26 = 0x40
    WriteByteTPI(TPI_AUDIO_HANDLING, AUD_PASS_BASIC);                   // 0x25 = 0x00
#endif

#ifndef F_9022A_9334
            SetChannelLayout(TWO_CHANNELS);             // Always 2 channesl in S/PDIF
#else
            ReadClearWriteTPI(TPI_AUDIO_INTERFACE_REG, BIT_5); // Use TPI 0x26[5] for 9022A/24A and 9334 channel layout
#endif

#ifdef I2S_AUDIO
        // I2S - Map channels - replace with call to API MAPI2S
        WriteByteTPI(TPI_I2S_EN, 0x80); // 0x1F
//        WriteByteTPI(TPI_I2S_EN, 0x91);
//        WriteByteTPI(TPI_I2S_EN, 0xA2);
//        WriteByteTPI(TPI_I2S_EN, 0xB3);

        // I2S - Stream Header Settings - replace with call to API SetI2S_StreamHeader
        WriteByteTPI(TPI_I2S_CHST_0, 0x00); // 0x21
        WriteByteTPI(TPI_I2S_CHST_1, 0x00);
        WriteByteTPI(TPI_I2S_CHST_2, 0x00);
        WriteByteTPI(TPI_I2S_CHST_3, 0x02);
        WriteByteTPI(TPI_I2S_CHST_4, 0x02);

        // I2S - Input Configuration - replace with call to API ConfigI2SInput
        WriteByteTPI(TPI_I2S_IN_CFG, (0x10 | SCK_SAMPLE_EDGE)); //0x20
#endif

    WriteByteTPI(TPI_AUDIO_SAMPLE_CTRL, TWO_CHANNELS);  // 0x27 = 0x01
    SetAudioInfoFrames(TWO_CHANNELS, 0x00, 0x00, 0x00, 0x00);
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  SetAVI_InfoFrames()
// PURPOSE       :  Load AVI InfoFrame data into registers and send to sink
// INPUT PARAMS  :  An API_Cmd parameter that holds the data to be sent
//                  in the InfoFrames
// OUTPUT PARAMS :  None
// GLOBALS USED  :  None
// RETURNS       :  TRUE
//////////////////////////////////////////////////////////////////////////////
bool SetAVI_InfoFrames(API_Cmd Command)
{

    byte B_Data[SIZE_AVI_INFOFRAME];
    byte VideoMode;                     // pointer to VModesTable[]
    byte i;
    byte TmpVal;

    TPI_TRACE_PRINT((">>SetAVI_InfoFrames()\n"));

    for (i = 0; i < SIZE_AVI_INFOFRAME; i++)
        B_Data[i] = 0;

    if (Command.CommandLength > VIDEO_SETUP_CMD_LEN)    // Command length > 9. AVI InfoFrame is set by the host
    {
        for (i = 1; i < Command.CommandLength - VIDEO_SETUP_CMD_LEN; i++)
            B_Data[i] = Command.Arg[VIDEO_SETUP_CMD_LEN + i - 1];
    }
    else                                                // Command length == 7. AVI InfoFrame is set by the FW
    {
        if ((Command.Arg[3] & TWO_LSBITS) == 1)         // AVI InfoFrame DByte1
            TmpVal = 2;
        else if ((Command.Arg[3] & TWO_LSBITS) == 2)
            TmpVal = 1;
        else
            TmpVal = 0;

            B_Data[1] = (TmpVal << 5) & BITS_OUT_FORMAT;                    // AVI Byte1: Y1Y0 (output format)

            if (((Command.Arg[6] >> 2) & TWO_LSBITS) == 3)                  // Extended colorimetry - xvYCC
            {
                B_Data[2] = 0xC0;                                           // Extended colorimetry info (B_Data[3] valid (CEA-861D, Table 11)

                if (((Command.Arg[6] >> 4) & THREE_LSBITS) == 0)            // xvYCC601
                    B_Data[3] &= ~BITS_6_5_4;

                else if (((Command.Arg[6] >> 4) & THREE_LSBITS) == 1)       // xvYCC709
                    B_Data[3] = (B_Data[3] & ~BITS_6_5_4) | BIT_4;
            }

            else if (((Command.Arg[6] >> 2) & TWO_LSBITS) == 2)             // BT.709
                B_Data[2] = 0x80;                                           // AVI Byte2: C1C0

            else if (((Command.Arg[6] >> 2) & TWO_LSBITS) == 1)             // BT.601
                B_Data[2] = 0x40;                                           // AVI Byte2: C1C0

			else															// Carries no data
			{																// AVI Byte2: C1C0
				B_Data[2] &= ~BITS_7_6;										// colorimetry = 0
				B_Data[3] &= ~BITS_6_5_4;									// Extended colorimetry = 0
			}

            VideoMode = ConvertVIC_To_VM_Index(vid_mode, Command.Arg[8] & LOW_NIBBLE);              /// //

			B_Data[4] = vid_mode;

#ifdef RX_ONBOARD
			if ((avi_information.byte_2 & PICTURE_ASPECT_RATIO_MASK) == PICTURE_ASPECT_RATIO_16x9)
			{
                B_Data[2] |= _16_To_9;                          // AVI Byte2: M1M0
				if (VModesTable[VideoMode].AspectRatio == _4or16 && AspectRatioTable[vid_mode-1] == _4)
				{
					vid_mode++;
					B_Data[4]++;
				}
			}
            else
            {
                B_Data[2] |= _4_To_3;                           // AVI Byte4: VIC
			}
#else
			B_Data[2] |= _4_To_3;                           // AVI Byte4: VIC
#endif

            B_Data[2] |= SAME_AS_AR;                                        // AVI Byte2: R3..R1
            B_Data[5] = VModesTable[VideoMode].PixRep;                      // AVI Byte5: Pixel Replication - PR3..PR0
    }

    B_Data[0] = 0x82 + 0x02 +0x0D;                                          // AVI InfoFrame ChecKsum

    for (i = 1; i < SIZE_AVI_INFOFRAME; i++)
        B_Data[0] += B_Data[i];

    B_Data[0] = 0x100 - B_Data[0];

    WriteBlockTPI(TPI_AVI_BYTE_0, SIZE_AVI_INFOFRAME, B_Data);
#ifdef DEV_EMBEDDED
    EnableEmbeddedSync();
#endif

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  SetAudioInfoFrames()
// PURPOSE       :  Load Audio InfoFrame data into registers and send to sink
// INPUT PARAMS  :  (1) Channel count (2) speaker configuration per CEA-861D
//                  Tables 19, 20 (3) Coding type: 0x09 for DSD Audio. 0 (refer
//                                      to stream header) for all the rest (4) Sample Frequency. Non
//                                      zero for HBR only (5) Audio Sample Length. Non zero for HBR
//                                      only.
// OUTPUT PARAMS :  None
// GLOBALS USED  :  None
// RETURNS       :  TRUE
//////////////////////////////////////////////////////////////////////////////
bool SetAudioInfoFrames(byte ChannelCount, byte CodingType, byte SS, byte Fs, byte SpeakerConfig)
{
    byte B_Data[SIZE_AUDIO_INFOFRAME];  // 14
    byte i;
#ifdef LATTICE_ORIGINAL
    byte TmpVal = 0;
#endif

    TPI_TRACE_PRINT((">>SetAudioInfoFrames()\n"));

#ifdef LATTICE_ORIGINAL
    for (i = 0; i < SIZE_AUDIO_INFOFRAME +1; i++)
#else
    for (i = 0; i < SIZE_AUDIO_INFOFRAME; i++)
#endif
        B_Data[i] = 0;

    B_Data[0] = EN_AUDIO_INFOFRAMES;        // 0xC2
    B_Data[1] = TYPE_AUDIO_INFOFRAMES;      // 0x84
    B_Data[2] = AUDIO_INFOFRAMES_VERSION;   // 0x01
    B_Data[3] = AUDIO_INFOFRAMES_LENGTH;    // 0x0A

    B_Data[5] = ChannelCount;               // 0 for "Refer to Stream Header" or for 2 Channels. 0x07 for 8 Channels
	B_Data[5] |= (CodingType << 4);                 // 0xC7[7:4] == 0b1001 for DSD Audio
    B_Data[4] = 0x84 + 0x01 + 0x0A;         // Calculate checksum

//    B_Data[6] = (Fs << 2) | SS;
	B_Data[6] = (Fs >> 1) | (SS >> 6);

	//write Fs to 0x27[5:3] and SS to 0x27[7:6] to update the IForm with the current value.
//	ReadModifyWriteTPI(TPI_AUDIO_SAMPLE_CTRL, BITS_7_6 | BITS_5_4_3, (B_Data[6] & BITS_1_0) << 6 | (B_Data[6] & 0x1C) << 1);

	B_Data[8] = SpeakerConfig;

    for (i = 5; i < SIZE_AUDIO_INFOFRAME; i++)
        B_Data[4] += B_Data[i];

    B_Data[4] = 0x100 - B_Data[4];
	g_audio_Checksum = B_Data[4];	// Audio checksum for global use

    WriteBlockTPI(TPI_AUDIO_BYTE_0, SIZE_AUDIO_INFOFRAME, B_Data);
	#ifdef DEV_EMBEDDED
    EnableEmbeddedSync();
	#endif
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   SetChannelLayout()
// PURPOSE      :   Set up the Channel layout field of internal register 0x2F
//                  (0x2F[1])
// INPUT PARAMS :   Number of audio channels: "0 for 2-Channels ."1" for 8.
// OUTPUT PARAMS:   None
// GLOBALS USED :   None
// RETURNS      :   TRUE 
//////////////////////////////////////////////////////////////////////////////
//#ifdef SetChannelLayout
bool SetChannelLayout(byte Count)
{
    // Indexed register 0x7A:0x2F[1]:
    WriteByteTPI(TPI_INTERNAL_PAGE_REG, 0x02); // Internal page 2
    WriteByteTPI(TPI_INDEXED_OFFSET_REG, 0x2F);

    Count &= THREE_LSBITS;

    if (Count == TWO_CHANNEL_LAYOUT)
    {
        // Clear 0x2F:
        ReadClearWriteTPI(TPI_INDEXED_VALUE_REG, BIT_1);
    }

    else if (Count == EIGHT_CHANNEL_LAYOUT)
    {
        // Set 0x2F[0]:
        ReadSetWriteTPI(TPI_INDEXED_VALUE_REG, BIT_1);
    }

    return TRUE;
}
//#endif

//////////////////////////////////////////////////////////////////////////////

#if 0
//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  SetAudioInfoFrames()
// PURPOSE       :  Load Audio InfoFrame data into registers and send to sink
// INPUT PARAMS  :  (1) Channel count (2) speaker configuration per CEA-861D
//                  Tables 19, 20 (3) Coding type: 0x09 for DSD Audio. 0 (refer
//                                      to stream header) for all the rest (4) Sample Frequency. Non
//                                      zero for HBR only (5) Audio Sample Length. Non zero for HBR
//                                      only.
// OUTPUT PARAMS :  None
// GLOBALS USED  :  None
// RETURNS       :  TRUE
//////////////////////////////////////////////////////////////////////////////
bool SetAudioInfoFrames(byte ChannelCount, byte CodingType, byte SS, byte Fs, byte SpeakerConfig)
{
    byte B_Data[SIZE_AUDIO_INFOFRAME];  // 14
    byte i;
    byte TmpVal = 0;

    TPI_TRACE_PRINT((">>SetAudioInfoFrames()\n"));

    for (i = 0; i < SIZE_AUDIO_INFOFRAME +1; i++)
        B_Data[i] = 0;

    B_Data[0] = EN_AUDIO_INFOFRAMES;        // 0xC2
    B_Data[1] = TYPE_AUDIO_INFOFRAMES;      // 0x84
    B_Data[2] = AUDIO_INFOFRAMES_VERSION;   // 0x01
    B_Data[3] = AUDIO_INFOFRAMES_LENGTH;    // 0x0A

    B_Data[5] = ChannelCount;               // 0 for "Refer to Stream Header" or for 2 Channels. 0x07 for 8 Channels
	B_Data[5] |= (CodingType << 4);                 // 0xC7[7:4] == 0b1001 for DSD Audio
    B_Data[4] = 0x84 + 0x01 + 0x0A;         // Calculate checksum

//    B_Data[6] = (Fs << 2) | SS;
	B_Data[6] = (Fs >> 1) | (SS >> 6);

	//write Fs to 0x27[5:3] and SS to 0x27[7:6] to update the IForm with the current value.
//	ReadModifyWriteTPI(TPI_AUDIO_SAMPLE_CTRL, BITS_7_6 | BITS_5_4_3, (B_Data[6] & BITS_1_0) << 6 | (B_Data[6] & 0x1C) << 1);

	B_Data[8] = SpeakerConfig;

    for (i = 5; i < SIZE_AUDIO_INFOFRAME; i++)
        B_Data[4] += B_Data[i];

    B_Data[4] = 0x100 - B_Data[4];
	g_audio_Checksum = B_Data[4];	// Audio checksum for global use

    WriteBlockTPI(TPI_AUDIO_BYTE_0, SIZE_AUDIO_INFOFRAME, B_Data);
	#ifdef DEV_EMBEDDED
    EnableEmbeddedSync();
	#endif
    return TRUE;
}
#endif


//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   HotPlugService()
// PURPOSE      :   Implement Hot Plug Service Loop activities
// INPUT PARAMS :   None
// OUTPUT PARAMS:   void
// GLOBALS USED :   LinkProtectionLevel
// RETURNS      :   An error code that indicates success or cause of failure
//////////////////////////////////////////////////////////////////////////////
void HotPlugService (void)
{
	TPI_TRACE_PRINT((">>HotPlugService()\n"));

	DisableInterrupts(0xFF);

#ifndef RX_ONBOARD
#ifdef SYS_BOARD_FPGA
	printf("** Only 480P supported: be sure input is 480P.");
	vid_mode = 2;												// Only 480P is supported on FPGA platform
#else
	vid_mode = EDID_Data.VideoDescriptor[0];					// use 1st mode supported by sink
#endif
#endif

#if 0
	// workaround for Bug#18128
	if (video_information.color_depth == AMF_COLOR_DEPTH_8BIT)
	{
		// Yes it is, so force 16bpps first!
		video_information.color_depth = AMF_COLOR_DEPTH_16BIT;
		InitVideo(vid_mode, X1, NO_3D_SUPPORT);
		// Now put it back to 8bit and go do the expected InitVideo() call
		video_information.color_depth = AMF_COLOR_DEPTH_8BIT;
	}
	// end workaround
#endif

	InitVideo(vid_mode, X1, NO_3D_SUPPORT);						// Set PLL Multiplier to x1 upon power up

#ifndef POWER_STATE_D0_AFTER_TMDS_ENABLE
	TxPowerState(TX_POWER_STATE_D0);
#endif

	if (IsHDMI_Sink())											// Set InfoFrames only if HDMI output mode
	{
		SetPreliminaryInfoFrames();
#if defined SiI9232_OR_SiI9236
#else
		SetBasicAudio();										// set audio interface to basic audio (an external command is needed to set to any other mode
#endif
	}
	else
	{
		SetAudioMute(AUDIO_MUTE_MUTED);
	}

#ifdef DEV_SUPPORT_EHDMI
//	EHDMI_ARC_Common_Enable();
	EHDMI_ARC_Common_With_HEC_Enable();
#endif

	if ((HDCP_TxSupports == TRUE) && (video_information.hdcp_authenticated == HDCP_AUTHENTICATED))
	{
#ifdef USE_BLACK_MODE
		SetInputColorSpace (INPUT_COLOR_SPACE_BLACK_MODE);
		TPI_DEBUG_PRINT (("TMDS -> Enabled\n"));
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, LINK_INTEGRITY_MODE_MASK | TMDS_OUTPUT_CONTROL_MASK, LINK_INTEGRITY_DYNAMIC | TMDS_OUTPUT_CONTROL_ACTIVE);
#else	// AV MUTE
		TPI_DEBUG_PRINT (("TMDS -> Enabled (Video Muted)\n"));
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, LINK_INTEGRITY_MODE_MASK | TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK, LINK_INTEGRITY_DYNAMIC | TMDS_OUTPUT_CONTROL_ACTIVE | AV_MUTE_MUTED);
#endif
		tmdsPoweredUp = TRUE;

		EnableInterrupts(HOT_PLUG_EVENT | RX_SENSE_EVENT | AUDIO_ERROR_EVENT | SECURITY_CHANGE_EVENT | V_READY_EVENT | HDCP_CHANGE_EVENT);
	}
	else
	{
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, LINK_INTEGRITY_MODE_MASK | TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK, LINK_INTEGRITY_DYNAMIC | TMDS_OUTPUT_CONTROL_ACTIVE | AV_MUTE_NORMAL);
		EnableInterrupts(HOT_PLUG_EVENT | RX_SENSE_EVENT | AUDIO_ERROR_EVENT | V_READY_EVENT);
	}

#ifdef POWER_STATE_D0_AFTER_TMDS_ENABLE
	TxPowerState(TX_POWER_STATE_D0);
#endif

}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      : HW_Reset()
// PURPOSE       : Send a
// INPUT PARAMS  : None
// OUTPUT PARAMS : void
// GLOBALS USED  : None
// RETURNS       : Void
//////////////////////////////////////////////////////////////////////////////
static void TxHW_Reset(void)
{
#ifndef LATTICE_ORIGINAL
	struct i2c_client *client = get_simgI2C_client(TPI);
	struct sii9136_platform_data *pdata =  dev_get_drvdata(&client->dev);
#endif
	pr_info("%s()\n", __func__);
	TPI_TRACE_PRINT((">>TxHW_Reset()\n"));

#ifdef LATTICE_ORIGINAL
	gpio_set_value(GPIO_HDMI_RESET_N, 1);
	mdelay(10);
	gpio_set_value(GPIO_HDMI_RESET_N, 0);
	mdelay(10);
	gpio_set_value(GPIO_HDMI_RESET_N, 1);
	mdelay(10);
#else
	gpio_set_value(pdata->gpio_hdmi_reset, 1);
	mdelay(10);
	gpio_set_value(pdata->gpio_hdmi_reset, 0);
	mdelay(10);
	gpio_set_value(pdata->gpio_hdmi_reset, 1);
	mdelay(10);

#endif

#if 0
	// Does this need to be done for every chip? Should it be moved into TXHAL_InitPostReset() for each applicable device?
	I2C_WriteByte(0x72, 0x7C, 0x14);					// HW debounce to 64ms (0x14)
#ifdef F_9136
	I2C_WriteByte(0x72, 0x82, 0xA5);
#endif
#endif
}


//////////////////////////////////////////////////////////////////////////////
// FUNCTION      : StartTPI()
// PURPOSE       : Start HW TPI mode by writing 0x00 to TPI address 0xC7. This
//                 will take the Tx out of power down mode.
// INPUT PARAMS  : None
// OUTPUT PARAMS : void
// GLOBALS USED  : None
// RETURNS       : TRUE if HW TPI started successfully. FALSE if failed to.
//////////////////////////////////////////////////////////////////////////////
static bool StartTPI(void)
{
	byte devID = 0x00;
    word wID = 0x0000;
	pr_info("%s()\n", __func__);

	WriteByteTPI(TPI_ENABLE, 0x00);            // Write "0" to 72:C7 to start HW TPI mode
	mdelay(100);

	devID = ReadIndexedRegister(INDEXED_PAGE_0, 0x03);
	wID = devID;
	wID <<= 8;
	devID = ReadIndexedRegister(INDEXED_PAGE_0, 0x02);
	wID |= devID;

	devID = ReadByteTPI(TPI_DEVICE_ID);

	pr_info("0x%04X\n", (int) wID);

	if (devID == SiI_DEVICE_ID) {
		pr_info (SiI_DEVICE_STRING);
		return TRUE;
	}

	pr_info("Unsupported TX\n");
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  EnableInterrupts()
// PURPOSE       :  Enable the interrupts specified in the input parameter
// INPUT PARAMS  :  A bit pattern with "1" for each interrupt that needs to be
//                  set in the Interrupt Enable Register (TPI offset 0x3C)
// OUTPUT PARAMS :  void
// GLOBALS USED  :  None
// RETURNS       :  TRUE
//////////////////////////////////////////////////////////////////////////////
bool EnableInterrupts(byte Interrupt_Pattern)
{
    TPI_TRACE_PRINT((">>EnableInterrupts()\n"));
    ReadSetWriteTPI(TPI_INTERRUPT_ENABLE_REG, Interrupt_Pattern);

#if defined SiI9232_OR_SiI9236
	WriteIndexedRegister(INDEXED_PAGE_0, 0x78, 0xDC);
#endif

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      :  DisableInterrupts()
// PURPOSE       :  Enable the interrupts specified in the input parameter
// INPUT PARAMS  :  A bit pattern with "1" for each interrupt that needs to be
//                  cleared in the Interrupt Enable Register (TPI offset 0x3C)
// OUTPUT PARAMS :  void
// GLOBALS USED  :  None
// RETURNS       :  TRUE
//////////////////////////////////////////////////////////////////////////////
static bool DisableInterrupts(byte Interrupt_Pattern)
{
	TPI_TRACE_PRINT((">>DisableInterrupts()\n"));
    ReadClearWriteTPI(TPI_INTERRUPT_ENABLE_REG, Interrupt_Pattern);

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION      : SetPreliminaryInfoFrames()
// PURPOSE       : Set InfoFrames for default (initial) setup only
// INPUT PARAMS  : VIC, output mode,
// OUTPUT PARAMS : void
// GLOBALS USED  : None
// RETURNS       : TRUE
//////////////////////////////////////////////////////////////////////////////
static bool SetPreliminaryInfoFrames()
{
    byte i;
    API_Cmd Command;        // to allow using function SetAVI_InfoFrames()

        TPI_TRACE_PRINT((">>SetPreliminaryInfoFrames()\n"));

    for (i = 0; i < MAX_COMMAND_ARGUMENTS; i++)
        Command.Arg[i] = 0;

        Command.CommandLength = 0;      // fixes SetAVI_InfoFrames() from going into an infinite loop

    Command.Arg[0] = vid_mode;

#ifdef DEV_SUPPORT_EDID
        if (EDID_Data.YCbCr_4_4_4)
        {
                Command.Arg[3] = 0x01;
        }
        else
        {
                if (EDID_Data.YCbCr_4_2_2)
                {
                        Command.Arg[3] = 0x02;
                }
        }
#else
        Command.Arg[3] = 0x00;
#endif

    SetAVI_InfoFrames(Command);
    return TRUE;
}

static void TxPowerState(byte powerState) {

	TPI_DEBUG_PRINT(("TX Power State D%d\n", (int)powerState));
	txPowerState = powerState;

#ifdef F_9136
	if (powerState == TX_POWER_STATE_D3) {
		ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_0, ENABLE);
		mdelay(10);
		ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_0, DISABLE);
		}
#endif

	ReadModifyWriteTPI(TPI_DEVICE_POWER_STATE_CTRL_REG, TX_POWER_STATE_MASK, powerState);
}

void EnableTMDS (void) {

	TPI_DEBUG_PRINT(("TMDS -> Enabled\n"));
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, TMDS_OUTPUT_CONTROL_MASK, TMDS_OUTPUT_CONTROL_ACTIVE);
	tmdsPoweredUp = TRUE;
}

void DisableTMDS (void) {

	pr_info("%s()\n", __func__);
	DEV_DBG("TMDS -> Disabled\n");
#ifdef USE_BLACK_MODE
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, TMDS_OUTPUT_CONTROL_MASK, TMDS_OUTPUT_CONTROL_POWER_DOWN);
#else	// AV MUTE
	//ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, TMDS_OUTPUT_CONTROL_MASK, TMDS_OUTPUT_CONTROL_POWER_DOWN);
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK, TMDS_OUTPUT_CONTROL_POWER_DOWN | AV_MUTE_MUTED);
#endif
	tmdsPoweredUp = FALSE;
}

#ifdef DEV_SUPPORT_HDCP
void RestartHDCP (void)
{
	TPI_DEBUG_PRINT (("HDCP -> Restart\n"));

	DisableTMDS();
	HDCP_Off();
	EnableTMDS();
}
#endif

void SetAudioMute (byte audioMute)
{
	ReadModifyWriteTPI(TPI_AUDIO_INTERFACE_REG, AUDIO_MUTE_MASK, audioMute);
}

//////////////////////////////////////////////////////////////////////////////
// FUNCTION     :   TPI_Poll ()
// PURPOSE      :   Poll Interrupt Status register for new interrupts
// INPUT PARAMS :   None
// OUTPUT PARAMS:   None
// GLOBALS USED :   LinkProtectionLevel
// RETURNS      :   None
//////////////////////////////////////////////////////////////////////////////
void TPI_Poll (void) {

	byte InterruptStatusImage;
	pr_info("%s()\n", __func__);

	if(txPowerState == TX_POWER_STATE_D0) {

		InterruptStatusImage = ReadByteTPI(TPI_INTERRUPT_STATUS_REG);

//		pr_info("%s() - 1. InterruptStatusImage: 0x%x\n", __func__, InterruptStatusImage);
		
		if (InterruptStatusImage & HOT_PLUG_EVENT) {

		DEV_DBG("%s() - HPD  -> \n", __func__);

		ReadSetWriteTPI(TPI_INTERRUPT_ENABLE_REG, HOT_PLUG_EVENT);  // Enable HPD interrupt bit

		// Repeat this loop while cable is bouncing:
		do {
				WriteByteTPI(TPI_INTERRUPT_STATUS_REG, HOT_PLUG_EVENT);
				mdelay(T_HPD_DELAY); // Delay for metastability protection and to help filter out connection bouncing
				InterruptStatusImage = ReadByteTPI(TPI_INTERRUPT_STATUS_REG);    // Read Interrupt status register
		} while (InterruptStatusImage & HOT_PLUG_EVENT);				// loop as long as HP interrupts recur

//		pr_info("%s() - 2. InterruptStatusImage: 0x%x\n", __func__, InterruptStatusImage);
		
		if (((InterruptStatusImage & HOT_PLUG_STATE) >> 2) != hdmiCableConnected) {

//			pr_info("%s() - 3. InterruptStatusImage: 0x%x\n", __func__, InterruptStatusImage);
			
			if (hdmiCableConnected == TRUE) {
				OnHdmiCableDisconnected();
				}

			else {
				OnHdmiCableConnected();
				ReadModifyWriteIndexedRegister(INDEXED_PAGE_0, 0x0A, 0x08, 0x08);
				}
		
			if (hdmiCableConnected == FALSE) {
				return;
				}

			}

		}
#ifdef HAS_CTRL_BUS
				if (InterruptStatusImage == NON_MASKABLE_INT) { 			// Check if NMI has occurred
					TPI_DEBUG_PRINT (("TP -> NMI Detected\n"));
					TPI_Init(); 											// Reset and re-initialize
					HotPlugService();
					}
#endif

#ifdef FORCE_MHD_OUTPUT
			if (mobileHDCableConnected == TRUE && hdmiCableConnected == TRUE && dsRxPoweredUp == FALSE) {
				OnDownstreamRxPoweredUp();
				}
#else
#if !defined SiI9232_OR_SiI9236
			// Check rx power
			if (((InterruptStatusImage & RX_SENSE_STATE) >> 3) != dsRxPoweredUp) {
	
				if (hdmiCableConnected == TRUE) {
					if (dsRxPoweredUp == TRUE) {
						OnDownstreamRxPoweredDown();
						}
	
					else {
						OnDownstreamRxPoweredUp();
						}
					}
	
				ClearInterrupt(RX_SENSE_EVENT);
				}
#endif
#endif

	// Check if Audio Error event has occurred:
	if (InterruptStatusImage & AUDIO_ERROR_EVENT) {
		// TPI_DEBUG_PRINT (("TP -> Audio Error Event\n"));
		// The hardware handles the event without need for host intervention (PR, p. 31)
		ClearInterrupt(AUDIO_ERROR_EVENT);
	}

#ifdef DEV_SUPPORT_HDCP
	if ((hdmiCableConnected == TRUE) && (dsRxPoweredUp == TRUE) && (HDCP_TxSupports == TRUE) && (video_information.hdcp_authenticated == HDCP_AUTHENTICATED)) {
		HDCP_CheckStatus(InterruptStatusImage);
	}
#endif
#if 0
#ifdef RX_ONBOARD
			if ((tmdsPoweredUp == TRUE) && (USRX_OutputChange == TRUE)) {
				TPI_TRACE_PRINT(("TP -> vid_mode...\n"));
				DisableTMDS();
	
				if ((HDCP_TxSupports == TRUE) && (video_information.hdcp_authenticated == HDCP_AUTHENTICATED)) {
					RestartHDCP();
					}
	
				HotPlugService();
// UART setting for 9136 SK
#if 0
				pvid_mode = vid_mode;
				USRX_OutputChange = FALSE;
#endif
				}
#endif
#endif
		SI_CecHandler(0 , 0);

#ifndef MHD_CABLE_HPD
			}
#endif

#ifdef F_9136
		else if (txPowerState == TX_POWER_STATE_D3) {
			if (PinTxInt == 0) {
				TPI_Init();
				}
			}
#endif
}



static void OnHdmiCableDisconnected(void) {

	pr_info("%s()\n", __func__);
	DEV_DBG("HDMI Disconnected\n");

	hdmiCableConnected = FALSE;

#ifdef DEV_SUPPORT_EDID
	edidDataValid = FALSE;
#endif

	OnDownstreamRxPoweredDown();

#ifdef F_9136
	TxPowerState(TX_POWER_STATE_D3);
#endif
}

static void OnHdmiCableConnected(void)
{
	pr_info("%s()\n", __func__);
	DEV_DBG("HDMI Connected\n");

#ifdef DEV_SUPPORT_MHD
	//Without this delay, the downstream BKSVs wil not be properly read, resulting in HDCP not being available.
	mdelay(500);
#else
	// No need to call TPI_Init here unless TX has been powered down on cable removal.
	//TPI_Init();
#endif

	hdmiCableConnected = TRUE;

#ifdef HDCP_DONT_CLEAR_BSTATUS
#else
	if ((HDCP_TxSupports == TRUE) && (video_information.hdcp_authenticated == HDCP_AUTHENTICATED)) {
		WriteIndexedRegister(INDEXED_PAGE_0, 0xCE, 0x00); // Clear BStatus
		WriteIndexedRegister(INDEXED_PAGE_0, 0xCF, 0x00);
		}
#endif

#ifdef DEV_SUPPORT_EDID
	DoEdidRead();
#else
	EDID_read();
#endif

#ifdef READKSV
	ReadModifyWriteTPI(0xBB, 0x08, 0x08);
#endif

	if (IsHDMI_Sink())              // select output mode (HDMI/DVI) according to sink capabilty
	{
		DEV_DBG("HDMI Sink Detected\n");
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_HDMI);
	}
	else
	{
		DEV_DBG("DVI Sink Detected\n");
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_DVI);
	}

#if defined SiI9232_OR_SiI9236
	OnDownstreamRxPoweredUp();		// RX power not determinable? Force to on for now.
#endif

}

static void OnDownstreamRxPoweredDown(void) {

	pr_info("%s()\n", __func__);
	DEV_DBG("DSRX -> Powered Down\n");

	dsRxPoweredUp = FALSE;

#ifdef DEV_SUPPORT_HDCP
	if ((HDCP_TxSupports == TRUE) && (video_information.hdcp_authenticated == HDCP_AUTHENTICATED)) {
		HDCP_Off();
		}
#endif

	DisableTMDS();
}

static void OnDownstreamRxPoweredUp(void) 
{

	TPI_DEBUG_PRINT (("DSRX -> Powered Up\n"));

	dsRxPoweredUp = TRUE;

	HotPlugService();
/*
	pvid_mode = vid_mode;
	USRX_OutputChange = FALSE;
*/
}

