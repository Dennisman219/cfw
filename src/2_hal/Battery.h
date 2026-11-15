/**
 * @file Battery.h
 * @brief Portable battery measurement role.
 *
 * Applications use this interface to read the battery voltage. The platform
 * implementation in 2_hal/Battery.cpp decides which ADC channel and sampling
 * time the board needs and how many samples to average.
 *
 * The values are raw 12-bit ADC counts on purpose. Turning them into
 * millivolts needs the per-radio calibration values stored in the external
 * EEPROM, and those settings are not implemented yet.
 */

#ifndef HAL_BATTERY_H_
#define HAL_BATTERY_H_

#include <stdint.h>

namespace platform
{

/**
 * @brief Battery voltage measurement.
 *
 * Channel selection, sampling time and averaging all happen behind this
 * interface, so application code only deals with 0-4095 ADC counts:
 *
 *     const uint16_t raw = platform::battery.read_average();
 */
struct Battery
{
    /** Configure the battery ADC input. */
    void init();

    /** Read one 12-bit sample (0-4095). Blocks for a few microseconds. */
    uint16_t read_raw();

    /**
     * @brief Read four samples and store their average.
     *
     * Averaging removes most of the noise on the battery divider.
     */
    uint16_t read_average();

    /** @return the last value produced by read_average(), or 0. */
    uint16_t average() const;

private:
    uint16_t average_ = 0;
};

/** The single battery reader used by the firmware. */
extern Battery battery;

} // namespace platform

#endif // HAL_BATTERY_H_
