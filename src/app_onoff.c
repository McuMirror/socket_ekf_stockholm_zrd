#include "app_socket.h"

/*********************************************************************
 * @fn      cmdOnOff_toggle
 *
 * @brief   toggle the relay on/off and update zcl onoff attribute
 *
 * @param   None
 *
 * @return  None
 */
void cmdOnOff_toggle(void) {

#if UART_PRINTF_MODE
    APP_DEBUG(DEBUG_ONOFF_EN, "cmdOnOff_toggle\r\n");
#endif


    zcl_onOffAttr_t *pOnOff = zcl_onOffAttrsGet();
    socket_settings.status_onoff = !socket_settings.status_onoff;
    pOnOff->onOff = socket_settings.status_onoff;
    if (pOnOff->startUpOnOff > ZCL_START_UP_ONOFF_SET_ONOFF_TO_ON) {
        socket_settings_save();
    }
    uint8_t status = OFF;

    if (pOnOff->onOff) {
        status = ON;
    }

    set_relay_status(status);
}

/*********************************************************************
 * @fn      cmdOnOff_on
 *
 * @brief   turn the relay on and update zcl onoff attribute
 *
 * @param   None
 *
 * @return  None
 */
void cmdOnOff_on(void) {

#if UART_PRINTF_MODE
    APP_DEBUG(DEBUG_ONOFF_EN, "cmdOnOff_on\r\n");
#endif

    zcl_onOffAttr_t *pOnOff = zcl_onOffAttrsGet();

    pOnOff->onOff = socket_settings.status_onoff = ZCL_ONOFF_STATUS_ON;
    if (pOnOff->startUpOnOff > ZCL_START_UP_ONOFF_SET_ONOFF_TO_ON) {
        socket_settings_save();
    }
    uint8_t status = ON;

    set_relay_status(status);
}

/*********************************************************************
 * @fn      cmdOnOff_off
 *
 * @brief   turn the relay off and update zcl onoff attribute
 *
 * @param   None
 *
 * @return  None
 */
void cmdOnOff_off(void) {

#if UART_PRINTF_MODE
    APP_DEBUG(DEBUG_ONOFF_EN, "cmdOnOff_off\r\n");
#endif

    zcl_onOffAttr_t *pOnOff = zcl_onOffAttrsGet();
    pOnOff->onOff = socket_settings.status_onoff = ZCL_ONOFF_STATUS_OFF;
    if (pOnOff->startUpOnOff > ZCL_START_UP_ONOFF_SET_ONOFF_TO_ON) {
        socket_settings_save();
    }
    uint8_t status = OFF;

    set_relay_status(status);
}

/*********************************************************************
 * @fn      remoteCmdOnOff
 *
 * @brief   send an on/off/toggle command to a remote zigbee endpoint
 *
 * @param   ep - source endpoint
 *
 * @param   cmd - zcl onoff command (0x00 off, 0x01 on, 0x02 toggle)
 *
 * @return  None
 */
void remoteCmdOnOff(uint8_t ep, uint8_t cmd) {
    epInfo_t dstEpInfo;
    TL_SETSTRUCTCONTENT(dstEpInfo, 0);

    dstEpInfo.profileId = HA_PROFILE_ID;

    dstEpInfo.dstAddrMode = APS_DSTADDR_EP_NOTPRESETNT;

    /* command 0x00 - off, 0x01 - on, 0x02 - toggle */

    switch(cmd) {
        case ZCL_CMD_ONOFF_OFF:
#if UART_PRINTF_MODE
            APP_DEBUG(DEBUG_ONOFF_EN, "OnOff command: off\r\n");
#endif /* UART_PRINTF_MODE */
            zcl_onOff_offCmd(ep, &dstEpInfo, FALSE);
            break;
        case ZCL_CMD_ONOFF_ON:
#if UART_PRINTF_MODE
            APP_DEBUG(DEBUG_ONOFF_EN, "OnOff command: on\r\n");
#endif /* UART_PRINTF_MODE */
            zcl_onOff_onCmd(ep, &dstEpInfo, FALSE);
            break;
        case ZCL_CMD_ONOFF_TOGGLE:
#if UART_PRINTF_MODE
            APP_DEBUG(DEBUG_ONOFF_EN, "OnOff command: toggle\r\n");
#endif /* UART_PRINTF_MODE */
            zcl_onOff_toggleCmd(ep, &dstEpInfo, FALSE);
            break;
        default:
            break;
    }
}
