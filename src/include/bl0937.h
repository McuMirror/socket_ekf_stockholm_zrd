#ifndef BL0937_H_
#define BL0937_H_

#include "tl_common.h"

#define BL0937_MODE_CURRENT   0
#define BL0937_MODE_VOLTAGE   1

void bl0937_init(uint32_t cf_pin, uint32_t cf1_pin, uint32_t sel_pin, uint8_t currentWhen);
void bl0937_process(void);
void bl0937_setMode(uint8_t mode);
uint8_t bl0937_getMode(void);
uint8_t bl0937_toggleMode(void);

uint16_t bl0937_getCurrent(void);
uint16_t bl0937_getVoltage(void);
uint16_t bl0937_getActivePower(void);
uint32_t bl0937_getEnergy(void);
void bl0937_resetEnergy(void);

uint16_t bl0937_getApparentPower(void);
uint16_t bl0937_getReactivePower(void);
uint8_t bl0937_getPowerFactor(void);

void bl0937_setResistors(float current, float voltage_upstream, float voltage_downstream);
void bl0937_expectedCurrent(float current);
void bl0937_expectedVoltage(uint16_t voltage);
void bl0937_expectedActivePower(uint16_t power);

void bl0937_adjustVoltage(float adjust);
void bl0937_adjustCurrent(float adjust);
void bl0937_adjustPower(float adjust);

void bl0937_setMultipliers(float current_mul, float voltage_mul, float power_mul);
void bl0937_resetMultipliers(void);
float bl0937_getCurrentMultiplier(void);
float bl0937_getVoltageMultiplier(void);
float bl0937_getPowerMultiplier(void);

#endif
