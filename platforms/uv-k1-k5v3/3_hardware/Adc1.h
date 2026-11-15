/**
 * @file Adc1.h
 * @brief Hardware proxy for the PY32F071 ADC1 peripheral.
 *
 * This driver exposes the ADC1 configuration and conversion operations used by
 * the firmware. It talks to the peripheral through the Puya LL library and
 * knows nothing about what is connected to the ADC input.
 *
 * The class is called Adc1 rather than ADC1 because the CMSIS headers define
 * `ADC1` as the name of the peripheral register block macro.
 *
 * On the UV-K1 / UV-K5(8) v3 the ADC inputs are PB0 and PB1. Both pins are put
 * into analog mode here, matching the board wiring. A caller selects which
 * channel to convert with set_channel().
 *
 * The proxy is set up for the simplest useful mode: one channel, one
 * software-triggered conversion at a time, 12-bit right-aligned result. Scan
 * mode, continuous conversion, DMA and injected channels are disabled. This is
 * all the battery measurement needs and keeps the code easy to follow.
 */

#ifndef PLATFORM_ADC1_H_
#define PLATFORM_ADC1_H_

#include <stdint.h>

namespace platform
{

/**
 * @brief Driver for the ADC1 successive-approximation converter.
 *
 * Typical use:
 *
 *     adc1.init();
 *     adc1.set_channel(8, Adc1::SamplingTime::Cycles41_5);
 *     adc1.calibrate();
 *     adc1.enable();
 *     const uint16_t value = adc1.read();
 */
struct Adc1
{
    /** ADC1 has external channels 0 to 23 on this package. */
    static constexpr uint8_t max_channel = 23;

    /**
     * @brief Sample-and-hold time, in ADC clock cycles.
     *
     * A longer sampling time gives the input capacitor more time to charge
     * from a high-impedance source such as a battery divider. The datasheet
     * lists these eight settings.
     */
    enum class SamplingTime : uint8_t
    {
        Cycles3_5,
        Cycles5_5,
        Cycles7_5,
        Cycles13_5,
        Cycles28_5,
        Cycles41_5,
        Cycles134_5,
        Cycles239_5,
    };

    /**
     * @brief Enable the ADC and GPIO clocks and configure the peripheral.
     *
     * Selects PCLK / 4 as the ADC clock (12 MHz at the board's 48 MHz core
     * clock), 12-bit right-aligned results, software trigger, single
     * conversion, one sequencer rank, no scan mode and no DMA.
     *
     * The ADC is left disabled after this call. Select a channel with
     * set_channel(), calibrate(), then enable().
     */
    void init();

    /**
     * @brief Select the channel for the next conversions.
     *
     * Configures sequencer rank 1, so every conversion reads this channel.
     * Invalid channel numbers are ignored. Call this while no conversion is
     * running.
     *
     * @param channel       External channel 0..23.
     * @param sampling      Sample-and-hold time for that channel.
     */
    void set_channel(uint8_t channel, SamplingTime sampling);

    /**
     * @brief Run the built-in calibration.
     *
     * The calibration removes internal offset errors. It must be run while the
     * ADC is disabled and takes a few ADC clock cycles. This function blocks
     * until the hardware reports that calibration has finished.
     */
    void calibrate();

    /** Enable the ADC. */
    void enable();

    /** Disable the ADC. */
    void disable();

    /** @return true while the ADC is enabled. */
    bool is_enabled() const;

    /** Start one conversion using the software trigger. */
    void start_conversion();

    /**
     * @brief Check whether the current conversion has finished.
     *
     * Reading the result does not clear this flag; call
     * clear_conversion_complete() first, otherwise the next read can return
     * before the new conversion has finished.
     */
    bool conversion_complete() const;

    /** Clear the end-of-sequence flag left by the previous conversion. */
    void clear_conversion_complete();

    /** @return the result of the last conversion as a 12-bit value. */
    uint16_t read_data() const;

    /**
     * @brief Perform one blocking conversion.
     *
     * Starts a conversion and waits for it to finish. One conversion takes
     * about 54 ADC clock cycles, a few microseconds at 12 MHz.
     */
    uint16_t read();
};

/** The single ADC1 instance used by this platform. */
extern Adc1 adc1;

} // namespace platform

#endif // PLATFORM_ADC1_H_
