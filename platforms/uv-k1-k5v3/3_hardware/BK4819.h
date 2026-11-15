/**
 * @file BK4819.h
 * @brief Hardware proxy for the BK4819 FM transceiver chip.
 *
 * The BK4819 contains the receiver, transmitter, frequency synthesizer,
 * filters, squelch detector and audio path. This driver exposes those parts
 * through the chip's register interface. It knows nothing about channels,
 * squelch levels or user settings; choosing those values is the job of the
 * portable radio role above it.
 *
 * The chip is connected through a three-wire serial bus that is bit-banged on
 * GPIO pins because it does not match the MCU's SPI peripheral:
 *
 *     PF9  chip select, active low
 *     PB8  serial clock
 *     PB9  serial data, bidirectional
 *
 * A register transfer sends the 8-bit register address and then 16 bits of
 * data, most significant bit first. A read sends the address with bit 7 set
 * and then reads 16 bits back from the chip. The bus is clocked slowly with a
 * one-microsecond wait on each edge, which is easy to follow and well inside
 * the chip's timing limits.
 *
 * Frequencies passed to this driver are in 10 Hz units, matching the chip's
 * tuning registers. For example 43400000 means 434.000 MHz. The portable
 * Radio role converts from hertz before calling in.
 */

#ifndef PLATFORM_BK4819_H_
#define PLATFORM_BK4819_H_

#include <stdint.h>

namespace platform
{

/**
 * @brief Driver for the BK4819 transceiver.
 *
 * Typical use:
 *
 *     bk4819.init();
 *     bk4819.set_bandwidth(BK4819::Bandwidth::Wide);
 *     bk4819.set_frequency(43350000);          // 433.500 MHz
 *     bk4819.set_antenna_path(43350000);
 *     bk4819.receive();
 *     bk4819.set_gpio(BK4819::Gpio::RxEnable, true);
 *     bk4819.set_audio_path(BK4819::AudioPath::Fm);
 *
 * The proxy does not add delays around mode changes. The caller is responsible
 * for the settling times the board and chip need, which live in the radio HAL.
 */
struct BK4819
{
    /** Receiver filter bandwidth selected with set_bandwidth(). */
    enum class Bandwidth : uint8_t
    {
        Wide,   ///< 25 kHz channel spacing, approximately 4.5 kHz IF filter.
        Narrow, ///< 12.5 kHz channel spacing, approximately 3.0 kHz IF filter.
    };

    /** Audio output path selected with set_audio_path(). */
    enum class AudioPath : uint8_t
    {
        Mute = 0,
        Fm = 1,
    };

    /**
     * @brief GPIO output selectors for the chip's own GPIO pins.
     *
     * REG_33 controls the chip's GPIO outputs. Each selector below maps to one
     * register bit through `0x40 >> selector`. The values and package pins come
     * from the reference firmware; they are BK4819 pins, not MCU pins.
     *
     * On this board GPIO5 (package pin 1) drives the red status LED and GPIO6
     * (package pin 2) drives the green one. Both are ordinary GPIO outputs that
     * the firmware has to switch itself.
     */
    enum class Gpio : uint8_t
    {
        RxEnable = 0, ///< GPIO0, package pin 28, receiver enable.
        PaEnable = 1, ///< GPIO1, package pin 29, external PA enable.
        UhfLna = 3,   ///< GPIO3, package pin 31, UHF low-noise amplifier.
        VhfLna = 4,   ///< GPIO4, package pin 32, VHF low-noise amplifier.
        GreenLed = 5, ///< GPIO6, package pin 2, green status LED.
        RedLed = 6,   ///< GPIO5, package pin 1, red status LED.
    };

    /**
     * @brief Squelch thresholds passed to set_squelch().
     *
     * RSSI and glitch thresholds are 8-bit values. Noise thresholds are 7-bit
     * values. The defaults describe a fully open squelch, which is useful
     * during bring-up when there are no channel settings yet.
     */
    struct Squelch
    {
        uint8_t open_rssi = 0;
        uint8_t close_rssi = 0;
        uint8_t open_noise = 127;
        uint8_t close_noise = 127;
        uint8_t close_glitch = 255;
        uint8_t open_glitch = 255;
    };

