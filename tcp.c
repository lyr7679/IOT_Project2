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
#include "tcp.h"
#include "timer.h"
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "arp.h"
//#include "mqtt.h"

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


// TODO: Add functions here

tcpTimer()
{
    switch(tcpState)
    {
    case TCP_CLOSED:
    {

    }
    break;
    case TCP_SYN_SENT:
    {
        tcpSynFlag = 1;
    }
        break;
    case TCP_ESTABLISHED:
    {

    }
        break;
    case TCP_FIN_WAIT_2:
        tcpAckFinFlag = 1;
        break;
    case TCP_LAST_ACK:
        tcpAckFinFlag = 1;
        break;
    case TCP_TIME_WAIT:
        break;
    }
}

void sendTcpMessage(etherHeader *ether, socket *s, uint16_t flags, uint8_t data[], uint16_t dataSize)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint16_t tcpHeaderLength;
    uint8_t *copyData;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);

    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s->remoteHwAddress[i];
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

    uint8_t ipHeaderLength = ip->size * 4;

    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s->remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    // TCP header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(s->localPort);
    tcp->destPort = htons(s->remotePort);

    tcp->windowSize = htons(1522);

    // adjust lengths
    tcpHeaderLength = sizeof(tcpHeader);
    tcpLength = tcpHeaderLength + dataSize;
    ip->length = htons(ipHeaderLength + tcpLength);

    // 32-bit sum over ip header
    calcIpChecksum(ip);

    // set tcp length
    //tcp->length = htons(tcpLength);

    uint16_t total_offset = 0;

    total_offset |= (5 << OFS_SHIFT) | flags;

    //tcp->offsetFields = htons(SYN);
    //tcp->offsetFields = (ipHeaderLength << 10) | SYN;
    tcp->offsetFields = htons(total_offset);

    //getting/setting sequence/acknowledgement num
     if(tcpState == TCP_CLOSED)
     {
         s->sequenceNumber = random32();
         s->acknowledgementNumber = 0;
         tcpState = TCP_SYN_SENT;
         startPeriodicTimer(tcpTimer, 5);
     }

     tcp->sequenceNumber = htonl(s->sequenceNumber);
     tcp->acknowledgementNumber = htonl(s->acknowledgementNumber);

    // copy data
    copyData = tcp->data;

    for (i = 0; i < dataSize; i++)
        copyData[i] = data[i];

    // 32-bit sum over pseudo-header
    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    sum += htons(tcpLength);
    //sumIpWords(&tcp->length, 2, &sum);

    // add tcp header
    tcp->checksum = 0;
    sumIpWords(tcp, tcpLength, &sum);
    tcp->checksum = getIpChecksum(sum);

    if(tcpState == TCP_TIME_WAIT)
        tcpState = TCP_CLOSED;

    // send packet with size = ether + tcp hdr + ip header + tcp_size
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}


void tcpSetState(uint8_t state)
{
    tcpState = state;
}

void tcpGetState(char **tcp_str)
{
    switch(tcpState)
    {
    case TCP_CLOSED:
        *tcp_str = "CLOSED";
        break;
    case TCP_SYN_SENT:
        *tcp_str = "SYN_SENT";
        break;
    case TCP_ESTABLISHED:
        *tcp_str = "ESTABLISHED";
        break;
    case TCP_FIN_WAIT_2:
        *tcp_str = "FIN_WAIT_2";
        break;
    case TCP_LAST_ACK:
        *tcp_str = "LAST_ACK";
        break;
    case TCP_TIME_WAIT:
        *tcp_str = "TIME_WAIT";
        break;
    }
}

bool tcpIsSyn(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));

    uint16_t curr_flags = htons(tcp->offsetFields);

    if((curr_flags & SYN) != 0)
        return true;
    return false;
}

bool tcpIsAck(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));

    uint16_t curr_flags = htons(tcp->offsetFields);

    if((curr_flags & ACK) != 0)
        return true;
    return false;

}

