/**
 * @file Clock.cpp
 * @brief UV-K1 / UV-K5(8) v3 implementation of the portable millisecond clock.
 *
 * The SysTick hardware proxy configures the timer peripheral. This file
 * decides that it should interrupt once per millisecond and turns those
 * interrupts into a millisecond counter for the rest of the firmware.
 */

#include "Clock.h"

#include "Fault.h"
#include "SysTickTimer.h"
#include "py32f0xx.h"

namespace platform
{

/** Definition of the single clock instance declared in Clock.h. */
Clock clock;

void Clock::init()
{
    // SysTick counts processor clock cycles. SystemCoreClock is 48,000,000, so
    // this produces one interrupt every millisecond.
    const uint32_t cycles_per_millisecond = SystemCoreClock / 1000U;

    if (!systick.configure(cycles_per_millisecond)) {
        // The reload value did not fit the 24-bit register. This cannot happen
        // with the clock used here, but a failed timebase is fatal.
        fault.panic();
    }

    // The clock is a system service, so give it a high priority.
    systick.set_interrupt_priority(0);
    systick.enable_interrupt();
    systick.enable();
}

uint32_t Clock::millis() const
{
    // A 32-bit read is atomic on Cortex-M0+, so no critical section is needed.
    return ticks_;
}

bool Clock::reached(uint32_t deadline) const
{
    // Subtracting unsigned values and interpreting the result as signed makes
    // the comparison correct even when the counter wraps from 0xFFFFFFFF to 0.
    return static_cast<int32_t>(millis() - deadline) >= 0;
}

void Clock::delay(uint32_t milliseconds) const
{
    systick.delay_ms(milliseconds);
}

void Clock::tick()
{
    ++ticks_;
}

} // namespace platform

/**
 * @brief SysTick interrupt handler, called once per millisecond.
 *
 * The name and C linkage matter: the startup file's vector table refers to the
 * symbol "SysTick_Handler". C++ would otherwise mangle the name.
 */
extern "C" void SysTick_Handler(void)
{
    platform::clock.tick();
}
