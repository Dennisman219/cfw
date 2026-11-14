/**
 * @file board.h
 * @brief Board-level hardware for the UV-K1 / UV-K5(8) v3.
 *
 * This is the lowest layer that knows about actual radio pins. Future
 * milestones will add display, keypad, radio and storage drivers here or in
 * sibling files.
 */

#ifndef PLATFORM_BOARD_H_
#define PLATFORM_BOARD_H_

namespace platform
{

/**
 * @brief Configure the board pins used by the boot firmware.
 *
 * Safe to call once from platform::init(). The backlight is turned off so the
 * heartbeat starts from a known state.
 */
void board_init();

/**
 * @brief Drive the boot heartbeat.
 *
 * Call repeatedly from the main loop. This function never blocks: it only
 * compares the current millisecond counter with a deadline.
 */
void board_poll();

/**
 * @brief Turn the display backlight on or off.
 *
 * On this radio the backlight is active high (PF8).
 */
void board_set_backlight(bool on);

} // namespace platform

#endif // PLATFORM_BOARD_H_
