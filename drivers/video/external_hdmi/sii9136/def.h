
#ifndef __DEF_H_
#define __DEF_H_

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// SiI9136
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// #define F_9136

#define SiI_DEVICE_ID           0xB4
#define SiI_DEVICE_STRING       "SiI 9136\n"

#define SiI9136_DRV_POLLING

#define SCK_SAMPLE_EDGE			0x80
#define CLOCK_EDGE_FALLING

// #define RX_ONBOARD
#define DEEP_COLOR

#define I2S_AUDIO

// #define DEV_SUPPORT_EDID
//#define DEV_SUPPORT_HDCP
#define DEV_SUPPORT_CEC_FEATURE_ABORT
#define DEV_SUPPORT_CEC_CONFIG_CPI_0
//#define DEV_SUPPORT_EHDMI

#define DEV_INDEXED_READ
#define DEV_INDEXED_WRITE
#define DEV_INDEXED_RMW

#define HDCP_DONT_CLEAR_BSTATUS

// typedef unsigned char		bool;
typedef unsigned char		byte;
typedef unsigned short		word;
typedef unsigned long		dword;

#define MAX_COMMAND_ARGUMENTS		24

// API Interface Data Structures
//==============================
typedef struct {
	byte	OpCode;
	byte	CommandLength;
	byte	Arg[MAX_COMMAND_ARGUMENTS];
	byte 	CheckSum;
} API_Cmd;

//byte vid_mode;


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Debug Definitions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define DISABLE 0x00
#define ENABLE  0xFF

// Compile debug prints inline or not
#define CONF__TPI_DEBUG_PRINT   (ENABLE)
#define CONF__TPI_EDID_PRINT    (ENABLE)
#define CONF__TPI_TRACE_PRINT	(ENABLE)
#define CONF__TPI_CDC_PRINT	(ENABLE)
#define CONF__TPI_TIME_PRINT	(DISABLE)

#define DEBUG	0
#if (CONF__TPI_TRACE_PRINT == ENABLE)
    #define TPI_TRACE_PRINT(x)      if (DEBUG == 0) printk x
#else
    #define TPI_TRACE_PRINT(x)
#endif

/*\
| | Debug Print Macro
| |
| | Note: TPI_DEBUG_PRINT Requires double parenthesis
| | Example:  TPI_DEBUG_PRINT(("hello, world!\n"));
\*/

#if (CONF__TPI_DEBUG_PRINT == ENABLE)
    #define TPI_DEBUG_PRINT(x)      if (DEBUG == 0) printk x
#else
    #define TPI_DEBUG_PRINT(x)
#endif

/*\
| | EDID Print Macro
| |
| | Note: To enable EDID description printing, both CONF__TPI_EDID_PRINT and CONF__TPI_DEBUG_PRINT must be enabled
| |
| | Note: TPI_EDID_PRINT Requires double parenthesis
| | Example:  TPI_EDID_PRINT(("hello, world!\n"));
\*/

#if (CONF__TPI_EDID_PRINT == ENABLE)
#ifdef LATTICE_ORIGINAL
    #define TPI_EDID_PRINT(x)       if (DEBUG == 0) TPI_DEBUG_PRINT(x)
#else
    #define TPI_EDID_PRINT(x)       do {if (DEBUG == 0) TPI_DEBUG_PRINT(x);} while(0)
#endif
#else
    #define TPI_EDID_PRINT(x)
#endif

// MSG API   - For Debug purposes
#define MSG_ALWAYS              0x00
#define MSG_STAT                0x01
#define MSG_DBG                 0x02

#define DEBUG_PRINT(l,x)      if (l<=5) printk x

#endif
