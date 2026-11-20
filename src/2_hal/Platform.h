/**
 * @file Platform.h
 * @brief Interface between the portable firmware and a hardware platform.
 *
 * Everything under src/ must only use interfaces like this one. Each
 * platform (the real radio, a simulator, a future radio) implements these
 * functions in its own directory. That is what allows the same applications
 * to run on different hardware.
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

namespace platform
{

/**
 * @brief Initialize the platform.
 *
 * Called once from main() before the main loop starts. Implementations set up
 * clocks, GPIO, the timebase and any other hardware that is always needed.
 */
void init();

/**
 * @brief Run one iteration of background platform work.
 * Here anything thats needs to be periodically happen for the platform can happen, like watchdog kicking or key scanning.
 * It must return quickly and must not block. A platform with no background work may leave this empty.
 */
void poll();

} // namespace platform

#endif // PLATFORM_H_
