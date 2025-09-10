#ifndef USEQGEN_MEMLNAUT_FAC_H
#define USEQGEN_MEMLNAUT_FAC_H

#include "memlnaut_audio/control_sgtl5000.h"
#include "memlnaut_audio/i2s_pio/i2s.h"
#include "uSeqGen_Base.h"

#define AUDIO_MEM __not_in_flash("memlnaut")

enum PinConfig_i2c
{
    i2c_sgt5000Data = 0,
    i2c_sgt5000Clk  = 1,
    i2s_pDIN        = 6,
    i2s_pDOUT       = 7,
    i2s_pBCLK       = 8,
    i2s_pWS         = 9,
    i2s_pMCLK       = 10,
};

static const int AUDIO_MEM bitsPerSample = 32;

static const float AUDIO_MEM amplitude =
    1 << (bitsPerSample - 2); // amplitude of square wave = 1/2 of maximum
static const float AUDIO_MEM neg_amplitude =
    -amplitude; // amplitude of square wave = 1/2 of maximum
static const float AUDIO_MEM one_over_amplitude = 1.f / amplitude;

static int32_t AUDIO_MEM_2 sample = amplitude; // current sample value

static AudioControlSGTL5000 codecCtl;

audiocallback_fptr_t audio_callback_ = nullptr;

static __attribute__((aligned(8))) pio_i2s i2s;

class uSeqGen_MEMLNaut_DAC final : public uSeqGen_Base
{
public:
    uSeqGen_MEMLNaut_DAC(queue_t* q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(2);
        SetOutputCount_(2);

        // SPI.setCS(PIN_CS);
        // SPI.begin(1); // Initialize the SPI bus

        // //pass settings to SPI class
        // SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0)); // Start
        // SPI transaction SPI.endTransaction();
    }

    ~uSeqGen_MEMLNaut_DAC() override {}

protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs,
                                 DSPatch::SignalBus& outputs) override
    {
        const float inL = GET_INPUT_SAFE(inputs, float, 0, 0.f);
        const float inR = GET_INPUT_SAFE(inputs, float, 0, 0.f);
        const int sigL  = static_cast<int>(inL * 2048.f) + 2047;
        const int sigR  = static_cast<int>(inR * 2048.f) + 2047;

        //   //assuming SPI already set up, so no need for begin/end transaction
        //   //CS is hardware controlled
        //   const uint16_t DAC_dataL = DAC_config_chan_A_gain | (sigL & 0xFFF);
        //   const uint16_t DAC_dataR = DAC_config_chan_B_gain | (sigR & 0xFFF);
        //   uint16_t ret;
        //   hw_write_masked(&spi_get_hw(spi0)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB,
        //   SPI_SSPCR0_DSS_BITS); // Fast set to 16-bits
        //   spi_write16_read16_blocking(spi0, &DAC_dataL, &ret, 1);
        //   hw_write_masked(&spi_get_hw(spi0)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB,
        //   SPI_SSPCR0_DSS_BITS); // Fast set to 16-bits
        //   spi_write16_read16_blocking(spi0, &DAC_dataR, &ret, 1);
    }

private:
};

#endif // USEQGEN_MUL_H
