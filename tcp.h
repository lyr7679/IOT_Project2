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

#ifndef TCP_H_
#define TCP_H_

#include <stdint.h>
#include <stdbool.h>
#include "ip.h"
#include "mqtt.h"

typedef struct _tcpHeader // 20 or more bytes
{
  uint16_t sourcePort;
  uint16_t destPort;
  uint32_t sequenceNumber;
  uint32_t acknowledgementNumber;
  uint16_t offsetFields;
  uint16_t windowSize;
  uint16_t checksum;
  uint16_t urgentPointer;
  uint8_t  data[0];
} tcpHeader;

// TCP states
#define TCP_CLOSED 0
#define TCP_LISTEN 1
#define TCP_SYN_RECEIVED 2
#define TCP_SYN_SENT 3
#define TCP_ESTABLISHED 4
#define TCP_FIN_WAIT_1 5
#define TCP_FIN_WAIT_2 6
#define TCP_CLOSING 7
#define TCP_CLOSE_WAIT 8
#define TCP_LAST_ACK 9
#define TCP_TIME_WAIT 10

// ARP states
#define ARP_REQ_SENT 11
#define ARP_REQ_RECEIVED 12
#define ARP_RES_SENT 13
#define ARP_RES_RECEIVED 14


// TCP offset/flags
#define FIN 0x0001
#define SYN 0x0002
#define RST 0x0004
#define PSH 0x0008
#define ACK 0x0010
#define URG 0x0020
#define ECE 0x0040
#define CWR 0x0080
#define NS  0x0100
#define OFS_SHIFT 12


uint8_t tcpState;

uint8_t arpReqFlag;
uint8_t arpResFlag;

uint8_t tcpSynFlag;
uint8_t tcpAckFlag;
uint8_t tcpFinFlag;

uint8_t tcpSynAckFlag;
uint8_t tcpAckFinFlag;
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void tcpSetState(uint8_t state);
void tcpGetState(char **tcp_str);

bool tcpIsSyn(etherHeader *ether);
bool tcpIsAck(etherHeader *ether);

bool isTcp(etherHeader *ether);

// TODO: Add functions here
bool tcpIsFin(etherHeader *ether);

void processTcp(etherHeader *ether, socket *s);

void sendTcpMessage(etherHeader *ether, socket *s, uint16_t flags, uint8_t data[], uint16_t dataSize);

void processArpResponse(etherHeader *ether, socket *s);

#endif
