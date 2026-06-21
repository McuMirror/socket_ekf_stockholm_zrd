#ifndef SRC_INCLUDE_APP_ONOFF_H_
#define SRC_INCLUDE_APP_ONOFF_H_

void cmdOnOff_toggle(void);
void cmdOnOff_on(void);
void cmdOnOff_off(void);
void remoteCmdOnOff(uint8_t ep, uint8_t cmd);

#endif /* SRC_INCLUDE_APP_ONOFF_H_ */
