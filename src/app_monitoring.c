#include "app_socket.h"
#include "bl0937.h"
#include "app_monitoring.h"

#define ID_ENERGY               0x2DEF
#define TOP_MASK                0xFFFF
#define FLASH_SAVE_SIZE         0x10
#define PROTECT_VOLTAGE         0x01
#define PROTECT_CURRENT         0x02
#define PROTECT_POWER           0x04
#define PROTECT_VOLTAGE_SAVE    0x08
#define VOLTAGE_ARRAY_NUM       10

ev_timer_event_t *timerAutoRestartEvt = NULL;

static energy_cons_t energy_cons = {0};
static uint8_t  default_energy_cons = false;
static uint8_t  protect_on = 0;
static bool     new_energy_save = false;

static uint16_t current, current_prot[4], voltage, voltage_prot[4], voltage_array[VOLTAGE_ARRAY_NUM];
static int16_t  power, power_prot[4];
static uint64_t cur_sum_delivered;
static uint32_t new_energy, old_energy = 0;
static uint8_t  first_start = true;
static uint8_t  onoff_state = 0;
static uint8_t  count_start = 4;

static uint8_t checksum(uint8_t *data, uint16_t length) {

    uint8_t crc8 = 0;

    for(uint8_t i = 0; i < (length - 1); i++) {
        crc8 += data[i];
    }

    crc8 += 0x58;

    return ~crc8;
}

static void energy_saveCb(void *args) {

//    light_blink_start(1, 250, 250);

    if (default_energy_cons) {
        energy_cons.crc = checksum((uint8_t*)&(energy_cons), sizeof(energy_cons_t));
        flash_erase(energy_cons.flash_addr_start);
        flash_write(energy_cons.flash_addr_start, sizeof(energy_cons_t), (uint8_t*)&(energy_cons));
        default_energy_cons = false;
#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_SAVE_EN, "Save energy_cons: %s to flash address - 0x%x\r\n", digit64toString(energy_cons.energy), energy_cons.flash_addr_start);
//        APP_DEBUG(DEBUG_SAVE_EN, "id: 0x%04x, crc1: 0x%x, crc2: 0x%x\r\n", energy_cons.id, checksum((uint8_t*)&energy_cons, sizeof(energy_cons_t)), energy_cons.crc);
#endif /* UART_PRINTF_MODE */
    } else {
        energy_cons.flash_addr_start += FLASH_SAVE_SIZE;
        if (energy_cons.flash_addr_start == END_USER_DATA) {
            energy_cons.flash_addr_start = BEGIN_USER_DATA;
        }
        if (energy_cons.flash_addr_start % FLASH_SECTOR_SIZE == 0) {
            flash_erase(energy_cons.flash_addr_start);
        }
        energy_cons.top++;
        energy_cons.top &= TOP_MASK;
        energy_cons.crc = checksum((uint8_t*)&(energy_cons), sizeof(energy_cons_t));
        flash_write(energy_cons.flash_addr_start, sizeof(energy_cons_t), (uint8_t*)&(energy_cons));
#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_SAVE_EN, "Save energy_cons to flash address - 0x%x\r\n", energy_cons.flash_addr_start);
        APP_DEBUG(DEBUG_SAVE_EN, "id: 0x%04x, crc1: 0x%x, crc2: 0x%x\r\n", energy_cons.id, checksum((uint8_t*)&energy_cons, sizeof(energy_cons_t)), energy_cons.crc);
#endif /* UART_PRINTF_MODE */

    }

    new_energy_save = false;
}


static void init_default_energy_cons() {
    flash_erase_sector(BEGIN_USER_DATA);
    memset(&energy_cons, 0, sizeof(energy_cons_t));
    energy_cons.id = ID_ENERGY;
    energy_cons.flash_addr_start = BEGIN_USER_DATA;
//    energy_cons.flash_addr_end = END_USER_DATA;
    g_zcl_seAttrs.cur_sum_delivered = 0;
    default_energy_cons = true;
    energy_saveCb(NULL);
}

static int32_t auto_restartCb(void *args) {

    if (protect_on & (PROTECT_VOLTAGE | PROTECT_CURRENT | PROTECT_POWER)) {
        if (socket_settings.status_onoff) cmdOnOff_off();
        return 0;
    }

    if (socket_settings.auto_restart && ((protect_on & PROTECT_VOLTAGE) || (protect_on & PROTECT_VOLTAGE_SAVE)) &&
            !(protect_on & PROTECT_CURRENT) && !(protect_on & PROTECT_POWER) && onoff_state) {
        cmdOnOff_on();
    }

    timerAutoRestartEvt = NULL;
    return -1;
}

