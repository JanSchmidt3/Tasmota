
#ifdef ESP32
#ifdef ESP32S3_BOX

#include <es8156.h>

void S3boxAudioPower(uint8_t pwr) {
  pinMode(46 , OUTPUT);
  digitalWrite(46, pwr);
}

uint32_t ES8156_init() {
  esp_err_t ret_val = ESP_OK;

  uint8_t i2caddr = ES8156_ADDR;
  if (I2cSetDevice(i2caddr, 1)) {
    I2cSetActiveFound(i2caddr, "ES8156-I2C", 1);
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


#endif
#endif
