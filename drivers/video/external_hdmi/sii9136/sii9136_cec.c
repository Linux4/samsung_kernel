
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include "sii9136_driver.h"
#include "sii9136_cec.h"
#include "sii9136_reg.h"
#include "def.h"

#ifndef LATTICE_ORIGINAL
#define IS_CDC 0
#endif

//-------------------------------------------------------------------------------
// CPI Enums, typedefs, and manifest constants
//-------------------------------------------------------------------------------

    /* New Source Task internal states    */

enum
{
    NST_START               = 1,
    NST_SENT_PS_REQUEST,
    NST_SENT_PWR_ON,
    NST_SENT_STREAM_REQUEST
};

    /* Task CPI states. */

enum
{
    CPI_IDLE            = 1,
    CPI_SENDING,
    CPI_WAIT_ACK,
    CPI_WAIT_RESPONSE,
    CPI_RESPONSE
};

typedef struct
{
    uint8_t     task;       // Current CEC task
    uint8_t     state;      // Internal task state
    uint8_t     cpiState;   // State of CPI transactions
    uint8_t     destLA;     // Logical address of target device
    uint8_t     taskData1;  // BYTE Data unique to task.
    uint16_t    taskData2;  // WORD Data unique to task.

} CEC_TASKSTATE;

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------

CEC_DEVICE   g_childPortList [SII_NUMBER_OF_PORTS];

uint8_t     g_cecAddress            = CEC_LOGADDR_PLAYBACK1;

uint16_t    g_cecPhysical           = 0xFFFF;               // Initialize to Broadcast addr, will get changed
															// when EDID is read.

static uint8_t  g_currentTask       = SI_CECTASK_IDLE;

static bool     l_cecEnabled        = false;
static uint8_t  l_powerStatus       = CEC_POWERSTATUS_ON;
static uint8_t  l_portSelect        = 0x00;
static uint8_t  l_sourcePowerStatus = CEC_POWERSTATUS_STANDBY;

/* Active Source Addresses  */
static uint8_t  l_activeSrcLogical  = CEC_LOGADDR_UNREGORBC;    // Logical address of our active source
static uint16_t l_activeSrcPhysical = 0x0000;

static uint8_t l_devTypes [16] =
{
    CEC_LOGADDR_TV,
    CEC_LOGADDR_RECDEV1,
    CEC_LOGADDR_RECDEV1,
    CEC_LOGADDR_TUNER1,
    CEC_LOGADDR_PLAYBACK1,
    CEC_LOGADDR_AUDSYS,
    CEC_LOGADDR_TUNER1,
    CEC_LOGADDR_TUNER1,
    CEC_LOGADDR_PLAYBACK1,
    CEC_LOGADDR_RECDEV1,
    CEC_LOGADDR_TUNER1,
    CEC_LOGADDR_PLAYBACK1,
    0x02,
    0x02,
    CEC_LOGADDR_TV,
    CEC_LOGADDR_TV
};

static uint8_t l_newTask            = false;
static CEC_TASKSTATE l_cecTaskState = { 0 };
static CEC_TASKSTATE l_cecTaskStateQueued =
{
    SI_CECTASK_ENUMERATE,           // Current CEC task
    0,                              // Internal task state
    CPI_IDLE,                       // cpi state
    0x00,                           // Logical address of target device
    0x00,                           // BYTE Data unique to task.
    0x0000                          // WORD Data unique to task.
};


//------------------------------------------------------------------------------
// Function:    CecSendUserControlPressed
// Description: Local function for sending a "remote control key pressed"
//              command to the specified source.
//------------------------------------------------------------------------------

static void CecSendUserControlPressed ( uint8_t keyCode, uint8_t destLA  )
{
    SI_CpiData_t cecFrame;

    cecFrame.opcode        = CECOP_USER_CONTROL_PRESSED;
    cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, destLA );
    cecFrame.args[0]       = keyCode;
    cecFrame.argCount      = 1;
    SI_CpiWrite( &cecFrame );
}

//------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------
static void PrintLogAddr ( uint8_t bLogAddr )
{

#if (INCLUDE_CEC_NAMES > CEC_NO_NAMES)
    DEBUG_PRINT( MSG_DBG,(  " [%X] %s", (int)bLogAddr, CpCecTranslateLA( bLogAddr ) ));
#else
    bLogAddr = 0;   // Passify the compiler
#endif
}

//------------------------------------------------------------------------------
// Function:    UpdateChildPortList
// Description: Store the PA and LA of the subsystem if it is at the next
//              next level down from us (direct child).
//              Returns the HDMI port index that is hosting this physical address
//------------------------------------------------------------------------------
static void UpdateChildPortList ( uint8_t newLA, uint16_t newPA )
{
    uint8_t     index;
    uint16_t    mask, pa;

    /* Determine Physical Address position (A.B.C.D) of our */
    /* device and generate a mask.                          */
    /* Field D cannot host any child devices.               */

    mask = 0x00F0;
    for ( index = 1; index < 4; index++ )
    {
        if (( g_cecPhysical & mask ) != 0)
            break;
        mask <<= 4;
    }

    /* Re-initialize the child port list with possible physical     */
    /* addresses of our immediate child devices.                    */
    /* If the new physical address is one of our direct children,   */
    /* update the LA for that position in the child port list.      */

    mask    = 0x0001 << ((index - 1) * 4);
    pa      = mask;
    for ( index = 0; index < SII_NUMBER_OF_PORTS; index++)
    {
        g_childPortList[ index].cecPA = pa;
        if ( pa == newPA )
        {
            g_childPortList[ index].cecLA = newLA;
            g_childPortList[ index].deviceType = l_devTypes[newLA];
            printk("\n***** [%04x][%02d]", newPA, newLA);
        }
        pa += mask;
    }
}

//------------------------------------------------------------------------------
// Function:    CecSendFeatureAbort
// Description: Send a feature abort as a response to this message unless
//              it was broadcast (illegal).
//------------------------------------------------------------------------------
static void CecSendFeatureAbort ( SI_CpiData_t *pCpi, uint8_t reason )
{
    SI_CpiData_t cecFrame;

    if (( pCpi->srcDestAddr & 0x0F) != CEC_LOGADDR_UNREGORBC )
    {
        cecFrame.opcode        = CECOP_FEATURE_ABORT;
        cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, (pCpi->srcDestAddr & 0xF0) >> 4 );
        cecFrame.args[0]       = pCpi->opcode;
        cecFrame.args[1]       = reason;
        cecFrame.argCount      = 2;
        SI_CpiWrite( &cecFrame );
    }
}

