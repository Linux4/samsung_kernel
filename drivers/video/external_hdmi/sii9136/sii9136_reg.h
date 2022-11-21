
#ifndef __HDMI_TX_REG_DEF_H_
#define __HDMI_TX_REG_DEF_H_

// Generic Masks
//==============
#define LOW_BYTE                0x00FF

#define LOW_NIBBLE              0x0F
#define HI_NIBBLE               0xF0

#define MSBIT                   0x80
#define LSBIT                   0x01

#define BIT_0                   0x01
#define BIT_1                   0x02
#define BIT_2                   0x04
#define BIT_3                   0x08
#define BIT_4                   0x10
#define BIT_5                   0x20
#define BIT_6                   0x40
#define BIT_7                   0x80

#define TWO_LSBITS              0x03
#define THREE_LSBITS            0x07
#define FOUR_LSBITS             0x0F
#define FIVE_LSBITS             0x1F
#define SEVEN_LSBITS            0x7F
#define TWO_MSBITS              0xC0
#define EIGHT_BITS              0xFF
#define BYTE_SIZE               0x08
#define BITS_1_0                0x03
#define BITS_2_1                0x06
#define BITS_2_1_0              0x07
#define BITS_6_5_4              0x70
#define BITS_7_6                0xC0

#define BIT0                    0x01
#define BIT1                    0x02
#define BIT2                    0x04
#define BIT3                    0x08
#define BIT4                    0x10
#define BIT5                    0x20
#define BIT6                    0x40
#define BIT7                    0x80

#define CEC_NO_TEXT         0
#define CEC_NO_NAMES        1
#define CEC_ALL_TEXT        2
#define INCLUDE_CEC_NAMES   CEC_NO_TEXT

#define CEC_SLAVE_ADDR 0xC0

#define SET_BITS    0xFF
#define CLEAR_BITS  0x00

// Interrupt Masks
//================
#define HOT_PLUG_EVENT          0x01
#define RX_SENSE_EVENT          0x02
#define HOT_PLUG_STATE          0x04
#define RX_SENSE_STATE          0x08

#define AUDIO_ERROR_EVENT       0x10
#define SECURITY_CHANGE_EVENT   0x20
#define V_READY_EVENT           0x40
#define HDCP_CHANGE_EVENT       0x80

#define NON_MASKABLE_INT		0xFF


#ifndef ALT_I2C_ADDR
	#define CONFIG_REG	(0x7A)
	#define TPI 		(0x72)
	#define CPI	 		(0xC0)
#else
	#define CONFIG_REG	(0x7E)
	#define TPI 		(0x76)
	#define CPI	 		(0xC4)	
#endif

// Audio Modes
//============
#define AUD_PASS_BASIC      0x00
#define AUD_PASS_ALL        0x01
#define AUD_DOWN_SAMPLE     0x02
#define AUD_DO_NOT_CHECK    0x03

#define REFER_TO_STREAM_HDR     0x00
#define TWO_CHANNELS            0x00
#define EIGHT_CHANNELS          0x01
#define AUD_IF_SPDIF            0x40
#define AUD_IF_I2S              0x80
#define AUD_IF_DSD				0xC0
#define AUD_IF_HBR				0x04

#define TWO_CHANNEL_LAYOUT      0x00
#define EIGHT_CHANNEL_LAYOUT    0x20

// I2C Slave Addresses
//====================
#define TPI_BASE_ADDR       0x72
#define TX_SLAVE_ADDR       0x72
#define CBUS_SLAVE_ADDR     0xC8
#define HDCP_SLAVE_ADDR     0x74
#define EDID_ROM_ADDR       0xA0
#define EDID_SEG_ADDR	    0x60

// InfoFrames
//===========
#define SIZE_AVI_INFOFRAME      0x0E     // including checksum byte
#define BITS_OUT_FORMAT         0x60    // Y1Y0 field

#define _4_To_3                 0x10    // Aspect ratio - 4:3  in InfoFrame DByte 1
#define _16_To_9                0x20    // Aspect ratio - 16:9 in InfoFrame DByte 1
#define SAME_AS_AR              0x08    // R3R2R1R0 - in AVI InfoFrame DByte 2

