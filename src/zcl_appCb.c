/********************************************************************************************************
 * @file    zcl_appCb.c
 *
 * @brief   This is the source file for zcl_appCb
 *
 * @author  Zigbee Group
 * @date    2021
 *
 * @par     Copyright (c) 2021, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *          All rights reserved.
 *
 *          Licensed under the Apache License, Version 2.0 (the "License");
 *          you may not use this file except in compliance with the License.
 *          You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 *          Unless required by applicable law or agreed to in writing, software
 *          distributed under the License is distributed on an "AS IS" BASIS,
 *          WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *          See the License for the specific language governing permissions and
 *          limitations under the License.
 *
 *******************************************************************************************************/

/**********************************************************************
 * INCLUDES
 */
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "ota.h"

#include "app_utility.h"
#include "app_endpoint_cfg.h"
#include "app_socket.h"
#include "app_led.h"
#include "zcl_custom_attr.h"
#include "app_led.h"
#include "app_onoff.h"
#include "bl0937.h"

/**********************************************************************
 * LOCAL CONSTANTS
 */



/**********************************************************************
 * TYPEDEFS
 */


/**********************************************************************
 * LOCAL FUNCTIONS
 */
#ifdef ZCL_READ
static void app_zclReadRspCmd(zclReadRspCmd_t *pReadRspCmd);
#endif
#ifdef ZCL_WRITE
static void app_zclWriteReqCmd(uint8_t epId, uint16_t clusterId, zclWriteCmd_t *pWriteReqCmd);
static void app_zclWriteRspCmd(zclWriteRspCmd_t *pWriteRspCmd);
#endif
#ifdef ZCL_REPORT
static void app_zclCfgReportCmd(uint8_t endPoint, uint16_t clusterId, zclCfgReportCmd_t *pCfgReportCmd);
static void app_zclCfgReportRspCmd(zclCfgReportRspCmd_t *pCfgReportRspCmd);
static void app_zclReportCmd(uint16_t clusterId, zclReportCmd_t *pReportCmd, aps_data_ind_t aps_data_ind);
#endif
static void app_zclDfltRspCmd(zclDefaultRspCmd_t *pDftRspCmd);


/**********************************************************************
 * GLOBAL VARIABLES
 */


/**********************************************************************
 * LOCAL VARIABLES
 */
#ifdef ZCL_IDENTIFY
static ev_timer_event_t *identifyTimerEvt = NULL;
#endif

uint8_t count_no_service = 0;


/**********************************************************************
 * FUNCTIONS
 */

/*********************************************************************
 * @fn      app_zclProcessIncomingMsg
 *
 * @brief   Process ZCL Foundation incoming message.
 *
 * @param   pInMsg - pointer to the received message
 *
 * @return  None
 */
void app_zclProcessIncomingMsg(zclIncoming_t *pInHdlrMsg)
{
//  APP_DEBUG(DEBUG_ZCL_CB_EN, "app_zclProcessIncomingMsg\n");

    uint16_t cluster = pInHdlrMsg->msg->indInfo.cluster_id;
    uint8_t endPoint = pInHdlrMsg->msg->indInfo.dst_ep;
    aps_data_ind_t aps_data_ind = pInHdlrMsg->msg->indInfo;

    switch(pInHdlrMsg->hdr.cmd)
    {
#ifdef ZCL_READ
        case ZCL_CMD_READ_RSP:
            app_zclReadRspCmd(pInHdlrMsg->attrCmd);
            break;
#endif
#ifdef ZCL_WRITE
        case ZCL_CMD_WRITE:
            app_zclWriteReqCmd(endPoint, pInHdlrMsg->msg->indInfo.cluster_id, pInHdlrMsg->attrCmd);
            break;
        case ZCL_CMD_WRITE_RSP:
            app_zclWriteRspCmd(pInHdlrMsg->attrCmd);
            break;
#endif
#ifdef ZCL_REPORT
        case ZCL_CMD_CONFIG_REPORT:
            app_zclCfgReportCmd(endPoint, cluster, pInHdlrMsg->attrCmd);
            break;
        case ZCL_CMD_CONFIG_REPORT_RSP:
            app_zclCfgReportRspCmd(pInHdlrMsg->attrCmd);
            break;
        case ZCL_CMD_REPORT:
            app_zclReportCmd(cluster, pInHdlrMsg->attrCmd, aps_data_ind);
            break;
#endif
        case ZCL_CMD_DEFAULT_RSP:
            app_zclDfltRspCmd(pInHdlrMsg->attrCmd);
            break;
        default:
            break;
    }
}

#ifdef ZCL_READ
/*********************************************************************
 * @fn      app_zclReadRspCmd
 *
 * @brief   Handler for ZCL Read Response command.
 *
 * @param   pInHdlrMsg - incoming message to process
 *
 * @return  None
 */