    /**
     * @brief Modulation used by the FSK data modem.
     *
     * The BK4819 contains a hardware 2-FSK modem. It moves raw bytes between
     * two radios; it does not know about AX.25, HDLC, flags, bit stuffing or
     * checksums. Those belong in the layer above.
     *
     * Ffsk1200_1800 is the recommended starting point. It sends 1200 bps with
     * 1200 Hz and 1800 Hz tones. This is not Bell 202 (1200/2200 Hz), so a
     * hardware TNC that expects Bell 202 may not decode it. Software TNCs
     * such as Dire Wolf do.
     *
     * Fsk1200 is the mode the stock AIRCOPY feature uses: 1200 bps direct
     * FSK without the 1200/1800 audio tones. It is useful for two radios
     * running the same firmware, but it is not an AFSK mode.
     *
     * Ffsk1200_2400 is listed in the datasheet, but at least one UV-K5
     * implementation reports that it does not work reliably. Prefer
     * Ffsk1200_1800 unless you are experimenting.
     */
    enum class FskMode : uint8_t
    {
        Fsk1200,       ///< Direct FSK, 1200 bps, no 1200/1800 audio tones.
        Ffsk1200_1800, ///< FFSK, 1200 bps, 1200/1800 Hz tones.
        Ffsk1200_2400, ///< FFSK, 1200 bps, 1200/2400 Hz tones.
    };

    /**
     * @brief Settings for the FSK data modem.
     *
     * The modem adds the minimal framing another BK4819 needs to find the
     * start of the data:
     *
     * - a preamble of 1 to 16 bytes, generated by the chip,
     * - a 2- or 4-byte sync word,
     * - the data bytes themselves.
     *
     * The block length is not sent over the air. The transmitter and receiver
     * must call fsk_send() and fsk_receive() with the same length. A higher
     * layer that needs variable-length messages can put a length byte in its
     * payload or define its own framing.
     *
     * The chip's optional CRC and scrambler are deliberately not used. They
     * are not AX.25-compatible, and the chip's CRC length rules are not
     * documented clearly. Add integrity checking in the layer above; AX.25
     * already carries its own FCS.
     *
     * Both radios must use the same mode, preamble length, sync word and
     * block length. The defaults below are the settings used by this firmware.
     */
    struct FskConfig
    {
        FskMode mode = FskMode::Ffsk1200_1800;

        /// Preamble length in bytes, 1 to 16. Values outside this range are
        /// clamped by fsk_configure().
        uint8_t preamble_bytes = 8;

        /// First two sync bytes, sent first. This value goes into REG_5A.
        uint16_t sync_first = 0x2DD4;

        /// Last two sync bytes. This value goes into REG_5B and is only used
        /// when four_byte_sync is true.
        uint16_t sync_second = 0x5AA5;

        /// true selects a 4-byte sync word, false selects a 2-byte sync word.
        bool four_byte_sync = true;

        /// Raw RF deviation value (REG_40 bits 11:0). 1200 is approximately
        /// 3 kHz in the reference measurements and suits a 12.5 kHz channel.
        uint16_t deviation = 1200;

        /// Tone2/FSK gain, 0 to 127. Values above 127 are clamped. Higher
        /// values give a louder modulation; 96 is the reference value.
        uint8_t gain = 96;
    };

    /** REG_3F/REG_02 bit for "squelch opened". */
    static constexpr uint16_t interrupt_squelch_found = 1U << 3;

    /** REG_3F/REG_02 bit for "squelch closed". */
    static constexpr uint16_t interrupt_squelch_lost = 1U << 2;

    /** REG_3F/REG_02 bit for "FSK transmitter finished". */
    static constexpr uint16_t interrupt_fsk_tx_finished = 1U << 15;

    /** REG_3F/REG_02 bit for "FSK TX FIFO almost empty". */
    static constexpr uint16_t interrupt_fsk_fifo_almost_empty = 1U << 14;

    /** REG_3F/REG_02 bit for "FSK receiver finished". */
    static constexpr uint16_t interrupt_fsk_rx_finished = 1U << 13;

    /** REG_3F/REG_02 bit for "FSK RX FIFO almost full". */
    static constexpr uint16_t interrupt_fsk_fifo_almost_full = 1U << 12;

    /** REG_3F/REG_02 bit for "FSK receiver found the sync word". */
    static constexpr uint16_t interrupt_fsk_rx_sync = 1U << 1;

    /** Largest block the FSK API accepts, in bytes. */
    static constexpr uint16_t fsk_max_frame = 256;

