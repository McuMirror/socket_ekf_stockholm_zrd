#include "app_socket.h"
#include "bl0937.h"
#include "factory_reset.h"

socket_settings_t socket_settings;

static bool boot_announce_sent = false;
uint8_t resp_time = false;

app_ctx_t g_appCtx = {
        .timerFactoryReset = NULL,
};

#ifdef ZCL_OTA
extern ota_callBack_t app_otaCb;

//running code firmware information
ota_preamble_t app_otaInfo = {
    .fileVer            = FILE_VERSION,
    .imageType          = IMAGE_TYPE,
    .manufacturerCode   = MANUFACTURER_CODE_TELINK,
};
#endif

//Must declare the application call back function which used by ZDO layer
const zdo_appIndCb_t appCbLst = {
    bdb_zdoStartDevCnf,//start device cnf cb
    NULL,//reset cnf cb
    NULL,//device announce indication cb
    app_leaveIndHandler,//leave ind cb
    app_leaveCnfHandler,//leave cnf cb
    app_nwkUpdateIndicateHandler,//nwk update ind cb
    NULL,//permit join ind cb
    NULL,//nlme sync cnf cb
    NULL,//tc join ind cb
    NULL,//tc detects that the frame counter is near limit
};


/**
 *  @brief Definition for bdb commissioning setting
 */
bdb_commissionSetting_t g_bdbCommissionSetting = {
    .linkKey.tcLinkKey.keyType = SS_GLOBAL_LINK_KEY,
    .linkKey.tcLinkKey.key = (uint8_t *)tcLinkKeyCentralDefault,             //can use unique link key stored in NV

    .linkKey.distributeLinkKey.keyType = MASTER_KEY,
    .linkKey.distributeLinkKey.key = (uint8_t *)linkKeyDistributedMaster,    //use linkKeyDistributedCertification before testing

    .linkKey.touchLinkKey.keyType = MASTER_KEY,
    .linkKey.touchLinkKey.key = (uint8_t *)touchLinkKeyMaster,               //use touchLinkKeyCertification before testing

#if TOUCHLINK_SUPPORT
    .touchlinkEnable = 1,                                               /* enable touch-link */
#else
    .touchlinkEnable = 0,                                               /* disable touch-link */
#endif
    .touchlinkChannel = DEFAULT_CHANNEL,                                /* touch-link default operation channel for target */
    .touchlinkLqiThreshold = 0xA0,                                      /* threshold for touch-link scan req/resp command */
};



/*********************************************************************
 * @fn      stack_init
 *
 * @brief   This function initialize the ZigBee stack and related profile. If HA/ZLL profile is
 *          enabled in this application, related cluster should be registered here.
 *
 * @param   None
 *
 * @return  None
 */
void stack_init(void)
{
    /* Initialize ZB stack */
    zb_init();

    /* Register stack CB */
    zb_zdoCbRegister((zdo_appIndCb_t *)&appCbLst);
}

/*********************************************************************
 * @fn      user_app_init
 *
 * @brief   This function initialize the application(Endpoint) information for this node.
 *
 * @param   None
 *
 * @return  None
 */
void user_app_init(void)
{
    af_nodeDescManuCodeUpdate(MANUFACTURER_CODE_TELINK);

    /* Initialize ZCL layer */
    /* Register Incoming ZCL Foundation command/response messages */
    zcl_init(app_zclProcessIncomingMsg);

    /* Register endPoint */
    af_endpointRegister(APP_ENDPOINT1, (af_simple_descriptor_t *)&app_ep_simpleDesc, zcl_rx_handler, NULL);

    zcl_reportingTabInit();

    start_message();
    socket_settings_restore();

    /* Register ZCL specific cluster information */
    zcl_register(APP_ENDPOINT1, APP_CB_CLUSTER_NUM, (zcl_specClusterInfo_t *)g_appClusterList);

    relay_init();
    energy_restore();
    bl0937_init(CF_GPIO, CF1_GPIO, SEL_GPIO, BL0937_MODE_CURRENT);
    bl0937_adjustVoltage(socket_settings.adjust_voltage);
    bl0937_adjustCurrent(socket_settings.adjust_current);
    bl0937_adjustPower(socket_settings.adjust_power);

    #if ZCL_GP_SUPPORT
    /* Initialize GP */
    gp_init(APP_ENDPOINT1);
#endif

#if ZCL_OTA_SUPPORT
    /* Initialize OTA */
    ota_init(OTA_TYPE_CLIENT, (af_simple_descriptor_t *)&app_ep_simpleDesc, &app_otaInfo, &app_otaCb);
#endif

#if ZCL_WWAH_SUPPORT
    /* Initialize WWAH server */
    wwah_init(WWAH_TYPE_SERVER, (af_simple_descriptor_t *)&app_simpleDesc);
#endif

#if TEST_SAVE_ENERGY
    TL_ZB_TIMER_SCHEDULE(energy_timerCb, NULL, TIMEOUT_1SEC);
#else
    TL_ZB_TIMER_SCHEDULE(energy_timerCb, NULL, TIMEOUT_1MIN);
#endif

    if (zb_getLocalShortAddr() >= 0xFFF8) {
        light_blink_start(90, 250, 750);
    }

    APP_DEBUG(UART_PRINTF_MODE, "zb_getLocalShortAddr: 0x%04x\r\n", zb_getLocalShortAddr());

//    TL_ZB_TIMER_SCHEDULE(app_time_cmdCb, NULL, TIMEOUT_10SEC);

//    APP_DEBUG(UART_PRINTF_MODE, "FLASH_ADDR_OF_OTA_IMAGE: 0x%08x\r\n", FLASH_ADDR_OF_OTA_IMAGE);
}

