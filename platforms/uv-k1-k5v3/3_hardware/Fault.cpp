/**
 * @file Fault.cpp
 * @brief Hard-fault handler and visible fault indication.
 *
 * The handler configures the backlight pin itself instead of relying on the
 * normal hardware initialization. A fault can happen before that code has run,
 * so the fault path must be self-contained.
 */

#include "Fault.h"

#include "py32f071_ll_bus.h"
#include "py32f071_ll_gpio.h"

namespace
{

/** Backlight pin on the UV-K1 / UV-K5(8) v3. The LED is active high. */
GPIO_TypeDef* const kBacklightPort = GPIOF;
constexpr uint32_t kBacklightPin = LL_GPIO_PIN_8;

/**
 * @brief Busy-loop delay used only by the fault indicator.
 *
 * SysTick may be the peripheral that failed, so the fault path cannot use the
 * millisecond timebase. The loop is calibrated by eye rather than precisely;
 * its only job is to make the blink pattern recognizable.
 */
void fault_delay()
{
    for (volatile uint32_t i = 0; i < 1000000U; ++i) {
        // Intentionally empty.
    }
}

void fault_backlight_init()
{
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOF);

    LL_GPIO_SetPinMode(kBacklightPort, kBacklightPin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(kBacklightPort, kBacklightPin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(kBacklightPort, kBacklightPin, LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinPull(kBacklightPort, kBacklightPin, LL_GPIO_PULL_NO);
}

[[noreturn]] void fault_blink_forever()
{
    while (true) {
        LL_GPIO_SetOutputPin(kBacklightPort, kBacklightPin);
        fault_delay();
        LL_GPIO_ResetOutputPin(kBacklightPort, kBacklightPin);
        fault_delay();
    }
}

} // namespace

namespace platform
{

/** Definition of the single fault handler instance declared in Fault.h. */
Fault fault;

[[noreturn]] void Fault::panic()
{
    // Stop interrupts first: a periodic interrupt would otherwise keep
    // disturbing the blink loop, and the fault may have been caused by one.
    __disable_irq();

    fault_backlight_init();
    fault_blink_forever();
}

} // namespace platform

/**
 * @brief Cortex-M0+ hard-fault vector.
 *
 * The name must stay unchanged because the startup vector table refers to it.
 */
extern "C" void HardFault_Handler(void)
{
    platform::fault.panic();
}
