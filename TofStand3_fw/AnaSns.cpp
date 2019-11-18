/*
 * AnaSns.cpp
 *
 *  Created on: 1 мая 2019 г.
 *      Author: Kreyl
 */

#include "ch.h"
#include "hal.h"
#include "kl_lib.h"
#include "board.h"
#include "MsgQ.h"
#include "shell.h"
#include "AnaSns.h"

#define ADC_MAX_VALUE       4095    // const: 2^12

const uint8_t AdcChannels[ADC_CHANNEL_CNT] = ADC_CHANNELS;
//static thread_reference_t ThdRef;
static const stm32_dma_stream_t *PDma;

static uint16_t IRawBuf[ADC_CHANNEL_CNT];
static uint32_t AverCnt = 0, IAverage[ADC_CHANNEL_CNT];

static Timer_t ITimer{ADC_TIM};

#if 1 // ============================ ADC Low Level ============================
// All 2xx, 4xx and F072 devices. Do not change.
#define ADC_CHNL_TEMPERATURE    16
#define ADC_CHNL_VREFINT        17
#define ADC_MAX_SEQ_LEN         16  // 1...16; Const, see ref man
// See datasheet, search VREFINT_CAL
#define ADC_VREFINT_CAL     (*(volatile uint16_t*)0x1FFF7A2A)   // for 4xx, ds p139

// ADC sampling_times
enum AdcSampleTime_t {
    ast3Cycles      = 0,
    ast15Cycles     = 1,
    ast28Cycles     = 2,
    ast56Cycles     = 3,
    ast84Cycles     = 4,
    ast112Cycles    = 5,
    ast144Cycles    = 6,
    ast480Cycles    = 7
};

enum ADCDiv_t {
    adcDiv2 = (uint32_t)(0b00 << 16),
    adcDiv4 = (uint32_t)(0b01 << 16),
    adcDiv6 = (uint32_t)(0b10 << 16),
    adcDiv8 = (uint32_t)(0b11 << 16),
};

static void SetChannelSampleTime(uint32_t AChnl, AdcSampleTime_t ASampleTime) {
    uint32_t Offset;
    if(AChnl <= 9) {
        Offset = AChnl * 3;
        ADC1->SMPR2 &= ~((uint32_t)0b111 << Offset);    // Clear bits
        ADC1->SMPR2 |= (uint32_t)ASampleTime << Offset; // Set new bits
    }
    else {
        Offset = (AChnl - 10) * 3;
        ADC1->SMPR1 &= ~((uint32_t)0b111 << Offset);    // Clear bits
        ADC1->SMPR1 |= (uint32_t)ASampleTime << Offset; // Set new bits
    }
}

static void SetSequenceItem(uint8_t SeqIndx, uint32_t AChnl) {
    uint32_t Offset;
    if(SeqIndx <= 6) {
        Offset = (SeqIndx - 1) * 5;
        ADC1->SQR3 &= ~(uint32_t)(0b11111 << Offset);
        ADC1->SQR3 |= (uint32_t)(AChnl << Offset);
    }
    else if(SeqIndx <= 12) {
        Offset = (SeqIndx - 7) * 5;
        ADC1->SQR2 &= ~(uint32_t)(0b11111 << Offset);
        ADC1->SQR2 |= (uint32_t)(AChnl << Offset);
    }
    else if(SeqIndx <= 16) {
        Offset = (SeqIndx - 13) * 5;
        ADC1->SQR1 &= ~(uint32_t)(0b11111 << Offset);
        ADC1->SQR1 |= (uint32_t)(AChnl << Offset);
    }
}

uint32_t Adc2mV(uint32_t AdcChValue, uint32_t VrefValue) {
    return ((3300UL * ADC_VREFINT_CAL / ADC_MAX_VALUE) * AdcChValue) / VrefValue;
}