void app_task(void) {

    if (!boot_announce_sent && zb_isDeviceJoinedNwk()) {
        zb_zdoSendDevAnnance();
        boot_announce_sent = true;
    }

    factoryRst_handler();
    button_handler();
    bl0937_process();
    monitoring_handler();

    if (BDB_STATE_GET() == BDB_STATE_IDLE && !button_idle()) {
        report_handler();
    }
}


extern volatile u16 T_evtExcept[4];

static void app_sysException(void) {

    APP_DEBUG(UART_PRINTF_MODE, "app_sysException, line: %d, event: %d, reset\r\n", T_evtExcept[0], T_evtExcept[1]);

#if 1
    SYSTEM_RESET();
#else
    led_on(LED_STATUS);
    while(1);
#endif
}

/*********************************************************************
 * @fn      user_init
 *
 * @brief   User level initialization code.
 *
 * @param   isRetention - if it is waking up with ram retention.
 *
 * @return  None
 */
void user_init(bool isRetention)
{
    (void)isRetention;

    /* Initialize LEDs*/
    led_init();

    factoryRst_init();

    /* Initialize Stack */
    stack_init();

    /* Initialize user application */
    user_app_init();

    /* Register except handler for test */
    sys_exceptHandlerRegister(app_sysException);


    /* User's Task */
#if ZBHCI_EN
    zbhciInit();
    ev_on_poll(EV_POLL_HCI, zbhciTask);
#endif
    ev_on_poll(EV_POLL_IDLE, app_task);

    /* Read the pre-install code from NV */
    if(bdb_preInstallCodeLoad(&g_appCtx.tcLinkKey.keyType, g_appCtx.tcLinkKey.key) == RET_OK){
        g_bdbCommissionSetting.linkKey.tcLinkKey.keyType = g_appCtx.tcLinkKey.keyType;
        g_bdbCommissionSetting.linkKey.tcLinkKey.key = g_appCtx.tcLinkKey.key;
    }

    /* Set default reporting configuration */
    uint8_t reportableChange = 0x01;

    /* OnOff */
    bdb_defaultReportingCfg(APP_ENDPOINT1, HA_PROFILE_ID, ZCL_CLUSTER_GEN_ON_OFF, ZCL_ATTRID_ONOFF,
            0, 65000, (uint8_t *)&reportableChange);

    uint16_t reportableChange_u16 = 0x01;

    /* Energy */
    uint64_t reportableChange_u64 = 0x10;
    bdb_defaultReportingCfg(APP_ENDPOINT1, HA_PROFILE_ID, ZCL_CLUSTER_SE_METERING,
            ZCL_ATTRID_CURRENT_SUMMATION_DELIVERD, 10, 65000, (uint8_t *)&reportableChange_u64);

    /* Voltage */
    reportableChange_u16 = 500;
    bdb_defaultReportingCfg(APP_ENDPOINT1, HA_PROFILE_ID, ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_RMS_VOLTAGE, 10, 65000, (uint8_t *)&reportableChange_u16);

    /* Current */
    reportableChange_u16 = 5;
    bdb_defaultReportingCfg(APP_ENDPOINT1, HA_PROFILE_ID, ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_RMS_CURRENT, 10, 65000, (uint8_t *)&reportableChange_u16);

    /* Power */
    int16_t reportableChange_s16 = 500;
    bdb_defaultReportingCfg(APP_ENDPOINT1, HA_PROFILE_ID, ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ACTIVE_POWER, 10, 65000, (uint8_t *)&reportableChange_s16);

    /* Initialize BDB */
    bdb_init((af_simple_descriptor_t *)&app_ep_simpleDesc, &g_bdbCommissionSetting, &g_zbBdbCb, 1);

    rf_setTxPower(MY_RF_POWER_INDEX);
}

