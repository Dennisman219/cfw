/**
 * @file Display.h
 * @brief Portable display role.
 *
 * Applications draw into a framebuffer and call flush(). They do not need to
 * know which controller the radio uses. Each platform implements this role in
 * its own 2_hal/Display.cpp.
 *
 * On the UV-K1 / UV-K5(8) v3, that implementation uses the ST7565 hardware
 * proxy for the controller and the backlight proxy for the LED circuit.
 */

#ifndef HAL_DISPLAY_H_
#define HAL_DISPLAY_H_

#include <stdint.h>

namespace platform
{

/**
 * @brief Display with a local framebuffer and backlight control.
 *
 * Drawing functions modify a 1 KiB copy of the screen in RAM. Nothing appears
 * on the panel until flush() sends that copy to the hardware.
 *
 * Pixel coordinates start at the top-left corner:
 *
 *     (0, 0)              (width - 1, 0)
 *        +--------------------------+
 *        |                          |
 *        |                          |
 *        +--------------------------+
 *     (0, height - 1)   (width - 1, height - 1)
 */
struct Display
{
    /** Panel size in pixels. */
    static constexpr uint8_t width = 128;
    static constexpr uint8_t height = 64;

    /**
     * @brief Initialize the display hardware and show a blank screen.
     *
     * The backlight starts off, so the screen does not light up before the
     * application has drawn something useful.
     */
    void init();

    /** Turn every pixel off. Call flush() to send the result to the panel. */
    void clear();

    /** Turn every pixel on. Call flush() to send the result to the panel. */
    void fill();

    /** Set or clear one pixel. Coordinates outside the panel are ignored. */
    void set_pixel(uint8_t x, uint8_t y, bool on = true);

    /** @return true when the pixel is set in the local framebuffer. */
    bool pixel(uint8_t x, uint8_t y) const;

    /**
     * @brief Send the framebuffer to the panel if it has changed.
     *
     * The panel stores its pixels in "pages": one byte holds eight vertical
     * pixels. The platform implementation takes care of that layout, so
     * application code only ever deals with x/y coordinates.
     */
    void flush();

    /** Turn the backlight on. */
    void backlight_on();

    /** Turn the backlight off. */
    void backlight_off();

    /** Set the backlight to a specific state. */
    void set_backlight(bool enabled);

    /** @return true when the backlight is currently on. */
    bool backlight_is_on() const;

private:
    /** One page holds eight horizontal rows of pixels. */
    static constexpr uint8_t pages = height / 8;

    /** Fill the whole framebuffer with one byte value (0x00 or 0xFF). */
    void set_all(uint8_t value);

    uint8_t framebuffer_[pages][width] = {};
    bool dirty_ = false;
};

/** The single display instance used by the firmware. */
extern Display display;

} // namespace platform

#endif // HAL_DISPLAY_H_
