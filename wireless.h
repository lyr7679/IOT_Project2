#include <stdio.h>
#include <inttypes.h>

#ifndef WIRELESS_H_
#define WIRELESS_H_

// Message types and params
#define NRF_SYNC        (0u)
#define NRF_DATA        (1)
#define NRF_JOIN        (2)

#define NO_MSGS         (10)
#define MSG_LEN         (25)
#define DEF_SYNC_TIME   (5)         // Default Sync message time

//DATA transmission
#define DATA_MAX_SIZE   (1500u)
#define META_DATA_SIZE  (2+1+2+4+1)   //startcode + slot + remlen + devBitNum+  checksum

//EEPROM
#define NO_OF_DEV_IN_BRIDGE     (10u)   /*1 word */
#define DEV1_NO_START           (14u)
#define DEV1_MAC_START          (15u)
#define DEV2_NO_START           (19u)
#define DEV2_MAC_START          (20u)

//NRF Register addresses
#define CONFIG      0x00
#define EN_AA       0x01
#define EN_RXADDR   0x02
#define SETUP_AW    0x03
#define SETUP_RETR  0x04
#define RF_CH       0x05
#define RF_SETUP    0x06
#define STATUS      0x07
#define OBSERVE_TX  0x08
#define RX_ADDR_P0  0x0A            // rx pipe address. add offsets for following pipes
#define TX_ADDR     0x10
#define RX_PW_P0    0x11            // number of bytes allowed in rx pipe 0. Add offsets for following pipes
#define FIFO_STATUS 0x17
#define DYNPD       0x1C            // enable dynamic payload for diff tx pipes
#define FEATURE     0x1D

//SPI Commands
#define R_REGISTER          (0x00)
#define W_REGISTER          (0x20)
#define R_RX_PAYLOAD        (0x61)
#define W_TX_PAYLOAD        (0xA0)
#define W_TX_PAYLOAD_NO_ACK (0xB0)
#define FLUSH_TX            (0xE1)
#define FLUSH_RX            (0xE2)
#define R_RX_PL_WID         (0x60)


//Wireless PINS
#define CE_GPIO         PORTB,1
#define JOIN_BUTTON     PORTF,4
#define SYNC_LED        PORTF,1
#define JOIN_LED        PORTF,2
#define PING_LED        PORTF,3


//Slot Management Macros

#define GUARD_TIMER             (500u)   // 10 milliseconds
#define TX_RX_DELAY             (200u)  // used by waitmicroseconds
#define TX_RX_DELAY_SLOT        (0.2)   //milliseconds
#define _32BYTE_PACKETS         (47u)   // 1500/32 , 32 byte packets
#define DEF_SLOT_WIDTH          (50u)

#define SYNC_SLOT   (0)
#define DL_SLOT     (2) + GUARD_TIMER + SYNC_SLOT
#define FACK_SLOT   DEF_SLOT_WIDTH +(TX_RX_DELAY_SLOT*_32BYTE_PACKETS) + DL_SLOT+ GUARD_TIMER
#define ACCESS_SLOT (2) + FACK_SLOT + GUARD_TIMER
#define UL0_SLOT    DEF_SLOT_WIDTH +(TX_RX_DELAY_SLOT*_32BYTE_PACKETS) + ACCESS_SLOT + GUARD_TIMER
#define UL1_SLOT    DEF_SLOT_WIDTH +(TX_RX_DELAY_SLOT*_32BYTE_PACKETS) + UL0_SLOT + GUARD_TIMER

// Packet Types
//*************CODE ADDED by Velu Manohar***************

#define PING_REQUEST 0x0
#define PING_RESPONSE 0x1
#define PUSH 0x2
#define DEVCAPS_REQUEST 0x3
#define DEVCAPS_RESPONSE 0x4
#define WEB_SERVER 0x5
// Struct for wireless packet
#define MAX_WIRELESS_PACKET_SIZE 22


// 1 byte
typedef struct _wirelessPacket
{
    uint8_t packetType;             // 1 byte
    uint8_t data[0];
} wirelessPacket;

// 6 bytes
// capStruct is a part of deviceCaps, can not exist without deviceCaps
typedef struct _description
{
    uint8_t inputOrOutput;          // 1 byte
    char capDescription[5];         // 5 bytes
} description;

// 20 bytes
typedef struct _deviceCaps
{
    uint8_t deviceNum;              // 1 byte
    uint8_t numOfCaps;              // 1 byte
    description caps[3];              // 18 bytes
} deviceCaps;

// 21 bytes
typedef struct _pushMessage
{
    // uint8_t topicName;              // 1 byte
    // uint8_t remainingLength;        // 1 byte
    // char data[19];                  // 19 bytes
    char topicName[5];
    char topicMessage[16];
} pushMessage;



//******************************************************

//Extern variables
uint8_t gf_mqtt_subscribe_caps;
uint8_t numOfSubCaps;
uint8_t gf_mqtt_device_pub;

char subTopicQueue[3][30];