static void app_zclReadRspCmd(zclReadRspCmd_t *pReadRspCmd) {
//    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_zclReadRspCmd\n");

    uint8_t numAttr = pReadRspCmd->numAttr;
    zclReadRspStatus_t *attrList = pReadRspCmd->attrList;
    uint32_t time_local;
    bool time_sent = false;

    for (uint8_t i = 0; i < numAttr; i++) {
        if (attrList[i].attrID == ZCL_ATTRID_LOCAL_TIME && attrList[i].status == ZCL_STA_SUCCESS) {
            time_local = attrList[i].data[0] & 0xff;
            time_local |= (attrList[i].data[1] << 8)  & 0x0000ffff;
            time_local |= (attrList[i].data[2] << 16) & 0x00ffffff;
            time_local |= (attrList[i].data[3] << 24) & 0xffffffff;
            zcl_setAttrVal(APP_ENDPOINT1, ZCL_CLUSTER_GEN_TIME, ZCL_ATTRID_LOCAL_TIME, (uint8_t*)&time_local);
            time_sent = true;
//            APP_DEBUG(DEBUG_TIME_EN, "Sync Local Time: %d\r\n", time_local+UNIX_TIME_CONST);
        }
    }

    if (time_sent) {
//        set_time_sent();
    }
}
#endif

#ifdef ZCL_WRITE
/*********************************************************************
 * @fn      app_zclWriteReqCmd
 *
 * @brief   Handler for ZCL Write Request command.
 *
 * @param
 *
 * @return  None
 */
