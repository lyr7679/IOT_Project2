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

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

#define QOS0    0
#define QOS1    2
#define QOS2    4

#define MQTT_PROTOCOL 0x04
#define KEEP_ALIVE_SHORT 60
#define KEEP_ALIVE_LONG 6000

#define MQTT_PACKET_IDENT 0x1234

//MQTT States
#define MQTT_CONNECT_SENT 11
#define MQTT_CONNECTED    12
#define MQTT_PUBLISH      13
#define MQTT_SUBSCRIBE    14
#define MQTT_DISCONNECT   15

//MQTT Flags
#define WILL 0x04
#define CLEAN_SESH 0x02
#define WILL_QOS 0x18
#define WILL_RET 0x20
#define PSWD 0x40
#define USRNAME 0x80

uint8_t mqttConnFlag;

typedef struct _mqttHeader
{
  uint8_t controlHeader;
  uint32_t remainingLength;
  uint8_t  data[0];         //mqtt variable header (all types)
} mqttHeader;

typedef struct _mqttConnectHeader
{
    uint16_t protocolLength;
    char protocolName[4];
    uint8_t protocolLevel;
    uint8_t controlFlags;
    uint16_t keepAliveTime;
    uint8_t data[0];        //payload

} mqttConnectHeader;

typedef struct _mqttConnackHeader
{
    uint8_t connackFlags;
    uint8_t returnCode;
} mqttConnackHeader;

typedef struct _mqttSubscribeHeader
{
    uint16_t packetID;
    uint8_t data[0];    //payload for topic name + qos
} mqttSubscribeHeader;

uint32_t encodeLength(uint32_t X);
void processMqtt(etherHeader* ether, socket* s);

void sendMqttConnect(etherHeader *ether, socket *s, uint8_t flags, char* clientID);
void sendMqttSubscribe(etherHeader *ether, socket *s, uint16_t packetIdent, char* topicName, uint8_t qosType);


#endif