#define BT_601                  0x40
#define BT_709                  0x80

#define SIZE_AUDIO_INFOFRAME    0x0F

#define EN_AUDIO_INFOFRAMES         0xC2
#define TYPE_AUDIO_INFOFRAMES       0x84
#define AUDIO_INFOFRAMES_VERSION    0x01
#define AUDIO_INFOFRAMES_LENGTH     0x0A


// FPLL Multipliers:
//==================

#define X1                      0x01

// 3D Constants
//=============

#define _3D_STRUC_PRESENT				0x02

// 3D_Stucture Constants
//=======================
#define FRAME_PACKING					0x00
#define FIELD_ALTERNATIVE				0x01
#define LINE_ALTERNATIVE				0x02
#define SIDE_BY_SIDE_FULL				0x03
#define L_PLUS_DEPTH					0x04
#define L_PLUS_DEPTH_PLUS_GRAPHICS		0x05
#define SIDE_BY_SIDE_HALF				0x08

// 3D_Ext_Data Constants
//======================
#define HORIZ_ODD_LEFT_ODD_RIGHT		0x00
#define HORIZ_ODD_LEFT_EVEN_RIGHT		0x01
#define HORIZ_EVEN_LEFT_ODD_RIGHT  		0x02
#define HORIZ_EVEN_LEFT_EVEN_RIGHT		0x03

#define QUINCUNX_ODD_LEFT_EVEN_RIGHT	0x04
#define QUINCUNX_ODD_LEFT_ODD_RIGHT		0x05
#define QUINCUNX_EVEN_LEFT_ODD_RIGHT	0x06
#define QUINCUNX_EVEN_LEFT_EVEN_RIGHT	0x07

#define NO_3D_SUPPORT					0x0F


//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
#define AMF_COLOR_DEPTH_8BIT    0
#define AMF_COLOR_DEPTH_10BIT   1
#define AMF_COLOR_DEPTH_12BIT   2
#define AMF_COLOR_DEPTH_16BIT   3

#define PICTURE_ASPECT_RATIO_MASK	0x30
#define	PICTURE_ASPECT_RATIO_4x3	0x10
#define PICTURE_ASPECT_RATIO_16x9	0x20

#define HDCP_NOT_AUTHENTICATED		0x00
#define HDCP_AUTHENTICATED			0x01

// TPI Video Mode Data
//====================

#define TPI_PIX_CLK_LSB						(0x00)
#define TPI_PIX_CLK_MSB						(0x01)

#define TPI_VERT_FREQ_LSB					(0x02)
#define TPI_VERT_FREQ_MSB					(0x03)

#define TPI_TOTAL_PIX_LSB					(0x04)
#define TPI_TOTAL_PIX_MSB					(0x05)

#define TPI_TOTAL_LINES_LSB					(0x06)
#define TPI_TOTAL_LINES_MSB					(0x07)

// Pixel Repetition Data
//======================

#define TPI_PIX_REPETITION					(0x08)

// TPI AVI Input and Output Format Data
//=====================================

/// AVI Input Format Data ================================================= ///

#define TPI_INPUT_FORMAT_REG				(0x09)


// TPI AVI InfoFrame Data
//======================= 

#define TPI_AVI_BYTE_0						(0x0C)
#define TPI_AVI_BYTE_1						(0x0D)
#define TPI_AVI_BYTE_2						(0x0E)
#define TPI_AVI_BYTE_3						(0x0F)
#define TPI_AVI_BYTE_4						(0x10)
#define TPI_AVI_BYTE_5						(0x11)

#define TPI_AUDIO_BYTE_0					(0xBF)

#define TPI_INFO_FRM_DBYTE5					0xC8
#define TPI_INFO_FRM_DBYTE6					0xC9

#define TPI_END_TOP_BAR_LSB					(0x12)
#define TPI_END_TOP_BAR_MSB					(0x13)

#define TPI_START_BTM_BAR_LSB				(0x14)
#define TPI_START_BTM_BAR_MSB				(0x15)