static void app_zclWriteReqCmd(uint8_t epId, uint16_t clusterId, zclWriteCmd_t *pWriteReqCmd) {

//    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_zclWriteReqCmd\r\n");

    uint8_t numAttr = pWriteReqCmd->numAttr;
    zclWriteRec_t *attr = pWriteReqCmd->attrList;
    bool save = false;

    if (clusterId == ZCL_CLUSTER_GEN_ON_OFF) {
        for (uint32_t i = 0; i < numAttr; i++) {
            if (attr[i].attrID == ZCL_ATTRID_START_UP_ONOFF) {
                uint8_t startup = attr[i].attrData[0];
                APP_DEBUG(DEBUG_ZCL_CB_EN, "startup: 0x%02x, ep: %d\r\n", startup, epId);
                if ((startup >= ZCL_START_UP_ONOFF_SET_ONOFF_TO_OFF && startup <= ZCL_START_UP_ONOFF_SET_ONOFF_TOGGLE) ||
                        startup == ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS) {
                    socket_settings.startUpOnOff = startup;
                    save = true;
                }
            } else if (attr[i].attrID == ZCL_ATTRID_CUSTOM_KEY_LOCK) {
                uint8_t key_lock = attr[i].attrData[0];
                APP_DEBUG(DEBUG_ZCL_CB_EN, "key_lock: 0x%02x, ep: %d\r\n", key_lock, epId);
                socket_settings.key_lock = key_lock;
                save = true;
            } else if (attr[i].attrID == ZCL_ATTRID_CUSTOM_LED) {
                uint8_t led_ctrl = attr[i].attrData[0];
                APP_DEBUG(DEBUG_ZCL_CB_EN, "led_ctrl: 0x%02x, ep: %d\r\n", led_ctrl, epId);
                if ( led_ctrl >= CONTROL_LED_OFF && led_ctrl < CONTROL_LED_MAX) {
                    socket_settings.led_control = led_ctrl;
                    save = true;
                    set_led_status(socket_settings.status_onoff);
                }
            }
        }
    } else if (clusterId == ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT) {
        for (uint8_t i = 0; i < numAttr; i++) {
            if (attr[i].attrID == ZCL_ATTRID_RMS_EXTREME_UNDER_VOLTAGE) {
                int16_t v_min = attr[i].attrData[0] & 0xff;
                v_min |= (attr[i].attrData[1] << 8) & 0xffff;
                APP_DEBUG(DEBUG_ZCL_CB_EN, "voltage_min: %d\r\n", v_min);
                if (v_min >= VOLTAGE_MIN && v_min <= VOLTAGE_MAX) {
                    socket_settings.voltage_min = v_min;
                    save = true;
                }
            } else if (attr[i].attrID == ZCL_ATTRID_RMS_EXTREME_OVER_VOLTAGE) {
                int16_t v_max = attr[i].attrData[0] & 0xff;
                v_max |= (attr[i].attrData[1] << 8) & 0xffff;
                APP_DEBUG(DEBUG_ZCL_CB_EN, "voltage_max: %d\r\n", v_max);
                if (v_max >= VOLTAGE_MIN && v_max <= VOLTAGE_MAX) {
                    socket_settings.voltage_max = v_max;
                    save = true;
                }
            } else if (attr[i].attrID == ZCL_ATTRID_CUSTOM_CURRENT_MAX) {
                uint16_t i_max = attr[i].attrData[0] & 0xff;
                i_max |= (attr[i].attrData[1] << 8) & 0xffff;
                APP_DEBUG(DEBUG_ZCL_CB_EN, "current_max: %d\r\n", i_max);
                if (i_max <= DEFAULT_CURRENT_MAX) {
                    socket_settings.current_max = i_max;
                    save = true;
                }
            } else if (attr[i].attrID == ZCL_ATTRID_CUSTOM_POWER_MAX) {
                int16_t p_max = attr[i].attrData[0] & 0xff;
                p_max |= (attr[i].attrData[1] << 8) & 0xffff;
                APP_DEBUG(DEBUG_ZCL_CB_EN, "power_max: %d\r\n", p_max);
                if (p_max <= DEFAULT_POWER_MAX) {
                    socket_settings.power_max = p_max;
                    save = true;
                }
            } else if (attr[i].attrID == ZCL_ATTRID_CUSTOM_TIME_RELOAD) {
                uint16_t time = attr[i].attrData[0] & 0xff;
                time |= (attr[i].attrData[1] << 8) & 0xffff;
                APP_DEBUG(DEBUG_ZCL_CB_EN, "time_reload: %d\r\n", time);
                if (time >= TIME_RELOAD_MIN && time <= TIME_RELOAD_MAX) {
                    socket_settings.time_reload = time;
                    save = true;
                }
            } else if (attr[i].attrID == ZCL_ATTRID_CUSTOM_PROTECT_CONTROL) {
                uint8_t ctrl = attr[i].attrData[0];
                APP_DEBUG(DEBUG_ZCL_CB_EN, "protect_control: %d\r\n", ctrl);
                if (ctrl == PROTECT_CONTROL_OFF || ctrl == PROTECT_CONTROL_ON) {
                    socket_settings.protect_control = ctrl;
                    save = true;
                }
            } else if (attr[i].attrID == ZCL_ATTRID_CUSTOM_AUTORESTART) {
                uint8_t restart = attr[i].attrData[0];
                APP_DEBUG(DEBUG_ZCL_CB_EN, "auto_restart: %d\r\n", restart);
                if (restart == AUTORESTART_OFF || restart == AUTORESTART_ON) {
                    socket_settings.auto_restart = restart;
                    save = true;
                }
            } else if (attr[i].attrID == ZCL_ATTRID_CUSTOM_ADJUST_CURRENT) {
                float adjust;
                uint8_t *pa = (uint8_t*)&adjust;
                pa[0] = attr[i].attrData[0];
                pa[1] = attr[i].attrData[1];
                pa[2] = attr[i].attrData[2];
                pa[3] = attr[i].attrData[3];
                APP_DEBUG(DEBUG_ZCL_CB_EN, "adjust_current: %s\r\n", float_to_string(adjust, 2));
                if (adjust >= ADJUST_MS_MIN && adjust <= ADJUST_MS_MAX) {
                    bl0937_adjustCurrent(adjust);
                }
            } else if (attr[i].attrID == ZCL_ATTRID_CUSTOM_ADJUST_VOLTAGE) {
                float adjust;
                uint8_t *pa = (uint8_t*)&adjust;
                pa[0] = attr[i].attrData[0];
                pa[1] = attr[i].attrData[1];
                pa[2] = attr[i].attrData[2];
                pa[3] = attr[i].attrData[3];
                APP_DEBUG(DEBUG_ZCL_CB_EN, "adjust_voltage: %s\r\n", float_to_string(adjust, 2));
                if (adjust >= ADJUST_MS_MIN && adjust <= ADJUST_MS_MAX) {
                    bl0937_adjustVoltage(adjust);
                }
            } else if (attr[i].attrID == ZCL_ATTRID_CUSTOM_ADJUST_POWER) {
                float adjust;
                uint8_t *pa = (uint8_t*)&adjust;
                pa[0] = attr[i].attrData[0];
                pa[1] = attr[i].attrData[1];
                pa[2] = attr[i].attrData[2];
                pa[3] = attr[i].attrData[3];
                APP_DEBUG(DEBUG_ZCL_CB_EN, "adjust_power: %s\r\n", float_to_string(adjust, 2));
                if (adjust >= ADJUST_MS_MIN && adjust <= ADJUST_MS_MAX) {
                    bl0937_adjustPower(adjust);
                }
            }
        }
    }

    if (save) socket_settings_save();

#ifdef ZCL_POLL_CTRL
    if(clusterId == ZCL_CLUSTER_GEN_POLL_CONTROL){
        for(int32_t i = 0; i < numAttr; i++){
            if(attr[i].attrID == ZCL_ATTRID_CHK_IN_INTERVAL){
                app_zclCheckInStart();
                return;
            }
        }
    }
#endif
}

/*********************************************************************
 * @fn      app_zclWriteRspCmd
 *
 * @brief   Handler for ZCL Write Response command.
 *
 * @param   pInHdlrMsg - incoming message to process
 *
 * @return  None
 */
static void app_zclWriteRspCmd(zclWriteRspCmd_t *pWriteRspCmd)
{
//    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_zclWriteRspCmd\n");

}
#endif


/*********************************************************************
 * @fn      app_zclDfltRspCmd
 *
 * @brief   Handler for ZCL Default Response command.
 *
 * @param   pInHdlrMsg - incoming message to process
 *
 * @return  None
 */
