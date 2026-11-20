/**
 * @file Radio.cpp
 * @brief UV-K1 / UV-K5(8) v3 implementation of the portable radio role.
 *
 * This file turns the abstract receive/transmit requests into the register and
 * GPIO sequences the board needs. The BK4819 hardware proxy owns the chip
 * itself; this layer owns the order of operations, the settling delays and the
 * board-level PA and antenna path.
 *
 * The FSK functions are deliberately thin: the proxy owns the chip-side FSK
 * sequence, FIFO packing, timing and status handling, while this layer maps
 * the portable enums and enforces the radio direction.
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

/*
 * The portable FSK enums and the BK4819 enums are a full mirror for now, so
 * each mapping is a one-to-one switch. Keeping the switch (instead of a cast)
 * makes a future divergence between the two headers a compiler warning rather
 * than silent corruption.
 */
BK4819::FskTxMode to_bk4819(Radio::FskTxMode mode)
{
    switch (mode) {
    case Radio::FskTxMode::Fsk1200_2400:
        return BK4819::FskTxMode::Fsk1200_2400;
    case Radio::FskTxMode::Ffsk1200_2400:
        return BK4819::FskTxMode::Ffsk1200_2400;
    case Radio::FskTxMode::NoaaSame:
        return BK4819::FskTxMode::NoaaSame;
    case Radio::FskTxMode::Ffsk1200_1800:
    default:
        return BK4819::FskTxMode::Ffsk1200_1800;
    }
}

BK4819::FskRxMode to_bk4819(Radio::FskRxMode mode)
{
    switch (mode) {
    case Radio::FskRxMode::Fsk1200_2400:
        return BK4819::FskRxMode::Fsk1200_2400;
    case Radio::FskRxMode::Ffsk1200_2400:
        return BK4819::FskRxMode::Ffsk1200_2400;
    case Radio::FskRxMode::Ffsk1200_1800:
    default:
        return BK4819::FskRxMode::Ffsk1200_1800;
    }
}

BK4819::FskRxBandwidth to_bk4819(Radio::FskRxBandwidth bandwidth)
{
    switch (bandwidth) {
    case Radio::FskRxBandwidth::Fsk1200:
        return BK4819::FskRxBandwidth::Fsk1200;
    case Radio::FskRxBandwidth::NoaaSame:
        return BK4819::FskRxBandwidth::NoaaSame;
    case Radio::FskRxBandwidth::Fsk2400OrFfsk1200_2400:
        return BK4819::FskRxBandwidth::Fsk2400OrFfsk1200_2400;
    case Radio::FskRxBandwidth::Ffsk1200_1800:
    default:
        return BK4819::FskRxBandwidth::Ffsk1200_1800;
    }
}

BK4819::FskPreambleType to_bk4819(Radio::FskPreambleType type)
{
    switch (type) {
    case Radio::FskPreambleType::Fixed0x55:
        return BK4819::FskPreambleType::Fixed0x55;
    case Radio::FskPreambleType::Fixed0xAA:
        return BK4819::FskPreambleType::Fixed0xAA;
    case Radio::FskPreambleType::SyncWordMsb:
    default:
        return BK4819::FskPreambleType::SyncWordMsb;
    }
}

BK4819::FskSyncLength to_bk4819(Radio::FskSyncLength length)
{
    return (length == Radio::FskSyncLength::FourBytes)
        ? BK4819::FskSyncLength::FourBytes
        : BK4819::FskSyncLength::TwoBytes;
}

BK4819::FskConfig to_bk4819(const Radio::FskConfig& config)
{
    BK4819::FskConfig out;
    out.tx_mode = to_bk4819(config.tx_mode);
    out.rx_mode = to_bk4819(config.rx_mode);
    out.rx_bandwidth = to_bk4819(config.rx_bandwidth);
    out.rx_gain = config.rx_gain;
    out.preamble_type = to_bk4819(config.preamble_type);
    out.preamble_bytes = config.preamble_bytes;
    out.sync_length = to_bk4819(config.sync_length);
    out.sync_first = config.sync_first;
    out.sync_second = config.sync_second;
    out.crc = config.crc;
    out.scrambler = config.scrambler;
    out.invert_tx = config.invert_tx;
    out.invert_rx = config.invert_rx;
    out.tx_fifo_threshold = config.tx_fifo_threshold;
    out.rx_fifo_threshold = config.rx_fifo_threshold;
    out.gain = config.gain;
    out.clock = config.clock;
    out.deviation = config.deviation;
    return out;
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

void Radio::configure_fsk(const FskConfig& config)
{
    bk4819.fsk_configure(to_bk4819(config));
}

void Radio::enable_fsk(bool enabled)
{
    bk4819.enable_fsk(enabled);
}

bool Radio::fsk_start_tx(const uint8_t* data, uint16_t length)
{
    if (mode_ != Mode::Transmit) {
        return false;
    }
    return bk4819.fsk_start_tx(data, length);
}

bool Radio::fsk_start_rx(uint16_t length)
{
    if (mode_ != Mode::Receive) {
        return false;
    }
    return bk4819.fsk_start_rx(length);
}

bool Radio::fsk_ready()
{
    return bk4819.fsk_rx_data_ready();
}

uint16_t Radio::fsk_read(uint8_t* buffer, uint16_t capacity)
{
    return bk4819.fsk_read(buffer, capacity);
}

bool Radio::fsk_tx_finished()
{
    return bk4819.fsk_tx_finished();
}

bool Radio::fsk_rx_finished()
{
    return bk4819.fsk_rx_finished();
}

void Radio::fsk_stop()
{
    bk4819.fsk_stop();
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