#define MAX_PUB_MSG_QUEUE_SIZE 10
char publishMsgQueue[MAX_PUB_MSG_QUEUE_SIZE][];
uint8_t pubWrPtr = 0;
uint8_t pubRdPtr = 0;

//-------------------------------------------------------
// Declare external variables for nrfSyncEnabled, nrfJoinEnabled and nrfJoinEnabled_BR

//see enableSync_BR()
extern bool nrfSyncEnabled;     // This is made true on the BRIDGE device and is used as a unique identifier of the bridge device

//see enableJoin_DEV()
extern bool nrfJoinEnabled;     // This is made true when the Join button is pressed on the Device and is a Device ONLY flag

//see enableJoin_BR()
extern bool nrfJoinEnabled_BR;  // This is made true when the Join button is pressed on the Bridge and is a Bridge ONLY flag

//-------------------------------------------------------
// Declare external variables for syncMsg, startCode, myMac

// see nrf24l0TxSync()
extern uint8_t syncMsg[7];      // This contains the Sync message and the 2 bytes of Sync Frame count that are sent in the SYNC SLOT
extern uint8_t startCode[2];    // Start code is sent in all the frames except for the SYNC message
extern uint8_t myMac[6];        // 6 bytes of local MyMAC address. DO NOT change this once a network has been joined

//-------------------------------------------------------
// Declare external variables for slotNo, syncFrameCount, packetLength, allocatedDevNum

extern uint8_t slotNo;          // Slot numbers are sent for each slot , see the frame format below for reference
extern uint16_t syncFrameCount; // Frame count is used to count the number of Sync messages sent
extern uint16_t packetLength;   // Parse the data in RxPacket array and get the Packetlength (like remaining length in MQTT)

// See eepromSetGetDevInfo_BR() and ( nrf24l0RxMsg() >> isJoinPacket block )
extern uint8_t allocatedDevNum; // Bridge keeps track of allocated device numbers when Mac address is sent in Join Req

//-------------------------------------------------------
// Declare external variables for Rxpacket, Rx_index
extern uint8_t Rxpacket[DATA_MAX_SIZE]; // This stores the received packets either on Bridge and Device sides and is cleared after packet is processed by callback functions

//see parsenrf24l01DataPacket() and nrf24l0RxMsg()
extern uint16_t Rx_index;               // read index of the Rxpacket buffer

//see getnrf24l01DataPacket()
extern uint16_t Rx_wrIndex;             // Write index of the Rxpacket buffer. This is incremented in the

//see getnrf24l01DataPacket()
extern uint8_t payloadlength;           // Payloadlength is the dynamic length extracted from the R_RX_PL_WID of NRF module


//-------------------------------------------------------
// Declare external variables for isSync, isMyPacket, msgAcked, isDevPacket, isBridgePacket, isJoinPacket
// Used by APPLICATION LAYER

extern bool isSync;             // Flag set to true when a Sync message is received
extern bool isMyPacket;         // Flag to check if the packet is meant for my Device
extern bool msgAcked;           // Flag to check whether my message was acked or not
extern bool isDevPacket;        // Flag to check whether a packet was meant for the Device in Dl slot
extern bool isJoinResp_dev;     // Flag to distinguish between normal DL packets and JOIN RESP pDL packet
extern bool isBridgePacket;     // Flag to check whethet a packet is meant for the Bridge and is made true in Uplink slots
extern bool isJoinPacket;       // Flag to check if the Bridge received a Join req in the ACCESS slot


//-------------------------------------------------------
// Declare external variables for downlinkSlotStart_br, fastackSlotStart_br, acessSlotStart_br, uplinkSlot_br, sendJoinResponse_BR

//See the // Timer Internal functions >> //BR timer callback functions below

extern bool downlinkSlotStart_br;   // Flag that is true only in the DL slot and used by Bridge ONLY
extern bool fastackSlotStart_br;    // Flag that is true only in the FACK slot and used by Bridge ONLY
extern bool acessSlotStart_br;      // Flag that is true only in the ACCESS slot and used by Bridge ONLY
extern bool uplinkSlot_br;          // Flag that is true only in the UPLINK 0 slot and used by Bridge ONLY
extern bool sendJoinResponse_BR;    // Flag that is made true when a proper JOIN REQ has been received and PARSED and JOIN RESP is meant to be sent in the next DL slot

//-------------------------------------------------------
// Declare external variables for uplinkSlot_dev, acessSlotStart_dev

// See the // Timer Internal functions >>//Device timer callback functions below

extern bool acessSlotStart_dev;     // Flag that is true only in the ACCESS slot and used by DEVICE only
extern bool uplinkSlot_dev;         // Flag that is true only in the UPLINK slot of the Device and used by DEVICE ONLY

//-------------------------------------------------------
// Declare external variable for txTimeStatus
extern bool txTimeStatus;   // Flag is made true in the different Timing functions , ONLY when this is true can data be transmitted


//FRAME FORMAT

//SYNC - DL SLOT - FACK SLOT - ACCESS SLOT - ULO - UL1------

//Data foramt

