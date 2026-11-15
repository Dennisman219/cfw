/**
 * @file Backlight.h
 * @brief Hardware proxy for the display backlight.
 *
 * The backlight is a white LED circuit controlled by one GPIO pin. It is not
 * part of the ST7565 controller, which is why it has its own hardware proxy.
 * The portable display role (src/2_hal/Display.h) exposes the combined
 * display and backlight API to applications.
 */

#ifndef PLATFORM_BACKLIGHT_H_
#define PLATFORM_BACKLIGHT_H_

namespace platform
{

/**
 * @brief On/off control for the display backlight.
 *
 * The backlight is connected to PF8 and is active high.
 */
struct Backlight
{
    /** Configure PF8 as an output and start with the backlight off. */
    void init();

    /** Turn the backlight on. */
    void on();

    /** Turn the backlight off. */
    void off();

    /** Set the backlight to a specific state. */
    void set(bool enabled);

    /** @return true when the backlight is currently on. */
    bool is_on() const;

private:
    bool on_ = false;
};

/** The single backlight instance used by this platform. */
extern Backlight backlight;

} // namespace platform

#endif // PLATFORM_BACKLIGHT_H_
