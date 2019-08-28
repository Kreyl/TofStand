/*
 * motorL6470.h
 *
 *  Created on: 25 июля 2015 г.
 *      Author: Kreyl
 */

#pragma once

#include "kl_lib.h"
#include "uart.h"
#include "board.h"

#define L6470_CS_DELAY() { for(volatile uint32_t i=0; i<10; i++) __NOP(); }

#if 1 // ========= Registers etc. ===========
// Registers
#define L6470_REG_ACCELERATION  0x05
#define L6470_REG_DECELERATION  0x06
#define L6470_REG_MAX_SPEED     0x07
#define L6470_REG_STEP_MODE     0x16
#define L6470_REG_CONFIG        0x18
#define L6470_REG_STATUS        0x19

// Values
#define L6470_MAX_SPEED_EVER    15610

#endif

enum Dir_t {dirStop = 0, dirForward = 1, dirReverse = 2};
// Step mode: how many microsteps in one step
enum StepMode_t {smFull=0, sm2=1, sm4=2, sm8=3, sm16=4, sm32=5, sm64=6, sm128=7};

class L6470_t {
private:
    Spi_t ISpi;
    void CsHi() { PinSetHi(M_SPI_GPIO, M_CS); L6470_CS_DELAY(); }
    void CsLo() { PinSetLo(M_SPI_GPIO, M_CS); }
    void CSHiLo() {
        PinSetHi(M_SPI_GPIO, M_CS);
        L6470_CS_DELAY();
        PinSetLo(M_SPI_GPIO, M_CS);
    }
    // Commands
    uint16_t GetStatus();
    void GetParam(uint8_t Addr, uint8_t *PParam1);
    void GetParam(uint8_t Addr, uint8_t *PParam1, uint8_t *PParam2);
    void SetParam8(uint8_t Addr, uint8_t Value);
    void SetParam16(uint8_t Addr, uint16_t Value);
    void Cmd(uint8_t ACmd); // Single-byte cmd
protected:
    void ResetOn()  { PinSetLo(M_AUX_GPIO, M_STBY_RST); }
    void ResetOff() { PinSetHi(M_AUX_GPIO, M_STBY_RST); }
    bool IsBusy()   { return !PinIsHi(M_AUX_GPIO, M_BUSY_SYNC1); }
    bool IsFlagOn() { return !PinIsHi(M_AUX_GPIO, M_FLAG1); }
public:
    L6470_t(SPI_TypeDef *ASpi) : ISpi(ASpi) {}
    void Init();
    // Motion
    void Run(Dir_t Dir, uint32_t Speed);
    // Stop
    void StopSoftAndHold() { Cmd(0b10110000); } // SoftStop
    void StopSoftAndHiZ()  { Cmd(0b10100000); } // SoftHiZ
    // Mode of operation
    void SetConfig(uint16_t Cfg) { SetParam16(L6470_REG_CONFIG, Cfg); }
    void SetStepMode(StepMode_t StepMode) { SetParam8(L6470_REG_STEP_MODE, (uint8_t)StepMode); }
    void SetAcceleration(uint16_t Value) { SetParam16(L6470_REG_ACCELERATION, Value); }
    void SetDeceleration(uint16_t Value) { SetParam16(L6470_REG_DECELERATION, Value); }
    void SetCurrent4Hold(uint8_t KVal) { SetParam8(0x09, KVal); }
    void SetCurrent4Run(uint8_t KVal)  { SetParam8(0x0A, KVal); }
    void SetCurrent4Acc(uint8_t KVal)  { SetParam8(0x0B, KVal); }
    void SetCurrent4Dec(uint8_t KVal)  { SetParam8(0x0C, KVal); }
};
