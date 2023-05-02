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
#include "wireless.h"
#include "hashTable.h"

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

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522

#define MAX_SOCKETS 5
#define MAX_TOPICS 20

#define MAX_ARP_TIMEOUT 5
#define MAX_TCP_HANDSHAKE_TIMEOUT 5

#define MQTT_DEFAULT_CONFIG_NARGS 1
#define MQTT_DEFAULT_N_TOPICS 3


//adafruit credentials
#define IO_USRNAME "uta_iot"
#define IO_KEY "aio_KmSy234QQLLYKPCTaL2d8ZZJA1oh"

#define INPUT 1
#define OUTPUT 0


//-----------------------------------------------------------------------------
// Globals                
//-----------------------------------------------------------------------------

socket sockets[MAX_SOCKETS];
topic topics[MAX_TOPICS] = {""};
uint32_t pingTime;

//-----------------------------------------------------------------------------
// Flags                
//-----------------------------------------------------------------------------
uint8_t gf_arp_send_request;
uint8_t gf_rx_arp;

uint8_t gf_send_ping;
uint8_t gf_rx_ping;

// TCP Flags
uint8_t gf_tcp_send_syn;
uint8_t gf_tcp_send_ack;
uint8_t gf_disconnect;
uint8_t gf_tcp_send_fin;
uint8_t gf_rx_lastAck;
uint8_t gf_tcp_rx_synack;

// MQTT Flags
uint8_t gf_mqtt_connect;
uint8_t gf_close_socket;
uint8_t gf_mqtt_subscribe;
uint8_t gf_mqtt_unsubscribe;
uint8_t gf_mqtt_publish;
uint8_t gf_mqtt_disconnect;
uint8_t gf_mqtt_rx_connack;
uint8_t gf_mqtt_rx_suback;
uint8_t gf_mqtt_rx_unsuback;
uint8_t gf_mqtt_connect_default;
uint8_t gf_mqtt_subscribe_default;
uint8_t gf_tcp_send_finack;
uint8_t gf_mqtt_rx_suback_default;




//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------

void deleteSocket(socket *s)
{
    uint8_t i;

    for(i = 0; i < HW_ADD_LENGTH; i++)
        s->remoteHwAddress[i] = 0;
    for(i = 0; i < IP_ADD_LENGTH; i++)
        s->remoteIpAddress[i] = 0;
    s->localPort = 0;
    s->remotePort = 0;
    s->id = 0;
    s->state = 0;
}


void closeSocketCallback()
{
    if(!gf_close_socket)    
        return;
    gf_disconnect = gf_close_socket;
    gf_close_socket = 0;
}

void tcpHandshakeTimeoutCallback()
{
    static uint8_t count = 0;
    if(!gf_tcp_rx_synack || gf_tcp_send_syn)
        return;

    count++;
    if(count >= MAX_TCP_HANDSHAKE_TIMEOUT)
    {
        putsUart0("Unable to connect");
        putcUart0('\n');
        deleteSocket(&(sockets[gf_rx_arp]));
        gf_mqtt_connect_default = 0;
        gf_mqtt_connect = 0;
        gf_tcp_rx_synack = 0;
        count = 0;
    }
    else
    {
        restartTimer(tcpHandshakeTimeoutCallback);
        gf_tcp_send_syn = gf_tcp_rx_synack;
        gf_tcp_rx_synack = 0;
        putsUart0("Tcp timeout\n");
    }
}

void arpRequestTimeoutCallback()
{
    static uint8_t count = 0;
    if(!gf_rx_arp || gf_arp_send_request)
        return;

    count++;
    if(count >= MAX_ARP_TIMEOUT)
    {
        putsUart0("Unable to reach ip");
        putcUart0('\n');
        deleteSocket(&(sockets[gf_rx_arp]));
        gf_mqtt_connect_default = 0;
        gf_mqtt_connect = 0;
        gf_rx_arp = 0;
        count = 0;
    }
    else
    {
        restartTimer(arpRequestTimeoutCallback);
        gf_arp_send_request = gf_rx_arp;
        gf_rx_arp = 0;
        putsUart0("Arp timeout\n");
    }
}

void setSocketHwAddress(etherHeader *ether, socket *s)
{
    arpPacket *arp = (arpPacket*)(ether->data);
    uint8_t i;
    uint8_t mqtt_ip[4];
    uint8_t gw_ip[4];
    getIpMqttBrokerAddress(mqtt_ip);
    getIpGatewayAddress(gw_ip);

    for (i = 0; i < HW_ADD_LENGTH; i++)
        s->remoteHwAddress[i] = arp->sourceAddress[i];

    if(memcmp(arp->sourceIp, gw_ip, sizeof(arp->sourceIp)) == 0)
    {
        for(i = 0; i < IP_ADD_LENGTH; i++)
        {
             s->remoteIpAddress[i] = mqtt_ip[i];
        }
    }
}

void removeDelimiters(char *input, char* output, char *delimiters)
{
    uint8_t i, j;
    uint8_t k = 0;
    for (i = 0; input[i] != '\0'; i++)
    {
        for (j = 0; delimiters[j] != '\0'; j++)
        {
            if(input[i] == delimiters[j])
            {
                i++;
                j = UINT8_MAX;
                continue;
            }
        }
        output[k++] = input[i];
    }
    output[k] = '\0';
}

