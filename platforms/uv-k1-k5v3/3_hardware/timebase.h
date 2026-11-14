/**
 * @file timebase.h
 * @brief Millisecond timebase provided by the Cortex-M0+ SysTick timer.
 */

#ifndef PLATFORM_TIMEBASE_H_
#define PLATFORM_TIMEBASE_H_

#include <stdint.h>

namespace platform
{

/**
 * @brief Start the 1 ms SysTick interrupt.
 *
 * Must be called after SystemCoreClock has a valid value (the platform sets it
 * to 48 MHz, matching the bootloader's clock configuration).
 */
void time_init();

/**
 * @brief Milliseconds since time_init().
 *
 * The counter wraps around after roughly 49.7 days. Always compare deadlines
 * with time_reached() instead of writing "now >= deadline"; the latter breaks
 * when the counter wraps.
 */
uint32_t millis();

/**
 * @brief Check whether a deadline has been reached, safely across wrap-around.
 *
 * @param now      Current value from millis().
 * @param deadline Absolute time returned by an earlier millis() call plus a
 *                 delay, for example: millis() + 50.
 */
bool time_reached(uint32_t now, uint32_t deadline);

} // namespace platform

#endif // PLATFORM_TIMEBASE_H_
