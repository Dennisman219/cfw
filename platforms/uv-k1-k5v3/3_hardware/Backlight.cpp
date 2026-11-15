/**
 * @file Backlight.cpp
 * @brief Display backlight control for the UV-K1 / UV-K5(8) v3.
 */

#include "Backlight.h"

#include "Gpio.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_gpio.h"

namespace
{

/** Backlight pin on the UV-K1 / UV-K5(8) v3. The LED is active high. */
constexpr uint32_t kBacklightPin = LL_GPIO_PIN_8;

} // namespace

namespace platform
{

/** Definition of the single backlight instance declared in Backlight.h. */
Backlight backlight;

void Backlight::init()
{
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOF);

    // Set the output latch low before enabling the output driver, so the
    // backlight cannot flash briefly during boot.
    gpio::set_low(GPIOF, kBacklightPin);
    gpio::configure_output(GPIOF, kBacklightPin);

    on_ = false;
}

void Backlight::on()
{
    gpio::set_high(GPIOF, kBacklightPin);
    on_ = true;
}

void Backlight::off()
{
    gpio::set_low(GPIOF, kBacklightPin);
    on_ = false;
}

void Backlight::set(bool enabled)
{
    if (enabled) {
        on();
    } else {
        off();
    }
}

bool Backlight::is_on() const
{
    return on_;
}

} // namespace platform
