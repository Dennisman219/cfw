/**
 * @file main.cpp
 * @brief Entry point of the platform-independent firmware.
 *
 * This file deliberately contains no hardware-specific code. Each platform
 * implements platform::init() and platform::poll() from Platform.h. Later
 * milestones will start the application framework here instead of polling the
 * platform directly.
 */

#include "Platform.h"

int main()
{
    platform::init();

    while (true) {
        platform::poll();
    }
}