//------------------------------------------------------------------------------
// Function:    CecHandleFeatureAbort
// Description: Received a failure response to a previous message.  Print a
//              message and notify the rest of the system
//------------------------------------------------------------------------------

#if (INCLUDE_CEC_NAMES > CEC_NO_TEXT)
static char *ml_abortReason [] =        // (0x00) <Feature Abort> Opcode    (RX)
    {
    "Unrecognized OpCode",              // 0x00
    "Not in correct mode to respond",   // 0x01
    "Cannot provide source",            // 0x02
    "Invalid Operand",                  // 0x03
    "Refused"                           // 0x04
    };
#endif

static void CecHandleFeatureAbort( SI_CpiData_t *pCpi )
{
    SI_CpiData_t cecFrame;

    cecFrame.opcode = pCpi->args[0];
    cecFrame.argCount = 0;
#if (INCLUDE_CEC_NAMES > CEC_NO_NAMES)
    DEBUG_PRINT(
        MSG_STAT, (
        "\nMessage %s(%02X) was %s by %s (%d)\n",
        CpCecTranslateOpcodeName( &cecFrame ), (int)pCpi->args[0],
        ml_abortReason[ (pCpi->args[1] <= CECAR_REFUSED) ? pCpi->args[1] : 0],
        CpCecTranslateLA( pCpi->srcDestAddr >> 4 ), (int)(pCpi->srcDestAddr >> 4)
        ));
#elif (INCLUDE_CEC_NAMES > CEC_NO_TEXT)
    DEBUG_PRINT(
        MSG_STAT, (
        "\nMessage %02X was %s by logical address %d\n",
        (int)pCpi->args[0],
        ml_abortReason[ (pCpi->args[1] <= CECAR_REFUSED) ? pCpi->args[1] : 0],
        (int)(pCpi->srcDestAddr >> 4)
        ));
#endif
}

//------------------------------------------------------------------------------
// Function:    CecHandleActiveSource
// Description: Process the CEC Active Source command by switching to the
//              broadcast port.
//------------------------------------------------------------------------------
static void CecHandleActiveSource ( SI_CpiData_t *pCpi )
{
    uint8_t     i;
    uint16_t    newPA;

    /* Extract the logical and physical addresses of the new active source. */

    l_activeSrcLogical  = (pCpi->srcDestAddr >> 4) & 0x0F;
    l_activeSrcPhysical = ((uint16_t)pCpi->args[0] << 8 ) | pCpi->args[1];

    UpdateChildPortList( l_activeSrcLogical, l_activeSrcPhysical );

    /* Determine the index of the HDMI port that    */
    /* is handling this physical address.           */

    newPA = l_activeSrcPhysical;
    for ( i = 0; i < 4; i++ )
    {
        if (( newPA & 0x000F ) != 0)
            break;
        newPA >>= 4;
    }

    /* Port address (1-based) is left in the lowest nybble. */
    /* Convert to 0-based and use it.                       */
    /* Signal main process of new port.  The main process   */
    /* will perform the physical port switch.               */

    l_portSelect = (( newPA & 0x000F ) - 1 );
    DEBUG_PRINT( MSG_DBG,( "\nACTIVE_SOURCE: %02X (%04X) (port %02X)\n", (int)l_activeSrcLogical, l_activeSrcPhysical, (int)l_portSelect ));
}

//------------------------------------------------------------------------------
// Function:    CecHandleInactiveSource
// Description: Process the CEC Inactive Source command
//------------------------------------------------------------------------------
static void CecHandleInactiveSource ( SI_CpiData_t *pCpi )
{
    uint8_t la;

    la = (pCpi->srcDestAddr >> 4) & 0x0F;
    if ( la == l_activeSrcLogical )    // The active source has deserted us!
    {
        l_activeSrcLogical  = CEC_LOGADDR_TV;
        l_activeSrcPhysical = 0x0000;
    }

    l_portSelect = 0xFF;    // Tell main process to choose another port.
}

//------------------------------------------------------------------------------
// Function:    CecHandleReportPhysicalAddress
// Description: Store the PA and LA of the subsystem.
//              This routine is called when a physical address was broadcast.
//              usually this routine is used for a system which configure as TV or Repeater.
//------------------------------------------------------------------------------

static void CecHandleReportPhysicalAddress ( SI_CpiData_t *pCpi )
{
    UpdateChildPortList(
        (pCpi->srcDestAddr >> 4) & 0xF,         // broadcast logical address
        (pCpi->args[0] << 8) | pCpi->args[1]    // broadcast physical address
        );
}

//------------------------------------------------------------------------------
// Function:    CecTaskEnumerate
// Description: Ping every logical address on the CEC bus to see if anything
//              is attached.  Code can be added to store information about an
//              attached device, but we currently do nothing with it.
//
//              As a side effect, we determines our Initiator address as
//              the first available address of 0x00, 0x0E or 0x0F.
//
// l_cecTaskState.taskData1 == Not Used.
// l_cecTaskState.taskData2 == Not used
//------------------------------------------------------------------------------
static uint8_t CecTaskEnumerate ( SI_CpiStatus_t *pCecStatus )
{
    uint8_t newTask = l_cecTaskState.task;

    g_cecAddress = CEC_LOGADDR_UNREGORBC;

    if ( l_cecTaskState.cpiState == CPI_IDLE )
    {
        if ( l_cecTaskState.destLA < CEC_LOGADDR_UNREGORBC )   // Don't ping broadcast address
        {
            SI_CpiSendPing( l_cecTaskState.destLA );
            DEBUG_PRINT( MSG_DBG, ( "...Ping...\n" ));
            PrintLogAddr( l_cecTaskState.destLA );
            l_cecTaskState.cpiState = CPI_WAIT_ACK;
        }
        else    // We're done
        {
            SI_CpiSetLogicalAddr( g_cecAddress );
            DEBUG_PRINT( MSG_DBG,( "ENUM DONE: Initiator address is " ));
            PrintLogAddr( g_cecAddress );
            DEBUG_PRINT( MSG_DBG,( "\n" ));

                /* Go to idle task, we're done. */

            l_cecTaskState.cpiState = CPI_IDLE;
            newTask = SI_CECTASK_IDLE;
        }
    }
    else if ( l_cecTaskState.cpiState == CPI_WAIT_ACK )
    {
        if ( pCecStatus->txState == SI_TX_SENDFAILED )
        {
            DEBUG_PRINT( MSG_DBG,( "\nEnum NoAck" ));
            PrintLogAddr( l_cecTaskState.destLA );

            /* If a TV address, grab it for our use.    */

            if (( g_cecAddress == CEC_LOGADDR_UNREGORBC ) &&
                (( l_cecTaskState.destLA == CEC_LOGADDR_TV ) ||
                ( l_cecTaskState.destLA == CEC_LOGADDR_FREEUSE )))
            {
                g_cecAddress = l_cecTaskState.destLA;
            }

            /* Restore Tx State to IDLE to try next address.    */

            l_cecTaskState.cpiState = CPI_IDLE;
            l_cecTaskState.destLA++;
        }
        else if ( pCecStatus->txState == SI_TX_SENDACKED )
        {
            DEBUG_PRINT( MSG_DBG,( "\n-----------------------------------------------> Enum Ack\n" ));
            PrintLogAddr( l_cecTaskState.destLA );

            /* TODO: Add code here to store info about this device if needed.   */

            /* Restore Tx State to IDLE to try next address.    */

            l_cecTaskState.cpiState = CPI_IDLE;
            l_cecTaskState.destLA++;
        }
    }

    return( newTask );
}

