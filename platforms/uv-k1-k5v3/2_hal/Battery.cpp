/**
 * @file Battery.cpp
 * @brief UV-K1 / UV-K5(8) v3 implementation of the portable battery role.
 *
 * The ADC1 hardware proxy provides the converter; this file decides that the
 * battery divider is on channel 8 and should be sampled with a long 41.5-cycle
 * sample-and-hold time. It also owns the four-sample average.
 */

#include "Battery.h"

#include "Adc1.h"
#include "SysTickTimer.h"

namespace platform
{
namespace
{

/** The battery voltage divider is connected to ADC1 channel 8. */
constexpr uint8_t kAdcChannel = 8;

/**
 * A long sampling time lets the sample-and-hold capacitor charge fully from
 * the relatively high-impedance divider. The reference firmware uses the same
 * setting.
 */
constexpr Adc1::SamplingTime kSamplingTime = Adc1::SamplingTime::Cycles41_5;

/** Number of samples averaged by read_average(). */
constexpr uint8_t kSampleCount = 4;

} // namespace

/** Definition of the single battery instance declared in Battery.h. */
Battery battery;

void Battery::init()
{
    adc1.init();
    adc1.set_channel(kAdcChannel, kSamplingTime);

    // Calibration compensates the internal ADC errors. It must run before the
    // ADC is enabled.
    adc1.calibrate();
    adc1.enable();

    // Give the analog circuitry a moment to settle before the first sample.
    // The datasheet's stabilization time is in the microsecond range; one
    // millisecond is a deliberately generous, easy-to-read delay.
    systick.delay_ms(1);

    average_ = 0;
}

uint16_t Battery::read_raw()
{
    return adc1.read();
}

uint16_t Battery::read_average()
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < kSampleCount; ++i) {
        sum += read_raw();
    }

    average_ = static_cast<uint16_t>(sum / kSampleCount);
    return average_;
}

uint16_t Battery::average() const
{
    return average_;
}

} // namespace platform
