
#ifdef ESP32
#if (defined(USE_I2S_AUDIO) || defined(USE_TTGO_WATCH) || defined(USE_M5STACK_CORE2) || defined(ESP32S3_BOX))
#if defined(USE_I2S_MIC) && defined(USE_SHINE) && defined(MP3_MIC_STREAM)

void Micserver(void) {

  if (!HttpCheckPriviledgedAccess()) { return; }

  String stmp = Webserver->uri();

  char *cp = strstr_P(stmp.c_str(), PSTR("/stream.mp3"));
  if (cp) {
    i2s_record_shine(/stream.mp3);
  }

}


#if 0


#define MP3_BOUNDARY "e8b8c539-047d-4777-a985-fbba6edff11e"

void Stream_mp3(void) {
  /*if(!WebcamCheckPriviledgedAccess()){
    audio_i2s.MP3Server->send(403,"","");
    return;
  }*/
  if (!audio_i2s.stream_enable) {
    return;
  }

  AddLog(LOG_LEVEL_DEBUG, PSTR("I2S: Handle mp3server"));
  audio_i2s.stream_active = 1;
  audio_i2s.client = audio_i2s.MP3Server->client();
  AddLog(LOG_LEVEL_DEBUG, PSTR("I2S: Create client"));

}

void HandleMP3Task(void) {
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;

  uint32_t tlen;

  if (!audio_i2s.client.connected()) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("MP3: Client fail"));
    audio_i2s.stream_active = 0;
  }
  if (1 == audio_i2s.stream_active) {
    audio_i2s.client.flush();
    audio_i2s.client.setTimeout(3);
    AddLog(LOG_LEVEL_DEBUG, PSTR("MP3: Start stream"));
    audio_i2s.client.print("HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace;boundary=" MP3_BOUNDARY "\r\n"
      "\r\n");
    audio_i2s.stream_active = 2;
  }
  if (2 == audio_i2s.stream_active) {

    if (0) {
      AddLog(LOG_LEVEL_DEBUG, PSTR("MP3: Frame fail"));
      audio_i2s.stream_active = 0;
    }
    audio_i2s.client.print("--" MP3_BOUNDARY "\r\n");
    audio_i2s.client.printf("Content-Type: audio/mpeg\r\n"
      "Content-Length: %d\r\n"
      "\r\n", static_cast<int>(_jpg_buf_len));

    tlen = audio_i2s.client.write(_jpg_buf, _jpg_buf_len);
    audio_i2s.client.print("\r\n");

  }
  if (0 == audio_i2s.stream_active) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("MP3: Stream exit"));
    audio_i2s.client.flush();
    audio_i2s.client.stop();
  }
}

void i2s_mp3_init(void) {
  if (!audio_i2s.stream_enable) {
    return;
  }
  audio_i2s.MP3Server = new ESP8266WebServer(81);
  audio_i2s.MP3Server->on(PSTR("/i2s.mp3"), Stream_mp3);
}

void i2s_mp3_loop(void) {
  if (!audio_i2s.stream_enable) {
    return;
  }
  audio_i2s.MP3Server->handleClient();
  if (audio_i2s.stream_active) {
    HandleMP3Task();
  }
}

void MP3ShowStream(void) {
  if (!audio_i2s.stream_enable) {
    return;
  }
  WSContentSend_P(PSTR("<p></p><center><href src='http://%_I:81/stream' alt='MP3 stream' style='width:99%%;'>mp3 stream</center><p></p>"),(uint32_t)WiFi.localIP());
}

void Cmd_MP3Stream(void) {
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 1)) {
    audio_i2s.stream_enable = XdrvMailbox.payload;
  }
  ResponseCmndNumber(audio_i2s.stream_enable);
}
#endif


#endif // USE_SHINE
#endif // USE_I2S_AUDIO
#endif // ESP32