static void app_zclDfltRspCmd(zclDefaultRspCmd_t *pDftRspCmd)
{
//  APP_DEBUG(DEBUG_ZCL_CB_EN, "app_zclDfltRspCmd\n");
#ifdef ZCL_OTA
    if( (pDftRspCmd->commandID == ZCL_CMD_OTA_UPGRADE_END_REQ) &&
        (pDftRspCmd->statusCode == ZCL_STA_ABORT) ){
        if(zcl_attr_imageUpgradeStatus == IMAGE_UPGRADE_STATUS_DOWNLOAD_COMPLETE){
            ota_upgradeAbort();
        }
    }
#endif
}

#ifdef ZCL_REPORT
/*********************************************************************
 * @fn      app_zclCfgReportCmd
 *
 * @brief   Handler for ZCL Configure Report command.
 *
 * @param   pInHdlrMsg - incoming message to process
 *
 * @return  None
 */
static void app_zclCfgReportCmd(uint8_t endPoint, uint16_t clusterId, zclCfgReportCmd_t *pCfgReportCmd)
{
    //APP_DEBUG(DEBUG_ZCL_CB_EN, "app_zclCfgReportCmd\r\n");
//    reportAttrTimerStop();
}

/*********************************************************************
 * @fn      app_zclCfgReportRspCmd
 *
 * @brief   Handler for ZCL Configure Report Response command.
 *
 * @param   pInHdlrMsg - incoming message to process
 *
 * @return  None
 */
static void app_zclCfgReportRspCmd(zclCfgReportRspCmd_t *pCfgReportRspCmd)
{
//    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_zclCfgReportRspCmd\n");

}

/*********************************************************************
 * @fn      app_zclReportCmd
 *
 * @brief   Handler for ZCL Report command.
 *
 * @param   pInHdlrMsg - incoming message to process
 *
 * @return  None
 */
static void app_zclReportCmd(uint16_t clusterId, zclReportCmd_t *pReportCmd, aps_data_ind_t aps_data_ind) {
//    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_zclReportCmd\r\n");

//    uint8_t numAttr = pReportCmd->numAttr;
//    zclReport_t *attrList = pReportCmd->attrList;
//
//    uint8_t ret;
//    uint16_t addr = aps_data_ind.src_short_addr;
//
//    for (uint8_t i = 0; i < numAttr; i++) {
//        if (clusterId == ZCL_CLUSTER_MS_TEMPERATURE_MEASUREMENT &&
//                attrList[i].dataType == ZCL_DATA_TYPE_INT16 &&
//                attrList[i].attrID == ZCL_TEMPERATURE_MEASUREMENT_ATTRID_MEASUREDVALUE) {
//            int16_t temp;
//
//            temp = attrList[i].attrData[0] & 0xFF;
//            temp |= (attrList[i].attrData[1] << 8) & 0xFFFF;
//
////            APP_DEBUG(DEBUG_ZCL_CB_EN, "temp: 0x%04x\r\n", (uint16_t)temp);
//
//            ret = bind_outsise_proc(addr, clusterId);
//
//            if (ret != OUTSIDE_SRC_CLUSTER_OK) {
//                continue;
//            }
//
//            app_set_remote_temperature(temp);
//            bind_remote_update_timer();
//        }
//    }

}
#endif

#ifdef ZCL_BASIC
/*********************************************************************
 * @fn      app_basicCb
 *
 * @brief   Handler for ZCL Basic Reset command.
 *
 * @param   pAddrInfo
 * @param   cmdId
 * @param   cmdPayload
 *
 * @return  status_t
 */
status_t app_basicCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload)
{
    if(cmdId == ZCL_CMD_BASIC_RESET_FAC_DEFAULT){
        //Reset all the attributes of all its clusters to factory defaults
        //zcl_nv_attr_reset();
    }

    return ZCL_STA_SUCCESS;
}
#endif

#ifdef ZCL_IDENTIFY
int32_t app_zclIdentifyTimerCb(void *arg)
{
    if(g_zcl_identifyAttrs.identifyTime <= 0){
        light_blink_stop();

        identifyTimerEvt = NULL;
        return -1;
    }
    g_zcl_identifyAttrs.identifyTime--;
    return 0;
}

void app_zclIdentifyTimerStop(void)
{
    if(identifyTimerEvt){
        TL_ZB_TIMER_CANCEL(&identifyTimerEvt);
    }
}

/*********************************************************************
 * @fn      app_zclIdentifyCmdHandler
 *
 * @brief   Handler for ZCL Identify command. This function will set blink LED.
 *
 * @param   endpoint
 * @param   srcAddr
 * @param   identifyTime - identify time
 *
 * @return  None
 */
void app_zclIdentifyCmdHandler(uint8_t endpoint, uint16_t srcAddr, uint16_t identifyTime)
{
    g_zcl_identifyAttrs.identifyTime = identifyTime;

    if(identifyTime == 0){
        app_zclIdentifyTimerStop();
        light_blink_stop();
    }else{
        if(!identifyTimerEvt){
            light_blink_start(identifyTime, 500, 500);
            identifyTimerEvt = TL_ZB_TIMER_SCHEDULE(app_zclIdentifyTimerCb, NULL, 1000);
        }
    }
}

