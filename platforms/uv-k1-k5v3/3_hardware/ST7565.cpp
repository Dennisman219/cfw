/**
 * @file ST7565.cpp
 * @brief Hardware proxy for the ST7565 LCD controller.
 *
 * The pin assignment and the initialization sequence come from the F4HWN
 * UV-K5 v3 reference firmware's st7565.c driver.
 *
 * Wiring on this board:
 *
 *     SPI1 SCK:  PA5
 *     SPI1 MOSI: PA7
 *     A0:        PA6   0 = command, 1 = data
 *     CS:        PB2   active low
 */

#include "ST7565.h"

#include "Gpio.h"
#include "SysTickTimer.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_gpio.h"
#include "py32f071_ll_spi.h"

namespace platform
{
namespace
{

constexpr uint32_t kDataCommandPin = LL_GPIO_PIN_6;
constexpr uint32_t kChipSelectPin = LL_GPIO_PIN_2;

// ST7565 command values ------------------------------------------------------
// The datasheet builds many commands by OR-ing a base value with a parameter.
// Keeping the base values separate makes the command calls easy to compare
// with the datasheet.

constexpr uint8_t kDisplayOff = 0xAE;
constexpr uint8_t kDisplayOn = 0xAF;
constexpr uint8_t kSetStartLine = 0x40;
constexpr uint8_t kSetPageAddress = 0xB0;
constexpr uint8_t kSetColumnAddressHigh = 0x10;
constexpr uint8_t kInverseOff = 0xA6;
constexpr uint8_t kInverseOn = 0xA7;
constexpr uint8_t kAllPixelsOff = 0xA4; // normal display
constexpr uint8_t kAllPixelsOn = 0xA5;  // force every pixel on
constexpr uint8_t kBiasOneNinth = 0xA2;
constexpr uint8_t kBiasOneSeventh = 0xA3;
constexpr uint8_t kSegmentNormal = 0xA0;
constexpr uint8_t kSegmentReverse = 0xA1;
constexpr uint8_t kComNormal = 0xC0;
constexpr uint8_t kComReverse = 0xC8;
constexpr uint8_t kSetElectronicVolume = 0x81; // followed by level 0..63
constexpr uint8_t kPowerControl = 0x28;        // booster/regulator/follower
constexpr uint8_t kRegulationRatio = 0x20;     // followed by ratio 0..7
constexpr uint8_t kSoftwareReset = 0xE2;

// Low-level transfers --------------------------------------------------------

void select_chip()
{
    gpio::set_low(GPIOB, kChipSelectPin);
}

void release_chip()
{
    gpio::set_high(GPIOB, kChipSelectPin);
}

void command_mode()
{
    gpio::set_low(GPIOA, kDataCommandPin);
}

void data_mode()
{
    gpio::set_high(GPIOA, kDataCommandPin);
}

/**
 * @brief Send one byte over SPI and wait until it has been clocked out.
 *
 * SPI is full duplex: every transmitted byte also receives one byte. The
 * received value is not used by the controller, but it must be read to clear
 * the receive flag before the next transfer.
 */
uint8_t transfer(uint8_t value)
{
    while (!LL_SPI_IsActiveFlag_TXE(SPI1)) {
    }

    LL_SPI_TransmitData8(SPI1, value);

    while (!LL_SPI_IsActiveFlag_RXNE(SPI1)) {
    }

    return LL_SPI_ReceiveData8(SPI1);
}

} // namespace

/** Definition of the single ST7565 instance declared in ST7565.h. */
ST7565 st7565;

void ST7565::init()
{
    LL_IOP_GRP1_EnableClock(
        LL_IOP_GRP1_PERIPH_GPIOA |
        LL_IOP_GRP1_PERIPH_GPIOB);

    // Idle levels before the pins become outputs.
    gpio::set_high(GPIOB, kChipSelectPin);
    gpio::set_low(GPIOA, kDataCommandPin);

    // SPI1 uses alternate function 0 on PA5 and PA7.
    gpio::configure_alternate(GPIOA, LL_GPIO_PIN_5, LL_GPIO_AF0_SPI1);
    gpio::configure_alternate(GPIOA, LL_GPIO_PIN_7, LL_GPIO_AF0_SPI1);

    gpio::configure_output(GPIOA, kDataCommandPin);
    gpio::configure_output(GPIOB, kChipSelectPin);

    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SPI1);