//------------------------------------------------------------------------------
// Function:    CecTaskStartNewSource
// Description: Request power status from a device.  If in standby, tell it
//              to turn on and wait until it does, then start the device
//              streaming to us.
//
// l_cecTaskState.taskData1 == Number of POWER_ON messages sent.
// l_cecTaskState.taskData2 == Physical address of target device.
//------------------------------------------------------------------------------
static uint8_t CecTaskStartNewSource ( SI_CpiStatus_t *pCecStatus )
{
    SI_CpiData_t    cecFrame;
    uint8_t         newTask = l_cecTaskState.task;

    if ( l_cecTaskState.cpiState == CPI_IDLE )
    {
        if ( l_cecTaskState.state == NST_START )
        {
                /* First time through here, TIMER_2 will not have   */
                /* been started yet, and therefore should return    */
                /* expired (0 == 0).                                */

            //if ( HalTimerExpired( TIMER_2 ))
            {
                DEBUG_PRINT( MSG_DBG,( "\nNST_START\n" ));
                si_CecSendMessage( CECOP_GIVE_DEVICE_POWER_STATUS, l_cecTaskState.destLA );

                l_cecTaskState.cpiState = CPI_WAIT_ACK;
                l_cecTaskState.state    = NST_SENT_PS_REQUEST;
            }
        }
    }
        /* Wait for acknowledgement of message sent.    */

    else if ( l_cecTaskState.cpiState == CPI_WAIT_ACK )
    {
        if ( pCecStatus->txState == SI_TX_SENDFAILED )
        {
            DEBUG_PRINT( MSG_DBG,( "New Source Task NoAck\n" ));

                /* Abort task */

            l_cecTaskState.cpiState = CPI_IDLE;
            newTask                 = SI_CECTASK_IDLE;
        }
        else if ( pCecStatus->txState == SI_TX_SENDACKED )
        {
            DEBUG_PRINT( MSG_DBG,( "New Source Task ACK\n" ));

            if ( l_cecTaskState.state == NST_SENT_PS_REQUEST )
            {
                l_cecTaskState.cpiState  = CPI_WAIT_RESPONSE;
            }
            else if ( l_cecTaskState.state == NST_SENT_PWR_ON )
            {
                    /* This will force us to start over with status request.    */

                l_cecTaskState.cpiState = CPI_IDLE;
                l_cecTaskState.state    = NST_START;
                l_cecTaskState.taskData1++;
            }
            else if ( l_cecTaskState.state == NST_SENT_STREAM_REQUEST )
            {
                    /* Done.    */

                l_cecTaskState.cpiState = CPI_IDLE;
                newTask                 = SI_CECTASK_IDLE;
            }
        }
    }

        /* Looking for a response to our message. During this time, we do nothing.  */
        /* The various message handlers will set the cpi state to CPI_RESPONSE when */
        /* a device has responded with an appropriate message.                      */

    else if ( l_cecTaskState.cpiState == CPI_WAIT_RESPONSE )
    {
        //Lee: Need to have a timeout here.
    }
        /* We have an answer. Go to next step.  */

    else if ( l_cecTaskState.cpiState == CPI_RESPONSE )
    {
        if ( l_cecTaskState.state == NST_SENT_PS_REQUEST )    // Acking GIVE_POWER_STATUS
        {
                /* l_sourcePowerStatus is updated by the REPORT_POWER_STATUS message.   */

            switch ( l_sourcePowerStatus )
            {
                case CEC_POWERSTATUS_ON:
                    {
                            /* Device power is on, tell device to send us some video.   */

                        cecFrame.opcode         = CECOP_SET_STREAM_PATH;
                        cecFrame.srcDestAddr    = MAKE_SRCDEST( g_cecAddress, CEC_LOGADDR_UNREGORBC );
                        cecFrame.args[0]        = (uint8_t)(l_cecTaskState.taskData2 >> 8);
                        cecFrame.args[1]        = (uint8_t)l_cecTaskState.taskData2;
                        cecFrame.argCount       = 2;
                        SI_CpiWrite( &cecFrame );
                        l_cecTaskState.cpiState = CPI_WAIT_ACK;
                        l_cecTaskState.state    = NST_SENT_STREAM_REQUEST;
                        break;
                    }
                case CEC_POWERSTATUS_STANDBY:
                case CEC_POWERSTATUS_ON_TO_STANDBY:
                    {
                            /* Device is in standby, tell it to turn on via remote control. */

                        if ( l_cecTaskState.taskData1 == 0 )
                        {
                            CecSendUserControlPressed( CEC_RC_POWER, l_cecTaskState.destLA );
                            l_cecTaskState.cpiState = CPI_WAIT_ACK;
                            l_cecTaskState.state    = NST_SENT_PWR_ON;
                            //HalTimerSet( TIMER_2, 500 );
                        }
                        else    // Abort task if already tried to turn it on device without success
                        {
                            l_cecTaskState.cpiState = CPI_IDLE;
                            newTask                 = SI_CECTASK_IDLE;
                        }
                        break;
                    }
                case CEC_POWERSTATUS_STANDBY_TO_ON:
                    {
                        l_cecTaskState.cpiState = CPI_IDLE;
                        l_cecTaskState.state    = NST_START;
                        //HalTimerSet( TIMER_2, 500 );   // Wait .5 seconds between requests for status
                        break;
                    }
            }
        }
    }

    return( newTask );
}

