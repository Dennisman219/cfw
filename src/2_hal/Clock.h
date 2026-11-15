/**
 * @file Clock.h
 * @brief Portable millisecond clock role.
 *
 * Applications use this interface for timekeeping. The platform implementation
 * in 2_hal/Clock.cpp decides which hardware timer provides the ticks; on the
 * UV-K1 / UV-K5(8) v3 that is the Cortex-M0+ SysTick timer.
 */

#ifndef HAL_CLOCK_H_
#define HAL_CLOCK_H_

#include <stdint.h>

namespace platform
{

/**
 * @brief Millisecond clock.
 *
 * A struct is used here because the clock has both state (the tick counter)
 * and behaviour (starting the timer, reading the time and checking deadlines).
 * Grouping them makes it clear that the counter and the functions operating on
 * it belong together.
 *
 * The platform owns exactly one instance, named `clock`, declared below. Code
 * that needs the current time uses:
 *
 *     const uint32_t now = platform::clock.millis();
 *     if (platform::clock.reached(now + 50)) {
 *         // 50 ms have passed
 *     }
 */
struct Clock
{
    /**
     * @brief Start the 1 ms timebase.
     *
     * Must be called after SystemCoreClock has a valid value (the platform sets
     * it to 48 MHz, matching the bootloader's clock configuration).
     */
    void init();

    /**
     * @brief Milliseconds since init().
     *
     * The counter wraps around after roughly 49.7 days. Always compare
     * deadlines with reached() instead of writing "now >= deadline"; the latter
     * breaks when the counter wraps.
     */
    uint32_t millis() const;

    /**
     * @brief Check whether a deadline has been reached, safely across wrap.
     *
     * @param deadline Absolute time returned by an earlier millis() call plus a
     *                 delay, for example: millis() + 50.
     */
    bool reached(uint32_t deadline) const;

    /**
     * @brief Wait for a number of milliseconds.
     *
     * This is a busy wait. Use it only for the short delays needed by hardware
     * initialization.
     */
    void delay(uint32_t milliseconds) const;

    /**
     * @brief Advance the counter by one millisecond.
     *
     * Called by the platform's timer interrupt handler. This is public because
     * the interrupt handler is a C function and cannot be a member function.
     */
    void tick();

private:
    /** Milliseconds since boot. Written by the interrupt, read by the main loop. */
    volatile uint32_t ticks_ = 0;
};

/** The single clock instance used by the firmware. */
extern Clock clock;

} // namespace platform

#endif // HAL_CLOCK_H_
