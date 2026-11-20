/**
 * @file BK4819.cpp
 * @brief Hardware proxy for the BK4819 FM transceiver chip.
 *
 * The register values come from the F4HWN UV-K5 v3 reference firmware and the
 * BK4819 register documentation in its driver. The bit-banged bus is also
 * modelled on that driver, but wrapped in the same hardware-proxy style as the
 * ST7565 and ADC1 drivers.
 */

#include "BK4819.h"

#include "Gpio.h"
#include "SysTickTimer.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_gpio.h"

namespace platform
{
namespace
{

/**
 * @brief Register numbers used by this driver.
 *
 * The BK4819 has a large register file; only the registers needed for basic FM
 * receive and transmit are named here. The public read_register() and
 * write_register() functions still accept any address for experiments.
 */
constexpr uint8_t kRegSoftReset = 0x00;
constexpr uint8_t kRegInterruptStatus = 0x02;
constexpr uint8_t kRegInterruptRequest = 0x0C;
constexpr uint8_t kRegAgcTable0 = 0x10;
constexpr uint8_t kRegAgcTable1 = 0x11;
constexpr uint8_t kRegAgcTable2 = 0x12;
constexpr uint8_t kRegAgcTable3 = 0x13;
constexpr uint8_t kRegAgcTableMinus1 = 0x14;
constexpr uint8_t kRegMicAgc = 0x19;
constexpr uint8_t kRegConfig1F = 0x1F;
constexpr uint8_t kRegPower = 0x30;
constexpr uint8_t kRegGpio = 0x33;
constexpr uint8_t kRegPa = 0x36;
constexpr uint8_t kRegPowerControl = 0x37;
constexpr uint8_t kRegFrequencyLow = 0x38;
constexpr uint8_t kRegFrequencyHigh = 0x39;
constexpr uint8_t kRegConfig3E = 0x3E;
constexpr uint8_t kRegInterruptMask = 0x3F;
constexpr uint8_t kRegTxDeviation = 0x40;
constexpr uint8_t kRegFilter = 0x43;
constexpr uint8_t kRegAudioPath = 0x47;
constexpr uint8_t kRegAudioGain = 0x48;
constexpr uint8_t kRegAgcControl = 0x49;
constexpr uint8_t kRegSquelchCloseGlitch = 0x4D;
constexpr uint8_t kRegSquelchTiming = 0x4E;
constexpr uint8_t kRegSquelchNoise = 0x4F;
constexpr uint8_t kRegTxMute = 0x50;
constexpr uint8_t kRegSubAudio = 0x51;
constexpr uint8_t kRegTxConfig = 0x52;
constexpr uint8_t kRegFskConfig = 0x58;
constexpr uint8_t kRegFskControl = 0x59;
constexpr uint8_t kRegFskSyncFirst = 0x5A;
constexpr uint8_t kRegFskSyncSecond = 0x5B;
constexpr uint8_t kRegFskOptions = 0x5C;
constexpr uint8_t kRegFskLength = 0x5D;
constexpr uint8_t kRegFskFifoThreshold = 0x5E;
constexpr uint8_t kRegFskFifo = 0x5F;
constexpr uint8_t kRegGlitch = 0x63;
constexpr uint8_t kRegNoise = 0x65;
constexpr uint8_t kRegRssi = 0x67;
constexpr uint8_t kRegToneControl = 0x70;
constexpr uint8_t kRegTone2 = 0x72;
constexpr uint8_t kRegSquelchRssi = 0x78;
constexpr uint8_t kRegAgc = 0x7B;
constexpr uint8_t kRegMicGain = 0x7D;
constexpr uint8_t kRegAgcMode = 0x7E;

/** Board pins used by the serial bus. */
constexpr uint32_t kChipSelectPin = LL_GPIO_PIN_9; ///< PF9
constexpr uint32_t kClockPin = LL_GPIO_PIN_8;      ///< PB8
constexpr uint32_t kDataPin = LL_GPIO_PIN_9;       ///< PB9

/** The bus needs one microsecond between each clock and data edge. */
constexpr uint32_t kBusDelayUs = 1;

/** Frequencies below this value (10 Hz units) use the VHF signal path. */
constexpr uint32_t kVhfLimit = 28000000U;

void select_chip()
{
    gpio::set_low(GPIOF, kChipSelectPin);
}

void release_chip()
{
    gpio::set_high(GPIOF, kChipSelectPin);
}

void clock_low()
{
    gpio::set_low(GPIOB, kClockPin);
}

void clock_high()
{
    gpio::set_high(GPIOB, kClockPin);
}

void data_low()
{
    gpio::set_low(GPIOB, kDataPin);
}

void data_high()
{
    gpio::set_high(GPIOB, kDataPin);
}

/** Let the chip drive the data line for a register read. */
void data_input()
{
    LL_GPIO_SetPinMode(GPIOB, kDataPin, LL_GPIO_MODE_INPUT);
}

/** Return the data line to push-pull output after a register read. */
void data_output()
{
    LL_GPIO_SetPinMode(GPIOB, kDataPin, LL_GPIO_MODE_OUTPUT);
}

/**
 * Delay between filling the TX FIFO and enabling the modulator. The
 * reference firmware uses 20 ms; without it the first bytes can be lost.
 */
constexpr uint32_t kFskTxStartDelayMs = 20;

} // namespace

/** Definition of the single BK4819 instance declared in BK4819.h. */
BK4819 bk4819;

void BK4819::init()
{
    // Enable the GPIO clocks here as well, so the proxy also works when it is
    // used without the board module that normally enables them.
    LL_IOP_GRP1_EnableClock(
        LL_IOP_GRP1_PERIPH_GPIOB |
        LL_IOP_GRP1_PERIPH_GPIOF);

    // Set the idle levels before the pins become outputs, so the chip never
    // sees a spurious chip-select pulse or clock edge.
    gpio::set_high(GPIOF, kChipSelectPin);
    gpio::set_high(GPIOB, kClockPin | kDataPin);
    gpio::configure_output(GPIOF, kChipSelectPin);
    gpio::configure_output(GPIOB, kClockPin | kDataPin);

    // The data line is switched to input during register reads. Keep its
    // internal pull-up enabled so it has a defined idle level whenever the
    // chip is not driving it, matching the reference board setup.
    LL_GPIO_SetPinPull(GPIOB, kDataPin, LL_GPIO_PULL_UP);

    release_chip();
    clock_high();
    data_high();

    // Reset the chip's serial interface, then load the power-on settings.
    write_register(kRegSoftReset, 0x8000);
    write_register(kRegSoftReset, 0x0000);

    // The soft reset clears REG_59; keep the cached control value and the
    // FSK session state in sync.
    fsk_control_ = 0;
    fsk_configured_ = false;
    fsk_saved_deviation_ = 0;
    fsk_rx_expected_ = 0;
    fsk_rx_received_ = 0;
    fsk_rx_finished_ = false;
    fsk_rx_drained_ = true;

    write_register(kRegPowerControl, 0x1D0F);
    write_register(kRegPa, 0x0022);

    // Receiver automatic gain control table, from strongest to weakest input.
    // The values are taken from the reference firmware, which uses them for
    // both narrow and wide FM.
    write_register(kRegAgcTable3, 0x03BE);
    write_register(kRegAgcTable2, 0x037B);
    write_register(kRegAgcTable1, 0x027B);
    write_register(kRegAgcTable0, 0x007A);
    write_register(kRegAgcTableMinus1, 0x0019);
    write_register(kRegAgcControl, (84U << 7) | 56U);
    write_register(kRegAgc, 0x8420);

    // Clear the AGC "fix" bit so the chip adjusts its gain automatically and
    // select AGC table index 3 (the strongest-signal entry). The reference
    // firmware takes the same steps when it enables automatic gain control.
    const uint16_t agc_mode = read_register(kRegAgcMode);
    write_register(
        kRegAgcMode,
        static_cast<uint16_t>((agc_mode & ~(1U << 15) & ~(0x7U << 12)) |
                              (3U << 12)));

    // The microphone AGC is disabled; the transmit gain is set later.
    write_register(kRegMicAgc, 0x1041);
    write_register(kRegMicGain, 0xE940);

    // Audio path gain: bits 15:12 are the reference value 11, bits 9:4 set the
    // volume gain to 58/63, and bits 3:0 set the DAC gain to 8/15.
    write_register(kRegAudioGain, (11U << 12) | (58U << 4) | 8U);

    // Analog configuration values from the reference firmware. They are not
    // described in the public register summary, so they are kept as a block
    // with their original names.
    write_register(kRegConfig1F, 0x5454);
    write_register(kRegConfig3E, 0xA037);

    // Start from the reference REG_33 base value: the two reserved bits set
    // and every chip GPIO output low. The radio HAL and status LED enable the
    // receive path, antenna LNA, PA and LEDs later.
    gpio_state_ = 0x9000;
    write_register(kRegGpio, gpio_state_);

    // No interrupt sources are enabled during bring-up.
    write_register(kRegInterruptMask, 0x0000);

    // REG_51 bit 14 can turn the GPIO6 pin (green LED, package pin 2) into a
    // CDCSS input. Write the normal, no-subaudio value so the pin stays a
    // plain output. CTCSS and DCS are not used yet.
    write_register(kRegSubAudio, 0x0000);
}

uint16_t BK4819::read_register(uint8_t address)
{
    release_chip();
    clock_low();
    systick.delay_us(kBusDelayUs);

    select_chip();

    // Bit 7 tells the chip that this transfer is a read. The remaining seven
    // bits are the register number.
    write_u8(static_cast<uint8_t>(address | 0x80U));
    const uint16_t value = read_u16();

    release_chip();
    systick.delay_us(kBusDelayUs);

    clock_high();
    data_high();
    return value;
}

void BK4819::write_register(uint8_t address, uint16_t value)
{
    release_chip();
    clock_low();
    systick.delay_us(kBusDelayUs);

    select_chip();
    write_u8(address);
    systick.delay_us(kBusDelayUs);
    write_u16(value);
    systick.delay_us(kBusDelayUs);

    release_chip();
    systick.delay_us(kBusDelayUs);

    clock_high();
    data_high();
}

void BK4819::write_u8(uint8_t value)
{
    clock_low();

    for (uint8_t bit = 0; bit < 8; ++bit) {
        if ((value & 0x80U) != 0) {
            data_high();
        } else {
            data_low();
        }
        systick.delay_us(kBusDelayUs);

        clock_high();
        systick.delay_us(kBusDelayUs);

        value = static_cast<uint8_t>(value << 1);

        clock_low();
        systick.delay_us(kBusDelayUs);
    }
}

void BK4819::write_u16(uint16_t value)
{
    clock_low();

    for (uint8_t bit = 0; bit < 16; ++bit) {
        if ((value & 0x8000U) != 0) {
            data_high();
        } else {
            data_low();
        }
        systick.delay_us(kBusDelayUs);

        clock_high();
        systick.delay_us(kBusDelayUs);

        value = static_cast<uint16_t>(value << 1);

        clock_low();
        systick.delay_us(kBusDelayUs);
    }
}

uint16_t BK4819::read_u16()
{
    data_input();
    systick.delay_us(kBusDelayUs);

    uint16_t value = 0;
    for (uint8_t bit = 0; bit < 16; ++bit) {
        value = static_cast<uint16_t>(value << 1);
        if (gpio::read(GPIOB, kDataPin)) {
            value |= 1U;
        }

        clock_high();
        systick.delay_us(kBusDelayUs);
        clock_low();
        systick.delay_us(kBusDelayUs);
    }

    data_output();
    return value;
}

void BK4819::set_frequency(uint32_t frequency)
{
    // The synthesizer uses a 25-bit value split over two registers, in 10 Hz
    // units. Writing the low half first is what the reference driver does.
    write_register(kRegFrequencyLow, static_cast<uint16_t>(frequency & 0xFFFFU));
    write_register(kRegFrequencyHigh, static_cast<uint16_t>((frequency >> 16) & 0xFFFFU));
}

void BK4819::set_bandwidth(Bandwidth bandwidth)
{
    // REG_43 selects the IF filter and the channel-spacing mode. These are the
    // reference firmware's default values: the filter narrows automatically
    // when the received signal is weak.
    const uint16_t value = (bandwidth == Bandwidth::Narrow) ? 0x3448U : 0x3428U;
    write_register(kRegFilter, value);
}

void BK4819::set_audio_path(AudioPath path)
{
    // REG_47: bits 15:12 = 6 and bit 6 = 1 are the reference output-mode
    // values; bits 11:8 select Mute or FM.
    write_register(
        kRegAudioPath,
        static_cast<uint16_t>((6U << 12) |
                              (static_cast<uint8_t>(path) << 8) |
                              (1U << 6)));
}

void BK4819::set_squelch(const Squelch& settings)
{
    // REG_70 turns both tone generators off before their thresholds change.
    write_register(kRegToneControl, 0x0000);

    // REG_4D holds the glitch threshold used when the squelch closes.
    write_register(
        kRegSquelchCloseGlitch,
        static_cast<uint16_t>(0xA000U | settings.close_glitch));

    // REG_4E sets the open/close delays and the glitch threshold used when the
    // squelch opens.
    write_register(
        kRegSquelchTiming,
        static_cast<uint16_t>((1U << 14) |
                              (5U << 11) |
                              (6U << 9) |
                              settings.open_glitch));

    // REG_4F holds the close (high byte) and open (low byte) noise limits.
    write_register(
        kRegSquelchNoise,
        static_cast<uint16_t>((settings.close_noise << 8) |
                              settings.open_noise));

    // REG_78 holds the open (high byte) and close (low byte) RSSI limits.
    write_register(
        kRegSquelchRssi,
        static_cast<uint16_t>((settings.open_rssi << 8) |
                              settings.close_rssi));
}

void BK4819::set_antenna_path(uint32_t frequency)
{
    if (frequency == 0xFFFFFFFFU) {
        set_gpio(Gpio::VhfLna, false);
        set_gpio(Gpio::UhfLna, false);
        return;
    }

    const bool vhf = frequency < kVhfLimit;
    set_gpio(Gpio::VhfLna, vhf);
    set_gpio(Gpio::UhfLna, !vhf);
}

void BK4819::set_gpio(Gpio pin, bool enabled)
{
    // Each chip GPIO has one control bit in REG_33. The reference driver maps
    // pin 0 to bit 6, pin 1 to bit 5, pin 3 to bit 3 and pin 4 to bit 2.
    const uint16_t bit = static_cast<uint16_t>(0x40U >> static_cast<uint8_t>(pin));
    if (enabled) {
        gpio_state_ = static_cast<uint16_t>(gpio_state_ | bit);
    } else {
        gpio_state_ = static_cast<uint16_t>(gpio_state_ & ~bit);
    }

    write_register(kRegGpio, gpio_state_);
}

void BK4819::set_power_amplifier(uint8_t bias, uint32_t frequency)
{
    // REG_36: bias in the high byte, PA control enable in bit 7, and a gain
    // setting that depends on the band. The reference selects less gain below
    // 280 MHz and more gain above it.
    const uint16_t gain = (frequency < kVhfLimit)
        ? static_cast<uint16_t>(1U << 3)
        : static_cast<uint16_t>((4U << 3) | 2U);

    write_register(
        kRegPa,
        static_cast<uint16_t>((static_cast<uint16_t>(bias) << 8) |
                              (1U << 7) |
                              gain));
}

void BK4819::set_fsk_tx_mode(FskTxMode mode)
{
    // REG_58 bits 15:13 select the transmit modulation.
    const uint16_t value = read_register(kRegFskConfig);
    write_register(
        kRegFskConfig,
        static_cast<uint16_t>((value & ~(0x07U << 13)) |
                              ((static_cast<uint16_t>(mode) & 0x07U) << 13)));
}

void BK4819::set_fsk_rx_mode(FskRxMode mode)
{
    // REG_58 bits 12:10 select the receive modulation.
    const uint16_t value = read_register(kRegFskConfig);
    write_register(
        kRegFskConfig,
        static_cast<uint16_t>((value & ~(0x07U << 10)) |
                              ((static_cast<uint16_t>(mode) & 0x07U) << 10)));
}

void BK4819::set_fsk_rx_bandwidth(FskRxBandwidth bandwidth)
{
    // REG_58 bits 3:1 select the receive filter width.
    const uint16_t value = read_register(kRegFskConfig);
    write_register(
        kRegFskConfig,
        static_cast<uint16_t>((value & ~(0x07U << 1)) |
                              ((static_cast<uint16_t>(bandwidth) & 0x07U) << 1)));
}

void BK4819::set_fsk_rx_gain(uint8_t gain)
{
    if (gain > 3U) {
        gain = 3U;
    }

    // REG_58 bits 9:8 hold the receive gain.
    const uint16_t value = read_register(kRegFskConfig);
    write_register(
        kRegFskConfig,
        static_cast<uint16_t>((value & ~(0x03U << 8)) |
                              (static_cast<uint16_t>(gain) << 8)));
}

void BK4819::set_fsk_preamble_type(FskPreambleType type)
{
    // REG_58 bits 5:4 select the preamble bit pattern.
    const uint16_t value = read_register(kRegFskConfig);
    write_register(
        kRegFskConfig,
        static_cast<uint16_t>(
            (value & ~(0x03U << 4)) |
            ((static_cast<uint16_t>(type) & 0x03U) << 4)));
}

void BK4819::enable_fsk(bool enabled)
{
    // Bit 0 is the documented global FSK enable. The reference firmware also
    // writes 11 to bits 7:6, so this function does the same while enabling.
    uint16_t value = read_register(kRegFskConfig);
    value = static_cast<uint16_t>(value & ~((1U << 0) | (0x03U << 6)));
    if (enabled) {
        value = static_cast<uint16_t>(value | (1U << 0) | (0x03U << 6));
    }
    write_register(kRegFskConfig, value);
}

void BK4819::write_fsk_control()
{
    write_register(kRegFskControl, fsk_control_);
}

void BK4819::set_fsk_preamble_length(uint8_t bytes)
{
    // REG_59 bits 7:4 store preamble_bytes - 1, so 0 means one byte and 15
    // means sixteen bytes.
    if (bytes < 1U) {
        bytes = 1U;
    } else if (bytes > 16U) {
        bytes = 16U;
    }

    fsk_control_ = static_cast<uint16_t>(
        (fsk_control_ & ~(0x0FU << 4)) |
        ((static_cast<uint16_t>(bytes) - 1U) << 4));
    write_fsk_control();
}

void BK4819::set_fsk_sync_length(FskSyncLength length)
{
    // REG_59 bit 3 selects four sync bytes instead of two.
    fsk_control_ = static_cast<uint16_t>(fsk_control_ & ~(1U << 3));
    if (length == FskSyncLength::FourBytes) {
        fsk_control_ = static_cast<uint16_t>(fsk_control_ | (1U << 3));
    }
    write_fsk_control();
}

void BK4819::set_fsk_scrambler(bool enabled)
{
    // REG_59 bit 13 enables the chip data scrambler.
    fsk_control_ = static_cast<uint16_t>(fsk_control_ & ~(1U << 13));
    if (enabled) {
        fsk_control_ = static_cast<uint16_t>(fsk_control_ | (1U << 13));
    }
    write_fsk_control();
}

void BK4819::set_fsk_invert_tx(bool inverted)
{
    // REG_59 bit 9 inverts the data bits before modulation.
    fsk_control_ = static_cast<uint16_t>(fsk_control_ & ~(1U << 9));
    if (inverted) {
        fsk_control_ = static_cast<uint16_t>(fsk_control_ | (1U << 9));
    }
    write_fsk_control();
}

void BK4819::set_fsk_invert_rx(bool inverted)
{
    // REG_59 bit 10 inverts the detected data bits.
    fsk_control_ = static_cast<uint16_t>(fsk_control_ & ~(1U << 10));
    if (inverted) {
        fsk_control_ = static_cast<uint16_t>(fsk_control_ | (1U << 10));
    }
    write_fsk_control();
}

void BK4819::set_fsk_sync_word(uint16_t first, uint16_t second)
{
    // REG_5A holds sync bytes 0 and 1, REG_5B holds bytes 2 and 3. The high
    // byte is sent first.
    write_register(kRegFskSyncFirst, first);
    write_register(kRegFskSyncSecond, second);
}

void BK4819::set_fsk_crc(bool enabled)
{
    // Only bit 6 (CRC enable) is documented. The other bits select
    // undocumented options; 0x5625 and 0x5665 are the values used by working
    // raw-FSK and stock AIRCOPY code.
    write_register(kRegFskOptions, enabled ? 0x5665U : 0x5625U);
}

void BK4819::set_fsk_data_length(uint16_t bytes)
{
    // REG_5D stores length - 1 in 11 bits: the low 8 bits live in <15:8> and
    // the high 3 bits in <7:5>. The field counts data bytes, not preamble or
    // sync bytes.
    if (bytes < 1U) {
        bytes = 1U;
    } else if (bytes > 0x0800U) {
        bytes = 0x0800U;
    }

    const uint16_t value = static_cast<uint16_t>(bytes - 1U);
    write_register(
        kRegFskLength,
        static_cast<uint16_t>(((value & 0x00FFU) << 8) |
                              (((value >> 8) & 0x07U) << 5)));
}

void BK4819::set_fsk_fifo_thresholds(uint8_t tx_words, uint8_t rx_words)
{
    // REG_5E: TX almost-empty threshold in bits 9:3, RX almost-full
    // threshold in bits 2:0.
    if (tx_words > 127U) {
        tx_words = 127U;
    }
    if (rx_words > 7U) {
        rx_words = 7U;
    }

    const uint16_t value = read_register(kRegFskFifoThreshold);
    write_register(
        kRegFskFifoThreshold,
        static_cast<uint16_t>((value & ~0x03FFU) |
                              (static_cast<uint16_t>(tx_words) << 3) |
                              rx_words));
}

void BK4819::clear_fsk_tx_fifo()
{
    // Bit 15 is a self-clearing request, so write it and then return to the
    // cached control value.
    write_register(kRegFskControl,
                   static_cast<uint16_t>(fsk_control_ | (1U << 15)));
    write_fsk_control();
}

void BK4819::clear_fsk_rx_fifo()
{
    write_register(kRegFskControl,
                   static_cast<uint16_t>(fsk_control_ | (1U << 14)));
    write_fsk_control();
}

void BK4819::enable_fsk_tx(bool enabled)
{
    // REG_59 bit 11 starts and stops the transmitter.
    fsk_control_ = static_cast<uint16_t>(fsk_control_ & ~(1U << 11));
    if (enabled) {
        fsk_control_ = static_cast<uint16_t>(fsk_control_ | (1U << 11));
    }
    write_fsk_control();
}

void BK4819::enable_fsk_rx(bool enabled)
{
    // REG_59 bit 12 starts and stops the receiver.
    fsk_control_ = static_cast<uint16_t>(fsk_control_ & ~(1U << 12));
    if (enabled) {
        fsk_control_ = static_cast<uint16_t>(fsk_control_ | (1U << 12));
    }
    write_fsk_control();
}

void BK4819::write_fsk_fifo(uint16_t word)
{
    write_register(kRegFskFifo, word);
}

uint16_t BK4819::read_fsk_fifo()
{
    return read_register(kRegFskFifo);
}

bool BK4819::fsk_tx_finished()
{
    return (read_register(kRegInterruptStatus) &
            interrupt_fsk_tx_finished) != 0U;
}

void BK4819::set_fsk_gain(uint8_t gain)
{
    // REG_70 bit 7 enables Tone2, which the FSK modulator uses. Bits 6:0 set
    // its gain. Tone1 stays off, so it cannot add an unwanted continuous tone.
    if (gain > 127U) {
        gain = 127U;
    }
    write_register(
        kRegToneControl,
        static_cast<uint16_t>((1U << 7) | static_cast<uint16_t>(gain)));
}

void BK4819::set_fsk_clock(uint16_t control_word)
{
    // REG_72 is the Tone2/FSK frequency control word. In FFSK mode it sets
    // the symbol clock; in direct FSK mode it sets the bit clock.
    write_register(kRegTone2, control_word);
}

void BK4819::disable_tones()
{
    write_register(kRegToneControl, 0x0000);
}

uint16_t BK4819::tx_deviation()
{
    return read_register(kRegTxDeviation);
}

void BK4819::set_tx_deviation(uint16_t deviation)
{
    // REG_40 bits 11:0 set the deviation. Bits 15:12 include the enable and
    // other settings and are preserved.
    deviation = static_cast<uint16_t>(deviation & 0x0FFFU);
    const uint16_t value = read_register(kRegTxDeviation);
    write_register(
        kRegTxDeviation,
        static_cast<uint16_t>((value & 0xF000U) | deviation));
}

void BK4819::fsk_configure(const FskConfig& config)
{
    // Keep a sanitized copy. The setters clamp the register fields again, but
    // fsk_read() uses the stored threshold and the start functions re-apply
    // the tone registers, so the cached values must be in range too.
    fsk_config_ = config;
    if (fsk_config_.preamble_bytes < 1U) {
        fsk_config_.preamble_bytes = 1U;
    } else if (fsk_config_.preamble_bytes > 16U) {
        fsk_config_.preamble_bytes = 16U;
    }
    if (fsk_config_.rx_gain > 3U) {
        fsk_config_.rx_gain = 3U;
    }
    if (fsk_config_.tx_fifo_threshold > 127U) {
        fsk_config_.tx_fifo_threshold = 127U;
    }
    if (fsk_config_.rx_fifo_threshold > 7U) {
        fsk_config_.rx_fifo_threshold = 7U;
    }
    if (fsk_config_.gain > 127U) {
        fsk_config_.gain = 127U;
    }
    fsk_config_.deviation =
        static_cast<uint16_t>(fsk_config_.deviation & 0x0FFFU);

    // Save the voice deviation before the first FSK session. A second
    // fsk_configure() must not overwrite it with the FSK value.
    if (!fsk_configured_) {
        fsk_saved_deviation_ = tx_deviation();
        fsk_configured_ = true;
    }

    // Abort any receive session left over from a previous configuration.
    fsk_rx_expected_ = 0;
    fsk_rx_received_ = 0;
    fsk_rx_finished_ = false;
    fsk_rx_drained_ = true;

    apply_fsk_config();
}

void BK4819::apply_fsk_config()
{
    // Changing modulation registers while a direction is enabled can inject
    // garbage into a FIFO or the modulator, so stop both directions and the
    // modem itself before reprogramming it.
    enable_fsk_tx(false);
    enable_fsk_rx(false);
    enable_fsk(false);

    // REG_58: modulation, receive bandwidth and gain.
    set_fsk_tx_mode(fsk_config_.tx_mode);
    set_fsk_rx_mode(fsk_config_.rx_mode);
    set_fsk_rx_bandwidth(fsk_config_.rx_bandwidth);
    set_fsk_rx_gain(fsk_config_.rx_gain);
    set_fsk_preamble_type(fsk_config_.preamble_type);

    // REG_59: preamble, sync length and data path options.
    set_fsk_preamble_length(fsk_config_.preamble_bytes);
    set_fsk_sync_length(fsk_config_.sync_length);
    set_fsk_scrambler(fsk_config_.scrambler);
    set_fsk_invert_tx(fsk_config_.invert_tx);
    set_fsk_invert_rx(fsk_config_.invert_rx);

    // REG_5A/5B: sync bytes. The second pair is ignored in 2-byte mode.
    set_fsk_sync_word(
        fsk_config_.sync_first,
        (fsk_config_.sync_length == FskSyncLength::FourBytes)
            ? fsk_config_.sync_second
            : 0x0000U);

    // REG_5C: the chip CRC is off by default; the layer above owns
    // integrity checking.
    set_fsk_crc(fsk_config_.crc);

    // REG_5E: FIFO thresholds.
    set_fsk_fifo_thresholds(
        fsk_config_.tx_fifo_threshold,
        fsk_config_.rx_fifo_threshold);

    // REG_70/72 and REG_40: tone gain, bit clock and deviation.
    apply_fsk_tones();
    set_tx_deviation(fsk_config_.deviation);

    // The modem is enabled but neither direction is started. Clear stale
    // FIFO contents and status flags before the first transfer.
    enable_fsk(true);
    clear_fsk_tx_fifo();
    clear_fsk_rx_fifo();

    // Enable the status bits the session functions use without disturbing
    // any other interrupt source.
    set_interrupt_mask(
        interrupt_mask() |
        interrupt_fsk_tx_finished |
        interrupt_fsk_rx_finished |
        interrupt_fsk_fifo_almost_full |
        interrupt_fsk_rx_sync);
    clear_interrupts();
}

void BK4819::apply_fsk_tones()
{
    // Another subsystem may program REG_70 for squelch or voice tones, so
    // re-apply the FSK gain and clock before every transfer.
    set_fsk_gain(fsk_config_.gain);
    set_fsk_clock(fsk_config_.clock);
}

void BK4819::write_fsk_bytes(const uint8_t* data, uint16_t length)
{
    // The FIFO takes 16-bit words and the chip sends the low byte first.
    // For an odd length the high byte of the last word is a don't-care.
    for (uint16_t i = 0; i < length; i += 2U) {
        uint16_t word = data[i];
        if (static_cast<uint16_t>(i + 1U) < length) {
            word = static_cast<uint16_t>(
                word | (static_cast<uint16_t>(data[i + 1U]) << 8));
        }
        write_fsk_fifo(word);
    }
}

bool BK4819::fsk_start_tx(const uint8_t* data, uint16_t length)
{
    if (!fsk_configured_ || (data == nullptr) || (length == 0U) ||
        (length > fsk_tx_fifo_bytes)) {
        return false;
    }

    // The modem is half-duplex. Abort any receive session and stop both
    // directions before touching the FIFO.
    fsk_rx_expected_ = 0;
    fsk_rx_received_ = 0;
    fsk_rx_finished_ = false;
    fsk_rx_drained_ = true;
    enable_fsk_rx(false);
    enable_fsk_tx(false);

    clear_fsk_tx_fifo();
    set_fsk_data_length(length);
    apply_fsk_tones();

    // Make sure the modem itself is on. fsk_configure() enables it, but a
    // manual enable_fsk(false) experiment or a previous stop may have left it
    // off. TX stays disabled until the FIFO is full and the delay has passed.
    enable_fsk(true);
    clear_interrupts();
    write_fsk_bytes(data, length);

    // The reference sequence waits after filling the FIFO and before
    // enabling the modulator. Without this delay the first bytes can be lost.
    systick.delay_ms(kFskTxStartDelayMs);

    enable_fsk_tx(true);
    return true;
}

bool BK4819::fsk_start_rx(uint16_t length)
{
    if (!fsk_configured_ || (length == 0U) ||
        (length > fsk_max_rx_bytes)) {
        return false;
    }

    // The modem is half-duplex. Stop both directions and start a new receive
    // session with a clean FIFO and status.
    enable_fsk_tx(false);
    enable_fsk_rx(false);

    fsk_rx_expected_ = length;
    fsk_rx_received_ = 0;
    fsk_rx_finished_ = false;
    fsk_rx_drained_ = false;

    clear_fsk_rx_fifo();
    set_fsk_data_length(length);

    // Radio::receive() programs the shared REG_70 through set_squelch(), so
    // restore the FSK tone setup before enabling the receiver.
    apply_fsk_tones();

    enable_fsk(true);
    clear_interrupts();
    enable_fsk_rx(true);
    return true;
}

uint16_t BK4819::fsk_read(uint8_t* buffer, uint16_t capacity)
{
    if ((buffer == nullptr) || (capacity == 0U) || !fsk_configured_ ||
        fsk_rx_drained_) {
        return 0;
    }

    const uint16_t status = interrupt_status();
    const bool almost_full =
        (status & interrupt_fsk_fifo_almost_full) != 0U;
    const bool finished =
        (status & interrupt_fsk_rx_finished) != 0U;
    if (finished) {
        fsk_rx_finished_ = true;
    }
    const bool done = finished || fsk_rx_finished_;

    if (!almost_full && !done) {
        return 0;
    }

    const uint16_t remaining = (fsk_rx_received_ < fsk_rx_expected_)
        ? static_cast<uint16_t>(fsk_rx_expected_ - fsk_rx_received_)
        : 0U;
    if (remaining == 0U) {
        fsk_rx_finished_ = true;
        fsk_rx_drained_ = true;
        enable_fsk_rx(false);
        return 0;
    }

    uint16_t words = static_cast<uint16_t>((remaining + 1U) / 2U);
    if (words > fsk_rx_fifo_words) {
        words = fsk_rx_fifo_words;
    }

    if (!done) {
        // The almost-full flag only guarantees the configured threshold, so
        // read exactly that many words and let the next flag ask for more.
        uint16_t threshold = fsk_config_.rx_fifo_threshold;
        if (threshold == 0U) {
            threshold = 1U;
        }
        if (words > threshold) {
            words = threshold;
        }
    }

    // The FIFO is word-oriented. Dropping the high byte of a word is only
    // safe when that word is the last one of the block, because then the high
    // byte is padding. If the buffer cannot hold everything that is left,
    // consume whole words only; the caller gets another chance later.
    if (capacity < remaining) {
        const uint16_t whole_words =
            static_cast<uint16_t>(capacity / 2U);
        if (words > whole_words) {
            words = whole_words;
        }
    }
    if (words == 0U) {
        return 0;
    }

    uint16_t written = 0;
    for (uint16_t i = 0; i < words; ++i) {
        const uint16_t word = read_fsk_fifo();

        if ((written < capacity) &&
            (fsk_rx_received_ < fsk_rx_expected_)) {
            buffer[written++] = static_cast<uint8_t>(word & 0x00FFU);
            ++fsk_rx_received_;
        }
        if ((written < capacity) &&
            (fsk_rx_received_ < fsk_rx_expected_)) {
            buffer[written++] =
                static_cast<uint8_t>((word >> 8) & 0x00FFU);
            ++fsk_rx_received_;
        }
    }

    if (fsk_rx_received_ >= fsk_rx_expected_) {
        // Every byte of the block has been handed to the caller. Stop the
        // receiver; a partially drained block keeps the latched finished flag
        // set, so the next ready()/fsk_read() call can pick up the rest.
        fsk_rx_finished_ = true;
        fsk_rx_drained_ = true;
        enable_fsk_rx(false);
    }

    clear_interrupts();
    return written;
}

bool BK4819::fsk_rx_data_ready()
{
    if (!fsk_configured_ || fsk_rx_drained_ ||
        (fsk_rx_expected_ == 0U)) {
        return false;
    }
    if (fsk_rx_finished_) {
        return true;
    }

    const uint16_t status = interrupt_status();
    return ((status & interrupt_fsk_fifo_almost_full) != 0U) ||
           ((status & interrupt_fsk_rx_finished) != 0U);
}

bool BK4819::fsk_rx_finished()
{
    if (fsk_rx_finished_) {
        return true;
    }
    if (fsk_configured_ && (fsk_rx_expected_ != 0U) &&
        ((interrupt_status() & interrupt_fsk_rx_finished) != 0U)) {
        fsk_rx_finished_ = true;
        return true;
    }
    return false;
}

bool BK4819::fsk_rx_sync()
{
    return (interrupt_status() & interrupt_fsk_rx_sync) != 0U;
}

void BK4819::fsk_stop()
{
    enable_fsk_tx(false);
    enable_fsk_rx(false);
    enable_fsk(false);
    disable_tones();

    // Restore the voice deviation only if fsk_configure() saved one. This
    // makes fsk_stop() safe to call twice.
    if (fsk_configured_) {
        set_tx_deviation(fsk_saved_deviation_);
        fsk_configured_ = false;
    }

    // Remove the FSK interrupt sources and clear any latched flags.
    set_interrupt_mask(
        interrupt_mask() &
        ~(interrupt_fsk_tx_finished |
          interrupt_fsk_rx_finished |
          interrupt_fsk_fifo_almost_full |
          interrupt_fsk_rx_sync));
    clear_interrupts();

    clear_fsk_tx_fifo();
    clear_fsk_rx_fifo();

    fsk_rx_expected_ = 0;
    fsk_rx_received_ = 0;
    fsk_rx_finished_ = false;
    fsk_rx_drained_ = true;
}

void BK4819::receive()
{
    // REG_37 enables the internal voltage regulators, DSP, crystal and band
    // gap. REG_30 then enables the receive chain, PLL, discriminator, AF DAC
    // and receive DSP, with the transmitter blocks left off.
    write_register(kRegPowerControl, 0x1F0F);
    write_register(kRegPower, 0x0000);
    write_register(kRegPower, 0xBFF1);
}

void BK4819::transmit()
{
    // Mute the audio output while the transmit path starts up.
    set_audio_path(AudioPath::Mute);

    // The reference "exit bypass" step changes the transmit DC filter to its
    // normal setting. Only bits 5:3 change; everything else is preserved.
    const uint16_t agc_mode = read_register(kRegAgcMode);
    write_register(
        kRegAgcMode,
        static_cast<uint16_t>((agc_mode & ~(0x7U << 3)) | (5U << 3)));

    // Unmute the transmit modulator and enable the transmit chain.
    write_register(kRegTxMute, 0x3B20);
    write_register(kRegPowerControl, 0x1D0F);
    write_register(kRegTxConfig, 0x028F);
    write_register(kRegPower, 0x0000);
    write_register(kRegPower, 0xC1FE);
}

void BK4819::idle()
{
    write_register(kRegPower, 0x0000);
}

void BK4819::sleep()
{
    write_register(kRegPower, 0x0000);
    write_register(kRegPowerControl, 0x1D00);
}

uint16_t BK4819::rssi()
{
    return static_cast<uint16_t>(read_register(kRegRssi) & 0x01FFU);
}

uint8_t BK4819::noise()
{
    return static_cast<uint8_t>(read_register(kRegNoise) & 0x007FU);
}

uint8_t BK4819::glitch()
{
    return static_cast<uint8_t>(read_register(kRegGlitch) & 0x00FFU);
}

bool BK4819::interrupt_requested()
{
    return (read_register(kRegInterruptRequest) & 1U) != 0;
}

uint16_t BK4819::interrupt_status()
{
    return read_register(kRegInterruptStatus);
}

void BK4819::clear_interrupts()
{
    write_register(kRegInterruptStatus, 0x0000);
}

void BK4819::set_interrupt_mask(uint16_t mask)
{
    write_register(kRegInterruptMask, mask);
}

uint16_t BK4819::interrupt_mask()
{
    return read_register(kRegInterruptMask);
}

} // namespace platform