uint8_t createSocket(uint16_t protocol, socket *s, uint8_t socketCount, uint8_t remote_ip[4], uint16_t remote_port)
{
    uint8_t i;
    uint8_t socketNum;
    bool found = false;
    for(i = 1; i < socketCount && !found; i++)
    {
        if(s[i].state == TCP_CLOSED)
        {
            found = true;
            socketNum = i;
        }
    }
    if(!found)
        return socketCount;
    // Make local port
    for(i = 0; i < IP_ADD_LENGTH; i++)
        s[socketNum].remoteIpAddress[i] = remote_ip[i];
    uint16_t temp = random32() & 0xFFFF;
    temp &= ~0xF;
    temp |= socketNum;
    s[socketNum].localPort = temp;
    s[socketNum].remotePort = remote_port;
    s[socketNum].state = 0;
    s[socketNum].id = protocol;
    return socketNum;
}


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

    setPinValue(BLUE_LED,1);  
}

initDefaultTimers()
{
    startOneshotTimer(arpRequestTimeoutCallback, 1);
    startOneshotTimer(tcpHandshakeTimeoutCallback, 3);
    startOneshotTimer(closeSocketCallback, 2);
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
char mqttMessages[MQTT_MAX_ARGUMENTS][MAX_CHARS];
uint8_t mqttFlags;
uint8_t nargs;
char* token;
uint8_t count = 0;

char MQTT_DEFAULT_CONFIG[MQTT_MAX_ARGUMENTS][MAX_CHARS] = {
    "Marvin"
};


uint8_t asciiToUint8(const char str[])
{
    uint8_t data;
    if (str[0] == '0' && tolower(str[1]) == 'x')
        sscanf(str, "%hhx", &data);
    else
        sscanf(str, "%hhu", &data);
    return data;
}

uint16_t asciiToUint16(const char str[])
{
    uint16_t data;
    if (str[0] == '0' && tolower(str[1]) == 'x')
        sscanf(str, "%hx", &data);
    else
        sscanf(str, "%hu", &data);
    return data;
}

bool checkRemoteBrokerAddress()
{
    uint8_t mqtt_ip[4];
    uint8_t local_ip[4];
    uint8_t gw_ip[4];
    int i, check = 0;

    getIpAddress(local_ip);
    getIpMqttBrokerAddress(mqtt_ip);
    getIpGatewayAddress(gw_ip);

    for(i = 0; i < IP_ADD_LENGTH - 1; i++)
    {
        if(local_ip[i] == mqtt_ip[i])
            check++;
    }

    if(check == 3)
    {
        return false;
    }
    else
        return true;
}

bool checkEmptyBrokerAddress()
{
    uint8_t ip[4];
    int i, check = 0;

    getIpMqttBrokerAddress(ip);

    for(i = 0; i < IP_ADD_LENGTH; i++)
    {
        if(ip[i] == 0)
            check++;
    }

    if(check != 4)
        return false;
    return true;
}

void processShell()
{
    bool end;
    char c;
    uint8_t i;
    uint8_t ip[IP_ADD_LENGTH];
    uint32_t* p32;
    char bufferTemp[80];

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
            if(strcmp(token, "macs") == 0)
            {
                snprintf(bufferTemp, 80, "%s", "\nDevice Number\t\tDevice MAC Address\n");
                putsUart0(bufferTemp);
                uint32_t temp = 0;
                uint16_t address = 14u;
                for(i = 0; i < readEeprom(10u); i++)
                {
                    temp = (readEeprom(address) & 0x0F000000) >> 24;
                    snprintf(bufferTemp, 80, "%d", temp);
                    putsUart0(bufferTemp);
                    address += 1u;
                    temp = readEeprom(address);
                    snprintf(bufferTemp, 80, "\t\t\t%d.%d.%d.%d\n", (temp & 0xFF000000) >> 24,
                    (temp & 0x00FF0000) >> 16, (temp & 0x0000FF00) >> 8, (temp & 0x000000FF));
                    putsUart0(bufferTemp);
                    address += 4u;
                }
            }
            if (strcmp(token, "ping") == 0)
            {
                token = strtok(NULL, " ");
                setShellDestinationDevNumber(atoi(token) & 0xFF);
                sendWirelessPing();
                
            }
            if (strcmp(token, "push") == 0)
            {
                
                token = strtok(NULL, " ");
                setShellDestinationDevNumber(atoi(token) & 0xFF);
                sendDevPush();
                
            }

            if (strcmp(token, "devCaps") == 0)
            {
                
                // token = strtok(NULL, " ");
                // setShellDestinationDevNumber(atoi(token) & 0xFF);
                sendDevCap();
            }
            if (strcmp(token, "reset") == 0)
            {
                token = strtok(NULL, " ");
                if(strcmp(token, "inteeprom") == 0){
                    for(i=0; i<200; i++){
                        writeEeprom(i + NO_OF_DEV_IN_BRIDGE , 0x00); // Flash the eeprom

                    }
                    putsUart0("Cleared Internal Eeprom until 200 bytes\n");
                }
            }

            if(strcmp(token, "showCaps") == 0)
            {
                MQTTBinding binding[3];
                char bindingTemp[30];
                char tempCaps[4][5] = {"MTRSP", "TEMPF", "BARCO", "DISTC"};
                uint16_t j = 0;
                char * inOrOut;
                snprintf(bufferTemp, 80, "%s", "Device Number\tType\tFunction\tUnits\n");

                for(i = 0; i < 4; i++)
                {
                    MQTTBinding *isBinding = mqtt_binding_table_get((MQTTBinding **)&binding, 3, tempCaps[i]);

                    if(isBinding != NULL)
                    {
                        for(j = 0; j < binding[0].numOfCaps; j++)
                        {
                            if(binding[j].inOut == INPUT)
                                strncpy(inOrOut, "input", 5);
                            else if(binding[j].inOut == OUTPUT)
                                strncpy(inOrOut, "output", 6);
                            strcpy(bindingTemp, strtok(binding[j].description, " "));
                            snprintf(bufferTemp, 80, "%c\t%s\t%s\t%s\n", binding[j].client_id[6], inOrOut, bindingTemp, strtok(NULL, " "));
                        }
                    }
                }

                if(isWebserverConnected())
                {
                    snprintf(bufferTemp, 80, "%d\tRefer to the Web Server\n", getWebserverDeviceNumber());
                }

            }
            if (strcmp(token, "ifconfig") == 0)
            {
                displayConnectionInfo();
            }
            if (strcmp(token, "reboot") == 0)
            {
                NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
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
                putsUart0("  set ip | gw | dns | time | mqtt | sn w.x.y.z\r");
                putsUart0("  macs (print assigned device MACs)\r");
            }
            if (strcmp(token, "status") == 0)
            {
                putsUart0("Prints status\n");
            }
            if (strcmp(token, "ping1") == 0)
            {
                uint8_t remote_ip[4];
                for (i = 0; i < IP_ADD_LENGTH; i++)
                {
                    token = strtok(NULL, " .");
                    remote_ip[i] = asciiToUint8(token);
                }
                uint8_t socketNum = createSocket(PROTOCOL_ICMP, sockets, MAX_SOCKETS, remote_ip, 0);
                if(socketNum < MAX_SOCKETS)
                {
                    restartTimer(arpRequestTimeoutCallback);
                    gf_arp_send_request = socketNum;
                }
                else
                {
                    putsUart0("No sockets available\n");
                }
            }
            if (strcmp(token, "connect") == 0)
            {
                uint8_t remote_ip[4];
                uint16_t remote_port;
                for (i = 0; i < IP_ADD_LENGTH; i++)
                {
                    token = strtok(NULL, " .");
                    remote_ip[i] = asciiToUint8(token);
                }
                token = strtok(NULL, " :");
                remote_port = asciiToUint16(token);
                
                
                uint8_t socketNum = createSocket(PROTOCOL_TCP, sockets, MAX_SOCKETS, remote_ip, remote_port);
                if(socketNum < MAX_SOCKETS)
                {
                    setTcpState(&(sockets[socketNum]), TCP_SYN_SENT);
                    restartTimer(arpRequestTimeoutCallback);
                    gf_arp_send_request = socketNum;
                }
                else
                {
                    putsUart0("No sockets available\n");
                }
                    
            }
            if (strcmp(token, "disconnect") == 0)
            {
                token = strtok(NULL, " ");
                uint8_t socketNumber = asciiToUint8(token);
                if(socketNumber < MAX_SOCKETS && socketNumber != 0)
                {
                    gf_tcp_send_fin = socketNumber;
                    setTcpState(&(sockets[socketNumber]), TCP_FIN_WAIT_1);
                }
                    
                
            }
            if (strcmp(token, "subscribe") == 0)
            {
                mqttFlags = 0;
                gf_mqtt_subscribe = 0;
                nargs = 0;
                token = strtok(NULL, " ");
                while(token != NULL)
                {
                    if(token[0] == '-')
                    {
                        switch(token[1])
                        {
                            case 'h': // Socket Number
                                token = strtok(NULL, " ");
                                gf_mqtt_subscribe = asciiToUint8(token);
                                break;
                            case 't': // Topic name
                                token = strtok(NULL, " ");
                                removeDelimiters(token, mqttMessages[nargs++], "");
                                gf_mqtt_rx_suback = addTopic(mqttMessages[nargs - 1], topics, MAX_TOPICS);
                                if(gf_mqtt_rx_suback == 0)
                                {
                                    gf_mqtt_subscribe = 0;
                                    return;
                                }
                                break;
                            case 'q':   // QoS
                                token = strtok(NULL, " ");
                                mqttFlags = asciiToUint8(token);
                                break;
                            default:
                                break;
                        }
                    }
                    token = strtok(NULL, " ");
                }
            }
            if (strcmp(token, "unsubscribe") == 0)
            {
                mqttFlags = 0;
                gf_mqtt_unsubscribe = 0;
                nargs = 0;
                token = strtok(NULL, " ");
                while(token != NULL)
                {
                    if(token[0] == '-')
                    {
                        switch(token[1])
                        {
                            case 'h': // Socket Number
                                token = strtok(NULL, " ");
                                gf_mqtt_unsubscribe = asciiToUint8(token);
                                break;
                            case 't': // Topic name
                                token = strtok(NULL, " ");
                                removeDelimiters(token, mqttMessages[nargs++], "");
                                gf_mqtt_rx_unsuback = getTopicIndex(token, topics, MAX_TOPICS);
                                break;
                            case 'q':   // QoS
                                token = strtok(NULL, " ");
                                mqttFlags = asciiToUint8(token);
                                break;
                            default:
                                break;
                        }
                    }
                    token = strtok(NULL, " ");
                }
            }
            if (strcmp(token, "ss") == 0)
            {
                uint8_t socketNumber, i;
                char str[10];
                uint8_t ip[4];
                putsUart0("Socket Number\t\tNetid\t\tState\t\tLocal Address:Port\tPeer Address:Port\n");
                for(socketNumber = 1; socketNumber < MAX_SOCKETS; socketNumber++)
                {
                    if(sockets[socketNumber].id == 0)
                        continue;
                    putcUart0('0' + socketNumber);
                    putsUart0("\t");
                    switch(sockets[socketNumber].id)
                    {
                        case PROTOCOL_ICMP:
                            putsUart0("icmp\t\t");
                            break;
                        case PROTOCOL_UDP:
                            putsUart0("udp\t\t");
                            break;
                        case PROTOCOL_TCP:
                            putsUart0("tcp\t\t");
                            switch(sockets[socketNumber].state)
                            {
                                case TCP_CLOSED:
                                    putsUart0("CLOSED");
                                    break;
                                case TCP_LISTEN:
                                    putsUart0("LISTEN");
                                    break;
                                case TCP_SYN_RECEIVED:
                                    putsUart0("SYN_RECEIVED\t");
                                    break;
                                case TCP_SYN_SENT:
                                    putsUart0("SYN_SENT");
                                    break;
                                case TCP_ESTABLISHED:
                                    putsUart0("ESTABLISHED");
                                    break;
                                case TCP_FIN_WAIT_1:
                                    putsUart0("FIN_WAIT_1");
                                    break;
                                case TCP_FIN_WAIT_2:
                                    putsUart0("FIN_WAIT_2");
                                    break;
                                case TCP_CLOSING:
                                    putsUart0("CLOSING");
                                    break;
                                case TCP_CLOSE_WAIT:
                                    putsUart0("CLOSING_WAIT");
                                    break;
                                case TCP_LAST_ACK:
                                    putsUart0("LAST_ACK");
                                    break;
                                case TCP_TIME_WAIT:
                                    putsUart0("WAITING");
                                    break;
                                default:
                                    break;
                            }
                            break;
                        default:
                            continue;
                            break;
                    }
                    putsUart0("\t");
                    getIpAddress(ip);
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
                        putsUart0(str);
                        if (i < IP_ADD_LENGTH-1)
                            putcUart0('.');
                    }
                    snprintf(str, sizeof(str), ":%"PRIu16"\t\t", sockets[socketNumber].localPort);
                    putsUart0(str);
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        snprintf(str, sizeof(str), "%"PRIu8, sockets[socketNumber].remoteIpAddress[i]);
                        putsUart0(str);
                        if (i < IP_ADD_LENGTH-1)
                            putcUart0('.');
                    }
                    snprintf(str, sizeof(str), ":%"PRIu16, sockets[socketNumber].remotePort);
                    putsUart0(str);
                    putcUart0('\n');
                }
            }
            if (strcmp(token, "mqttConnect") == 0)
            {
                gf_mqtt_connect_default = 0;
                nargs = 0;
                mqttFlags = 0;
                token = strtok(NULL, " ");
                removeDelimiters(token, mqttMessages[nargs++], "");
                token = strtok(NULL, " ");
                while(token != NULL)
                {
                    if(token[0] == '-')
                    {
                        switch(token[1])
                        {
                            case 'h':
                                token = strtok(NULL, " ");
                                gf_mqtt_connect_default = asciiToUint8(token);
                                if(sockets[gf_mqtt_connect_default].state != TCP_ESTABLISHED)
                                    gf_mqtt_connect_default = 0;
                                break;
                            case 'w':
                                mqttFlags |= MQTT_WILL;
                                token = strtok(NULL, " ");
                                removeDelimiters(token, mqttMessages[nargs++], "");
                                break;
                            case 'u':
                                mqttFlags |= MQTT_USERNAME;
                                //token = strtok(NULL, " ");
                                token = IO_USRNAME;
                                removeDelimiters(token, mqttMessages[nargs++], "");                            
                                break;
                            case 'p':
                                mqttFlags |= MQTT_PASSWORD;
                                //token = strtok(NULL, " ");
                                token = IO_KEY;
                                removeDelimiters(token, mqttMessages[nargs++], "");
                                break;
                            default:
                                break;
                        }
                    }
                    token = strtok(NULL, " ");
                }
                if(gf_mqtt_connect_default != 0)
                    return;
                uint8_t ip_address[4];

                if(!checkEmptyBrokerAddress && !checkRemoteBrokerAddress())
                    getIpMqttBrokerAddress(ip_address);
                else if(checkRemoteBrokerAddress())
                    getIpGatewayAddress(ip_address);
                else
                    getIpMqttBrokerAddress(ip_address);

                //getIpMqttBrokerAddress(mqtt_address);
                uint8_t socketNum = createSocket(PROTOCOL_TCP, sockets, MAX_SOCKETS, ip_address, MQTT_PORT);
                if(socketNum < MAX_SOCKETS)
                {
                    setTcpState(&(sockets[socketNum]), TCP_SYN_SENT);
                    restartTimer(arpRequestTimeoutCallback);
                    gf_arp_send_request = socketNum;
                    gf_mqtt_connect = socketNum;
                }
                else
                {
                    putsUart0("No sockets available\n");
                }
            }
            if (strcmp(token, "publish") == 0)
            {
                nargs = 0;
                mqttFlags = 0;
                gf_mqtt_publish = 0;
                token = strtok(NULL, " ");
                while(token != NULL)
                {
                    if(token[0] == '-')
                    {
                        switch(token[1])
                        {
                            case 'h': // Socket Number
                                token = strtok(NULL, " ");
                                gf_mqtt_publish = asciiToUint8(token);
                                break;
                            case 't': // Topic name
                                token = strtok(NULL, " ");
                                removeDelimiters(token, mqttMessages[nargs++], "");
                                break;
                            case 'm':   // Message
                                token = strtok(NULL, " ");
                                removeDelimiters(token, mqttMessages[nargs++], "");
                                break;
                            case 'q':   // QoS
                                token = strtok(NULL, " ");
                                mqttFlags = asciiToUint8(token);
                                break;
                            default:
                                break;
                        }
                    }
                    token = strtok(NULL, " ");
                }
            }
            if (strcmp(token, "publishUptime") == 0)
            {
                char str[32];
                sprintf(str, "%u""ms", getUptime());
                nargs = 0;
                mqttFlags = 0;
                gf_mqtt_publish = 0;
                token = strtok(NULL, " ");
                while(token != NULL)
                {
                    if(token[0] == '-')
                    {
                        switch(token[1])
                        {
                            case 'h': // Socket Number
                                token = strtok(NULL, " ");
                                gf_mqtt_publish = asciiToUint8(token);
                                break;
                            case 'q':   // QoS
                                token = strtok(NULL, " ");
                                mqttFlags = asciiToUint8(token);
                                break;
                            default:
                                break;
                        }
                    }
                    token = strtok(NULL, " ");
                }
                strcpy(mqttMessages[0], "Time");
                strcpy(mqttMessages[1], str);
                nargs = 2;
                
            }
            if (strcmp(token, "mqttDisconnect") == 0)
            {
                mqttFlags = 0;
                gf_mqtt_subscribe = 0;
                nargs = 0;
                token = strtok(NULL, " ");
                while(token != NULL)
                {
                    if(token[0] == '-')
                    {
                        switch(token[1])
                        {
                            case 'h': // Socket Number
                                token = strtok(NULL, " ");
                                gf_mqtt_disconnect = asciiToUint8(token);
                                break;
                            default:
                                break;
                        }
                    }
                    token = strtok(NULL, " ");
                }
            }
            if (strcmp(token, "showTopics") == 0)
            {
                uint8_t i;
                putsUart0("Subbed Topics:\n");
                for(i = 1; i < MAX_TOPICS; i++)
                {
                    if(topics[i].name[0] != '\0')
                    {
                        putsUart0(topics[i].name);
                        putcUart0('\n');
                    }
                }
            }
        }
    }
}