//SYNC - SYNC Msg - 5 bytes + 2 bytes SyncFrameCount

//DL Slot msg  = Start code (2bytes) + Slot no(1 byte) + remaining Length(2 bytes) + DeviceBitNum(4bytes) + Payload + Checksum(1 byte)

// Uplink slots are similar to DL slot format

// JOIN REQ format - Start Code(4 bytes) + Slot Number(1 byte) + (Remaining length(2 bytes)|0x8000) + Mac Address(6 bytes) + Checksum(1 bytes)

// JOIN RESPONSE FORMAT - Start Code + Slot Number + (Remaining length|0x8000) + Allocated Device Number(1 byte) + Checksum
//**********************************************************************
//Subroutines
//######################################################################

typedef void (*callback)(uint8_t *data, uint16_t size);

//----------------------------------------------------
/* Wireless Global common subroutines */
//----------------------------------------------------
// Initialize the NRF module
void nrf24l0Init();

// Transmit message with an uint8_t* pointer and the data size
//where (devBitNum = 1<<deviceNum)
//This is also used for sending ACKs with multicast or sending data to multiple devices
// Device sends data to Bridge whose dev bit is 0xFFFFFFF
int nrf24l0TxMsg(uint8_t* data, uint16_t size, uint32_t devBitNum);


// Check for available messages
//Get the message and store in RxPacket
//parse the data packet and determine what to do with it  by setting off flags
//Based on the flags that are set off perform respective actions
//Reset the Rxindex and Rx_wrindex to make the RxPacket a circular buffer
void nrf24l0RxMsg(callback fn);


//----------------------------------------------------
/* Wireless Global Device subroutines */
//----------------------------------------------------
void enableJoin_DEV();                          // Enable for sending a Join req in Access slot on Pushbutton press
void eepromSetDevInfo_DEV(uint8_t deviceNum);   // Set the Device number received as Join response in EEPROM
uint8_t eepromGetDevInfo_DEV();                 // Get my device number stored in EEPROM


//----------------------------------------------------
/* Wireless Global Bridge subroutines */
//----------------------------------------------------
// Send a Sync Message. This function determines what a Bridge device is. ONLY call this function on Bridge Devices
void enableSync_BR();

// Allocate a device num for new device OR Get device number if already available
uint8_t eepromSetGetDevInfo_BR(uint8_t *data);


//----------------------------------------------------
// Timer Internal functions
//----------------------------------------------------

//BR timer callback functions
void syncSlot_BR();
void downlinkSlot_BR();
void fastackSlot_BR();
void joinAccessSlot_BR();
void uplinkSlot_BR();

//Device timer callback functions
void joinAccessSlot_DEV();
void uplinkSlot_DEV();

//Timing handler functions
void TimerHandler_BR();
void syncRxDevSlot();

//----------------------------------------------------
/*Wireless Internal subroutines*/
//----------------------------------------------------

//----------------------------------------------------
//Initialize Hardware
void nrf24l0ChipInit(); // Configure registers in NRF module. Called in nrf24l0Init()

//----------------------------------------------------
//SPI Bus functions
void enableNrfCs(void);
void disableNrfCs(void);
void writeNrfReg(uint8_t reg, uint8_t data);
void writeNrfData(uint8_t data);
void readNrfReg(uint8_t reg, uint8_t *data);
void readNrfData(uint8_t *data);
void nrf24l0PowerUp();

//----------------------------------------------------
//Slot management

// Used for determining the period of sending SYNC Messages
// See syncRxDevSlot() and nrf24l0TxSync()
uint32_t getMySlot(uint8_t devno);


//----------------------------------------------------
// Tx/ Rx functions
//----------------------------------------------------
void putNrf24l0DataPacket(uint8_t *data, uint16_t size);    // Physical layer to transmit packets
void getnrf24l01DataPacket();                               // Physical layer to get packets and fill the RxPacket buffer
void parsenrf24l01DataPacket();                             // Parse the received packet and set off flags based on what to do next
bool nrf24l0TxStatus();                                     // Check the status of the TX FIFO of the NRF module
bool nrf24l0RxStatus();                                     // Check the status of the RX FIFO of the NRF module
bool isNrf24l0DataAvailable(void);                          // Check to see if Data is available in the SPI bus for reception

//----------------------------------------------------
//Callback functions

//See nrf24l0RxMsg() and main()
// Pop and print the received data in Putty
// APPLICATION layer should use this function and modify it
callback dataReceived(uint8_t *data, uint16_t size);

//----------------------------------------------------
//Checksum functions
uint8_t nrf24l0GetChecksum(uint8_t *ptr, uint16_t size);


//----------------------------------------------------
//Tx Msg handlers for SYNC, JOINREQ, JOINRESP
void nrf24l0TxSync();           // Send Syncs periodically
void nrf24l0TxJoinReq_DEV();    // Send Join Request. Use for DEVICES ONLY
void nrf24l0TxJoinResp_BR();    // Send JOIN RESPONSE to a valid Join request. Use for BRIDGE ONLY


#endif /* WIRELESS_H_ */
