/**
 * @file Display.cpp
 * @brief UV-K1 / UV-K5(8) v3 implementation of the portable display role.
 *
 * This file owns the framebuffer and the drawing logic. It maps the abstract
 * screen operations onto the ST7565 hardware proxy and the backlight proxy.
 * Neither of those proxies knows anything about a framebuffer.
 */

#include "Display.h"

#include "Backlight.h"
#include "ST7565.h"

namespace platform
{
namespace
{

/** The panel's first column is wired four pixels in from the controller's. */
constexpr uint8_t kColumnOffset = 4;

} // namespace

/** Definition of the single display instance declared in Display.h. */
Display display;

void Display::init()
{
    // The backlight is part of the display assembly from the application's
    // point of view, but it is a separate piece of hardware behind the scenes.
    backlight.init();
    st7565.init();

    clear();
    flush();
}

void Display::set_all(uint8_t value)
{
    for (uint8_t page = 0; page < pages; ++page) {
        for (uint8_t x = 0; x < width; ++x) {
            framebuffer_[page][x] = value;
        }
    }
    dirty_ = true;
}

void Display::clear()
{
    set_all(0x00);
}

void Display::fill()
{
    set_all(0xFF);
}

void Display::set_pixel(uint8_t x, uint8_t y, bool on)
{
    if (x >= width || y >= height) {
        return;
    }

    const uint8_t mask = static_cast<uint8_t>(1U << (y % 8));
    if (on) {
        framebuffer_[y / 8][x] |= mask;
    } else {
        framebuffer_[y / 8][x] &= static_cast<uint8_t>(~mask);
    }
    dirty_ = true;
}

bool Display::pixel(uint8_t x, uint8_t y) const
{
    if (x >= width || y >= height) {
        return false;
    }

    return (framebuffer_[y / 8][x] & (1U << (y % 8))) != 0;
}

void Display::flush()
{
    if (!dirty_) {
        return;
    }

    // Start at line 0 of the controller's internal RAM. Without this command
    // the panel would keep whatever start line the previous firmware selected.
    st7565.set_start_line(0);

    for (uint8_t page = 0; page < pages; ++page) {
        st7565.set_page(page);
        st7565.set_column(kColumnOffset);

        // One framebuffer row maps to one page. The controller increments its
        // column address automatically, so the whole row is one burst.
        st7565.write_data(framebuffer_[page], width);
    }

    dirty_ = false;
}

void Display::backlight_on()
{
    backlight.on();
}

void Display::backlight_off()
{
    backlight.off();
}

void Display::set_backlight(bool enabled)
{
    backlight.set(enabled);
}

bool Display::backlight_is_on() const
{
    return backlight.is_on();
}

} // namespace platform
