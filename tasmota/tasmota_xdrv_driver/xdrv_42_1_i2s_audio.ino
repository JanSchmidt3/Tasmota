
#ifdef ESP32
#ifdef ESP32S3_BOX
#include <driver/i2s.h>
#include <es8156.h>
#include <es8311.h>
#include <es7243e.h>
#include <es7210.h>

void S3boxAudioPower(uint8_t pwr) {
  pinMode(46 , OUTPUT);
  digitalWrite(46, pwr);
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
  }
}
#endif

#ifdef USE_SHINE

#include <layer3.h>
#include <types__.h>

typedef uint8_t mp3buf_t;

int32_t wav2mp3(char *path) {
  shine_config_t  config;
  shine_t         s;
  File wav_in;
  File mp3_out;

  wav_in = ufsp->open("test.wav", FS_FILE_READ);
  if (!wav_in) {
    return -1;
  }

  mp3_out = ufsp->open("test.mp3", FS_FILE_WRITE);
  if (!mp3_out) {
    wav_in.close();
    return -2;
  }



  shine_set_config_mpeg_defaults(&config.mpeg);

  config.mpeg.mode = MONO;
  config.mpeg.bitr = 128;
  config.wave.samplerate = 16000;  // 48kHz @39%-45% Single core usage :)
  config.wave.channels = (channels)2;


  if (shine_check_config(config.wave.samplerate, config.mpeg.bitr) < 0) {
    return -3;
  }

  s = shine_initialise(&config);
  if (!s) {
    return -4;
  }

  uint16_t samples_per_pass = shine_samples_per_pass(s);

  uint8_t *ucp;
  int written;
  int16_t *buffer;
  uint32_t bread;

  /* All the magic happens here */
  while (1) {
    bread = wav_in.read((uint8_t*)buffer, samples_per_pass * 2);
    if (bread) {
      break;
    }
    ucp = shine_encode_buffer(s, &buffer, &written);
    mp3_out.write(ucp, written);
  }

  /* Flush and write remaining data. */
  ucp = shine_flush(s, &written);
  mp3_out.write(ucp, written);

  /* Close encoder. */
  shine_close(s);

  wav_in.close();
  mp3_out.close();

  return 0;
}




#if 0
#define STREAM_BUFFERS 2
void shine_task(void *pvParameter) {

    shine_config_t  config;  // Pointer to shine_global_config
    shine_t         s;
    shine_global_config *sc;
    mp3buf_t        databufs[STREAM_BUFFERS];
    mp3buf_t        *databuf;
    int             written, samples_per_pass, err=0, next_buf = 0;

    char            *lbuf;
    BaseType_t      recvd;
    int             *_buffer, *p, i, buf_pos, source_pos, source_max_samples;
    unsigned char   *data;
    int16_t         *buffer[2]; //Stereo max samples per pass

    shine_set_config_mpeg_defaults(&config.mpeg);

    config.mpeg.mode = JOINT_STEREO;
    config.mpeg.bitr = 256;
    config.wave.samplerate = 48000;  // 48kHz @39%-45% Single core usage :)
    config.wave.channels = (channels)2;
    source_max_samples = 512;


    if(!(s = shine_initialise(&config))) {
        ESP_LOGI("SHINE: ", "Falied to allocate shine_global_config!");
        ESP_LOGI("SHINE: ", "Size shine_global_config: %d", sizeof(shine_global_config));
        vTaskDelete(NULL);
    }

    // Set up streaming buffers
    sc = s;
    for (i=0;i<STREAM_BUFFERS;i++) {
       databufs[i].buf  = malloc(800);
       databufs[i].size = 0;
    }

    //samples_per_pass 48k mono 1152 / stereo 1152
    //samples_per_pass 24k mono 576 / stereo 576
    /* Number of samples (per channel) to feed the encoder with. */
    samples_per_pass = sc->mpeg.granules_per_frame * 576;

    /* Number of samples (per channel) to feed the encoder with. */
    samples_per_pass = shine_samples_per_pass(s);
    ESP_LOGI("SHINE: ", "Samples per pass %d",samples_per_pass);

    vTaskDelay(1000/portTICK_RATE_MS);
    source_pos = buf_pos = 0;
    uint32_t sample_ctr = 0;
    int lintl[3] = {0},  lintr[3] = {0};
    int si = 0;


    while(1) {
        //Buffers are received from the audio capture task
        xQueueReceive(processed_soundbuffer_q, &buffer, portMAX_DELAY);
        //Encode
        data = shine_encode_buffer_interleaved(s, buffer, &written);

        databufs[next_buf].size = written;
        memcpy(databufs[next_buf].buf, data, written);
        databuf = &databufs[next_buf];
        //Send endoded buffer to streaming server
        recvd = xQueueSend(mp3_buffer_q, &databuf, 0);

        next_buf++;
        if (next_buf == STREAM_BUFFERS) next_buf = 0;
        //Let FreeRTOS do things
        vTaskDelay(5/portTICK_RATE_MS);

    }
    vTaskDelete(NULL);
}
#endif // USE_SHINE
#endif


#endif
