#pragma once

/////////////////////////////////////////////////////////////////////////////////////////
// GMvCommon.h
//---------------------------------------------------------------------------
// Created by Byeongjae Gim
// Email: gaiama78@gmail.com, byeongjae.kim@samsung.com
/////////////////////////////////////////////////////////////////////////////////////////

// ! define
#if defined( DG_PRODUCT_K )
	#define DG_MV_WIDTH 16
	#define DG_MV_HEIGHT 29
	#define DG_MV_SSC_NUM 6
	#define DG_MV_RESOLUTION_BYTE 2
	#define DG_MV_SAMPLING_RATE_HZ 5

	//#define DG_MV_PKT_PARITY_CRC16

	#if defined( DG_TSP_IC_SYNAPTICS )
	#elif defined( DG_TSP_IC_STM )
	#endif
#elif defined( DG_PRODUCT_H )
	#define DG_MV_WIDTH 18
	#define DG_MV_HEIGHT 32
	#define DG_MV_SSC_NUM 0
	#define DG_MV_RESOLUTION_BYTE 2
	#define DG_MV_SAMPLING_RATE_HZ 5

	#define DG_MV_SC_STAGE_NUM 2
	#define DG_MV_SC_FINAL_MIN -20
	#define DG_MV_SC_LEVEL_MAX 500
	#define DG_MV_SC_LEVEL_PEAK_MIN 10

	#define DG_MV_MC_STAGE_NUM 2
	#define DG_MV_MC_FINAL_MIN -20//TBD

	#define DG_MV_CODEC_1

	#define DG_MV_PKT_PARITY_CRC16
	//#define DG_MV_PKT_PARITY_CRC32
	//#define DG_MV_PKT_PARITY_RS_10_6
#elif defined( DG_PRODUCT_J )
	#define DG_MV_WIDTH 16
	#define DG_MV_HEIGHT 28
	#define DG_MV_SSC_NUM 0
	#define DG_MV_SAMPLING_RATE_HZ 5
#endif

// Multiverse Packet
#define DG_MV_PKT_SYNC_BYTE 0xFF
#if defined( DG_MV_PKT_PARITY_CRC16 )
	#define DG_MV_PKT_PARITY_SIZE_HEADER 2
#elif defined( DG_MV_PKT_PARITY_CRC32 )
	#define DG_MV_PKT_PARITY_SIZE_HEADER 4
#elif defined( DG_MV_PKT_PARITY_RS_10_6 )
	#define DG_MV_PKT_PARITY_SIZE_HEADER 4
#else
	#define DG_MV_PKT_PARITY_SIZE_HEADER 0
#endif
#define DG_MV_PKT_MAX_SIZE ((sizeof( struct SGMvPktHdr ) + (DG_MV_WIDTH * DG_MV_HEIGHT * DG_MV_RESOLUTION_BYTE)) << 1) // based on MC with ECC

// I2C
#define DG_MV_I2C_REG_PKT_HEADER_RX 0x6000
#define DG_MV_I2C_REG_PKT_HEADER_TX 0x6001
#define DG_MV_I2C_REG_PKT_PAYLOAD_RX 0x6002
#define DG_MV_I2C_REG_PKT_PAYLOAD_TX 0x6003
#define DG_MV_I2C_ACK_RETRY_COUNT 5
#define DG_MV_I2C_ACK_WAIT_MS 1
#define DG_MV_I2C_ACK_WAIT_US 1000
#define DG_MV_I2C_ACK_WAIT_COUNT 250 // 250ms

// Brane
#define DG_MV_BRANE_PATH _T("/sys/class/sec/Multiverse/Brane")
#define DG_MV_BRANE_BUFFER_MMAP_ENABLE 1
#define DG_MV_BRANE_BUFFER_MMAP_INTERCEPT 1
#define DG_MV_BRANE_SIGACTION_ENABLE 0
#define DG_MV_BRANE_SIGACTION_ID 50 // 34~64(realtime signal), realtime signal can send 32bit payload in siginfo.si_int. Sure?

//
#define DG_MV_SC_QUANT_RATIO DG_MV_SC_LEVEL_PEAK_MIN // ~50, under 6bit
#define DG_MV_SC_QUANT_MAX 63 // 6bit(63)

//
#define DG_MV_INITIAL_SKIP_COUNT 5
#define DG_MV_BT_WINDOW_LEN (DG_MV_SAMPLING_RATE_HZ >> 1)
#define DG_MV_BT_VARIANCE_MAX 7

// ! enum
typedef enum// : sint32
{
	EG_MV_SERVICE_SC_F = 0,
	EG_MV_SERVICE_SC_0,
	EG_MV_SERVICE_SC_1,
	EG_MV_SERVICE_MC_F,
	EG_MV_SERVICE_MC_0,
	EG_MV_SERVICE_MC_1,
	EG_MV_SERVICE_SSC_F,
	EG_MV_SERVICE_SSC_0,
	EG_MV_SERVICE_SSC_1,
	EG_MV_SERVICE_GRIP_TO_SNOOZE,
	EG_MV_SERVICE_GRIP_FOR_QUICK_CAMERA,
	EG_MV_SERVICE_NUM
} EG_MV_SERVICE;

