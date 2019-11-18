/*
 * LoadCtrl.cpp
 *
 *  Created on: Oct 3, 2019
 *      Author: laurelindo
 */

#include "LoadCtrl.h"
#include "kl_lib.h"
#include "uart.h"
#include "shell.h"

/*
 * D0: C53            D8:  R130 (100R)
 * D1: C54            D9:  R128 (68R)
 * D2: C50            D10: R126 (47R)
 * D3: C49            D11: R116 (22R)
 * D4: R114 (10R)     D12: C51
 * D5: R112 (6R8)     D13: C47
 * D6: R110 (4R7)     D14: C52
 * D7: R124 (3R3)     D15: C48
 */

static const Spi_t ISpi{LOAD_SPI};
static uint16_t Bitmask = 0;

static const uint16_t CapCnt2Bitmask[9] = {
        0x0000,
        0x0001, 0x0003, 0x0007, 0x000F, 0x100F, 0x300F, 0x700F, 0xF00F
};

namespace Load {

void Init() {
    // Load control
    PinSetupOut(LOAD_SRCLR, omPushPull);
    PinSetHi(LOAD_SRCLR); // Do not clear
    PinSetupOut(LOAD_RCLK, omPushPull);
    PinSetLo(LOAD_RCLK);
    // SPI
    PinSetupAlterFunc(LOAD_SRCLK, omPushPull, pudNone, LOAD_AF);
    PinSetupAlterFunc(LOAD_SER, omPushPull, pudNone, LOAD_AF);
    ISpi.Setup(boMSB, cpolIdleLow, cphaFirstEdge, 50000000, bitn16);
    ISpi.Enable();
}

void SetRaw(uint16_t W16) {
//    Printf("Bitmsk: %X\r", W16);
    ISpi.ReadWriteWord(W16^0xFFFF); // Invert it
    PinSetHi(LOAD_RCLK);
    PinSetLo(LOAD_RCLK);
}

void ConnectCapacitors(uint8_t CapCnt) {
    if(CapCnt > 8) CapCnt = 8;
    Bitmask &= 0x0FF0; // Clear capacitors connection
    Bitmask |= CapCnt2Bitmask[CapCnt];
    SetRaw(Bitmask);
}

void SetResLoad(uint8_t LoadValue) {
    Bitmask &= 0xF00F; // Clear resistors connection
    if(LoadValue & 0x01) Bitmask |= 0x0100; // Connect 100R
    if(LoadValue & 0x02) Bitmask |= 0x0200; // Connect 68R
    if(LoadValue & 0x04) Bitmask |= 0x0400; // Connect 47R
    if(LoadValue & 0x08) Bitmask |= 0x0800; // Connect 22R
    if(LoadValue & 0x10) Bitmask |= 0x0010; // Connect 10R
    if(LoadValue & 0x20) Bitmask |= 0x0020; // Connect 6R8
    if(LoadValue & 0x40) Bitmask |= 0x0040; // Connect 4R7
    if(LoadValue & 0x80) Bitmask |= 0x0080; // Connect 3R3
    SetRaw(Bitmask);
}


}; // namespace
