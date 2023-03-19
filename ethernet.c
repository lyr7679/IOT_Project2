// Ethernet Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6

// Pinning for IoT projects with wireless modules:
// N24L01+ RF transceiver
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS on PE0
//   INT on PB2
// Xbee module
//   DIN (UART1TX) on PC5
//   DOUT (UART1RX) on PC4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "tm4c123gh6pm.h"
#include "clock.h"
#include "eeprom.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "timer.h"
#include "eth0.h"
#include "arp.h"
#include "ip.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include "mqtt.h"

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4

// EEPROM Map
#define EEPROM_DHCP        1
#define EEPROM_IP          2
#define EEPROM_SUBNET_MASK 3
#define EEPROM_GATEWAY     4
#define EEPROM_DNS         5
#define EEPROM_TIME        6
#define EEPROM_MQTT        7
#define EEPROM_ERASED      0xFFFFFFFF

bool connectCommand = false;
char *topicName;
char *topicData;
//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
    enablePinPullup(PUSH_BUTTON);
}

void displayConnectionInfo()
{
    uint8_t i;
    char str[10];
    uint8_t mac[6];
    uint8_t ip[4];
    getEtherMacAddress(mac);
    putsUart0("  HW:    ");
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%02"PRIu8, mac[i]);
        putsUart0(str);
        if (i < HW_ADD_LENGTH-1)
            putcUart0(':');
    }
    putcUart0('\n');
    getIpAddress(ip);
    putsUart0("  IP:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH-1)
            putcUart0('.');
    }
    putsUart0(" (static)");
    putcUart0('\n');
    getIpSubnetMask(ip);
    putsUart0("  SN:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH-1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpGatewayAddress(ip);
    putsUart0("  GW:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH-1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpDnsAddress(ip);
    putsUart0("  DNS:   ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH-1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpTimeServerAddress(ip);
    putsUart0("  Time:  ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH-1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpMqttBrokerAddress(ip);
    putsUart0("  MQTT:  ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH-1)
            putcUart0('.');
    }
    putcUart0('\n');
    if (isEtherLinkUp())
        putsUart0("Link is up\n");
    else
        putsUart0("Link is down\n");
}

void displayStatusInfo()
{
    uint8_t i;
    char str[10];
    char *tcp_str;
    char *mqtt_str;
    uint8_t ip[4];

    putcUart0('\n');
    getIpAddress(ip);
    putsUart0("  IP:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH-1)
            putcUart0('.');
    }

    putcUart0('\n');
    getIpMqttBrokerAddress(ip);
    putsUart0("  MQTT:  ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH-1)
            putcUart0('.');
    }

    putcUart0('\n');
    mqttGetState(&mqtt_str);
    putsUart0("  MQTT STATE:  ");
    putsUart0(mqtt_str);

    putcUart0('\n');
    tcpGetState(&tcp_str);
    putsUart0("  TCP STATE:  ");
    putsUart0(tcp_str);
    putcUart0('\n');
}
void readConfiguration()
{
    uint32_t temp;
    uint8_t* ip;

    temp = readEeprom(EEPROM_IP);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*)&temp;
        setIpAddress(ip);
    }
    temp = readEeprom(EEPROM_SUBNET_MASK);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*)&temp;
        setIpSubnetMask(ip);
    }
    temp = readEeprom(EEPROM_GATEWAY);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*)&temp;
        setIpGatewayAddress(ip);
    }
    temp = readEeprom(EEPROM_DNS);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*)&temp;
        setIpDnsAddress(ip);
    }
    temp = readEeprom(EEPROM_TIME);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*)&temp;
        setIpTimeServerAddress(ip);
    }
    temp = readEeprom(EEPROM_MQTT);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*)&temp;
        setIpMqttBrokerAddress(ip);
    }
}

#define MAX_CHARS 80
char strInput[MAX_CHARS+1];
char* token;
uint8_t count = 0;

uint8_t asciiToUint8(const char str[])
{
    uint8_t data;
    if (str[0] == '0' && tolower(str[1]) == 'x')
        sscanf(str, "%hhx", &data);
    else
        sscanf(str, "%hhu", &data);
    return data;
}

