#include "app_socket.h"

static light_t light;

/*********************************************************************
 * @fn      led_on
 *
 * @brief   turn led on by driving the gpio pin high
 *
 * @param   pin - gpio pin number
 *
 * @return  None
 */
void led_on(uint32_t pin)
{
    drv_gpio_write(pin, LED_ON);
}

/*********************************************************************
 * @fn      led_off
 *
 * @brief   turn led off by driving the gpio pin low
 *
 * @param   pin - gpio pin number
 *
 * @return  None
 */
void led_off(uint32_t pin)
{
    drv_gpio_write(pin, LED_OFF);
}

/*********************************************************************
 * @fn      set_led_status
 *
 * @brief   set the status led according to the configured led mode
 *          (off / on / follows relay / inverted relay)
 *
 * @param   status - relay on/off state
 *
 * @return  None
 */
void set_led_status(bool status) {
#if !UART_PRINTF_MODE
    switch(socket_settings.led_control) {
        case CONTROL_LED_OFF:
            led_off(light.led_status_pin);
            break;
        case CONTROL_LED_ON:
            led_on(light.led_status_pin);
            break;
        case CONTROL_LED_ON_OFF:
            if (socket_settings.status_onoff) led_on(light.led_status_pin);
            else led_off(light.led_status_pin);
            break;
        case CONTROL_LED_OFF_ON:
            if (socket_settings.status_onoff) led_off(light.led_status_pin);
            else led_on(light.led_status_pin);
            break;
        default:
            break;
    }
#endif
}

/*********************************************************************
 * @fn      led_init
 *
 * @brief   initialise leds — configure gpios, turn both off
 *
 * @param   None
 *
 * @return  None
 */
void led_init(void) {
    TL_SETSTRUCTCONTENT(light, 0);
    light.timerLedEvt = NULL;
    light.led_net_pin = LED_NET_GPIO;
#if !UART_PRINTF_MODE
    light.led_status_pin = LED_STATUS_GPIO;
    led_off(light.led_status_pin);
#endif
    led_off(light.led_net_pin);

}


/*********************************************************************
 * @fn      zclLightTimerCb
 *
 * @brief   timer callback for network led blinking — toggles the
 *          led on/off according to the programmed blink pattern
 *
 * @param   arg - unused
 *
 * @return  interval in ms until next toggle, or -1 to stop
 */
int32_t zclLightTimerCb(void *arg)
{
    uint32_t interval = 0;

    if (light.timer_stop) {
        led_off(light.led_net_pin);
        light.timer_stop = false;
        light.times = 0;
        light.timerLedEvt = NULL;
        return -1;
    }

    if(light.sta == light.oriSta) {
        light.times--;
        if(light.times <= 0){
            led_off(light.led_net_pin);
            light.timerLedEvt = NULL;
            return -1;
        }
    }

    light.sta = !light.sta;
    if(light.sta) {
        led_on(light.led_net_pin);
        interval = light.ledOnTime;
    } else {
        led_off(light.led_net_pin);
        interval = light.ledOffTime;
    }

    return interval;
}

/*********************************************************************
 * @fn      light_blink_start
 *
 * @brief   start a blinking pattern on the network led
 *
 * @param   times - number of blink cycles
 *
 * @param   ledOnTime - on time in ms
 *
 * @param   ledOffTime - off time in ms
 *
 * @return  None
 */
void light_blink_start(uint8_t times, uint16_t ledOnTime, uint16_t ledOffTime)
{
    uint32_t interval = 0;
    light.times = times;

    if(!light.timerLedEvt){
        if(light.oriSta){
            led_off(light.led_net_pin);
            light.sta = 0;
            interval = ledOffTime;
        }else{
            led_on(light.led_net_pin);
            light.sta = 1;
            interval = ledOnTime;
        }
        light.ledOnTime = ledOnTime;
        light.ledOffTime = ledOffTime;

        light.timerLedEvt = TL_ZB_TIMER_SCHEDULE(zclLightTimerCb, NULL, interval);
    }
}

/*********************************************************************
 * @fn      light_blink_stop
 *
 * @brief   stop the blinking pattern and turn the network led off
 *
 * @param   None
 *
 * @return  None
 */
void light_blink_stop(void) {

    uint8_t ret = 0;

    if(light.timerLedEvt){
        ret = TL_ZB_TIMER_CANCEL(&light.timerLedEvt);
        if (ret == NO_TIMER_AVAIL || ret == SUCCESS) {
            light.timerLedEvt = NULL;
        } else if (ret == TIMER_CANCEL_NOT_ALLOWED) {
            light.timer_stop = true;
        }

        light.times = 0;
        led_off(light.led_net_pin);
//        if(light.oriSta){
//            led_on(light.led_net_pin);
//        }else{
//            led_off(light.led_net_pin);
//        }
    }
}
