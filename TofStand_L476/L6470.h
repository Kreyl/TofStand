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

#define L6470_INVERT            TRUE

#define L6470_CS_DELAY() { for(volatile uint32_t i=0; i<20; i++); }

#if 1 // ========= Registers etc. ===========
// Registers
#define L6470_REG_ACCELERATION  0x05
#define L6470_REG_DECELERATION  0x06
#define L6470_REG_MAX_SPEED     0x07
#define L6470_REG_STEP_MODE     0x16
#define L6470_REG_CONFIG        0x18
#define L6470_REG_STATUS        0x19

// Values
#define SPD_MAX                 40005
#define SPD_MAX_SMALL           (SPD_MAX_SMALL / 1024) // For some reason, there are two definitions of speed

#endif

enum Dir_t {dirStop = 0, dirForward = 1, dirReverse = 2};
// Step mode: how many microsteps in one step
enum StepMode_t {smFull=0, sm2=1, sm4=2, sm8=3, sm16=4, sm32=5, sm64=6, sm128=7};

class L6470_t {
private:
    Spi_t ISpi;
    void CsHi();
    void CsLo();
    void CSHiLo();
    void WriteByte(uint8_t b);
    uint8_t ReadByte();

    uint16_t GetStatus();
    void Cmd(uint8_t ACmd); // Single-byte cmd
protected:
    void ResetOn();
    void ResetOff();
    bool IsBusy();
    bool IsFlagOn();
public:
    L6470_t(SPI_TypeDef *ASpi) : ISpi(ASpi) {}
    // Control
    void Init();
    void Reset();
    uint8_t GetParam8(uint8_t Addr);
    uint16_t GetParam16(uint8_t Addr);
    uint32_t GetParam32(uint8_t Addr);
    void SetParam8(uint8_t Addr, uint8_t Value);
    void SetParam16(uint8_t Addr, uint16_t Value);
    void SetParam24(uint8_t Addr, uint32_t Value);
    // Motion
    void Run(Dir_t Dir, uint32_t Speed);
    void Move(Dir_t Dir, uint32_t Speed, uint32_t Steps);
    bool IsStopped();
    int32_t GetAbsPos();
    void ResetAbsPos();
    // Stop
    void StopSoftAndHold() { Cmd(0b10110000); } // SoftStop
    void StopSoftAndHiZ()  { Cmd(0b10100000); } // SoftHiZ
    void StopNow(); // HardStop the motor using switch. Way faster than SPI cmd.
    // Mode of operation
    void SetMaxSpeed(uint32_t MaxSpd) { SetParam16(L6470_REG_MAX_SPEED, MaxSpd); }
    void SetConfig(uint16_t Cfg) { SetParam16(L6470_REG_CONFIG, Cfg); }
    void SetStepMode(StepMode_t StepMode) { SetParam8(L6470_REG_STEP_MODE, (uint8_t)StepMode); }
    void SetAcceleration(uint16_t Value) { SetParam16(L6470_REG_ACCELERATION, Value); }
    void SetDeceleration(uint16_t Value) { SetParam16(L6470_REG_DECELERATION, Value); }
    void SetCurrent4Hold(uint8_t KVal) { SetParam8(0x09, KVal); }
    void SetCurrent4Run(uint8_t KVal)  { SetParam8(0x0A, KVal); }
    void SetCurrent4Acc(uint8_t KVal)  { SetParam8(0x0B, KVal); }
    void SetCurrent4Dec(uint8_t KVal)  { SetParam8(0x0C, KVal); }
    void SetStallThreshold(uint8_t AValue)  { SetParam8(0x14, AValue); }
};
