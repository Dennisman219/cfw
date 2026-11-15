/**
 * @file StatusLed.cpp
 * @brief UV-K1 / UV-K5(8) v3 implementation of the portable status LED role.
 *
 * The LED is not wired to the MCU. Its red and green channels are driven by
 * two GPIO outputs of the BK4819, so this file translates a color into the
 * matching pair of chip GPIO states.
 */

#include "StatusLed.h"

#include "BK4819.h"

namespace platform
{
namespace
{

/** Apply the on/off state of both LED channels. */
void apply(bool red, bool green)
{
    bk4819.set_gpio(BK4819::Gpio::RedLed, red);
    bk4819.set_gpio(BK4819::Gpio::GreenLed, green);
}

} // namespace

/** Definition of the single status LED instance declared in StatusLed.h. */
StatusLed status_led;

void StatusLed::init()
{
    set(Color::Off);
}

void StatusLed::set(Color color)
{
    color_ = color;

    switch (color) {
    case Color::Off:
        apply(false, false);
        break;
    case Color::Red:
        apply(true, false);
        break;
    case Color::Green:
        apply(false, true);
        break;
    case Color::Amber:
        apply(true, true);
        break;
    }
}

StatusLed::Color StatusLed::color() const
{
    return color_;
}

} // namespace platform
