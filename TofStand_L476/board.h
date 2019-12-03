/*
 * board.h
 *
 *  Created on: 05 08 2018
 *      Author: Kreyl
 */

#pragma once

// ==== General ====
#define BOARD_NAME          "ToFStand3"
#define APP_NAME            "ToFStand3"

// MCU type as defined in the ST header.
#define STM32L476xx

// Freq of external crystal if any. Leave it here even if not used.
#define CRYSTAL_FREQ_HZ         12000000

// OS timer settings
#define STM32_ST_IRQ_PRIORITY   2
#define STM32_ST_USE_TIMER      5
#define SYS_TIM_CLK             (Clk.APB1FreqHz)    // Timer 5 is clocked by APB1

//  Periphery
#define I2C1_ENABLED            FALSE
#define I2C2_ENABLED            FALSE
#define I2C3_ENABLED            FALSE
#define SIMPLESENSORS_ENABLED   TRUE
#define BUTTONS_ENABLED         TRUE

#define ADC_REQUIRED            FALSE
#define STM32_DMA_REQUIRED      TRUE    // Leave this macro name for OS

#if 1 // ========================== GPIO =======================================
// EXTI
#define INDIVIDUAL_EXTI_IRQ_REQUIRED    FALSE

// UART
#define UART_GPIO       GPIOB
#define UART_TX_PIN     6
#define UART_RX_PIN     7

#define UART_RPI_GPIO   GPIOC
#define UART_RPI_TX     10
#define UART_RPI_RX     11

// LED
#define LED_PIN         GPIOB, 8

// Buttons
#define BTN_UP_PIN      GPIOB, 0, pudPullDown
#define BTN_DOWN_PIN    GPIOB, 1, pudPullDown
#define BTN_FAST_PIN    GPIOB, 2, pudPullDown

// Sensors
#define CHAMBER_CLOSED_PIN  GPIOB, 10
#define USB_DETECT_PIN  GPIOA, 9, pudPullDown

// 7 segment
#define SEGS_SPI        SPI2
#define SEG_MOSI        GPIOB, 15, omPushPull, pudNone, AF5
#define SEG_CLK         GPIOB, 13, omPushPull, pudNone, AF5
#define SEG_DRV_LE      GPIOB, 12
#define SEG_DRV_OE      GPIOB, 14

// Endstops
#define ENDSTOP_TOP     GPIOA, 1
#define ENDSTOP_BOTTOM  GPIOA, 0
#define ENDSTOP_TOUCH   GPIOA, 2

#if 1 // ==== L6470 ====
#define M_SPI           SPI1
#define M_SPI_GPIO      GPIOA
#define M_SCK           5
#define M_MISO          6
#define M_MOSI          7
#define M_CS            8
#define M_SPI_AF        AF5

#define M_AUX_GPIO      GPIOC
#define M_BUSY_SYNC1    3
#define M_SW1           4
#define M_FLAG1         5
#define M_STBY_RST      2
#endif

// USB
#define USB_DM          GPIOA, 11
#define USB_DP          GPIOA, 12
#define USB_AF          AF10
#define BOARD_OTG_NOVBUSSENS    TRUE

#endif // GPIO


#if ADC_REQUIRED // ======================= Inner ADC ==========================
// Clock divider: clock is generated from the APB2
#define ADC_CLK_DIVIDER		adcDiv2

// ADC channels
#define ADC_BATTERY_CHNL 	14
// ADC_VREFINT_CHNL
#define ADC_CHANNELS        { ADC_BATTERY_CHNL, ADC_VREFINT_CHNL }
#define ADC_CHANNEL_CNT     2   // Do not use countof(AdcChannels) as preprocessor does not know what is countof => cannot check
#define ADC_SAMPLE_TIME     ast24d5Cycles
#define ADC_OVERSAMPLING_RATIO  64   // 1 (no oversampling), 2, 4, 8, 16, 32, 64, 128, 256
#endif

#if 1 // =========================== DMA =======================================
// ==== Uart ====
// Remap is made automatically if required
#define UART_DMA_TX     STM32_DMA_STREAM_ID(2, 6)
#define UART_DMA_RX     STM32_DMA_STREAM_ID(2, 7)
#define UART_DMA_CHNL   2
#define UART_DMA_TX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_LOW | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_M2P | STM32_DMA_CR_TCIE)
#define UART_DMA_RX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_MEDIUM | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_P2M | STM32_DMA_CR_CIRC)

#define UART_RPI_DMA_TX     STM32_DMA_STREAM_ID(2, 3)
#define UART_RPI_DMA_RX     STM32_DMA_STREAM_ID(2, 5)
#define UART_RPI_DMA_CHNL   2
#define UART_DMA_RPI_TX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_LOW | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_M2P | STM32_DMA_CR_TCIE)
#define UART_RPI_DMA_RX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_MEDIUM | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_P2M | STM32_DMA_CR_CIRC)

// ==== SPI ====
#define SEGS_SPI_DMA_TX STM32_DMA_STREAM_ID(1, 5)
#define SEGS_DMA_TX_CHNL   1
#define SEGS_DMA_TX_MODE  \
        (STM32_DMA_CR_CHSEL(SEGS_DMA_TX_CHNL) | \
        DMA_PRIORITY_LOW |     \
        STM32_DMA_CR_MSIZE_BYTE | \
        STM32_DMA_CR_PSIZE_BYTE | \
        STM32_DMA_CR_TCIE | \
        STM32_DMA_CR_MINC |    /* Mem pointer increase */  \
        STM32_DMA_CR_DIR_M2P   /* Mem to peripheral */ )

#endif // DMA

#if 1 // ========================== USART ======================================
#define PRINTF_FLOAT_EN FALSE
#define UART_TXBUF_SZ   4096
#define UART_RXBUF_SZ   99

#define UARTS_CNT       2

#define CMD_UART_PARAMS \
    USART1, UART_GPIO, UART_TX_PIN, UART_GPIO, UART_RX_PIN, \
    UART_DMA_TX, UART_DMA_RX, UART_DMA_TX_MODE(UART_DMA_CHNL), UART_DMA_RX_MODE(UART_DMA_CHNL), uartclkHSI

#define RPI_UART_PARAMS \
    UART4, UART_RPI_GPIO, UART_RPI_TX, UART_RPI_GPIO, UART_RPI_RX, \
    UART_RPI_DMA_TX, UART_RPI_DMA_RX, \
    UART_DMA_RPI_TX_MODE(UART_RPI_DMA_CHNL), UART_RPI_DMA_RX_MODE(UART_RPI_DMA_CHNL), uartclkHSI

#endif
