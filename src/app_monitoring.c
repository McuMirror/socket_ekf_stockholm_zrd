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
#define VOLTAGE_ARRAY_NUM       5

ev_timer_event_t *timerAutoRestartEvt = NULL;

static energy_cons_t energy_cons = {0};
//static uint8_t  default_energy_cons = false;
static uint8_t  protect_on = 0;
static bool     new_energy_save = false;
//static uint32_t flash_addr_start = BEGIN_USER_DATA;

static uint16_t current, current_prot[4], voltage, voltage_prot[4], voltage_array[VOLTAGE_ARRAY_NUM];
static uint16_t power, power_prot[4];
static uint64_t cur_sum_delivered;
static uint32_t new_energy, old_energy = 0;
static uint8_t  first_start = true;
static uint8_t  onoff_state = 0;
static uint8_t  count_start = 4;

/*********************************************************************
 * @fn      checksum
 *
 * @brief   compute crc8 checksum for flash data validation
 *
 * @param   data - pointer to data buffer
 *
 * @param   length - size of data in bytes
 *
 * @return  computed checksum byte
 */
static uint8_t checksum(uint8_t *data, uint16_t length) {

    uint8_t crc8 = 0;

    for(uint16_t i = 0; i < (length - 1); i++) {
        crc8 += data[i];
    }

    crc8 += 0x58;

    return ~crc8;
}

/*********************************************************************
 * @fn      energy_saveCb
 *
 * @brief   write current energy record to flash with wear leveling
 *          advances to next slot, erases sectors as needed
 *
 * @param   args - unused
 *
 * @return  None
 */
static void energy_saveCb(void *args) {

    uint32_t energy_cons_size = sizeof(energy_cons_t);

    energy_cons.flash_addr_start += FLASH_SAVE_SIZE;
    if (energy_cons.flash_addr_start == END_USER_DATA) {
        energy_cons.flash_addr_start = BEGIN_USER_DATA;
    }
    if (energy_cons.flash_addr_start % FLASH_SECTOR_SIZE == 0) {
        flash_erase(energy_cons.flash_addr_start);
    }
    // last fragment in the sector?
    if ((energy_cons.flash_addr_start % FLASH_SECTOR_SIZE) == (FLASH_SECTOR_SIZE - FLASH_SAVE_SIZE)) {
        uint32_t next_sector = energy_cons.flash_addr_start + FLASH_SAVE_SIZE;
        if (next_sector >= END_USER_DATA) {
            next_sector = BEGIN_USER_DATA;
        }
        flash_erase(next_sector);
    }
    energy_cons.top++;
    energy_cons.top &= TOP_MASK;
    energy_cons.crc = checksum((uint8_t*)&energy_cons, energy_cons_size);
    flash_write(energy_cons.flash_addr_start, energy_cons_size, (uint8_t*)&energy_cons);
#if UART_PRINTF_MODE
    APP_DEBUG(DEBUG_SAVE_EN, "Save energy: %s to flash address - 0x%x\r\n", digit64toString(energy_cons.energy), energy_cons.flash_addr_start);
#endif

    new_energy_save = false;
}


/*********************************************************************
 * @fn      init_default_energy_cons
 *
 * @brief   erase and write a default (zero) energy record to flash
 *
 * @param   None
 *
 * @return  None
 */
static void init_default_energy_cons(void) {

    energy_cons_t *energy = &energy_cons;
    uint32_t energy_cons_size = sizeof(energy_cons_t);

    memset(energy, 0, energy_cons_size);

    energy->id = ID_ENERGY;
    energy->flash_addr_start = BEGIN_USER_DATA;
    energy->crc = checksum((uint8_t*)energy, energy_cons_size);
    flash_erase(energy->flash_addr_start);
    if (BEGIN_USER_DATA + FLASH_SECTOR_SIZE < END_USER_DATA) {
        flash_erase(BEGIN_USER_DATA + FLASH_SECTOR_SIZE);
    }
    flash_write(energy->flash_addr_start, energy_cons_size, (uint8_t*)energy);
    g_zcl_seAttrs.cur_sum_delivered = 0;

    APP_DEBUG(DEBUG_SAVE_EN, "Save default energy to flash address - 0x%x\r\n", energy->flash_addr_start);
//    APP_DEBUG(DEBUG_SAVE_EN, "id: 0x%04x, crc1: 0x%02x, crc2: 0x%02x\r\n", energy->id, checksum((uint8_t*)energy, energy_cons_size), energy->crc);
}


/*********************************************************************
 * @fn      auto_restartCb
 *
 * @brief   timer callback to turn socket back on after voltage
 *          protection expires (if auto_restart is enabled)
 *
 * @param   args - unused
 *
 * @return  0 to keep timer, -1 to stop
 */
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

