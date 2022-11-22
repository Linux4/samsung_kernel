/*
 * TI's FM Stack
 *
 * Copyright 2001-2008 Texas Instruments, Inc. - http://www.ti.com/
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and  
 * limitations under the License.
 */
/*******************************************************************************\
*
*   FILE NAME:      fm_rx.c
*
*   BRIEF:          This file defines the API of the FM Rx stack.
*
*   DESCRIPTION:    General
*
*                   
*                   
*   AUTHOR:   Zvi Schneider
*
\*******************************************************************************/
#include "fmc_types.h"
#include "fmc_defs.h"
#include "fmc_os.h"
#include "fmc_log.h"
#include "fmc_debug.h"
#include "fm_rx.h"
#include "fm_rx_sm.h"
#include "fm_trace.h"

FMC_LOG_SET_MODULE(FMC_LOG_MODULE_FMRX);

#if  FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED

/*
    Possible initialization states
*/
typedef enum _tagFmRxState {
    _FM_RX_INIT_STATE_NOT_INITIALIZED,
    _FM_RX_INIT_STATE_INITIALIZED,
    _FM_RX_INIT_STATE_INIT_FAILED
} _FmRxInitState;
typedef struct
{
    FMC_BOOL cond;
    FmRxStatus  errorStatus;
}_FmRxConditionParams;

/* current init state */
static _FmRxInitState _fmRxInitState = _FM_RX_INIT_STATE_NOT_INITIALIZED;

/* Macro that should be used at the start of APIs that require the FM TX to be enabled to start */

#define _FM_RX_FUNC_START_AND_LOCK_ENABLED(funcName) \
            FMC_FUNC_START_AND_LOCK(funcName);       \
            FMC_VERIFY_ERR(((FM_RX_SM_IsContextEnabled() == FMC_TRUE) &&  \
                           (FM_RX_SM_IsCmdPending(FM_RX_CMD_DISABLE) ==   \
                            FMC_FALSE)),             \
                          FM_RX_STATUS_CONTEXT_NOT_ENABLED,               \
            ("%s May Be Called only when FM RX is enabled", funcName))

#define _FM_RX_FUNC_END_AND_UNLOCK_ENABLED()    FMC_FUNC_END_AND_UNLOCK()

#define _FM_RX_FUNC_START_ENABLED(funcName) \
            FMC_FUNC_START(funcName);       \
            FMC_VERIFY_ERR(((FM_RX_SM_IsContextEnabled() == FMC_TRUE) &&  \
                           (FM_RX_SM_IsCmdPending(FM_RX_CMD_DISABLE) ==   \
                            FMC_FALSE)),    \
                          FM_RX_STATUS_CONTEXT_NOT_ENABLED,               \
            ("%s May Be Called only when FM RX is enabled", funcName))

#define _FM_RX_FUNC_END_ENABLED()    FMC_FUNC_END()

FmRxStatus _FM_RX_SimpleCmdAndCopyParams(FmRxContext *fmContext, 
                                FMC_UINT paramIn,
                                FMC_BOOL condition,
                                FmRxCmdType cmdT,
                                const char * funcName);
FmRxStatus _FM_RX_SimpleCmd(FmRxContext *fmContext, 
                                FmRxCmdType cmdT,
                                const char * funcName);


 FmRxCallBack fmRxInitAsyncAppCallback = NULL;

 FmRxErrorCallBack fmRxErrorAppCallback = NULL;



extern FMC_BOOL fmRxSendDisableEventToApp;
/********************************************************************************
 *
 * Function declarations
 *
 *******************************************************************************/

 /*-------------------------------------------------------------------------------
  * FM_RX_Init()
  *
  * Brief:  
  *      Initializes FM RX module.
  *
  * Description:
  *    It is usually called at system startup.
  *
  *    This function must be called by the system before any other FM RX function.
  *
  * Type:
  *      Synchronous
  *
  * Parameters:
  *      void.
  *
  * Returns:
  *      FMC_STATUS_SUCCESS - FM was initialized successfully.
  *
  *      FMC_STATUS_FAILED -  FM failed initialization.
  */

FmRxStatus FM_RX_Init(const FmRxErrorCallBack fmErrorCallback)
{
    FmRxStatus  status = FM_RX_STATUS_SUCCESS;

    FMC_FUNC_START(("FM_RX_Init"));
    fm_trace_init();

    /*Save the CallBack */
    fmRxErrorAppCallback = fmErrorCallback;
    
    FMC_VERIFY_ERR((_fmRxInitState == _FM_RX_INIT_STATE_NOT_INITIALIZED), FM_RX_STATUS_NOT_DE_INITIALIZED,
                        ("FM_RX_Init: Invalid call while FM RX Is not De-Initialized"));
    
    /* Assume failure. If we fail before reaching the end, we will stay in this state */
    _fmRxInitState = _FM_RX_INIT_STATE_INIT_FAILED;
    
    /* Init RX & TX common module */
    status = FMCI_Init();
    FMC_VERIFY_FATAL((status == FMC_STATUS_SUCCESS), status, 
                        ("FM_RX_Init: FMCI_Init Failed (%s)", FMC_DEBUG_FmcStatusStr(status)));
    
    /* Init FM TX state machine */
    FM_RX_SM_Init();
        
    _fmRxInitState = _FM_RX_INIT_STATE_INITIALIZED;
    
    FMC_LOG_INFO(("FM_RX_Init: FM RX Initialization completed Successfully"));
    
    FMC_FUNC_END();
    
    return status;
}



 /*-------------------------------------------------------------------------------
  * FM_RX_Init_Async()
  *
  * Brief:  
  *      Initializes FM RX module.
  *
  * Description:
  *    It is usually called at system startup.
  *
  *    This function must be called by the system before any other FM RX function.
  *
  * Type:
  *      Synchronous\Asynchronous
  *  
  * Generated Events:
  *      eventType=FM_RX_EVENT_CMD_DONE, with commandType == FM_RX_INIT_ASYNC
  *      The only feilds that are valid at the event:
  *
  *      eventType
  *      status
  *      p.cmdDone.cmd   
  *
  * Parameters:
   *     fmInitCallback [in] - The FM RX Init event will be sent to this callback.
  *
  * Returns:
  *      FMC_STATUS_SUCCESS   -   FM was initialized successfully.
  *
  *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
  *                              the application upon completion.
  *
  *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
  *
  *      FMC_STATUS_FAILED -  FM failed initialization.
  *
  */