void processShell()
{
    bool end;
    char c;
    uint8_t i;
    uint8_t ip[IP_ADD_LENGTH];
    uint32_t* p32;

    if (kbhitUart0())
    {
        c = getcUart0();

        end = (c == 13) || (count == MAX_CHARS);
        if (!end)
        {
            if ((c == 8 || c == 127) && count > 0)
                count--;
            if (c >= ' ' && c < 127)
                strInput[count++] = c;
        }
        else
        {
            strInput[count] = '\0';
            count = 0;
            token = strtok(strInput, " ");
            if (strcmp(token, "ifconfig") == 0)
            {
                displayConnectionInfo();
            }
            if (strcmp(token, "reboot") == 0)
            {
                NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
            }
            if (strcmp(token, "connect") == 0)
            {
                arpReqFlag = 1;
                connectCommand = true;
            }
            if (strcmp(token, "disconnect") == 0)
            {
                //tcpAckFinFlag = 1;
                //tcpState = TCP_FIN_WAIT_2;
                mqttDisconnFlag = 1;
            }
            if (strcmp(token, "finish") == 0)
            {
                tcpAckFinFlag = 1;
                tcpState = TCP_FIN_WAIT_2;
            }
            if (strcmp(token, "publish") == 0)
            {
                token = strtok(NULL, " ");
                topicName = token;

                token = strtok(NULL, "\0");
                topicData = token;

                mqttPubFlag = 1;
            }
            if (strcmp(token, "subscribe") == 0)
            {
                token = strtok(NULL, " ");
                topicName = token;
                mqttSubFlag = 1;
            }
            if (strcmp(token, "unsubscribe") == 0)
            {
                token = strtok(NULL, " ");
                topicName = token;
                mqttUnsubFlag = 1;
            }
            if (strcmp(token, "status") == 0)
            {
                displayStatusInfo();
            }
            if (strcmp(token, "set") == 0)
            {
                token = strtok(NULL, " ");
                if (strcmp(token, "ip") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpAddress(ip);
                    p32 = (uint32_t*)ip;
                    writeEeprom(EEPROM_IP, *p32);
                }
                if (strcmp(token, "sn") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpSubnetMask(ip);
                    p32 = (uint32_t*)ip;
                    writeEeprom(EEPROM_SUBNET_MASK, *p32);
                }
                if (strcmp(token, "gw") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpGatewayAddress(ip);
                    p32 = (uint32_t*)ip;
                    writeEeprom(EEPROM_GATEWAY, *p32);
                }
                if (strcmp(token, "dns") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpDnsAddress(ip);
                    p32 = (uint32_t*)ip;
                    writeEeprom(EEPROM_DNS, *p32);
                }
                if (strcmp(token, "time") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpTimeServerAddress(ip);
                    p32 = (uint32_t*)ip;
                    writeEeprom(EEPROM_TIME, *p32);
                }
                if (strcmp(token, "mqtt") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpMqttBrokerAddress(ip);
                    p32 = (uint32_t*)ip;
                    writeEeprom(EEPROM_MQTT, *p32);
                }
            }

            if (strcmp(token, "help") == 0)
            {
                putsUart0("Commands:\r");
                putsUart0("  ifconfig\r");
                putsUart0("  reboot\r");
                putsUart0("  set ip|gw|dns|time|mqtt|sn w.x.y.z\r");
                putsUart0("  status\r");
                putsUart0("  connect\r");
                putsUart0("  disconnect\r");
                putsUart0("  publish topic topic_data\r");
                putsUart0("  subscribe topic\r");
            }
        }
    }
}

