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
//-----------------------------------------------------reset------------------------
#include "hashTable.h"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint32_t fnv1_hash(const char *str)
{
    uint32_t hash = FNV_OFFSET_BASIS;
    const char *p;
    for (p = str; *p; p++)
    {
        hash *= FNV_PRIME;
        hash ^= (uint32_t)(*p);
    }
    return hash % HASH_TABLE_SIZE;
}


void mqtt_binding_table_put(MQTTBinding **bindings, uint8_t bindings_count)
{
    uint8_t i;
    uint32_t j;

    for ( i = 0; i < bindings_count; i++)
    {
        // Check if the client_id is not empty
        if (bindings[i]->client_id[0] != '\0')
        {
            uint32_t index = fnv1_hash(bindings[i]->devCaps);
            uint16_t entry_addr = (uint16_t)(index * sizeof(MQTTBinding));

            // Write the binding to EEPROM
            uint8_t *binding_ptr = (uint8_t *)bindings[i];
            for (j = 0; j < sizeof(MQTTBinding); j++)
            {
                i2cEepromWrite(EEPROM_ADDRESS, entry_addr + j, binding_ptr[j]);
                waitMicrosecond(5000);
            }
        }
    }
}

MQTTBinding *mqtt_binding_table_get(MQTTBinding **bindings, uint8_t bindings_count, const char *devCaps)
{
    uint8_t i;
    uint32_t j;

    for (i = 0; i < bindings_count; i++)
    {
        if (strncmp(bindings[i]->devCaps, devCaps, sizeof(bindings[i]->devCaps)) == 0)
        {
            // Read the binding from EEPROM
            uint32_t index = fnv1_hash(bindings[i]->devCaps);
            uint16_t entry_addr = (uint16_t)(index * sizeof(MQTTBinding));
            uint8_t *binding_ptr = (uint8_t *)bindings[i];

            for (j = 0; j < sizeof(MQTTBinding); j++)
            {
                binding_ptr[j] = i2cEepromRead(EEPROM_ADDRESS, entry_addr + j);
            }

            // Return the found binding
            return bindings[i];
        }
    }

    // Return NULL if not found
    return NULL;
}

bool mqtt_binding_table_remove(MQTTBinding **bindings, uint8_t bindings_count, const char *devCaps)
{
    bool removed = false;
    uint8_t i;
    uint32_t j;

    for (i = 0; i < bindings_count; i++)
    {
        if (strncmp(bindings[i]->devCaps, devCaps, sizeof(bindings[i]->devCaps)) == 0)
        {
            // Write 0xFF to the corresponding EEPROM area
            uint32_t index = fnv1_hash(bindings[i]->devCaps);
            uint16_t entry_addr = (uint16_t)(index * sizeof(MQTTBinding));

            for (j = 0; j < sizeof(MQTTBinding); j++)
            {
                i2cEepromWrite(EEPROM_ADDRESS, entry_addr + j, 0xFF);
                waitMicrosecond(5000);
            }

            // Clear the binding in the array
            memset(bindings[i], 0, sizeof(MQTTBinding));
            removed = true;
        }
    }

    return removed;
}
