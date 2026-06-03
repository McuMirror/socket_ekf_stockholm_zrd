#ifndef SRC_INCLUDE_APP_MONITORING_H_
#define SRC_INCLUDE_APP_MONITORING_H_

typedef struct __attribute__((packed)) {
    uint16_t id;                        /* ID_ENERGY                */
    uint16_t top;                       /* 0x0 .. 0xFFFF            */
    uint32_t flash_addr_start;          /* flash page address start */
    uint64_t energy :48;
    uint8_t  res    :8;
    uint8_t  crc    :8;
} energy_cons_t;

void monitoring_handler();
void energy_restore();
void energy_save();
void energy_remove();
void clear_auto_restart();
void set_energy();
int32_t energy_timerCb(void *args);
void reset_voltage();

#endif /* SRC_INCLUDE_APP_MONITORING_H_ */