void processTransmission()
{
    uint8_t buffer[MAX_PACKET_SIZE];
    etherHeader *data = (etherHeader*) buffer;
    uint8_t local_ip[4];
    
    if (gf_send_ping)
    {
        pingTime = getUptime();
        sendPing(data, sockets[gf_send_ping]);
        gf_rx_ping = gf_send_ping;
        gf_send_ping = 0;
    }
    if (gf_arp_send_request)
    {
        getIpAddress(local_ip);
        sendArpRequest(data, local_ip, sockets[gf_arp_send_request].remoteIpAddress);
        gf_rx_arp = gf_arp_send_request;
        gf_arp_send_request = 0;
    }
    if (gf_tcp_send_syn)
    {
        sendTcpMessage(data, sockets[gf_tcp_send_syn], SYN, NULL, 0);
        restartTimer(tcpHandshakeTimeoutCallback);
        gf_tcp_rx_synack = gf_tcp_send_syn;
        gf_tcp_send_syn = 0;
    }
    if (gf_tcp_send_ack)
    {
        sendTcpMessage(data, sockets[gf_tcp_send_ack], ACK, NULL, 0);
        gf_tcp_send_ack = 0;
    }
    if (gf_mqtt_subscribe)
    {
        if(sockets[gf_mqtt_subscribe].state == TCP_ESTABLISHED)
        {
            // mqttSubscribe(data, sockets[gf_mqtt_subscribe], mqttFlags, &(mqttMessages[0]), nargs);
            sendMqttMessage(data, sockets[gf_mqtt_subscribe], SUBSCRIBE, mqttFlags, (void *)mqttMessages, MAX_CHARS, nargs);
        }
        gf_mqtt_subscribe = 0;
    }
    if (gf_mqtt_subscribe_caps && numOfSubCaps)
    {
        mqttFlags = 0;
        gf_mqtt_rx_suback = addTopic(subTopicQueue[numOfSubCaps - 1], topics, MAX_TOPICS);
        sendMqttMessage(data, sockets[gf_mqtt_subscribe_caps], SUBSCRIBE, mqttFlags, (void *)subTopicQueue[numOfSubCaps - 1], 30, 1);
        numOfSubCaps--;
    }
    if (gf_mqtt_subscribe_default)
    {
        static uint8_t j = 1;
        if(sockets[gf_mqtt_subscribe_default].state == TCP_ESTABLISHED)
        {   
            while(j < MQTT_DEFAULT_N_TOPICS + 1)
            {
                if(topics[j].name[0] == '\0')
                    j++;
                else
                    break;
            }
            if(j >= MQTT_DEFAULT_N_TOPICS + 1)
            {
                j = 1;
                gf_mqtt_subscribe_default = 0;
                return;
            }

            sendMqttMessage(data, sockets[gf_mqtt_subscribe_default], SUBSCRIBE, mqttFlags, (void *)(topics[j++].name), MAX_CHARS, 1);
            gf_mqtt_rx_suback_default = gf_mqtt_subscribe_default;
            gf_mqtt_subscribe_default = 0;
        }
    }
    if (gf_mqtt_unsubscribe)
    {
        if(sockets[gf_mqtt_unsubscribe].state == TCP_ESTABLISHED)
        {
            sendMqttMessage(data, sockets[gf_mqtt_unsubscribe], UNSUBSCRIBE, mqttFlags, (void *)mqttMessages, MAX_CHARS, nargs);
        }
        gf_mqtt_unsubscribe = 0;
    }
    if (gf_mqtt_publish)
    {
        if(sockets[gf_mqtt_publish].state == TCP_ESTABLISHED)
        {
            // mqttPublish(data, sockets[gf_mqtt_publish], mqttFlags, &(mqttMessages[0]), nargs);
            sendMqttMessage(data, sockets[gf_mqtt_publish], PUBLISH, mqttFlags, (void *)mqttMessages, MAX_CHARS, nargs);
        }
        gf_mqtt_publish = 0;
    }
    if (gf_tcp_send_fin)
    {
        sendTcpMessage(data, sockets[gf_tcp_send_fin], FIN, NULL, 0);
        gf_tcp_send_fin = 0;
    }
    if (gf_tcp_send_finack)
    {
        sendTcpMessage(data, sockets[gf_tcp_send_finack], (FIN | ACK), NULL, 0);
        gf_rx_lastAck = gf_tcp_send_finack;
        gf_tcp_send_finack = 0;
    }
    if (gf_disconnect)
    {
        if (sockets[gf_disconnect].state == TCP_CLOSE_WAIT)
        {
            sendTcpMessage(data, sockets[gf_disconnect], FIN, NULL, 0);
            gf_rx_lastAck = gf_disconnect;
        }
        if (sockets[gf_disconnect].state == TCP_FIN_WAIT_2)
        {
            sendTcpMessage(data, sockets[gf_disconnect], ACK, NULL, 0);
        }
        putsUart0("Socket closed\n");
        deleteSocket(&(sockets[gf_disconnect]));
        gf_disconnect = 0;
    }
    if (gf_mqtt_disconnect)
    {
        if(sockets[gf_mqtt_disconnect].state == TCP_ESTABLISHED)
        {
            sendMqttMessage(data, sockets[gf_mqtt_disconnect], DISCONNECT, mqttFlags, (void *)mqttMessages, MAX_CHARS, nargs);
        }
        gf_mqtt_disconnect = 0;
    }
    if (gf_mqtt_connect)
    {
        if(sockets[gf_mqtt_connect].state == TCP_ESTABLISHED)
        {
            putsUart0("Sending mqtt connect\n");
            // mqttConnect(data, sockets[gf_mqtt_connect], mqttFlags, &(mqttMessages[0]), nargs);
            sendMqttMessage(data, sockets[gf_mqtt_connect], CONNECT, mqttFlags, (void *)mqttMessages, MAX_CHARS, nargs);
            gf_mqtt_rx_connack = gf_mqtt_connect;
            gf_mqtt_connect = 0;
        }
    }
    if(gf_mqtt_device_pub)
    {
        char publishMsg[2][30];
        bool isValidRead = readPubMsgBuffer(&publishMsg);
        if(isValidRead)
        {
            sendMqttMessage(data, sockets[gf_mqtt_device_pub], PUBLISH, mqttFlags, (void *)publishMsg, 30, 2);
            if(isPubMsgBufferEmpty())
                gf_mqtt_device_pub = 0;
        }
        else
            gf_mqtt_device_pub = 0;   

    }
    if (gf_mqtt_connect_default)
    {
        if(sockets[gf_mqtt_connect_default].state == TCP_ESTABLISHED)
        {
            putsUart0("Sending mqtt connect\n");
            if(checkRemoteBrokerAddress())
            {
                mqttFlags = 192;
                //mqttMessages[0] = "-h";
                strncpy(mqttMessages[0], "-h", 2);
                //mqttMessages[1] = IO_USRNAME;
                //mqttMessages[2] = IO_KEY;
                strncpy(mqttMessages[1], IO_USRNAME, strlen(IO_USRNAME));
                strncpy(mqttMessages[2], IO_KEY, strlen(IO_KEY));

                sendMqttMessage(data, sockets[gf_mqtt_connect_default], CONNECT, mqttFlags, (void *)mqttMessages, MAX_CHARS, 3);
            }
            else
            // mqttConnect(data, sockets[gf_mqtt_connect], mqttFlags, &(mqttMessages[0]), nargs);
                sendMqttMessage(data, sockets[gf_mqtt_connect_default], CONNECT, mqttFlags, (void *)MQTT_DEFAULT_CONFIG, MAX_CHARS, MQTT_DEFAULT_CONFIG_NARGS);
            gf_mqtt_rx_connack = gf_mqtt_connect_default;
            gf_mqtt_connect_default = 0;
        }
    }
}

