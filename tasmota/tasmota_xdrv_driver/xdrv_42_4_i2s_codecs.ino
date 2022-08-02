
/*
  audio is2 support for ESP32-S3 box and box lite

  Copyright (C) 2022  Gerhard Mutz and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef ESP32
#ifdef ESP32S3_BOX
#include <driver/i2s.h>
#include <es8156.h>
#include <es8311.h>
#include <es7243e.h>
#include <es7210.h>


#define S3BOX_APWR_GPIO 46

void S3boxAudioPower(uint8_t pwr) {
  digitalWrite(S3BOX_APWR_GPIO, pwr);
}

// box lite dac init
uint32_t ES8156_init() {
  uint32_t ret_val = ESP_OK;

  if (I2cSetDevice(ES8156_ADDR, 1)) {
    I2cSetActiveFound(ES8156_ADDR, "ES8156-I2C", 1);
    audio_hal_codec_config_t cfg = {
       .i2s_iface = {
         .mode = AUDIO_HAL_MODE_SLAVE,
         .bits = AUDIO_HAL_BIT_LENGTH_16BITS,
       }
    };
    ret_val |= es8156_codec_init(&Wire1, &cfg);
    ret_val |= es8156_codec_set_voice_volume(75);
  }
  return ret_val;
}

// box lite adc init
uint32_t es7243e_init() {
    uint32_t ret_val = ESP_OK;

    if (I2cSetDevice(ES7243_ADDR, 1)) {
      I2cSetActiveFound(ES7243_ADDR, "ES7243e-I2C", 1);

      audio_hal_codec_config_t cfg = {
        .i2s_iface = {
          .mode = AUDIO_HAL_MODE_SLAVE,
          .bits = AUDIO_HAL_BIT_LENGTH_16BITS,
        }
      };

      ret_val |= es7243e_adc_init(&Wire1, &cfg);
    }

    return ret_val;
}
// box adc init
uint32_t es7210_init() {
  uint32_t ret_val = ESP_OK;

  if (I2cSetDevice(ES7210_ADDR, 1)) {
    I2cSetActiveFound(ES7210_ADDR, "ES7210-I2C", 1);
    audio_hal_codec_config_t cfg = {
        .adc_input = AUDIO_HAL_ADC_INPUT_ALL,
        .codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE,
        .i2s_iface = {
          .mode = AUDIO_HAL_MODE_SLAVE,
          .fmt = AUDIO_HAL_I2S_NORMAL,
          .samples = AUDIO_HAL_16K_SAMPLES,
          .bits = AUDIO_HAL_BIT_LENGTH_16BITS,
        },
    };

    ret_val |= es7210_adc_init(&Wire1, &cfg);
    ret_val |= es7210_adc_config_i2s(cfg.codec_mode, &cfg.i2s_iface);
    ret_val |= es7210_adc_set_gain((es7210_input_mics_t)(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2), (es7210_gain_value_t) GAIN_37_5DB);
    ret_val |= es7210_adc_set_gain((es7210_input_mics_t)(ES7210_INPUT_MIC3 | ES7210_INPUT_MIC4), (es7210_gain_value_t) GAIN_0DB);
    ret_val |= es7210_adc_ctrl_state(cfg.codec_mode, AUDIO_HAL_CTRL_START);
  }
  return ret_val;
}

// box dac init
uint32_t ES8311_init() {
  uint32_t ret_val = ESP_OK;

  if (I2cSetDevice(ES8311_ADDR, 1)) {
    I2cSetActiveFound(ES8311_ADDR, "ES8311-I2C", 1);
    audio_hal_codec_config_t cfg = {
        .dac_output = AUDIO_HAL_DAC_OUTPUT_LINE1,
        .codec_mode = AUDIO_HAL_CODEC_MODE_DECODE,
        .i2s_iface = {
          .mode = AUDIO_HAL_MODE_SLAVE,
          .fmt = AUDIO_HAL_I2S_NORMAL,
          .samples = AUDIO_HAL_16K_SAMPLES,
          .bits = AUDIO_HAL_BIT_LENGTH_16BITS,
        },
    };

    ret_val |= es8311_codec_init(&Wire1, &cfg);
    ret_val |= es8311_set_bits_per_sample(cfg.i2s_iface.bits);
    ret_val |= es8311_config_fmt((es_i2s_fmt_t)cfg.i2s_iface.fmt);
    ret_val |= es8311_codec_set_voice_volume(75);
    ret_val |= es8311_codec_ctrl_state(cfg.codec_mode, AUDIO_HAL_CTRL_START);

  }
  return ret_val;
}

void S3boxInit() {
  if (TasmotaGlobal.i2c_enabled_2) {
    // box lite
    ES8156_init();
    es7243e_init();
    // box full
    ES8311_init();
    es7210_init();

    pinMode(S3BOX_APWR_GPIO , OUTPUT);
  }
}
#endif // ESP32S3_BOX


#ifdef USE_W8960

#define W8960_ADDR 0x1a

void W8960_Write(uint8_t reg_addr, uint16_t data) {
  reg_addr <<= 1;
  reg_addr |=  ((data >> 8) & 1);
  data &= 0xff;
  Wire1.beginTransmission(W8960_ADDR);
  Wire1.write(reg_addr);
  Wire1.write(data);
  Wire1.endTransmission();
}

void W8960_Init(void) {

  if (!TasmotaGlobal.i2c_enabled_2) {
    return;
  }

  if (I2cSetDevice(W8960_ADDR, 1)) {
    I2cSetActiveFound(W8960_ADDR, "W8960-I2C", 1);

    // reset
    W8960_Write(0x0f, 0x0000);
    delay(10);

    // enable dac and adc
    W8960_Write(0x19, (1<<8)|(1<<7)|(1<<6)|(1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<1));
    // left speaker not used
    W8960_Write(0x1A, (1<<8)|(1<<7)|(1<<6)|(1<<5)|(1<<4)|(1<<3));
    W8960_Write(0x2F, (1<<5)|(1<<4)|(1<<3)|(1<<2));

    // Configure clock
    W8960_Write(0x04, 0x0000);

    // Configure ADC/DAC
    W8960_Write(0x05, 0x0000);

    // Configure audio interface
    // I2S format 16 bits word length
    //W8960_Write(0x07 0x0002)
    W8960_Write(0x07, 0x0022);

    // Configure HP_L and HP_R OUTPUTS
    W8960_Write(0x02, 0x017f);
    W8960_Write(0x03, 0x017f);

    // Configure SPK_RP and SPK_RN
    W8960_Write(0x28, 0x0177);
    W8960_Write(0x29, 0x0177);

    // Enable the OUTPUTS, only right speaker is wired
    W8960_Write(0x31, 0x0080);

    // Configure DAC volume
    W8960_Write(0x0a, 0x01FF);
    W8960_Write(0x0b, 0x01FF);

    // Configure MIXER
    W8960_Write(0x22, (1<<8)|(1<<7));
    W8960_Write(0x25, (1<<8)|(1<<7));

    // Jack Detect
    //W8960_Write(0x18, (1<<6)|(0<<5));
    //W8960_Write(0x17, 0x01C3);
    //W8960_Write(0x30, 0x0009);

    //  input volume
    W8960_Write(0x00, 0x0127);
    W8960_Write(0x01, 0x0127);

    //  set ADC Volume
    W8960_Write(0x15, 0x01c3);
    W8960_Write(0x16, 0x01c3);

    // disable bypass switch
    W8960_Write(0x2d, 0x0000);
    W8960_Write(0x2e, 0x0000);

    // connect LINPUT1 to PGA and set PGA Boost Gain.
    W8960_Write(0x20, 0x0020|(1<<8)|(1<<3));
    W8960_Write(0x21, 0x0020|(1<<8)|(1<<3));

  }

}

void W8960_SetGain(uint8_t sel, uint16_t value) {
  switch (sel) {
    case 0:
      // output dac in 0.5 db steps
      value &= 0x00ff;
      value |= 0x0100;
      W8960_Write(0x0a, value);
      W8960_Write(0x0b, value);
      break;
    case 1:
      // input pga in 0.75 db steps
      value &= 0x001f;
      value |= 0x0120;
      W8960_Write(0x00, value);
      W8960_Write(0x01, value);
      break;
    case 2:
      // adc in 0.5 db steps
      value &= 0x00ff;
      value |= 0x0100;
      W8960_Write(0x15, value);
      W8960_Write(0x16, value);
      break;
  }
}

#endif // USE_W8960

#endif // ESP32
