// TCP Library (framework only)
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
#include <stdint.h>
#include <stdbool.h>
#include "tcp.h"
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

// Determines whether packet is TCP packet
// Must be an IP packet
bool isTcp(etherHeader* ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    uint32_t sum = 0;
    bool ok;
    uint16_t tmp16;
    ok = (ip->protocol == PROTOCOL_TCP);
    if (ok)
    {
        // 32-bit sum over pseudo-header
        sumIpWords(ip->sourceIp, 8, &sum);
        tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;
        tmp16 = htons(ntohs(ip->length) - (ip->size * 4));
        sumIpWords(&tmp16, 2, &sum);
        // add tcp header and data
        sumIpWords(tcp, ntohs(ip->length) - (ip->size * 4), &sum);
        ok = (getIpChecksum(sum) == 0);
    }

    return ok;
}


void sendTcpMessage(etherHeader *ether, socket s, uint32_t flags, uint8_t data[], uint16_t dataSize)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint8_t *copyData;
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
    
    // TCP header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(s.localPort);
    tcp->destPort = htons(s.remotePort);
    uint8_t ipHeaderLength = ip->size * 4;

    switch(s.state)
    {
        case TCP_SYN_SENT:
            if(flags == SYN)
            {
                tcp->sequenceNumber = htonl(random32());
                tcp->acknowledgementNumber = 0;
            }
            else
            {
                tcp->sequenceNumber = htonl(s.acknowledgementNumber);
                tcp->acknowledgementNumber = htonl(s.sequenceNumber + 1);       
            }
            break;
        case TCP_FIN_WAIT_1:
            tcp->sequenceNumber = htonl(s.acknowledgementNumber);
            tcp->acknowledgementNumber = 0;
            break;
        case TCP_CLOSE_WAIT:
            tcp->sequenceNumber = htonl(s.acknowledgementNumber);
            tcp->acknowledgementNumber = htonl(s.sequenceNumber + 1);
            if(flags == FIN)
            {
                tcp->sequenceNumber = htonl(s.acknowledgementNumber);
                tcp->acknowledgementNumber = 0;
            }
            break;
        default:
            tcp->sequenceNumber = htonl(s.acknowledgementNumber);
            tcp->acknowledgementNumber = htonl(s.sequenceNumber);   
            break;
    }
    s.acknowledgementNumber += dataSize;

    // TCP flags and offset
    tcp->offsetFields = htons(flags | 0x5000);
    tcp->windowSize = htons((uint16_t)1500);
    tcp->urgentPointer = 0;
    // adjust lengths
    tcpLength = sizeof(tcpHeader) + dataSize;
    ip->length = htons(ipHeaderLength + tcpLength);
    

    // copy data
    copyData = tcp->data;
    for (i = 0; i < dataSize; i++)
        copyData[i] = data[i];
    
    // 32-bit sum over ip header
    calcIpChecksum(ip);
    calcTcpChecksum(ip, tcpLength);
    
    // send packet with size = ether + ip header + tcp header + data size
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}
// Sets socket state
void setTcpState(socket *s, uint8_t state)
{
    s->state = state;
}

// Gets pointer to TCP payload of frame
uint8_t* getTcpData(etherHeader *ether, socket *s)
{
    uint16_t dataSize;

    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    // Find Payload size
    if (s->state != TCP_ESTABLISHED)
        dataSize = 1;
    else
    {
        uint16_t tcpLength = ((tcp->offsetFields & 0xF0) >> 4) * 4;
        uint16_t ipLength = ip->size * 4;
        uint16_t totalSize = ntohs(ip->length);
        dataSize = totalSize - ipLength - tcpLength;
    }
    
    // if((s->acknowledgementNumber != ntohl(tcp->acknowledgementNumber)) && s->state == TCP_ESTABLISHED)
    //     return NULL;

    // Update seq numbers
    s->sequenceNumber = ntohl(tcp->sequenceNumber) + dataSize;
    s->acknowledgementNumber = ntohl(tcp->acknowledgementNumber);

    
    return tcp->data;
}

bool establishTcpSocket(socket *sockets, uint8_t socketCount ,socket *s)
{
    uint8_t i;
    bool ok = false;
    uint8_t socketNumber = UINT8_MAX;
    while(!ok) 
    {
        socketNumber++;
        if(socketNumber >= socketCount)
            return false;
        // Empty socket slot
        ok = sockets[socketNumber].localPort == 0;
    }
    
    for (i = 0; i < HW_ADD_LENGTH; i++)
        sockets[socketNumber].remoteHwAddress[i] = s->remoteHwAddress[i];
    for (i = 0; i < IP_ADD_LENGTH; i++)
        sockets[socketNumber].remoteIpAddress[i] = s->remoteIpAddress[i];
    sockets[socketNumber].localPort = s->localPort;
    sockets[socketNumber].remotePort = s->remotePort;
    sockets[socketNumber].sequenceNumber = 0;
    sockets[socketNumber].acknowledgementNumber = 0;
    sockets[socketNumber].id = PROTOCOL_TCP;
    return true;
}

uint16_t getTcpFlags(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    return (ntohs(tcp->offsetFields)) & 0xFF;
}

uint8_t isTcpSocketConnected(etherHeader *ether, socket *sockets, uint8_t socketCount)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    
    // Find Socket
    uint16_t socketNumber = ntohs(tcp->destPort) & 0xF;
    if(socketNumber >= socketCount)
        return socketCount;

    if(sockets[socketNumber].remotePort != ntohs(tcp->sourcePort))
        return socketCount;

    return socketNumber;
}

void calcTcpChecksum(ipHeader *ip, uint16_t tcpLength)
{
    uint16_t pseudoTcpLength = htons(tcpLength);
    uint32_t sum = 0;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    sumIpWords(ip->sourceIp, 8, &sum);
    uint16_t tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    sumIpWords(&pseudoTcpLength, 2, &sum);

    tcp->checksum = 0;
    sumIpWords(tcp, tcpLength, &sum);
    tcp->checksum = getIpChecksum(sum);
}