//------------------------------------------------------------------------------
// Function:    ValidateCecMessage
// Description: Validate parameter count of passed CEC message
//              Returns FALSE if not enough operands for the message
//              Returns TRUE if the correct amount or more of operands (if more
//              the message processor willl just ignore them).
//------------------------------------------------------------------------------
static bool ValidateCecMessage ( SI_CpiData_t *pCpi )
{
    uint8_t parameterCount = 0;
    bool    countOK = true;

    /* Determine required parameter count   */

    switch ( pCpi->opcode )
    {
        case CECOP_IMAGE_VIEW_ON:
        case CECOP_TEXT_VIEW_ON:
        case CECOP_STANDBY:
        case CECOP_GIVE_PHYSICAL_ADDRESS:
        case CECOP_GIVE_DEVICE_POWER_STATUS:
        case CECOP_GET_MENU_LANGUAGE:
        case CECOP_GET_CEC_VERSION:
            parameterCount = 0;
            break;
        case CECOP_REPORT_POWER_STATUS:         // power status
        case CECOP_CEC_VERSION:                 // cec version
            parameterCount = 1;
            break;
        case CECOP_INACTIVE_SOURCE:             // physical address
        case CECOP_FEATURE_ABORT:               // feature opcode / abort reason
        case CECOP_ACTIVE_SOURCE:               // physical address
            parameterCount = 2;
            break;
        case CECOP_REPORT_PHYSICAL_ADDRESS:     // physical address / device type
        case CECOP_DEVICE_VENDOR_ID:            // vendor id
            parameterCount = 3;
            break;
        case CECOP_SET_OSD_NAME:                // osd name (1-14 bytes)
        case CECOP_SET_OSD_STRING:              // 1 + x   display control / osd string (1-13 bytes)
            parameterCount = 1;                 // must have a minimum of 1 operands
            break;
        case CECOP_ABORT:
            break;

        case CECOP_ARC_INITIATE:
            break;
        case CECOP_ARC_REPORT_INITIATED:
            break;
        case CECOP_ARC_REPORT_TERMINATED:
            break;

        case CECOP_ARC_REQUEST_INITIATION:
            break;
        case CECOP_ARC_REQUEST_TERMINATION:
            break;
        case CECOP_ARC_TERMINATE:
            break;
        default:
            break;
    }

    /* Test for correct parameter count.    */

    if ( pCpi->argCount < parameterCount )
    {
        countOK = false;
    }

    return( countOK );
}

//------------------------------------------------------------------------------
// Function:    si_CecRxMsgHandlerFirst
// Description: This is the first message handler called in the chain.
//              It parses messages we don't want to pass on to outside handlers
//              and those that we need to look at, but not exclusively
//------------------------------------------------------------------------------
static bool si_CecRxMsgHandlerFirst ( SI_CpiData_t *pCpi )
{
    bool            msgHandled, isDirectAddressed;

    /* Check messages we handle for correct number of operands and abort if incorrect.  */

    msgHandled = true;
    if ( ValidateCecMessage( pCpi ))    // If invalid message, ignore it, but treat it as handled
    {
        isDirectAddressed = !((pCpi->srcDestAddr & 0x0F ) == CEC_LOGADDR_UNREGORBC );
        if ( isDirectAddressed )
        {
            switch ( pCpi->opcode )
            {
                case CECOP_INACTIVE_SOURCE:
                    CecHandleInactiveSource( pCpi );
                    msgHandled = false;             // Let user also handle it if they want.
                    break;

                default:
                    msgHandled = false;
                    break;
            }
        }
        else
        {
            switch ( pCpi->opcode )
            {
                case CECOP_ACTIVE_SOURCE:
                    CecHandleActiveSource( pCpi );
                    msgHandled = false;             // Let user also handle it if they want.
                    break;
                default:
                    msgHandled = false;
                    break;
            }
        }
    }

    return( msgHandled );
}

