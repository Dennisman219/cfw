/**
 * @file platform.cpp
 * @brief UV-K1 / UV-K5(8) v3 implementation of the portable platform interface.
 *
 * This file wires the generic interface from src/2_hal/Platform.h to the
 * hardware support code in this platform directory. Keeping the wiring in one
 * place makes it easy to see what the rest of the firmware can use.
 */

#include "Platform.h"

#include "board.h"
#include "timebase.h"

namespace platform
{

void init()
{
    // Configure pins first, then start the timebase that drives the heartbeat.
    board_init();
    time_init();
}

void poll()
{
    // No blocking work here: the main loop must stay responsive for the
    // application framework that will be added in later milestones.
    board_poll();
}

} // namespace platform
