#ifndef USEQGEN_MT_FAC_H
#define USEQGEN_MT_FAC_H

#include "uSeqGen_Base.h"
// #include "Arduino.h"
#ifdef ARDUINO
#include "SPI.h"
#endif

#define DAC_config_chan_A_gain 0b0001000000000000
#define DAC_config_chan_B_gain 0b1001000000000000
#define PIN_CS 21

class uSeqGen_MT_DAC final : public uSeqGen_Base
{
public:
    uSeqGen_MT_DAC(queue_t* q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(2);
        SetOutputCount_(2);

        SPI.setCS(PIN_CS);
        SPI.begin(1); // Initialize the SPI bus

        // pass settings to SPI class
        SPI.beginTransaction(
            SPISettings(20000000, MSBFIRST, SPI_MODE0)); // Start SPI transaction
        SPI.endTransaction();
    }

    // Static volatile variables for thread-safe access to latest DAC values
    // DSP thread writes, main thread reads - no synchronization needed
    static volatile uint16_t latest_dac_left;
    static volatile uint16_t latest_dac_right;

    ~uSeqGen_MT_DAC() override {}

protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs,
                                 DSPatch::SignalBus& outputs) override
    {
        const float inL = GET_INPUT_SAFE(inputs, float, 0, 0.f);
        const float inR = GET_INPUT_SAFE(inputs, float, 0, 0.f);
        const int sigL  = static_cast<int>(inL * 2048.f) + 2047;
        const int sigR  = static_cast<int>(inR * 2048.f) + 2047;

        // Store latest DAC values for LED visualization (zero overhead)
        latest_dac_left  = static_cast<uint16_t>(sigL & 0xFFF);
        latest_dac_right = static_cast<uint16_t>(sigR & 0xFFF);

        // assuming SPI already set up, so no need for begin/end transaction
        // CS is hardware controlled
        const uint16_t DAC_dataL = DAC_config_chan_A_gain | (sigL & 0xFFF);
        const uint16_t DAC_dataR = DAC_config_chan_B_gain | (sigR & 0xFFF);
        uint16_t ret;
        hw_write_masked(&spi_get_hw(spi0)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB,
                        SPI_SSPCR0_DSS_BITS); // Fast set to 16-bits
        spi_write16_read16_blocking(spi0, &DAC_dataL, &ret, 1);
        hw_write_masked(&spi_get_hw(spi0)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB,
                        SPI_SSPCR0_DSS_BITS); // Fast set to 16-bits
        spi_write16_read16_blocking(spi0, &DAC_dataR, &ret, 1);
    }

private:
};

#endif // USEQGEN_MUL_H