#define TPI_END_LEFT_BAR_LSB				(0x16)
#define TPI_END_LEFT_BAR_MSB				(0x17)

#define TPI_END_RIGHT_BAR_LSB				(0x18)
#define TPI_END_RIGHT_BAR_MSB				(0x19)


// ===================================================== //

#define TPI_SYSTEM_CONTROL_DATA_REG			(0x1A)

#define LINK_INTEGRITY_MODE_MASK			(BIT_6)
#define LINK_INTEGRITY_STATIC				(0x00)
#define LINK_INTEGRITY_DYNAMIC				(0x40)

#define TMDS_OUTPUT_CONTROL_MASK			(BIT_4)
#define TMDS_OUTPUT_CONTROL_ACTIVE			(0x00)
#define TMDS_OUTPUT_CONTROL_POWER_DOWN		(0x10)

#define AV_MUTE_MASK						(BIT_3)
#define AV_MUTE_NORMAL						(0x00)
#define AV_MUTE_MUTED						(0x08)

#define DDC_BUS_REQUEST_MASK				(BIT_2)
#define DDC_BUS_REQUEST_NOT_USING			(0x00)
#define DDC_BUS_REQUEST_REQUESTED			(0x04)

#define DDC_BUS_GRANT_MASK					(BIT_1)
#define DDC_BUS_GRANT_NOT_AVAILABLE			(0x00)
#define DDC_BUS_GRANT_GRANTED				(0x02)

#define OUTPUT_MODE_MASK					(BIT_0)
#define OUTPUT_MODE_DVI						(0x00)
#define OUTPUT_MODE_HDMI					(0x01)


// TPI Identification Registers
//=============================

#define TPI_DEVICE_ID						(0x1B)
#define TPI_DEVICE_REV_ID					(0x1C)

#define TPI_RESERVED2						(0x1D)

// Configuration of I2S Interface
//===============================

#define TPI_I2S_EN							(0x1F)
#define TPI_I2S_IN_CFG						(0x20)

// Available only when TPI 0x26[7:6]=10 to select I2S input
//=========================================================
#define TPI_I2S_CHST_0						(0x21)
#define TPI_I2S_CHST_1						(0x22)
#define TPI_I2S_CHST_2						(0x23)
#define TPI_I2S_CHST_3						(0x24)
#define TPI_I2S_CHST_4						(0x25)

// Available only when 0x26[7:6]=01
//=================================
#define TPI_SPDIF_HEADER					(0x24)
#define TPI_AUDIO_HANDLING					(0x25)

// Audio Configuration Regiaters
//==============================
#define TPI_AUDIO_INTERFACE_REG				(0x26)

// Finish this...

#define AUDIO_MUTE_MASK						(BIT_4)
#define AUDIO_MUTE_NORMAL					(0x00)
#define AUDIO_MUTE_MUTED					(0x10)

#define TPI_AUDIO_SAMPLE_CTRL				(0x27)


/// HDCP Query Data Register ============================================== ///

#define TPI_HDCP_QUERY_DATA_REG				(0x29)

#define EXTENDED_LINK_PROTECTION_MASK		(BIT_7)
#define EXTENDED_LINK_PROTECTION_NONE		(0x00)
#define EXTENDED_LINK_PROTECTION_SECURE		(0x80)

#define LOCAL_LINK_PROTECTION_MASK			(BIT_6)
#define LOCAL_LINK_PROTECTION_NONE			(0x00)
#define LOCAL_LINK_PROTECTION_SECURE		(0x40)

#define LINK_STATUS_MASK					(BIT_5 | BIT_4)
#define LINK_STATUS_NORMAL					(0x00)
#define LINK_STATUS_LINK_LOST				(0x10)
#define LINK_STATUS_RENEGOTIATION_REQ		(0x20)
#define LINK_STATUS_LINK_SUSPENDED			(0x30)

#define HDCP_REPEATER_MASK					(BIT_3)
#define HDCP_REPEATER_NO					(0x00)
#define HDCP_REPEATER_YES					(0x08)

