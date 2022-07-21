
/*
  audio is2 support for wav mic and convert, this may be deleted later since we have mp3

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
#if (defined(USE_I2S_AUDIO) || defined(USE_TTGO_WATCH) || defined(USE_M5STACK_CORE2) || defined(ESP32S3_BOX))
#if defined(USE_I2S_MIC)

// micro to wav file

#define DATA_SIZE 1024

#ifndef USE_SHINE

void mic_task(void *arg){
  uint32_t data_offset = 0;
  while (1) {
      uint32_t bytes_read;
      i2s_read(audio_i2s.i2s_port, (char *)(audio_i2s.mic_buff + data_offset), DATA_SIZE, &bytes_read, (100 / portTICK_RATE_MS));
      if (bytes_read != DATA_SIZE) break;
      data_offset += DATA_SIZE;
      if (data_offset >= audio_i2s.mic_size-DATA_SIZE) break;
  }
  SpeakerMic(MODE_SPK);
  SaveWav(audio_i2s.mic_path, audio_i2s.mic_buff, audio_i2s.mic_size);
  free(audio_i2s.mic_buff);
  vTaskDelete(audio_i2s.mic_task_h);
}

uint32_t i2s_record(char *path, uint32_t secs) {
  esp_err_t err = ESP_OK;

  if (audio_i2s.decoder || audio_i2s.mp3) return 0;

  err = SpeakerMic(MODE_MIC);
  if (err) {
    SpeakerMic(MODE_SPK);
    return err;
  }

  audio_i2s.mic_size = secs * audio_i2s.mic_rate * 2 * audio_i2s.mic_channels;

  audio_i2s.mic_buff = (uint8_t*)heap_caps_malloc(audio_i2s.mic_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!audio_i2s.mic_buff) return 2;

  if (*path=='+') {
    path++;
    strlcpy(audio_i2s.mic_path, path , sizeof(audio_i2s.mic_path));
    xTaskCreatePinnedToCore(mic_task, "MIC", 4096, NULL, 3, &audio_i2s.mic_task_h, 1);
    return 0;
  }

  uint32_t data_offset = 0;
  uint32_t stime=millis();
  while (1) {
    uint32_t bytes_read;
    i2s_read(audio_i2s.i2s_port, (char *)(audio_i2s.mic_buff + data_offset), DATA_SIZE, &bytes_read, (100 / portTICK_RATE_MS));
    if (bytes_read != DATA_SIZE) break;
    data_offset += DATA_SIZE;
    if (data_offset >= audio_i2s.mic_size-DATA_SIZE) break;
    delay(0);
  }
  //AddLog(LOG_LEVEL_INFO, PSTR("rectime: %d ms"), millis()-stime);
  SpeakerMic(MODE_SPK);
  // save to path
  SaveWav(path, audio_i2s.mic_buff, audio_i2s.mic_size);
  free(audio_i2s.mic_buff);
  return 0;
}

static const uint8_t wavHTemplate[] PROGMEM = { // Hardcoded simple WAV header with 0xffffffff lengths all around
    0x52, 0x49, 0x46, 0x46, 0xff, 0xff, 0xff, 0xff, 0x57, 0x41, 0x56, 0x45,
    0x66, 0x6d, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x22, 0x56, 0x00, 0x00, 0x88, 0x58, 0x01, 0x00, 0x04, 0x00, 0x10, 0x00,
    0x64, 0x61, 0x74, 0x61, 0xff, 0xff, 0xff, 0xff };

bool SaveWav(char *path, uint8_t *buff, uint32_t size) {
  File fwp = ufsp->open(path, "w");
  if (!fwp) return false;
  uint8_t wavHeader[sizeof(wavHTemplate)];
  memcpy_P(wavHeader, wavHTemplate, sizeof(wavHTemplate));

  uint8_t channels = audio_i2s.mic_channels;
  uint32_t hertz = audio_i2s.mic_rate;
  uint8_t bps = 16;

  wavHeader[22] = channels & 0xff;
  wavHeader[23] = 0;
  wavHeader[24] = hertz & 0xff;
  wavHeader[25] = (hertz >> 8) & 0xff;
  wavHeader[26] = (hertz >> 16) & 0xff;
  wavHeader[27] = (hertz >> 24) & 0xff;
  int byteRate = hertz * bps * channels / 8;
  wavHeader[28] = byteRate & 0xff;
  wavHeader[29] = (byteRate >> 8) & 0xff;
  wavHeader[30] = (byteRate >> 16) & 0xff;
  wavHeader[31] = (byteRate >> 24) & 0xff;
  wavHeader[32] = channels * bps / 8;
  wavHeader[33] = 0;
  wavHeader[34] = bps;
  wavHeader[35] = 0;

  fwp.write(wavHeader, sizeof(wavHeader));

  fwp.write(buff, size);
  fwp.close();

  return true;
}


void Cmd_MicRec(void) {

  if (audio_i2s.mic_task_h) {
    // stop task
    vTaskDelete(audio_i2s.mic_task_h);
    audio_i2s.mic_task_h = nullptr;
    ResponseCmndChar_P(PSTR("Stopped"));
  }

  if (XdrvMailbox.data_len > 0) {
    uint16 time = 10;
    char *cp = strchr(XdrvMailbox.data, ':');
    if (cp) {
      time = atoi(cp + 1);
      *cp = 0;
    }
    if (time<10) time = 10;
    if (time>30) time = 30;
    i2s_record(XdrvMailbox.data, time);
    ResponseCmndChar(XdrvMailbox.data);
  }
}

#endif // no USE_SHINE


#ifdef USE_SHINE
#ifdef WAV2MP3
#include <layer3.h>
#include <types.h>
typedef uint8_t mp3buf_t;


// min freq = 16 KHz Stereo or 32 KHz Mono
int32_t wav2mp3(char *path) {
  int32_t error = 0;
  shine_config_t  config;
  shine_t s = nullptr;
  File wav_in = (File)nullptr;
  File mp3_out = (File)nullptr;
  uint8_t *ucp;
  int written;
  int16_t *buffer = nullptr;
  uint32_t bread;
  uint16_t samples_per_pass;
  char mpath[64];
  char *cp;
  uint8_t chans = 1;
  uint32_t sfreq = 16000;

  strlcpy(mpath, path, sizeof(mpath));

  wav_in = ufsp->open(mpath, FS_FILE_READ);
  if (!wav_in) {
    error = -1;
    goto exit;
  }

  // script>wav2mp3("/test2.wav")
  uint8_t wavHeader[sizeof(wavHTemplate)];
  wav_in.read((uint8_t*)wavHeader, sizeof(wavHTemplate));
  chans = wavHeader[22];
  sfreq = wavHeader[24]|(wavHeader[25]<<8)|(wavHeader[26]<<16)|(wavHeader[27]<<24);

  cp = strchr(mpath, '.');
  if (!cp) {
    error = -6;
    goto exit;
  }

  strcpy(cp, ".mp3");

  mp3_out = ufsp->open(mpath, FS_FILE_WRITE);
  if (!mp3_out) {
    error = -2;
    goto exit;
  }

  shine_set_config_mpeg_defaults(&config.mpeg);

  if (chans == 1) {
    config.mpeg.mode = MONO;
  } else {
    config.mpeg.mode = STEREO;
  }
  config.mpeg.bitr = 128;
  config.wave.samplerate = sfreq;
  config.wave.channels = (channels)chans;


  if (shine_check_config(config.wave.samplerate, config.mpeg.bitr) < 0) {
    error = -3;
    goto exit;
  }

  s = shine_initialise(&config);
  if (!s) {
    error = -4;
    goto exit;
  }

  samples_per_pass = shine_samples_per_pass(s);


  buffer = (int16_t*)malloc(samples_per_pass * 2 * chans);
  if (!buffer) {
    error = -5;
    goto exit;
  }

  AddLog(LOG_LEVEL_INFO, PSTR("mp3 encoding %d channels with freq %d Hz"), chans, sfreq);

  while (1) {
    bread = wav_in.read((uint8_t*)buffer, samples_per_pass * 2 * chans);
    if (!bread) {
      break;
    }
    ucp = shine_encode_buffer_interleaved(s, buffer, &written);
    mp3_out.write(ucp, written);
  }
  ucp = shine_flush(s, &written);
  mp3_out.write(ucp, written);

exit:
  if (s) {
    shine_close(s);
  }
  if (wav_in) {
    wav_in.close();
  }
  if (mp3_out) {
    mp3_out.close();
  }

  if (buffer) {
    free(buffer);
  }

  AddLog(LOG_LEVEL_INFO, PSTR("mp3 encoding exit with code: %d"), error);

  return error;
}

#ifdef WAV2MP3
void Cmd_wav2mp3(void) {
  if (XdrvMailbox.data_len > 0) {
#ifdef USE_SHINE
    wav2mp3(XdrvMailbox.data);
#endif // USE_SHINE
  }
  ResponseCmndChar(XdrvMailbox.data);
}

#endif // WAV2MP3
#endif  // WAV2MP3
#endif // USE_SHINE
#endif // USE_I2S_MIC
#endif // USE_I2S_AUDIO
#endif // EPS32