//------------------------------------------------------------------------------
// Function:    si_CecRxMsgHandlerLast
// Description: This is the last message handler called in the chain, and
//              parses any messages left untouched by the previous handlers.
//------------------------------------------------------------------------------
static bool si_CecRxMsgHandlerLast ( SI_CpiData_t *pCpi )
{
    bool            isDirectAddressed;
    SI_CpiData_t    cecFrame;

    isDirectAddressed = !((pCpi->srcDestAddr & 0x0F ) == CEC_LOGADDR_UNREGORBC );

    if ( ValidateCecMessage( pCpi ))            // If invalid message, ignore it, but treat it as handled
    {
        if ( isDirectAddressed )
        {
            switch ( pCpi->opcode )
            {
                case CECOP_FEATURE_ABORT:
                    CecHandleFeatureAbort( pCpi );
                    break;

                case CECOP_IMAGE_VIEW_ON:       // In our case, respond the same to both these messages
                case CECOP_TEXT_VIEW_ON:
                    break;

                case CECOP_STANDBY:             // Direct and Broadcast

                        /* Setting this here will let the main task know    */
                        /* (via SI_CecGetPowerState) and at the same time   */
                        /* prevent us from broadcasting a STANDBY message   */
                        /* of our own when the main task responds by        */
                        /* calling SI_CecSetPowerState( STANDBY );          */

                    l_powerStatus = CEC_POWERSTATUS_STANDBY;
                    break;

                case CECOP_INACTIVE_SOURCE:
                    CecHandleInactiveSource( pCpi );
                    break;

                case CECOP_GIVE_PHYSICAL_ADDRESS:
                    /*
		     * We are a TX chip, so set default accordingly.  TUNER1 is STB1.  Default
		     * Physical address will be 1.0.0.0
		     */
                    cecFrame.opcode        = CECOP_REPORT_PHYSICAL_ADDRESS;
                    cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, CEC_LOGADDR_UNREGORBC );
                    cecFrame.args[0]       = 0x10;             // [Physical Address]
                    cecFrame.args[1]       = 0x00;             // [Physical Address]
                    cecFrame.args[2]       = CEC_LOGADDR_TUNER1;   // [Device Type] = 3 = STB1
                    cecFrame.argCount      = 3;
                    SI_CpiWrite( &cecFrame );
                    break;

                case CECOP_GIVE_DEVICE_POWER_STATUS:

                    /* TV responds with power status.   */

                    cecFrame.opcode        = CECOP_REPORT_POWER_STATUS;
                    cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, (pCpi->srcDestAddr & 0xF0) >> 4 );
                    cecFrame.args[0]       = l_powerStatus;
                    cecFrame.argCount      = 1;
                    SI_CpiWrite( &cecFrame );
                    break;

                case CECOP_GET_MENU_LANGUAGE:

                    /* TV Responds with a Set Menu language command.    */

                    cecFrame.opcode         = CECOP_SET_MENU_LANGUAGE;
                    cecFrame.srcDestAddr    = CEC_LOGADDR_UNREGORBC;
                    cecFrame.args[0]        = 'e';     // [language code see iso/fdis 639-2]
                    cecFrame.args[1]        = 'n';     // [language code see iso/fdis 639-2]
                    cecFrame.args[2]        = 'g';     // [language code see iso/fdis 639-2]
                    cecFrame.argCount       = 3;
                    SI_CpiWrite( &cecFrame );
                    break;

                case CECOP_GET_CEC_VERSION:

                    /* TV responds to this request with it's CEC version support.   */

                    cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, (pCpi->srcDestAddr & 0xF0) >> 4 );
                    cecFrame.opcode        = CECOP_CEC_VERSION;
                    cecFrame.args[0]       = 0x04;       // Report CEC1.3a
                    cecFrame.argCount      = 1;
                    SI_CpiWrite( &cecFrame );
                    break;

                case CECOP_REPORT_POWER_STATUS:         // Someone sent us their power state.

                    l_sourcePowerStatus = pCpi->args[0];

                        /* Let NEW SOURCE task know about it.   */

                    if ( l_cecTaskState.task == SI_CECTASK_NEWSOURCE )
                    {
                        l_cecTaskState.cpiState = CPI_RESPONSE;
                    }
                    break;

                /* Do not reply to directly addressed 'Broadcast' msgs.  */

                case CECOP_ACTIVE_SOURCE:
                case CECOP_REPORT_PHYSICAL_ADDRESS:     // A physical address was broadcast -- ignore it.
                case CECOP_REQUEST_ACTIVE_SOURCE:       // We are not a source, so ignore this one.
                case CECOP_ROUTING_CHANGE:              // We are not a downstream switch, so ignore this one.
                case CECOP_ROUTING_INFORMATION:         // We are not a downstream switch, so ignore this one.
                case CECOP_SET_STREAM_PATH:             // We are not a source, so ignore this one.
                case CECOP_SET_MENU_LANGUAGE:           // As a TV, we can ignore this message
                case CECOP_DEVICE_VENDOR_ID:
                    break;

                case CECOP_ABORT:       // Send Feature Abort for all unsupported features.
                default:

                    CecSendFeatureAbort( pCpi, CECAR_UNRECOG_OPCODE );
                    break;
            }
        }

        /* Respond to broadcast messages.   */

        else
        {
            switch ( pCpi->opcode )
            {
                case CECOP_STANDBY:

                        /* Setting this here will let the main task know    */
                        /* (via SI_CecGetPowerState) and at the same time   */
                        /* prevent us from broadcasting a STANDBY message   */
                        /* of our own when the main task responds by        */
                        /* calling SI_CecSetPowerState( STANDBY );          */

                    l_powerStatus = CEC_POWERSTATUS_STANDBY;
                    break;

                case CECOP_ACTIVE_SOURCE:
                    CecHandleActiveSource( pCpi );
                    break;

                case CECOP_REPORT_PHYSICAL_ADDRESS:
                    CecHandleReportPhysicalAddress( pCpi );
                    break;

                /* Do not reply to 'Broadcast' msgs that we don't need.  */

                case CECOP_REQUEST_ACTIVE_SOURCE:       // We are not a source, so ignore this one.
                case CECOP_ROUTING_CHANGE:              // We are not a downstream switch, so ignore this one.
                case CECOP_ROUTING_INFORMATION:         // We are not a downstream switch, so ignore this one.
                case CECOP_SET_STREAM_PATH:             // We are not a source, so ignore this one.
                case CECOP_SET_MENU_LANGUAGE:           // As a TV, we can ignore this message
                    break;
            }
        }
    }

    return( true );
}

//------------------------------------------------------------------------------
// Function:    si_CecSendMessage
// Description: Send a single byte (no parameter) message on the CEC bus.  Does
//              no wait for a reply.
//------------------------------------------------------------------------------
void si_CecSendMessage ( uint8_t opCode, uint8_t dest )
{
    SI_CpiData_t cecFrame;

    cecFrame.opcode        = opCode;
    cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, dest );
    cecFrame.argCount      = 0;
    SI_CpiWrite( &cecFrame );
}

//------------------------------------------------------------------------------
// Function:    SI_CecSetPowerState
// Description: Set the CEC local power state.
//
// Valid values:    CEC_POWERSTATUS_ON
//                  CEC_POWERSTATUS_STANDBY
//                  CEC_POWERSTATUS_STANDBY_TO_ON
//                  CEC_POWERSTATUS_ON_TO_STANDBY
//------------------------------------------------------------------------------
void SI_CecSetPowerState ( uint8_t newPowerState )
{
    if ( !l_cecEnabled )
        return;

    if ( l_powerStatus != newPowerState )
    {
        switch ( newPowerState )
        {
            case CEC_POWERSTATUS_ON:

                /* Find out who is the active source. Let the   */
                /* ACTIVE_SOURCE handler do the rest.           */

                si_CecSendMessage( CECOP_REQUEST_ACTIVE_SOURCE, CEC_LOGADDR_UNREGORBC );
                break;

            case CEC_POWERSTATUS_STANDBY:

                /* If we are shutting down, tell every one else to also.   */

                si_CecSendMessage( CECOP_STANDBY, CEC_LOGADDR_UNREGORBC );
                break;

            case CEC_POWERSTATUS_STANDBY_TO_ON:
            case CEC_POWERSTATUS_ON_TO_STANDBY:
            default:
                break;
        }

    l_powerStatus = newPowerState;
    }
}

//------------------------------------------------------------------------------
// Function:    SI_CecGetDevicePA
// Description: Return the physical address for this Host device
//
//------------------------------------------------------------------------------

