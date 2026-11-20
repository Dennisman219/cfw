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
     * @brief Transmit mode field of REG_58.
     *
     * The chip has one 2-FSK modulator. The FFSK modes use a fixed 1200 bps
     * symbol rate and generate the audio tones themselves. Direct FSK uses
     * the bit clock programmed with set_fsk_clock() and no audio tones.
     */
    enum class FskTxMode : uint8_t
    {
        Fsk1200_2400 = 0,  ///< Direct FSK, 1.2 or 2.4 kbps.
        Ffsk1200_1800 = 1, ///< FFSK, 1200 bps, 1200/1800 Hz tones.
        Ffsk1200_2400 = 3, ///< FFSK, 1200 bps, 1200/2400 Hz tones.
        NoaaSame = 5,      ///< NOAA SAME transmitter.
    };

    /** Receive mode field of REG_58. */
    enum class FskRxMode : uint8_t
    {
        Fsk1200_2400 = 0,  ///< Direct FSK, 1.2 or 2.4 kbps.
        Ffsk1200_1800 = 7, ///< FFSK, 1200 bps, 1200/1800 Hz tones.
        Ffsk1200_2400 = 4, ///< FFSK, 1200 bps, 1200/2400 Hz tones.
    };

    /** Receive bandwidth field of REG_58. */
    enum class FskRxBandwidth : uint8_t
    {
        Fsk1200 = 0,                ///< Direct FSK, 1200 bps.
        Ffsk1200_1800 = 1,          ///< FFSK 1200/1800.
        NoaaSame = 2,               ///< NOAA SAME.
        Fsk2400OrFfsk1200_2400 = 4, ///< Direct FSK 2.4 kbps or FFSK 1200/2400.
    };

    /** Preamble bit pattern field of REG_58. */
    enum class FskPreambleType : uint8_t
    {
        SyncWordMsb = 0, ///< 0xAA or 0x55, chosen by the MSB of sync byte 0.
        Fixed0x55 = 2,   ///< Always 0x55.
        Fixed0xAA = 3,   ///< Always 0xAA.
    };

    /** Sync word length field of REG_59. */
    enum class FskSyncLength : uint8_t
    {
        TwoBytes,  ///< Sync bytes 0 and 1 from REG_5A only.
        FourBytes, ///< Sync bytes 0 to 3 from REG_5A and REG_5B.
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

    /** Number of 16-bit words in the FSK TX FIFO (256 bytes). */
    static constexpr uint16_t fsk_tx_fifo_words = 128;

    /** Number of 16-bit words in the FSK RX FIFO (16 bytes). */
    static constexpr uint16_t fsk_rx_fifo_words = 8;

    /** Number of bytes in the FSK TX FIFO. */
    static constexpr uint16_t fsk_tx_fifo_bytes = fsk_tx_fifo_words * 2U;

    /** Number of bytes in the FSK RX FIFO. */
    static constexpr uint16_t fsk_rx_fifo_bytes = fsk_rx_fifo_words * 2U;

    /** Largest block the 11-bit FSK data-length field can describe. */
    static constexpr uint16_t fsk_max_rx_bytes = 0x0800U;

    /**
     * @brief REG_72 control word for a 1200 bps bit clock.
     *
     * The board uses the BK4819's 26 MHz crystal. The datasheet formula is
     * frequency_Hz * 10.32444, so 1200 Hz becomes 12389 = 0x3065.
     */
    static constexpr uint16_t fsk_clock_1200 = 0x3065;

    /**
     * @brief Complete FSK register configuration.
     *
     * This is a full mirror of the chip's datasheet fields. Values are in raw
     * chip units and are clamped by the setters when fsk_configure() applies
     * them. The defaults are the FFSK 1200/1800 setup used by the reference
     * firmware: 8 preamble bytes, 4-byte sync word 0x2DD4/0x5AA5, deviation
     * 1200, Tone2 gain 96 and a 1200 bps bit clock.
     */
    struct FskConfig
    {
        FskTxMode tx_mode = FskTxMode::Ffsk1200_1800;
        FskRxMode rx_mode = FskRxMode::Ffsk1200_1800;
        FskRxBandwidth rx_bandwidth = FskRxBandwidth::Ffsk1200_1800;
        uint8_t rx_gain = 3;

        FskPreambleType preamble_type = FskPreambleType::SyncWordMsb;
        uint8_t preamble_bytes = 8;
        FskSyncLength sync_length = FskSyncLength::FourBytes;
        uint16_t sync_first = 0x2DD4;
        uint16_t sync_second = 0x5AA5;

        bool crc = false;
        bool scrambler = false;
        bool invert_tx = false;
        bool invert_rx = false;

        /** TX FIFO almost-empty threshold, 0 to 127 words. */
        uint8_t tx_fifo_threshold = 64;

        /** RX FIFO almost-full threshold, 0 to 7 words. */
        uint8_t rx_fifo_threshold = 4;

        /** Tone2/FSK gain, 0 to 127. */
        uint8_t gain = 96;

        /** REG_72 Tone2/FSK frequency control word. */
        uint16_t clock = fsk_clock_1200;

        /** Raw REG_40 transmit deviation, 0 to 4095. */
        uint16_t deviation = 1200;
    };

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

    /*
     * FSK modem register interface.
     *
     * Each function below programs one datasheet field. They are the
     * datasheet-level surface used by fsk_configure() and by experiments.
     * Starting a transfer, timing the 20 ms FIFO-to-modulator delay, packing
     * and draining FIFO words and saving the voice deviation are handled by
     * the complete-session functions further down.
     */

    /** Select the FSK transmit mode (REG_58 bits 15:13). */
    void set_fsk_tx_mode(FskTxMode mode);

    /** Select the FSK receive mode (REG_58 bits 12:10). */
    void set_fsk_rx_mode(FskRxMode mode);

    /** Select the FSK receive bandwidth (REG_58 bits 3:1). */
    void set_fsk_rx_bandwidth(FskRxBandwidth bandwidth);

    /** Set the FSK receive gain, 0 to 3 (REG_58 bits 9:8). */
    void set_fsk_rx_gain(uint8_t gain);

    /** Select the preamble bit pattern (REG_58 bits 5:4). */
    void set_fsk_preamble_type(FskPreambleType type);

    /**
     * @brief Enable the FSK modem (REG_58 bit 0).
     *
     * The reference firmware also sets REG_58 bits 7:6 to 11; this function
     * does the same. It does not enable either direction; use
     * enable_fsk_tx() or enable_fsk_rx() for that.
     */
    void enable_fsk(bool enabled);

    /**
     * @brief Set the chip-generated preamble length (REG_59 bits 7:4).
     *
     * @param bytes 1 to 16. Values outside that range are clamped.
     */
    void set_fsk_preamble_length(uint8_t bytes);

    /** Select a 2- or 4-byte sync word (REG_59 bit 3). */
    void set_fsk_sync_length(FskSyncLength length);

    /** Enable or disable the chip data scrambler (REG_59 bit 13). */
    void set_fsk_scrambler(bool enabled);

    /** Invert data bits on transmit (REG_59 bit 9). */
    void set_fsk_invert_tx(bool inverted);

    /** Invert data bits on receive (REG_59 bit 10). */
    void set_fsk_invert_rx(bool inverted);

    /**
     * @brief Program the sync word bytes.
     *
     * @param first  Sync bytes 0 and 1, high byte first (REG_5A).
     * @param second Sync bytes 2 and 3, high byte first (REG_5B). Only used
     *               when the sync length is four bytes.
     */
    void set_fsk_sync_word(uint16_t first, uint16_t second);

    /**
     * @brief Enable or disable the chip CRC option (REG_5C bit 6).
     *
     * This firmware keeps the CRC off and leaves integrity checking to the
     * layer above. The register's other bits are not documented in the public
     * summary; the two values used here come from working raw-FSK and stock
     * AIRCOPY code.
     */
    void set_fsk_crc(bool enabled);

    /**
     * @brief Set the number of data bytes in the block (REG_5D).
     *
     * The 11-bit field stores length - 1. It counts only the caller's data,
     * not the preamble or sync word. Both ends must program the same length.
     *
     * @param bytes 1 to 2048. Values outside that range are clamped.
     */
    void set_fsk_data_length(uint16_t bytes);

    /**
     * @brief Program the FIFO threshold fields (REG_5E).
     *
     * @param tx_words TX FIFO almost-empty threshold, 0 to 127 words.
     * @param rx_words RX FIFO almost-full threshold, 0 to 7 words.
     */
    void set_fsk_fifo_thresholds(uint8_t tx_words, uint8_t rx_words);

    /** Clear the transmit FIFO (REG_59 bit 15). */
    void clear_fsk_tx_fifo();

    /** Clear the receive FIFO (REG_59 bit 14). */
    void clear_fsk_rx_fifo();

    /** Enable or disable the FSK transmitter (REG_59 bit 11). */
    void enable_fsk_tx(bool enabled);

    /** Enable or disable the FSK receiver (REG_59 bit 12). */
    void enable_fsk_rx(bool enabled);

    /** Push one 16-bit word into the TX FIFO (REG_5F). */
    void write_fsk_fifo(uint16_t word);

    /** Read one 16-bit word from the RX FIFO (REG_5F). */
    uint16_t read_fsk_fifo();

    /**
     * @brief Set the Tone2/FSK tuning gain (REG_70 bits 6:0).
     *
     * This also enables the Tone2 generator, which the FSK modulator uses.
     * Tone1 is left off.
     *
     * @param gain 0 to 127. Values above 127 are clamped.
     */
    void set_fsk_gain(uint8_t gain);

    /**
     * @brief Program the Tone2/FSK frequency control word (REG_72).
     *
     * Use fsk_clock_1200 for a 1200 bps bit clock.
     */
    void set_fsk_clock(uint16_t control_word);

    /** Turn both tone generators off (REG_70). */
    void disable_tones();

    /** @return the raw REG_40 transmit deviation value. */
    uint16_t tx_deviation();

    /**
     * @brief Program the RF transmit deviation (REG_40 bits 11:0).
     *
     * The upper four bits of the register are preserved.
     */
    void set_tx_deviation(uint16_t deviation);

    /*
     * Complete FSK sessions.
     *
     * These functions own the chip-side sequence: register ordering, FIFO
     * packing and draining, the 20 ms FIFO-to-modulator delay, interrupt
     * masking and deviation save/restore. Use them for normal data transfers;
     * the per-field setters above remain available for experiments.
     */

    /**
     * @brief Program every FSK register and enable the modem.
     *
     * Both directions are stopped and both FIFOs are cleared. The FSK status
     * bits are unmasked, the modem is enabled and the voice deviation is
     * saved the first time this is called. fsk_stop() restores it. The
     * modem is left idle: neither direction is enabled.
     */
    void fsk_configure(const FskConfig& config);

    /**
     * @brief Load the TX FIFO and start sending one block.
     *
     * The TX FIFO holds fsk_tx_fifo_bytes (256) bytes. The function clears
     * the FIFO, programs the data length, packs the bytes low-byte-first,
     * waits the required 20 ms and enables the transmitter. It returns
     * immediately after starting; poll fsk_tx_finished() before stopping the
     * transmitter.
     *
     * @param data   Bytes to send. Must not be null.
     * @param length Number of bytes, 1 to fsk_tx_fifo_bytes.
     * @return false when the modem is not configured or the arguments are
     *         out of range.
     */
    bool fsk_start_tx(const uint8_t* data, uint16_t length);

    /**
     * @brief Arm the receiver for a block of exactly length bytes.
     *
     * The receiver streams the block through its 16-byte RX FIFO. Poll
     * fsk_rx_data_ready() and drain with fsk_read() at least every 20 ms at
     * 1200 bps. The length can be up to fsk_max_rx_bytes (2048).
     *
     * @return false when the modem is not configured or the length is out of
     *         range.
     */
    bool fsk_start_rx(uint16_t length);

    /**
     * @brief Drain received bytes from the RX FIFO.
     *
     * Reads at most fsk_rx_fifo_bytes (16) bytes per call. Call it when
     * fsk_rx_data_ready() is true. The high byte of the last word is ignored
     * for an odd-length block. If capacity is too small for everything that is
     * left, only whole FIFO words are consumed, so no valid byte is lost
     * between calls; call it again with room for the rest.
     *
     * @param buffer   Destination buffer.
     * @param capacity Destination size in bytes.
     * @return Number of bytes stored.
     */
    uint16_t fsk_read(uint8_t* buffer, uint16_t capacity);

    /** @return true when received bytes are waiting in the RX FIFO. */
    bool fsk_rx_data_ready();

    /** @return true once the receiver has finished the programmed block. */
    bool fsk_rx_finished();

    /** @return true once the modulator has sent the whole TX FIFO. */
    bool fsk_tx_finished();

    /** @return true when the receiver has found the sync word. */
    bool fsk_rx_sync();

    /**
     * @brief Stop both FSK directions and restore the voice settings.
     *
     * Disables the FSK transmitter and receiver, disables the modem and tone
     * generator, restores the saved voice deviation and removes the FSK
     * interrupt sources. Safe to call twice.
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

    /** @return the current interrupt mask from REG_3F. */
    uint16_t interrupt_mask();

private:
    /** Write the cached REG_59 control value. */
    void write_fsk_control();

    /** Program every FSK register from fsk_config_. */
    void apply_fsk_config();

    /** Re-apply REG_70 and REG_72 after another subsystem touched them. */
    void apply_fsk_tones();

    /** Pack bytes into the TX FIFO, low byte first. */
    void write_fsk_bytes(const uint8_t* data, uint16_t length);

    /** Clock one byte out on the serial bus, most significant bit first. */
    void write_u8(uint8_t value);

    /** Clock one 16-bit word out on the serial bus. */
    void write_u16(uint16_t value);

    /** Switch the data pin to input and clock one 16-bit word in. */
    uint16_t read_u16();

    /** Latched state of the chip's GPIO outputs, used by set_gpio(). */
    uint16_t gpio_state_ = 0x9000;

    /** Cached REG_59 value: preamble, sync length, scramble and direction. */
    uint16_t fsk_control_ = 0;

    /** True between fsk_configure() and fsk_stop(). */
    bool fsk_configured_ = false;

    /** REG_40 value from before the first FSK setup. */
    uint16_t fsk_saved_deviation_ = 0;

    /** Settings from the last fsk_configure() call. */
    FskConfig fsk_config_;

    /** Number of bytes expected in the current receive block. */
    uint16_t fsk_rx_expected_ = 0;

    /** Number of bytes already handed to the caller. */
    uint16_t fsk_rx_received_ = 0;

    /** Latched "receiver finished" flag. */
    bool fsk_rx_finished_ = false;

    /** True once the finished RX FIFO has been drained. */
    bool fsk_rx_drained_ = true;
};

/** The single BK4819 instance used by this platform. */
extern BK4819 bk4819;

} // namespace platform

#endif // PLATFORM_BK4819_H_
