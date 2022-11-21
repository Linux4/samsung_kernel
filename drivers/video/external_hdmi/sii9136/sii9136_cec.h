
#ifndef __HDMI_TX_CEC_H__
#define __HDMI_TX_CEC_H__


#include <linux/types.h>
#include "sii9136_cec_enums.h"

#define MAKE_SRCDEST( src, dest)    (( src << 4) | dest )

#define SII_NUMBER_OF_PORTS         5

enum
{
    SI_CECTASK_IDLE,
    SI_CECTASK_ENUMERATE,
    SI_CECTASK_NEWSOURCE,
    SI_CECTASK_SENDMSG
};

typedef struct
{
    uint8_t deviceType;     // 0 - Device is a TV.
                            // 1 - Device is a Recording device
                            // 2 - Device is a reserved device
                            // 3 - Device is a Tuner
                            // 4 - Device is a Playback device
                            // 5 - Device is an Audio System
    uint8_t  cecLA;         // CEC Logical address of the device.
    uint16_t cecPA;         // CEC Physical address of the device.
} CEC_DEVICE;


#define SII_MAX_CMD_SIZE 16

typedef enum 
{
    SI_TX_WAITCMD,
    SI_TX_SENDING,
    SI_TX_SENDACKED,
    SI_TX_SENDFAILED
} SI_txState_t;

typedef enum 
{
    SI_CEC_SHORTPULSE       = 0x80,
    SI_CEC_BADSTART         = 0x40,
    SI_CEC_RXOVERFLOW       = 0x20,
    SI_CEC_ERRORS           = (SI_CEC_SHORTPULSE | SI_CEC_BADSTART | SI_CEC_RXOVERFLOW)
} SI_cecError_t;

//-------------------------------------------------------------------------------
// CPI data structures
//-------------------------------------------------------------------------------

typedef struct
{
    uint8_t srcDestAddr;            // Source in upper nybble, dest in lower nybble
    uint8_t opcode;
    uint8_t args[ SII_MAX_CMD_SIZE ];
    uint8_t argCount;
   	uint8_t nextFrameArgCount;
} SI_CpiData_t;

typedef struct 
{
    uint8_t rxState;
    uint8_t txState;
    uint8_t cecError;

} SI_CpiStatus_t;

// CDC Feedback Messages
#define CDC_FB_MSG_NONE                 0x0000  // default value: no messages
#define CDC_FB_MSG_HST_SEARCH_DONE      0x0001  // Hosts search task finished
#define CDC_FB_MSG_CONNECT_DONE         0x0002  // Connect task finished
#define CDC_FB_MSG_CONNECT_ADJ_DONE     0x0003  // Connect to Adjacent device task finished
#define CDC_FB_MSG_DISCONNECT_DONE      0x0004  // Disonnect task finished
#define CDC_FB_MSG_DISCONNECT_ALL_DONE  0x0005  // Disonnect All task finished
#define CDC_FB_MSG_DISCONNECT_LOST_DONE 0x0006  // Disonnect Lost Device task finished
#define CDC_FB_MSG_HPD_SIGNAL_DONE      0x0007  // HPD signalling task finished
#define CDC_FB_MSG_HPD_STATE_CHANGED    0x0008  // HPD state of a Source has been changed by Sink device 
#define CDC_FB_MSG_HPD_CAPABILITY_CONF  0x0009  // HPD capability has been confirmed
#define CDC_FB_MSG_CAPABILITY_CHANGED   0x000a  // One of Devices in the HDMI network has changed its capability state 
#define CDC_FB_MSG_ERROR                0x00E0  // General error occured 
#define CDC_FB_MSG_ERR_NONCDC_CMD       0x00E1  // Non CDC command received
#define CDC_FB_MSG_ERR_HPD_SIGNAL       0x00E2  // HDMI Source not responded 
#define CDC_FB_MSG_ERR_HPD_CAP_FAILED   0x00E3  // Failed to verify HDP-over-CDC capability


void        si_CecSendMessage( uint8_t opCode, uint8_t dest );

char *CpCecTranslateLA( uint8_t bLogAddr );
char *CpCecTranslateOpcodeName( SI_CpiData_t *pMsg );
bool CpCecPrintCommand( SI_CpiData_t *pMsg, bool isTX );

uint8_t SiIRegioRead ( uint16_t regAddr );
uint8_t     SI_CecHandler( uint8_t currentPort, bool returnTask );
uint16_t	SI_CecGetDevicePA( void );
void		SI_CecSetDevicePA( uint16_t devPa );
uint8_t SI_CecInit ( void );

uint8_t SI_CpiRead( SI_CpiData_t *pCpi );
uint8_t SI_CpiWrite( SI_CpiData_t *pCpi );
uint8_t SI_CpiStatus( SI_CpiStatus_t *pCpiStat );

uint8_t SI_CpiInit( void );
uint8_t SI_CpiSetLogicalAddr( uint8_t logicalAddress );
uint8_t	SI_CpiGetLogicalAddr( void );
void SI_CpiSendPing( uint8_t bCECLogAddr );

bool CpCecRxMsgHandler( SI_CpiData_t *pCpi );

#endif
