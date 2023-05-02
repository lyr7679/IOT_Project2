
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "tm4c123gh6pm.h"
#include "clock.h"
#include "wait.h"
#include "gpio.h"
#include "timer_wireless.h"
#include "spi1.h"
#include "eeprom.h"
#include "uart0.h"
#include "wireless.h"
#include "hashTable.h"
#include "mqtt.h" // Added for getMqttBrokerAddress and setMqttBrokerAddress
#include "i2c0.h"

bool isBridge = false;
bool nrfSyncEnabled = false;
bool nrfJoinEnabled = false;
bool nrfJoinEnabled_BR = false;
uint8_t nrfJoinCount;

uint8_t syncMsg[7] = {0x55,0xAA,0x55,0xAA,0x55};
uint8_t startCode[2] = {0xFE, 0xFE};
uint8_t myMac[6] = {75,78,1,2,3,56};

uint8_t slotNo  = 0;
uint16_t syncFrameCount = 0;
uint16_t packetLength =0;

uint8_t allocatedDevNum =0;
uint32_t lastMsgDevNo_br =0;

uint8_t Rxpacket[DATA_MAX_SIZE] = {0};
uint16_t Rx_index =0;                               // read index
uint16_t Rx_wrIndex =0;                             // write index
uint8_t payloadlength =32;


//*************Bridge Data structures***************
pushMessageDevNum pushMsgBuffer[MAX_PSH_MSG_BUFFER_SIZE];
uint8_t pushWrPtr = 0;
uint8_t pushRdPtr = 0;

char pubMsgBuffer[MAX_PUB_MSG_BUFFER_SIZE][2][30]; // each topic/msg can be 30 characters long with 2 arguments (topic,msg)
uint8_t pubWrPtr = 0;
uint8_t pubRdPtr = 0;

bool webserverConnectionStatus = false;
uint8_t webserverDeviceNumber = 0xFF;

uint8_t shellDestinationDevNumber = 0;
char longTopic[30] = "uta_iot/feeds/";
//*******************************************************

//*************CTX RX Structs and flags***************
uint8_t buffer[MAX_WIRELESS_PACKET_SIZE] = {0};
bool dataReceivedFlag = false;
//BR flags
bool sendPingRequestFlag = false;
bool sendDevCapsRequestFlag = false;
//Device Flags
bool sendPingResponseFlag = false;
bool sendDevCapsResponseFlag = false;
//Flags Shared by BR and Device
bool sendPushFlag = false;
#define MAX_PACKCET_SIZE 22
//*******************************************************



//Packet Type flags
bool isSync = false;
bool isMyPacket = false;
bool msgAcked = false;
bool isDevPacket = false;
bool isJoinResp_dev = false;
bool isBridgePacket = false;
bool isJoinPacket = false;

//Timer flags
bool downlinkSlotStart_br  = false;
bool fastackSlotStart_br = false;
bool acessSlotStart_br = false;
bool uplinkSlot_br = false;
bool sendJoinResponse_BR = false;

bool timersStarted_br = false;

bool acessSlotStart_dev = false;
bool uplinkSlot_dev = false;


//Transmit status flags
bool txTimeStatus = false; // Its okay for you to transmit
bool appSendFlag = false; // Flag for the application layer to send the data. Make it true to send data.
//----------------------------------------------------
//Debug
//----------------------------------------------------

uint8_t fifoStatus = 0;
bool debugMsg = false;

//----------------------------------------------------
// Initialize hardware
//----------------------------------------------------