/*********************************************************************
 * @fn      monitoring_handler
 *
 * @brief   main monitoring task — reads voltage, current, power and
 *          energy from bl0937 every second, updates zcl attributes,
 *          detects over-current / over-power / voltage faults, and
 *          triggers auto-restart if configured
 *
 * @param   None
 *
 * @return  None
 */
void monitoring_handler(void) {

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

/*********************************************************************
 * @fn      energy_timerCb
 *
 * @brief   periodic timer callback — schedules deferred energy save
 *          if a save was requested
 *
 * @param   args - unused
 *
 * @return  0 to keep timer running
 */
int32_t energy_timerCb(void *args) {

    if (new_energy_save) {
        TL_SCHEDULE_TASK(energy_saveCb, NULL);
    }

    return 0;
}

/*********************************************************************
 * @fn      clear_auto_restart
 *
 * @brief   clear all protection flags and cancel pending
 *          auto-restart timer
 *
 * @param   None
 *
 * @return  None
 */
void clear_auto_restart(void) {

    protect_on = 0;

    if (timerAutoRestartEvt) {
        TL_ZB_TIMER_CANCEL(&timerAutoRestartEvt);
        timerAutoRestartEvt = NULL;
    }

}

/*********************************************************************
 * @fn      energy_restore
 *
 * @brief   scan flash for the latest valid energy record by
 *          following the top chain through slots
 *
 * @param   None
 *
 * @return  None
 */
void energy_restore(void) {

    energy_cons_t energy_curr, energy_next;

    uint32_t flash_addr = BEGIN_USER_DATA;
//    uint32_t flash_addr_start = BEGIN_USER_DATA;
    uint32_t energy_cons_size = sizeof(energy_cons_t);

    while (flash_addr < END_USER_DATA) {
        flash_read_page(flash_addr, energy_cons_size, (uint8_t*)&energy_curr);
        if (energy_curr.id == ID_ENERGY && checksum((uint8_t*)&energy_curr, energy_cons_size) == energy_curr.crc) {
            break;
        }
        flash_addr += FLASH_SAVE_SIZE;
    }

    if (flash_addr >= END_USER_DATA) {
        APP_DEBUG(DEBUG_SAVE_EN, "No saved energy! Init.\r\n");
        init_default_energy_cons();
        return;
    } else {
        flash_addr += FLASH_SAVE_SIZE;
        while(flash_addr < END_USER_DATA) {
            flash_read_page(flash_addr, energy_cons_size, (uint8_t*)&energy_next);
            if (energy_next.id == ID_ENERGY && checksum((uint8_t*)&energy_next, energy_cons_size) == energy_next.crc) {
                if ((energy_curr.top + 1) == energy_next.top || (energy_curr.top == TOP_MASK && energy_next.top == 0)) {
                    memcpy(&energy_curr, &energy_next, energy_cons_size);
                    flash_addr += FLASH_SAVE_SIZE;
                    continue;
                }
                break;
            }
            break;
        }
        energy_curr.flash_addr_start = flash_addr - FLASH_SAVE_SIZE;
    }

    memcpy(&energy_cons, &energy_curr, energy_cons_size);
    g_zcl_seAttrs.cur_sum_delivered = energy_cons.energy;

#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_SAVE_EN, "Read energy_cons: %s from flash address - 0x%x\r\n", digit64toString(energy_cons.energy), energy_cons.flash_addr_start);
#endif /* UART_PRINTF_MODE */
}


/*********************************************************************
 * @fn      energy_save
 *
 * @brief   request an energy save — sets flag so the next
 *          timer tick schedules the actual flash write
 *
 * @param   None
 *
 * @return  None
 */
void energy_save(void) {

    new_energy_save = true;
}


/*********************************************************************
 * @fn      energy_remove
 *
 * @brief   erase all energy data and write a fresh default record
 *
 * @param   None
 *
 * @return  None
 */
void energy_remove(void) {

#if UART_PRINTF_MODE
        APP_DEBUG(DEBUG_SAVE_EN, "Energy removed\r\n");
#endif /* UART_PRINTF_MODE */

    init_default_energy_cons();
}

#if TEST_SAVE_ENERGY
/*********************************************************************
 * @fn      set_energy
 *
 * @brief   test helper — increments energy by 1 and saves
 *          (only compiled when TEST_SAVE_ENERGY is defined)
 *
 * @param   None
 *
 * @return  None
 */
void set_energy(void) {
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

/*********************************************************************
 * @fn      reset_voltage
 *
 * @brief   trigger re-initialization of voltage averaging and
 *          protection arrays on next monitoring cycle
 *
 * @param   None
 *
 * @return  None
 */
void reset_voltage(void) {
    first_start = true;
    count_start = 4;
}
