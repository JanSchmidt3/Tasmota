
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
#if (defined(USE_I2S_AUDIO) || defined(USE_TTGO_WATCH) || defined(USE_M5STACK_CORE2) || defined(ESP32S3_BOX) || defined(USE_I2S_MIC))
// micro to wav file

#define DATA_SIZE 1024

#ifndef USE_SHINE


#define WAV_BUFF_SIZE 512

File wav_fwp;
bool wav_record;

void mic_task(void *path){
uint8_t buffer[WAV_BUFF_SIZE];
char *file = (char*) path;

  SaveWav(file);

  wav_record = true;
  AddLog(LOG_LEVEL_INFO, PSTR("wav task created"));
  uint8_t index=0;
  while (wav_record) {
      uint32_t bytes_read;
      i2s_read(audio_i2s.mic_port, (char *)buffer, WAV_BUFF_SIZE, &bytes_read, (100 / portTICK_RATE_MS));
      uint32_t bwritten = wav_fwp.write(buffer, bytes_read);
      if (bwritten != bytes_read) {
        AddLog(LOG_LEVEL_INFO, PSTR("wav task %d - %d"),bwritten,bytes_read);
        break;
      }
      index++;
      if (index==200) break;
  }
  SpeakerMic(MODE_SPK);
  wav_fwp.close();
  AddLog(LOG_LEVEL_INFO, PSTR("wav task stopped"));
  vTaskDelete(NULL);
}

uint32_t i2s_record(char *path) {
  esp_err_t err = ESP_OK;

  if (audio_i2s.decoder || audio_i2s.mp3) return 0;

  err = SpeakerMic(MODE_MIC);
  if (err) {
    SpeakerMic(MODE_SPK);
    return err;
  }

  xTaskCreatePinnedToCore(mic_task, "MIC", 6000, (void*)path, 3, &audio_i2s.mic_task_h, 1);

  return 0;
}

static const uint8_t wavHTemplate[] PROGMEM = { // Hardcoded simple WAV header with 0xffffffff lengths all around
    0x52, 0x49, 0x46, 0x46, 0xff, 0xff, 0xff, 0xff, 0x57, 0x41, 0x56, 0x45,
    0x66, 0x6d, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x22, 0x56, 0x00, 0x00, 0x88, 0x58, 0x01, 0x00, 0x04, 0x00, 0x10, 0x00,
    0x64, 0x61, 0x74, 0x61, 0xff, 0xff, 0xff, 0xff };

bool SaveWav(char *path) {
  wav_fwp = ufsp->open(path, "w");
  if (!wav_fwp) return false;

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

  wav_fwp.write(wavHeader, sizeof(wavHeader));
  wav_fwp.flush();

  return true;
}

void Cmd_MicRec(void) {

  if (wav_record) {
    // stop task
    wav_record = false;
    delay(100);
  }

  if (XdrvMailbox.data_len > 0) {
    i2s_record(XdrvMailbox.data);
  }
  ResponseCmndChar(XdrvMailbox.data);

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

void Cmd_wav2mp3(void) {
  if (XdrvMailbox.data_len > 0) {
    wav2mp3(XdrvMailbox.data);
  }
  ResponseCmndChar(XdrvMailbox.data);
}

#endif  // WAV2MP3
#endif // USE_SHINE

#endif // USE_I2S_AUDIO
#endif // ESP32
