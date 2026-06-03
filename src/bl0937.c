#include "app_socket.h"
#include "bl0937.h"

#define V_REF                  1.218f

#define R_CURRENT              0.001f

#define R_VOLTAGE              ((3 * 680) + 1)

#define F_OSC                  2000000u

#define SEL_TOGGLE_US          1000000ul

#define PULSE_TIMEOUT_US       2000000ul
#define PULSE_TIMEOUT_TICKS    (PULSE_TIMEOUT_US * sys_tick_per_us)

static uint32_t _cf_pin;
static uint32_t _cf1_pin;
static uint32_t _sel_pin;

static float _current_resistor = R_CURRENT;
static float _voltage_resistor = R_VOLTAGE;
static float _vref = V_REF;

static float _current_multiplier;
static float _voltage_multiplier;
static float _power_multiplier;

static volatile uint32_t _last_cf_tick = 0;
static volatile uint32_t _last_cf1_tick = 0;

static volatile uint32_t _power_pulse_ticks = 0;
static volatile uint32_t _pulse_count = 0;

static volatile uint32_t _current_pulse_ticks = 0;
static volatile uint32_t _voltage_pulse_ticks = 0;

static volatile uint32_t _last_current_tick = 0;
static volatile uint32_t _last_voltage_tick = 0;

static float _current = 0;
static uint16_t _voltage = 0;
static uint16_t _power = 0;

static uint8_t _current_mode = 0;
static volatile uint8_t _cf1_mode;

static uint32_t _last_sel_tick = 0;

static void _calcDefaulPowertMultiplier(void)
{
    _power_multiplier   = (  50850000.0f * _vref * _vref * _voltage_resistor / _current_resistor / 48.0f / F_OSC) / 1.1371681416f;
}

static void _calcDefaultVoltageMultiplier(void)
{
    _voltage_multiplier = ( 221380000.0f * _vref * _voltage_resistor /  2.0f / F_OSC) / 1.0474137931f;
}

static void _calcDefaultCurrentMultiplier(void)
{
    _current_multiplier = ( 531500000.0f * _vref / _current_resistor / 24.0f / F_OSC) / 1.166666f;
}

static void _calcDefaultMultipliers(void)
{
    _calcDefaulPowertMultiplier();
    _calcDefaultVoltageMultiplier();
    _calcDefaultCurrentMultiplier();
}

_attribute_ram_code_ static void bl0937_cf_irq(void)
{
    uint32_t now = clock_time();
    uint32_t diff = now - _last_cf_tick;

    if (diff < PULSE_TIMEOUT_TICKS) {
        _power_pulse_ticks = diff;
    }

    _last_cf_tick = now;
    _pulse_count++;
}

_attribute_ram_code_ static void bl0937_cf1_irq(void)
{
    uint32_t now = clock_time();
    uint32_t diff = now - _last_cf1_tick;

    if (diff < PULSE_TIMEOUT_TICKS) {
        if (_cf1_mode == _current_mode) {
            _current_pulse_ticks = diff;
            _last_current_tick = now;
        } else {
            _voltage_pulse_ticks = diff;
            _last_voltage_tick = now;
        }
    }

    _last_cf1_tick = now;
}

void bl0937_init(uint32_t cf_pin, uint32_t cf1_pin, uint32_t sel_pin, uint8_t currentWhen)
{
    _cf_pin = cf_pin;
    _cf1_pin = cf1_pin;
    _sel_pin = sel_pin;
    _current_mode = currentWhen;

    drv_gpio_func_set(cf_pin);
    drv_gpio_input_en(cf_pin, 1);
    drv_gpio_up_down_resistor(cf_pin, PM_PIN_PULLUP_10K);

    drv_gpio_func_set(cf1_pin);
    drv_gpio_input_en(cf1_pin, 1);
    drv_gpio_up_down_resistor(cf1_pin, PM_PIN_PULLUP_10K);

    drv_gpio_func_set(sel_pin);
    drv_gpio_output_en(sel_pin, 1);

    _calcDefaultMultipliers();

    _cf1_mode = _current_mode;
    drv_gpio_write(sel_pin, _cf1_mode);

    _last_cf_tick = clock_time();
    _last_cf1_tick = clock_time();
    _last_current_tick = clock_time();
    _last_voltage_tick = clock_time();
    _last_sel_tick = clock_time();

    _current_pulse_ticks = 0;
    _voltage_pulse_ticks = 0;
    _power_pulse_ticks = 0;

    drv_gpio_irq_config(GPIO_IRQ_MODE, cf_pin, GPIO_FALLING_EDGE, bl0937_cf_irq);
    drv_gpio_irq_en(cf_pin);

    drv_gpio_irq_config(GPIO_IRQ_RISC0_MODE, cf1_pin, GPIO_FALLING_EDGE, bl0937_cf1_irq);
    drv_gpio_irq_risc0_en(cf1_pin);
}