FmRxStatus FM_RX_Init_Async(const FmRxCallBack fmInitCallback,const FmRxErrorCallBack fmErrorCallback)
{
    FmRxStatus  status = FM_RX_STATUS_SUCCESS;
    FmRxSmContextState contextState=FM_RX_SM_CONTEXT_STATE_DESTROYED;
    FmRxContext *context = NULL;

    FMC_FUNC_START(("FM_RX_Init_Async"));

    FMC_VERIFY_ERR((0 != fmInitCallback), FM_RX_STATUS_INVALID_PARM, ("Null fmInitCallback"));
    
    /*Save the CallBack */
    fmRxInitAsyncAppCallback =fmInitCallback;

    /*Check if the stack is initialized*/
    if (_fmRxInitState==_FM_RX_INIT_STATE_INITIALIZED)
       {
    
        /*Need to verify What is the current SM state */
        contextState =FM_RX_SM_GetContextState();

        /*Get the current context */
        context =FM_RX_SM_GetContext();

        /*According to the State we need to perfrom the crospanding operations */
        switch(contextState)
        {                

            /*
                The Stack is allready disabled    
                Need only to Destroy 
            */
            case FM_RX_SM_CONTEXT_STATE_DISABLED:
                   status= FM_RX_Destroy(&context);                                                       
                   FMC_VERIFY_ERR((status== FM_RX_STATUS_SUCCESS), FM_RX_STATUS_FAILED, 
                   ("FM_RX_Init_Async: FM_RX_Destroy (%s)", FMC_DEBUG_FmcStatusStr(status)));
                 break;
           
            case FM_RX_SM_CONTEXT_STATE_ENABLED:
             /*
                Set up a flag that specified that this 
                specific event is not pass to the App
            */
                    fmRxSendDisableEventToApp=FMC_FALSE;
                    status= FM_RX_Disable(context);                      
                   FMC_VERIFY_ERR((status== FM_RX_STATUS_PENDING), FM_RX_STATUS_FAILED, 
                   ("FM_RX_Init_Async: FM_RX_Disable (%s)", FMC_DEBUG_FmcStatusStr(status)));
                 break;                      
             /*
                Set up a flag that specified that this specific event     
                Is not pass to the App
            */   
            case  FM_RX_SM_CONTEXT_STATE_DISABLING:          
                    fmRxSendDisableEventToApp=FMC_FALSE;
                    status = FM_RX_STATUS_PENDING;
                    break;
             /*
                At enabling state we need to disable
                the stuck, but not to pass the event 
                to the application  
             */
            case  FM_RX_SM_CONTEXT_STATE_ENABLING:                                  
                    fmRxSendDisableEventToApp=FMC_FALSE;
                    status=FM_RX_Disable(context);                 
                    FMC_VERIFY_ERR((status== FM_RX_STATUS_PENDING), FM_RX_STATUS_FAILED, 
                   ("FM_RX_Init_Async: FM_RX_Disable (%s)", FMC_DEBUG_FmcStatusStr(status)));

              /*  
                Action is not needed here.
              */
            case FM_RX_SM_CONTEXT_STATE_DESTROYED:
            default:
                 break;
            }

    }
    else
     {
        /*  
            First Init
        */
           status=FM_RX_Init(fmErrorCallback);
      }         

    FMC_LOG_INFO(("FM_RX_Init_Async: FM RX Initialization completed Successfully"));
    
    FMC_FUNC_END();
    
    return status;
}
 /*-------------------------------------------------------------------------------
  * FM_Deinit()
  *
  * Brief:  
  *      Deinitializes FM RX module.
  *
  * Description:
  *      After calling this function, no FM function (RX or TX) can be called.
  *
  * Caveats:
  *      Both FM RX & TX must be detroyed before calling this function.
  *
  * Type:
  *      Synchronous
  *
  * Parameters:
  *      void
  *
  * Returns:
  *      N/A (void)
  */

FmRxStatus FM_RX_Deinit(void)
{
    FmRxStatus  status = FM_RX_STATUS_SUCCESS;
    
    FMC_FUNC_START(("FM_RX_Deinit"));
    
    if (_fmRxInitState !=_FM_RX_INIT_STATE_INITIALIZED)
    {
        FMC_LOG_INFO(("FM_RX_Deinit: Module wasn't properly initialized. Exiting without de-initializing"));
        FMC_RET(FM_RX_STATUS_SUCCESS);
    }
    
    FMC_VERIFY_FATAL((FM_RX_SM_GetContextState() ==  FM_RX_SM_CONTEXT_STATE_DESTROYED), 
                        FM_RX_STATUS_CONTEXT_NOT_DESTROYED, ("FM_RX_Deinit: FM RX Context must first be destoryed"));
    
    /* De-Init FM TX state machine */
    FM_RX_SM_Deinit();
    
    /* De-Init RX & TX common module */
    FMCI_Deinit();
    
    _fmRxInitState = _FM_RX_INIT_STATE_NOT_INITIALIZED;

    fmRxErrorAppCallback = NULL;

    FMC_FUNC_END();
fm_trace_deinit();
    
    return status;
}

/*-------------------------------------------------------------------------------
 * FM_RX_Create()
 *
 * Brief:  
 *      Allocates a unique FM RX context.
 *
 * Description:
 *      This function must be called before any other FM RX function.
 *
 *      The allocated context should be supplied in subsequent FM RX calls.
 *      The caller must also provide a callback function, which will be called 
 *      on FM RX events.
 *
 *      Both FM RX & TX Contexts may exist concurrently. However, only one at a time may be enabled 
 *      (see FM_RX_Enable() for details)
 *
 *      Currently, only a single context may be created
 *
 * Type:
 *      Synchronous
 *
 * Parameters:
 *      appHandle [in] - application handle, can be a null pointer (use default application).
 *
 *      fmCallback [in] - all FM RX events will be sent to this callback.
 *      
 *      fmContext [out] - allocated FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_SUCCESS - Operation completed successfully.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_ALREADY_EXISTS - FM RX Context already created
 *
 *      FM_RX_STATUS_FM_NOT_INITIALIZED - FM Not initialized. Please call FM_Init() first
 */

FmRxStatus FM_RX_Create(FmcAppHandle *appHandle, const FmRxCallBack fmCallback, FmRxContext **fmContext)
{
    FmRxStatus  status = FM_RX_STATUS_SUCCESS;
    
    FMC_FUNC_START(("FM_RX_Create"));
    
    FMC_VERIFY_ERR((appHandle == NULL), FMC_STATUS_NOT_SUPPORTED, ("FM_RX_Create: appHandle Must be null currently"));
    FMC_VERIFY_ERR((_fmRxInitState == _FM_RX_INIT_STATE_INITIALIZED), FM_RX_STATUS_NOT_INITIALIZED,
                        ("FM_RX_Create: FM RX Not Initialized"));
    FMC_VERIFY_FATAL((FM_RX_SM_GetContextState() ==  FM_RX_SM_CONTEXT_STATE_DESTROYED), 
                        FM_RX_STATUS_CONTEXT_NOT_DESTROYED, ("FM_RX_Deinit: FM TX Context must first be destoryed"));
    
    status = FM_RX_SM_Create(fmCallback, fmContext);
    FMC_VERIFY_FATAL((status == FM_RX_STATUS_SUCCESS), status, 
                        ("FM_RX_Create: FM_RX_SM_Create Failed (%s)", FMC_DEBUG_FmcStatusStr(status)));
    
    FMC_LOG_INFO(("FM_RX_Create: Successfully Create FM RX Context"));
    
    FMC_FUNC_END();
    
    return status;
}

/*-------------------------------------------------------------------------------
 * FM_RX_Destroy()
 *
 * Brief:  
 *      Releases the FM RX context (previously allocated with FM_RX_Create()).
 *
 * Description:
 *      An application should call this function when it completes using FM RX services.
 *
 *      Upon completion, the FM RX context is set to null in order to prevent 
 *      the application from an illegal attempt to keep using it.
 *
 *      This function perfomrs an orderly destruction of the context. This means that:
 *      1. If the FM RX context is enabled it is first disabled
 *
 * Generated Events:
 *      1. If the context is disabled, FM_RX_EVENT_CMD_DONE, with commandType == FM_RX_CMD_DISABLE; And
 *      2. Another event FM_RX_EVENT_CMD_DONE, with commandType == FM_RX_CMD_DESTROY
 *
 * Type:
 *      Synchronous/Asynchronous
 *
 * Parameters:
 *      fmContext [in/out] - FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_SUCCESS - Operation completed successfully.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
 * 
 *      FM_RX_STATUS_IN_PROGRESS - This function is already in progress 
 */
