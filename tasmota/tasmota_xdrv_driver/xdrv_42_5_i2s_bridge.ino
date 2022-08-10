
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
#if (defined(USE_I2S_AUDIO) || defined(USE_TTGO_WATCH) || defined(USE_M5STACK_CORE2) || defined(ESP32S3_BOX) || defined(USE_I2S_MIC))
#ifdef I2S_BRIDGE

#ifndef I2S_BRIDGE_PORT
#define I2S_BRIDGE_PORT 6970
#endif

#define I2S_BRIDGE_BUFFER_SIZE 512

#define I2S_BRIDGE_MODE_OFF 0
#define I2S_BRIDGE_MODE_READ 1
#define I2S_BRIDGE_MODE_WRITE 2
#define I2S_BRIDGE_MODE_MASTER 4

void i2s_bridge_init(uint8_t mode) {

  audio_i2s.bridge_mode.mode = mode;

  if (I2S_BRIDGE_MODE_OFF == mode) {
    audio_i2s.i2s_bridge_udp.flush();
    audio_i2s.i2s_bridge_udp.stop();
    SendBridgeCmd(I2S_BRIDGE_MODE_OFF);
    AUDIO_PWR_OFF
  } else {
    audio_i2s.i2s_bridge_udp.begin(I2S_BRIDGE_PORT);
    xTaskCreatePinnedToCore(i2s_bridge_task, "BRIDGE", 8192, NULL, 3, &audio_i2s.i2s_bridge_h, 1);
    if (!(audio_i2s.bridge_mode.mode & I2S_BRIDGE_MODE_MASTER)) {
      AddLog(LOG_LEVEL_INFO, PSTR("I2S_bridge: slave started"));
    } else {
      char buffer[32];
      sprintf_P(buffer, PSTR("%u.%u.%u.%u"), audio_i2s.i2s_bridge_ip[0], audio_i2s.i2s_bridge_ip[1], audio_i2s.i2s_bridge_ip[2], audio_i2s.i2s_bridge_ip[3]);
      AddLog(LOG_LEVEL_INFO, PSTR("I2S_bridge: master started sending to ip: %s"), buffer);
    }
    AUDIO_PWR_ON
  }
}

