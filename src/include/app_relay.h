#ifndef SRC_INCLUDE_APP_RELAY_H_
#define SRC_INCLUDE_APP_RELAY_H_

typedef struct {
    uint32_t relay_pin_on;
    uint32_t relay_pin_off;
} relay_t;

void relay_init(void);
void set_relay_status(uint8_t status);

#endif /* SRC_INCLUDE_APP_RELAY_H_ */
