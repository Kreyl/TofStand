/*
 * board.h
 *
 *  Created on: 12 сент. 2015 г.
 *      Author: Kreyl
 */

#pragma once

// ==== General ====
#define BOARD_NAME          "ToFStand3"
#define APP_NAME            "ToFStand3"

// MCU type as defined in the ST header.
#define STM32F205xx

// Freq of external crystal if any. Leave it here even if not used.
#define CRYSTAL_FREQ_HZ 12000000

// OS timer settings
#define STM32_ST_IRQ_PRIORITY   2
#define STM32_ST_USE_TIMER      5
#define SYS_TIM_CLK             (Clk.GetTimInputFreq(TIM5))

//  Periphery
#define I2C1_ENABLED            FALSE
#define I2C2_ENABLED            FALSE
#define I2C3_ENABLED            FALSE
#define SIMPLESENSORS_ENABLED   TRUE
#define BUTTONS_ENABLED         TRUE

#define ADC_REQUIRED            FALSE

#if 1 // ========================== GPIO =======================================
// EXTI
#define INDIVIDUAL_EXTI_IRQ_REQUIRED    FALSE

// UART
#define UART_GPIO       GPIOB
#define UART_TX_PIN     6
#define UART_RX_PIN     7

// LED
#define LED_PIN         GPIOB, 8

// Buttons
#define BTN_UP_PIN      GPIOB, 0, pudPullDown
#define BTN_DOWN_PIN    GPIOB, 1, pudPullDown
#define BTN_FAST_PIN    GPIOB, 2, pudPullDown

// 7 segment
#define SEG_MOSI        GPIOB, 15, omPushPull, pudNone, AF5
#define SEG_CLK         GPIOB, 13, omPushPull, pudNone, AF5
#define SEG_DRV_LE      GPIOB, 12
#define SEG_DRV_OE      GPIOB, 14

#endif // GPIO

#if 1 // =========================== SPI =======================================
#define SEGS_SPI        SPI2
#endif

#if I2C2_ENABLED // ====================== I2C ================================
#define I2C2_BAUDRATE   400000
#define I2C_PILL        i2c2
#endif

#if ADC_REQUIRED // ======================= Inner ADC ==========================
// Measuremen timing src
#define ADC_TIM             TIM2

// ADC channels
#define ADC_VOLTAGE_CH      4
#define ADC_CURRENT_CH      6
#define ADC_VREFINT_CH      17 // Constant

#define ADC_CHANNELS        { ADC_VOLTAGE_CH, ADC_CURRENT_CH, ADC_VREFINT_CH }
// Index in array
#define ADC_VOLTAGE_INDX    0
#define ADC_CURRENT_INDX    1
#define ADC_VREFINT_INDX    2

#define ADC_CHANNEL_CNT     3   // Do not use countof(AdcChannels) as preprocessor does not know what is countof => cannot check
#endif

#if 1 // =========================== DMA =======================================
#define STM32_DMA_REQUIRED  TRUE
// ==== Uart ====
// Remap is made automatically if required
#define UART_DMA_TX     STM32_DMA_STREAM_ID(2, 7)
#define UART_DMA_RX     STM32_DMA_STREAM_ID(2, 5)
#define UART_DMA_CHNL   4
#define UART_DMA_TX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_LOW | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_M2P | STM32_DMA_CR_TCIE)
#define UART_DMA_RX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_MEDIUM | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_P2M | STM32_DMA_CR_CIRC)

// ==== SPI ====
#define SEGS_SPI_DMA_TX STM32_DMA_STREAM_ID(1, 4)
#define SEGS_DMA_TX_CHNL   0
#define SEGS_DMA_TX_MODE  \
        (STM32_DMA_CR_CHSEL(SEGS_DMA_TX_CHNL) | \
        DMA_PRIORITY_LOW |     \
        STM32_DMA_CR_MSIZE_BYTE | \
        STM32_DMA_CR_PSIZE_BYTE | \
        STM32_DMA_CR_TCIE | \
        STM32_DMA_CR_MINC |    /* Mem pointer increase */  \
        STM32_DMA_CR_DIR_M2P   /* Mem to peripheral */ )

#if I2C1_ENABLED // ==== I2C1 ====
#define I2C1_DMA_TX     STM32_DMA1_STREAM6
#define I2C1_DMA_RX     STM32_DMA1_STREAM5
#endif
#if I2C2_ENABLED // ==== I2C2 ====
#define I2C2_DMA_TX     STM32_DMA1_STREAM7
#define I2C2_DMA_RX     STM32_DMA1_STREAM3
#endif

#if ADC_REQUIRED
#define ADC_DMA         STM32_DMA_STREAM_ID(2, 0)
#define ADC_DMA_MODE    STM32_DMA_CR_CHSEL(0) |   /* DMA2 Stream0 Channel0 */ \
                        DMA_PRIORITY_LOW | \
                        STM32_DMA_CR_MSIZE_HWORD | \
                        STM32_DMA_CR_PSIZE_HWORD | \
                        STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                        STM32_DMA_CR_DIR_P2M |    /* Direction is peripheral to memory */ \
                        STM32_DMA_CR_CIRC |       /* Circulate */ \
                        STM32_DMA_CR_TCIE         /* Enable Transmission Complete IRQ */
#endif // ADC

#endif // DMA

#if 1 // ========================== USART ======================================
#define PRINTF_FLOAT_EN FALSE
#define UART_TXBUF_SZ   8192
#define UART_RXBUF_SZ   99

#define UARTS_CNT       1

#define CMD_UART_PARAMS \
    USART1, UART_GPIO, UART_TX_PIN, UART_GPIO, UART_RX_PIN, \
    UART_DMA_TX, UART_DMA_RX, UART_DMA_TX_MODE(UART_DMA_CHNL), UART_DMA_RX_MODE(UART_DMA_CHNL)

#endif
