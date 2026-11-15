/**
 * @file Fault.h
 * @brief Fatal-error handling for the radio platform.
 */

#ifndef PLATFORM_FAULT_H_
#define PLATFORM_FAULT_H_

namespace platform
{

/**
 * @brief Fatal-error handling.
 *
 * Like Clock, the platform owns exactly one instance, named `fault`. The struct
 * groups the behaviour needed to stop the firmware and make a crash visible.
 *
 * There is no state yet. The object still gives the fault path a clear name and
 * leaves room for future diagnostics, such as storing a fault code or the
 * stacked program counter.
 */
struct Fault
{
    /**
     * @brief Stop the firmware and show a fast backlight blink.
     *
     * Interrupts are disabled and the function never returns. The fast blink
     * pattern (~5 Hz) makes a crashed firmware recognizable without a debugger.
     *
     * This is public because the Cortex-M0+ hard-fault vector is a C function
     * and cannot be a member function.
     */
    [[noreturn]] void panic();
};

/** The single fault handler instance used by this platform. */
extern Fault fault;

} // namespace platform

/**
 * @brief Check a condition and enter the fault handler when it is false.
 *
 * This is a small replacement for the C library assert() macro.
 */
#define PLATFORM_ASSERT(condition)      \
    do {                                \
        if (!(condition)) {             \
            platform::fault.panic();    \
        }                               \
    } while (false)

#endif // PLATFORM_FAULT_H_