void monitoring_handler() {

    static uint32_t monitoring_time = 0;
    uint32_t voltage_summ = 0;
    uint16_t v;
    uint8_t i;

    if(clock_time_exceed(monitoring_time, TIMEOUT_TICK_1SEC)) {
        monitoring_time = clock_time();

        voltage = bl0937_getVoltage();
        current = bl0937_getCurrent();
        power = bl0937_getActivePower();
        new_energy = bl0937_getEnergy();

        if (first_start) {
#if UART_PRINTF_MODE
            APP_DEBUG(DEBUG_MONITORING_EN, "first start: %d\r\n", count_start);
#endif
            if (count_start) {
                count_start--;
                return;
            }
            first_start = false;
            old_energy = new_energy;
            for (i = 0; i < 4; i++) {
                current_prot[i] = current;
                power_prot[i] = power;
                voltage_prot[i] = voltage;
            }
            for (i = 0; i < VOLTAGE_ARRAY_NUM; i++) {
                voltage_array[i] = voltage;
            }
            return;
        }

#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_MONITORING_EN, "current:    %d\r\n", current);
        APP_DEBUG(DEBUG_MONITORING_EN, "voltage:    %d\r\n", voltage);
        APP_DEBUG(DEBUG_MONITORING_EN, "power:      %d, 0x%04x\r\n", power, power);
        APP_DEBUG(DEBUG_MONITORING_EN, "energy:     %d\r\n", new_energy);
        APP_DEBUG(DEBUG_MONITORING_EN, "new_energy: %d,%s old_en:  %d\r\n", new_energy, new_energy > 9?"\t":"\t\t", old_energy);
#endif
        memcpy(voltage_array, &voltage_array[1], (sizeof(uint16_t)*(VOLTAGE_ARRAY_NUM - 1)));
        voltage_array[VOLTAGE_ARRAY_NUM - 1] = voltage;

        for(i = 0; i < VOLTAGE_ARRAY_NUM; i++) {
            voltage_summ += voltage_array[i];
        }

        v = (uint16_t)(voltage_summ / VOLTAGE_ARRAY_NUM);

#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_MONITORING_EN, "voltage_s:  %d\r\n", v);
#endif

        zcl_setAttrVal(APP_ENDPOINT1, ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT, ZCL_ATTRID_RMS_VOLTAGE, (uint8_t*)&v);
        zcl_setAttrVal(APP_ENDPOINT1, ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT, ZCL_ATTRID_RMS_CURRENT, (uint8_t*)&current);
        zcl_setAttrVal(APP_ENDPOINT1, ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT, ZCL_ATTRID_ACTIVE_POWER, (uint8_t*)&power);

        if (new_energy > old_energy) {
//            APP_DEBUG(DEBUG_MONITORING_EN, "new_energy: %d > old_energy: %d\r\n", new_energy, old_energy);
            cur_sum_delivered = (uint64_t)(energy_cons.energy + (new_energy - old_energy)) & 0xFFFFFFFFFFFF;
            old_energy = new_energy;
            energy_cons.energy = cur_sum_delivered;
            energy_save();
            zcl_setAttrVal(APP_ENDPOINT1, ZCL_CLUSTER_SE_METERING, ZCL_ATTRID_CURRENT_SUMMATION_DELIVERD, (uint8_t*)&cur_sum_delivered);
        }

        protect_on &= PROTECT_VOLTAGE_SAVE;

        if (socket_settings.current_max &&
                current_prot[0] > socket_settings.current_max && current_prot[1] > socket_settings.current_max &&
                current_prot[2] > socket_settings.current_max && current_prot[3] > socket_settings.current_max &&
                current > socket_settings.current_max) {
//            APP_DEBUG(DEBUG_MONITORING_EN, "current\r\n");
            protect_on |= PROTECT_CURRENT;
        }

        if (socket_settings.power_max &&
                power_prot[0] > socket_settings.power_max && power_prot[1] > socket_settings.power_max &&
                power_prot[2] > socket_settings.power_max && power_prot[3] > socket_settings.power_max &&
                power > socket_settings.power_max) {
//            APP_DEBUG(DEBUG_MONITORING_EN, "power: %d, power_max: %d\r\n", power, socket_settings.power_max);
            protect_on |= PROTECT_POWER;
        }

        if ((socket_settings.voltage_min &&
                voltage_prot[0] < socket_settings.voltage_min && voltage_prot[1] < socket_settings.voltage_min &&
                voltage_prot[2] < socket_settings.voltage_min && voltage_prot[3] < socket_settings.voltage_min &&
                voltage < socket_settings.voltage_min) || (socket_settings.voltage_max &&
                voltage_prot[0] > socket_settings.voltage_max && voltage_prot[1] > socket_settings.voltage_max &&
                voltage_prot[2] > socket_settings.voltage_max && voltage_prot[3] > socket_settings.voltage_max &&
                voltage > socket_settings.voltage_max)) {
//            APP_DEBUG(DEBUG_MONITORING_EN, "voltage_prot: %d, voltage: %d\r\n", voltage_prot, voltage);
            protect_on |= PROTECT_VOLTAGE | PROTECT_VOLTAGE_SAVE;
        }

        for(i = 0; i < 4; i++) {
            if (i == 3) {
                current_prot[i] = current;
                power_prot[i] = power;
                voltage_prot[i] = voltage;
            } else {
                current_prot[i] = current_prot[i+1];
                power_prot[i] = power_prot[i+1];
                voltage_prot[i] = voltage_prot[i+1];
            }
        }

        if (socket_settings.protect_control && protect_on && socket_settings.status_onoff) {
            if (!timerAutoRestartEvt) {
                onoff_state = socket_settings.status_onoff;
                cmdOnOff_off();
                timerAutoRestartEvt = TL_ZB_TIMER_SCHEDULE(auto_restartCb, NULL, (socket_settings.time_reload * 1000));
            }
        }
    }
}

