/**
 * @file Gpio.h
 * @brief Small helpers for configuring and using GPIO pins.
 *
 * The Puya LL library has one important trap: most of its "set pin" functions
 * accept exactly one pin, even when the parameter is called PinMask. They
 * calculate the register position with expressions such as Pin * Pin, which
 * only works for a single bit. Passing a combined mask like
 *
 *     LL_GPIO_PIN_3 | LL_GPIO_PIN_4
 *
 * would silently modify the wrong bits.
 *
 * These helpers accept combined masks and split them into one call per pin, so
 * board and driver code can stay readable without repeating that loop.
 */

#ifndef PLATFORM_GPIO_H_
#define PLATFORM_GPIO_H_

#include <stdint.h>

#include "py32f071_ll_gpio.h"

namespace platform
{
namespace gpio
{

/**
 * @brief Configure one or more pins as push-pull outputs.
 *
 * Set the output latch to the desired idle level *before* calling this, for
 * example with set_high() or set_low(). The pin then starts at the correct
 * level instead of briefly glitching when it becomes an output.
 */
inline void configure_output(GPIO_TypeDef* port, uint32_t pins)
{
    for (uint32_t bit = 0; bit < 16; ++bit) {
        const uint32_t pin = 1UL << bit;
        if ((pins & pin) == 0) {
            continue;
        }

        LL_GPIO_SetPinMode(port, pin, LL_GPIO_MODE_OUTPUT);
        LL_GPIO_SetPinOutputType(port, pin, LL_GPIO_OUTPUT_PUSHPULL);
        LL_GPIO_SetPinSpeed(port, pin, LL_GPIO_SPEED_FREQ_VERY_HIGH);
        LL_GPIO_SetPinPull(port, pin, LL_GPIO_PULL_NO);
    }
}

/**
 * @brief Configure one or more pins as inputs.
 *
 * @param pull LL_GPIO_PULL_UP, LL_GPIO_PULL_DOWN or LL_GPIO_PULL_NO.
 */
inline void configure_input(GPIO_TypeDef* port, uint32_t pins, uint32_t pull)
{
    for (uint32_t bit = 0; bit < 16; ++bit) {
        const uint32_t pin = 1UL << bit;
        if ((pins & pin) == 0) {
            continue;
        }

        LL_GPIO_SetPinMode(port, pin, LL_GPIO_MODE_INPUT);
        LL_GPIO_SetPinPull(port, pin, pull);
    }
}

/**
 * @brief Configure one or more pins as analog inputs.
 *
 * Analog mode disconnects the digital input buffer, which the ADC needs for an
 * accurate measurement.
 */
inline void configure_analog(GPIO_TypeDef* port, uint32_t pins)
{
    for (uint32_t bit = 0; bit < 16; ++bit) {
        const uint32_t pin = 1UL << bit;
        if ((pins & pin) == 0) {
            continue;
        }

        LL_GPIO_SetPinMode(port, pin, LL_GPIO_MODE_ANALOG);
        LL_GPIO_SetPinPull(port, pin, LL_GPIO_PULL_NO);
    }
}

/**
 * @brief Configure one pin as an alternate-function output.
 *
 * @param pin       A single pin from LL_GPIO_PIN_0 to LL_GPIO_PIN_7. The
 *                  register layout makes a combined mask invalid here.
 * @param alternate One of the LL_GPIO_AFx values, for example
 *                  LL_GPIO_AF0_SPI1.
 */
inline void configure_alternate(GPIO_TypeDef* port, uint32_t pin, uint32_t alternate)
{
    LL_GPIO_SetPinMode(port, pin, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinOutputType(port, pin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(port, pin, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(port, pin, LL_GPIO_PULL_NO);
    LL_GPIO_SetAFPin_0_7(port, pin, alternate);
}

/** Drive the selected pins high. Combined masks are fine here. */
inline void set_high(GPIO_TypeDef* port, uint32_t pins)
{
    LL_GPIO_SetOutputPin(port, pins);
}

/** Drive the selected pins low. Combined masks are fine here. */
inline void set_low(GPIO_TypeDef* port, uint32_t pins)
{
    LL_GPIO_ResetOutputPin(port, pins);
}

/** @return true when the input pin is high. */
inline bool read(GPIO_TypeDef* port, uint32_t pin)
{
    return LL_GPIO_IsInputPinSet(port, pin) != 0;
}

/** @return the state of all input pins on the port, one bit per pin. */
inline uint32_t read_port(GPIO_TypeDef* port)
{
    return LL_GPIO_ReadInputPort(port);
}

} // namespace gpio
} // namespace platform

#endif // PLATFORM_GPIO_H_