/*********************************************************************
 * @fn      app_zcltriggerCmdHandler
 *
 * @brief   Handler for ZCL trigger command.
 *
 * @param   pTriggerEffect
 *
 * @return  None
 */
static void app_zcltriggerCmdHandler(zcl_triggerEffect_t *pTriggerEffect)
{
    uint8_t effectId = pTriggerEffect->effectId;
//  uint8_t effectVariant = pTriggerEffect->effectVariant;


    switch (effectId) {
        case IDENTIFY_EFFECT_BLINK:
            light_blink_start(1, 500, 500);
            break;
        case IDENTIFY_EFFECT_BREATHE:
            light_blink_start(15, 300, 700);
            break;
        case IDENTIFY_EFFECT_OKAY:
            light_blink_start(2, 250, 250);
            break;
        case IDENTIFY_EFFECT_CHANNEL_CHANGE:
            light_blink_start(1, 500, 7500);
            break;
        case IDENTIFY_EFFECT_FINISH_EFFECT:
            light_blink_start(1, 300, 700);
            break;
        case IDENTIFY_EFFECT_STOP_EFFECT:
            light_blink_stop();
            break;
        default:
            break;
    }
}

/*********************************************************************
 * @fn      app_identifyCb
 *
 * @brief   Handler for ZCL Identify command.
 *
 * @param   pAddrInfo
 * @param   cmdId
 * @param   cmdPayload
 *
 * @return  status_t
 */
status_t app_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload)
{
    if(pAddrInfo->dstEp == APP_ENDPOINT1){
        if(pAddrInfo->dirCluster == ZCL_FRAME_CLIENT_SERVER_DIR){
            switch(cmdId){
                case ZCL_CMD_IDENTIFY:
                    app_zclIdentifyCmdHandler(pAddrInfo->dstEp, pAddrInfo->srcAddr, ((zcl_identifyCmd_t *)cmdPayload)->identifyTime);
                    break;
                case ZCL_CMD_TRIGGER_EFFECT:
                    app_zcltriggerCmdHandler((zcl_triggerEffect_t *)cmdPayload);
                    break;
                default:
                    break;
            }
        }
    }

    return ZCL_STA_SUCCESS;
}
#endif

#ifdef ZCL_GROUP
/*********************************************************************
 * @fn      app_zclAddGroupRspCmdHandler
 *
 * @brief   Handler for ZCL add group response command.
 *
 * @param   pAddGroupRsp
 *
 * @return  None
 */
static void app_zclAddGroupRspCmdHandler(uint8_t ep, zcl_addGroupRsp_t *pAddGroupRsp) {

//    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_zclAddGroupRspCmdHandler. ep: %d, status: %d, gid: %d\r\n", ep, pAddGroupRsp->status, pAddGroupRsp->groupId);
}

/*********************************************************************
 * @fn      app_zclViewGroupRspCmdHandler
 *
 * @brief   Handler for ZCL view group response command.
 *
 * @param   pViewGroupRsp
 *
 * @return  None
 */
static void app_zclViewGroupRspCmdHandler(zcl_viewGroupRsp_t *pViewGroupRsp)
{

}

/*********************************************************************
 * @fn      app_zclRemoveGroupRspCmdHandler
 *
 * @brief   Handler for ZCL remove group response command.
 *
 * @param   pRemoveGroupRsp
 *
 * @return  None
 */
static void app_zclRemoveGroupRspCmdHandler(zcl_removeGroupRsp_t *pRemoveGroupRsp)
{

}

/*********************************************************************
 * @fn      app_zclGetGroupMembershipRspCmdHandler
 *
 * @brief   Handler for ZCL get group membership response command.
 *
 * @param   pGetGroupMembershipRsp
 *
 * @return  None
 */
static void app_zclGetGroupMembershipRspCmdHandler(zcl_getGroupMembershipRsp_t *pGetGroupMembershipRsp)
{

}

/*********************************************************************
 * @fn      app_groupCb
 *
 * @brief   Handler for ZCL Group command.
 *
 * @param   pAddrInfo
 * @param   cmdId
 * @param   cmdPayload
 *
 * @return  status_t
 */
status_t app_groupCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload) {

    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_groupCb. ep: %d, cmdId\r\n", pAddrInfo->dstEp, cmdId);

	if(pAddrInfo->dstEp == APP_ENDPOINT1) {
		if(pAddrInfo->dirCluster == ZCL_FRAME_SERVER_CLIENT_DIR){
			switch(cmdId){
				case ZCL_CMD_GROUP_ADD_GROUP_RSP:
					app_zclAddGroupRspCmdHandler(pAddrInfo->dstEp, (zcl_addGroupRsp_t *)cmdPayload);
					break;
				case ZCL_CMD_GROUP_VIEW_GROUP_RSP:
					app_zclViewGroupRspCmdHandler((zcl_viewGroupRsp_t *)cmdPayload);
					break;
				case ZCL_CMD_GROUP_REMOVE_GROUP_RSP:
					app_zclRemoveGroupRspCmdHandler((zcl_removeGroupRsp_t *)cmdPayload);
					break;
				case ZCL_CMD_GROUP_GET_MEMBERSHIP_RSP:
					app_zclGetGroupMembershipRspCmdHandler((zcl_getGroupMembershipRsp_t *)cmdPayload);
					break;
				default:
					break;
			}
		}
	}

	return ZCL_STA_SUCCESS;
}
#endif	/* ZCL_GROUP */