FmRxStatus FM_RX_Destroy(FmRxContext **fmContext)
{
    FmRxStatus  status = FM_RX_STATUS_SUCCESS;
    
    FMC_FUNC_START(("FM_RX_Destroy"));
    
    FMC_VERIFY_ERR((*fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("FM_RX_Destroy: Invalid Context Ptr"));
    FMC_VERIFY_ERR((FM_RX_SM_GetContextState() ==  FM_RX_SM_CONTEXT_STATE_DISABLED), 
                        FM_RX_STATUS_CONTEXT_NOT_DISABLED, ("FM_RX_Destroy: FM RX Context must be disabled before destroyed"));
    
    status = FM_RX_SM_Destroy(fmContext);
    FMC_VERIFY_FATAL((status == FM_RX_STATUS_SUCCESS), status, 
                        ("FM_RX_Destroy: FM_RX_SM_Destroy Failed (%s)", FMC_DEBUG_FmcStatusStr(status)));
    
    /* Make sure pointer will not stay dangling */
    *fmContext = NULL;
    
    FMC_LOG_INFO(("FM_RX_Destroy: Successfully Destroyed FM RX Context"));
    
    FMC_FUNC_END();
    
    return status;
}

/*-------------------------------------------------------------------------------
 * FM_RX_Enable()
 *
 * Brief:  
 *      Enable FM RX
 *
 * Description:
 *      After calling this function, FM RX is enabled and ready for configurations.
 *
 *      This procedure will initialize the FM RX Module in the chip. This process is comrised
 *      of the following steps:
 *      1. Power on the chip and send the chip init script (only if BT radio is not on at the moment)
 *      2. Power on the FM module in the chip
 *      3. Send the FM Init Script
 *      4. Apply default values that are defined in fm_config.h
 *      5. Apply default values in the FM RX configuraion script (if such a file exists)
 *
 *      The application may call any FM RX function only after successfuly completion
 *      of all the steps listed above.
 *
 *      If, for any reason, the application decides to abort the enabling process, it should call
 *      FM_RX_Disable() any time. In that case, a single event will be sent to the application
 *      when disabling completes (there will NOT be an event for enabling)
 *
 *      EITHER FM RX or FM TX MAY BE ENABLED CONCURRENTLY, BUT NOT BOTH
 *
 * Caveats:
 *      When the receiver is enabled, it is not locked on any frequency. To actually
 *      start reception, either FM_RX_Tune() or FM_RX_Seek() must be called.
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_ENABLE
 *
 * Type:
 *      Synchronous/Asynchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
 *
 *      FM_RX_STATUS_FM_TX_ALREADY_ENABLED - FM TX already enabled, only one a time allowed
 * 
 *      FM_RX_STATUS_IN_PROGRESS - This function is already in progress 
 *
 *      FM_RX_STATUS_CONTEXT_ALREADY_ENABLED - The context is already enabled. Please disable it first.
 */

FmRxStatus FM_RX_Enable(FmRxContext *fmContext)
{
    FmRxStatus      status;
    FmRxGenCmd * enableCmd = NULL;
    FMC_FUNC_START_AND_LOCK(("FM_RX_Enable"));
    
    FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("FM_RX_Enable: Invalid Context Ptr"));
    FMC_VERIFY_ERR((FM_RX_SM_GetContextState() ==  FM_RX_SM_CONTEXT_STATE_DISABLED), 
                        FM_RX_STATUS_CONTEXT_NOT_DISABLED, ("FM_RX_Enable: FM RX Context Is Not Disabled"));
    FMC_VERIFY_ERR((FM_RX_SM_IsCmdPending(FM_RX_CMD_ENABLE) == FMC_FALSE), FM_RX_STATUS_IN_PROGRESS,
                        ("FM_RX_Enable: Enabling Already In Progress"));
    
    /* When we are disabled there must not be any pending commands in the queue */
    FMC_VERIFY_FATAL((FMC_IsListEmpty(FMCI_GetCmdsQueue()) == FMC_TRUE), FMC_STATUS_INTERNAL_ERROR,
                        ("FM_RX_Enable: Context is Disabled but there are pending command in the queue"));
    
    /* 
        Direct FM task events to the RX's event handler.
        This must be done here (and not in fm_rx_sm.c) since only afterwards
        we will be able to send the event the event handler
    */
    status = FMCI_SetEventCallback(Fm_Rx_Stack_EventCallback);
    FMC_VERIFY_FATAL((status == FMC_STATUS_SUCCESS), status, 
                        ("FM_RX_Enable: FMCI_SetEventCallback Failed (%s)", FMC_DEBUG_FmcStatusStr(status)));
    
    /* Allocates the command and insert to commands queue */
    status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, FM_RX_CMD_ENABLE, (FmcBaseCmd**)&enableCmd);
    FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, ("FM_RX_Enable"));
    
    
    status = FM_RX_STATUS_PENDING;
    
    FMC_FUNC_END_AND_UNLOCK();
    /* Trigger RX SM to execute the command in FM Task context */
    FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
    
    
    return status;
    
}
/*-------------------------------------------------------------------------------
 * FM_RX_Disable()
 *
 * Brief:  
 *      Disable FM RX
 *
 * Description:
 *      Performs an ordely disabling of the FM RX module.
 * 
 *      FM RX settings will be lost and must be re-configured after the next FM RX power on (via FM_RX_Enable()).
 *
 *      If this function is called while an FM_RX_Enable() is in progress, it will abort the enabling process.
 *
 * Generated Events:
 *      1. If the function is asynchronous (aborts enabling process), an event
 *          with event type==FM_RX_EVENT_CMD_DONE,  command type==FM_RX_CMD_DISABLE
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_SUCCESS - Operation completed successfully.
 *
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
 * 
 *      FM_RX_STATUS_IN_PROGRESS - This function is already in progress 
 */
FmRxStatus FM_RX_Disable(FmRxContext *fmContext)
{
    FmRxStatus      status;
    FmRxGenCmd * disableCmd = NULL;
    
    FMC_FUNC_START(("FM_RX_Disable"));
    FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("FM_RX_Disable: Invalid Context Ptr"));
    
    status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, FM_RX_CMD_DISABLE, (FmcBaseCmd**)&disableCmd);
    FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, ("FM_RX_Disable"));

    status = FM_RX_STATUS_PENDING;

    FMC_FUNC_END();
    
    /* Trigger TX SM to execute the command in FM Task context */
    FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);

    return status;
}