    /**
     * @brief Configure the bus pins and load the chip's power-on settings.
     *
     * This leaves the chip idle: the receiver and transmitter are off and the
     * audio path is muted. Call receive() or transmit() afterwards.
     */
    void init();

    /**
     * @brief Read one 16-bit register.
     *
     * @param address Register number, 0x00 to 0xFF.
     */
    uint16_t read_register(uint8_t address);

    /**
     * @brief Write one 16-bit register.
     *
     * @param address Register number, 0x00 to 0xFF.
     * @param value   New register value.
     */
    void write_register(uint8_t address, uint16_t value);

    /**
     * @brief Program the synthesizer frequency.
     *
     * @param frequency Frequency in 10 Hz units, for example 43350000 for
     *                  433.500 MHz.
     */
    void set_frequency(uint32_t frequency);

    /** Select the receiver filter bandwidth. */
    void set_bandwidth(Bandwidth bandwidth);

    /** Select what the chip sends to the audio output pin. */
    void set_audio_path(AudioPath path);

    /** Program all squelch thresholds at once. */
    void set_squelch(const Squelch& settings);

    /**
     * @brief Select the VHF or UHF signal path for a frequency.
     *
     * Frequencies below 28 MHz (in 10 Hz units, so 28000000) use the VHF LNA,
     * everything else uses the UHF LNA. Pass 0xFFFFFFFF to turn both off.
     *
     * @param frequency Frequency in 10 Hz units.
     */
    void set_antenna_path(uint32_t frequency);

    /**
     * @brief Set one of the chip's own GPIO output pins.
     *
     * The outputs are controlled through REG_33. The driver keeps the other
     * GPIO states unchanged.
     */
    void set_gpio(Gpio pin, bool enabled);

    /**
     * @brief Configure the power amplifier control output.
     *
     * @param bias      PA bias value, 0 to 255. Higher values give more drive.
     * @param frequency Frequency in 10 Hz units, used to pick the PA gain.
     */
    void set_power_amplifier(uint8_t bias, uint32_t frequency);

    /**
     * @brief Program the FSK modem and remember the configuration.
     *
     * The modem is bit-transparent: it sends the bytes exactly as supplied,
     * after its own preamble and sync word. It does not add NRZI, bit
     * stuffing, flags, a length byte or a checksum.
     *
     * The radio path still has to be powered by the layer above. A clean
     * transmit sequence is:
     *
     *     bk4819.fsk_configure(config);   // while the PA is still off
     *     radio.transmit(433500000U);     // powers the TX chain and PA
     *     bk4819.fsk_send(data, length);
     *     while (!bk4819.fsk_tx_finished()) {}
     *     bk4819.fsk_stop();
     *     radio.idle();
     *
     * For receive, power the receiver first and configure afterwards, because
     * Radio::receive() programs the shared REG_70 tone/gain register:
     *
     *     radio.receive(433500000U);
     *     bk4819.fsk_configure(config);
     *     bk4819.fsk_receive(length);
     *     while (!bk4819.fsk_poll()) {}
     *     const uint16_t n = bk4819.fsk_read(buffer, sizeof(buffer));
     *     bk4819.fsk_stop();
     *
     * fsk_stop() restores the voice deviation and turns the modem off.
     */
    void fsk_configure(const FskConfig& config);

    /**
     * @brief Load a block into the TX FIFO and start sending it.
     *
     * The block is sent after the configured preamble and sync word. The
     * maximum length is fsk_max_frame (256 bytes), the size of the chip's TX
     * FIFO. The function returns immediately; poll fsk_tx_finished() before
     * turning the transmitter off. At 1200 bps a 256-byte block takes about
     * 1.7 seconds on air.
     *
     * This function includes the 20 ms settling delay the reference firmware
     * uses between filling the FIFO and enabling the modulator.
     *
     * @param data   Bytes to send. Must not be null.
     * @param length Number of bytes, 1 to fsk_max_frame.
     * @return true if the block was accepted, false on a bad length or pointer.
     */
    bool fsk_send(const uint8_t* data, uint16_t length);

    /**
     * @brief Arm the receiver for a block of exactly length bytes.
     *
     * The chip waits for the sync word, then fills its RX FIFO. Call
     * fsk_poll() regularly; at 1200 bps it must run at least every 50 ms so
     * the 16-byte RX FIFO does not overflow.
     *
     * Both ends must use the same length. If the sender stops early, the
     * receiver stays armed until a new call or fsk_stop().
     *
     * @param length Number of bytes to receive, 1 to fsk_max_frame.
     * @return true if the receiver was armed, false on a bad length.
     */
    bool fsk_receive(uint16_t length);

