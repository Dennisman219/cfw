/**
 * @file Platform.cpp
 * @brief UV-K1 / UV-K5(8) v3 implementation of the portable platform interface.
 *
 * This file wires the generic interface from src/2_hal/Platform.h to the
 * hardware support code in this platform directory. Keeping the wiring in one
 * place makes it easy to see what the rest of the firmware can use.
 */

#include "Platform.h"

#include "Battery.h"
#include "Clock.h"
#include "Display.h"
#include "Radio.h"
#include "StatusLed.h"
#include "SysTickTimer.h"

namespace platform
{
namespace
{

/** Receive frequency used to check the radio during board bring-up. */
constexpr uint32_t kBringUpFrequencyHz = 433500000U;


} // namespace

void init()
{
    // Order matters: the clock must run before anything waits, and the board
    // must enable the GPIO clocks before the individual drivers configure
    // their pins.
    clock.init();

    // Reset the radio before the other drivers run. The BK4819 keeps its state
    // across an MCU reset, so this also makes sure a warm reset cannot leave it
    // in an old receive or transmit state. It stays idle until the bring-up
    // tune at the end of init().
    radio.init();
    status_led.init();

    display.init();

    display.flush();
    display.backlight_on();

    // Temporary bring-up check: walk through the LED colors so the red
    // channel, the green channel and their mixed color can be checked by eye.
    // Remove this when the application starts using the status LED.
    status_led.set(StatusLed::Color::Red);
    systick.delay_ms(150);
    status_led.set(StatusLed::Color::Green);
    systick.delay_ms(150);
    status_led.set(StatusLed::Color::Amber);
    systick.delay_ms(150);
    status_led.set(StatusLed::Color::Off);

    // Initialize and read the battery last. If the ADC clock is wrong, the
    // radio now freezes with a visible test pattern instead of a dark screen.
    // The averaged raw value stays in platform::battery and can be inspected
    // over SWD even though nothing displays it yet.
    battery.init();
    (void)battery.read_average();

    // Bring up the receiver last, after the display is already showing a
    // picture. The receiver tunes to 433.500 MHz with the squelch held open, so
    // the raw RSSI and the radio state can be inspected over SWD. The audio PA
    // stays off, so this does not make any sound yet.
    radio.receive(kBringUpFrequencyHz);
}

void poll()
{
    clock.tick();
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
    platform::poll();
}