uint16_t SI_CecGetDevicePA (void)
{
	pr_info("%s() g_cecPhysical: 0x%x \n", __func__, g_cecPhysical);
    return( g_cecPhysical );
}

//------------------------------------------------------------------------------
// Function:    SI_CecSetDevicePA
// Description: Set the host device physical address (initiator physical address)
//------------------------------------------------------------------------------

void SI_CecSetDevicePA (uint16_t devPa)
{
    g_cecPhysical = devPa;
    DEBUG_PRINT( MSG_STAT, ("CEC: Device PA [%04X]\n", (int)devPa));
}

//------------------------------------------------------------------------------
// Function:    SI_CecInit
// Description: Initialize the CEC subsystem.
//------------------------------------------------------------------------------
uint8_t SI_CecInit ( void )
{
    uint16_t i;

    SI_CpiInit();

    for ( i = 0; i < SII_NUMBER_OF_PORTS; i++)
    {
        //if this device is the TV then assign the PA to
        //the childPortList
        if (SI_CpiGetLogicalAddr() == CEC_LOGADDR_TV)
            g_childPortList[i].cecPA = (i+1) << 12;
        else
            g_childPortList[i].cecPA = 0xFFFF;
        g_childPortList[i].cecLA = 0xFF;
    }

#ifdef F_9022A_9334
    // Initialize the CDC
    CpCdcInit();
#endif

    l_cecEnabled = true;
    return( true );

	return 1;    
}



//------------------------------------------------------------------------------
// Function:    si_TaskServer
// Description: Calls the current task handler.  A task is used to handle cases
//              where we must send and receive a specific set of CEC commands.
//------------------------------------------------------------------------------
static void si_TaskServer ( SI_CpiStatus_t *pCecStatus )
{
    switch ( g_currentTask )
    {
        case SI_CECTASK_IDLE:
            if ( l_newTask )
            {
                /* New task; copy the queued task block into the operating task block.  */

                memcpy( &l_cecTaskState, &l_cecTaskStateQueued, sizeof( CEC_TASKSTATE ));
                l_newTask = false;
                g_currentTask = l_cecTaskState.task;
            }
            break;

        case SI_CECTASK_ENUMERATE:
            g_currentTask = CecTaskEnumerate( pCecStatus );
            break;

        case SI_CECTASK_NEWSOURCE:
            g_currentTask = CecTaskStartNewSource( pCecStatus );
            break;

        default:
            break;
    }

}


//------------------------------------------------------------------------------
// Function:    SI_CecHandler
// Description: Polls the send/receive state of the CPI hardware and runs the
//              current task, if any.
//
//              A task is used to handle cases where we must send and receive
//              a specific set of CEC commands.
//------------------------------------------------------------------------------
uint8_t SI_CecHandler ( uint8_t currentPort, bool returnTask )
{
#ifdef LATTICE_ORIGINAL
    bool cdcCalled  = false;
#endif
    SI_CpiStatus_t  cecStatus;

    l_portSelect = currentPort;

    /* Get the CEC transceiver status and pass it to the    */
    /* current task.  The task will send any required       */
    /* CEC commands.                                        */

	pr_info("%s() \n", __func__);

    SI_CpiStatus( &cecStatus );
    si_TaskServer( &cecStatus );

    /* Now look to see if any CEC commands were received.   */

    if ( cecStatus.rxState )
    {
        uint8_t         frameCount;
        SI_CpiData_t    cecFrame;

        /* Get CEC frames until no more are present.    */

        cecStatus.rxState = 0;      // Clear activity flag
        while ((frameCount = ((SiIRegioRead( REG_CEC_RX_COUNT) & 0xF0) >> 4)))
        {
            DEBUG_PRINT( MSG_DBG, ("\n%d frames in Rx Fifo\n", (int)frameCount ));

            if ( SI_CpiRead( &cecFrame ))
            {
                DEBUG_PRINT( MSG_STAT, ( "Error in Rx Fifo \n" ));
                break;
            }

            /* Send the frame through the RX message Handler chain. */
#if 1
            for (;;)
            {
                if ( si_CecRxMsgHandlerFirst( &cecFrame ))  // We get a chance at it before the end user App
                    break;
#ifdef SII_ARC_SUPPORT
                if ( si_CecRxMsgHandlerARC( &cecFrame ))    // A captive end-user handler to check for HEAC commands
                    break;
#endif
                if ( CpCecRxMsgHandler( &cecFrame ))        // Let the end-user app have a shot at it.
                    break;
#if (IS_CDC == 1)
                if (si_CDCProcess(&cecFrame))            // Check CDC handler
                {
                    cdcCalled = true;
                    break;
                }
#endif
                si_CecRxMsgHandlerLast( &cecFrame );        // We get a chance at it afer the end user App to cleanup
                break;
            }
#endif
        }
    }

#if (IS_CDC == 1)
    /* If CDC handler was not called in the RX loop at least once, we must call it now. */
    if (!cdcCalled)
    {
        si_CDCProcess(NULL);  // No message associated with this call.
    }
#endif

    if ( l_portSelect != currentPort )
    {
        DEBUG_PRINT( MSG_DBG, ("\nNew port value returned: %02X (Return Task: %d)\n", (int)l_portSelect, (int)returnTask ));
    }

    return( returnTask ? g_currentTask : l_portSelect );
}

//------------------------------------------------------------------------------
// Function:    SiIRegioReadBlock
// Description: Read a block of registers starting with the specified register.
//              The register address parameter is translated into an I2C
//              slave address and offset.
//              The block of data bytes is read from the I2C slave address
//              and offset.
//------------------------------------------------------------------------------
void SiIRegioReadBlock ( uint16_t regAddr, uint8_t* buffer, uint16_t length)
{
    hdmi_read_block_reg(CEC_SLAVE_ADDR, (uint8_t)regAddr, length, buffer);
}
//------------------------------------------------------------------------------
// Function:    SiIRegioWriteBlock
// Description: Write a block of registers starting with the specified register.
//              The register address parameter is translated into an I2C slave
//              address and offset.
//              The block of data bytes is written to the I2C slave address
//              and offset.
//------------------------------------------------------------------------------
void SiIRegioWriteBlock ( uint16_t regAddr, uint8_t* buffer, uint16_t length)
{
    hdmi_write_block_reg(CEC_SLAVE_ADDR, (uint8_t)regAddr, length, buffer);
}