/*-------------------------------------------------------------------------------
 * FM_RX_SetBand()
 *
 * Brief:  
 *      Set the radio band.
 *
 * Description:
 *      Choose between Europe/US band (87.5-108MHz) and Japan band (76-90MHz).
 *
 * Default Values: 
 *      Europe/US
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_BAND
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 *      band [in] -     the band to set
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled 
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_SetBand(FmRxContext *fmContext, FmcBand band)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                band,
                                ((FMC_BAND_JAPAN==band)||(FMC_BAND_EUROPE_US == band)),
                                FM_RX_CMD_SET_BAND,
                                "FM_RX_SetBand");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetBand()
 *
 * Brief:  
 *      Returns the current band.
 *
 * Description:
 *      Retrieves the currently set band
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_BAND
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_GetBand(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_BAND,
                                "FM_RX_GetBand");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetMonoStereoMode()
 *
 * Brief:  
 *      Set the Mono/Stereo mode.
 *
 * Description:
 *      Set the FM RX to Mono or Stereo mode. If stereo mode is set, but no stereo is detected then mono mode will be chosen.
 *      If mono will be set then mono output will be forced.When change will be detected(in stereo mode) MONO <-> STEREO 
 *      FM_RX_EVENT_MONO_STEREO_MODE_CHANGED event will be recieved.
 *
 * Default Values: 
 *      Stereo
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_MONO_STEREO_MODE
 *      2. Event type==FM_RX_EVENT_MONO_STEREO_MODE_CHANGED, with command type == FM_RX_CMD_NONE
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 *      mode [in] - The mode to set: FM_STEREO_MODE or FM_MONO_MODE.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_SetMonoStereoMode(FmRxContext *fmContext, FmRxMonoStereoMode mode)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                mode,
                                ((FM_RX_MONO_MODE==mode)||(FM_RX_STEREO_MODE== mode)),
                                FM_RX_CMD_SET_MONO_STEREO_MODE,
                                "FM_RX_SetMonoStereoMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetMonoStereoMode()
 *
 * Brief: 
 *      Returns the current Mono/Stereo mode.
 *
 * Description:
 *      Returns the current Mono/Stereo mode.
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_MONO_STEREO_MODE
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_GetMonoStereoMode(FmRxContext *fmContext)
{

    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_MONO_STEREO_MODE,
                                "FM_RX_GetMonoStereoMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetMuteMode()
 *
 * Brief:  
 *      Set the mute mode.
 *
 * Description:
 *      Sets the radio mute mode:
 *      mute: Audio is muted completely
 *      attenuate: Audio is attenuated
 *      not muted: Audio volume is set according to the configured volume level
 *
 *      The volume level is unaffected by this function.
 *
 * Default Values: 
 *      Not Muted
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_MUTE_MODE
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 *      mode [in] - the mode to set
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

 
FmRxStatus FM_RX_SetMuteMode(FmRxContext *fmContext, FmcMuteMode mode)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                mode,
                                ((FMC_MUTE==mode)||(FMC_ATTENUATE== mode)||(FMC_NOT_MUTE== mode)),
                                FM_RX_CMD_SET_MUTE_MODE,
                                "FM_RX_SetMuteMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetMuteMode()
 *
 * Brief:  
 *      Returns the current mute mode.
 *
 * Description:
 *      Returns the current mute mode
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_MUTE_MODE
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_GetMuteMode(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_MUTE_MODE,
                                "FM_RX_GetMuteMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetRfDependentMute()
 *
 * Brief:  
 *      Enable/Disable the RF dependent mute feature.
 *
 * Description:
 *      This function turns on or off the RF-Dependent Mute mode. Turning it on causes
 *      the audio to be attenuated according to the RSSI. The lower the RSSI the bigger
 *      the attenuation.
 *
 *      This function doesn't affect the mute mode set via FM_RX_SetMuteMode()
 *
 * Default Values: 
 *      Turned Off
 
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_RF_DEPENDENT_MUTE_MODE
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 *      mode [in] - The mute mode
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_SetRfDependentMuteMode(FmRxContext *fmContext, FmRxRfDependentMuteMode mode)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                mode,
                                ((FM_RX_RF_DEPENDENT_MUTE_ON==mode)||(FM_RX_RF_DEPENDENT_MUTE_OFF== mode)),
                                FM_RX_CMD_SET_RF_DEPENDENT_MUTE_MODE,
                                "FM_RX_SetRfDependentMuteMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetRfDependentMute()
 *
 * Brief:  
 *      Returns the current RF-Dependent mute mode.
 *
 * Description:
 *      Returns the current RF-Dependent mute mode
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_RF_DEPENDENT_MUTE_MODE
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_GetRfDependentMute(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_RF_DEPENDENT_MUTE_MODE,
                                "FM_RX_GetRfDependentMute");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetRssiThreshold()
 *
 * Brief:  
 *      Sets the RSSI tuning threshold
 *
 * Description:
 *      Received Signal Strength Indication (RSSI) is a measurement of the power present in a 
 *      received radio signal. RSSI levels range from 1 (no signal) to 127 (maximum power). 
 *
 *      This function sets the RSSI threshold level. Valid threshold values are from (1X1.5051 dBuV) - (127X1.5051 dBuV)
 *
 *      The threshold is used in the following processes:
 *      1. When seeking a station (FM_RX_Seek()). The received RSSI must be >= threshold for 
 *          a frequency to be considered as a valid station.
 *      2. In the AF process. Switching to an AF is triggered when the RSSI of the tuned frequency
 *          falls below the RSSI threshold.
 *
 * Default Values: 
 *      7X1.5051 dBuV
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_RSSI_THRESHOLD
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 *      threshold [in] - The threshold level should be Bigger then (0).
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_SetRssiThreshold(FmRxContext *fmContext, FMC_INT threshold)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                threshold,
                                ((threshold >= 1)&&(threshold <= 127)),
                                FM_RX_CMD_SET_RSSI_THRESHOLD,
                                "FM_RX_SetRssiThreshold");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetRssiThreshold()
 *
 * Brief:  
 *      Returns the current RSSI threshold.
 *
 * Description:
 *      Returns the current RSSI threshold
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_RSSI_THRESHOLD
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_GetRssiThreshold(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_RSSI_THRESHOLD,
                                "FM_RX_GetRssiThreshold");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetDeEmphasisFilter()
 *
 * Brief: 
 *      Selects the receiver's de-emphasis filter
 *
 * Description:
 *      The FM transmitting station uses a pre-emphasis filter to provide better S/N ratio, 
 *      and the receiver uses a de-emphasis filter corresponding to the transmitter to restore 
 *      the original audio. This varies per country. Therefore the filter has to configure the 
 *      appropriate filter accordingly.
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_DEEMPHASIS_FILTER
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 *      filter [in] - the de-emphasis filter to use
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_NOT_SUPPORTED - The requested filter value is not supported
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_SetDeEmphasisFilter(FmRxContext *fmContext, FmcEmphasisFilter filter)
{

    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                filter,
                                ((FMC_EMPHASIS_FILTER_NONE==filter)||(FMC_EMPHASIS_FILTER_75_USEC==filter)||(filter== FMC_EMPHASIS_FILTER_50_USEC)),
                                FM_RX_CMD_SET_DEEMPHASIS_FILTER,
                                "FM_RX_SetDeEmphasisFilter");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetDeEmphasisFilter()
 *
 * Brief:  
 *      Returns the current de-emphasis filter
 *
 * Description:
 *      Returns the current de-emphasis filter
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_DEEMPHASIS_FILTER
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_GetDeEmphasisFilter(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_DEEMPHASIS_FILTER,
                                "FM_RX_GetDeEmphasisFilter");
}


/*-------------------------------------------------------------------------------
 * FM_RX_SetVolume()
 *
 * Brief:  
 *      Set the gain level of the audio left & right channels
 *
 * Description:
 *      Set the gain level of the audio left & right channels, 0 - 70 (0 is no gain, 70 is maximum gain)
 *
 * Default Values: 
 *      TBD
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_VOLUME
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 *      level [in] - the level to set: 0 (weakest) - 70 (loudest).
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_SetVolume(FmRxContext *fmContext, FMC_UINT level)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                level,
                                (level<=70),
                                FM_RX_CMD_SET_VOLUME,
                                "FM_RX_SetVolume");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetVolume()
 *
 * Brief:  
 *      Returns the current volume level.
 *
 * Description:
 *      Returns the current volume level
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_VOLUME
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_GetVolume(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_VOLUME,
                                "FM_RX_GetVolume");
}

/*-------------------------------------------------------------------------------
 * FM_RX_Tune()
 *
 * Brief:  
 *      Tune the receiver to the specified frequency
 *
 * Description:
 *      Tune the receiver to the specified frequency. This is useful when setting the receiver
 *      to a preset station. The receiver will be tuned to the specified frequency regardless
 *      of any signal present.
 *
 *      Calling this function stops other frequency related processes in progress (seek, switch to AF)
 *      In that case, the application will receive a single event, reporting the tune result. The application
 *      will NOT recreive any event regarding the process that is being stopped.
 *
 * Default Values: 
 *      NO DEFAULT VALUE AVAILABLE
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_TUNE
 *
 * Type:
 *      Synchronous/Asynchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 *      freq [in] - the frequency to set in KHz.
 *
 *                  The value must match the configured band.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_Tune(FmRxContext *fmContext, FmcFreq freq)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                freq,
                                ((FMC_FIRST_FREQ_JAPAN_KHZ<=freq)&&(FMC_LAST_FREQ_US_EUROPE_KHZ>=freq)),
                                FM_RX_CMD_TUNE,
                                "FM_RX_Tune");
}

/*-------------------------------------------------------------------------------
 * FM_RX_IsValidChannel()
 *
 * Brief:  
 *      Is the channel valid.
 *
 * Description:
 *      
 *           Verify that the tune channel is valid.Acorrding to 2 parameters:
 *              1.Channel that was set via the API FM_RX_Tune()
 *              2. RSSI search level that was set via the API FM_RX_SetRssiThreshold
 *                 Deafult RSSI value is : 7.                  
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_IS_CHANNEL_VALID
 *
 * CMD_DONE Value:
 *
 *                     1 = Valid.
 *                     0 = Not Valid.
 *
 *
 * Type:
 *      Synchronous/Asynchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_IsValidChannel(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_IS_CHANNEL_VALID,
                                "FM_RX_IsValidChannel");
}


/*-------------------------------------------------------------------------------
 * FM_RX_GetTunedFrequency()
 *
 * Brief:  
 *      Returns the currently tuned frequency.
 *
 * Description:
 *      Returns the currently tuned frequency.
 *
 * Caveats:
 *      TBD: What frequency to return when AF switch / seek are executing or pending?
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_TUNED_FREQUENCY
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_NO_VALUE_AVAILABLE - Frequency was never set (first call FM_RX_Tune())
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_GetTunedFrequency(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_TUNED_FREQUENCY,
                                "FM_RX_GetTunedFrequency");
}

/*-------------------------------------------------------------------------------
 * FM_RX_Seek()
 *
 * Brief: 
 *      Seeks the next good station or stop the current seek.
 *
 * Description:
 *      Seeks up/down for a frequency with an RSSI that is higher than the configured
 *      RSSI threshold level (set via FM_RX_SetRssiThreshold()).
 *
 *      New frequencies are scanned starting from the currently tuned frequency, and
 *      progressing according to the specified direction, in steps of 100 kHz.
 *
 *      When this function is called for the first time after enabling FM RX (via
 *      FM_RX_Enable()), seeking will begin from the beginning of the band limit.
 *
 *      The seek command stops when one of the following conditions occur:
 *      1. A frequency with compliant RSSI was found. That frequency will be the new
 *          tuned frequency.
 *      2. The band limit was reached. The band limit will be the new tuned frequency.
 *      3. FM_RX_StopSeek() was called. The last scanned frequency will be the new
 *          tuned frequency.
 *      4. TBD: FM_RX_Tune() was called. The new tuned frequency will be the one specified
 *          in the FM_RX_Tune() call.
 *      
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SEEK, and
 *          a. If the seek completed successfully, the status will be FM_RX_STATUS_SUCCESS, and
 *              value== newly tuned frequency
 *          b. If the seek stopped because the band limit was reached, the status will be
 *              FM_RX_STATUS_SEEK_REACHED_BAND_LIMIT, and value==band limit frequency.
 *
 *      If the seek stopped due to a FM_RX_StopSeek() command that was issued, a single
 *      event will be issued as described in the FM_RX_StopSeek() description.
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 *      direction [in] - The seek direction: FM_RX_DIR_DOWN or FM_RX_DIR_UP.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_Seek(FmRxContext *fmContext, FmRxSeekDirection direction)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                direction,
                                ((direction == FM_RX_SEEK_DIRECTION_DOWN )||(direction == FM_RX_SEEK_DIRECTION_UP)),
                                FM_RX_CMD_SEEK,
                                "FM_RX_Seek");
}

/*-------------------------------------------------------------------------------
 * FM_RX_StopSeek()
 *
 * Brief: 
 *      Stops a seek in progress
 *
 * Description:
 *      Stops a seek that was started via FM_RX_Seek()
 *
 *      The new tuned frequency will be the last scanned frequency. The new frequency
 *      will be indicated to the application via the generated event.
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_STOP_SEEK. 
 *          command value == newly tuned frequency (last scanned frequency)
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_SEEK_IS_NOT_IN_PROGRESS - No seek currently in progress
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_StopSeek(FmRxContext *fmContext)
{

    FmRxStatus status = FM_RX_STATUS_PENDING;

    /* Lock the stack */
    _FM_RX_FUNC_START_AND_LOCK_ENABLED("FM_RX_StopSeek");
    
    FMC_UNUSED_PARAMETER(fmContext);
    
    if(FM_RX_SM_GetRunningCmd() == FM_RX_CMD_SEEK)
    {
        FM_RX_SM_SetUpperEvent( FM_RX_SM_UPPER_EVENT_STOP_SEEK);
        FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
    }
    else
    {
        status = FM_RX_STATUS_SEEK_IS_NOT_IN_PROGRESS;
    }
    _FM_RX_FUNC_END_AND_UNLOCK_ENABLED();
    return status;
}