void checkPending(etherHeader *ether, socket *s)
{
    if(arpReqFlag)
    {
        uint8_t i;
        uint8_t mqtt_ip[4];
        uint8_t ip2[] = {0, 0, 0, 0};
        getIpMqttBrokerAddress(mqtt_ip);

        if(memcmp(mqtt_ip, ip2, sizeof(mqtt_ip)) != 0)
        {
            for(i = 0; i < IP_ADD_LENGTH; i++)
            {
                 s->remoteIpAddress[i] = mqtt_ip[i];
            }
        }
        uint8_t ip[4];
        getIpAddress(ip);
        sendArpRequest(ether, ip, s->remoteIpAddress);
        arpReqFlag = 0;
    }

    if(arpResFlag)
    {
        sendArpResponse(ether);
        arpResFlag = 0;
    }

    if(tcpSynFlag)
    {
        sendTcpMessage(ether, s, SYN, NULL, 0);
        tcpSynFlag = 0;
        s->sequenceNumber++;
    }

    if(tcpAckFlag)
    {
        sendTcpMessage(ether, s, ACK, NULL, 0);
        tcpAckFlag = 0;
    }

    if(tcpAckFinFlag)
    {
        sendTcpMessage(ether, s, (ACK | FIN), NULL, 0);
        tcpAckFinFlag = 0;
    }

    if(tcpFinFlag)
    {
        sendTcpMessage(ether, s, FIN, NULL, 0);
        tcpFinFlag = 0;
    }
    if(mqttConnFlag)
    {
        //mqttState = MQTT_CONNECT_SENT;
        sendMqttConnect(ether, s, CLEAN_SESH, "raquel");
        mqttConnFlag = 0;
    }

    if(mqttPubFlag)
    {
        mqttState = MQTT_PUBLISH;
        sendMqttPublish(ether, s, topicName, topicData);
        mqttPubFlag = 0;
    }

    if(mqttSubFlag)
    {
        mqttState = MQTT_SUBSCRIBE;
        sendMqttSubscribe(ether, s, topicName);
        mqttSubFlag = 0;
    }

    if(mqttUnsubFlag)
    {
        mqttState = MQTT_UNSUBSCRIBE;
        sendMqttUnsub(ether, s, topicName);
        mqttUnsubFlag = 0;
    }

    if(mqttDisconnFlag)
    {
        mqttState = MQTT_DISCONNECT;
        sendMqttDisconnect(ether, s);
        mqttDisconnFlag = 0;
    }
}
//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522

int main(void)
{
    uint8_t* udpData;
    uint8_t buffer[MAX_PACKET_SIZE];
    etherHeader *data = (etherHeader*) buffer;
    socket s;
    uint8_t ip[4];
    int i, check = 0;

    tcpState = TCP_CLOSED;
    mqttState = MQTT_DISCONNECT;

    // Init controller
    initHw();

    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    // Init timer
    initTimer();

    // Init ethernet interface (eth0)
    // Use the value x from the spreadsheet
    putsUart0("\nStarting eth0\n");
    initEther(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    setEtherMacAddress(2, 3, 4, 5, 6, 72);

    // Init EEPROM
    initEeprom();
    readConfiguration();
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);

    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity

    s.localPort = 65530;
    s.remotePort = 1883;

//    s.remoteIpAddress[0] = 192;
//    s.remoteIpAddress[1] = 168;
//    s.remoteIpAddress[2] = 1;
//    s.remoteIpAddress[3] = 1;

    getIpMqttBrokerAddress(ip);

    for(i = 0; i < IP_ADD_LENGTH; i++)
    {
        if(ip[i] == 0)
            check++;
    }

    if(check != 4)
    {
        for(i = 0; i < IP_ADD_LENGTH; i++)
        {
             s.remoteIpAddress[i] = ip[i];
        }
        arpReqFlag = 1;
        connectCommand = true;
    }

    s.sequenceNumber = random32();

    while (true)
    {

        // Put terminal processing here
        processShell();

        checkPending(data, &s);

        // Packet processing
        if (isEtherDataAvailable())
        {
            if (isEtherOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
            }

            // Get packet
            getEtherPacket(data, MAX_PACKET_SIZE);



            // Handle ARP request
            if (isArpRequest(data))
            {
                sendArpResponse(data);
            }

            if(isArpResponse(data))
            {
                processArpResponse(data, &s);

                if(connectCommand)
                //if(!check)
                {
                    tcpSynFlag = 1;
                    //check++;
                    //tcpState = TCP_SYN_SENT;
                    connectCommand = false;
                }
            }
            // Handle IP datagram
            if (isIp(data))
            {
                if (isIpUnicast(data))
                {
                    // Handle ICMP ping request
                    if (isPingRequest(data))
                    {
                        sendPingResponse(data);
                    }

                    // Handle UDP datagram
                    if (isUdp(data))
                    {
                        udpData = getUdpData(data);
                        if (strcmp((char*)udpData, "on") == 0)
                            setPinValue(GREEN_LED, 1);
                        if (strcmp((char*)udpData, "off") == 0)
                            setPinValue(GREEN_LED, 0);
                        getUdpMessageSocket(data, &s);
                        sendUdpMessage(data, s, (uint8_t*)"Received", 9);
                    }

                    // Handle TCP datagram
                    if (isTcp(data))
                    {
                        processTcp(data, &s);
                    }
                }
            }
        }
    }
}
