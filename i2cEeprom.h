// i2c Eeprom Library
// Julian Schneider

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// 24LC512 External EEPROM Configuration:
// SCL -> PB2 -> 2.2k pull-ups for 100kHz
// SDA -> PB3 -> 2.2k pull-ups for 100kHz
// A0, A1, A2 -> GND
// Vdd -> Vcc
// WP -> GND
// Vss -> GND

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef I2CEEPROM_H_
#define I2CEEPROM_H_

#include <stdint.h>

#include "tm4c123gh6pm.h"
#include "wait.h"
#include "i2c0.h"
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint8_t i2cEepromRead(uint8_t add, uint16_t location);
void i2cEepromReset(uint8_t add, uint16_t location);
void i2cEepromWrite(uint8_t add, uint16_t location, uint8_t data);

#endif