/*-------------------------------------------------------------------------------
 * FM_RX_CompleteScan()
 *
 * Brief: 
 *      Perfrom a complete Scan on the Band.
 *
 * Description:
 *      
 *      Perfrom a complete Scan on the Band and return a list of frequnecies that are valid acorrding to the RSSI.
 *
 * Generated Events:
 *      Event type==FM_RX_EVENT_COMPLETE_SCAN_DONE.
 *      
 *
 * Relevant Data:
 *
 *       "p.completeScanData"
 *       "p.completeScanData.numOfChannels" - contains the number of channels that was found in the Scan.
 *       "p.completeScanData.channelsData" - Contains the Freq in MHz for each channel.
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_CompleteScan(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_COMPLETE_SCAN,
                                "FM_RX_CompleteScan");
}


/*-------------------------------------------------------------------------------
 * FM_RX_GetCompleteScanProgress()
 *
 * Brief: 
 *      Get the progress from the Complte Scan.
 *
 * Description:
 *      
 *      Get the progress from the Complte Scan.This function will be valid only after calling the FM_RX_CompleteScan()
 *      When calling this function without calling the FM_RX_CompleteScan() it will return FM_RX_STATUS_COMPLETE_SCAN_IS_NOT_IN_PROGRESS.
 *
 * Generated Events:
 *      Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_COMPLETE_SCAN_PROGRESS. 
 *      command value = US band if you read back 87500 MHz it would be 0% and 108000 MHz would be 100%. 
 *                                 JAPAN band if you read back 76000 MHz it would be 0% and 90000 MHz would be 100%. 
 *      
 *
 * Relevant Data:
 *
 *       "p.cmdDone"
 *       "p.cmdDone.cmd" - FM_RX_CMD_COMPLETE_SCAN_PROGRESS cmd type.
 *       "p.cmdDone.value" - Contains Freq Progress in MHz .
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *
 *
 *      FM_RX_STATUS_COMPLETE_SCAN_IS_NOT_IN_PROGRESS - No Complete Scan currently in progress
 *
 */

