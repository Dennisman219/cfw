/**
 * @file fault.h
 * @brief Fatal-error handling for the radio platform.
 */

#ifndef PLATFORM_FAULT_H_
#define PLATFORM_FAULT_H_

namespace platform
{

/**
 * @brief Stop the firmware and show a fast backlight blink.
 *
 * Interrupts are disabled and the function never returns. The fast blink
 * pattern (~5 Hz) makes a crashed firmware recognizable without a debugger.
 */
[[noreturn]] void panic();

} // namespace platform

/**
 * @brief Check a condition and enter the fault handler when it is false.
 *
 * This is a small replacement for the C library assert() macro. It does not
 * print a message because milestone 1 has no console; attach a debugger or
 * inspect the call site if the fault blink appears.
 */
#define PLATFORM_ASSERT(condition)      \
    do {                                \
        if (!(condition)) {             \
            platform::panic();          \
        }                               \
    } while (false)

#endif // PLATFORM_FAULT_H_
