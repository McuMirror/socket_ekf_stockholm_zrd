#ifndef SRC_INCLUDE_APP_SOCKET_H_
#define SRC_INCLUDE_APP_SOCKET_H_

#include "tl_common.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "gp.h"

#include "app_utility.h"
#include "app_endpoint_cfg.h"
#include "app_led.h"
#include "app_button.h"
#include "app_onoff.h"
#include "app_relay.h"
#include "app_monitoring.h"

#define PROTECT_CONTROL_OFF     0x00
#define PROTECT_CONTROL_ON      0x01
#define AUTORESTART_OFF         0x00
#define AUTORESTART_ON          0x01
#define KEY_LOCK_OFF            0x00
#define KEY_LOCK_ON             0x01
#define TIME_RELOAD_MIN         5                   // sec
#define TIME_RELOAD_MAX         60                  // sec
#define VOLTAGE_MIN             0                   // 0V
#define VOLTAGE_MAX             30000               // 300V
#define ADJUST_MS_MIN          -99.0f
#define ADJUST_MS_MAX           100.0f

#define DEFAULT_CURRENT_MAX     1600                // 16A
#define DEFAULT_POWER_MAX       3600                // 3600W
#define DEFAULT_VOLTAGE_MIN     18000               // 180V
#define DEFAULT_VOLTAGE_MAX     26000               // 260V
#define DEFAULT_ADJUST_VOLTAGE  -2.5f
#define DEFAULT_ADJUST_CURRENT  0.0f
#define DEFAULT_ADJUST_POWER    0.0f
#define DEFAULT_TIME_RELOAD     TIME_RELOAD_MIN     // 5sec
#define DEFAULT_PROTECT_CONTROL PROTECT_CONTROL_OFF // 0 - off, 1 - on
#define DEFAULT_AUTORESTART     AUTORESTART_OFF     // 0 - off, 1 - on
#define DEFAULT_KEY_LOCK        KEY_LOCK_OFF        // 0 - off, 1 - on
#define DEFAULT_LED_CONTROL     CONTROL_LED_ON_OFF  // if relay if on, LED is on, if relay is off, LED is off


typedef struct{
    uint8_t keyType; /* CERTIFICATION_KEY or MASTER_KEY key for touch-link or distribute network
                   SS_UNIQUE_LINK_KEY or SS_GLOBAL_LINK_KEY for distribute network */
    uint8_t key[16]; /* the key used */
}app_linkKey_info_t;

typedef struct __attribute__((packed)) {
    uint8_t  status_onoff;
    uint8_t  startUpOnOff;
    uint16_t current_max;
    int16_t  power_max;
    int16_t  voltage_min;
    int16_t  voltage_max;
    float    adjust_current;
    float    adjust_power;
    float    adjust_voltage;
    uint16_t time_reload;
    uint8_t  protect_control;
    uint8_t  auto_restart;
    uint8_t  key_lock;
    uint8_t  led_control;
    uint8_t  crc;
} socket_settings_t;


typedef struct{
    ev_timer_event_t *timerFactoryReset;
    ev_timer_event_t *timerLedEvt;

    bool net_steer_start;

    button_t button[MAX_BUTTON_NUM];
    u8  keyPressed;

    app_linkKey_info_t tcLinkKey;
} app_ctx_t;

extern socket_settings_t socket_settings;

extern app_ctx_t g_appCtx;
extern bdb_commissionSetting_t g_bdbCommissionSetting;
extern bdb_appCb_t g_zbBdbCb;

nv_sts_t socket_settings_restore();
nv_sts_t socket_settings_save();
void socket_settints_default();

status_t app_basicCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload);
status_t app_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload);
status_t app_sceneCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload);
status_t app_onOffCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t app_meteringCb(zclIncomingAddrInfo_t *pAddrInfo, uint8_t cmdId, void *cmdPayload);

void app_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf);
void app_leaveIndHandler(nlme_leave_ind_t *pLeaveInd);
void app_otaProcessMsgHandler(uint8_t evt, uint8_t status);
bool app_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate);
void app_zclProcessIncomingMsg(zclIncoming_t *pInHdlrMsg);

#endif /* SRC_INCLUDE_APP_SOCKET_H_ */
