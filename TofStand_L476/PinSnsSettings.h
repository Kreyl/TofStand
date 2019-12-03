/*
 * SnsPins.h
 *
 *  Created on: 17 џэт. 2015 у.
 *      Author: Kreyl
 */

/* ================ Documentation =================
 * There are several (may be 1) groups of sensors (say, buttons and USB connection).
 *
 */

#pragma once

#include "SimpleSensors.h"

#ifndef SIMPLESENSORS_ENABLED
#define SIMPLESENSORS_ENABLED   FALSE
#endif

#if SIMPLESENSORS_ENABLED
#define SNS_POLL_PERIOD_MS      63

// Handlers
extern void ProcessButtons(PinSnsState_t *PState, uint32_t Len);
extern void ProcessChamberClosed(PinSnsState_t *PState, uint32_t Len);
extern void ProcessUsbConnect(PinSnsState_t *PState, uint32_t Len);

const PinSns_t PinSns[] = {
        // Buttons
        {BTN_UP_PIN, ProcessButtons},
        {BTN_DOWN_PIN, ProcessButtons},
        {BTN_FAST_PIN, ProcessButtons},
        // Chamber closed
        {CHAMBER_CLOSED_PIN, pudPullDown, ProcessChamberClosed},
        // USB
        {USB_DETECT_PIN, ProcessUsbConnect},
};
#define PIN_SNS_CNT     countof(PinSns)

#endif  // if enabled