#ifdef ZCL_SCENE
/*********************************************************************
 * @fn      app_zclAddSceneRspCmdHandler
 *
 * @brief   Handler for ZCL add scene response command.
 *
 * @param   cmdId
 * @param   pAddSceneRsp
 *
 * @return  None
 */
static void app_zclAddSceneRspCmdHandler(uint8_t cmdId, addSceneRsp_t *pAddSceneRsp)
{

}

/*********************************************************************
 * @fn      app_zclViewSceneRspCmdHandler
 *
 * @brief   Handler for ZCL view scene response command.
 *
 * @param   cmdId
 * @param   pViewSceneRsp
 *
 * @return  None
 */
static void app_zclViewSceneRspCmdHandler(uint8_t cmdId, viewSceneRsp_t *pViewSceneRsp)
{

}

/*********************************************************************
 * @fn      app_zclRemoveSceneRspCmdHandler
 *
 * @brief   Handler for ZCL remove scene response command.
 *
 * @param   pRemoveSceneRsp
 *
 * @return  None
 */
static void app_zclRemoveSceneRspCmdHandler(removeSceneRsp_t *pRemoveSceneRsp)
{

}

/*********************************************************************
 * @fn      app_zclRemoveAllSceneRspCmdHandler
 *
 * @brief   Handler for ZCL remove all scene response command.
 *
 * @param   pRemoveAllSceneRsp
 *
 * @return  None
 */
static void app_zclRemoveAllSceneRspCmdHandler(removeAllSceneRsp_t *pRemoveAllSceneRsp)
{

}

/*********************************************************************
 * @fn      app_zclStoreSceneRspCmdHandler
 *
 * @brief   Handler for ZCL store scene response command.
 *
 * @param   pStoreSceneRsp
 *
 * @return  None
 */
static void app_zclStoreSceneRspCmdHandler(storeSceneRsp_t *pStoreSceneRsp)
{

}

/*********************************************************************
 * @fn      app_zclGetSceneMembershipRspCmdHandler
 *
 * @brief   Handler for ZCL get scene membership response command.
 *
 * @param   pGetSceneMembershipRsp
 *
 * @return  None
 */
static void app_zclGetSceneMembershipRspCmdHandler(getSceneMemRsp_t *pGetSceneMembershipRsp)
{

}

/*********************************************************************
 * @fn      app_sceneRecallReqHandler
 *
 * @brief   Handler for ZCL scene recall command. This function will recall scene.
 *
 * @param   pApsdeInd
 * @param   pScene
 *
 * @return  None
 */
static void app_sceneRecallReqHandler(zclIncomingAddrInfo_t *pAddrInfo, zcl_sceneEntry_t *pScene)
{
    u8 *pData = pScene->extField;
    u16 clusterID = 0xFFFF;
    u8 extLen = 0;

    while (pData < pScene->extField + pScene->extFieldLen) {
        clusterID = BUILD_U16(pData[0], pData[1]);
        pData += 2;//cluster id

        extLen = *pData++;//length

        if (clusterID == ZCL_CLUSTER_GEN_ON_OFF) {
            if (extLen >= 1) {
                u8 onOff = *pData++;

                app_onOffCb(pAddrInfo, onOff, NULL);
                extLen--;
            }
        }

        pData += extLen;
    }
}

/*********************************************************************
 * @fn      app_sceneStoreReqHandler
 *
 * @brief   Handler for ZCL scene store command. This function will set scene attribute first.
 *
 * @param   pApsdeInd
 * @param   pScene
 *
 * @return  None
 */
static void app_sceneStoreReqHandler(zcl_sceneEntry_t *pScene, uint8_t endpoint) {
    u8 extLen = 0;

    zcl_onOffAttr_t *pOnOff = zcl_onOffAttrsGet();

    pScene->extField[extLen++] = LO_UINT16(ZCL_CLUSTER_GEN_ON_OFF);
    pScene->extField[extLen++] = HI_UINT16(ZCL_CLUSTER_GEN_ON_OFF);
    pScene->extField[extLen++] = sizeof(u8);
    pScene->extField[extLen++] = pOnOff->onOff;

    pScene->extFieldLen = extLen;
}

/*********************************************************************
 * @fn      app_sceneCb
 *
 * @brief   Handler for ZCL Scene command.
 *
 * @param   pAddrInfo
 * @param   cmdId
 * @param   cmdPayload
 *
 * @return  status_t
 */