int32_t energy_timerCb(void *args) {

    if (new_energy_save) {
        TL_SCHEDULE_TASK(energy_saveCb, NULL);
    }

    return 0;
}

void clear_auto_restart() {

    protect_on = 0;

    if (timerAutoRestartEvt) TL_ZB_TIMER_CANCEL(&timerAutoRestartEvt);

}

void energy_restore() {

    energy_cons_t energy_curr, energy_next;
    uint8_t find_config = false;

    uint32_t flash_addr = BEGIN_USER_DATA;

    flash_read_page(flash_addr, sizeof(energy_cons_t), (uint8_t*)&energy_curr);

    if (energy_curr.id != ID_ENERGY || checksum((uint8_t*)&energy_curr, sizeof(energy_cons_t)) != energy_curr.crc) {
#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_SAVE_EN, "No saved energy_cons! Init.\r\n");
#endif /* UART_PRINTF_MODE */
        init_default_energy_cons();
        return;
    }

    flash_addr += FLASH_SAVE_SIZE;

    while(flash_addr < END_USER_DATA) {
        flash_read_page(flash_addr, sizeof(energy_cons_t), (uint8_t*)&energy_next);
        if (energy_next.id == ID_ENERGY && checksum((uint8_t*)&energy_next, sizeof(energy_cons_t)) == energy_next.crc) {
            if ((energy_curr.top + 1) == energy_next.top || (energy_curr.top == TOP_MASK && energy_next.top == 0)) {
                memcpy(&energy_curr, &energy_next, sizeof(energy_cons_t));
                flash_addr += FLASH_SAVE_SIZE;
                continue;
            }
            find_config = true;
            break;
        }
        find_config = true;
        break;
    }

    if (find_config) {
        memcpy(&energy_cons, &energy_curr, sizeof(energy_cons_t));
        energy_cons.flash_addr_start = flash_addr-FLASH_SAVE_SIZE;
        g_zcl_seAttrs.cur_sum_delivered = energy_cons.energy;
#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_SAVE_EN, "Read energy_cons: %s from flash address - 0x%x\r\n", digit64toString(energy_cons.energy), energy_cons.flash_addr_start);
#endif /* UART_PRINTF_MODE */
    } else {
#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_SAVE_EN, "No active saved energy_cons! Reinit.\r\n");
#endif /* UART_PRINTF_MODE */
        init_default_energy_cons();
    }

}

void energy_save() {

    new_energy_save = true;
}


void energy_remove() {

#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_SAVE_EN, "Energy removed\r\n");
#endif /* UART_PRINTF_MODE */

    init_default_energy_cons();
}

#if TEST_SAVE_ENERGY
void set_energy() {
    new_energy = old_energy + 1;
    if (new_energy > old_energy) {
//        APP_DEBUG(DEBUG_SAVE_EN, "new_energy: %d > old_energy: %d\r\n", new_energy, old_energy);
        cur_sum_delivered = (uint64_t)(energy_cons.energy + (new_energy - old_energy)) & 0xFFFFFFFFFFFF;
        old_energy = new_energy;
        energy_cons.energy = cur_sum_delivered;
        energy_save();
        zcl_setAttrVal(APP_ENDPOINT1, ZCL_CLUSTER_SE_METERING, ZCL_ATTRID_CURRENT_SUMMATION_DELIVERD, (uint8_t*)&cur_sum_delivered);
    }
}
#endif

void reset_voltage() {
    first_start = true;
    count_start = 4;
}
