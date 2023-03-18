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
#include "uart0.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------

#define CONTROL_CONNECT     0x10
#define CONTROL_PUBLISH     0x30
#define CONTROL_SUBSCRIBE   0x82
#define CONTROL_UNSUB       0xA2
#define CONTROL_DISCONNECT  0xE0

#define MAX_TOPICS         20

char topic_name_arr[MAX_TOPICS][80] = {};
int topic_name_id[MAX_TOPICS];

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

bool isPub(etherHeader* ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    mqttHeader* mqtt_packet = (mqttHeader*)tcp->data;

    if(mqtt_packet->controlHeader == CONTROL_PUBLISH || mqtt_packet->controlHeader == 0x31 || mqtt_packet->controlHeader == 0x32)
        return true;
    return false;
}

void processMqtt(etherHeader* ether, socket* s)
{

    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    //mqttHeader* mqtt_packet = (mqttHeader*)tcp->data;

    //if its a publish we receive that means we are subscribed to it
    //we need to print out the topic name and data onto putty
    if(isPub(ether))
    {
        int i = 1, j = 0, count = 0;
        int nameLength = 0;
        int remainingLength = 0;
        char str[80] = {};

        //get out of remaining length bytes
        while((tcp->data[i] & 0x80))
        {
            i++;
            count++;
        }
        //i++;
        i -= count;

        for(j = count; j >= 0; j--)
        {
            remainingLength |= tcp->data[i] << (j * 8);
            if(count != 0)
            {
                i++;
            }
        }
        i++;

        //msb/lsb for topic name
        nameLength = tcp->data[i++] << 8;
        nameLength |= tcp->data[i++];

        //save topic name to string to print to putty
        for(j = 0; j < nameLength; j++)
        {
            str[j] = (char) tcp->data[i++];
        }
        putsUart0("Topic Name: ");
        putsUart0(str);
        putcUart0('\n');

        //print out remaining message length which is data
        memset(str, '\0', sizeof(str));

        for(j = 0; j < (remainingLength - nameLength - 2); j++)
        {
            str[j] = (char) tcp->data[i++];
        }
        putsUart0("Data: ");
        putsUart0(str);
        putsUart0("\n\n");
    }
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
    int packetID = random32();

    mqtt_packet->controlHeader = CONTROL_SUBSCRIBE;

    //packet identifier msb/lsb
    mqtt_packet->data[i++] = packetID & 0x000000F0;
    mqtt_packet->data[i++] = packetID & 0x0000000F;

    //payload - topic filter msb/lsb
    mqtt_packet->data[i++] = strlen(topicName) & 0xF0;
    mqtt_packet->data[i++] = strlen(topicName) & 0x0F;

    //save topic name into topic name array
    for(j = 0; j < MAX_TOPICS; j++)
    {
        if(topic_name_arr[j][0] == '\0')
        {
            strncpy(topic_name_arr[j], topicName, strlen(topicName));
            topic_name_id[j] = packetID;
            break;
        }
    }

    //topic filter name
    for(j = 0; j < strlen(topicName); j++)
    {
        mqtt_packet->data[i++] =  topicName[j];
    }

    //last byte is reserved except for last 2 bits which are qos level
    mqtt_packet->data[i++] = 0x00;

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
//    mqtt_packet->data[i++] = strlen(topicData) & 0xF0;
//    mqtt_packet->data[i++] = strlen(topicData) & 0x0F;

    //topic data itself
    for(j = 0; j < strlen(topicData); j++)
    {
        mqtt_packet->data[i++] = topicData[j];
    }

    //i--;
    //i now holds remaining length amount
    mqtt_packet->remainingLength = encodeLength(i);

    uint16_t dataSize = 2 + mqtt_packet->remainingLength;
    ///uint16_t dataSize = 2 + i;

    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) mqtt_packet, dataSize);
}

void sendMqttUnsub(etherHeader *ether, socket *s, char* topicName)
{
    int i = 0, j = 0;
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    mqttHeader* mqtt_packet = (mqttHeader*)tcp->data;
    int packetID = 0;
    char str[50];
    char str1[50];

    mqtt_packet->controlHeader = CONTROL_UNSUB;

    for(j = 0; j < MAX_TOPICS; j++)
    {
        strncpy(str, topic_name_arr[j], sizeof(topic_name_arr[j]));
        strncpy(str1, topicName, sizeof(topicName));
        if(!strcmp(topicName, topic_name_arr[j]))
        {
            packetID = topic_name_id[j];
            memset(topic_name_arr[j], '\0', 80);
        }
    }

    //packet identifier msb/lsb
    mqtt_packet->data[i++] = packetID & 0x000000F0;
     mqtt_packet->data[i++] = packetID & 0x0000000F;
    //msb/lsb for topic name
    mqtt_packet->data[i++] = strlen(topicName) & 0xF0;
    mqtt_packet->data[i++] = strlen(topicName) & 0x0F;

    //topic name itself
    for(j = 0; j < strlen(topicName); j++)
    {
        mqtt_packet->data[i++] = topicName[j];
    }

    //i now holds remaining length amount
    mqtt_packet->remainingLength = encodeLength(i);

    uint16_t dataSize = 2 + mqtt_packet->remainingLength;
    ///uint16_t dataSize = 2 + i;

    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) mqtt_packet, dataSize);
}

void sendMqttDisconnect(etherHeader *ether, socket *s)
{
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    mqttHeader* mqtt_packet = (mqttHeader*)tcp->data;

    mqtt_packet->controlHeader = CONTROL_DISCONNECT;
    mqtt_packet->remainingLength = 0;

    uint16_t dataSize = 2 + mqtt_packet->remainingLength;

    sendTcpMessage(ether, s, PSH | ACK, (uint8_t*) mqtt_packet, dataSize);
}



