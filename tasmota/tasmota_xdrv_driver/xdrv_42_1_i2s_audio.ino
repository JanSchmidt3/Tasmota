
#ifdef ESP32
#ifdef ESP32S3_BOX
#include <driver/i2s.h>
#include <es8156.h>
#include <es7243e.h>

void S3boxAudioPower(uint8_t pwr) {
  pinMode(46 , OUTPUT);
  digitalWrite(46, pwr);
}

uint32_t ES8156_init() {
  esp_err_t ret_val = ESP_OK;

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

esp_err_t es7243e_init() {
    esp_err_t ret_val = ESP_OK;

    if (I2cSetDevice(ES7243_ADDR, 1)) {
      I2cSetActiveFound(ES7243_ADDR, "ES7243e-I2C", 1);
    }

    audio_hal_codec_config_t cfg = {
        .i2s_iface = {
          .mode = AUDIO_HAL_MODE_SLAVE,
          .bits = AUDIO_HAL_BIT_LENGTH_16BITS,
        }
    };

    ret_val |= es8156_codec_init(&Wire1, &cfg);

    return ret_val;
}


void S3boxInit() {

  I2c2Begin(8, 18);

  ES8156_init();

  es7243e_init();

/*
#define SBOX_SCLK 17
#define SBOX_LRCK 47
#define SBOX_DOUT 16
#define SBOX_SDIN 15
#define SBOX_MCLK 2

//i2s_config_t i2s_config = I2S_CONFIG_DEFAULT();

i2s_pin_config_t pins = {
  .mck_io_num = SBOX_MCLK,
  .bck_io_num = SBOX_SCLK,
  .ws_io_num = SBOX_LRCK,
  .data_out_num = SBOX_DOUT,
  .data_in_num = SBOX_SDIN,

};*/

//i2s_set_pin((i2s_port_t)0, &pins);
}

#endif
#endif
