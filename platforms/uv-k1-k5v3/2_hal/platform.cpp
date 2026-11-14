/**
 * @file platform.cpp
 * @brief UV-K1 / UV-K5(8) v3 implementation of the portable platform interface.
 *
 * This file wires the generic interface from src/2_hal/Platform.h to the
 * hardware support code in this platform directory. Keeping the wiring in one
 * place makes it easy to see what the rest of the firmware can use.
 */

#include "Platform.h"

#include "timebase.h"

namespace platform
{

void init()
{
    time_init();
}

void poll()
{
    // The boot heartbeat is added in the next commit. Until then the main loop
    // only needs to let the SysTick interrupt run.
}

} // namespace platform
