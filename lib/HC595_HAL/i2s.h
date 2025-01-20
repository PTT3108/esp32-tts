#pragma once

#include "stdint.h"


extern uint32_t i2s_port_data;

bool i2s_init(void);
void i2s_write(uint8_t pin, uint8_t val);
uint8_t i2s_state(uint8_t pin);
