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
#include "mqtt.h"
#include "timer.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------

// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void mqttConnect(etherHeader *ether, socket s, uint8_t flags, char *data[], uint8_t nargs)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tcpLength;

    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
     for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }
    uint8_t ipHeaderLength = ip->size * 4;

    // TCP header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(s.localPort);
    tcp->destPort = htons(s.remotePort);
    tcp->sequenceNumber = htonl(s.acknowledgementNumber);
    tcp->acknowledgementNumber = htonl(s.sequenceNumber);   
    tcp->offsetFields = htons(PSH | ACK | 0x5000);
    tcp->windowSize = htons(1500);
    tcp->urgentPointer = 0;

    // MQTT Packet
    uint16_t mqttLength = 0;
    uint8_t *mqtt = tcp->data;

    mqtt[mqttLength++] = CONNECT;
    uint8_t *mqttRemainingLength = &(mqtt[mqttLength++]);
    mqttRemainingLength[0] = 0x00;
    // mqtt[mqttLength++] = 0x00;
    
    mqtt[mqttLength++] = 0x0;
    mqtt[mqttLength++] = 0x04;
    mqtt[mqttLength++] = 'M';
    mqtt[mqttLength++] = 'Q';
    mqtt[mqttLength++] = 'T';
    mqtt[mqttLength++] = 'T';
    mqtt[mqttLength++] = 0x04; // MQTT v3.1.1
    mqtt[mqttLength++] = flags; // Connect Flags
    mqtt[mqttLength++] = 0xFF; // Keep alive seconds
    mqtt[mqttLength++] = 0xFF;

    uint16_t argumentLength = 0;
    uint8_t argumentIndex = 0;
    char *arg;
    while(argumentIndex < nargs)
    {
        arg = &(data[argumentIndex]);
        argumentLength = getArgumentLength(arg);
        mqtt[mqttLength++] = (argumentLength >> 8) & 0xFF;
        mqtt[mqttLength++] = argumentLength & 0xFF;
        for(i = 0; i < argumentLength; i++)
            mqtt[mqttLength++] = arg[i];
        argumentIndex++;
    }
    

    if(mqttLength > 0x7F)
        mqttRemainingLength[0] |= 0x80;
    mqttRemainingLength[0] |= (mqttLength & 0x7F);
    // mqttRemainingLength[1] = (mqttLength >> 7) & 0x7F;

    s.acknowledgementNumber += mqttLength;
    tcpLength = sizeof(tcpHeader) + mqttLength;
    ip->length = htons(ipHeaderLength + tcpLength);

    // 32-bit sum over ip header
    calcIpChecksum(ip);
    // 32-bit sum over TCP header
    calcTcpChecksum(ip, tcpLength);
    // send packet with size = ether + ip header + TCP header + MQTT packet
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

uint16_t getArgumentLength(char *str)
{
    uint16_t size = 0;
    while(str[size] != '\0' && str[size] != '-')
        size++;
    return size;
}

