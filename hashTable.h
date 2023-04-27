// Hash Table Library
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

#ifndef HASH_TABLE_H_
#define HASH_TABLE_H_

#define EEPROM_ADDRESS   0x50            // EEPROM I2C address
#define PAGE_SIZE        128             // Page size in bytes
#define MAX_ADDRESS      0xFFFF          // Maximum address in EEPROM (65536)
#define MAX_NUM_PAGES    0x200           // Maximum page number is 512 or 0x200
#define MAX_BINDING_SIZE 64              // Max string length of topic names

#define HASH_TABLE_SIZE 256
#define FNV_OFFSET_BASIS 2166136261
#define FNV_PRIME 16777619

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "i2cEeprom.h"
#include "wait.h"

typedef struct {
    char client_id[16];
    char topic[32];
    uint8_t qos;
    uint16_t nextAddr;
} MQTTBinding;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint32_t fnv1_hash(const char *str);
void mqtt_binding_table_put(const char *client_id, const char *topic, uint8_t qos);
bool mqtt_binding_table_get(const char *client_id, MQTTBinding *binding);
void mqtt_binding_table_remove(const char *client_id);

#endif // HASH_TABLE_H_
