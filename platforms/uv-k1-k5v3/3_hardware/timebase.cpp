/**
 * @file timebase.cpp
 * @brief 1 ms timebase built on the ARM SysTick timer.
 */

#include "timebase.h"
#include "py32f0xx.h"

namespace platform {
    /** Milliseconds since boot. Written by the interrupt, read by the main loop. */
    volatile uint32_t g_millis = 0;

    void time_init() {
        // SysTick counts core clock cycles. SystemCoreClock is 48,000,000, so this
        // loads 47,999 and produces one interrupt every millisecond.
        const uint32_t ticks_per_millisecond = SystemCoreClock / 1000U;

        // SysTick_Config also enables the interrupt and selects the core clock as
        // its source. It returns non-zero only if the reload value does not fit in
        // the 24-bit register, which cannot happen with the values used here.
        (void) SysTick_Config(ticks_per_millisecond);

        // A timebase is a system service, so give it a high priority.
        NVIC_SetPriority(SysTick_IRQn, 0);
    }

    uint32_t millis() {
        // A 32-bit read is atomic on Cortex-M0+, so no critical section is needed.
        return g_millis;
    }

    bool time_reached(uint32_t now, uint32_t deadline) {
        // Subtracting unsigned values and interpreting the result as signed makes
        // the comparison correct even when the counter wraps from 0xFFFFFFFF to 0.
        return static_cast<int32_t>(now - deadline) >= 0;
    }
} // namespace platform

/**
 * @brief SysTick interrupt handler, called once per millisecond.
 *
 * The name and C linkage matter: the startup file's vector table refers to the
 * symbol "SysTick_Handler". C++ would otherwise mangle the name.
 */
extern "C" void SysTick_Handler(void) {
    ++g_millis;
}