typedef enum// : sint32
{
	EG_MV_ID_NONE = -1,
	EG_MV_ID_UNREGISTER_ALL,

	EG_MV_ID_SC_F_REGISTER = 100,
	EG_MV_ID_SC_F_UNREGISTER,
	EG_MV_ID_SC_F_DATA,

	EG_MV_ID_SC_0_REGISTER = 110,
	EG_MV_ID_SC_0_UNREGISTER,
	EG_MV_ID_SC_0_DATA,

	EG_MV_ID_SC_1_REGISTER = 120,
	EG_MV_ID_SC_1_UNREGISTER,
	EG_MV_ID_SC_1_DATA,

	EG_MV_ID_MC_F_REGISTER = 130,
	EG_MV_ID_MC_F_UNREGISTER,
	EG_MV_ID_MC_F_DATA,

	EG_MV_ID_MC_0_REGISTER = 140,
	EG_MV_ID_MC_0_UNREGISTER,
	EG_MV_ID_MC_0_DATA,

	EG_MV_ID_MC_1_REGISTER = 150,
	EG_MV_ID_MC_1_UNREGISTER,
	EG_MV_ID_MC_1_DATA,

	EG_MV_ID_SSC_F_REGISTER = 160,
	EG_MV_ID_SSC_F_UNREGISTER,
	EG_MV_ID_SSC_F_DATA,

	EG_MV_ID_SSC_0_REGISTER = 170,
	EG_MV_ID_SSC_0_UNREGISTER,
	EG_MV_ID_SSC_0_DATA,

	EG_MV_ID_SSC_1_REGISTER = 180,
	EG_MV_ID_SSC_1_UNREGISTER,
	EG_MV_ID_SSC_1_DATA,

	EG_MV_ID_GRIP_TO_SNOOZE_REGISTER = 190,
	EG_MV_ID_GRIP_TO_SNOOZE_UNREGISTER,
	EG_MV_ID_GRIP_TO_SNOOZE_DATA,

	EG_MV_ID_GRIP_FOR_QUICK_CAMERA_REGISTER = 200,
	EG_MV_ID_GRIP_FOR_QUICK_CAMERA_UNREGISTER,
	EG_MV_ID_GRIP_FOR_QUICK_CAMERA_DATA,
} EG_MV_ID;

typedef enum// : sint32
{
	EG_MV_DATA_TYPE_S8 = 0,
	EG_MV_DATA_TYPE_U8,
	EG_MV_DATA_TYPE_U8_NORM,
	EG_MV_DATA_TYPE_S16,
	EG_MV_DATA_TYPE_U16,
	EG_MV_DATA_TYPE_U16_NORM,
	EG_MV_DATA_TYPE_F32,
	EG_MV_DATA_TYPE_NUM
} EG_MV_DATA_TYPE;

typedef enum// : uint8
{
	EG_MV_PKT_ID_BRANE = 0,
	EG_MV_PKT_ID_ACK,
	EG_MV_PKT_ID_START,
	EG_MV_PKT_ID_STOP,
	EG_MV_PKT_ID_DATA,
	EG_MV_PKT_ID_NUM
} EG_MV_PKT_ID;

typedef enum// : uint4
{
	EG_MV_PKT_ARG1_BRANE_MMAP = 0,
	EG_MV_PKT_ARG1_BRANE_UNMMAP
} EG_MV_PKT_ARG1_BRANE;

typedef enum// : uint4
{
	EG_MV_PKT_ARG1_ACK_WAIT = 0,
	EG_MV_PKT_ARG1_ACK_ACK,
	EG_MV_PKT_ARG1_ACK_NACK,
	EG_MV_PKT_ARG1_ACK_RESET
} EG_MV_PKT_ARG1_ACK;

// ! struct
#pragma pack( 1 )
typedef struct SGMvPktHdr // uint8[6]
{
	uint8 u8SyncByte, u8PktId;
	union
	{
		struct
		{
			uint8 u8Arg1;
			uint8 u8Arg2;
		};
		struct
		{
			uint8 u8SubId;
			unsigned u5Reserved : 5;
			unsigned u3ParityType : 3;
		};
	};
	uint16 u16PldSize;
} TGMvPktHdr;

typedef struct SGMvBraneBufInfo
{
	uint16 u16TotalSize, u16WPos, u16RPos, u16RSize;
} TGMvBraneBufInfo;

typedef struct SGMvScImpulse
{
	unsigned u1Virtual : 1;
	unsigned u1HalfPixel : 1;
	unsigned u6X : 6; // ~63
	unsigned u6Level : 6; // ~63
	unsigned u2Reserved : 2;
} TGMvScImpulse;
#pragma pack()