// --------------------------------------------------------
// TEST CODE to check if structs are correct, needs to be deleted and changed with proper shell commands
// --------------------------------------------------------
#define MAX_CHARS 80
uint8_t count1 = 0;
char strInput[MAX_CHARS+1];
char* token;
void processShell1(wirelessPacket* message)
{
    bool end;
    char c;
    uint8_t i;
    //    uint32_t* p32;

    if (kbhitUart0())
    {
        c = getcUart0();

        end = (c == 13) || (count1 == MAX_CHARS);
        if (!end)
        {
            if ((c == 8 || c == 127) && count1 > 0)
                count1--;
            if (c >= ' ' && c < 127)
                strInput[count1++] = c;
        }
        else
        {
            strInput[count1] = '\0';
            count1 = 0;
            token = strtok(strInput, " ");
            if (strcmp(token, "push") == 0)
            {
                message = (wirelessPacket*)buffer;
                message->packetType = PUSH;
                sendPushFlag = true;
                appSendFlag = true;
            }
            if (strcmp(token, "ping") == 0)
            {
                message = (wirelessPacket*)buffer;
                message->packetType = PING_REQUEST;
                sendPingRequestFlag = true;
                appSendFlag = true;
            }

            if (strcmp(token, "devCaps") == 0)
            {
                message = (wirelessPacket*)buffer;
                message->packetType = DEVCAPS_REQUEST;
                sendDevCapsRequestFlag = true;
                appSendFlag = true;
            }

            if (strcmp(token, "reboot") == 0)
            {
                NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
            }

            if (strcmp(token, "debug") == 0)
            {
                token = strtok(NULL, " ");
                if(strcmp(token, "on") == 0){
                    debugMsg = true;
                }
                else if(strcmp(token, "off") == 0){
                    debugMsg = false;
                }
                else{
                    debugMsg = false;
                }
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

            if (strcmp(token, "help") == 0)
            {
                putsUart0("push\n");
                putsUart0("ping\n");
                putsUart0("devCaps\n");
                putsUart0("reboot\n");
                putsUart0("debug on/off\n");
                putsUart0("reset inteeprom\n");
            }
        }
    }
}


//////////////////////////////////////////////////////////////

void sendDevCap(void)
{
    wirelessPacket* message = (wirelessPacket*)buffer;
    message->packetType = DEVCAPS_REQUEST;
    sendDevCapsRequestFlag = true;
    appSendFlag = true;
}
void sendDevPush(void)
{
    wirelessPacket* message = (wirelessPacket*)buffer;
    message->packetType = PUSH;
    sendPushFlag = true;
    appSendFlag = true;
}

void sendWirelessPing(void)
{
    
    wirelessPacket* message = (wirelessPacket*)buffer;
    message->packetType = PING_REQUEST;
    sendPingRequestFlag = true;
    appSendFlag = true;
}

void nrf24l0Init()
{

    // Assume that the clocks are initialized already

    enablePort(PORTF);
    _delay_cycles(3);
    selectPinPushPullOutput(SYNC_LED);                      //RED LED
    selectPinPushPullOutput(JOIN_LED);                      //BLUE LED
    selectPinDigitalInput(JOIN_BUTTON);
    enablePinPullup(JOIN_BUTTON);

    enablePort(PORTB);                                      // used for Chip enable
    selectPinPushPullOutput(CE_GPIO);

    setPinValue(CE_GPIO,1);                                 // init always in Rx mode so that isnrfDataAvailable checks for available data
    waitMicrosecond(TX_RX_DELAY);

    // SPI1 Initialization
    initSpi1(USE_SSI_RX);
    setSpi1BaudRate(1e6, 40e6); // 1Mhz baud
    setSpi1Mode(0,0);
    selectPinPushPullOutput(SSI1FSS);

    //Initialize nrf chip
    nrf24l0ChipInit();

    initTimer_ms();

    setPinValue(JOIN_LED,1);                               // Init Flash
    waitMicrosecond(100000);
    setPinValue(JOIN_LED, 0);
    waitMicrosecond(100000);

}

void nrf24l0ChipInit()
{
    uint8_t data =0;

    //configure the module
    setPinValue(CE_GPIO, 0);
    disableNrfCs();

    waitMicrosecond(5000);                                  // Wait for the nrf module to settle

    data = 0x0C;                                            // Interrupts off,Enable CRC, 2 byte CRC
    writeNrfReg(W_REGISTER|CONFIG,data );

    data = 0x00;                                            // Turn off retransmission   /*Check for using retransmission later*/
    writeNrfReg(W_REGISTER|SETUP_RETR,data );

    data = 0x03;                                            // RF data rate to 1Mbps / 1 Mhz and power is 0 dbm max power
    writeNrfReg(W_REGISTER|RF_SETUP,data );

    data = 0x05;                                            // enable dynamic payloads, enable Tx_payload, NO_ACK
    writeNrfReg(W_REGISTER|FEATURE,data );

    data = 0x3F;                                            // Configure dynamic payload lengths for all data pipes
    writeNrfReg(W_REGISTER|DYNPD,data );

    data = 0x70;
    writeNrfReg(W_REGISTER|STATUS, data);

    data = 76;                                              // Set RF channel to be 2400 + 76(data) mhz
    writeNrfReg(W_REGISTER|RF_CH,data );

    data = 0x00;                                            // Turn off auto ACKs
    writeNrfReg(W_REGISTER|EN_AA,data );

    enableNrfCs();                                          // Flush Tx FIFO after sending a SYNC
    writeSpi1Data(W_REGISTER|FLUSH_TX);
    readSpi1Data();
    disableNrfCs();

    enableNrfCs();                                          // Flush Tx FIFO after sending a SYNC
    writeSpi1Data(W_REGISTER|FLUSH_RX);
    readSpi1Data();
    disableNrfCs();

    nrf24l0PowerUp();

}
//----------------------------------------------------
// Timer functions
//----------------------------------------------------

//----------------------------------------------------
//Device slots

void joinAccessSlot_DEV(){
    txTimeStatus = true;
    slotNo = 3;
    acessSlotStart_dev = true;

    //debug
    if(debugMsg){
        putsUart0("JOINstart-D\n"); }
}
void uplinkSlot_DEV(){
    txTimeStatus = true;
    slotNo = eepromGetDevInfo_DEV() + 4;                            // Uplink slot start
    acessSlotStart_dev = false;
    uplinkSlot_dev = true;                                          // Turn off after transmission
    //    appSendFlag = true;

    //debug
    if(debugMsg){
        putsUart0("UPLINKstart-D\n"); }
}

void syncRxDevSlot(){
    // Check if Sync was received by the device and wait for respective slots
    if(nrfJoinEnabled){
        stopTimer_ms(joinAccessSlot_DEV);
        startOneshotTimer_ms(joinAccessSlot_DEV, ACCESS_SLOT);
    }
    else{
        uint32_t myUplinkSlot = getMySlot(eepromGetDevInfo_DEV()); // Get the time you have to wait for your slotNo
        //myUplinkSlot += GUARD_TIMER;

        char str[30];
        snprintf(str, sizeof(str),"uplink slot time : %lu\n", myUplinkSlot);
        putsUart0(str);
        uint8_t mydev = eepromGetDevInfo_DEV();
        snprintf(str, sizeof(str),"My dev no : %u\n", (1<<mydev));
        putsUart0(str);

        stopTimer_ms(uplinkSlot_DEV);
        startOneshotTimer_ms(uplinkSlot_DEV, myUplinkSlot);
    }
    /*reset all the checks/data */
    isSync = false;
    uplinkSlot_dev = false;                         // turned on by timer in its slot
    acessSlotStart_dev = false;                     // Turned on by timer if join enabled in device
    txTimeStatus = false;                           // Turned on by one shot timers

}


//----------------------------------------------------
//Bridge Slots

void syncSlot_BR()
{
    nrfSyncEnabled = true;
    slotNo = 0;
    setPinValue(SYNC_LED,1);                // Toggle SYNC for each sync sent
    if(debugMsg){
        putsUart0("\nSYNC-BR\n"); }
}
void downlinkSlot_BR(){
    setPinValue(SYNC_LED,0);                // Toggle SYNC for each sync sent
    slotNo = 1;
    txTimeStatus = true;
    downlinkSlotStart_br =true;

    if(debugMsg){
        putsUart0("DL-BR\n"); }
}
void fastackSlot_BR(){
    txTimeStatus = true;
    slotNo =2;
    downlinkSlotStart_br = false;
    fastackSlotStart_br = true;
    appSendFlag = true;

    if(debugMsg){
        putsUart0("FACK-BR\n"); }

}
void joinAccessSlot_BR(){
    txTimeStatus = true;
    slotNo =3;
    fastackSlotStart_br = false;
    acessSlotStart_br = true;

    if(debugMsg){
        putsUart0("JOIN-BR\n"); }
}
void uplinkSlot_BR(){
    acessSlotStart_br = false;
    uplinkSlot_br = false;                  // since uplink is not being used by bridge

    if(debugMsg){
        putsUart0("UL-BR\n"); }
}

void TimerHandler_BR()
{
    if(!timersStarted_br){                  // Start timers if not started
        stopTimer_ms(downlinkSlot_BR);
        stopTimer_ms(fastackSlot_BR);
        stopTimer_ms(joinAccessSlot_BR);
        stopTimer_ms(uplinkSlot_BR);

        // putsUart0("ts -BR\n");
        startOneshotTimer_ms(downlinkSlot_BR, DL_SLOT);
        startOneshotTimer_ms(fastackSlot_BR, FACK_SLOT);
        startOneshotTimer_ms(joinAccessSlot_BR, ACCESS_SLOT);
        startOneshotTimer_ms(uplinkSlot_BR, UL0_SLOT);

        timersStarted_br = true;
    }
}


//----------------------------------------------------
// SPI bus functions
//----------------------------------------------------

void enableNrfCs(void)
{
    setPinValue(SSI1FSS, 0);
    _delay_cycles(4);                    // allow line to settle
}

void disableNrfCs(void)
{
    setPinValue(SSI1FSS, 1);
}

void writeNrfReg(uint8_t reg, uint8_t data)
{
    enableNrfCs();
    writeSpi1Data(reg);
    readSpi1Data();
    writeSpi1Data(data);
    readSpi1Data();
    disableNrfCs();
}

void writeNrfData(uint8_t data)
{
    writeSpi1Data(data);
    readSpi1Data();
}

void readNrfReg(uint8_t reg, uint8_t *data)
{
    enableNrfCs();
    writeSpi1Data(reg);
    readSpi1Data();
    writeSpi1Data(0);
    *data = readSpi1Data();
    disableNrfCs();
}

void readNrfData(uint8_t *data)
{
    writeSpi1Data(0);
    *data = readSpi1Data();
}

//----------------------------------------------------
// Timing and Slot management functions
//----------------------------------------------------
void enableSync_BR()
{
    nrfSyncEnabled = true;
    isBridge = true;
}

void enableJoin_DEV()
{
    if(!getPinValue(JOIN_BUTTON)){ // wait for PB2 button press on device
        nrfJoinEnabled = true;
        nrfJoinCount =0;
    }
}

void enableJoin_BR()
{
    if(!getPinValue(JOIN_BUTTON)){ // wait for PB2 button press on bridge
        nrfJoinEnabled_BR = true;
        setPinValue(JOIN_LED, 1);
    }
}

//----------------------------------------------------
// Tx/ Rx functions
//----------------------------------------------------

void nrf24l0PulseCE(){

    setPinValue(CE_GPIO,1);
    waitMicrosecond(15);
    setPinValue(CE_GPIO,0);

}

void putNrf24l0DataPacket(uint8_t *data, uint16_t size)
{
    uint8_t j =0;
    int i = 0;

    // go to standby 1 mode
    setPinValue(CE_GPIO,0);
    waitMicrosecond(TX_RX_DELAY);

    // Tx mode setting
    writeNrfReg(W_REGISTER|CONFIG,0x0E);        // CRC enabled, PWRUP, Prim_rx =0
    enableNrfCs();                              // Frame begins
    writeNrfData(W_TX_PAYLOAD_NO_ACK);

    //Write data byte by byte
    for(i = 0, j =0; i < size ;i++, j++) {

        if( (j != 0 ) && (j % 32) == 0 ) {
            nrf24l0PulseCE();
            while(true != nrf24l0TxStatus());   //for every 32 bytes high to low transition //

            writeNrfReg(W_REGISTER|STATUS,0x20);
            disableNrfCs();

            waitMicrosecond(TX_RX_DELAY);
            enableNrfCs();

            writeNrfData(W_TX_PAYLOAD_NO_ACK);
        }
        writeNrfData(data[i]);
    }
    disableNrfCs();                             // Frame ends

    nrf24l0PulseCE();

    waitMicrosecond(TX_RX_DELAY);

    // poll till data is transmitted
    while(true != nrf24l0TxStatus());
    writeNrfReg(W_REGISTER|STATUS,0x20);        // clear the TX bit

}

void getnrf24l01DataPacket(){                   // Get 32 bytes of data at a time

    int i=0;
    //CE enable to Rx mode//
    setPinValue(CE_GPIO,1);

    writeNrfReg(W_REGISTER|CONFIG,0x0F);        // CRC enabled, PWRUP, Prim_Rx =1
    waitMicrosecond(TX_RX_DELAY);
    readNrfReg(R_REGISTER|R_RX_PL_WID, &payloadlength); // get the payload length of Rx packet

    enableNrfCs();                              // Frame begins
    writeNrfData(R_RX_PAYLOAD);

    while(i<payloadlength){
        readNrfData(&Rxpacket[Rx_wrIndex]);
        Rx_wrIndex = (Rx_wrIndex +1)%DATA_MAX_SIZE;
        ++i;
    }
    disableNrfCs();                             // Frame ends


    enableNrfCs();                              // Flush Tx FIFO after sending a SYNC
    writeSpi1Data(W_REGISTER|FLUSH_RX);
    readSpi1Data();
    disableNrfCs();
}

void parsenrf24l01DataPacket(){
    // Parse the 32 bytes from getRxpacket and check for Sync, Start code, Slot number, Length , Data
    uint8_t deviceNum = 0;
    uint32_t devBits =0;

    if(debugMsg){
        int i;
        uint8_t j = 0;
        char str[40];
        putsUart0("debug Rxpacket data: \n");
        for(i = 0; i<payloadlength; i++) {
            snprintf(str,sizeof(str),"%u ",Rxpacket[i]);
            putsUart0(str);
        }
        putsUart0("\n");
    }

    // check sync. Last byte for frame count1 is omitted
    if(strncmp((char*)&Rxpacket[Rx_wrIndex - payloadlength], (char*)syncMsg, sizeof(syncMsg) -2) == 0){
        isSync = true;
        Rx_index = 0;
        Rx_wrIndex = 0;
    }
    else{

        if(packetLength >32){ // Check for continuos transmission of data and we dont check for the payload length in subsequent packets
            putsUart0("More than 32 bytes HANDLE LATER\n");
        }
        else if(strncmp((char*)&Rxpacket[Rx_index], (char*)startCode, sizeof(startCode)) == 0){ // check start code
            deviceNum = eepromGetDevInfo_DEV();                 //Get device number stored in EEPROm. For already stored devices

            if(!nrfJoinEnabled && !isBridge) { // check whether myDevice got an ACK
                devBits = devBits | (Rxpacket[Rx_index +8] << 24);
                devBits = devBits | (Rxpacket[Rx_index +7] << 16);
                devBits = devBits | (Rxpacket[Rx_index +6] << 8);
                devBits = devBits | (Rxpacket[Rx_index +5]);

                if( (devBits & 0xFFFF0000) || (devBits & (1 << deviceNum)) == 0 ) {
                    return;
                }

                if(Rxpacket[Rx_index +2] == 2){                //Packet in  FACK slot (slot no =2) received by DEVICE
                    //                    if( Rxpacket[Rx_index +5] == deviceNum){         // check whether myDevice got an ACK
                    msgAcked = true;                             // Flag used by APPLICATION LAYER
                    //                    }
                }
            }

            else if(isBridge){
                devBits = devBits | (Rxpacket[Rx_index +8] << 24);
                devBits = devBits | (Rxpacket[Rx_index +7] << 16);
                devBits = devBits | (Rxpacket[Rx_index +6] << 8);
                devBits = devBits | (Rxpacket[Rx_index +5]);

                if(devBits & 0xFFFF0000) {
                    lastMsgDevNo_br = (devBits & 0x0000FFFF);
                }
            }
            if((Rxpacket[Rx_index+ 4])!=0x80){
                packetLength = Rxpacket[Rx_index+ 4] << 8;          // MSB ; Remlen is sent as LSB,MSB in the uint16 from tx side
            }
            packetLength |= Rxpacket[Rx_index +3];              // LSB of Remlen

            if(Rxpacket[Rx_index + 2] == 1){                    // packet in DL slot received by DEVICE

                if(Rxpacket[Rx_index+ 4]==0x80){               // Distinguish between JOIN RESP and normal DL slot
                    isJoinResp_dev = true;
                }
                else{
                    isDevPacket = true;
                    putsUart0("DL data received\n");
                }
                /*Used in setting dev no in downlink slotNo */
            }

            else if((Rxpacket[Rx_index +2] == 3)&& nrfJoinEnabled_BR){ // Msg in ACCESS slot received by the Bridge
                if(Rxpacket[Rx_index+ 4]==0x80){               // Check for JOIN REQ by checking for MSB bit set in uint16 remlen
                    isJoinPacket = true;
                }
            }
            else if(Rxpacket[Rx_index +2] > 3){                 // packet in Uplink slots slot
                isBridgePacket = true;                          // Is this packet received by the Bridge in Uplink Slots
            }
        }
        else{                                                   // Error handling in case of junk data
            Rx_index =0;
            Rx_wrIndex =0;
        }
    }
}

bool nrf24l0TxStatus()
{
    uint8_t data = 0;
    readNrfReg(R_REGISTER|STATUS,&data);
    return ((data & 0x20) >> 5);            // bit 5 is Tx_DS data sent fifo interrupt
}

bool nrf24l0RxStatus()
{
    uint8_t data = 0;
    readNrfReg(R_REGISTER|STATUS,&data);
    if(((data & 0x0E) == 0x0E) || ((data & 0x0E) == 0x0C)){ // check for the data pipe in RX_P_NO in FIFO_STATUS
        return false;
    }
    else{
        readNrfReg(R_REGISTER|STATUS,&data);                // Read status register to check RX_DR
        writeNrfReg(R_REGISTER|STATUS,(data & ~0x40));      // Set the RX_DR bit to clear the interrupt
        return true;
    }
}

void nrf24l0PowerUp()
{
    uint8_t data;

    data = 0;
    readNrfReg(R_REGISTER|CONFIG, &data);

    if(!(data & 0x02)) {
        writeNrfReg(W_REGISTER|CONFIG, 0x0E);               // CRC enabled, PWRUP, PRIM_RX=0
        waitMicrosecond(5000);                              // Wait 5 ms for checking that data write is complete

    }

}

bool isNrf24l0DataAvailable(void)                           // Check that data is available in Rx FIFO
{
    uint8_t data = 0;

    nrf24l0PowerUp();

    writeNrfReg(W_REGISTER|CONFIG, 0x0F);                   // CRC enabled, PWRup, PRIM_RX=1

    data = 0x70;
    writeNrfReg(W_REGISTER|STATUS, data);

    setPinValue(CE_GPIO,1);

    waitMicrosecond(TX_RX_DELAY);

    if(nrf24l0RxStatus()){
        writeNrfReg(W_REGISTER|STATUS,0x40);                // Clear the Rx interrupt
        return true;
    }
    else{
        return false;
    }
}

callback dataReceived(uint8_t *data, uint16_t size)
{
    int i;
    uint8_t j = 0;
    char str[40];
    // putsUart0("data received to app layer\n");
    /*data copy*/
    for(i = 0; i < size; i++) {
        buffer[i] = data[i];
        // snprintf(str,sizeof(str),"%u ",buffer[i]);
        // putsUart0(str);
    }
    // putsUart0("\n");

    dataReceivedFlag = true;
}


uint8_t nrf24l0GetChecksum(uint8_t *ptr, uint16_t size)
{
    uint8_t sum = 0;
    uint8_t i = 0;

    /*1 bytes checksum */
    for(i = 0; i < size; i ++)
    {
        sum += ptr[i];
    }

    return ~sum;
}

void nrf24l0TxSync()
{
    if(nrfSyncEnabled)
    {

        syncMsg[sizeof(syncMsg) -2] = (uint8_t)((syncFrameCount & 0xFF00)>>8);      // Frame count
        syncMsg[sizeof(syncMsg) -1] = (uint8_t)(syncFrameCount & 0x00FF);
        putNrf24l0DataPacket(syncMsg, sizeof(syncMsg));                             /*send sync message*/
        nrfSyncEnabled = false;
        ++syncFrameCount;
        char str[40];
        uint32_t mySlotTemp = getMySlot(readEeprom(NO_OF_DEV_IN_BRIDGE));
        snprintf(str, sizeof(str), "%"PRIu32"\n",mySlotTemp);
         putsUart0(str);
        mySlotTemp += GUARD_TIMER;
        stopTimer_ms(syncSlot_BR);
        startOneshotTimer_ms(syncSlot_BR, mySlotTemp);

        timersStarted_br = false;
        //        appSendFlag = true;
        msgAcked = false;

        Rx_index =0;
        Rx_wrIndex =0;
    }
}

void nrf24l0TxJoinReq_DEV()
{
    if(nrfJoinCount < 10){
        uint8_t joinBuffer[15] = {0};
        uint8_t* ptr = joinBuffer;
        uint8_t slot = 3;                                           // ACCESS slot
        uint16_t remlen = (1+sizeof(myMac))| (0x8000);           // 0x8000 in MSB byte separates a JOIN RESP from a normal DL packet

        strncpy((char*)ptr,(char*)startCode,sizeof(startCode));
        ptr += sizeof(startCode);
        strncpy((char*)ptr , (char*)&slot, sizeof(slot));
        ptr += sizeof(slot);
        strncpy((char*)ptr , (char*)&remlen, sizeof(remlen));
        ptr += sizeof(remlen);
        strncpy((char*)ptr , (char*)myMac, sizeof(myMac));
        ptr += sizeof(myMac);
        *ptr += nrf24l0GetChecksum(joinBuffer,sizeof(joinBuffer));

        //send join request in access slot
        putNrf24l0DataPacket(joinBuffer, sizeof(joinBuffer));
        txTimeStatus = false;
        putsUart0("JOIN REQ-D\n");
        acessSlotStart_dev = false;
        nrfJoinCount++;
    }
}

void nrf24l0TxJoinResp_BR()
{
    uint8_t joinBuffer[10] = {0};
    uint8_t* ptr = joinBuffer;
    uint8_t slot = 1;                                           // DL slot
    uint16_t remlen = ((1+1)|0x8000);                           //crc + allocated dev no + 0x8000 MSB set for JOIN RESP

    strncpy((char*)ptr,(char*)startCode,sizeof(startCode));
    ptr += sizeof(startCode);
    strncpy((char*)ptr , (char*)&slot, sizeof(slot));
    ptr += sizeof(slot);
    strncpy((char*)ptr , (char*)&remlen, sizeof(remlen));
    ptr += sizeof(remlen);
    strncpy((char*)ptr , (char*)&allocatedDevNum, sizeof(allocatedDevNum));
    ptr += sizeof(allocatedDevNum);
    *ptr += nrf24l0GetChecksum(joinBuffer,sizeof(joinBuffer));

    //send join response in access slot//
    putNrf24l0DataPacket(joinBuffer, sizeof(joinBuffer));
    txTimeStatus = false;
    nrfJoinEnabled_BR = false;
    sendJoinResponse_BR = false;
    downlinkSlotStart_br = false;                               // Make this false so that no more data is sent along with Join RESP in DL slot
    setPinValue(JOIN_LED, 0);

}

int nrf24l0TxMsg(uint8_t* data, uint16_t size, uint32_t devBitNum)
{
    uint8_t packet[DATA_MAX_SIZE] = {0};
    uint8_t *ptr = packet;
    uint8_t temp = 0;
    uint16_t remlen = size+ 1 + sizeof(devBitNum);              /*Increment length for checksum */

    if(size > (DATA_MAX_SIZE - META_DATA_SIZE))
    {
        return -1;
    }

    /*meta data*/
    strncpy((char*)ptr,(char*)startCode,sizeof(startCode));
    ptr += sizeof(startCode);
    strncpy((char*)ptr , (char*)&slotNo, sizeof(slotNo));
    ptr += sizeof(slotNo);
    strncpy((char*)ptr , (char*)&remlen, sizeof(remlen));
    ptr += sizeof(remlen);

    //user data//
    temp = (uint8_t)(devBitNum & 0xFF);
    strncpy((char*)ptr , (char*)&temp, 1);//Use devno for sending DL packets from Bridge or uplink packets from DEVICE
    ptr += 1;

    temp = (uint8_t)((devBitNum >> 8 ) & 0xFF);
    strncpy((char*)ptr , (char*)&temp, 1);//Use devno for sending DL packets from Bridge or uplink packets from DEVICE
    ptr += 1;

    temp = (uint8_t)((devBitNum >> 16) & 0xFF);
    strncpy((char*)ptr , (char*)&temp, 1);//Use devno for sending DL packets from Bridge or uplink packets from DEVICE
    ptr += 1;

    temp = (uint8_t)((devBitNum >> 24) & 0xFF);
    strncpy((char*)ptr , (char*)&temp, 1);//Use devno for sending DL packets from Bridge or uplink packets from DEVICE
    ptr += 1;


    strncpy((char*)ptr, (char*)data, size);
    ptr += size;

    //calculate checksum//
    *ptr = nrf24l0GetChecksum(packet,size + META_DATA_SIZE);

    putNrf24l0DataPacket(packet, size + META_DATA_SIZE);
    //guard timer //
    waitMicrosecond(GUARD_TIMER);

    enableNrfCs();                                              // Flush Tx FIFO after sending a SYNC
    writeSpi1Data(W_REGISTER|FLUSH_TX);
    readSpi1Data();
    disableNrfCs();

    memset(packet, 0, sizeof(packet));

    return 0;
}



void nrf24l0RxMsg(callback fn)
{
    // Devices use this function as used in template code to keep on polling the data to check for received messages
    //**SYNC
    // Check for Sync message and start timers for the devices transmission slot
    // If a new device is Joining a timer is started to wait for transmission in ACCESS slot
    // If the bridge is transmitting (assumption that Bridge device also sends the Sync packets), Wait for Downlink and FASTACK slots to transmit data

    uint16_t size = 0;
    uint8_t Rxchecksum = 0;                             // Received checksum
    uint8_t devMac[6] = {0};
    int i,j = 0;
    char str[30];

    if(isNrf24l0DataAvailable()) {

        getnrf24l01DataPacket();
        parsenrf24l01DataPacket();                      // set flags if syn message or startcode or

        if(isSync)                                      //if sync //
        {
            syncRxDevSlot();                            // Handle device sync slots
            msgAcked = false;                           // Check for ACKs is reset after each SYNC

            for(i=0; i<32;i++){
                Rxpacket[i] = 0;
            }

            setPinValue(SYNC_LED,1);                    // Toggle SYNC for each sync received
            waitMicrosecond(1000);
            setPinValue(SYNC_LED,0);
            putsUart0("SYNC-D\n");
        }

        else if(nrfJoinEnabled && isJoinResp_dev)          //JOIN RESP for dev
        {
            putsUart0("joinresponse-D\n");
            snprintf(str, sizeof(str), "allocated dev no: %"PRIu8,Rxpacket[Rx_index + 5]);
            putsUart0(str);
            uint32_t temp = (1<<Rxpacket[Rx_index + 5]);
            snprintf(str, sizeof(str), "\n allocated devbits no: %"PRIu32, temp);
            putsUart0(str);

            eepromSetDevInfo_DEV(Rxpacket[Rx_index + 5]);
            setPinValue(JOIN_LED,0);
            nrfJoinEnabled = false;                     // This is true from the moment dev sends a join req and receives a
            Rx_index += payloadlength - 1;
            isJoinResp_dev = false;
        }
        else if(msgAcked)                               //check acks for my device //
        {
            appSendFlag = false;                        // Received ack . Stop retransmission. Send this to Application layer to stop retransmissting messages
            Rx_index += payloadlength - 1;
        }

        else if(isJoinPacket){                          // Bridge checks for packets received in Access slot if join button pressed on Bridge
            if(nrfJoinEnabled_BR && acessSlotStart_br)
            {
                for(i = 0, j = Rx_index + 5; i< 6; i++, j++)
                {
                    devMac[i] = Rxpacket[j];
                }
                sendJoinResponse_BR = true;
                allocatedDevNum =  eepromSetGetDevInfo_BR(devMac); // Set mac address of bridge in eeprom
                Rx_index += payloadlength - 1;
                putsUart0("Join Packet Req- BR\n");
            }
        }
        else if((isBridgePacket) ||(isDevPacket))
        {                                               // is data for me? (Device in DL slots and BRIDGE in UL slots)
            //crc check*/
            Rxchecksum = Rxpacket[Rx_index + payloadlength -1];
            if(Rxchecksum == nrf24l0GetChecksum(&Rxpacket[Rx_index],payloadlength-1))
            {
                //Devices and Bridge can pop the data out and utilize it as needed
                size = payloadlength -META_DATA_SIZE;
                fn(&Rxpacket[Rx_index +META_DATA_SIZE -1], size);                       // downlink data. Pointer to User data start and size of the user data
                Rx_index = 0;
                Rx_wrIndex = 0;                                                         // reset for next packet Rx
                packetLength = 0;
                Rx_index += payloadlength - 1;

            }
            //reset the buffer. */ //Extra precaution
            for (i=0; i<sizeof(Rxpacket); i++){
                Rxpacket[i] =0;
            }
        }
    }

}

uint32_t getMySlot(uint8_t devno)
{

    //Handle slots for all transmissions for Devices and bridges separately
    // default value of EEPROM
    if(devno == 0xFF) {
        devno = 0;
    }

    return UL0_SLOT + (((DEF_SLOT_WIDTH) +(TX_RX_DELAY_SLOT*_32BYTE_PACKETS) + GUARD_TIMER)*devno) ;

}

uint32_t getDeviceNum_BR(){

}


void eepromSetDevInfo_DEV(uint8_t deviceNum)
{
    uint8_t *ptr;
    uint8_t i;

    writeEeprom(NO_OF_DEV_IN_BRIDGE,1);                         // Number of devices is 1
    writeEeprom(DEV1_NO_START,deviceNum);                       // Device number

    ptr = (uint8_t*)DEV1_MAC_START;

    /*My mac address*/
    for(i = 0; i < sizeof(myMac); i++,++ptr) {
        writeEeprom(ptr, myMac[i]);
    }


}

uint8_t eepromGetDevInfo_DEV()
{
    return readEeprom(DEV1_NO_START);                           // Only one device for Dev; Device number
}

uint8_t eepromSetGetDevInfo_BR(uint8_t *data)
{
    int i=0;
    int j = 0;
    uint8_t devNo = 0;
    uint8_t* ptr = (uint8_t*)NO_OF_DEV_IN_BRIDGE;
    char str[30];
    uint8_t mac[6] = {0};

    uint8_t noOfDev = (readEeprom(NO_OF_DEV_IN_BRIDGE) == 0xFF) ? 0 : readEeprom(NO_OF_DEV_IN_BRIDGE);
    ptr = (uint8_t*)DEV1_MAC_START;

    for(i = 0; i < noOfDev; i++) {

        for(j = 0; j < 6; j++,ptr++){
            mac[j] = readEeprom(ptr);
        }

        if(strncmp((char*)mac,(char*)data,sizeof(myMac)) == 0) {
            ptr -= (sizeof(myMac) + 1);
            devNo = readEeprom(ptr);

            putsUart0("Existing Dev No:-BR:  ");
            snprintf(str, sizeof(str),"%u", devNo);
            putsUart0(str);
            putsUart0("\n");

            return devNo;
        }
        ptr += 1 + sizeof(myMac);
    } // for ends

    if(noOfDev == 0) {
        ptr = (uint8_t*)DEV1_MAC_START;
    }

    --ptr;
    ++noOfDev;
    writeEeprom(ptr, noOfDev);
    devNo = readEeprom(ptr);
    writeEeprom(NO_OF_DEV_IN_BRIDGE,devNo);                     /* store total no of devices */
    ++ptr;

    putsUart0("Dev No: -BR:  ");
    snprintf(str, sizeof(str),"%u", devNo);
    putsUart0(str);
    putsUart0("\n");

    /*mac*/
    for(i = 0; i < sizeof(myMac); i++,++ptr) {
        writeEeprom(ptr, data[i]);
    }

    return devNo;
}

uint32_t getBridgeDevNoAddr_DEV(){
    uint32_t temp = 0xFFFF0000 | eepromGetDevInfo_DEV();
    return temp;
}

void capToDescription(char * devCap, char * description)
{
    if(strcmp(devCap, "MTRSP") == 0)
        strcpy(description, "Motor Speed Fast Medium Slow");
    
    else if(strcmp(devCap, "MTRDR") == 0)
        strcpy(description, "Motor Direction Forward Reverse"); 

    else if(strcmp(devCap, "MTRPW") == 0)
        strcpy(description, "Motor Power Start Stop");  

    else if(strcmp(devCap, "TEMPF") == 0)
        strcpy(description, "Temperature Degrees(Farenheit)"); 

    else if(strcmp(devCap, "BARCO") == 0)
        strcpy(description, "Barcode Code");  

    else if(strcmp(devCap, "DISTC") == 0)
        strcpy(description, "Distance_Sensor Count");   

    else if(strcmp(devCap, "DISTO") == 0)
        strcpy(description, "Distance_Sensor Detect");
}


void setWebserverDeviceNumber(uint8_t devNum)
{
     webserverDeviceNumber = devNum;
}

uint8_t getWebserverDeviceNumber(void)
{
     return webserverDeviceNumber;
}

bool isWebserverConnected(void)
{
    return webserverConnectionStatus;
}

void initWireless(void)
{
    nrf24l0Init();
    enableSync_BR();
    initEeprom();
}

void processWireless(void)
{
     wirelessPacket *wp = (wirelessPacket *)buffer;      // allocate space for the wireless packet
     // Bridge Functions //
    nrf24l0TxSync();
    TimerHandler_BR();
    enableJoin_BR();

    nrf24l0RxMsg(dataReceived);
    if (dataReceivedFlag == true)
    {
        wp = (wirelessPacket*)buffer;
        if (wp->packetType == PING_RESPONSE)
        {
            //Ping Response Function
        }
        else if (wp->packetType == DEVCAPS_RESPONSE)
        {
            deviceCaps *devCaps = (deviceCaps*)wp->data;
            char * descrip;
            char tempTopic[30] = {};
            uint8_t i;
            strncpy(tempTopic, longTopic, strlen(longTopic));

            for(i = 0; i < devCaps->numOfCaps; i++)
            {
                strncpy(tempTopic + strlen(longTopic), devCaps->caps[i].capDescription, 5);
                strcpy(subTopicQueue[i], tempTopic);
                //topic[15] = devCaps->caps[i][0];
            }
            numOfSubCaps = devCaps->numOfCaps;
            gf_mqtt_subscribe_caps = getMqttBrokerSocketIndex();
            // Check if device is present in EEEPROM
            MQTTBinding binding[3];

            char tempCaps[4][5] = {"MTRSP", "TEMPF", "BARCO", "DISTC"};

            for(i = 0; i < 4; i++)
            {
                MQTTBinding *isBinding = mqtt_binding_table_get((MQTTBinding **)&binding, 3, tempCaps[i]);

                if(isBinding != NULL)
                {
                    for(i = 0; i < devCaps->numOfCaps; i++)
                    {
                        binding[i].client_id[0] = 'd';
                        binding[i].client_id[1] = 'e';
                        binding[i].client_id[2] = 'v';
                        binding[i].client_id[3] = 'i';
                        binding[i].client_id[4] = 'c';
                        binding[i].client_id[5] = 'e';
                        binding[i].client_id[6] = devCaps->deviceNum + '0';

                        description desc = devCaps->caps[i];
                        capToDescription(desc.capDescription, descrip);

                        strncpy(binding[i].topic, subTopicQueue[i], strlen(subTopicQueue[i]));
                        strncpy(binding[i].devCaps, desc.capDescription, 5);
                        strncpy(binding[i].description, descrip, strlen(descrip));
                        binding[i].inOut = desc.inputOrOutput;
                    }
                    mqtt_binding_table_put((MQTTBinding **)&binding, devCaps->numOfCaps);
                }
            }
        }
        else if (wp->packetType == PUSH)
        {
            pushMessage *pushMsg = (pushMessage*)wp->data;
            
            // Using Table for long topic name (Not needed)
            /*
            MQTTBinding binding[3];
            strncpy(binding[0].client_id, "device", 6);
            // Get deviceNumber from timeslot number
            // binding.client_id[0][6] = getTimeSlot();
            strncpy(binding[0].devCaps, pushMsg->topicName, 5);
            // Find full topic name
            bool isDevicePresent = mqtt_binding_table_get(&binding);
            */

            char topicName[30] = {};
            strncpy(topicName, longTopic, strlen(longTopic));
            strncat(topicName, pushMsg->topicName, 5);
            // TODO add message to publishMsgBuffer
            // putsUart0(pushMsg->topicMessage);
            if((pubWrPtr + 1) % MAX_PUB_MSG_BUFFER_SIZE != pubRdPtr)
            {
                strcpy(pubMsgBuffer[pubWrPtr][PUB_MSG_BUFFER_TOPIC_INDEX], topicName);
                strcpy(pubMsgBuffer[pubWrPtr++][PUB_MSG_BUFFER_MSG_INDEX], pushMsg->topicMessage);
            }
            gf_mqtt_device_pub = getMqttBrokerSocketIndex();

        }
        else if (wp->packetType == WEB_SERVER)
        {
            uint8_t timeSlot = 0;
            setWebserverDeviceNumber(timeSlot);
            webserverConnectionStatus = true;
        }
        else
        {
            putsUart0("Invalid Packet Type\n\n");
        }
        dataReceivedFlag = false;
    }
    //check for when Bridge can transmit
    if(txTimeStatus)
    {
        if(sendJoinResponse_BR && downlinkSlotStart_br){
            nrf24l0TxJoinResp_BR();
            putsUart0("Join-RESP-BR\n");
        }
        if(downlinkSlotStart_br && appSendFlag){ // appsendflag is made true by proceshell or application handlers
            //                nrf24l0TxMsg(data,sizeof(data), 2); // Bit 2 is set (0b00000010) which represents device number 2
            //                appSendFlag = false;
            //                putsUart0("DL sent\n");
        }
        if(fastackSlotStart_br && appSendFlag){
            //                nrf24l0TxMsg(data,sizeof(data), 2);         // Bit 2 is set (0b00000010) which represents device number 2
            //                putsUart0("FACK sent\n");
            //                appSendFlag = false;
        }
        if (downlinkSlotStart_br && sendPingRequestFlag)
        {
            // cast as uint8_t
            nrf24l0TxMsg((uint8_t*)wp, sizeof(wp), 2);  //wp just an example cast your wireless  packet struct
            putsUart0("ping message sent.\n");          // ping message packet type set in process shell,
            // packet type must equal to PING_REQUEST for the devices team
            sendPingRequestFlag = false;

        }
        else if (downlinkSlotStart_br && sendDevCapsRequestFlag)
        {
            // convert deviceCaps struct to uint8_t
            // 0xFFFF is multicast address to all devices
            nrf24l0TxMsg((uint8_t*)wp, sizeof(wp), 0xFFFF); // devCaps message packet type set in process shell,
            // packet type must equal to DEVCAPS_REQUEST for the devices team
            putsUart0("caps message sent.\n");
            sendDevCapsRequestFlag = false;


        }
        else if (downlinkSlotStart_br && sendPushFlag)
        {
            // convert data struct to uint8_t
            nrf24l0TxMsg((uint8_t*)wp, sizeof(wp), 2); // push message packet type set in process shell,
            // packet type must equal to PUSH for the devices team
            putsUart0("push message sent.\n");
            sendPushFlag = false;

        }
        else if (sendPushFlag)
        {
            wp->packetType = PUSH; //always pushing our data
            pushMessageDevNum pushData;
            readPushMsgBuffer(&pushData);
            pushMessage *wpPushMsg = wp->data;
            // wpPushMsg = pushData.pushMsg;
            strncpy(wpPushMsg->topicName, (pushData.pushMsg.topicName), 5);
            strcpy(wpPushMsg->topicMessage, pushData.pushMsg.topicMessage);


            // convert data struct to uint8_t
            nrf24l0TxMsg((uint8_t*)wp, sizeof(wp), pushData.devNum); // push message packet type set in process shell,
                                                        // packet type must equal to PUSH for the devices team
            putsUart0("push message sent.\n");
            sendPushFlag = false;

        }
    }
}

int main1()
{
    initSystemClockTo40Mhz();

    initEeprom();
    initUart0();
    setUart0BaudRate(115200, 40e6);
    nrf24l0Init();

    putsUart0("Bridge powered up \n");
    // Flush Rx for device and bridge
    uint8_t data[] = {1,2,3,4,5,6}; // Random data, magic number change later
    enableSync_BR();


    wirelessPacket *wp = (wirelessPacket *)buffer;      // allocate space for the wireless packet
    putsUart0("press help to know the functions\n");

    while(true)
    {
        processShell1(wp);

        // Bridge Functions //
        nrf24l0TxSync();
        TimerHandler_BR();
        enableJoin_BR();

        //        appSendFlag = true;
        //Common functions//
        nrf24l0RxMsg(dataReceived);
        if (dataReceivedFlag == true)
        {
            wp = (wirelessPacket*)buffer;
            if (wp->packetType == PING_RESPONSE)
            {
                //Ping Response Function
            }
            else if (wp->packetType == DEVCAPS_RESPONSE)
            {
                deviceCaps *devCaps = (deviceCaps*)wp->data;
                char * descrip;
                char tempTopic[30] = {};
                uint8_t i;
                strncpy(tempTopic, longTopic, strlen(longTopic));

                for(i = 0; i < devCaps->numOfCaps; i++)
                {
                    strncpy(tempTopic + strlen(longTopic), devCaps->caps[i].capDescription, 5);
                    strcpy(subTopicQueue[i], tempTopic);
                    //topic[15] = devCaps->caps[i][0];
                }
                numOfSubCaps = devCaps->numOfCaps;
                gf_mqtt_subscribe_caps = getMqttBrokerSocketIndex();
                // Check if device is present in EEEPROM
                MQTTBinding binding[3];

                char tempCaps[4][5] = {"MTRSP", "TEMPF", "BARCO", "DISTC"};

                for(i = 0; i < 4; i++)
                {
                    MQTTBinding *isBinding = mqtt_binding_table_get((MQTTBinding **)&binding, 3, tempCaps[i]);

                    if(isBinding != NULL)
                    {
                        for(i = 0; i < devCaps->numOfCaps; i++)
                        {
                            binding[i].client_id[0] = 'd';
                            binding[i].client_id[1] = 'e';
                            binding[i].client_id[2] = 'v';
                            binding[i].client_id[3] = 'i';
                            binding[i].client_id[4] = 'c';
                            binding[i].client_id[5] = 'e';
                            binding[i].client_id[6] = devCaps->deviceNum + '0';

                            description desc = devCaps->caps[i];
                            capToDescription(desc.capDescription, descrip);

                            strncpy(binding[i].topic, subTopicQueue[i], strlen(subTopicQueue[i]));
                            strncpy(binding[i].devCaps, desc.capDescription, 5);
                            strncpy(binding[i].description, descrip, strlen(descrip));
                            binding[i].inOut = desc.inputOrOutput;
                        }
                        mqtt_binding_table_put((MQTTBinding **)&binding, devCaps->numOfCaps);
                    }
                }
            }
            else if (wp->packetType == PUSH)
            {
                pushMessage *pushMsg = (pushMessage*)wp->data;
                
                // Using Table for long topic name (Not needed)
                /*
                MQTTBinding binding[3];
                strncpy(binding[0].client_id, "device", 6);
                // Get deviceNumber from timeslot number
                // binding.client_id[0][6] = getTimeSlot();
                strncpy(binding[0].devCaps, pushMsg->topicName, 5);
                // Find full topic name
                bool isDevicePresent = mqtt_binding_table_get(&binding);
                */

                char topicName[30] = {};
                strncpy(topicName, longTopic, strlen(longTopic));
                strncat(topicName, pushMsg->topicName, 5);
                // TODO add message to publishMsgBuffer

                if((pubWrPtr + 1) % MAX_PUB_MSG_BUFFER_SIZE != pubRdPtr)
                {
                    strcpy(pubMsgBuffer[pubWrPtr][PUB_MSG_BUFFER_TOPIC_INDEX], topicName);
                    strcpy(pubMsgBuffer[pubWrPtr++][PUB_MSG_BUFFER_MSG_INDEX], pushMsg->topicMessage);
                }
                gf_mqtt_device_pub = getMqttBrokerSocketIndex();

            }
            else if (wp->packetType == WEB_SERVER)
            {
                uint8_t timeSlot = 0;
                setWebserverDeviceNumber(timeSlot);
                webserverConnectionStatus = true;
            }
            else
            {
                putsUart0("Invalid Packet Type\n\n");
            }
            dataReceivedFlag = false;
        }
        //check for when Bridge can transmit
        if(txTimeStatus)
        {
            if(sendJoinResponse_BR && downlinkSlotStart_br){
                nrf24l0TxJoinResp_BR();
                putsUart0("Join-RESP-BR\n");
            }
            if(downlinkSlotStart_br && appSendFlag){ // appsendflag is made true by proceshell or application handlers
                //                nrf24l0TxMsg(data,sizeof(data), 2); // Bit 2 is set (0b00000010) which represents device number 2
                //                appSendFlag = false;
                //                putsUart0("DL sent\n");
            }
            if(fastackSlotStart_br && appSendFlag){
                //                nrf24l0TxMsg(data,sizeof(data), 2);         // Bit 2 is set (0b00000010) which represents device number 2
                //                putsUart0("FACK sent\n");
                //                appSendFlag = false;
            }
            if (downlinkSlotStart_br && sendPingRequestFlag)
            {
                // cast as uint8_t
                nrf24l0TxMsg((uint8_t*)wp, sizeof(wp), shellDestinationDevNumber);  //wp just an example cast your wireless  packet struct
                putsUart0("ping message sent.\n");          // ping message packet type set in process shell,
                // packet type must equal to PING_REQUEST for the devices team
                sendPingRequestFlag = false;

            }
            else if (downlinkSlotStart_br && sendDevCapsRequestFlag)
            {
                // convert deviceCaps struct to uint8_t
                nrf24l0TxMsg((uint8_t*)wp, sizeof(wp), shellDestinationDevNumber); // devCaps message packet type set in process shell,
                // packet type must equal to DEVCAPS_REQUEST for the devices team
                putsUart0("caps message sent.\n");
                sendDevCapsRequestFlag = false;


            }
            else if (downlinkSlotStart_br && sendPushFlag)
            {
                // convert data struct to uint8_t
                nrf24l0TxMsg((uint8_t*)wp, sizeof(wp), shellDestinationDevNumber); // push message packet type set in process shell,
                // packet type must equal to PUSH for the devices team
                putsUart0("push message sent.\n");
                sendPushFlag = false;

            }
            else if (sendPushFlag)
            {
                wp->packetType = PUSH; //always pushing our data
                pushMessageDevNum pushData;
                readPushMsgBuffer(&pushData);

                // convert data struct to uint8_t
                nrf24l0TxMsg((uint8_t*)wp, sizeof(wp), shellDestinationDevNumber); // push message packet type set in process shell,
                                                            // packet type must equal to PUSH for the devices team
                putsUart0("push message sent.\n");
                sendPushFlag = false;

            }
        }
    }
}

bool queuePushMsg(pushMessage *pushMsg, uint8_t devNum)
{
    if((pushWrPtr + 1) % MAX_PSH_MSG_BUFFER_SIZE == pushRdPtr)
        return false;
    
    
    strncpy(pushMsgBuffer[pushWrPtr].pushMsg.topicName, pushMsg->topicName, 5);
    strcpy(pushMsgBuffer[pushWrPtr].pushMsg.topicMessage, pushMsg->topicMessage);
    pushMsgBuffer[pushWrPtr].devNum = devNum;
    
    pushWrPtr = (pushWrPtr + 1) % MAX_PSH_MSG_BUFFER_SIZE;
    sendPushFlag = true;
    return true;
}
bool readPushMsgBuffer(pushMessageDevNum *pushMsgDevNum)
{
    if(pushRdPtr == pushWrPtr)
        return false;
    
    pushMessageDevNum tempMsgBuffer = pushMsgBuffer[pushRdPtr];
    strncpy(pushMsgDevNum->pushMsg.topicName, tempMsgBuffer.pushMsg.topicName, 5);
    strcpy(pushMsgDevNum->pushMsg.topicMessage, tempMsgBuffer.pushMsg.topicMessage);
    pushMsgDevNum->devNum = tempMsgBuffer.devNum;

    pushRdPtr = (pushRdPtr + 1) % MAX_PUB_MSG_BUFFER_SIZE;
    return true;
}
bool readPubMsgBuffer(char pubMsg[1][2][30])
{
    if(pubRdPtr == pubWrPtr)
        return false;
    

    strcpy(pubMsg[0][PUB_MSG_BUFFER_TOPIC_INDEX], pubMsgBuffer[pubRdPtr][PUB_MSG_BUFFER_TOPIC_INDEX]);
    strcpy(pubMsg[0][PUB_MSG_BUFFER_MSG_INDEX], pubMsgBuffer[pubRdPtr][PUB_MSG_BUFFER_MSG_INDEX]);
    pubRdPtr = (pubRdPtr + 1) % MAX_PUB_MSG_BUFFER_SIZE;
    return true;


}
bool isPubMsgBufferEmpty(void)
{
    return pubRdPtr == pubWrPtr;
}

void setShellDestinationDevNumber(uint8_t devNum)
{
    shellDestinationDevNumber = devNum;
}