// IRQ Handler
extern "C"
void AdcTxIrq(void *p, uint32_t flags) {
//    PinSetLo(GPIOB, 5);
    for(uint32_t i=0; i<ADC_CHANNEL_CNT; i++) IAverage[i] += IRawBuf[i];
    // Check if average done
    if(++AverCnt >= ADC_AVERAGE_CNT) {
        AverCnt = 0;
        for(uint32_t i=0; i<ADC_CHANNEL_CNT; i++) IAverage[i] /= ADC_AVERAGE_CNT;
        // Send data
//        PrintfI("%u %u\r", IAverage[ADC_VOLTAGE_INDX], IAverage[ADC_VREFINT_INDX]);
        int32_t v = VADC2Vmv(Adc2mV(IAverage[ADC_VOLTAGE_INDX], IAverage[ADC_VREFINT_INDX]));
        AnaOnMeasurementCompletedI(v);
        for(uint32_t i=0; i<ADC_CHANNEL_CNT; i++) IAverage[i] = 0;
    } // if average done
    PinSetLo(GPIOB, 5);
}
#endif

void AnaSnsInitAll() {
    // GPIOs
    PinSetupAnalog(VBUS_VOLTAGE);
    PinSetupAnalog(VBUS_CURRENT);

    // === ADC ===
    rccEnableADC1(FALSE);       // Enable digital clock
    // Setup ADCCLK
    ADC->CCR = (ADC->CCR & ~ADC_CCR_ADCPRE) | (uint32_t)ADC_CLK_DIVIDER;
    // Setup sequence length
    ADC1->SQR1 &= ~ADC_SQR1_L;  // Clear count
    ADC1->SQR1 |= (ADC_CHANNEL_CNT - 1) << 20;
    // Setup channels
    uint8_t SeqIndx = 1;    // First sequence item is 1, not 0
    for(uint8_t i=0; i < ADC_CHANNEL_CNT; i++) {
        SetChannelSampleTime(AdcChannels[i], ADC_SAMPLE_TIME);
        SetSequenceItem(SeqIndx++, AdcChannels[i]);
    }
    // Enable VRef
    ADC->CCR |= (uint32_t)ADC_CCR_TSVREFE;
    // Setup mode
    ADC1->CR1 = ADC_CR1_SCAN;               // Mode = scan
    ADC1->CR2 = ADC_CR2_DMA | ADC_CR2_ADON | ADC_CR2_DDS; // Enable DMA, enable ADC, continuous conversion

    // Timer to trigger
    ADC1->CR2 |= (0b0110UL << 24) | (0b01UL << 28); // TIM2 TRGO, ExtEn=rising edge
    ITimer.Init();
    ITimer.SetTopValue(Clk.GetTimInputFreq(ADC_TIM)/48000 - 1);
    ITimer.SetUpdateFrequencyChangingPrescaler(ADC_MEAS_FREQ_HZ);
    ITimer.SelectMasterMode(mmUpdate);

    // DEBUG: see Timer frequency at PA0
#if 0
    ITimer.SetCCR1(18);
    TIM2->BDTR = 0xC000;   // Main output Enable
    TIM2->CCMR1 |= (0b110 << 4);
    TIM2->CCER  |= TIM_CCER_CC1E;
    PinSetupAlterFunc(GPIOA, 0, omPushPull, pudNone, AF1);
#endif
    PinSetupOut(GPIOB,  5, omPushPull);
    ITimer.EnableIrqOnUpdate();
    ITimer.ClearUpdateIrqPendingBit();
    ITimer.EnableIrq(28, IRQ_PRIO_MEDIUM);

    ITimer.Enable();

    // DMA
    PDma = dmaStreamAlloc(ADC_DMA, IRQ_PRIO_LOW, AdcTxIrq, nullptr);
    dmaStreamSetPeripheral(PDma, &ADC1->DR);
    dmaStreamSetMode      (PDma, ADC_DMA_MODE);
    dmaStreamSetMemory0(PDma, IRawBuf);
    dmaStreamSetTransactionSize(PDma, ADC_CHANNEL_CNT);
    dmaStreamEnable(PDma);
}

extern "C"
void VectorB0() {
    CH_IRQ_PROLOGUE();
    TIM2->SR &= ~TIM_SR_UIF;
    PinSetHi(GPIOB, 5);
    CH_IRQ_EPILOGUE();
}
