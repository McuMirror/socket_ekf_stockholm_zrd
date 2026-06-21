#include "app_socket.h"

static relay_t relay;

/*********************************************************************
 * @fn      set_relay_onoffCb
 *
 * @brief   timer callback to turn the control pin off after 10 ms
 *
 * @param   args - pointer to the gpio pin number
 *
 * @return  -1 to stop the timer
 */
static int32_t set_relay_onoffCb(void *args) {
    uint32_t *pin = (uint32_t*)args;

    drv_gpio_write(*pin, OFF);

    return -1;
}

/*********************************************************************
 * @fn      set_relay_on
 *
 * @brief   pulse the relay "on" pin for 10 ms
 *
 * @param   None
 *
 * @return  None
 */
static void set_relay_on(void) {

    drv_gpio_write(relay.relay_pin_on, ON);

    TL_ZB_TIMER_SCHEDULE(set_relay_onoffCb, &(relay.relay_pin_on), 10);
}

/*********************************************************************
 * @fn      set_relay_off
 *
 * @brief   pulse the relay "off" pin for 10 ms
 *
 * @param   None
 *
 * @return  None
 */
static void set_relay_off(void) {

    drv_gpio_write(relay.relay_pin_off, ON);

    TL_ZB_TIMER_SCHEDULE(set_relay_onoffCb, &(relay.relay_pin_off), 10);
}

/*********************************************************************
 * @fn      check_first_start
 *
 * @brief   apply the startup on/off configuration on boot
 *
 * @param   None
 *
 * @return  None
 */
static void check_first_start(void) {

    switch(socket_settings.startUpOnOff) {
        case ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS:
            APP_DEBUG(DEBUG_ONOFF_EN, "Startup with ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS\r\n");
            if (socket_settings.status_onoff) cmdOnOff_on();
            else cmdOnOff_off();
            break;
        case ZCL_START_UP_ONOFF_SET_ONOFF_TOGGLE:
            APP_DEBUG(DEBUG_ONOFF_EN, "Startup with ZCL_START_UP_ONOFF_SET_ONOFF_TOGGLE\r\n");
            cmdOnOff_toggle();
            break;
        case ZCL_START_UP_ONOFF_SET_ONOFF_TO_ON:
            APP_DEBUG(DEBUG_ONOFF_EN, "Startup with ZCL_START_UP_ONOFF_SET_ONOFF_TO_ON\r\n");
            cmdOnOff_on();
            break;
        case ZCL_START_UP_ONOFF_SET_ONOFF_TO_OFF:
            APP_DEBUG(DEBUG_ONOFF_EN, "Startup with ZCL_START_UP_ONOFF_SET_ONOFF_TO_OFF\r\n");
            cmdOnOff_off();
            break;
        default:
            APP_DEBUG(DEBUG_ONOFF_EN, "Startup with UNKNOWN\r\n");
            cmdOnOff_off();
            break;
    }
}

/*********************************************************************
 * @fn      set_relay_status
 *
 * @brief   set relay and led according to the given on/off status
 *
 * @param   status - 0 = off, non-zero = on
 *
 * @return  None
 */
void set_relay_status(uint8_t status) {
    set_led_status(status);
    if (status) {
        set_relay_on();
        clear_auto_restart();
    } else {
        set_relay_off();
    }
}

/*********************************************************************
 * @fn      relay_init
 *
 * @brief   initialise relay gpios and apply the startup on/off
 *          configuration
 *
 * @param   None
 *
 * @return  None
 */
void relay_init(void) {
    relay.relay_pin_on = RELAY_ON_GPIO;
    relay.relay_pin_off = RELAY_OFF_GPIO;
    check_first_start();
}