#define CONNECTOR_TYPE_MASK					(BIT_2 | BIT_0)
#define CONNECTOR_TYPE_DVI					(0x00)
#define CONNECTOR_TYPE_RSVD					(0x01)
#define CONNECTOR_TYPE_HDMI					(0x04)
#define CONNECTOR_TYPE_FUTURE				(0x05)

#define PROTECTION_TYPE_MASK				(BIT_1)
#define PROTECTION_TYPE_NONE				(0x00)
#define PROTECTION_TYPE_HDCP				(0x02)

/// HDCP Control Data Register ============================================ ///

#define TPI_HDCP_CONTROL_DATA_REG			(0x2A)

#define PROTECTION_LEVEL_MASK				(BIT_0)
#define PROTECTION_LEVEL_MIN				(0x00)
#define PROTECTION_LEVEL_MAX				(0x01)

/// HDCP Revision Data Register =========================================== ///

#define TPI_HDCP_REVISION_DATA_REG			(0x30)

#define HDCP_MAJOR_REVISION_MASK			(BIT_7 | BIT_6 | BIT_5 | BIT_4)
#define HDCP_MAJOR_REVISION_VALUE			(0x10)

#define HDCP_MINOR_REVISION_MASK			(BIT_3 | BIT_2 | BIT_1 | BIT_0)
#define HDCP_MINOR_REVISION_VALUE			(0x02)

/// HDCP AKSV Registers =================================================== ///

#define TPI_AKSV_1_REG						(0x36)
#define TPI_AKSV_2_REG						(0x37)
#define TPI_AKSV_3_REG						(0x38)
#define TPI_AKSV_4_REG						(0x39)
#define TPI_AKSV_5_REG						(0x3A)

#define TPI_DEEP_COLOR_GCP					(0x40)

/// Interrupt Enable Register ============================================= ///

#define TPI_INTERRUPT_ENABLE_REG			(0x3C)

/// Interrupt Status Register ============================================= ///

#define TPI_INTERRUPT_STATUS_REG			(0x3D)


/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ///

/// KSV FIFO First Status Register ============================================= ///

#define TPI_KSV_FIFO_FIRST_STATUS_REG		(0x3E)

#define KSV_FIFO_FIRST_MASK					(BIT_1)
#define KSV_FIFO_FIRST_NO					(0x00)
#define KSV_FIFO_FIRST_YES					(0x02)

/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ///


/// KSV FIFO Last Status Register ============================================= ///

#define TPI_KSV_FIFO_LAST_STATUS_REG		(0x41)

#define KSV_FIFO_LAST_MASK					(BIT_7)
#define KSV_FIFO_LAST_NO					(0x00)
#define KSV_FIFO_LAST_YES					(0x80)

#define KSV_FIFO_BYTES_MASK					(BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0)

/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ///


// Sync Register Configuration and Sync Monitoring Registers
//==========================================================

#define TPI_SYNC_GEN_CTRL					(0x60)
#define TPI_SYNC_POLAR_DETECT				(0x61)

// Explicit Sync DE Generator Registers (TPI 0x60[7]=0)
//=====================================================

#define TPI_DE_DLY							(0x62)
#define TPI_DE_CTRL							(0x63)
#define TPI_DE_TOP							(0x64)

#define TPI_RESERVED4						(0x65)

#define TPI_DE_CNT_7_0						(0x66)
#define TPI_DE_CNT_11_8						(0x67)

#define TPI_DE_LIN_7_0						(0x68)
#define TPI_DE_LIN_10_8						(0x69)

#define TPI_DE_H_RES_7_0					(0x6A)
#define TPI_DE_H_RES_10_8					(0x6B)

#define TPI_DE_V_RES_7_0					(0x6C)
#define TPI_DE_V_RES_10_8					(0x6D)


//------------------------------------------------------------------------------
// Registers in Page 8
//------------------------------------------------------------------------------
#define REG_CEC_DEBUG_2             0x886

#define	REG_CEC_DEBUG_3             0x887
#define BIT_SNOOP_EN                        0x01
#define BIT_FLUSH_TX_FIFO                   0x80

#define REG_CEC_TX_INIT             0x888
#define BIT_SEND_POLL                       0x80

#define REG_CEC_TX_DEST             0x889