FmRxStatus FM_RX_GetCompleteScanProgress(FmRxContext *fmContext)
{
  
    FmRxStatus status = FM_RX_STATUS_PENDING;

    /* Lock the stack */
    _FM_RX_FUNC_START_AND_LOCK_ENABLED("FM_RX_GetCompleteScanProgress");
    
    FMC_UNUSED_PARAMETER(fmContext);
    
    if(FM_RX_SM_GetRunningCmd() == FM_RX_CMD_COMPLETE_SCAN)
    {
        FM_RX_SM_SetUpperEvent( FM_RX_SM_UPPER_EVENT_GET_COMPLETE_SCAN_PROGRESS);
        FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
    }
    else
    {
        status = FM_RX_STATUS_COMPLETE_SCAN_IS_NOT_IN_PROGRESS;
    }
    _FM_RX_FUNC_END_AND_UNLOCK_ENABLED();
    return status;
}

/*-------------------------------------------------------------------------------
 * FM_RX_StopCompleteScan()
 *
 * Brief: 
 *      Stop the Complte Scan.
 *
 * Description:
 *      
 *      Stop the Complte Scan.This function will be valid only after calling the FM_RX_CompleteScan()
 *      When calling this function without calling the FM_RX_CompleteScan() it will return FM_RX_STATUS_COMPLETE_SCAN_IS_NOT_IN_PROGRESS.
 *
 * Generated Events:
 *      Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_STATUS_COMPLETE_SCAN_STOPPED. 
 *      command value = Frequency in MHz pirior to the FM_RX_CompleteScan() request.
 *
 * Relevant Data:
 *
 *       "p.cmdDone"
 *       "p.cmdDone.cmd" - FM_RX_STATUS_COMPLETE_SCAN_STOPPED cmd type.
 *       "p.cmdDone.value" - Contains Freq in MHz that pirior to the Complete Scan.
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_COMPLETE_SCAN_IS_NOT_IN_PROGRESS - No Complete Scan currently in progress
 *
 */
FmRxStatus FM_RX_StopCompleteScan(FmRxContext *fmContext)
{
  
    FmRxStatus status = FM_RX_STATUS_PENDING;

    /* Lock the stack */
    _FM_RX_FUNC_START_AND_LOCK_ENABLED("FM_RX_StopCompleteScan");
    
    FMC_UNUSED_PARAMETER(fmContext);
    
    if(FM_RX_SM_IsCompleteScanStoppable())
    {
        FM_RX_SM_SetUpperEvent( FM_RX_SM_UPPER_EVENT_STOP_COMPLETE_SCAN);
        FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
    }
    else
    {
        status = FM_RX_STATUS_COMPLETE_SCAN_IS_NOT_IN_PROGRESS;
    }
    _FM_RX_FUNC_END_AND_UNLOCK_ENABLED();
    return status;
}


/*-------------------------------------------------------------------------------
 * FM_RX_StopCompleteScan()
 *
 * Brief: 
 *      Stop the Complte Scan.
 *
 * Description:
 *      
 *      Stop the Complte Scan.This function will be valid only after calling the FM_RX_CompleteScan()
 *      When calling this function without calling the FM_RX_CompleteScan() it will return FM_RX_STATUS_COMPLETE_SCAN_IS_NOT_IN_PROGRESS.
 *
 * Generated Events:
 *      Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_STATUS_COMPLETE_SCAN_STOPPED. 
 *      command value = Frequency in MHz pirior to the FM_RX_CompleteScan() request.
 *
 * Relevant Data:
 *
 *       "p.cmdDone"
 *       "p.cmdDone.cmd" - FM_RX_STATUS_COMPLETE_SCAN_STOPPED cmd type.
 *       "p.cmdDone.value" - Contains Freq in MHz that pirior to the Complete Scan.
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_COMPLETE_SCAN_IS_NOT_IN_PROGRESS - No Complete Scan currently in progress
 *
 */

FmRxStatus FM_RX_GetRssi(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_RSSI,
                                "FM_RX_GetRssi");
}

/*-------------------------------------------------------------------------------
 * FM_RX_EnableRds()
 *
 * Brief:  
 *      Enables RDS reception.
 *
 * Description:
 *      Starts RDS data reception. RDS data reception will be active when the receiver is actually tuned to some frequency.
 *      Only when RDS enabled , RDS field (PI,AF,PTY,PS,RT,repertoire,...)will be parsed.
 *
 * Default Values: 
 *      RDS Is disabled
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_ENABLE_RDS
 *      2. Event type==FM_RX_EVENT_PI_CODE_CHANGED, with command type == FM_RX_CMD_NONE
 *      3. Event type==FM_RX_EVENT_AF_LIST_CHANGED, with command type == FM_RX_CMD_NONE
 *      4. Event type==FM_RX_EVENT_PS_CHANGED, with command type == FM_RX_CMD_NONE
 *      5. Event type==FM_RX_EVENT_RADIO_TEXT, with command type == FM_RX_CMD_NONE
 *      6. Event type==FM_RX_EVENT_RAW_RDS, with command type == FM_RX_CMD_NONE
 *      7. Event type==FM_RX_EVENT_PTY_CODE_CHANGED, with command type == FM_RX_CMD_NONE
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_EnableRds(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_ENABLE_RDS,
                                "FM_RX_EnableRds");
}

/*-------------------------------------------------------------------------------
 * FM_RX_DisableRds()
 *
 * Brief:  
 *      Disables RDS reception.
 *
 * Description:
 *      Stops RDS data reception.
 *
 *      All RDS configurations are retained.
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_DISABLE_RDS
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_DisableRds(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_DISABLE_RDS,
                                "FM_RX_DisableRds");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetRdsSystem()
 *
 * Brief: 
 *      Choose the RDS mode - RDS/RBDS
 *
 * Description:
 *      RDS is the system used in Europe.  RBDS is a new system used in the US (although most stations 
 *      in the US transmit RDS, a few have started to transmit RBDS).  RBDS includes all RDS specifications, 
 *      with some additional information.
 *
 * Default Values: 
 *      RDS 
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_RDS_SYSTEM
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 *      rdsSystem [in] - The RDS system to use.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_SetRdsSystem(FmRxContext *fmContext, FmcRdsSystem  rdsSystem)
{
    
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                rdsSystem,
                                ((rdsSystem==FM_RDS_SYSTEM_RBDS)||(rdsSystem==FM_RDS_SYSTEM_RDS)),
                                FM_RX_CMD_SET_RDS_SYSTEM,
                                "FM_RX_SetRdsSystem");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetRdsSystem()
 *
 * Brief:  
 *      Returns the current RDS System in use.
 *
 * Description:
 *      Returns the current RDS System in use
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_RDS_SYSTEM
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_GetRdsSystem(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_RDS_SYSTEM,
                                "FM_RX_GetRdsSystem");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetRdsGroupMask()
 *
 * Brief: 
 *      Selects which RDS gorups to report in a raw RDS event
 *
 * Description:
 *      The application will be called back whenever RDS data of one of the requested types is received.
 *
 *      To disable these notifications, FM_RDS_GROUP_TYPE_MASK_NONE should be specified.
 *      To receive an event for any group, FM_RDS_GROUP_TYPE_MASK_ALL should be specified.
 *
 * Default Values: 
 *      ALL (FM_RDS_GROUP_TYPE_MASK_ALL)
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_RDS_GROUP_MASK
 *
 * Type:
 *      Asynchronous\Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 *      groupMask [in] - Group mask
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 */
FmRxStatus FM_RX_SetRdsGroupMask(FmRxContext *fmContext, FmcRdsGroupTypeMask groupMask)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                groupMask,
                                FMC_TRUE,
                                FM_RX_CMD_SET_RDS_GROUP_MASK,
                                "FM_RX_SetRdsGroupMask");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetRdsGroupMask()
 *
 * Brief: 
 *      Returns the current RDS group mask
 *
 * Description:
 *      Returns the current RDS group mask
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_RDS_GROUP_MASK
 *
 * Type:
 *      Asynchronous\Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 *      groupMask [out] - The current group mask
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 */

