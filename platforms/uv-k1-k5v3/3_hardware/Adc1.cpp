/**
 * @file Adc1.cpp
 * @brief Hardware proxy for the PY32F071 ADC1 peripheral.
 *
 * The configuration follows the F4HWN UV-K5 v3 reference firmware's board.c:
 * ADC1 clocked from PCLK / 4, 12-bit right-aligned, software-triggered single
 * conversions with one sequencer rank.
 */

#include "Adc1.h"

#include "Gpio.h"
#include "py32f071_ll_adc.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_gpio.h"
#include "py32f071_ll_rcc.h"

namespace platform
{
namespace
{

/**
 * Map each SamplingTime value to the matching LL constant.
 *
 * Keeping the vendor constants here means Adc1.h does not have to include the
 * LL headers, and callers can use readable cycle counts instead of register
 * bit patterns.
 */
constexpr uint32_t kSamplingTimeValues[] = {
    LL_ADC_SAMPLINGTIME_3CYCLES_5,
    LL_ADC_SAMPLINGTIME_5CYCLES_5,
    LL_ADC_SAMPLINGTIME_7CYCLES_5,
    LL_ADC_SAMPLINGTIME_13CYCLES_5,
    LL_ADC_SAMPLINGTIME_28CYCLES_5,
    LL_ADC_SAMPLINGTIME_41CYCLES_5,
    LL_ADC_SAMPLINGTIME_134CYCLES_5,
    LL_ADC_SAMPLINGTIME_239CYCLES_5,
};

constexpr uint8_t kSamplingTimeCount =
    sizeof(kSamplingTimeValues) / sizeof(kSamplingTimeValues[0]);

static_assert(
    static_cast<uint8_t>(Adc1::SamplingTime::Cycles239_5) + 1 ==
        kSamplingTimeCount,
    "the sampling-time table must list every SamplingTime value in order");

/**
 * Map an external channel number to the matching LL constant.
 *
 * The vendor helper macro __LL_ADC_DECIMAL_NB_TO_CHANNEL() cannot be used
 * here: the copy in 4_cmsis contains a stray backslash and does not compile.
 * A plain table is also easier to read and to compare with the datasheet.
 */
constexpr uint32_t kChannelValues[] = {
    LL_ADC_CHANNEL_0,
    LL_ADC_CHANNEL_1,
    LL_ADC_CHANNEL_2,
    LL_ADC_CHANNEL_3,
    LL_ADC_CHANNEL_4,
    LL_ADC_CHANNEL_5,
    LL_ADC_CHANNEL_6,
    LL_ADC_CHANNEL_7,
    LL_ADC_CHANNEL_8,
    LL_ADC_CHANNEL_9,
    LL_ADC_CHANNEL_10,
    LL_ADC_CHANNEL_11,
    LL_ADC_CHANNEL_12,
    LL_ADC_CHANNEL_13,
    LL_ADC_CHANNEL_14,
    LL_ADC_CHANNEL_15,
    LL_ADC_CHANNEL_16,
    LL_ADC_CHANNEL_17,
    LL_ADC_CHANNEL_18,
    LL_ADC_CHANNEL_19,
    LL_ADC_CHANNEL_20,
    LL_ADC_CHANNEL_21,
    LL_ADC_CHANNEL_22,
    LL_ADC_CHANNEL_23,
};

constexpr uint8_t kChannelCount =
    sizeof(kChannelValues) / sizeof(kChannelValues[0]);

static_assert(
    kChannelCount == Adc1::max_channel + 1,
    "the channel table must cover every ADC1 external channel");

} // namespace

/** Definition of the single ADC1 instance declared in Adc1.h. */
Adc1 adc1;

void Adc1::init()
{
    // The battery divider is connected to PB0 and PB1. Analog mode disconnects
    // the digital input buffer, which is needed for an accurate measurement.
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
    gpio::configure_analog(GPIOB, LL_GPIO_PIN_0 | LL_GPIO_PIN_1);

    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_ADC1);

    // The ADC clock must stay within its maximum rating. PCLK is 48 MHz, so
    // dividing by four gives 12 MHz, matching the reference firmware.
    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_PCLK_DIV4);

    // Start from a known state in case the ADC was already running. The
    // calibration below requires the converter to be disabled, and a stale
    // end-of-sequence flag would make the first read() return too early.
    LL_ADC_Disable(ADC1);
    LL_ADC_ClearFlag_EOS(ADC1);

    // No internal temperature sensor, Vref or battery channel is routed to the
    // converter; all measurements come from the external pins.
    LL_ADC_SetCommonPathInternalCh(ADC1_COMMON, LL_ADC_PATH_INTERNAL_NONE);

    LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);
    LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);

    // One channel, one conversion per software trigger.
    LL_ADC_SetSequencersScanMode(ADC1, LL_ADC_SEQ_SCAN_DISABLE);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_NONE);
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    LL_ADC_REG_SetSequencerDiscont(ADC1, LL_ADC_REG_SEQ_DISCONT_DISABLE);
}

void Adc1::set_channel(uint8_t channel, SamplingTime sampling)
{
    if (channel > max_channel ||
        static_cast<uint8_t>(sampling) >= kSamplingTimeCount) {
        return;
    }

    // LL_ADC_CHANNEL_x constants combine the channel number with the position
    // of that channel's sampling-time field, so one entry from the table is
    // valid for both register writes below.
    const uint32_t ll_channel = kChannelValues[channel];

    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, ll_channel);
    LL_ADC_SetChannelSamplingTime(
        ADC1, ll_channel, kSamplingTimeValues[static_cast<uint8_t>(sampling)]);
}

void Adc1::calibrate()
{
    LL_ADC_StartCalibration(ADC1);
    while (LL_ADC_IsCalibrationOnGoing(ADC1)) {
    }
}

void Adc1::enable()
{
    LL_ADC_Enable(ADC1);
}

void Adc1::disable()
{
    LL_ADC_Disable(ADC1);
}

bool Adc1::is_enabled() const
{
    return LL_ADC_IsEnabled(ADC1) != 0;
}

void Adc1::start_conversion()
{
    LL_ADC_REG_StartConversionSWStart(ADC1);
}

bool Adc1::conversion_complete() const
{
    return LL_ADC_IsActiveFlag_EOS(ADC1) != 0;
}

void Adc1::clear_conversion_complete()
{
    LL_ADC_ClearFlag_EOS(ADC1);
}

uint16_t Adc1::read_data() const
{
    return LL_ADC_REG_ReadConversionData12(ADC1);
}

uint16_t Adc1::read()
{
    start_conversion();

    while (!conversion_complete()) {
    }
    clear_conversion_complete();

    return read_data();
}

} // namespace platform
