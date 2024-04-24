/*******************************************************************************
 System Tasks File

  File Name:
    thread_demo.c

  Summary:
    This file contains source code necessary to thread demo application.

  Description:
    This file contains source code necessary to thread demo application.
    
  Remarks:
    
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *******************************************************************************/
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
#include <string.h>
#include <stdio.h>
#include "definitions.h"

#include "configuration.h"
#include "openthread/dataset.h"
#include "openthread/dataset_ftd.h"
#include "openthread/instance.h"
#include "openthread/thread.h"
#include "openthread/ip6.h"
#include "openthread/thread_ftd.h"

#include "utils/uart.h"
#include "timers.h"
#include "thread_demo.h"
#include "udp_demo.h"

#include <assert.h>

#define LEADER_NAME ("Leader-")
// *****************************************************************************
// *****************************************************************************

devMsgType_t demoCommand;

extern float SP_Volt;
extern float SP_ldr;

extern otInstance *instance;
otOperationalDataset aDataset;
otDeviceRole state = OT_DEVICE_ROLE_DISABLED;
static TimerHandle_t sAppTimerHandle = NULL;
SYS_CONSOLE_HANDLE appConsoleHandle;
bool dataInitialized = false;
bool deviceStateUpdated = false;

void threadTmrCb(TimerHandle_t pxTimer);

#define TOTAL_DEMO_DEVICES              (10)

void otHandleStateChangecb(otChangedFlags aFlags, void *aContext)
{
    //Check if Device role is changed
    if(aFlags & ((uint32_t)1 << 2))
    {
        APP_Msg_T    appMsg;
        appMsg.msgId = APP_MSG_OT_STATE_HANDLE_EVT;
        OSAL_QUEUE_Send(&appData.appQueue, &appMsg, 0);
//        threadHandleStateChange();
    }
}

void threadAppinit()
{
    APP_Msg_T    appMsg;
    otError err = OT_ERROR_NONE;
    
    appConsoleHandle = SYS_CONSOLE_HandleGet(SYS_CONSOLE_INDEX_0);
    
    err = otSetStateChangedCallback(instance, otHandleStateChangecb, NULL);
    if (err != OT_ERROR_NONE)
    {
       app_printf("App_Err: SetStateChangedCallback failed\n");
        //print error code
        assert(err);
    }
    
    sAppTimerHandle = xTimerCreate("thread app tmr", (APP_BROADCAST_TIMER_INTERVAL_MS / portTICK_PERIOD_MS), true, ( void * ) 0, threadTmrCb);
    if(sAppTimerHandle == NULL)
    {
        app_printf("App_Err: App Timer creation failed\n");
    }
    appMsg.msgId = APP_MSG_OT_NW_CONFIG_EVT;
    OSAL_QUEUE_Send(&appData.appQueue, &appMsg, 0);
}
void threadTmrCb(TimerHandle_t pxTimer)
{
    APP_Msg_T    appMsg;
    appMsg.msgId = APP_MSG_OT_SEND_ADDR_TMR_EVT;
    OSAL_QUEUE_Send(&appData.appQueue, &appMsg, 0);
}
extern devMsgType_t demoMsg;
extern float temp_demo_value;
extern otIp6Address gatewayAddr;
void threadSendIPAddr(void)
{
    if(0 != gatewayAddr.mFields.m32[0])
    {
        devTypeSolarReport_t solarReport;
        solarReport.voltage = SP_Volt;
        solarReport.lightIntensity = SP_ldr;

        demoMsg.msgType = MSG_TYPE_SOLAR_REPORT;
        memcpy(&demoMsg.msg, &solarReport, sizeof(devTypeSolarReport_t));
        threadUdpSend(&gatewayAddr, 4 + sizeof(devTypeSolarReport_t), (uint8_t *)&demoMsg);
        app_printf("App Log:Solar Report\r\n");

    }
}

