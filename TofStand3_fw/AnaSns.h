/*
 * AnaSns.h
 *
 *  Created on: 1 мая 2019 г.
 *      Author: Kreyl
 */

#pragma once

// Clock divider: clock is generated from the APB2
#define ADC_CLK_DIVIDER     adcDiv2
#define ADC_SAMPLE_TIME     ast56Cycles

#define ADC_MEAS_FREQ_HZ    8000
#define ADC_AVERAGE_CNT     8   // How many times to measure every channel

#define VDIV_Rup            10
#define VDIV_Rdown          1
#define VADC2Vmv(VADC)      ((VADC * (VDIV_Rup + VDIV_Rdown)) / VDIV_Rdown)

void AnaSnsInitAll();
void AnaOnMeasurementCompletedI(uint32_t v); // Implement it where needed
