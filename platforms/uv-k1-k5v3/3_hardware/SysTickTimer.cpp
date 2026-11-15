/**
 * @file SysTickTimer.cpp
 * @brief Hardware proxy for the ARM Cortex-M0+ SysTick timer.
 */

#include "SysTickTimer.h"

#include "py32f0xx.h"

namespace platform
{

/** Definition of the single SysTick timer instance declared in SysTickTimer.h. */
SysTickTimer systick;

bool SysTickTimer::configure(uint32_t cycles)
{
    // LOAD is 24 bits wide. A period of `cycles` clocks needs LOAD = cycles - 1
    // because the counter also spends one clock at zero before reloading.
    if (cycles == 0 || cycles > 0x01000000U) {
        return false;
    }

    // Stop the timer and disable its interrupt while the registers change.
    SysTick->CTRL = 0;
    SysTick->LOAD = cycles - 1U;

    // Writing any value to VAL clears the counter and the count flag.
    SysTick->VAL = 0;

    // Use the processor clock (48 MHz on this platform). Interrupt and counter
    // stay off until the caller enables them.
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk;
    return true;
}

void SysTickTimer::enable()
{
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
}

void SysTickTimer::disable()
{
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
}

void SysTickTimer::enable_interrupt()
{
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

void SysTickTimer::disable_interrupt()
{
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
}

void SysTickTimer::set_interrupt_priority(uint32_t priority)
{
    NVIC_SetPriority(SysTick_IRQn, priority);
}

void SysTickTimer::clear_count_flag()
{
    // The COUNTFLAG bit is cleared by reading the control register.
    (void)SysTick->CTRL;
}

bool SysTickTimer::take_count_flag()
{
    return (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0;
}

bool SysTickTimer::count_flag()
{
    return take_count_flag();
}

uint32_t SysTickTimer::counter() const
{
    return SysTick->VAL;
}

void SysTickTimer::delay_us(uint32_t microseconds)
{
    if (microseconds == 0) {
        return;
    }

    // SysTick counts at the processor clock. At 48 MHz one microsecond is 48
    // counter ticks.
    const uint32_t ticks_per_microsecond = SystemCoreClock / 1000000U;
    uint32_t remaining = microseconds * ticks_per_microsecond;

    // The counter counts down and reloads after zero. Remember how many counts
    // one full period contains so a wrap can be detected.
    const uint32_t period = SysTick->LOAD + 1U;
    uint32_t previous = SysTick->VAL;

    while (remaining > 0) {
        const uint32_t current = SysTick->VAL;

        // Ignore identical readings. Without this a very fast loop could keep
        // sampling the same counter value and never make progress.
        if (current == previous) {
            continue;
        }

        // A smaller value means the counter went down without wrapping. A
        // larger value means it reached zero and reloaded since the last read.
        uint32_t elapsed;
        if (current < previous) {
            elapsed = previous - current;
        } else {
            elapsed = previous + (period - current);
        }

        previous = current;
        if (elapsed >= remaining) {
            remaining = 0;
        } else {
            remaining -= elapsed;
        }
    }
}

void SysTickTimer::delay_ms(uint32_t milliseconds)
{
    // Drop a flag left over from before the delay started, so the first wait is
    // a full millisecond instead of returning immediately.
    clear_count_flag();

    while (milliseconds > 0) {
        while (!take_count_flag()) {
        }
        --milliseconds;
    }
}

} // namespace platform