void threadConfigNwParameters()
{
    APP_Msg_T    appMsg;
#if ENABLE_SLEEP  
    DEVICE_DeepSleepWakeSrc_T wakeupSrc;
    DEVICE_GetDeepSleepWakeUpSrc(&wakeupSrc);
   
    if(wakeupSrc == DEVICE_DEEP_SLEEP_WAKE_NONE  ||  wakeupSrc == DEVICE_DEEP_SLEEP_WAKE_MCLR)
#endif
    {
        otError err = OT_ERROR_NONE;

        memset(&aDataset, 0 , sizeof(otOperationalDataset));
#if DEVICE_AS_LEADER  
        err = otDatasetCreateNewNetwork(instance, &aDataset);
        if (err != OT_ERROR_NONE) {/* Print error message and assert */}
#endif

        aDataset.mChannel = CHANNEL;
        aDataset.mChannelMask = (otChannelMask)(1LU<<CHANNEL); //(otChannelMask)0x7fff800;
        aDataset.mPanId = PANID;

        uint8_t ex_panid[OT_EXT_PAN_ID_SIZE] = EXD_PANID;
        memcpy(aDataset.mExtendedPanId.m8,ex_panid,sizeof(aDataset.mExtendedPanId));

        uint8_t nw_key[OT_NETWORK_KEY_SIZE] = NW_KEY;
        memcpy(aDataset.mNetworkKey.m8,nw_key,sizeof(aDataset.mNetworkKey));

        static char nwk_name[] = NWK_NAME;
        uint8_t nwk_len = strlen(nwk_name);
        memcpy(aDataset.mNetworkName.m8,nwk_name,nwk_len);

        uint8_t ml_prefix[OT_MESH_LOCAL_PREFIX_SIZE] = ML_PREFIX;
        memcpy(aDataset.mMeshLocalPrefix.m8,ml_prefix,sizeof(aDataset.mMeshLocalPrefix));

        aDataset.mComponents.mIsChannelMaskPresent = true;
        aDataset.mComponents.mIsExtendedPanIdPresent = true;
        aDataset.mComponents.mIsMeshLocalPrefixPresent = true;
        aDataset.mComponents.mIsNetworkKeyPresent = true;
        aDataset.mComponents.mIsNetworkNamePresent = true;
        aDataset.mComponents.mIsPanIdPresent = true;
        aDataset.mComponents.mIsChannelPresent = true;

        err = otDatasetSetActive(instance,&aDataset);
        if (err != OT_ERROR_NONE) 
        {
            app_printf("App_Err: Dataset update failed\n");
        }
    }
    otThreadSetRouterSelectionJitter(instance, 5);
    appMsg.msgId = APP_MSG_OT_NWK_START_EVT;
    OSAL_QUEUE_Send(&appData.appQueue, &appMsg, 0);
}

void threadNwStart()
{
    APP_Msg_T    appMsg;
#if ENABLE_SLEEP
    otLinkModeConfig config;
    
    config.mRxOnWhenIdle = false; ///< 1, if the sender has its receiver on when not transmitting. 0, otherwise.
    config.mDeviceType = false;   ///< 1, if the sender is an FTD. 0, otherwise.
    config.mNetworkData = false;  
     
    otThreadSetLinkMode(instance, config);
#endif
    otIp6SetEnabled(instance,true);

    
    if (OT_ERROR_NONE == otThreadSetEnabled(instance,true))
    {
        //printIpv6Address();
        appMsg.msgId = APP_MSG_OT_PRINT_IP_EVT;
        OSAL_QUEUE_Send(&appData.appQueue, &appMsg, 0);
    }
}

void threadHandleStateChange(void)
{
    // get state and check if state is child, yes, print attach successful.
    state = otThreadGetDeviceRole(instance);
    if(state == OT_DEVICE_ROLE_CHILD)
    {
#if OPENTHREAD_MTD
        if(!dataInitialized)
        {
            //if MTD, Start timer to send periodic data, IF FTD, do nothing.
            app_printf("App_Log: Device State Child. Start app timer\n");
            xTimerStart(sAppTimerHandle, 0);
            threadInitData();
            dataInitialized = true;
            deviceStateUpdated = true;
        }
#endif      
    }
    else if (state == OT_DEVICE_ROLE_ROUTER)
    {
#if OPENTHREAD_FTD
        if(!dataInitialized)
        {            
            //Start timer to send periodic data 
            app_printf("App_Log: Device State Router. Start app timer\n");
            xTimerStart(sAppTimerHandle, 0);
            threadInitData();
            dataInitialized = true;
            deviceStateUpdated = true;
        }
#endif            
    }
    else if (state == OT_DEVICE_ROLE_LEADER)
    {
        // I'm your Leader!!!!!, You listen to me !!!!!!!!
        //Listen to data
        app_printf("App_Log: I'm your Leader!!!!!, You listen to me !!!!!!!!\n");
        threadInitData();
        xTimerStart(sAppTimerHandle, 0);
        deviceStateUpdated = true;
    }
    else if (state == OT_DEVICE_ROLE_DETACHED)
    {
        if (deviceStateUpdated)
        {
            //Detached from NWk, Cry now.
            app_printf("App_Err: Device state is detached. Detached from NWk, Cry now.\n");
        }
        else
        {
            app_printf("App_Log: Detached Device\r\n");
        }

    }
    else if (state == OT_DEVICE_ROLE_DISABLED)
    {
        // Just reset the device.
        app_printf("App_Err: Device state is disabled. Reset the device!!!!\n");
    }
    // not handling other flags as of now.
}

void threadInitData()
{
    threadUdpOpen();
    threadUdpBind();
}

void threadSenddata()
{
//    threadUdpSend();
}

void otPlatUartReceived(const uint8_t *aBuf, uint16_t aBufLength)
{
    // Do nothing!!!!
}

void otPlatUartSendDone(void)
{
    // Do nothing!!!!
}


/*******************************************************************************
 End of File
 */