void mqttSubscribe(etherHeader *ether, socket s, uint8_t QoS, char *data[], uint8_t nargs)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];
    uint16_t topicLength;

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
     for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }
    uint8_t ipHeaderLength = ip->size * 4;

    // TCP header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(s.localPort);
    tcp->destPort = htons(s.remotePort);
    tcp->sequenceNumber = htonl(s.acknowledgementNumber);
    tcp->acknowledgementNumber = htonl(s.sequenceNumber);   
    tcp->offsetFields = htons(PSH | ACK | 0x5000);
    tcp->windowSize = htons(1500);
    tcp->urgentPointer = 0;
    

    uint16_t mqttLength = 0;
    uint8_t *mqtt = tcp->data;

    mqtt[mqttLength++] = (SUBSCRIBE << 4);
    uint8_t *mqttRemainingLength = &(mqtt[mqttLength++]);
    mqttRemainingLength[0] = 0x00;
    mqtt[mqttLength++] = 0x00;


    mqtt[mqttLength++] = ((s.localPort & 0xF) >> 8) & 0xFF;
    mqtt[mqttLength++] = (s.localPort & 0xF) & 0xFF;

    uint16_t argumentLength = 0;
    uint8_t argumentIndex = 0;
    while(argumentIndex < nargs)
    {
        argumentLength = getArgumentLength(&(data[argumentIndex]));
        mqtt[mqttLength++] = (argumentLength >> 8) & 0xFF;
        mqtt[mqttLength++] = argumentLength & 0xFF;
        for(i = 0; i < argumentLength; i++)
            mqtt[mqttLength++] = data[argumentIndex][i];
        argumentIndex++;
    }
    mqtt[mqttLength++] = QoS;


    if(mqttLength > 0x7F)
        mqttRemainingLength[0] |= 0x80;
    mqttRemainingLength[0] |= (mqttLength & 0x7F);
    mqttRemainingLength[1] = (mqttLength >> 7) & 0x7F;
    s.acknowledgementNumber += mqttLength;
    tcpLength = sizeof(tcpHeader) + mqttLength;
    ip->length = htons(ipHeaderLength + tcpLength);

    // 32-bit sum over ip header
    calcIpChecksum(ip);
    // 32-bit sum over TCP header
    calcTcpChecksum(ip, tcpLength);
    // send packet with size = ether + ip header + TCP header + MQTT packet
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

void mqttPublish(etherHeader *ether, socket s, uint8_t QoS, char *data[], uint8_t nargs)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];
    uint16_t topicLength;

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
     for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }
    uint8_t ipHeaderLength = ip->size * 4;

    // TCP header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(s.localPort);
    tcp->destPort = htons(s.remotePort);
    tcp->sequenceNumber = htonl(s.acknowledgementNumber);
    tcp->acknowledgementNumber = htonl(s.sequenceNumber);   
    tcp->offsetFields = htons(PSH | ACK | 0x5000);
    tcp->windowSize = htons(1500);
    tcp->urgentPointer = 0;
    



    uint16_t mqttLength = 0;
    uint8_t *mqtt = tcp->data;

    mqtt[mqttLength++] = (PUBLISH << 4);
    uint8_t *mqttRemainingLength = &(mqtt[mqttLength++]);
    mqttRemainingLength[0] = 0x00;
    mqtt[mqttLength++] = 0x00;


    uint16_t argumentLength = 0;
    uint8_t argumentIndex = 0;
    while(argumentIndex < nargs)
    {
        argumentLength = getArgumentLength(&(data[argumentIndex]));
        mqtt[mqttLength++] = (argumentLength >> 8) & 0xFF;
        mqtt[mqttLength++] = argumentLength & 0xFF;
        for(i = 0; i < argumentLength; i++)
            mqtt[mqttLength++] = data[argumentIndex][i];
        argumentIndex++;
    }


    if(mqttLength > 0x7F)
        mqttRemainingLength[0] |= 0x80;
    mqttRemainingLength[0] |= (mqttLength & 0x7F);
    mqttRemainingLength[1] = (mqttLength >> 7) & 0x7F;
    s.acknowledgementNumber += mqttLength;
    tcpLength = sizeof(tcpHeader) + mqttLength;
    ip->length = htons(ipHeaderLength + tcpLength);

    // 32-bit sum over ip header
    calcIpChecksum(ip);
    // 32-bit sum over TCP header
    calcTcpChecksum(ip, tcpLength);
    // send packet with size = ether + ip header + TCP header + MQTT packet
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

