/**
 * @file Radio.h
 * @brief Portable radio transceiver role.
 *
 * Applications tune, receive and transmit through this interface. Each
 * platform implements the role in its own 2_hal/Radio.cpp. On the UV-K1 /
 * UV-K5(8) v3 that implementation uses the BK4819 hardware proxy.
 *
 * Frequencies are in hertz. The BK4819 tunes in 10 Hz steps, so the platform
 * implementation rounds a requested frequency down to the next 10 Hz boundary
 * and frequency() reports the value that was actually programmed.
 *
 * This role covers basic FM voice operation only. CTCSS, DCS, DTMF, FSK,
 * scanning, companding and scrambling are not implemented yet. Transmit power
 * calibration also needs the external EEPROM settings, so transmit() uses a
 * fixed placeholder PA bias for now.
 */

#ifndef HAL_RADIO_H_
#define HAL_RADIO_H_

#include <stdint.h>

namespace platform
{

/**
 * @brief FM transceiver control.
 *
 * Typical receive use:
 *
 *     radio.init();
 *     radio.receive(433500000U);   // 433.500 MHz
 *     const uint16_t level = radio.rssi();
 *
 * receive() and transmit() select the frequency and the signal path. idle()
 * stops both directions and mutes the audio output.
 */
struct Radio
{
    /** Current direction of the radio. */
    enum class Mode : uint8_t
    {
        Off,      ///< Receiver and transmitter are stopped.
        Receive,  ///< Receiver is running.
        Transmit, ///< Transmitter and external PA are running.
    };

    /** Channel spacing and receiver filter width. */
    enum class Bandwidth : uint8_t
    {
        Wide,   ///< 25 kHz channels.
        Narrow, ///< 12.5 kHz channels.
    };

    /**
     * @brief Initialize the radio hardware and leave it idle.
     *
     * Call once after the clock and board are ready, before receive() or
     * transmit().
     */
    void init();

    /**
     * @brief Tune to a frequency and start receiving.
     *
     * The squelch is held fully open during bring-up, so the chip always
     * demodulates and outputs audio. Channel settings and squelch levels will
     * be added with the external EEPROM support.
     *
     * @param frequency_hz Receive frequency in hertz.
     */
    void receive(uint32_t frequency_hz);

    /**
     * @brief Tune to a frequency, start transmitting and enable the PA.
     *
     * The transmit power amplifier bias is a fixed placeholder until the
     * per-radio calibration values from the external EEPROM are available.
     * Do not use this for on-air operation yet.
     *
     * @param frequency_hz Transmit frequency in hertz.
     */
    void transmit(uint32_t frequency_hz);

    /** Stop the receiver and transmitter, disable the PA and mute audio. */
    void idle();

    /** Select the receiver filter bandwidth. */
    void set_bandwidth(Bandwidth bandwidth);

    /** @return the selected filter bandwidth. */
    Bandwidth bandwidth() const;

    /**
     * @brief Read the current received signal strength.
     *
     * The result is the chip's raw 9-bit RSSI value (0 to 511) in 0.5 dB
     * steps. The reference firmware converts it to roughly
     * `(value / 2) - 160` dBm before gain correction; turning it into a
     * calibrated level needs the per-radio calibration data.
     */
    uint16_t rssi();

    /** @return the current radio direction. */
    Mode mode() const;

    /**
     * @brief Return the frequency that was actually programmed.
     *
     * The value is rounded down to the hardware's 10 Hz tuning step.
     */
    uint32_t frequency() const;

private:
    Mode mode_ = Mode::Off;
    Bandwidth bandwidth_ = Bandwidth::Wide;
    uint32_t frequency_ = 0;
};

/** The single radio instance used by the firmware. */
extern Radio radio;

} // namespace platform

#endif // HAL_RADIO_H_