status_t app_sceneCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload) {

    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_sceneCb. cmdId: 0x%02x\r\n", cmdId);

    status_t status = ZCL_STA_SUCCESS;

	if(pAddrInfo->dstEp == APP_ENDPOINT1 || pAddrInfo->dstEp == APP_ENDPOINT2){
		if(pAddrInfo->dirCluster == ZCL_FRAME_SERVER_CLIENT_DIR){
			switch(cmdId){
				case ZCL_CMD_SCENE_ADD_SCENE_RSP:
				case ZCL_CMD_SCENE_ENHANCED_ADD_SCENE_RSP:
					app_zclAddSceneRspCmdHandler(cmdId, (addSceneRsp_t *)cmdPayload);
					break;
				case ZCL_CMD_SCENE_VIEW_SCENE_RSP:
				case ZCL_CMD_SCENE_ENHANCED_VIEW_SCENE_RSP:
					app_zclViewSceneRspCmdHandler(cmdId, (viewSceneRsp_t *)cmdPayload);
					break;
				case ZCL_CMD_SCENE_REMOVE_SCENE_RSP:
					app_zclRemoveSceneRspCmdHandler((removeSceneRsp_t *)cmdPayload);
					break;
				case ZCL_CMD_SCENE_REMOVE_ALL_SCENE_RSP:
					app_zclRemoveAllSceneRspCmdHandler((removeAllSceneRsp_t *)cmdPayload);
					break;
				case ZCL_CMD_SCENE_STORE_SCENE_RSP:
					app_zclStoreSceneRspCmdHandler((storeSceneRsp_t *)cmdPayload);
					break;
				case ZCL_CMD_SCENE_GET_SCENE_MEMSHIP_RSP:
					app_zclGetSceneMembershipRspCmdHandler((getSceneMemRsp_t *)cmdPayload);
					break;
				default:
	                status = ZCL_STA_UNSUP_MANU_CLUSTER_COMMAND;
					break;
			}
		} else if (pAddrInfo->dirCluster == ZCL_FRAME_CLIENT_SERVER_DIR) {
            switch (cmdId) {
                case ZCL_CMD_SCENE_STORE_SCENE:
                    app_sceneStoreReqHandler((zcl_sceneEntry_t *)cmdPayload, pAddrInfo->dstEp);
                    break;
                case ZCL_CMD_SCENE_RECALL_SCENE:
                    app_sceneRecallReqHandler(pAddrInfo, (zcl_sceneEntry_t *)cmdPayload);
                    break;
                default:
                    status = ZCL_STA_UNSUP_MANU_CLUSTER_COMMAND;
                    break;
            }
        }

	}

	return status;
}

#endif	/* ZCL_SCENE */


/*********************************************************************
 * @fn      app_timeCb
 *
 * @brief   Handler for ZCL Identify command.
 *
 * @param   pAddrInfo
 * @param   cmdId
 * @param   cmdPayload
 *
 * @return  status_t
 */
status_t app_timeCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload) {

    //APP_DEBUG(DEBUG_ZCL_CB_EN, "app_timeCb. cmd: 0x%x\r\n", cmdId);

    return ZCL_STA_SUCCESS;
}



//static int32_t checkRespTimeCb(void *arg) {
//
//    if (device_online) {
//        if (!resp_time) {
//            if (count_no_service++ == 3) {
//                device_online = false;
//#if UART_PRINTF_MODE// && DEBUG_LEVEL
//                APP_DEBUG(DEBUG_ZCL_CB_EN, "No service!\r\n");
//#endif
//            }
//        } else {
//            count_no_service = 0;
//        }
//    } else {
//        if (resp_time) {
//            device_online = true;
//            count_no_service = 0;
//            APP_DEBUG(UART_PRINTF_MODE, "Device online\r\n");
//        }
//    }
//
//    resp_time = false;
//
//    return -1;
//}
//
//
//int32_t getTimeCb(void *arg) {
//
//    if(zb_isDeviceJoinedNwk()){
//        epInfo_t dstEpInfo;
//        TL_SETSTRUCTCONTENT(dstEpInfo, 0);
//
//        dstEpInfo.profileId = HA_PROFILE_ID;
//#if FIND_AND_BIND_SUPPORT
//        dstEpInfo.dstAddrMode = APS_DSTADDR_EP_NOTPRESETNT;
//#else
//        dstEpInfo.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
//        dstEpInfo.dstEp = APP_ENDPOINT1;
//        dstEpInfo.dstAddr.shortAddr = 0x0;
//#endif
//        zclAttrInfo_t *pAttrEntry;
//        pAttrEntry = zcl_findAttribute(APP_ENDPOINT1, ZCL_CLUSTER_GEN_TIME, ZCL_ATTRID_TIME);
//
//        zclReadCmd_t *pReadCmd = (zclReadCmd_t *)ev_buf_allocate(sizeof(zclReadCmd_t) + sizeof(uint16_t));
//        if(pReadCmd){
//            pReadCmd->numAttr = 1;
//            pReadCmd->attrID[0] = ZCL_ATTRID_TIME;
//
//            zcl_read(APP_ENDPOINT1, &dstEpInfo, ZCL_CLUSTER_GEN_TIME, MANUFACTURER_CODE_NONE, 0, 0, 0, pReadCmd);
//
//            ev_buf_free((uint8_t *)pReadCmd);
//
//            TL_ZB_TIMER_SCHEDULE(checkRespTimeCb, NULL, TIMEOUT_2SEC);
//        }
//    }
//
//    return 0;
//}

