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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mqtt.h"
#include "timer.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------

#define CONTROL_CONNECT   0x10
#define CONTROL_SUBSCRIBE 0x82

// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint32_t encodeLength(uint32_t X)
{
    uint32_t encodedByte = 0;

    do
    {
        encodedByte = X % 128;
        X /= 128;

        if(X > 0)
            encodedByte |= 128;
        return encodedByte;
    }while(X > 0);

    //return encodedByte;
}

void processMqtt(etherHeader* ether, socket* s)
{

}


void sendMqttConnect(etherHeader *ether, socket *s, uint8_t flags, char* clientID)
{
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    mqttHeader* mqtt_packet = (mqttHeader*)tcp->data;

    mqtt_packet->controlHeader = CONTROL_CONNECT;
    mqtt_packet->remainingLength = encodeLength(sizeof(mqttConnectHeader) + sizeof(clientID));

    //1 for control header, 1 for uint8_t data[0]
    uint16_t dataSize = 2 + mqtt_packet->remainingLength;

    mqttConnectHeader* connect = (mqttConnectHeader*)mqtt_packet->remainingLength;

    connect->protocolLength = htons(0x04);

    connect->protocolName[0] = 'M';
    connect->protocolName[1] = 'Q';
    connect->protocolName[2] = 'T';
    connect->protocolName[3] = 'T';

    connect->protocolLevel = MQTT_PROTOCOL;
    connect->controlFlags = flags;
    connect->keepAliveTime = htons(KEEP_ALIVE_LONG);

    uint8_t* connect_payload = connect->data;
    connect_payload = (uint8_t*)atoi(clientID);

    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) mqtt_packet, dataSize);
}

void sendMqttSubscribe(etherHeader *ether, socket *s, uint16_t packetIdent, char* topicName, uint8_t qosType)
{
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    mqttHeader* mqtt_packet = (mqttHeader*)tcp->data;

    mqtt_packet->controlHeader = CONTROL_SUBSCRIBE;
    mqtt_packet->remainingLength = encodeLength(sizeof(packetIdent) + sizeof(topicName) + (sizeof(topicName)/sizeof(topicName[0])) + sizeof(qosType));

    //1 for control header, 1 for uint8_t data[0]
    uint16_t dataSize = 2 + mqtt_packet->remainingLength;

    mqttSubscribeHeader* subscribe = (mqttSubscribeHeader*)mqtt_packet->remainingLength;

    subscribe->packetID = packetIdent;

    uint8_t* subscribe_payload = subscribe->data;
    subscribe_payload = (uint8_t*)atoi(topicName);
}
