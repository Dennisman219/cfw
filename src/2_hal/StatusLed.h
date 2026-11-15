/**
 * @file StatusLed.h
 * @brief Portable red/green status LED role.
 *
 * The UV-K1 / UV-K5(8) v3 has a red/green LED at the top of the case. It is
 * not connected to the MCU. Both channels are driven by GPIO outputs of the
 * BK4819 radio chip, so the platform implementation uses the BK4819 hardware
 * proxy instead of a GPIO pin on the microcontroller.
 *
 * The chip does not drive the LED automatically. Application code decides
 * when to show red, green or amber; the radio never lights it by itself.
 * Red and green together usually look amber, but the exact mixed color should
 * be confirmed on hardware.
 */

#ifndef HAL_STATUS_LED_H_
#define HAL_STATUS_LED_H_

#include <stdint.h>

namespace platform
{

/**
 * @brief Red/green status LED.
 *
 * Typical use:
 *
 *     status_led.set(StatusLed::Color::Red);
 *     status_led.set(StatusLed::Color::Off);
 */
struct StatusLed
{
    /** Colors the LED can show. */
    enum class Color : uint8_t
    {
        Off,
        Red,
        Green,
        Amber, ///< Red and green at the same time.
    };

    /**
     * @brief Turn both LED channels off.
     *
     * Call after the BK4819 proxy has been initialized, because switching the
     * LED writes a register through that proxy.
     */
    void init();

    /** Switch the LED to one color, or turn it off. */
    void set(Color color);

    /** @return the color selected last. */
    Color color() const;

private:
    Color color_ = Color::Off;
};

/** The single status LED instance used by the firmware. */
extern StatusLed status_led;

} // namespace platform

#endif // HAL_STATUS_LED_H_
