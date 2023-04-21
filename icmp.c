// ICMP Library
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
#include "icmp.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------

// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Determines whether packet is ping request
// Must be an IP packet
bool isPingRequest(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    icmpHeader *icmp = (icmpHeader*)((uint8_t*)ip + ipHeaderLength);
    return (ip->protocol == PROTOCOL_ICMP && icmp->type == 8);
}

bool isPingResponse(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    icmpHeader *icmp = (icmpHeader*)((uint8_t*)ip + ipHeaderLength);
    return (ip->protocol == PROTOCOL_ICMP && icmp->type == 0);
}
// Sends a ping response given the request data
void sendPingResponse(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    icmpHeader *icmp = (icmpHeader*)((uint8_t*)ip + ipHeaderLength);
    uint8_t i, tmp;
    uint16_t icmp_size;
    uint32_t sum = 0;
    // swap source and destination fields
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        tmp = ether->destAddress[i];
        ether->destAddress[i] = ether->sourceAddress[i];
        ether->sourceAddress[i] = tmp;
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        tmp = ip->destIp[i];
        ip->destIp[i] = ip ->sourceIp[i];
        ip->sourceIp[i] = tmp;
    }
    // this is a response
    icmp->type = 0;
    // calc icmp checksum
    icmp->check = 0;
    icmp_size = ntohs(ip->length) - ipHeaderLength;
    sumIpWords(icmp, icmp_size, &sum);
    icmp->check = getIpChecksum(sum);
    // send packet
    putEtherPacket(ether, sizeof(etherHeader) + ntohs(ip->length));
}

void sendPing(etherHeader *ether, socket s)
{
    uint8_t i, tmp;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    ipHeader *ip = (ipHeader*)ether->data;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_ICMP;
    ip->headerChecksum = 0;
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }
    uint8_t ipHeaderLength = ip->size * 4;
    ip->length = htons(ipHeaderLength + sizeof(icmpHeader));
    calcIpChecksum(ip);
    
    icmpHeader *icmp = (icmpHeader*)((uint8_t*)ip + ipHeaderLength);
    
    uint16_t icmp_size;
    
    icmp->type = 8;
    icmp->code = 0;
    // calc icmp checksum
    icmp->check = 0;
    icmp_size = 8;

    uint32_t sum = 0;
    sumIpWords(icmp, icmp_size, &sum);
    icmp->check = getIpChecksum(sum);
    // send packet
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + 8);
}