FmRxStatus FM_RX_GetRdsGroupMask(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_RDS_GROUP_MASK,
                                "FM_RX_GetRdsGroupMask");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetRdsAFSwitchMode()
 *
 * Brief: 
 *      Sets the RDS AF feature On or Off
 *
 * Description:
 *      Turns the 'Alternative Frequency' (AF) automatic switching feature of the RDS on or off. 
 *
 *      When RDS is enabled, the receiver obtains, via RDS, a list of one or more alternate
 *      frequencies published by the currently tuned station (if available).
 *      
 *      When AF switching is turned on, and the RSSI falls beneath the configured RSSI threshold level
 *      (configured via FM_RX_SetRssiThreshold()), an attempt will be made to switch to
 *      the first frequency on the list, then to the second, and so on. The process stops
 *      when one of the following conditions are met:
 *      1. The attempt succeeded. In that case the newly tuned frequency is the alternate frequency.
 *      2. The list is exhausted. In that case, the tuned frequency is unaltered.
 *
 *      An attempt to switch is successful if all of the following conditions are met:
 *      1. The RSSI of the alternate frequency is greater than or equal to the RSSI threshold level.
 *      2. The RDS PI (Program Indicator) of the alternate frequency matches that of the tuned
 *          frequency.
 *
 *      TBD: How to handle seek and tune commands that are issued while AF is in progress?
 *
 * Caveats:
 *      AF automatic switching can operate only when RDS reception is enabled (via FM_RX_EnableRds()) 
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_RDS_AF_SWITCH_MODE
 *      2. Event type==FM_RX_EVENT_AF_SWITCH_COMPLETE, with command type == FM_RX_CMD_NONE
 *      3. Event type==FM_RX_EVENT_AF_SWITCH_TO_FREQ_FAILED, with command type == FM_RX_CMD_NONE
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM context.
 *
 *      mode [in] - AF feature On or Off
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_SetRdsAfSwitchMode(FmRxContext *fmContext, FmRxRdsAfSwitchMode mode)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                mode,
                                ((mode==FM_RX_RDS_AF_SWITCH_MODE_ON)||(mode==FM_RX_RDS_AF_SWITCH_MODE_OFF)),
                                FM_RX_CMD_SET_RDS_AF_SWITCH_MODE,
                                "FM_RX_SetRdsAfSwitchMode");
}
/*-------------------------------------------------------------------------------
 * FM_RX_GetRdsAFSwitchMode()
 *
 * Brief:  
 *      Returns the current RDS AF Mode.
 *
 * Description:
 *      Returns the current RDS AF Mode
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_RDS_AF_SWITCH_MODE
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_GetRdsAfSwitchMode(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_GET_RDS_AF_SWITCH_MODE,
                                "FM_RX_GetRdsAfSwitchMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetChannelSpacing()
 *
 * Brief:  
 *      Sets the Seek channel spacing interval.
 *
 * Description:
 *      When performing seek operation there is in interval spacing between 
 *      the stations that are searched.
 *
 * Default Value: 
 *      FMC_CHANNEL_SPACING_100_KHZ.
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_SET_CHANNEL_SPACING
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *      channelSpacing [in] - Channel spacing.
 *
 * Returns:
 * 
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_SetChannelSpacing(FmRxContext *fmContext,FmcChannelSpacing channelSpacing)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                channelSpacing,
                                ((FMC_CHANNEL_SPACING_50_KHZ==channelSpacing)||
                                                          (FMC_CHANNEL_SPACING_100_KHZ== channelSpacing)||
                                                           (FMC_CHANNEL_SPACING_200_KHZ== channelSpacing)),
                                FM_RX_CMD_SET_CHANNEL_SPACING,
                                "FM_RX_SetChannelSpacing");
}


/*-------------------------------------------------------------------------------
 * FM_RX_GetChannelSpacing()
 *
 * Brief:  
 *      Gets the Seek channel spacing interval.
 *
 * Description:
 *      Gets the Seek channel spacing interval.
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_CHANNEL_SPACING
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 * 
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */

FmRxStatus FM_RX_GetChannelSpacing(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext,                      
                                FM_RX_CMD_GET_CHANNEL_SPACING,
                                "FM_RX_GetChannelSpacing");
}


/*-------------------------------------------------------------------------------
 * FM_RX_GetFwVersion()
 *
 * Brief:  
 *      Gets the FW version.
 *
 * Description:
 *      Gets the FW version.
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_GET_FW_VERSION
 *
 * CMD_DONE Value in Hex:
 *                                When read in decimal, the digit in
 *                                 thousands place indicated ROM version,
 *                                 digits in hundreds and tens place indicate
 *                                 Patch version and the digit in units place
 *                                 indicates Minor Derivative
 *             
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_GetFwVersion(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext,                      
                                FM_RX_CMD_GET_FW_VERSION,
                                "FM_RX_GetFwVersion");
}


/*-------------------------------------------------------------------------------
 * FM_RX_ChangeAudioTarget()
 *
 * Brief:  
 *      Changes the Audio Targets.
 *
 * Description:
 *      Changes the Audio Targets. This command can be called both when Audio enabled and when it is disabled.
 *      
 *      If Audio Routing is disabled it associates the FM operation with the resources requested in the VAC - 
 *      and the it should succeed if the resource was defined in the optional resources of the FM RX opperation 
 *      (it will not occupy the resources).
 *      
 *      If Audio Routing is enabled  it associates the FM operation with the resources requested in the VAC -
 *      this operation will occupy the resources and therefore can fail if the resources are unavailble, In this  
 *      case the unavailible resource list will be returned in the FM_RX_EVENT_CMD_DONE event with the 
 *      status code FM_RX_STATUS_AUDIO_OPERATION_UNAVAILIBLE_RESOURCES.
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_CHANGE_AUDIO_TARGET
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 *      rxTagets[in] - The requested Audio Targets Mask.
 *
 *      digitalConfig[in] - The digital configuration (will be used only if digital target requested in the rxTagets).
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 *      FM_RX_STATUS_AUDIO_OPERATION_UNAVAILIBLE_RESOURCES - Resources requested by operation unavailible.
 */