    // Mode 3 (clock idle high, data sampled on the second edge), 8 bits,
    // MSB first, software chip select. PCLK is 48 MHz, so /64 gives about
    // 750 kHz on the LCD clock line. The ST7565 tolerates much faster, but a
    // slow bus is easier to probe while bringing up the hardware.
    LL_SPI_SetTransferDirection(SPI1, LL_SPI_FULL_DUPLEX);
    LL_SPI_SetMode(SPI1, LL_SPI_MODE_MASTER);
    LL_SPI_SetDataWidth(SPI1, LL_SPI_DATAWIDTH_8BIT);
    LL_SPI_SetClockPolarity(SPI1, LL_SPI_POLARITY_HIGH);
    LL_SPI_SetClockPhase(SPI1, LL_SPI_PHASE_2EDGE);
    LL_SPI_SetNSSMode(SPI1, LL_SPI_NSS_SOFT);
    LL_SPI_SetBaudRatePrescaler(SPI1, LL_SPI_BAUDRATEPRESCALER_DIV64);
    LL_SPI_SetTransferBitOrder(SPI1, LL_SPI_MSB_FIRST);
    LL_SPI_DisableCRC(SPI1);
    LL_SPI_Enable(SPI1);

    // Controller reset and configuration. The delays come from the ST7565
    // datasheet and the reference driver; they give the charge pump time to
    // stabilize before the display is switched on.
    reset();
    set_bias(true);
    set_com_direction(false);
    set_segment_direction(true);
    set_inverse(false);
    set_all_pixels_on(false);
    set_regulation_ratio(4);
    set_contrast(31);

    write_command(kPowerControl | 0b011); // booster off, regulator + follower on
    systick.delay_ms(1);
    write_command(kPowerControl | 0b110); // booster + regulator on
    systick.delay_ms(1);
    for (uint8_t i = 0; i < 4; ++i) {
        write_command(kPowerControl | 0b111);
    }
    systick.delay_ms(40);

    set_start_line(0);
    display_on();
}

void ST7565::reset()
{
    write_command(kSoftwareReset);

    // The controller needs time to finish its internal reset before the power
    // circuit is configured.
    systick.delay_ms(120);
}

void ST7565::display_on()
{
    write_command(kDisplayOn);
}

void ST7565::display_off()
{
    write_command(kDisplayOff);
}

void ST7565::set_inverse(bool enabled)
{
    write_command(enabled ? kInverseOn : kInverseOff);
}

void ST7565::set_all_pixels_on(bool enabled)
{
    write_command(enabled ? kAllPixelsOn : kAllPixelsOff);
}

void ST7565::set_segment_direction(bool reverse)
{
    write_command(reverse ? kSegmentReverse : kSegmentNormal);
}

void ST7565::set_com_direction(bool reverse)
{
    write_command(reverse ? kComReverse : kComNormal);
}

void ST7565::set_bias(bool one_ninth)
{
    write_command(one_ninth ? kBiasOneNinth : kBiasOneSeventh);
}

void ST7565::set_regulation_ratio(uint8_t ratio)
{
    if (ratio > 7) {
        ratio = 7;
    }
    write_command(static_cast<uint8_t>(kRegulationRatio | ratio));
}

void ST7565::set_contrast(uint8_t level)
{
    if (level > max_contrast) {
        level = max_contrast;
    }

    write_command(kSetElectronicVolume);
    write_command(level);
}

void ST7565::set_start_line(uint8_t line)
{
    if (line > max_start_line) {
        return;
    }
    write_command(static_cast<uint8_t>(kSetStartLine | line));
}

void ST7565::set_page(uint8_t page)
{
    if (page >= pages) {
        return;
    }
    write_command(static_cast<uint8_t>(kSetPageAddress | page));
}

void ST7565::set_column(uint8_t column)
{
    if (column >= columns) {
        return;
    }

    write_command(static_cast<uint8_t>(
        kSetColumnAddressHigh | ((column >> 4) & 0x0FU)));
    write_command(static_cast<uint8_t>(column & 0x0FU));
}

void ST7565::write_data(const uint8_t* data, uint8_t count)
{
    data_mode();
    select_chip();

    for (uint8_t i = 0; i < count; ++i) {
        (void)transfer(data[i]);
    }

    release_chip();
}

void ST7565::write_command(uint8_t value)
{
    command_mode();
    select_chip();
    (void)transfer(value);
    release_chip();
}

} // namespace platform
