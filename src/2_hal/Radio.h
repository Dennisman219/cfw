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
 * This role covers basic FM voice operation and a raw FSK data pipe. CTCSS,
 * DCS, DTMF, scanning, companding and scrambling are not implemented yet.
 * Transmit power calibration also needs the external EEPROM settings, so
 * transmit() uses a fixed placeholder PA bias for now.
 *
 * The FSK pipe adds only the framing another BK4819 needs: a preamble, a sync
 * word and the caller's bytes. AX.25, HDLC, NRZI, bit stuffing and checksums
 * belong in the layer above.
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
     * @brief FSK transmit mode (REG_58 bits 15:13).
     *
     * Ffsk1200_1800 is the recommended default. The chip generates 1200 Hz
     * and 1800 Hz tones at 1200 bps. This is not Bell 202 (1200/2200 Hz), so
     * a hardware TNC built for Bell 202 may not decode it. Software
     * demodulators such as Dire Wolf usually do.
     *
     * Fsk1200_2400 is direct FSK, the mode the stock AIRCOPY feature uses.
     * Ffsk1200_2400 is listed in the datasheet but reported as unreliable on
     * the UV-K5, so prefer Ffsk1200_1800.
     *
     * For now this enum is a full mirror of the BK4819 field. Other radios
     * will need a mapping to their own modes.
     */
    enum class FskTxMode : uint8_t
    {
        Fsk1200_2400 = 0,  ///< Direct FSK, 1.2 or 2.4 kbps.
        Ffsk1200_1800 = 1, ///< FFSK, 1200 bps, 1200/1800 Hz tones.
        Ffsk1200_2400 = 3, ///< FFSK, 1200 bps, 1200/2400 Hz tones.
        NoaaSame = 5,      ///< NOAA SAME transmitter.
    };

    /** FSK receive mode (REG_58 bits 12:10). */
    enum class FskRxMode : uint8_t
    {
        Fsk1200_2400 = 0,  ///< Direct FSK, 1.2 or 2.4 kbps.
        Ffsk1200_1800 = 7, ///< FFSK, 1200 bps, 1200/1800 Hz tones.
        Ffsk1200_2400 = 4, ///< FFSK, 1200 bps, 1200/2400 Hz tones.
    };

    /** FSK receive bandwidth (REG_58 bits 3:1). */
    enum class FskRxBandwidth : uint8_t
    {
        Fsk1200 = 0,                ///< Direct FSK, 1200 bps.
        Ffsk1200_1800 = 1,          ///< FFSK 1200/1800.
        NoaaSame = 2,               ///< NOAA SAME.
        Fsk2400OrFfsk1200_2400 = 4, ///< Direct FSK 2400 or FFSK 1200/2400.
    };

    /** Preamble bit pattern (REG_58 bits 5:4). */
    enum class FskPreambleType : uint8_t
    {
        SyncWordMsb = 0, ///< 0xAA or 0x55, chosen by the MSB of sync byte 0.
        Fixed0x55 = 2,   ///< Always 0x55.
        Fixed0xAA = 3,   ///< Always 0xAA.
    };

    /** Sync word length (REG_59 bit 3). */
    enum class FskSyncLength : uint8_t
    {
        TwoBytes,  ///< Sync bytes 0 and 1 from REG_5A only.
        FourBytes, ///< Sync bytes 0 to 3 from REG_5A and REG_5B.
    };

    /**
     * @brief Complete FSK configuration.
     *
     * For now this is a full mirror of the BK4819 FSK register fields, so the
     * portable role exposes the same capability as the hardware proxy. The
     * platform implementation maps each field to the chip's own type.
     *
     * The defaults are FFSK 1200/1800, 8 preamble bytes, a 4-byte sync word
     * 0x2DD4/0x5AA5, deviation 1200, Tone2 gain 96 and the 1200 bps clock.
     * The chip adds only the preamble, the sync word and the caller's bytes;
     * AX.25 flags, bit stuffing, NRZI, a length byte and a checksum belong in
     * the layer above.
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

        /// Sync bytes 0 and 1. The high byte is sent first.
        uint16_t sync_first = 0x2DD4;

        /// Sync bytes 2 and 3. Only used with a four-byte sync word.
        uint16_t sync_second = 0x5AA5;

        bool crc = false;
        bool scrambler = false;
        bool invert_tx = false;
        bool invert_rx = false;

        /// TX FIFO almost-empty threshold, 0 to 127 words.
        uint8_t tx_fifo_threshold = 64;

        /// RX FIFO almost-full threshold, 0 to 7 words.
        uint8_t rx_fifo_threshold = 4;

        /// Tone2/FSK gain, 0 to 127.
        uint8_t gain = 96;

        /// Tone2/FSK frequency control word (REG_72).
        uint16_t clock = 0x3065;

        /// Raw RF deviation, 0 to 4095.
        uint16_t deviation = 1200;
    };

    /** TX FIFO size in 16-bit words. */
    static constexpr uint16_t fsk_tx_fifo_words = 128;

    /** RX FIFO size in 16-bit words. */
    static constexpr uint16_t fsk_rx_fifo_words = 8;

    /** TX FIFO size in bytes; also the largest fsk_start_tx() block. */
    static constexpr uint16_t fsk_tx_fifo_bytes = 256;

    /** RX FIFO size in bytes; fsk_read() returns at most this much. */
    static constexpr uint16_t fsk_rx_fifo_bytes = 16;

    /** Largest fsk_start_rx() block, set by the 11-bit data-length field. */
    static constexpr uint16_t fsk_max_rx_bytes = 2048;

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

    /*
     * Raw FSK FIFO interface.
     *
     * For now the radio exposes the same FSK capabilities and queue sizes as
     * the BK4819. configure_fsk() programs the whole modem; fsk_start_tx()
     * and fsk_start_rx() start one transfer; ready() says when received bytes
     * are waiting and fsk_read() drains them. There is no internal block
     * buffer.
     *
     * The chip adds only a preamble and a sync word. It does not add AX.25
     * flags, bit stuffing, NRZI, a length byte or a checksum; those belong in
     * the layer above. The block length is programmed in the chip but not
     * sent over the air, so both ends must use the same length.
     *
     * Typical transmit sequence:
     *
     *     radio.configure_fsk(config);       // PA is still off
     *     radio.transmit(433500000U);
     *     radio.fsk_start_tx(data, length);  // 1 to fsk_tx_fifo_bytes
     *     while (!radio.fsk_tx_finished()) {}
     *     radio.fsk_stop();
     *
     * Typical receive sequence:
     *
     *     radio.receive(433500000U);
     *     radio.configure_fsk(config);
     *     radio.fsk_start_rx(length);        // 1 to fsk_max_rx_bytes
     *     uint16_t total = 0;
     *     while (total < length) {
     *         if (radio.ready()) {
     *             total += radio.fsk_read(&buffer[total], length - total);
     *         } else if (radio.fsk_rx_finished()) {
     *             break;                     // truncated block
     *         }
     *         // wait 20 ms
     *     }
     *     radio.fsk_stop();
     */

    /**
     * @brief Program the complete FSK modem.
     *
     * The function forwards the full register configuration to the hardware
     * proxy, which stops both directions, programs every field, clears the
     * FIFOs, unmasks the FSK status bits and saves the voice deviation.
     *
     * For transmit, call this before transmit() so the tone generator is set
     * up while the PA is still off. For receive, call it before or after
     * receive(); fsk_start_rx() re-applies the shared tone registers.
     */
    void configure_fsk(const FskConfig& config);

    /**
     * @brief Enable or disable the FSK modem itself.
     *
     * configure_fsk() enables it and fsk_stop() disables it, so this is only
     * needed for manual experiments. It does not start either direction.
     */
    void enable_fsk(bool enabled);

    /**
     * @brief Fill the TX FIFO and start one block.
     *
     * The radio must already be transmitting; call transmit() first. The
     * hardware proxy programs the data length, packs the bytes low-byte-first,
     * waits the required 20 ms and enables the modulator. Poll
     * fsk_tx_finished() before stopping the transmitter. The block is limited
     * to fsk_tx_fifo_bytes (256 bytes); a longer block would need FIFO
     * refilling and is not supported yet.
     *
     * @param data   Bytes to send. Must not be null.
     * @param length Number of bytes, 1 to fsk_tx_fifo_bytes.
     * @return true if the block was started; false on a bad length, a null
     *         pointer or when the radio is not transmitting.
     */
    bool fsk_start_tx(const uint8_t* data, uint16_t length);

    /**
     * @brief Arm the receiver for a block of exactly length bytes.
     *
     * The radio must already be receiving; call receive() first. The receiver
     * streams the block through the 16-byte RX FIFO, so call ready() and
     * fsk_read() at least every 20 ms at 1200 bps. The length can be up to
     * fsk_max_rx_bytes (2048).
     *
     * @param length Number of bytes, 1 to fsk_max_rx_bytes.
     * @return true if the receiver was armed; false on a bad length or when
     *         the radio is not receiving.
     */
    bool fsk_start_rx(uint16_t length);

    /**
     * @brief Check whether received bytes are waiting in the RX FIFO.
     *
     * This is the drain signal: when it returns true, call fsk_read(). It
     * returns true when the FIFO reaches the almost-full threshold and again
     * when the receiver finishes, until the FIFO has been drained.
     */
    bool fsk_ready();

    /**
     * @brief Drain received bytes from the RX FIFO.
     *
     * Returns at most fsk_rx_fifo_bytes (16) bytes per call. The caller owns
     * the block buffer and must call this while ready() is true. The high byte
     * of the last word is ignored for an odd-length block. If capacity is too
     * small for everything that is left, only whole FIFO words are consumed,
     * so no valid byte is lost between calls.
     *
     * @param buffer   Destination buffer. Must not be null.
     * @param capacity Destination size in bytes.
     * @return Number of bytes stored.
     */
    uint16_t fsk_read(uint8_t* buffer, uint16_t capacity);

    /**
     * @brief Check whether the modulator has sent the whole block.
     *
     * This says nothing about the external PA, the antenna path or whether
     * the other radio heard the block.
     */
    bool fsk_tx_finished();

    /**
     * @brief Check whether the receiver has finished the programmed block.
     *
     * There may still be bytes in the FIFO. Drain them with ready() and
     * fsk_read() before stopping. If the carrier drops early, the programmed
     * number of bytes may never arrive; use an application timeout and
     * fsk_stop() to recover.
     */
    bool fsk_rx_finished();

    /**
     * @brief Stop the FSK modem and restore the voice deviation.
     *
     * The caller still owns the radio direction, frequency, antenna path and
     * PA. Call idle() or receive() afterwards.
     */
    void fsk_stop();

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