//status_t app_onOffCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload) {
//
////    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_onOffCb, dstEp: %d\r\n", pAddrInfo->dstEp);
//
//    zcl_onOffAttr_t *pOnOff = zcl_onOffAttrsGet();
//
//    if(pAddrInfo->dstEp == APP_ENDPOINT1) {
//            switch(cmdId){
//                case ZCL_CMD_ONOFF_ON:
//                    APP_DEBUG(UART_PRINTF_MODE, "pAddrInfo->dstEp: %d, cmd on\r\n", pAddrInfo->dstEp);
//                    cmdOnOff_on(pAddrInfo->dstEp);
//                    break;
//                case ZCL_CMD_ONOFF_OFF:
//                    APP_DEBUG(UART_PRINTF_MODE, "pAddrInfo->dstEp: %d, cmd off\r\n", pAddrInfo->dstEp);
//                    cmdOnOff_off(pAddrInfo->dstEp);
//                    break;
//                case ZCL_CMD_ONOFF_TOGGLE:
//                    APP_DEBUG(UART_PRINTF_MODE, "pAddrInfo->dstEp: %d, cmd toggle\r\n", pAddrInfo->dstEp);
//                    cmdOnOff_toggle(pAddrInfo->dstEp);
//                    break;
////                case ZCL_CMD_OFF_WITH_EFFECT:
////                    if(pOnOff->globalSceneControl == TRUE){
////                        /* TODO: store its settings in its global scene */
////
////                        pOnOff->globalSceneControl = FALSE;
////                    }
////                    sampleLight_onoff_offWithEffectProcess((zcl_onoff_offWithEffectCmd_t *)cmdPayload);
////                    break;
//                case ZCL_CMD_ON_WITH_RECALL_GLOBAL_SCENE:
//                    if(pOnOff->globalSceneControl == FALSE){
////                        app_onoff_onWithRecallGlobalSceneProcess();
//                        pOnOff->globalSceneControl = TRUE;
//                    }
//                    break;
////                case ZCL_CMD_ON_WITH_TIMED_OFF:
////                    sampleLight_onoff_onWithTimedOffProcess((zcl_onoff_onWithTimeOffCmd_t *)cmdPayload);
////                    break;
//                default:
//                    break;
//            }
//#if 0
//        } else {
//            APP_DEBUG(DEBUG_ZCL_CB_EN, "pAddrInfo->dstEp: %d, cmd off\r\n", pAddrInfo->dstEp);
//            cmdOnOff_off(pAddrInfo->dstEp);
//        }
//#endif
//    }
//
//    return ZCL_STA_SUCCESS;
//}

status_t app_msInputCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload) {

    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_aInputCb(). pAddrInfo->dirCluster: %0x%x, cmdId: 0x%x\r\n", pAddrInfo->dirCluster, cmdId);

    status_t status = ZCL_STA_SUCCESS;

    return status;
}

/*********************************************************************
 * @fn      app_levelCb
 *
 * @brief   Handler for ZCL LEVEL command. This function will set LEVEL attribute first.
 *
 * @param   pAddrInfo
 * @param   cmd - level cluster command id
 * @param   cmdPayload
 *
 * @return  status_t
 */

status_t app_levelCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload) {
    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_LevelCb\r\n");
//    if(pAddrInfo->dstEp == APP_ENDPOINT1 || pAddrInfo->dstEp == APP_ENDPOINT2) {
//        switch(cmdId){
//            case ZCL_CMD_LEVEL_MOVE_TO_LEVEL:
//                app_displayMoveToLevelProcess(pAddrInfo->dstEp, cmdId, (moveToLvl_t *)cmdPayload);
//                break;
//            default:
//                break;
//        }
//    }

    return ZCL_STA_SUCCESS;
}

status_t app_colorCtrlCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload) {
#if UART_PRINTF_MODE && DEBUG_ZCL_CB_EN
    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_colorCtrlCb\r\n");
#endif
//    status_t status = ZCL_STA_SUCCESS;

    return ZCL_STA_SUCCESS; //status;
}

/*********************************************************************
 * @fn      app_meteringCb
 *
 * @brief   Handler for ZCL Identify command.
 *
 * @param   pAddrInfo
 * @param   cmdId
 * @param   cmdPayload
 *
 * @return  status_t
 */
status_t app_meteringCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload)
{

//    APP_DEBUG(DEBUG_ZCL_CB_EN, "app_meteringCb\r\n");
    return ZCL_STA_SUCCESS;
}


