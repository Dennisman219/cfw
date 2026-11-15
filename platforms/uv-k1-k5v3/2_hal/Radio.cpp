/**
 * @file Radio.cpp
 * @brief UV-K1 / UV-K5(8) v3 implementation of the portable radio role.
 *
 * This file turns the abstract receive/transmit requests into the register and
 * GPIO sequences the board needs. The BK4819 hardware proxy owns the chip
 * itself; this layer owns the order of operations, the settling delays and the
 * board-level PA and antenna path.
 */

#include "Radio.h"

#include "BK4819.h"
#include "SysTickTimer.h"

namespace platform
{
namespace
{

/** The BK4819 tunes in 10 Hz steps. */
constexpr uint32_t kFrequencyStepHz = 10;

/**
 * Placeholder PA bias for the first transmit tests. The real value is
 * calibrated per radio and stored in the external EEPROM, which is not
 * implemented yet. 50 is deliberately conservative.
 */
constexpr uint8_t kTransmitBias = 50;

BK4819::Bandwidth to_bk4819(Radio::Bandwidth bandwidth)
{
    return (bandwidth == Radio::Bandwidth::Narrow)
        ? BK4819::Bandwidth::Narrow
        : BK4819::Bandwidth::Wide;
}

uint32_t round_to_step(uint32_t frequency_hz)
{
    return frequency_hz - (frequency_hz % kFrequencyStepHz);
}

} // namespace

/** Definition of the single radio instance declared in Radio.h. */
Radio radio;

void Radio::init()
{
    bk4819.init();

    // Start with a known filter setting and keep the audio muted until a mode
    // is selected.
    bandwidth_ = Bandwidth::Wide;
    bk4819.set_bandwidth(to_bk4819(bandwidth_));
    bk4819.set_audio_path(BK4819::AudioPath::Mute);
    bk4819.idle();

    frequency_ = 0;
    mode_ = Mode::Off;
}

void Radio::receive(uint32_t frequency_hz)
{
    // Disable the PA and receive path before changing frequency or filter
    // settings. This avoids transmitting into the old antenna path and avoids
    // a click while the synthesizer retunes.
    bk4819.set_gpio(BK4819::Gpio::PaEnable, false);
    bk4819.set_power_amplifier(0, 0);
    bk4819.set_gpio(BK4819::Gpio::RxEnable, false);
    bk4819.set_audio_path(BK4819::AudioPath::Mute);

    bk4819.set_bandwidth(to_bk4819(bandwidth_));

    frequency_ = round_to_step(frequency_hz);
    const uint32_t chip_frequency = frequency_ / kFrequencyStepHz;
    bk4819.set_frequency(chip_frequency);

    // Without channel settings there is no useful squelch threshold, so keep
    // the squelch fully open. The defaults in BK4819::Squelch describe that.
    const BK4819::Squelch squelch;
    bk4819.set_squelch(squelch);

    bk4819.receive();

    // Select the VHF or UHF LNA only after the receiver is powered, then turn
    // on the receive switch and connect the demodulated audio.
    bk4819.set_antenna_path(chip_frequency);
    bk4819.set_gpio(BK4819::Gpio::RxEnable, true);
    bk4819.set_audio_path(BK4819::AudioPath::Fm);

    mode_ = Mode::Receive;
}

void Radio::transmit(uint32_t frequency_hz)
{
    // Never retune the transmitter with the PA still enabled, and remove the
    // old bias so the PA cannot briefly run at the previous power level.
    bk4819.set_gpio(BK4819::Gpio::PaEnable, false);
    bk4819.set_power_amplifier(0, 0);
    bk4819.set_gpio(BK4819::Gpio::RxEnable, false);
    bk4819.set_audio_path(BK4819::AudioPath::Mute);

    bk4819.set_bandwidth(to_bk4819(bandwidth_));

    frequency_ = round_to_step(frequency_hz);
    const uint32_t chip_frequency = frequency_ / kFrequencyStepHz;
    bk4819.set_frequency(chip_frequency);

    bk4819.transmit();

    // The reference firmware waits 10 ms for the transmit chain to settle,
    // then selects the antenna path and enables the external PA.
    systick.delay_ms(10);
    bk4819.set_antenna_path(chip_frequency);
    bk4819.set_gpio(BK4819::Gpio::PaEnable, true);

    // Give the PA time to turn on before applying bias, then let the output
    // settle before the caller starts modulating.
    systick.delay_ms(5);
    bk4819.set_power_amplifier(kTransmitBias, chip_frequency);
    systick.delay_ms(10);

    mode_ = Mode::Transmit;
}

void Radio::idle()
{
    bk4819.set_gpio(BK4819::Gpio::PaEnable, false);
    bk4819.set_power_amplifier(0, 0);
    bk4819.set_gpio(BK4819::Gpio::RxEnable, false);
    bk4819.set_audio_path(BK4819::AudioPath::Mute);
    bk4819.idle();

    mode_ = Mode::Off;
}

void Radio::set_bandwidth(Bandwidth bandwidth)
{
    bandwidth_ = bandwidth;
    bk4819.set_bandwidth(to_bk4819(bandwidth_));
}

Radio::Bandwidth Radio::bandwidth() const
{
    return bandwidth_;
}

uint16_t Radio::rssi()
{
    // This reads the chip's live RSSI register. It is meaningful while the
    // receiver is running; the value is not cached.
    return bk4819.rssi();
}

Radio::Mode Radio::mode() const
{
    return mode_;
}

uint32_t Radio::frequency() const
{
    return frequency_;
}

} // namespace platform
