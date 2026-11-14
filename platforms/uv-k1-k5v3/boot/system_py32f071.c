/**
 * @file system_py32f071.c
 * @brief Minimal system setup for the PY32F071xB on the UV-K1 / UV-K5(8) v3.
 *
 * This file replaces the vendor system file with only what this firmware
 * needs. Keeping it small makes it obvious what happens before main() runs.
 *
 * Startup order in boot/startup_py32f071xx.s:
 *   1. set the stack pointer,
 *   2. call SystemInit() (this file),
 *   3. copy .data from flash to RAM and zero .bss,
 *   4. run C++ static constructors,
 *   5. call main().
 *
 * SystemInit() therefore runs before global variables have their initial
 * values. Only register writes belong here; do not read configuration
 * variables or use C++ objects.
 */

#include <stdint.h>

#include "py32f0xx.h"

/* The stock bootloader occupies the first 10 KiB of flash and jumps to the
 * application at this offset. The linker script uses the same value. */
#define VECT_TAB_OFFSET 0x2800U

/**
 * @brief System clock frequency in hertz.
 *
 * The stock UV-K1 / UV-K5(8) v3 bootloader configures the PLL and leaves the
 * core running at 48 MHz before jumping to the application. We do not
 * reconfigure the clock in this milestone; documenting the assumption in one
 * place keeps the timebase and future peripheral drivers consistent.
 *
 * This variable lives in .data, so its initial value is copied from flash
 * after SystemInit() has returned.
 */
uint32_t SystemCoreClock = 48000000U;

/**
 * @brief Called by the reset handler before .data and .bss are initialized.
 *
 * Without this relocation the Cortex-M0+ would keep using the bootloader's
 * vector table instead of the one linked at 0x08002800. Every interrupt would
 * then jump into bootloader code, which usually ends in a hard fault.
 */
void SystemInit(void)
{
    SCB->VTOR = FLASH_BASE + VECT_TAB_OFFSET;
}

/**
 * @brief Stub called by newlib's __libc_init_array().
 *
 * The default C runtime startup files are not linked (-nostartfiles), so this
 * empty function keeps newlib's static-constructor helper happy. C++ global
 * constructors are placed in .init_array and still run normally.
 */
void _init(void)
{
}
