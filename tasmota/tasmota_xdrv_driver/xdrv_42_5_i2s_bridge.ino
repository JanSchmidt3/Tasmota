
/*
  audio is2 support pcm bridge

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
#if defined(USE_I2S_MIC) && defined(I2S_BRIDGE)

#define I2S_BRIDGE_PORT 6970
#define I2S_BRIDGE_BUFFER_SIZE 1024

#define I2S_BRIDGE_MODE_OFF 0
#define I2S_BRIDGE_MODE_READ 1
#define I2S_BRIDGE_MODE_WRITE 2
#define I2S_BRIDGE_MODE_MASTER 4

void i2s_bridge_init(uint8_t mode) {
  if (I2S_BRIDGE_MODE_OFF == mode) {
    audio_i2s.i2s_bridge_udp.flush();
    audio_i2s.i2s_bridge_udp.stop();
  } else {
    audio_i2s.i2s_bridge_udp.begin(I2S_BRIDGE_PORT);
    xTaskCreatePinnedToCore(i2s_bridge_task, "BRIDGE", 8192, NULL, 3, &audio_i2s.i2s_bridge_h, 1);
    AddLog(LOG_LEVEL_INFO, PSTR("I2S_bridge: started"));
  }
}

void i2s_bridge_task(void *arg) {
int16_t packet_buffer[I2S_BRIDGE_BUFFER_SIZE];
uint16_t bytesize;

  while (I2S_BRIDGE_MODE_OFF != audio_i2s.bridge_mode) {
    if (audio_i2s.bridge_mode & I2S_BRIDGE_MODE_READ) {
      if (audio_i2s.i2s_bridge_udp.parsePacket()) {
        uint32_t bytes_written;
        int32_t len = audio_i2s.i2s_bridge_udp.read((uint8_t *)packet_buffer, I2S_BRIDGE_BUFFER_SIZE - 1);
        if (!(audio_i2s.bridge_mode & I2S_BRIDGE_MODE_MASTER)) {
          audio_i2s.i2s_bridge_ip = audio_i2s.i2s_bridge_udp.remoteIP();
        }
        i2s_write(audio_i2s.mic_port, (const uint8_t*)packet_buffer, sizeof(uint32_t), &bytes_written, 0);
      }
    }

    if (audio_i2s.bridge_mode & I2S_BRIDGE_MODE_WRITE) {
      uint32_t bytes_read;
      bytesize = I2S_BRIDGE_BUFFER_SIZE;
      i2s_read(audio_i2s.mic_port, (char *)packet_buffer, bytesize, &bytes_read, (100 / portTICK_RATE_MS));
      audio_i2s.i2s_bridge_udp.beginPacket(audio_i2s.i2s_bridge_ip, I2S_BRIDGE_PORT);
      audio_i2s.i2s_bridge_udp.write((const uint8_t*)packet_buffer, bytes_read);
      audio_i2s.i2s_bridge_udp.endPacket();
    }
    delay(1);
    //vTaskDelay(0);
    //taskYIELD();
  }
  AddLog(LOG_LEVEL_INFO, PSTR("I2S_bridge: stopped"));
  vTaskDelete(NULL);
}



void Cmd_I2SBridge(void) {
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 7)) {
    audio_i2s.bridge_mode = XdrvMailbox.payload;
    if (audio_i2s.bridge_mode & I2S_BRIDGE_MODE_MASTER) {
      audio_i2s.i2s_bridge_ip = audio_i2s.i2s_bridge_ip.fromString("192.168.188.105");
    }
    i2s_bridge_init(audio_i2s.bridge_mode);
  }
  ResponseCmndNumber(audio_i2s.bridge_mode);
}


#endif // USE_I2S_MIC
#endif // USE_I2S_AUDIO
#endif // ESP32