//------------------------------------------------------------------------------
// Function:    SiIRegioRead
// Description: Read a one byte register.
//              The register address parameter is translated into an I2C slave
//              address and offset. The I2C slave address and offset are used
//              to perform an I2C read operation.
//------------------------------------------------------------------------------
uint8_t SiIRegioRead ( uint16_t regAddr )
{
    return (hdmi_read_reg(CEC_SLAVE_ADDR, (uint8_t)regAddr));
}

//------------------------------------------------------------------------------
// Function:    SiIRegioWrite
// Description: Write a one byte register.
//              The register address parameter is translated into an I2C
//              slave address and offset. The I2C slave address and offset
//              are used to perform an I2C write operation.
//------------------------------------------------------------------------------
#ifdef LATTICE_ORIGINAL
void SiIRegioWrite ( uint8_t regAddr, uint8_t value )
#else
void SiIRegioWrite ( uint16_t regAddr, uint8_t value )
#endif
{
    hdmi_write_reg(CEC_SLAVE_ADDR, (uint8_t)regAddr, value);
}

//------------------------------------------------------------------------------
// Function:    SiIRegioModify
// Description: Modify a one byte register under mask.
//              The register address parameter is translated into an I2C
//              slave address and offset. The I2C slave address and offset are
//              used to perform I2C read and write operations.
//
//              All bits specified in the mask are set in the register
//              according to the value specified.
//              A mask of 0x00 does not change any bits.
//              A mask of 0xFF is the same a writing a byte - all bits
//              are set to the value given.
//              When only some bits in the mask are set, only those bits are
//              changed to the values given.
//------------------------------------------------------------------------------
void SiIRegioModify ( uint16_t regAddr, uint8_t mask, uint8_t value)
{
    uint8_t abyte;

    abyte = hdmi_read_reg( CEC_SLAVE_ADDR, (uint8_t)regAddr );
    abyte &= (~mask);                                       //first clear all bits in mask
    abyte |= (mask & value);                                //then set bits from value
    hdmi_write_reg( CEC_SLAVE_ADDR, (uint8_t)regAddr, abyte);
}

//------------------------------------------------------------------------------
// Function:    SI_CpiSetLogicalAddr
// Description: Configure the CPI subsystem to respond to a specific CEC
//              logical address.
//------------------------------------------------------------------------------
uint8_t SI_CpiSetLogicalAddr ( uint8_t logicalAddress )
{
    uint8_t capture_address[2];
    uint8_t capture_addr_sel = 0x01;

    capture_address[0] = 0;
    capture_address[1] = 0;
    if( logicalAddress < 8 )
    {
        capture_addr_sel = capture_addr_sel << logicalAddress;
        capture_address[0] = capture_addr_sel;
    }
    else
    {
        capture_addr_sel   = capture_addr_sel << ( logicalAddress - 8 );
        capture_address[1] = capture_addr_sel;
    }

        // Set Capture Address

//    SiIRegioWriteBlock(REG_CEC_CAPTURE_ID0, capture_address, 2 );
	SiIRegioWrite(REG_CEC_CAPTURE_ID0, capture_address[0]);
    SiIRegioWrite(REG_CEC_CAPTURE_ID1, capture_address[1]);

    SiIRegioWrite(REG_CEC_TX_INIT, logicalAddress);
//	DEBUG_PRINT(MSG_STAT,("CEC: logicalAddress: 0x%x\n", (int)logicalAddress)); 

    return( true );
}

//------------------------------------------------------------------------------
// Function:    SI_CpiSendPing
// Description: Initiate sending a ping, and used for checking available
//                       CEC devices
//------------------------------------------------------------------------------
void SI_CpiSendPing ( uint8_t bCECLogAddr )
{
    SiIRegioWrite( REG_CEC_TX_DEST, BIT_SEND_POLL | bCECLogAddr );
}

//------------------------------------------------------------------------------
// Function:    SI_CpiWrite
// Description: Send CEC command via CPI register set
//------------------------------------------------------------------------------
uint8_t SI_CpiWrite( SI_CpiData_t *pCpi )
{
    uint8_t cec_int_status_reg[2];

#if (INCLUDE_CEC_NAMES > CEC_NO_TEXT)
    CpCecPrintCommand( pCpi, true );
#endif
    SiIRegioModify( REG_CEC_DEBUG_3, BIT_FLUSH_TX_FIFO, BIT_FLUSH_TX_FIFO );  // Clear Tx Buffer

        /* Clear Tx-related buffers; write 1 to bits to be cleared. */

    cec_int_status_reg[0] = 0x64 ; // Clear Tx Transmit Buffer Full Bit, Tx msg Sent Event Bit, and Tx FIFO Empty Event Bit
    cec_int_status_reg[1] = 0x02 ; // Clear Tx Frame Retranmit Count Exceeded Bit.
    SiIRegioWriteBlock( REG_CEC_INT_STATUS_0, cec_int_status_reg, 2 );

        /* Send the command */

    SiIRegioWrite( REG_CEC_TX_DEST, pCpi->srcDestAddr & 0x0F );           // Destination
    SiIRegioWrite( REG_CEC_TX_COMMAND, pCpi->opcode );
    SiIRegioWriteBlock( REG_CEC_TX_OPERAND_0, pCpi->args, pCpi->argCount );
    SiIRegioWrite( REG_CEC_TRANSMIT_DATA, BIT_TRANSMIT_CMD | pCpi->argCount );

    return( true );
}

//------------------------------------------------------------------------------
// Function:    SI_CpiRead
// Description: Reads a CEC message from the CPI read FIFO, if present.
//------------------------------------------------------------------------------
uint8_t SI_CpiRead( SI_CpiData_t *pCpi )
{
    bool    error = false;
    uint8_t argCount;

    argCount = SiIRegioRead( REG_CEC_RX_COUNT );

    if ( argCount & BIT_MSG_ERROR )
    {
        error = true;
    }
    else
    {
        pCpi->argCount = argCount & 0x0F;
        pCpi->srcDestAddr = SiIRegioRead( REG_CEC_RX_CMD_HEADER );
        pCpi->opcode = SiIRegioRead( REG_CEC_RX_OPCODE );
        if ( pCpi->argCount )
        {
            SiIRegioReadBlock( REG_CEC_RX_OPERAND_0, pCpi->args, pCpi->argCount );
        }
    }

        // Clear CLR_RX_FIFO_CUR;
        // Clear current frame from Rx FIFO

    SiIRegioModify( REG_CEC_RX_CONTROL, BIT_CLR_RX_FIFO_CUR, BIT_CLR_RX_FIFO_CUR );

#if (INCLUDE_CEC_NAMES > CEC_NO_TEXT)
    if ( !error )
    {
        CpCecPrintCommand( pCpi, false );
    }
#endif
    return( error );
}

