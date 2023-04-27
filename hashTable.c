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

void mqtt_binding_table_put(const char *client_id, const char *topic, uint8_t qos)
{
    uint16_t index = fnv1_hash(client_id);
    uint16_t offsetIndex = index % MAX_BINDING_SIZE;
    uint16_t previousOffset = 0;
    MQTTBinding binding;

    // Calculate pointer offset
    if(offsetIndex < MAX_BINDING_SIZE / 2)
    {
        index -= offsetIndex;
    }
    else
    {
        index += MAX_BINDING_SIZE - offsetIndex;
    }

    // Prepare binding struct data
    strncpy(binding.client_id, client_id, sizeof(binding.client_id) - 1);
    binding.client_id[sizeof(binding.client_id) - 1] = '\0';
    strncpy(binding.topic, topic, sizeof(binding.topic) - 1);
    binding.topic[sizeof(binding.topic) - 1] = '\0';
    binding.qos = qos;
    binding.nextAddr = 0;

    uint16_t entry_addr = index;
    uint8_t *entry_ptr = (uint8_t *)&binding;

    // Perform write at next available 64-byte offset
    bool writeComplete = false;
    while (!writeComplete)
    {
        if (i2cEepromRead(EEPROM_ADDRESS, entry_addr) == 0xFF)
        {
            uint8_t i;
            for (i = 0; i < sizeof(MQTTBinding); i++)
            {
                i2cEepromWrite(EEPROM_ADDRESS, entry_addr + i, entry_ptr[i]);
                // It takes 5ms for a write cycle to be complete
                waitMicrosecond(5000);
            }
            writeComplete = true;
        }
        else if (i2cEepromRead(EEPROM_ADDRESS, entry_addr) != 0xFF) // Check if block is occupied by same device
        {
            MQTTBinding readBuffer;
            uint8_t *read_ptr = (uint8_t *)&readBuffer;
            uint8_t i;
            for (i = 0; i < sizeof(MQTTBinding); i++)
            {
                read_ptr[i] = i2cEepromRead(EEPROM_ADDRESS, entry_addr + i);
            }

            if (strncmp(readBuffer.client_id, client_id, sizeof(readBuffer.client_id)) == 0)
            {
                previousOffset = entry_addr;
            }

            if (readBuffer.nextAddr == 0)
            {
                entry_addr += MAX_BINDING_SIZE;
            }
            else
            {
                entry_addr = readBuffer.nextAddr;
            }
        }
        else
        {
            entry_addr += MAX_BINDING_SIZE;
        }
    }

    // Update previous offset with location of additional binding
    if (previousOffset != 0)
    {
        MQTTBinding readBuffer;
        uint8_t *read_ptr = (uint8_t *)&readBuffer;
        uint8_t i;
        for (i = 0; i < sizeof(MQTTBinding); i++)
        {
            read_ptr[i] = i2cEepromRead(EEPROM_ADDRESS, previousOffset + i);
        }

        readBuffer.nextAddr = entry_addr;

        for (i = 0; i < sizeof(MQTTBinding); i++)
        {
            i2cEepromWrite(EEPROM_ADDRESS, previousOffset + i, read_ptr[i]);
            // It takes 5ms for a write cycle to be complete
            waitMicrosecond(5000);
        }
    }
}

bool mqtt_binding_table_get(const char *client_id, MQTTBinding *binding)
{
    uint16_t index = fnv1_hash(client_id);
    uint16_t offsetIndex = index % MAX_BINDING_SIZE;

    // Calculate pointer offset
    if(offsetIndex < MAX_BINDING_SIZE / 2)
    {
        index -= offsetIndex;
    }
    else
    {
        index += MAX_BINDING_SIZE - offsetIndex;
    }

    uint16_t entry_addr = index;
    uint8_t *entry_ptr = (uint8_t *)binding;

    // Read binding from memory
    bool entrySeen = false;
    do
    {
        bool validClientId = false;
        // Read binding from current index
        uint8_t i;
        for (i = 0; i < sizeof(MQTTBinding); i++)
        {
            entry_ptr[i] = i2cEepromRead(EEPROM_ADDRESS, entry_addr + i);
        }

        // Smoking gun to view at least one entry in external eeprom
        if ( entry_ptr[0] != 0xFF )
        {
            entrySeen = true;
        }

        // Verify bindings match read data
        if (strncmp(binding->client_id, client_id, sizeof(binding->client_id)) == 0)
        {
            validClientId = true;
        }
        if ( validClientId)
        {
            return true;
        }

        // Prepare for next read
        entry_addr += MAX_BINDING_SIZE;
    } while( !entrySeen );

    return false;
}

void mqtt_binding_table_remove(const char *client_id)
{
    MQTTBinding binding;

    if (mqtt_binding_table_get(client_id, &binding))
    {
        uint16_t index = fnv1_hash(client_id);
        uint16_t offsetIndex = index % MAX_BINDING_SIZE;

        // Calculate pointer offset
        if(offsetIndex < MAX_BINDING_SIZE / 2)
        {
            index -= offsetIndex;
        }
        else
        {
            index += MAX_BINDING_SIZE - offsetIndex;
        }

        uint16_t entry_addr = index;

        // Read entry to check for additional device entries
        MQTTBinding readBuffer;
        do
        {
            uint8_t *read_ptr = (uint8_t *)&readBuffer;
            uint8_t i;
            for (i = 0; i < sizeof(MQTTBinding); i++)
            {
                read_ptr[i] = i2cEepromRead(EEPROM_ADDRESS, entry_addr + i);
            }

            index = readBuffer.nextAddr;

            // Write 0xFF to the removed entry
            for (i = 0; i < sizeof(MQTTBinding); i++)
            {
                i2cEepromWrite(EEPROM_ADDRESS, entry_addr + i, 0xFF);
                // It takes 5ms for a write cycle to be complete
                waitMicrosecond(5000);
            }

            entry_addr = index;
        } while( entry_addr != 0 );
    }
}