void i2s_bridge_task(void *arg) {
int16_t packet_buffer[I2S_BRIDGE_BUFFER_SIZE/2];
uint16_t bytesize;

  while (I2S_BRIDGE_MODE_OFF != audio_i2s.bridge_mode.mode) {
    if (audio_i2s.bridge_mode.mode & I2S_BRIDGE_MODE_READ) {
      if (audio_i2s.i2s_bridge_udp.parsePacket()) {
        uint32_t bytes_written;
        uint32_t len = audio_i2s.i2s_bridge_udp.available();
        if (len > I2S_BRIDGE_BUFFER_SIZE) {
          len = I2S_BRIDGE_BUFFER_SIZE;
        }
        len = audio_i2s.i2s_bridge_udp.read((uint8_t *)packet_buffer, len);
        if (audio_i2s.bridge_mode.swap_speaker) {
          uint16_t *uwp = (uint16_t*)packet_buffer;
          for (uint32_t cnt = 0; cnt < len / 4; cnt += 2) {
            uint16_t tmp = uwp[cnt];
            uwp[cnt] = uwp[cnt + 1];
            uwp[cnt + 1] = tmp;
          }
        }
        if (!(audio_i2s.bridge_mode.mode & I2S_BRIDGE_MODE_MASTER)) {
          audio_i2s.i2s_bridge_ip = audio_i2s.i2s_bridge_udp.remoteIP();
        }
        audio_i2s.i2s_bridge_udp.flush();
        //AddLog(LOG_LEVEL_INFO, PSTR(">>> %d"), len);
        i2s_write(audio_i2s.i2s_port, (const uint8_t*)packet_buffer, len, &bytes_written, 0);
      }
    }

    if (audio_i2s.bridge_mode.mode & I2S_BRIDGE_MODE_WRITE) {
      uint32_t bytes_read;
      bytesize = I2S_BRIDGE_BUFFER_SIZE;
      i2s_read(audio_i2s.mic_port, (char *)packet_buffer, bytesize, &bytes_read, (100 / portTICK_RATE_MS));
      //AddLog(LOG_LEVEL_INFO, PSTR(">>> %d"), bytes_read);
      if (audio_i2s.bridge_mode.swap_mic) {
        uint16_t *uwp = (uint16_t*)packet_buffer;
        for (uint32_t cnt = 0; cnt < bytes_read / 4; cnt += 2) {
          uint16_t tmp = uwp[cnt];
          uwp[cnt] = uwp[cnt + 1];
          uwp[cnt + 1] = tmp;
        }
      }
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
  if (XdrvMailbox.data_len > 0) {
    char *cp = XdrvMailbox.data;
    if (strchr(cp, '.')) {
      // enter destination ip
       audio_i2s.i2s_bridge_ip.fromString(cp);
    }
  }
  I2SBridgeCmd(XdrvMailbox.payload, 1);
}

void SendBridgeCmd(uint8_t mode) {
char slavecmd[16];
  if ((audio_i2s.bridge_mode.mode & I2S_BRIDGE_MODE_MASTER) || (mode == I2S_BRIDGE_MODE_OFF)) {
    sprintf(slavecmd,"cmd:%d", mode);
    audio_i2s.i2s_bridgec_udp.beginPacket(audio_i2s.i2s_bridge_ip, I2S_BRIDGE_PORT + 1);
    audio_i2s.i2s_bridgec_udp.write((const uint8_t*)slavecmd,strlen(slavecmd));
    audio_i2s.i2s_bridgec_udp.endPacket();
  }
}

void I2SBridgeCmd(uint8_t val, uint8_t flg) {
  if ((val >= 0) && (val <= 11)) {
    if (val > 7) {
      switch (val) {
        case 8:
          audio_i2s.bridge_mode.swap_mic = 1;
          break;
        case 9:
          audio_i2s.bridge_mode.swap_mic = 0;
          break;
        case 10:
          audio_i2s.bridge_mode.swap_speaker = 1;
          break;
        case 11:
          audio_i2s.bridge_mode.swap_speaker = 0;
          break;
      }
    } else {
      if (audio_i2s.bridge_mode.mode != val) {
        if ((val == I2S_BRIDGE_MODE_OFF) && (audio_i2s.bridge_mode.mode != I2S_BRIDGE_MODE_OFF)) {
          i2s_bridge_init(I2S_BRIDGE_MODE_OFF);
        } else {
          if (audio_i2s.bridge_mode.mode == I2S_BRIDGE_MODE_OFF) {
            i2s_bridge_init(val);
          }
        }
        audio_i2s.bridge_mode.mode = val;

        if (flg) {
          if (val & I2S_BRIDGE_MODE_MASTER) {
            //audio_i2s.i2s_bridge_ip = IPAddress(192,168,188,105);
            //audio_i2s.i2s_bridge_ip.fromString("192.168.188.105");
            // set slave to complementary mode
            uint8_t slavemode = I2S_BRIDGE_MODE_READ;
            if (audio_i2s.bridge_mode.mode & I2S_BRIDGE_MODE_READ) {
              slavemode = I2S_BRIDGE_MODE_WRITE;
            }
            SendBridgeCmd(slavemode);
          }
        }
      }
    }
  }
  ResponseCmndNumber(audio_i2s.bridge_mode.mode);
}

void i2s_bridge_loop(void) {
  uint8_t packet_buffer[64];

  if (TasmotaGlobal.global_state.wifi_down) {
    return;
  }

  if (audio_i2s.i2s_bridgec_udp.parsePacket()) {
      // received control command
    memset(packet_buffer,0,sizeof(packet_buffer));
    audio_i2s.i2s_bridgec_udp.read(packet_buffer, 63);
    char *cp = (char*)packet_buffer;
    if (!strncmp(cp, "cmd:", 4)) {
      cp += 4;
      I2SBridgeCmd(atoi(cp), 0);
      audio_i2s.i2s_bridge_ip = audio_i2s.i2s_bridgec_udp.remoteIP();
      AddLog(LOG_LEVEL_INFO, PSTR("I2S_bridge: remote cmd %d"), audio_i2s.bridge_mode.mode);
    }
  }
}


void I2SBridgeInit(void) {
  // start udp control channel
  audio_i2s.i2s_bridgec_udp.begin(I2S_BRIDGE_PORT + 1);
  //audio_i2s.i2s_bridgec_udp.flush();
  //audio_i2s.i2s_bridgec_udp.stop();
}

#endif // I2S_BRIDGE
#endif // USE_I2S_AUDIO
#endif // ESP32
