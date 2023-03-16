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
#define CONTROL_PUBLISH   0x30
#define CONTROL_SUBSCRIBE 0x82

// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint8_t encodeLength(uint8_t X)
{
    uint8_t encodedByte = 0;

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
    int i = 0;
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    mqttHeader* mqtt_packet = (mqttHeader*)tcp->data;

    mqtt_packet->controlHeader = CONTROL_CONNECT;
    mqtt_packet->remainingLength = encodeLength(sizeof(mqttConnectHeader) + strlen(clientID) + 2);

    //1 for control header, 1 for uint8_t data[0]
    uint16_t dataSize = 2 + mqtt_packet->remainingLength;

    mqttConnectHeader* connect = (mqttConnectHeader*)mqtt_packet->data;

    connect->protocolLength = htons(0x04);
    //connect->protocolLength = 4;
//    connect->msb = 0;
//    connect->lsb = 4;

    connect->protocolName[0] = 'M';
    connect->protocolName[1] = 'Q';
    connect->protocolName[2] = 'T';
    connect->protocolName[3] = 'T';

    connect->protocolLevel = MQTT_PROTOCOL;
    connect->controlFlags = flags;
    connect->keepAliveTime = htons(KEEP_ALIVE_LONG);

    mqttConnectPayload* payload = (mqttConnectPayload*)connect->data;
    payload->msb_lsb = htons(strlen(clientID));
    //payload->msb_lsb = htons(6);
    //uint8_t* payload_data = (uint8_t*)payload->data;
    //payload_data = (uint8_t*)clientID;
    for(i = 0; i < strlen(clientID); i++)
    {
        payload->data[i] = clientID[i];
    }

    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) mqtt_packet, dataSize);
}

//void sendMqttSubscribe(etherHeader *ether, socket *s, uint16_t packetIdent, char* topicName, uint8_t qosType)
void sendMqttSubscribe(etherHeader *ether, socket *s, char* topicName)
{
    uint16_t i = 0, j = 0;
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    mqttHeader* mqtt_packet = (mqttHeader*)tcp->data;

    mqtt_packet->controlHeader = CONTROL_SUBSCRIBE;
//    mqtt_packet->remainingLength = encodeLength(sizeof(packetIdent) + sizeof(topicName) + (sizeof(topicName)/sizeof(topicName[0])) + sizeof(qosType));
//
//    //1 for control header, 1 for uint8_t data[0]
//    uint16_t dataSize = 2 + mqtt_packet->remainingLength;
//
//    mqttSubscribeHeader* subscribe = (mqttSubscribeHeader*)mqtt_packet->remainingLength;
//
//    subscribe->packetID = packetIdent;
//
//    uint8_t* subscribe_payload = subscribe->data;
//    subscribe_payload = (uint8_t*)atoi(topicName);

    //packet identifier msb/lsb
    mqtt_packet->data[i++] = 0x00;
    mqtt_packet->data[i++] = 0x08;

    //payload - topic filter msb/lsb
    mqtt_packet->data[i++] = strlen(topicName) & 0xF0;
    mqtt_packet->data[i++] = strlen(topicName) & 0x0F;

    //topic filter name
    for(j = 0; j < strlen(topicName); j++)
    {
        mqtt_packet->data[i++] =  topicName[j];
    }

    //last byte is reserved except for last 2 bits which are qos level
    mqtt_packet->data[i] = 0x00;

    //i now holds remaining length amount
    mqtt_packet->remainingLength = encodeLength(i);

    uint16_t dataSize = 2 + mqtt_packet->remainingLength;

    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) mqtt_packet, dataSize);
}

void sendMqttPublish(etherHeader *ether, socket *s, char* topicName, char* topicData)
{
    int i = 0, j = 0;
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    mqttHeader* mqtt_packet = (mqttHeader*)tcp->data;

    mqtt_packet->controlHeader = CONTROL_PUBLISH;
    //mqtt_packet->remainingLength = encodeLength(0);

//    mqttPublishHeader* publish = (mqttPublishHeader*)mqtt_packet->data;
//    publish->header_msb_lsb = htons(strlen(topicName);
//    for(i = 0; i < strlen(topicName); i++)
//    {
//        publish->topic[i] = topicName[i];
//    }

    //msb/lsb for topic name
    mqtt_packet->data[i++] = strlen(topicName) & 0xF0;
    mqtt_packet->data[i++] = strlen(topicName) & 0x0F;

    //topic name itself
    for(j = 0; j < strlen(topicName); j++)
    {
        mqtt_packet->data[i++] = topicName[j];
    }

    //packet identifier, 2 bytes, always 0x00 for qos 0
//    mqtt_packet->data[i++] = 0;
//    mqtt_packet->data[i++] = 0;

    //payload - msb/lsb for topic data
    mqtt_packet->data[i++] = strlen(topicData) & 0xF0;
    mqtt_packet->data[i++] = strlen(topicData) & 0x0F;

    //topic data itself
    for(j = 0; j < strlen(topicData); j++)
    {
        mqtt_packet->data[i++] = topicData[j];
    }

    i--;
    //i now holds remaining length amount
    mqtt_packet->remainingLength = encodeLength(i);

    uint16_t dataSize = 2 + mqtt_packet->remainingLength;
    ///uint16_t dataSize = 2 + i;

    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) mqtt_packet, dataSize);
}
