/*
 * 7segment.cpp
 *
 *  Created on: Oct 2, 2019
 *      Author: laurelindo
 */

#include "7segment.h"
#include "board.h"
#include "kl_lib.h"

#if 1 // ============================ Char Gen =================================
#define DIGIT_CNT   4

static const uint8_t Char1Gen[10][4] = { // Digits
        {0xB7, 0xD7, 252, 219}, // 0
        {0x14, 0x05,  24,   9}, // 1
        {0x73, 0xE6, 205, 234}, // 2
        {0x76, 0x67,  93, 107}, // 3
        {0xD4, 0x35,  57,  57}, // 4
        {0xE6,  115, 117, 115}, // 5
        {0xC7,  241, 241, 241}, // 6
        {0x34,    7,  28,  11}, // 7
        {0xF7,  247, 253, 251}, // 8
        {0xF4,   55,  61,  59}, // 9
};

#define SHOW_NONE   0xFF
#define SHOW_t      0x74

static const uint8_t CharGen_t[4] = {195, 240, 225, 240};

static const uint8_t PointSegment[DIGIT_CNT] = {4, 2, 8, 8};

#endif

static const Spi_t ISpi{LEDS_SPI};
static const stm32_dma_stream_t *IDma;

static uint8_t IBuf[DIGIT_CNT];

static void IDmaDone(void *p, uint32_t flags) {
    PinSetHi(LED_DRV_LE);
    PinSetLo(LED_DRV_LE);
}

static void ShowBuf() {
    dmaStreamDisable(IDma);
    dmaStreamSetTransactionSize(IDma, DIGIT_CNT);
    dmaStreamSetMode(IDma, LEDS_DMA_TX_MODE);
    dmaStreamSetMemory0(IDma, IBuf);
    dmaStreamEnable(IDma);
}

void SegmentInit() {
    // Control pins
    PinSetupOut(LED_DRV_OE, omPushPull);
    PinSetHi(LED_DRV_OE);
    PinSetupOut(LED_DRV_LE, omPushPull);
    PinSetLo(LED_DRV_LE);
    // SPI
    PinSetupAlterFunc(LED_CLK);
    PinSetupAlterFunc(LED_MOSI);
    ISpi.Setup(boMSB, cpolIdleLow, cphaFirstEdge, 50000000);
    IDma = dmaStreamAlloc(LEDS_SPI_DMA_TX, IRQ_PRIO_LOW, IDmaDone, nullptr);
    dmaStreamSetPeripheral(IDma, &LEDS_SPI->DR);
    dmaStreamSetMode(IDma, LEDS_DMA_TX_MODE);
    ISpi.EnableTxDma();
    ISpi.Enable();
    // CLS
    for(int i=0; i<DIGIT_CNT; i++) IBuf[i] = 0;
    ShowBuf();
    PinSetLo(LED_DRV_OE);
}

void SegmentShow(uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4, uint8_t PointMsk) {
    IBuf[0] = v4;
    IBuf[1] = v3;
    IBuf[2] = v2;
    IBuf[3] = v1;

    for(int i=0; i<4; i++) {
        if(IBuf[i] == SHOW_NONE) IBuf[i] = 0; // Hide it
        else if(IBuf[i] == SHOW_t) IBuf[i] = CharGen_t[3-i];
        else {
            IBuf[i] = Char1Gen[IBuf[i]][3-i];
            if(PointMsk & (1 << i)) IBuf[i] += PointSegment[i];
        }
    }

    ShowBuf();
}

void SegmentPutUint(uint32_t n, uint32_t base, uint8_t PointMsk) {
    uint8_t digits[DIGIT_CNT] = {SHOW_NONE, SHOW_NONE, SHOW_NONE, SHOW_NONE};
    uint32_t len = 0;
    // Place digits to buffer
    do {
        digits[len++] = n % base;
        n /= base;
    } while(n > 0);
    // Print digits
    SegmentShow(digits[3], digits[2], digits[1], digits[0], PointMsk);
}


void SegmentShowT(uint32_t N) {
    uint8_t dig4 = N % 10;
    N /= 10;
    uint8_t dig3 = N % 10;
    SegmentShow(SHOW_t, SHOW_NONE, dig3, dig4);
}


void SegmentShowGood() {
    IBuf[3] = 167;
    IBuf[2] = 225;
    IBuf[1] = 209;
    IBuf[0] = 233;
    ShowBuf();
}

void SegmentShowBad() {
    IBuf[3] = 0;
    IBuf[2] = 241;
    IBuf[1] = 189;
    IBuf[0] = 233;
    ShowBuf();
}

void SegmentShowPoints(uint8_t PointMsk) {
    for(int i=0; i<4; i++) {
        uint8_t PointSeg = PointSegment[i];
        if(PointMsk & (1 << i)) IBuf[i] |= PointSeg;
        else IBuf[i] &= ~PointSeg;
    }
    ShowBuf();
}