//------------------------------------------------------------------------------
// Function:    SI_CpiStatus
// Description: Check CPI registers for a CEC event
//------------------------------------------------------------------------------
uint8_t SI_CpiStatus( SI_CpiStatus_t *pStatus )
{
    uint8_t cecStatus[2];

    pStatus->txState    = 0;
    pStatus->cecError   = 0;
    pStatus->rxState    = 0;

	pr_info("%s() - 1 \n", __func__);

    SiIRegioReadBlock( REG_CEC_INT_STATUS_0, cecStatus, 2);

    if ( (cecStatus[0] & 0x7F) || cecStatus[1] )
    {
        DEBUG_PRINT(MSG_STAT,("\nCEC Status: %02X %02X\n", (int) cecStatus[0], (int) cecStatus[1]));

            // Clear interrupts

        if ( cecStatus[1] & BIT_FRAME_RETRANSM_OV )
        {
            DEBUG_PRINT(MSG_DBG,("\n!TX retry count exceeded! [%02X][%02X]\n",(int) cecStatus[0], (int) cecStatus[1]));

                /* Flush TX, otherwise after clearing the BIT_FRAME_RETRANSM_OV */
                /* interrupt, the TX command will be re-sent.                   */

            SiIRegioModify( REG_CEC_DEBUG_3,BIT_FLUSH_TX_FIFO, BIT_FLUSH_TX_FIFO );
        }

            // Clear set bits that are set

        SiIRegioWriteBlock( REG_CEC_INT_STATUS_0, cecStatus, 2 );

            // RX Processing

        if ( cecStatus[0] & BIT_RX_MSG_RECEIVED )
        {
            pStatus->rxState = 1;   // Flag caller that CEC frames are present in RX FIFO
        }

            // RX Errors processing

        if ( cecStatus[1] & BIT_SHORT_PULSE_DET )
        {
            pStatus->cecError |= SI_CEC_SHORTPULSE;
        }

        if ( cecStatus[1] & BIT_START_IRREGULAR )
        {
            pStatus->cecError |= SI_CEC_BADSTART;
        }

        if ( cecStatus[1] & BIT_RX_FIFO_OVERRUN )
        {
            pStatus->cecError |= SI_CEC_RXOVERFLOW;
        }

            // TX Processing

        if ( cecStatus[0] & BIT_TX_FIFO_EMPTY )
        {
            pStatus->txState = SI_TX_WAITCMD;
        }
        if ( cecStatus[0] & BIT_TX_MESSAGE_SENT )
        {
            pStatus->txState = SI_TX_SENDACKED;
        }
        if ( cecStatus[1] & BIT_FRAME_RETRANSM_OV )
        {
            pStatus->txState = SI_TX_SENDFAILED;
        }
    }

    return( true );
}

//------------------------------------------------------------------------------
// Function:    SI_CpiGetLogicalAddr
// Description: Get Logical Address
//------------------------------------------------------------------------------
uint8_t SI_CpiGetLogicalAddr( void )
{
    return SiIRegioRead( REG_CEC_TX_INIT);
}

//------------------------------------------------------------------------------
// Function:    SI_CpiInit
// Description: Initialize the CPI subsystem for communicating via CEC
//------------------------------------------------------------------------------
uint8_t SI_CpiInit( void )
{

#ifdef DEV_SUPPORT_CEC_FEATURE_ABORT
    // Turn on CEC auto response to <Abort> command.
    SiIRegioWrite( CEC_OP_ABORT_31, BIT7 );
#else
    // Turn off CEC auto response to <Abort> command.
    SiIRegioWrite( CEC_OP_ABORT_31, CLEAR_BITS );
#endif

#ifdef DEV_SUPPORT_CEC_CONFIG_CPI_0
    // Bit 4 of the CC Config reister must be cleared to enable CEC
	SiIRegioModify (REG_CEC_CONFIG_CPI, 0x10, 0x00);
#endif

    // initialized he CEC CEC_LOGADDR_TV logical address
    if ( !SI_CpiSetLogicalAddr( CEC_LOGADDR_PLAYBACK1 ))
    {
        pr_info("\n Cannot init CPI/CEC");
        return( false );
    }

    return( true );
}

//------------------------------------------------------------------------------
// Function:    CecViewOn
// Description: Take the HDMI switch and/or sink device out of standby and
//              enable it to display video.
//------------------------------------------------------------------------------

static void CecViewOn ( SI_CpiData_t *pCpi )
{
    pCpi = pCpi;

    SI_CecSetPowerState( CEC_POWERSTATUS_ON );
}

//------------------------------------------------------------------------------
// Function:    CpCecRxMsgHandler
// Description: Parse received messages and execute response as necessary
//              Only handle the messages needed at the top level to interact
//              with the Port Switch hardware.  The SI_API message handler
//              handles all messages not handled here.
//
// Warning:     This function is referenced by the Silicon Image CEC API library
//              and must be present for it to link properly.  If not used,
//              it should return 0 (false);
//
//              Returns true if message was processed by this handler
//------------------------------------------------------------------------------
bool CpCecRxMsgHandler ( SI_CpiData_t *pCpi )
{
    bool            processedMsg, isDirectAddressed;

    isDirectAddressed = !((pCpi->srcDestAddr & 0x0F ) == CEC_LOGADDR_UNREGORBC );

    processedMsg = true;
    if ( isDirectAddressed )
    {
        switch ( pCpi->opcode )
        {
            case CECOP_IMAGE_VIEW_ON:       // In our case, respond the same to both these messages
            case CECOP_TEXT_VIEW_ON:
                CecViewOn( pCpi );
                break;

            case CECOP_INACTIVE_SOURCE:
                break;
            default:
                processedMsg = false;
                break;
        }
    }

    /* Respond to broadcast messages.   */

    else
    {
        switch ( pCpi->opcode )
        {
            case CECOP_ACTIVE_SOURCE:
                break;
            default:
                processedMsg = false;
            break;
    }
    }

    return( processedMsg );
}


