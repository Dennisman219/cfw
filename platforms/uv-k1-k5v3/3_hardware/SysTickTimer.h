/**
 * @file SysTickTimer.h
 * @brief Hardware proxy for the ARM Cortex-M0+ SysTick timer.
 *
 * The SysTick timer is part of the processor core rather than a Puya
 * peripheral, so this proxy works with the CMSIS register definitions. It
 * exposes the timer's configuration and control operations; the HAL clock
 * decides how often the timer should interrupt and what to do with the ticks.
 */

#ifndef PLATFORM_SYSTICK_TIMER_H_
#define PLATFORM_SYSTICK_TIMER_H_

#include <stdint.h>

namespace platform
{

/**
 * @brief Driver for the Cortex-M0+ SysTick down-counter.
 *
 * The class is called SysTickTimer rather than SysTick because CMSIS uses
 * `SysTick` as the name of the peripheral register block macro.
 */
struct SysTickTimer
{
    /**
     * @brief Configure the reload value and select the processor clock.
     *
     * The counter and its interrupt stay disabled. Call enable_interrupt() and
     * enable() afterwards to start the timer.
     *
     * @param cycles Number of processor clock cycles between two counter
     *               zero events.
     * @return true on success, false if cycles does not fit the 24-bit reload
     *         register (or is zero).
     */
    bool configure(uint32_t cycles);

    /** Start the down-counter. */
    void enable();

    /** Stop the down-counter. */
    void disable();

    /** Enable the interrupt that fires when the counter reaches zero. */
    void enable_interrupt();

    /** Disable the counter-zero interrupt. */
    void disable_interrupt();

    /**
     * @brief Set the NVIC priority of the SysTick interrupt.
     *
     * @param priority 0 is the highest priority. The number of usable priority
     *                 levels depends on the NVIC configuration.
     */
    void set_interrupt_priority(uint32_t priority);

    /**
     * @brief Clear the counter-zero flag.
     *
     * Reading the control register clears the flag, so this is just a read.
     */
    void clear_count_flag();

    /**
     * @brief Read and clear the counter-zero flag.
     *
     * @return true when the counter has reached zero since the last read.
     */
    bool count_flag();

    /** @return the current counter value. */
    uint32_t counter() const;

    /**
     * @brief Block for a short number of microseconds using the SysTick counter.
     *
     * This is meant for bit-banged bus timing, where a delay of a few
     * microseconds is needed between GPIO edges. The timer must already be
     * configured and running. This is a busy wait, so keep the requested delay
     * short.
     */
    void delay_us(uint32_t microseconds);

    /**
     * @brief Block for a number of milliseconds using the SysTick counter.
     *
     * This is a hardware-level busy wait, so it works even if the HAL clock or
     * its interrupt has not been started. It waits for one counter-zero event
     * per millisecond, which requires the timer to be configured for a 1 ms
     * period.
     */
    void delay_ms(uint32_t milliseconds);

private:
    /** @return true when the count flag is set (reading clears it). */
    bool take_count_flag();
};

/** The single SysTick timer instance used by this platform. */
extern SysTickTimer systick;

} // namespace platform

#endif // PLATFORM_SYSTICK_TIMER_H_