void bl0937_process(void)
{
    uint32_t now;

    if (clock_time_exceed(_last_sel_tick, SEL_TOGGLE_US)) {
        now = clock_time();
        drv_gpio_write(_sel_pin, 1 - _cf1_mode);
        _cf1_mode = 1 - _cf1_mode;

        _last_cf1_tick = now + 50000 * sys_tick_per_us;
        _last_sel_tick = now;
    }

    if (clock_time_exceed(_last_current_tick, PULSE_TIMEOUT_US)) {
        _current_pulse_ticks = 0;
    }
    if (clock_time_exceed(_last_voltage_tick, PULSE_TIMEOUT_US)) {
        _voltage_pulse_ticks = 0;
    }
    if (clock_time_exceed(_last_cf_tick, PULSE_TIMEOUT_US)) {
        _power_pulse_ticks = 0;
    }
}

void bl0937_setMode(uint8_t mode)
{
    _cf1_mode = (mode == BL0937_MODE_CURRENT) ? _current_mode : 1 - _current_mode;
    drv_gpio_write(_sel_pin, _cf1_mode);

    _last_cf1_tick = clock_time() + 50000 * sys_tick_per_us;
    _last_sel_tick = clock_time();
}

uint8_t bl0937_getMode(void)
{
    return (_cf1_mode == _current_mode) ? BL0937_MODE_CURRENT : BL0937_MODE_VOLTAGE;
}

uint8_t bl0937_toggleMode(void)
{
    uint8_t new = (bl0937_getMode() == BL0937_MODE_CURRENT) ? BL0937_MODE_VOLTAGE : BL0937_MODE_CURRENT;
    bl0937_setMode(new);
    return new;
}

uint16_t bl0937_getCurrent(void)
{
    if (_current_pulse_ticks > 0) {
        uint32_t us = _current_pulse_ticks / sys_tick_per_us;
        _current = _current_multiplier / (float)us;
    } else {
        _current = 0;
    }

    return (uint16_t)(_current * 100.0f);
}

uint16_t bl0937_getVoltage(void)
{
    if (_voltage_pulse_ticks > 0) {
        uint32_t us = _voltage_pulse_ticks / sys_tick_per_us;
        _voltage = (uint16_t)(_voltage_multiplier * 100.0f / (float)us);
    } else {
        _voltage = 0;
    }

    return _voltage;
}

uint32_t bl0937_getEnergy(void)
{
    return (uint32_t)(_pulse_count * _power_multiplier / 3600000.0f);
}

uint16_t bl0937_getActivePower(void)
{
    if (_power_pulse_ticks > 0) {
        uint32_t us = _power_pulse_ticks / sys_tick_per_us;
        _power = (uint16_t)(_power_multiplier * 100.0f / (float)us);
    } else {
        _power = 0;
    }

    return _power;
}

void bl0937_resetEnergy(void)
{
    _pulse_count = 0;
}

void bl0937_setResistors(float current, float voltage_upstream, float voltage_downstream)
{
    if (voltage_downstream > 0) {
        _current_resistor = current;
        _voltage_resistor = (voltage_upstream + voltage_downstream) / voltage_downstream;
        _calcDefaultMultipliers();
    }
}

void bl0937_expectedCurrent(float value)
{
    if (_current == 0) bl0937_getCurrent();
    if (_current > 0) _current_multiplier *= (value / _current);
}

void bl0937_expectedVoltage(uint16_t value)
{
    if (_voltage == 0) bl0937_getVoltage();
    if (_voltage > 0) _voltage_multiplier *= ((float)value / (float)_voltage);
}

void bl0937_expectedActivePower(uint16_t value)
{
    if (_power == 0) bl0937_getActivePower();
    if (_power > 0) _power_multiplier *= ((float)value / (float)_power);
}

void bl0937_adjustVoltage(float adjust) {
    zcl_msAttr_t *ms_attrs = zcl_msAttrsGet();
    ms_attrs->adjust_voltage = adjust;
    if (socket_settings.adjust_voltage != adjust) {
        socket_settings.adjust_voltage = adjust;
        socket_settings_save();
    }
    _calcDefaultVoltageMultiplier();
    float factor = (100.0f + adjust) / 100.0f;
    _voltage_multiplier *= factor;
    reset_voltage();
}

void bl0937_adjustCurrent(float adjust) {
    zcl_msAttr_t *ms_attrs = zcl_msAttrsGet();
    ms_attrs->adjust_current = adjust;
    if (socket_settings.adjust_current != adjust) {
        socket_settings.adjust_current = adjust;
        socket_settings_save();
    }
    _calcDefaultCurrentMultiplier();
    float factor = (100.0f + adjust) / 100.0f;
    _current_multiplier *= factor;
}

void bl0937_adjustPower(float adjust) {
    zcl_msAttr_t *ms_attrs = zcl_msAttrsGet();
    ms_attrs->adjust_power = adjust;
    if (socket_settings.adjust_power != adjust) {
        socket_settings.adjust_power = adjust;
        socket_settings_save();
    }
    _calcDefaulPowertMultiplier();
    float factor = (100.0f + adjust) / 100.0f;
    _power_multiplier   *= factor;
}

void bl0937_setMultipliers(float current_mul, float voltage_mul, float power_mul)
{
    _current_multiplier = current_mul;
    _voltage_multiplier = voltage_mul;
    _power_multiplier = power_mul;
}

void bl0937_resetMultipliers(void)
{
    _calcDefaultMultipliers();
}