    /**
     * @brief Service the FSK receiver and check for a complete block.
     *
     * Drains the chip's RX FIFO into the internal block buffer and returns
     * true once the requested number of bytes has arrived. The data stays in
     * the buffer until the next fsk_receive() or fsk_stop().
     *
     * A received length smaller than the requested length means the carrier
     * dropped before all bytes arrived.
     */
    bool fsk_poll();

    /**
     * @brief Copy the received block out of the driver.
     *
     * @param buffer   Destination buffer. Must not be null.
     * @param capacity Size of the destination in bytes.
     * @return Number of bytes copied, at most capacity and the number
     *         received.
     */
    uint16_t fsk_read(uint8_t* buffer, uint16_t capacity) const;

    /** @return the number of bytes in the current or last received block. */
    uint16_t fsk_received_length() const;

    /**
     * @brief Check whether the transmitter has sent the whole block.
     *
     * This reports that the modulator has finished the FIFO. It says nothing
     * about the external PA, antenna path or whether the other radio heard it.
     */
    bool fsk_tx_finished();

    /**
     * @brief Turn the FSK modem off and restore the voice deviation.
     *
     * The caller still owns the radio direction, frequency, antenna path and
     * PA. Call radio.idle() or radio.receive() afterwards.
     */
    void fsk_stop();

    /** Power up the receiver and its DSP. */
    void receive();

    /**
     * @brief Power up the transmitter and its DSP.
     *
     * This does not enable the external PA. The caller selects the antenna
     * path, enables Gpio::PaEnable and programs the bias afterwards.
     */
    void transmit();

    /** Stop the receiver and transmitter, leaving the chip powered. */
    void idle();

    /** Put the chip into its lowest-power state. */
    void sleep();

    /** @return the raw 9-bit RSSI value (0 to 511). */
    uint16_t rssi();

    /** @return the raw 7-bit noise indicator (0 to 127). */
    uint8_t noise();

    /** @return the raw 8-bit glitch indicator (0 to 255). */
    uint8_t glitch();

    /** @return true while the chip is asking the MCU for attention. */
    bool interrupt_requested();

    /** @return the latched interrupt flags, one bit per event. */
    uint16_t interrupt_status();

    /** Acknowledge and clear the latched interrupt flags. */
    void clear_interrupts();

    /** Select which events may assert the interrupt request. */
    void set_interrupt_mask(uint16_t mask);

private:
    /** Build the REG_59 value for the current preamble and sync settings. */
    uint16_t fsk_control_value() const;

    /** Convert a byte count to the REG_5D length field. */
    static uint16_t fsk_length_value(uint16_t length);

    /** Program the mode-dependent FSK registers (REG_58, 70, 71, 72). */
    void write_fsk_mode();

    /** Read words from the RX FIFO into the block buffer. */
    void read_fsk_fifo(uint16_t words);

    /** Clock one byte out on the serial bus, most significant bit first. */
    void write_u8(uint8_t value);

    /** Clock one 16-bit word out on the serial bus. */
    void write_u16(uint16_t value);

    /** Switch the data pin to input and clock one 16-bit word in. */
    uint16_t read_u16();

    /** Latched state of the chip's GPIO outputs, used by set_gpio(). */
    uint16_t gpio_state_ = 0x9000;

    /** Settings from the last fsk_configure() call. */
    FskConfig fsk_config_;

    /** Voice deviation read from REG_40 before the first FSK setup. */
    uint16_t fsk_saved_deviation_ = 0;

    /** True between fsk_configure() and fsk_stop(). */
    bool fsk_configured_ = false;

    /** True while fsk_receive() is collecting a block. */
    bool fsk_rx_active_ = false;

    /** True after fsk_poll() has collected a complete block. */
    bool fsk_rx_finished_ = false;

    /** Number of bytes currently stored in fsk_rx_buffer_. */
    uint16_t fsk_rx_length_ = 0;

    /** Number of bytes the current block should contain. */
    uint16_t fsk_rx_expected_ = 0;

    /** Received block. The size matches the chip's TX FIFO limit. */
    uint8_t fsk_rx_buffer_[fsk_max_frame];
};

/** The single BK4819 instance used by this platform. */
extern BK4819 bk4819;

} // namespace platform

#endif // PLATFORM_BK4819_H_
