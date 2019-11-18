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
#if L6470_INVERT
    PinSetLo(M_AUX_GPIO, M_SW1);  // SW1 must be hi when not used
#else
    PinSetHi(M_AUX_GPIO, M_SW1);  // SW1 must be hi when not used
#endif
//    PinSetupInput(M_AUX_GPIO, M_BUSY_SYNC1, pudPullUp);
    // SPI pins
    PinSetupOut      (M_SPI_GPIO, M_CS,   omPushPull);
    PinSetupAlterFunc(M_SPI_GPIO, M_SCK,  omPushPull, pudNone, M_SPI_AF);
    PinSetupAlterFunc(M_SPI_GPIO, M_MISO, omPushPull, pudNone, M_SPI_AF);
    PinSetupAlterFunc(M_SPI_GPIO, M_MOSI, omPushPull, pudNone, M_SPI_AF);
    CsHi();
    // ==== SPI ==== MSB first, master, ClkLowIdle, FirstEdge, Baudrate=5MHz
#if L6470_INVERT
    ISpi.Setup(boMSB, cpolIdleHigh, cphaFirstEdge, 5000000);
#else
    ISpi.Setup(boMSB, cpolIdleLow, cphaFirstEdge, 5000000);
#endif
    ISpi.Enable();
    Reset();
}

void L6470_t::Reset() {
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
    WriteByte(0b01010000 | b);
    CSHiLo();
    WriteByte(dwb.b[2]);
    CSHiLo();
    WriteByte(dwb.b[1]);
    CSHiLo();
    WriteByte(dwb.b[0]);
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
    WriteByte(0b01000000 | b);
    CSHiLo();
    WriteByte(dwb.b[2]);
    CSHiLo();
    WriteByte(dwb.b[1]);
    CSHiLo();
    WriteByte(dwb.b[0]);
    CsHi();
    chSysUnlock();
}
#endif


#if 1 // ======================= Params, Read/Write ============================
void L6470_t::Cmd(uint8_t ACmd) {
    CsLo();
    WriteByte(ACmd);
    CsHi();
}

void L6470_t::SetParam8(uint8_t Addr, uint8_t Value) {
    CsLo();
    WriteByte(Addr);
    CSHiLo();
    WriteByte(Value);
    CsHi();
}
void L6470_t::SetParam16(uint8_t Addr, uint16_t Value) {
    CsLo();
    WriteByte(Addr);
    CSHiLo();
    WriteByte(0xFF & (Value >> 8)); // MSB
    CSHiLo();
    WriteByte(0xFF & (Value >> 0)); // LSB
    CsHi();
}

uint8_t L6470_t::GetParam8(uint8_t Addr) {
    uint8_t v1 = 0;
    CsLo();
    WriteByte(0b00100000 | Addr);
    CSHiLo();
    v1 = ReadByte();
    CsHi();
    return v1;
}

uint16_t L6470_t::GetParam16(uint8_t Addr) {
    uint32_t v1, v2;
    CsLo();
    WriteByte(0b00100000 | Addr);
    CSHiLo();
    v1 = ReadByte();
    CSHiLo();
    v2 = ReadByte();
    CsHi();
    return (v1 << 8) | (v2 << 0);
}

uint32_t L6470_t::GetParam32(uint8_t Addr) {
    uint32_t v1, v2, v3;
    CsLo();
    WriteByte(0b00100000 | Addr);
    CSHiLo();
    v1 = ReadByte();
    CSHiLo();
    v2 = ReadByte();
    CSHiLo();
    v3 = ReadByte();
    CsHi();
    return (v1 << 16) | (v2 << 8) | (v3 << 0);
}

uint16_t L6470_t::GetStatus() {
    uint16_t rslt;
    CsLo();
    WriteByte(0b11010000);
    CSHiLo();
    rslt = ReadByte();
    CSHiLo();
    rslt <<= 8;
    rslt |= ReadByte();
    CsHi();
    return rslt;
}
#endif

#if 1 // =========================== Lo Level ==================================
#if L6470_INVERT
void L6470_t::CsHi() { PinSetLo(M_SPI_GPIO, M_CS); L6470_CS_DELAY(); }
void L6470_t::CsLo() { PinSetHi(M_SPI_GPIO, M_CS); L6470_CS_DELAY(); }
void L6470_t::CSHiLo() {
    PinSetLo(M_SPI_GPIO, M_CS);
    L6470_CS_DELAY();
    PinSetHi(M_SPI_GPIO, M_CS);
}

void L6470_t::WriteByte(uint8_t b) {
    b ^= 0xFF;
    ISpi.ReadWriteByte(b);
}

uint8_t L6470_t::ReadByte() {
    uint8_t b = ISpi.ReadWriteByte(0);
    b ^= 0xFF;
    return b;
}

void L6470_t::ResetOn()  { PinSetHi(M_AUX_GPIO, M_STBY_RST); }
void L6470_t::ResetOff() { PinSetLo(M_AUX_GPIO, M_STBY_RST); }

bool L6470_t::IsBusy()   { return PinIsHi(M_AUX_GPIO, M_BUSY_SYNC1); }
bool L6470_t::IsFlagOn() { return PinIsHi(M_AUX_GPIO, M_FLAG1); }

void L6470_t::SwitchLoHi() {
    PinSetHi(M_AUX_GPIO, M_SW1);
    L6470_CS_DELAY();
    PinSetLo(M_AUX_GPIO, M_SW1);
}

#else
void L6470_t::CsHi() { PinSetHi(M_SPI_GPIO, M_CS); L6470_CS_DELAY(); }
void L6470_t::CsLo() { PinSetLo(M_SPI_GPIO, M_CS); L6470_CS_DELAY(); }
void L6470_t::CSHiLo() {
    PinSetHi(M_SPI_GPIO, M_CS);
    L6470_CS_DELAY();
    PinSetLo(M_SPI_GPIO, M_CS);
}

void L6470_t::WriteByte(uint8_t b) { ISpi.ReadWriteByte(b); }
uint8_t L6470_t::ReadByte() { return ISpi.ReadWriteByte(0); }

void L6470_t::ResetOn()  { PinSetLo(M_AUX_GPIO, M_STBY_RST); }
void L6470_t::ResetOff() { PinSetHi(M_AUX_GPIO, M_STBY_RST); }

bool L6470_t::IsBusy()   { return !PinIsHi(M_AUX_GPIO, M_BUSY_SYNC1); }
bool L6470_t::IsFlagOn() { return !PinIsHi(M_AUX_GPIO, M_FLAG1); }

void L6470_t::SwitchLoHi() {
    PinSetLo(M_AUX_GPIO, M_SW1);
    L6470_CS_DELAY();
    PinSetHi(M_AUX_GPIO, M_SW1);
}

#endif // invert
#endif // Lo level