static uint8_t checksum(uint8_t *data, uint16_t length) {

    uint8_t crc8 = 0;

    for(uint8_t i = 0; i < (length - 1); i++) {
        crc8 += data[i];
    }

    return crc8;
}

#if UART_PRINTF_MODE
static void print_setting_sr(nv_sts_t st, socket_settings_t *socket_settings_tmp, bool save) {

    APP_DEBUG(DEBUG_SAVE_EN, "Settings %s. Return: %s\r\n", save?"saved":"restored", st==NV_SUCC?"Ok":"Error");

    APP_DEBUG(DEBUG_SAVE_EN, "status_onoff:     0x%02x\r\n", socket_settings_tmp->status_onoff);
    APP_DEBUG(DEBUG_SAVE_EN, "startUpOnOff:     0x%02x\r\n", socket_settings_tmp->startUpOnOff);
    APP_DEBUG(DEBUG_SAVE_EN, "current_max:      0x%04x\r\n", socket_settings_tmp->current_max);
    APP_DEBUG(DEBUG_SAVE_EN, "adjust_current:   %s\r\n", float_to_string(socket_settings_tmp->adjust_current, 2));
    APP_DEBUG(DEBUG_SAVE_EN, "power_max:        0x%04x\r\n", socket_settings_tmp->power_max);
    APP_DEBUG(DEBUG_SAVE_EN, "adjust_power:     %s\r\n", float_to_string(socket_settings_tmp->adjust_power, 2));
    APP_DEBUG(DEBUG_SAVE_EN, "voltage_min:      0x%04x\r\n", socket_settings_tmp->voltage_min);
    APP_DEBUG(DEBUG_SAVE_EN, "voltage_max:      0x%04x\r\n", socket_settings_tmp->voltage_max);
    APP_DEBUG(DEBUG_SAVE_EN, "adjust_voltage:   %s\r\n", float_to_string(socket_settings_tmp->adjust_voltage, 2));
    APP_DEBUG(DEBUG_SAVE_EN, "time_reload:      0x%04x\r\n", socket_settings_tmp->time_reload);
    APP_DEBUG(DEBUG_SAVE_EN, "protect_control:  0x%02x\r\n", socket_settings_tmp->protect_control);
    APP_DEBUG(DEBUG_SAVE_EN, "auto_restart:     0x%02x\r\n", socket_settings_tmp->auto_restart);
    APP_DEBUG(DEBUG_SAVE_EN, "key_lock:         0x%02x\r\n", socket_settings_tmp->key_lock);
    APP_DEBUG(DEBUG_SAVE_EN, "led_control:      0x%02x\r\n", socket_settings_tmp->led_control);

}
#endif

nv_sts_t socket_settings_save() {
    nv_sts_t st = NV_SUCC;

#if NV_ENABLE

    APP_DEBUG(DEBUG_SAVE_EN, "Saved socket settings\r\n");

    socket_settings.crc = checksum((uint8_t*)&socket_settings, sizeof(socket_settings_t));
    st = nv_flashWriteNew(1, NV_MODULE_APP,  NV_ITEM_APP_USER_CFG, sizeof(socket_settings_t), (uint8_t*)&socket_settings);

#else
    st = NV_ENABLE_PROTECT_ERROR;
#endif

#if UART_PRINTF_MODE && DEBUG_SAVE_EN
        print_setting_sr(st, &socket_settings, false);
#endif
    return st;
}

