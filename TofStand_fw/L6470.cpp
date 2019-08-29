/*
 * motorL6470.cpp
 *
 *  Created on: 25 июля 2015 г.
 *      Author: Kreyl
 */

#include "L6470.h"
#include "uart.h"

void L6470_t::Init() {
    // Aux pins
    PinSetupOut(M_AUX_GPIO, M_STBY_RST, omPushPull);
    PinSetupOut(M_AUX_GPIO, M_SW1, omPushPull);
    PinSetHi(M_AUX_GPIO, M_SW1);  // SW1 must be hi when not used
//    PinSetupInput(M_AUX_GPIO, M_BUSY_SYNC1, pudPullUp);
    // SPI pins
    PinSetupOut      (M_SPI_GPIO, M_CS,   omPushPull);
    PinSetupAlterFunc(M_SPI_GPIO, M_SCK,  omPushPull, pudNone, M_SPI_AF);
    PinSetupAlterFunc(M_SPI_GPIO, M_MISO, omPushPull, pudNone, M_SPI_AF);
    PinSetupAlterFunc(M_SPI_GPIO, M_MOSI, omPushPull, pudNone, M_SPI_AF);
    CsHi();
    // ==== SPI ==== MSB first, master, ClkLowIdle, FirstEdge, Baudrate=...
    ISpi.Setup(boMSB, cpolIdleLow, cphaFirstEdge, 5000000);
    ISpi.Enable();
    // ==== Reset IC ====
    ResetOn();
    chThdSleepMilliseconds(7);
    ResetOff();
    chThdSleepMilliseconds(27);
}

#if 1 // ============================ Motion ===================================
void L6470_t::Run(Dir_t Dir, uint32_t Speed) {
    uint8_t b = 0;
    switch(Dir) {
        case dirStop:
            chSysLock();
            StopSoftAndHiZ();
            chSysUnlock();
            return;
        case dirForward:
            b = 1;
            break;
        case dirReverse:
            b = 0;
            break;
    }
    SetParam16(L6470_REG_MAX_SPEED, 0x3FF); // Top speed possible
    Convert::DWordBytes_t dwb;
    dwb.DWord = Speed;
    chSysLock();
    CsLo();
    ISpi.ReadWriteByte(0b01010000 | b);
    CSHiLo();
    ISpi.ReadWriteByte(dwb.b[2]);
    CSHiLo();
    ISpi.ReadWriteByte(dwb.b[1]);
    CSHiLo();
    ISpi.ReadWriteByte(dwb.b[0]);
    CsHi();
    chSysUnlock();
}

void L6470_t::Move(Dir_t Dir, uint32_t Speed, uint32_t Steps) {
    uint8_t b = 0;
    switch(Dir) {
        case dirStop:
            chSysLock();
            StopSoftAndHiZ();
            chSysUnlock();
            return;
        case dirForward:
            b = 1;
            break;
        case dirReverse:
            b = 0;
            break;
    }
    // Set speed
    SetMaxSpeed(Speed / 1024);
    // Send MOVE cmd
    Steps &= 0x3FFFFF; // 22 bits allowed
    Convert::DWordBytes_t dwb;
    dwb.DWord = Steps;
    chSysLock();
    CsLo();
    ISpi.ReadWriteByte(0b01000000 | b);
    CSHiLo();
    ISpi.ReadWriteByte(dwb.b[2]);
    CSHiLo();
    ISpi.ReadWriteByte(dwb.b[1]);
    CSHiLo();
    ISpi.ReadWriteByte(dwb.b[0]);
    CsHi();
    chSysUnlock();
}

void L6470_t::SwitchLoHi() {
    PinSetLo(M_AUX_GPIO, M_SW1);
    L6470_CS_DELAY();
    PinSetHi(M_AUX_GPIO, M_SW1);
}
#endif


#if 1 // ======================= Params, Read/Write ============================
void L6470_t::Cmd(uint8_t ACmd) {
    CsLo();
    ISpi.ReadWriteByte(ACmd);
    CsHi();
}

void L6470_t::SetParam8(uint8_t Addr, uint8_t Value) {
    CsLo();
    ISpi.ReadWriteByte(Addr);
    CSHiLo();
    ISpi.ReadWriteByte(Value);
    CsHi();
}
void L6470_t::SetParam16(uint8_t Addr, uint16_t Value) {
    CsLo();
    ISpi.ReadWriteByte(Addr);
    CSHiLo();
    ISpi.ReadWriteByte(0xFF & (Value >> 8)); // MSB
    CSHiLo();
    ISpi.ReadWriteByte(0xFF & (Value >> 0)); // LSB
    CsHi();
}

uint8_t L6470_t::GetParam8(uint8_t Addr) {
    uint8_t v1 = 0;
    CsLo();
    ISpi.ReadWriteByte(0b00100000 | Addr);
    CSHiLo();
    v1 = ISpi.ReadWriteByte(0);
    CsHi();
    return v1;
}

uint16_t L6470_t::GetParam16(uint8_t Addr) {
    uint32_t v1, v2;
    CsLo();
    ISpi.ReadWriteByte(0b00100000 | Addr);
    CSHiLo();
    v1 = ISpi.ReadWriteByte(0);
    CSHiLo();
    v2 = ISpi.ReadWriteByte(0);
    CsHi();
    return (v1 << 8) | (v2 << 0);
}

uint32_t L6470_t::GetParam32(uint8_t Addr) {
    uint32_t v1, v2, v3;
    CsLo();
    ISpi.ReadWriteByte(0b00100000 | Addr);
    CSHiLo();
    v1 = ISpi.ReadWriteByte(0);
    CSHiLo();
    v2 = ISpi.ReadWriteByte(0);
    CSHiLo();
    v3 = ISpi.ReadWriteByte(0);
    CsHi();
    return (v1 << 16) | (v2 << 8) | (v3 << 0);
}

uint16_t L6470_t::GetStatus() {
    uint16_t rslt;
    CsLo();
    ISpi.ReadWriteByte(0b11010000);
    CSHiLo();
    rslt = ISpi.ReadWriteByte(0);
    CSHiLo();
    rslt <<= 8;
    rslt |= ISpi.ReadWriteByte(0);
    CsHi();
    return rslt;
}
#endif