connectMqttBroker()
{
    uint8_t ip_address[4];

    if(!checkEmptyBrokerAddress() && !checkRemoteBrokerAddress())
        getIpMqttBrokerAddress(ip_address);
    else if(checkRemoteBrokerAddress())
        getIpGatewayAddress(ip_address);
    else
        getIpMqttBrokerAddress(ip_address);

    //getIpMqttBrokerAddress(mqtt_address);
    uint8_t socketNum = createSocket(PROTOCOL_TCP, sockets, MAX_SOCKETS, ip_address, MQTT_PORT);
    setTcpState(&(sockets[socketNum]), TCP_SYN_SENT);
    restartTimer(arpRequestTimeoutCallback);
    gf_arp_send_request = socketNum;
    gf_mqtt_connect_default = socketNum;
}
//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    char str[32];
    uint8_t *udpData;
    uint8_t *tcpData; 
    uint8_t *mqttMessage;
    uint16_t topicLength;
    uint8_t *mqttData;
    uint8_t buffer[MAX_PACKET_SIZE];
    etherHeader *data = (etherHeader*) buffer;
    socket s;

    // Init controller
    initHw();

    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);
    initWireless();
    // Init timer
    initTimer();

    initDefaultTimers();

    // Init ethernet interface (eth0)
    // Use the value x from the spreadsheet
    putsUart0("\nStarting eth0\n");
    initEther(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    setEtherMacAddress(2, 3, 4, 5, 6, 69);

    // Init EEPROM
    
    readConfiguration();

    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);
    uint8_t local_ip[4];
    uint8_t local_mac[6];
    getIpAddress(local_ip);
    getEtherMacAddress(local_mac);

    connectMqttBroker();

    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    while (true)
    {
        // Put terminal processing here
        processShell();

        processTransmission();

        processWireless();

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
            
            // Handle ARP response
            if(isArpResponse(data))
            {
                if(gf_rx_arp)
                {
                    setSocketHwAddress(data, &(sockets[gf_rx_arp]));
                    switch(sockets[gf_rx_arp].id)
                    {
                        case PROTOCOL_TCP:
                            gf_tcp_send_syn = gf_rx_arp;
                            restartTimer(tcpHandshakeTimeoutCallback);
                            break;
                        case PROTOCOL_ICMP:
                            gf_send_ping = gf_rx_arp;
                            break;
                    }
                    gf_rx_arp = 0;
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
                        putsUart0("Pinged\n");
                    }

                    // Handle ICMP ping response
                    if(isPingResponse(data))
                    {
                        putsUart0("Time = ");
                        sprintf(str, "%u""ms", getUptime() - pingTime);
                        putsUart0(str);
                        putcUart0('\n');
                        deleteSocket(&(sockets[gf_rx_ping]));
                        gf_rx_ping = 0;
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
                        uint8_t socketNumber = isTcpSocketConnected(data, sockets, MAX_SOCKETS);
                        socket *s = &(sockets[socketNumber]);
                        // Invalid socket
                        if(socketNumber >= MAX_SOCKETS)
                            continue;
                        
                        tcpData = getTcpData(data, s);
                        uint16_t tcpFlags = getTcpFlags(data);
                        switch(s->state)
                        {
                            case TCP_SYN_SENT:
                                switch(tcpFlags)
                                {
                                    case (SYN | ACK):
                                        gf_tcp_send_ack = socketNumber;
                                        gf_tcp_rx_synack = 0;
                                        setTcpState(s, TCP_ESTABLISHED);
                                        break;
                                    case (RST | ACK):
                                        deleteSocket(&(sockets[gf_tcp_rx_synack]));
                                        putsUart0("TCP Connection refused\nPort not open\n");
                                        gf_tcp_rx_synack = 0;
                                        break;
                                    default:
                                        break;
                                }
                                break;
                            case TCP_ESTABLISHED:
                                switch(tcpFlags)
                                {
                                    case (PSH | ACK):
                                        gf_tcp_send_ack = socketNumber;
                                        if(isMqtt(data))
                                        {
                                            uint8_t mqttFlags = getMqttFlags(tcpData);
                                            mqttData = getMqttData(tcpData);
                                            switch(mqttFlags)
                                            {
                                                case CONNACK:
                                                    if(gf_mqtt_rx_connack)
                                                    {
                                                        if(mqttData[1] == 0)
                                                        {
                                                            putsUart0("Connected\n");
                                                            //gf_mqtt_subscribe_default = gf_mqtt_rx_connack;
                                                            setMqttBrokerSocketIndex(gf_mqtt_rx_connack);
                                                        }
                                                        else
                                                        {
                                                            putsUart0("MQTT connection refused\nReturned: ");
                                                            putcUart0(mqttData[1] + '0');
                                                            putsUart0("\n");
                                                            deleteSocket(&(sockets[gf_mqtt_rx_connack]));
                                                        }
                                                        gf_mqtt_rx_connack = 0;
                                                    }
                                                    break;
                                                case SUBACK:
                                                    if(gf_mqtt_rx_suback)
                                                    {
                                                        putsUart0("Subscribed to ");
                                                        putsUart0(topics[gf_mqtt_rx_suback].name);
                                                        putcUart0('\n');
                                                        gf_mqtt_rx_suback = 0;
                                                    }
                                                    else if(gf_mqtt_rx_suback_default)
                                                    {
                                                        putsUart0("Subscribed to default topics\n");
                                                        gf_mqtt_subscribe_default = gf_mqtt_rx_suback_default;
                                                        gf_mqtt_rx_suback_default = 0;
                                                    }
                                                    break;
                                                case UNSUBACK:
                                                    if(gf_mqtt_rx_unsuback)
                                                    {
                                                        putsUart0("Unsubscribed from ");
                                                        putsUart0(topics[gf_mqtt_rx_unsuback].name);
                                                        putcUart0('\n');
                                                        removeTopic(gf_mqtt_rx_unsuback, topics);
                                                        gf_mqtt_rx_unsuback = 0;
                                                    }
                                                    break;
                                                case PUBLISH:
                                                    // Extract topic information and msg from publish
                                                    topicLength = (mqttData[0] << 8) + mqttData[1];
                                                    char shortTopicName[5];
                                                    // 0012 uta_iot/feed/mtrsp
                                                    // 0 1  0123456789ABC
                                                    strncpy(shortTopicName, (char *)mqttData[2 + topicLength - 5], 5);
                                                    uint16_t msgLength = getMqttMessageLength(tcpData);
                                                    mqttMessage = getMqttMessage(tcpData);
                                                    pushMessage pshMsg;
                                                    strncpy(pshMsg.topicName, shortTopicName, 5);
                                                    strncpy(pshMsg.topicMessage, (char *)mqttMessage, msgLength);

                                                    MQTTBinding binding[3];
//                                                    strncpy(binding[0].devCaps, shortTopicName, 5);
                                                    MQTTBinding *isDevicePresent = mqtt_binding_table_get((MQTTBinding **)&binding, 3, pshMsg.topicName);

                                                    if(isDevicePresent != NULL)
                                                    {
                                                        bool isOverflow = queuePushMsg(&pshMsg, (binding[0].client_id[6]) - '0');
                                                        if(isOverflow)
                                                            putsUart0("Push Message Buffer overload\n");

                                                    }                                                    
                                                break;
                                            }
                                        }
                                        break;
                                    case (FIN | ACK):
                                        gf_tcp_send_ack = 0;
                                        gf_tcp_send_finack = socketNumber;
                                        gf_close_socket = socketNumber;
                                        setTcpState(s, TCP_CLOSE_WAIT);
                                        // restartTimer(closeSocketCallback);       
                                        break;
                                    case (RST | ACK):
                                        resetSocket(s);
                                        setTcpState(s, TCP_SYN_SENT);
                                        putsUart0("Reset received\n");
                                        gf_tcp_send_syn = socketNumber;
                                        if(isMqttSocket(s))
                                            gf_mqtt_connect_default = socketNumber;
                                        break;
                                    case ACK:
                                        break;
                                    default:
                                        gf_tcp_send_ack = socketNumber;
                                        break;
                                }
                                break;
                            case TCP_FIN_WAIT_1:
                                switch(tcpFlags)
                                {
                                    case ACK:
                                        setTcpState(s, TCP_FIN_WAIT_2);
                                        break;
                                    default:
                                }
                            case TCP_FIN_WAIT_2:
                                switch(tcpFlags)
                                {
                                    case FIN:
                                        setTcpState(s, TCP_TIME_WAIT);
                                        restartTimer(closeSocketCallback);
                                        gf_close_socket = socketNumber;
                                        break;
                                    default:
                                }
                            case TCP_CLOSE_WAIT:
                                switch(tcpFlags)
                                {
                                    case ACK:
                                        if(gf_rx_lastAck)
                                        {
                                            deleteSocket(&(sockets[gf_rx_lastAck]));
                                            putsUart0("Socket closed");
                                            gf_rx_lastAck = 0;
                                        }
                                        break;
                                    default:
                                }
                            
                            default:
                                    
                        }
                    }
                }
            }
        }
    }
}