nv_sts_t socket_settings_restore() {
    nv_sts_t st = NV_SUCC;

#if NV_ENABLE

    socket_settings_t socket_settings_tmp;

    st = nv_flashReadNew(1, NV_MODULE_APP,  NV_ITEM_APP_USER_CFG, sizeof(socket_settings_t), (uint8_t*)&socket_settings_tmp);

    if (st == NV_SUCC && socket_settings_tmp.crc == checksum((uint8_t*)&socket_settings_tmp, sizeof(socket_settings_t))) {

#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_SAVE_EN, "Restored socket settings\r\n");
#if DEBUG_SAVE_EN
        print_setting_sr(st, &socket_settings_tmp, false);
#endif
#endif

    } else {
        /* default config */
        APP_DEBUG(DEBUG_SAVE_EN, "Default socket settings \r\n");
        socket_settings_tmp.startUpOnOff = ZCL_START_UP_ONOFF_SET_ONOFF_TO_OFF;
        socket_settings_tmp.status_onoff = ZCL_ONOFF_STATUS_OFF;
        socket_settings_tmp.current_max = DEFAULT_CURRENT_MAX;
        socket_settings.adjust_current = DEFAULT_ADJUST_CURRENT;
        socket_settings_tmp.power_max = DEFAULT_POWER_MAX;
        socket_settings_tmp.adjust_power = DEFAULT_ADJUST_POWER;
        socket_settings_tmp.voltage_min = DEFAULT_VOLTAGE_MIN;
        socket_settings_tmp.voltage_max = DEFAULT_VOLTAGE_MAX;
        socket_settings_tmp.adjust_voltage = DEFAULT_ADJUST_VOLTAGE;
        socket_settings_tmp.time_reload = DEFAULT_TIME_RELOAD;
        socket_settings_tmp.protect_control = DEFAULT_PROTECT_CONTROL;
        socket_settings_tmp.auto_restart = DEFAULT_AUTORESTART;
        socket_settings_tmp.key_lock = DEFAULT_KEY_LOCK;
        socket_settings_tmp.led_control = DEFAULT_LED_CONTROL;
    }

    memcpy(&socket_settings, &socket_settings_tmp, (sizeof(socket_settings_t)));
    g_zcl_onOffAttrs.onOff = socket_settings.status_onoff;
    g_zcl_onOffAttrs.startUpOnOff = socket_settings.startUpOnOff;
    g_zcl_onOffAttrs.key_lock = socket_settings.key_lock;
    g_zcl_onOffAttrs.led_control = socket_settings.led_control;
    g_zcl_msAttrs.current_max = socket_settings.current_max;
    g_zcl_msAttrs.adjust_current = socket_settings.adjust_current;
    g_zcl_msAttrs.power_max = socket_settings.power_max;
    g_zcl_msAttrs.adjust_power = socket_settings.adjust_power;
    g_zcl_msAttrs.voltage_min = socket_settings.voltage_min;
    g_zcl_msAttrs.voltage_max = socket_settings.voltage_max;
    g_zcl_msAttrs.adjust_voltage = socket_settings.adjust_voltage;
    g_zcl_msAttrs.time_reload = socket_settings.time_reload;
    g_zcl_msAttrs.protect_control = socket_settings.protect_control;
    g_zcl_msAttrs.auto_restart = socket_settings.auto_restart;

#else
    st = NV_ENABLE_PROTECT_ERROR;
#endif

    return st;
}

void socket_settints_default() {
    socket_settings.startUpOnOff = ZCL_START_UP_ONOFF_SET_ONOFF_TO_OFF;
    socket_settings.status_onoff = ZCL_ONOFF_STATUS_OFF;
    socket_settings.current_max = DEFAULT_CURRENT_MAX;
    socket_settings.adjust_current = DEFAULT_ADJUST_CURRENT;
    socket_settings.power_max = DEFAULT_POWER_MAX;
    socket_settings.adjust_power = DEFAULT_ADJUST_POWER;
    socket_settings.voltage_min = DEFAULT_VOLTAGE_MIN;
    socket_settings.voltage_max = DEFAULT_VOLTAGE_MAX;
    socket_settings.adjust_voltage = DEFAULT_ADJUST_VOLTAGE;
    socket_settings.time_reload = DEFAULT_TIME_RELOAD;
    socket_settings.protect_control = DEFAULT_PROTECT_CONTROL;
    socket_settings.auto_restart = DEFAULT_AUTORESTART;
    socket_settings.key_lock = DEFAULT_KEY_LOCK;
    socket_settings.led_control = DEFAULT_LED_CONTROL;

    socket_settings_save();

    g_zcl_onOffAttrs.onOff = socket_settings.status_onoff;
    g_zcl_onOffAttrs.startUpOnOff = socket_settings.startUpOnOff;
    g_zcl_onOffAttrs.key_lock = socket_settings.key_lock;
    g_zcl_onOffAttrs.led_control = socket_settings.led_control;
    g_zcl_msAttrs.current_max = socket_settings.current_max;
    g_zcl_msAttrs.adjust_current = socket_settings.adjust_current;
    g_zcl_msAttrs.power_max = socket_settings.power_max;
    g_zcl_msAttrs.adjust_power = socket_settings.adjust_power;
    g_zcl_msAttrs.voltage_min = socket_settings.voltage_min;
    g_zcl_msAttrs.voltage_max = socket_settings.voltage_max;
    g_zcl_msAttrs.adjust_voltage = socket_settings.adjust_voltage;
    g_zcl_msAttrs.time_reload = socket_settings.time_reload;
    g_zcl_msAttrs.protect_control = socket_settings.protect_control;
    g_zcl_msAttrs.auto_restart = socket_settings.auto_restart;

    bl0937_adjustVoltage(socket_settings.adjust_voltage);
    bl0937_adjustCurrent(socket_settings.adjust_current);
    bl0937_adjustPower(socket_settings.adjust_power);

    cmdOnOff_off();
}