FmRxStatus FM_RX_ChangeAudioTarget (FmRxContext *fmContext, FmRxAudioTargetMask rxTagets, ECAL_SampleFrequency eSampleFreq)
{
    FmRxStatus      status;
    FmRxSetAudioTargetCmd   *setAudioTargetCmd = NULL;
    
    _FM_RX_FUNC_START_AND_LOCK_ENABLED("FM_RX_ChangeAudioTarget");
    FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("FM_RX_ChangeAudioTarget: Invalid Context Ptr"));
    
    /* Verify that the band value is valid */
    /*FMC_VERIFY_ERR(condition, 
                        FM_RX_STATUS_INVALID_PARM, ("%s: Invalid param",funcName));*/
    
    /* Allocates the command and insert to commands queue */
    status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, 
                                                FM_RX_CMD_CHANGE_AUDIO_TARGET, 
                                                (FmcBaseCmd**)&setAudioTargetCmd);
    FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, ("FM_RX_ChangeAudioTarget"));
    
    
    /* Copy cmd parms for the cmd execution phase*/
    setAudioTargetCmd->rxTargetMask= rxTagets;
    setAudioTargetCmd->eSampleFreq = eSampleFreq;
    
    status = FM_RX_STATUS_PENDING;

    /* Trigger TX SM to execute the command in FM Task context */
    FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
    
    _FM_RX_FUNC_END_AND_UNLOCK_ENABLED();
    
    return status;
}
/*-------------------------------------------------------------------------------
 * FM_RX_ChangeDigitalTargetConfiguration()
 *
 * Brief:  
 *      Change the digital audio configuration. 
 *
 * Description:
 *
 *      Change the digital audio configuration. This function should be called only if a 
 *      digital audio target was set (by defualt or by a call to FM_RX_ChangeAudioTarget).
 *      The digital configuration will apply on every digital target set (if few will be possible in the future). 
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_CHANGE_DIGITAL_AUDIO_CONFIGURATION
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *      digitalConfig[in] - sample frequency for the digital resources.
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_ChangeDigitalTargetConfiguration(FmRxContext *fmContext, ECAL_SampleFrequency eSampleFreq)
{
    FmRxStatus      status;
    FmRxSetDigitalAudioConfigurationCmd     *setDigitalTargetConfigurationCmd = NULL;
    
    _FM_RX_FUNC_START_AND_LOCK_ENABLED("FM_RX_ChangeAudioTarget");
    FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("FM_RX_ChangeAudioTarget: Invalid Context Ptr"));
    
    /* Verify that the band value is valid */
    /*FMC_VERIFY_ERR(condition, 
                        FM_RX_STATUS_INVALID_PARM, ("%s: Invalid param",funcName));*/
    
    /* Allocates the command and insert to commands queue */
    status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, 
                                                    FM_RX_CMD_CHANGE_DIGITAL_AUDIO_CONFIGURATION, 
                                                    (FmcBaseCmd**)&setDigitalTargetConfigurationCmd);
    FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, ("FM_RX_ChangeAudioTarget"));
    
    
    /* Copy cmd parms for the cmd execution phase*/
    setDigitalTargetConfigurationCmd->eSampleFreq = eSampleFreq;
    
    status = FM_RX_STATUS_PENDING;
    
    /* Trigger TX SM to execute the command in FM Task context */
    FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
    
    _FM_RX_FUNC_END_AND_UNLOCK_ENABLED();
    
    return status;
        
}
/*-------------------------------------------------------------------------------
 * FM_RX_EnableAudioRouting()
 *
 * Brief:  
 *      Enable the Audio routing.
 *
 * Description:
 *      
 *      Enables the audio routing according to the VAC configuration. if 
 *      FM_RX_ChangeAudioTarget()/FM_RX_ChangeDigitalTargetConfiguration() was called the routing will 
 *      be according to the configurations done by these calles.
 *      
 *      This command will start the VAC operations (and occupy the resources). if digital target was selected to be fm over Sco/A3dp 
 *      
 *      
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_ENABLE_AUDIO
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *      
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 *      FM_RX_STATUS_AUDIO_OPERATION_UNAVAILIBLE_RESOURCES - Resources requested by operation unavailible.
 */

FmRxStatus FM_RX_EnableAudioRouting (FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_ENABLE_AUDIO,
                                "FM_RX_EnableAudioRouting");
}
/*-------------------------------------------------------------------------------
 * FM_RX_DisableAudioRouting()
 *
 * Brief:  
 *      Disables the audio routing.
 *
 * Description:
 *      Disables the audio routing. This command will also Stop the VAC operations (and realese the resources).
 *
 * Generated Events:
 *      1. Event type==FM_RX_EVENT_CMD_DONE, with command type == FM_RX_CMD_DISABLE_AUDIO
 *
 * Type:
 *      Asynchronous/Synchronous
 *
 * Parameters:
 *      fmContext [in] - FM RX context.
 *
 * Returns:
 *      FM_RX_STATUS_PENDING - Operation started successfully, an event will be sent to
 *                              the application upon completion.
 *
 *      FM_RX_STATUS_INVALID_PARM - The function was called  with an invalid parameter
 *
 *      FM_RX_STATUS_CONTEXT_NOT_ENABLED - The context is not enabled
 *
 *      FM_RX_STATUS_TOO_MANY_PENDING_OPERATIONS - Too many operations are already waiting
 *                                                      execution in operations queue.
 */
FmRxStatus FM_RX_DisableAudioRouting (FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_DISABLE_AUDIO,
                                "FM_RX_DisableAudioRouting");
}

/***************************************************************************************************
 *                                                  
 ***************************************************************************************************/


FmRxStatus _FM_RX_SimpleCmdAndCopyParams(FmRxContext *fmContext, 
                                FMC_UINT paramIn,
                                FMC_BOOL condition,
                                FmRxCmdType cmdT,
                                const char * funcName)
{
    FmRxStatus      status;
    FmRxSimpleSetOneParamCmd * baseCmd;

    _FM_RX_FUNC_START_ENABLED(funcName);
    FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("%s: Invalid Context Ptr",funcName));

    /* Verify that the band value is valid */
    FMC_VERIFY_ERR(condition, 
                        FM_RX_STATUS_INVALID_PARM, ("%s: Invalid param",funcName));

    /* Allocates the command and insert to commands queue */
    status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, cmdT, (FmcBaseCmd**)&baseCmd);
    FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, (funcName));
    
    /* Copy cmd parms for the cmd execution phase*/
    /*we assume here strongly that the stract pointed by base command as as its first field base command and another field with the param*/

    baseCmd->paramIn = paramIn;
    
    status = FM_RX_STATUS_PENDING;
    
    /* Trigger TX SM to execute the command in FM Task context */
    FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);

    _FM_RX_FUNC_END_ENABLED();

    return status;
}
FmRxStatus _FM_RX_SimpleCmd(FmRxContext *fmContext, 
                                FmRxCmdType cmdT,
                                const char * funcName)
{
    FmRxStatus      status;
    FmRxSimpleSetOneParamCmd * baseCmd;
    _FM_RX_FUNC_START_ENABLED(funcName);
    FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("%s: Invalid Context Ptr",funcName));

    status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, cmdT, (FmcBaseCmd**)&baseCmd);
    FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, (funcName));

    status = FM_RX_STATUS_PENDING;

    /* Trigger TX SM to execute the command in FM Task context */
    FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);

    _FM_RX_FUNC_END_ENABLED();

    return status;
}
#else  /*FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED*/

FmRxStatus FM_RX_Init(void)
{
    
        return FM_RX_STATUS_SUCCESS;
    
}

FmRxStatus FM_RX_Deinit(void)
{

        return FM_RX_STATUS_SUCCESS;
}


#endif /*FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED*/



