// MQTT Library (framework only)
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: -
// Target uC:       -
// System Clock:    -

// Hardware configuration:
// -

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>
#include "tcp.h"


#define MQTT_PORT   1883

#define CONNECT     0x10
#define CONNACK     0x20
#define PUBLISH     0x30
#define PUBACK      0x40
#define PUBREC      0x50
#define PUBREL      0x60
#define PUBCOMP     0x70
#define SUBSCRIBE   0x80
#define SUBACK      0x90
#define UNSUBSCRIBE 0xA0
#define UNSUBACK    0xB0
#define PINGREQ     0xC0
#define PINGRESP    0xD0
#define DISCONNECT  0xE0

#define MQTT_CONNECT_HEADER_LENGTH  12
#define MQTT_USERNAME               128
#define MQTT_PASSWORD               64
#define MQTT_WILL_RETAIN            32
#define MQTT_WILL                   4
#define MQTT_CLEAN                  2
#define MQTT_MAX_ARGUMENTS          5
#define MQTT_MAX_ARGUMENT_LENGTH    80


typedef struct _topic
{
    char name[MQTT_MAX_ARGUMENT_LENGTH];
} topic;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void mqttConnect(etherHeader *ether, socket s, uint8_t flags, char *data[], uint8_t nargs);
void mqttSubscribe(etherHeader *ether, socket s, uint8_t QoS, char *data[], uint8_t nargs);
void mqttPublish(etherHeader *ether, socket s, uint8_t QoS, char *data[], uint8_t nargs);
void mqttDisconnect(etherHeader *ether, socket s, uint8_t QoS, char *data[], uint8_t nargs);
void sendMqttMessage(etherHeader *ether, socket s, uint8_t controlHeader, uint8_t messageFlags, void *data, uint8_t MAX_ARGUMENT_LENGTH, uint8_t nargs);
uint16_t getArgumentLength(char *str);

bool isMqtt(etherHeader *ether);

uint8_t getMqttFlags(uint8_t *data);

uint8_t *getMqttData(uint8_t *data);

uint8_t addTopic(char *name, topic *topics, uint8_t topicCount);
void removeTopic(uint8_t topicIndex, topic *topics);
uint8_t getTopicIndex(char *name, topic *topics, uint8_t topicCount);
void *getMqttMessage(uint8_t *data);
uint16_t getMqttMessageLength(uint8_t *data);

#endif