#define REG_CEC_CONFIG_CPI          0x88E

#define REG_CEC_TX_COMMAND          0x88F
#define REG_CEC_TX_OPERAND_0        0x890

#define REG_CEC_TRANSMIT_DATA       0x89F

#define	BIT_TX_BFR_ACCESS                   0x40
#define	BIT_TX_AUTO_CALC                    0x20
#define	BIT_TRANSMIT_CMD                    0x10

#define REG_CEC_CAPTURE_ID0         0x8A2
#define REG_CEC_CAPTURE_ID1         0x8A3

// 0xA6 CPI Interrupt Status Register (R/W)
#define REG_CEC_INT_STATUS_0        0x8A6
#define BIT_CEC_LINE_STATE                  0x80
#define BIT_TX_MESSAGE_SENT                 0x20
#define BIT_TX_HOTPLUG                      0x10
#define BIT_POWER_STAT_CHANGE               0x08
#define BIT_TX_FIFO_EMPTY                   0x04
#define BIT_RX_MSG_RECEIVED                 0x02
#define BIT_CMD_RECEIVED                    0x01

// 0xA7 CPI Interrupt Status Register (R/W)
#define REG_CEC_INT_STATUS_1        0x8A7
#define BIT_RX_FIFO_OVERRUN                 0x08
#define BIT_SHORT_PULSE_DET                 0x04
#define BIT_FRAME_RETRANSM_OV               0x02
#define BIT_START_IRREGULAR                 0x01

#define REG_CEC_RX_CONTROL          0x8AC
// CEC  CEC_RX_CONTROL bits
#define BIT_CLR_RX_FIFO_CUR         0x01
#define BIT_CLR_RX_FIFO_ALL         0x02

#define REG_CEC_RX_COUNT            0x8AD
#define BIT_MSG_ERROR               0x80

#define REG_CEC_RX_CMD_HEADER       0x8AE
#define REG_CEC_RX_OPCODE           0x8AF
#define REG_CEC_RX_OPERAND_0        0x8B0

//#define CEC_OP_ABORT_31             0x8DF
#define CEC_OP_ABORT_31             0xDF


// TPI Enable Register
//====================

#define TPI_ENABLE							(0xC7)


// Indexed Register Offsets, Constants
//====================================
#define INDEXED_PAGE_0			0x01
#define INDEXED_PAGE_1			0x02
#define INDEXED_PAGE_2			0x03


// Generic Constants
//==================
#define FALSE                   0
#define TRUE                    1

#define OFF                     0
#define ON                      1

#define LOW                     0
#define HIGH                    1

// ===================================================== //

#define TPI_DEVICE_POWER_STATE_CTRL_REG		(0x1E)

#define TX_POWER_STATE_MASK					(BIT_1 | BIT_0)
#define TX_POWER_STATE_D0					(0x00)
#define TX_POWER_STATE_D1					(0x01)
#define TX_POWER_STATE_D2					(0x02)
#define TX_POWER_STATE_D3					(0x03)


// Pixel Repetition Masks
//=======================
#define BIT_BUS_24          0x20
#define BIT_EDGE_RISE       0x10

//Audio Maps
//==========
#define BIT_AUDIO_MUTE      0x10

// Input/Output Format Masks
//==========================
#define BITS_IN_RGB         0x00
#define BITS_IN_YCBCR444    0x01
#define BITS_IN_YCBCR422    0x02

#define BITS_IN_AUTO_RANGE  0x00
#define BITS_IN_FULL_RANGE  0x04
#define BITS_IN_LTD_RANGE   0x08

#define BIT_EN_DITHER_10_8  0x40
#define BIT_EXTENDED_MODE   0x80

#define BITS_OUT_RGB        0x00
#define BITS_OUT_YCBCR444   0x01
#define BITS_OUT_YCBCR422   0x02

#define BITS_OUT_AUTO_RANGE 0x00
#define BITS_OUT_FULL_RANGE 0x04
#define BITS_OUT_LTD_RANGE  0x08

#define BIT_BT_709          0x10



#define VIDEO_SETUP_CMD_LEN     	(9)

#endif
