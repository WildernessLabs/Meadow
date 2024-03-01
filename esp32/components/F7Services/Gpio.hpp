/*
 *  Gpio.hpp
 *
 *  Constants defining the pins on the ESP32 that are
 *  available to Meadow.
 */

#ifndef _GPIO_HPP_
#define _GPIO_HPP_

#include "sdkconfig.h"

#define USE_MEADOW

#include "esp_system.h"
#include "driver/gpio.h"

/**
 * The Gpio namespace contains the definitions of the constants that define the GPIO
 * pins along with their purpose.
 */
namespace Gpio
{
    /**
     *  UART Tx pin for the logging output.
     */
    const gpio_num_t LOGGING_TX_PIN = GPIO_NUM_1;

    /**
     *  UART Rx pin for the logging output.
     *
     *  Note that the pin used here is not actually connected to
     *  anything but the pin is necessary to setup the logging UART.
     */
    const gpio_num_t LOGGING_RX_PIN = GPIO_NUM_21;

    /**
     *  ESP32 Boot pin.
     */
    const gpio_num_t BOOT_PIN = GPIO_NUM_0;

    /**
     *  Usual GPIO UART0 Rx line.
     *
     *  This pin is not used in the application for UART communication as
     *  it is being used for signalling.
     */
    const gpio_num_t UART0_RX_PIN = GPIO_NUM_3;

    /**
     *  GPIO pin connected to UART0 Tx line.
     */
    const gpio_num_t UART0_TX_PIN = GPIO_NUM_1;

    /**
     *  Pin used to indicate that the ESP32 SPI interface is ready
     *  to receive / transmit data.
     */
    const gpio_num_t ESP32_SPI_READY_PIN = BOOT_PIN;

    /**
     *  Pin used to indicate that the ESP32 has a message waiting
     *  to be transmitted to the STM32.
     */
    const gpio_num_t ESP32_MESSAGE_WAITING_PIN = UART0_RX_PIN;

    /**
     *  SPI MISO pin.
     */
    const gpio_num_t MISO_PIN = GPIO_NUM_19;

    /**
     *  SPI MOSI pin.
     */
    const gpio_num_t MOSI_PIN = GPIO_NUM_23;

    /**
     *  SPI Clock pin.
     */
    const gpio_num_t CLOCK_PIN = GPIO_NUM_18;

    /**
     *  SPI Chip Select pin.
     */
    const gpio_num_t CHIP_SELECT_PIN = GPIO_NUM_5;
}

#endif /* _GPIO_HPP_ */