void mqttDisconnect(etherHeader *ether, socket s, uint8_t QoS, char *data[], uint8_t nargs)
{
        uint8_t i;
    uint32_t sum;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];
    uint16_t topicLength;

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
     for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }
    uint8_t ipHeaderLength = ip->size * 4;

    // TCP header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(s.localPort);
    tcp->destPort = htons(s.remotePort);
    tcp->sequenceNumber = htonl(s.acknowledgementNumber);
    tcp->acknowledgementNumber = htonl(s.sequenceNumber);   
    tcp->offsetFields = htons(PSH | ACK | 0x5000);
    tcp->windowSize = htons(1500);
    tcp->urgentPointer = 0;
    



    uint16_t mqttLength = 0;
    uint8_t *mqtt = tcp->data;

    mqtt[mqttLength++] = (DISCONNECT << 4);
    uint8_t *mqttRemainingLength = &(mqtt[mqttLength++]);
    mqttRemainingLength[0] = 0x00;
    mqtt[mqttLength++] = 0x00;


    if(mqttLength > 0x7F)
        mqttRemainingLength[0] |= 0x80;
    mqttRemainingLength[0] |= (mqttLength & 0x7F);
    mqttRemainingLength[1] = (mqttLength >> 7) & 0x7F;
    s.acknowledgementNumber += mqttLength;
    tcpLength = sizeof(tcpHeader) + mqttLength;
    ip->length = htons(ipHeaderLength + tcpLength);

    // 32-bit sum over ip header
    calcIpChecksum(ip);
    // 32-bit sum over TCP header
    calcTcpChecksum(ip, tcpLength);
    // send packet with size = ether + ip header + TCP header + MQTT packet
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);    
}

void sendMqttMessage(etherHeader *ether, socket s, uint8_t controlHeader, uint8_t messageFlags, void *data, uint8_t MAX_ARGUMENT_LENGTH, uint8_t nargs)
{
    static uint16_t id = 1;
    uint8_t i;
    uint32_t sum;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
     for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }
    uint8_t ipHeaderLength = ip->size * 4;

    // TCP header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(s.localPort);
    tcp->destPort = htons(s.remotePort);
    tcp->sequenceNumber = htonl(s.acknowledgementNumber);
    tcp->acknowledgementNumber = htonl(s.sequenceNumber);   
    tcp->offsetFields = htons(PSH | ACK | 0x5000);
    tcp->windowSize = htons(1500);
    tcp->urgentPointer = 0;


    uint16_t argumentLength = 0;
    uint8_t argumentIndex = 0;
    char *arg;

    // MQTT Packet
    uint16_t mqttLength = 0;
    uint8_t *mqtt = tcp->data;

    mqtt[mqttLength++] = controlHeader;
    uint8_t *mqttRemainingLength = &(mqtt[mqttLength++]);
    mqttRemainingLength[0] = 0x00;
    mqtt[mqttLength++] = 0x00;
    
    switch(controlHeader & 0xF0)
    {
        case CONNECT:
        {
            mqtt[mqttLength++] = 0x0;
            mqtt[mqttLength++] = 0x04;
            mqtt[mqttLength++] = 'M';
            mqtt[mqttLength++] = 'Q';
            mqtt[mqttLength++] = 'T';
            mqtt[mqttLength++] = 'T';
            mqtt[mqttLength++] = 0x04; // MQTT v3.1.1
            mqtt[mqttLength++] = messageFlags; // Connect Flags
            mqtt[mqttLength++] = 0xFF; // Keep alive seconds
            mqtt[mqttLength++] = 0xFF;
            break;
        }
        case UNSUBSCRIBE:
        case SUBSCRIBE:
            mqtt[0] |= 0x02;
        case PINGRESP:
        case PINGREQ:
        {
            mqtt[mqttLength++] = id >> 8 & 0xFF;
            mqtt[mqttLength++] = id & 0xFF;
            id++;
            break;
        }
        default:
            break;
    }

    while(argumentIndex < nargs)
    {
        arg = (char *)(data + (argumentIndex * MAX_ARGUMENT_LENGTH));
        argumentLength = getArgumentLength(arg);
        mqtt[mqttLength++] = (argumentLength >> 8) & 0xFF;
        mqtt[mqttLength++] = argumentLength & 0xFF;
        for(i = 0; i < argumentLength; i++)
            mqtt[mqttLength++] = arg[i];
        argumentIndex++;
        if((controlHeader & 0xF0) == SUBSCRIBE)
            mqtt[mqttLength++] = messageFlags;
        if((controlHeader & 0xF0) == PUBLISH)
        {
            arg = (char *)(data + (argumentIndex * MAX_ARGUMENT_LENGTH));
            argumentLength = getArgumentLength(arg);
            for(i = 0; i < argumentLength; i++)
                mqtt[mqttLength++] = arg[i];
            argumentIndex++;
        }
            
    }
    

    mqttRemainingLength[0] |= 0x80;
    mqttRemainingLength[0] |= ((mqttLength - 3) & 0x7F);
    mqttRemainingLength[1] = ((mqttLength - 3) >> 7) & 0x7F;

    s.acknowledgementNumber += mqttLength;
    tcpLength = sizeof(tcpHeader) + mqttLength;
    ip->length = htons(ipHeaderLength + tcpLength);

    // 32-bit sum over ip header
    calcIpChecksum(ip);
    // 32-bit sum over TCP header
    calcTcpChecksum(ip, tcpLength);
    // send packet with size = ether + ip header + TCP header + MQTT packet
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}


