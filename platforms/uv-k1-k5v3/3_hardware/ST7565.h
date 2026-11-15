/**
 * @file ST7565.h
 * @brief Hardware proxy for the ST7565 LCD controller.
 *
 * This driver exposes the commands the controller understands according to
 * its datasheet: display on/off, page and column addressing, contrast,
 * start line, scan direction and so on. It executes them over SPI1 using the
 * PY32 LL library.
 *
 * It deliberately knows nothing about framebuffers or drawing. Translating a
 * screen image into pages, columns and data bytes is the job of the portable
 * display role in src/2_hal/Display.h.
 */

#ifndef PLATFORM_ST7565_H_
#define PLATFORM_ST7565_H_

#include <stdint.h>

namespace platform
{

/**
 * @brief Driver for one ST7565 controller.
 *
 * The controller is write-only on this board: there is no MISO connection, so
 * it cannot be read back. All functions therefore return nothing.
 */
struct ST7565
{
    /** The controller has 132 column drivers (the panel uses 128 of them). */
    static constexpr uint8_t columns = 132;

    /** The controller RAM is organized as eight pages of eight rows each. */
    static constexpr uint8_t pages = 8;

    /** Valid range for set_start_line(). */
    static constexpr uint8_t max_start_line = 63;

    /** Valid range for set_contrast(). */
    static constexpr uint8_t max_contrast = 63;

    /**
     * @brief Configure the pins and SPI1, reset the controller and apply the
     *        default panel settings.
     *
     * This does not clear the display RAM. The caller should send a full frame
     * afterwards if the screen must be blank.
     */
    void init();

    /** Send the software-reset command and wait for the controller. */
    void reset();

    /** Turn the display on (the RAM contents stay intact). */
    void display_on();

    /** Turn the display off (the RAM contents stay intact). */
    void display_off();

    /** Enable or disable inverted pixels. */
    void set_inverse(bool enabled);

    /** Force all pixels on, or return to normal display. */
    void set_all_pixels_on(bool enabled);

    /**
     * @brief Select the segment (column) scan direction.
     *
     * @param reverse false for normal, true for reversed.
     */
    void set_segment_direction(bool reverse);

    /**
     * @brief Select the common (row) scan direction.
     *
     * @param reverse false for normal, true for reversed.
     */
    void set_com_direction(bool reverse);

    /**
     * @brief Select the LCD bias.
     *
     * @param one_ninth true for 1/9 bias, false for 1/7 bias.
     */
    void set_bias(bool one_ninth);

    /**
     * @brief Set the internal regulator resistor ratio.
     *
     * @param ratio 0..7. Higher values increase the contrast range.
     */
    void set_regulation_ratio(uint8_t ratio);

    /**
     * @brief Set the electronic volume (contrast).
     *
     * @param level 0..63.
     */
    void set_contrast(uint8_t level);

    /**
     * @brief Set which line of display RAM is shown at the top of the panel.
     *
     * @param line 0..63.
     */
    void set_start_line(uint8_t line);

    /**
     * @brief Select the page that subsequent data bytes are written to.
     *
     * @param page 0..7.
     */
    void set_page(uint8_t page);

    /**
     * @brief Set the column address within the selected page.
     *
     * The controller increments the column automatically after every data
     * byte, so a whole page can be sent with one write_data() call.
     *
     * @param column 0..131. The panel on this radio starts at column 4.
     */
    void set_column(uint8_t column);

    /**
     * @brief Write pixel data at the current page and column.
     *
     * @param data  Bytes to write, one byte per eight vertical pixels.
     * @param count Number of bytes. The controller advances the column after
     *              each byte.
     */
    void write_data(const uint8_t* data, uint8_t count);

private:
    /** Send one command byte with A0 low. */
    void write_command(uint8_t value);
};

/** The single ST7565 instance used by this platform. */
extern ST7565 st7565;

} // namespace platform

#endif // PLATFORM_ST7565_H_