bool tcpIsFin(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));

    uint16_t curr_flags = htons(tcp->offsetFields);

    if((curr_flags & FIN) != 0)
        return true;
    return false;
}

bool tcpIsPsh(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));

    uint16_t curr_flags = htons(tcp->offsetFields);

    if((curr_flags & PSH) != 0)
        return true;
    return false;
}


void processTcp(etherHeader *ether, socket *s)
{
    ipHeader *ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    uint32_t dataSize = 0;

    switch(tcpState)
    {
        case TCP_CLOSED:
//            if(s->remoteHwAddress == NULL)
//            {
//                //means we don't know MAC for destination
//                arpReqFlag = 1;
//                //start timer waiting for arp response
//                tcpState = ARP_REQ_SENT;
//            }
//            else
//            {
//                tcpSynFlag = 1;
//                //start timer waiting for SYN+ACK
//                tcpState = TCP_SYN_SENT;
//            }
            break;

        case TCP_SYN_SENT:
            //putsUart0("\nstate: SYN_SENT\n");
            if(tcpIsSyn(ether) && tcpIsAck(ether))
            {
                stopTimer(tcpTimer);
                s->acknowledgementNumber = htonl(tcp->sequenceNumber) + 1;
                tcpAckFlag = 1;
                tcpState = TCP_ESTABLISHED;

                //after established we want to go "connected" state
                mqttConnFlag = 1;
            }
            break;

        case TCP_ESTABLISHED:
            //passive close
            putsUart0("\nstate: ESTABLISHED\n");
            s->sequenceNumber = htonl(tcp->acknowledgementNumber);
            if(tcpIsFin(ether) && tcpIsAck(ether))
            {
                s->acknowledgementNumber = htonl(tcp->sequenceNumber) + 1;
                tcpAckFinFlag = 1;
                restartTimer(tcpTimer);
                tcpState = TCP_LAST_ACK;
            }
            //active close
            else if( 0 == 1 /*flag is set for an active close?*/)
            {
                tcpFinFlag = 1;
                //start timer waiting for ack
                tcpState = TCP_FIN_WAIT_1;
            }
            //mqtt data coming in
            else if(tcpIsPsh(ether) && tcpIsAck(ether))
            {
                dataSize = htons(ip->length) - (ip->size * 4) - sizeof(tcpHeader);
                s->acknowledgementNumber = htonl(tcp->sequenceNumber) + dataSize;
                tcpAckFlag = 1;
                //processMqtt(ether, s);
            }
            break;

        case TCP_LAST_ACK:
            if(tcpIsAck(ether))
            {
                stopTimer(tcpTimer);
                tcpState = TCP_CLOSED;
            }
            break;

        case TCP_FIN_WAIT_1:
            if(tcpIsAck(ether))
            {
                //start timer waiting for fin
                tcpState = TCP_FIN_WAIT_2;
            }
            break;

        case TCP_FIN_WAIT_2:
            //if we remove this line it becomes a KEEP_ALIVE
            s->sequenceNumber = htonl(tcp->acknowledgementNumber);
            if(tcpIsFin(ether) && tcpIsAck(ether))
            {
                stopTimer(tcpTimer);
                s->acknowledgementNumber = htonl(tcp->sequenceNumber) + 1;
                tcpAckFlag = 1;
                tcpState = TCP_TIME_WAIT;
            }
            break;

        case TCP_TIME_WAIT:
            //wait for specified amount of time
            tcpState = TCP_CLOSED;
            break;
    }
}

void processArpResponse(etherHeader *ether, socket *s)
{
    //fill in hw address
    uint8_t i;
    arpPacket *arp = (arpPacket*)ether->data;

    //check source ip matches hardcoded ip (192.168.1.1)
    if(memcmp(arp->sourceIp, s->remoteIpAddress, sizeof(arp->sourceIp)) == 0)
    {
        for(i = 0; i < HW_ADD_LENGTH; i++)
        {
            s->remoteHwAddress[i] = ether->sourceAddress[i];
        }
    }

}