bool isMqtt(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    return ntohs(tcp->sourcePort) == 1883;
}

uint8_t getMqttFlags(uint8_t *data)
{
    return data[0] & 0xF0;
}

uint8_t *getMqttData(uint8_t *data)
{
    uint8_t mqttFlag = data[0] & 0xF0;
    uint32_t msgLength = 0;
    data++;
    msgLength += *data & 0x7F;
    if(*data & 0x80)
    {
        msgLength += (*data & 0x7F) << 7;
        data++;
        if(*data & 0x80)
        {
            data++;
            msgLength += (*data & 0x7F) << 14;
            if(*data & 0x80)
            {
                data++;
                msgLength += *data << 21;
            }   
        }  
    } 



    switch(mqttFlag)
    {
        case CONNACK:
            return &(data[msgLength - 1]);
        case PUBLISH:
            return ++data;
        default:
            return NULL;
    }
}

uint8_t addTopic(char *name, topic *topics, uint8_t topicCount)
{
    uint8_t i;
    for(i = 1; i < topicCount; i++)
    {
        if(topics[i].name[0] == '\0')
        {
            strcpy(topics[i].name, name);
            return i;
        }
    }
    return 0;
}

void removeTopic(uint8_t topicIndex, topic *topics)
{
    topics[topicIndex].name[0] = '\0';
}

uint8_t getTopicIndex(char *name, topic *topics, uint8_t topicCount)
{
    uint8_t i;
    for(i = 1; i < topicCount; i++)
    {
        if(strcmp(name, topics[i].name) == 0)
            return i;
    }
    return 0;
}

void *getMqttMessage(uint8_t *data)
{
    uint32_t msgLength;
    uint16_t topicLength;

    
    data++;
    msgLength = 0;
    msgLength += *data & 0x7F;
    if(*data & 0x80)
    {
        msgLength += (*data & 0x7F) << 7;
        data++;
        if(*data & 0x80)
        {
            data++;
            msgLength += (*data & 0x7F) << 14;
            if(*data & 0x80)
            {
                data++;
                msgLength += *data << 21;
            }   
        }  
    }
    data++;

    topicLength = 0;
    topicLength += (data[0] & 0xFF) << 8;
    topicLength += data[1] & 0xFF;

    return &(data[2 + topicLength]);
}

uint16_t getMqttMessageLength(uint8_t *data)
{
    uint32_t msgLength;
    uint16_t topicLength;
    
    data++;
    msgLength = 0;
    msgLength += *data & 0x7F;
    if(*data & 0x80)
    {
        msgLength += (*data & 0x7F) << 7;
        data++;
        if(*data & 0x80)
        {
            data++;
            msgLength += (*data & 0x7F) << 14;
            if(*data & 0x80)
            {
                data++;
                msgLength += *data << 21;
            }   
        }  
    }
    data++;

    topicLength = 0;
    topicLength += (data[0] & 0xFF) << 8;
    topicLength += data[1] & 0xFF;

    return msgLength - topicLength - 2;
}