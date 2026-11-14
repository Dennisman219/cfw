/**
 * @file main.cpp
 * @brief Entry point of the platform-independent firmware.
 *
 * This file deliberately contains no hardware code. The platform provides
 * the implementation of platform::init() and platform::poll(); later
 * milestones will start the application framework here instead of polling the
 * platform directly.
 */

int main()
{
    while (true) {
        // Boot skeleton: the platform code will run here in a later commit.
    }
}
