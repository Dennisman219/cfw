/**
 * @file board.cpp
 * @brief Board-level hardware for the UV-K1 / UV-K5(8) v3.
 *
 * The first milestone only needs one output: the display backlight on PF8.
 * Driving it from the 1 ms timebase proves that the clock, vector table,
 * SysTick interrupt and GPIO all work. It is also easy to see without opening
 * the radio or connecting a debugger.
 */

#include "board.h"

#include "timebase.h"

#include "py32f071_ll_bus.h"
#include "py32f071_ll_gpio.h"

namespace
{

/** Backlight pin. The LED is active high. */
GPIO_TypeDef* const kBacklightPort = GPIOF;
constexpr uint32_t kBacklightPin = LL_GPIO_PIN_8;

/** Heartbeat: a short 50 ms flash once per second. */
constexpr uint32_t kHeartbeatPeriodMs = 1000U;
constexpr uint32_t kHeartbeatPulseMs = 50U;

/** State of the non-blocking heartbeat generator. */
bool g_pulse_active = false;
uint32_t g_pulse_end_ms = 0;
uint32_t g_next_beat_ms = 0;

} // namespace

namespace platform
{

void board_init()
{
    // The GPIO peripheral must be clocked before its registers can be used.
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOF);

    LL_GPIO_SetPinMode(kBacklightPort, kBacklightPin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(kBacklightPort, kBacklightPin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(kBacklightPort, kBacklightPin, LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinPull(kBacklightPort, kBacklightPin, LL_GPIO_PULL_NO);

    board_set_backlight(false);

    // Start with a pulse immediately so there is no one-second wait after
    // flashing. millis() is still near zero here, which is fine.
    g_next_beat_ms = millis();
    g_pulse_active = false;
}

void board_set_backlight(bool on)
{
    if (on) {
        LL_GPIO_SetOutputPin(kBacklightPort, kBacklightPin);
    } else {
        LL_GPIO_ResetOutputPin(kBacklightPort, kBacklightPin);
    }
}

void board_poll()
{
    const uint32_t now = millis();

    if (g_pulse_active) {
        if (time_reached(now, g_pulse_end_ms)) {
            board_set_backlight(false);
            g_pulse_active = false;
        }
    } else if (time_reached(now, g_next_beat_ms)) {
        board_set_backlight(true);
        g_pulse_active = true;
        g_pulse_end_ms = now + kHeartbeatPulseMs;
        g_next_beat_ms = now + kHeartbeatPeriodMs;
    }
}

} // namespace platform
