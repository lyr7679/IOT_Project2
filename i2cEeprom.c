// I2C EEPROM Library
// Julian Schneider & Dario Ugalde

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------
#include "i2cEeprom.h"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint8_t i2cEepromRead(uint8_t add, uint16_t location)
{
    // set internal register counter in device
    I2C0_MSA_R = add << 1; // add:r/~w=0
    I2C0_MDR_R = (location >> 8) & 0xFF;                     // High Byte
    I2C0_MICR_R = I2C_MICR_IC;
    I2C0_MCS_R = I2C_MCS_START | I2C_MCS_RUN;
    while ((I2C0_MRIS_R & I2C_MRIS_RIS) == 0);

    I2C0_MDR_R = (location & 0xFF);                           // Low Byte
    I2C0_MICR_R = I2C_MICR_IC;
    I2C0_MCS_R = I2C_MCS_RUN;
    while ((I2C0_MRIS_R & I2C_MRIS_RIS) == 0);

    // read data from register
    I2C0_MSA_R = (add << 1) | 1; // add:r/~w=1
    I2C0_MICR_R = I2C_MICR_IC;
    I2C0_MCS_R = I2C_MCS_START | I2C_MCS_RUN | I2C_MCS_STOP;    // Repeated start is needed
    while ((I2C0_MRIS_R & I2C_MRIS_RIS) == 0);
    return I2C0_MDR_R;
}

void i2cEepromReset(uint8_t add, uint16_t location)
{
    // send address and register high byte
        I2C0_MSA_R = add << 1; // add:r/~w=0
        I2C0_MDR_R = (location >> 8) & 0xFF;                    // High Byte
        I2C0_MICR_R = I2C_MICR_IC;
        I2C0_MCS_R = I2C_MCS_START | I2C_MCS_RUN;
        while ((I2C0_MRIS_R & I2C_MRIS_RIS) == 0);


        // send register low byte
        I2C0_MDR_R = (location & 0xFF);
        I2C0_MICR_R = I2C_MICR_IC;
        I2C0_MCS_R = I2C_MCS_RUN;
        while ((I2C0_MRIS_R & I2C_MRIS_RIS) == 0);

        // write data to register
        I2C0_MDR_R = 0xFF;
        I2C0_MICR_R = I2C_MICR_IC;
        I2C0_MCS_R = I2C_MCS_RUN | I2C_MCS_STOP;
        while (!(I2C0_MRIS_R & I2C_MRIS_RIS));
}

void i2cEepromWrite(uint8_t add, uint16_t location, uint8_t data)
{
    // send address and register high byte
    I2C0_MSA_R = add << 1; // add:r/~w=0
    I2C0_MDR_R = (location >> 8) & 0xFF;                    // High Byte
    I2C0_MICR_R = I2C_MICR_IC;
    I2C0_MCS_R = I2C_MCS_START | I2C_MCS_RUN;
    while ((I2C0_MRIS_R & I2C_MRIS_RIS) == 0);

    // send register low byte
    I2C0_MDR_R = (location & 0xFF);
    I2C0_MICR_R = I2C_MICR_IC;
    I2C0_MCS_R = I2C_MCS_RUN;
    while ((I2C0_MRIS_R & I2C_MRIS_RIS) == 0);

    // write data to register
    I2C0_MDR_R = data;
    I2C0_MICR_R = I2C_MICR_IC;
    I2C0_MCS_R = I2C_MCS_RUN | I2C_MCS_STOP;
    while (!(I2C0_MRIS_R & I2C_MRIS_RIS));
}

