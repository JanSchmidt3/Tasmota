/*
  xdsp_16_esp32_epaper_47.ino - LILIGO47 e-paper support for Tasmota

  Copyright (C) 2021  Theo Arends, Gerhard Mutz and LILIGO

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
#ifdef USE_DISPLAY
#ifdef USE_LILYGO47

#define XDSP_16                16

#define EPD47_BLACK  0
#define EPD47_WHITE 15

#include <epd4in7.h>

Epd47 *epd47;
bool epd47_init_done = false;
extern uint8_t color_type;
extern uint16_t fg_color;
extern uint16_t bg_color;

/*********************************************************************************************/

void EpdInitDriver47(void) {
  if (PinUsed(GPIO_EPD_DATA)) {

    Settings.display_model = XDSP_16;

    if (Settings.display_width != EPD47_WIDTH) {
      Settings.display_width = EPD47_WIDTH;
    }
    if (Settings.display_height != EPD47_HEIGHT) {
      Settings.display_height = EPD47_HEIGHT;
    }

    // init renderer
    epd47  = new Epd47(Settings.display_width, Settings.display_height);
    epd47->Init();

    renderer = epd47;
    renderer->DisplayInit(DISPLAY_INIT_MODE, Settings.display_size, Settings.display_rotate, Settings.display_font);
    renderer->setTextColor(EPD47_BLACK, EPD47_WHITE);

#ifdef SHOW_SPLASH
    // Welcome text
    renderer->setTextFont(2);
    renderer->DrawStringAt(50, 50, "LILGO 4.7 E-Paper Display!", EPD47_BLACK, 0);
    renderer->Updateframe();
#endif

    fg_color = EPD47_BLACK;
    bg_color = EPD47_WHITE;
    color_type = COLOR_COLOR;

#ifdef USE_FT5206
    // start digitizer
    FT5206_Touch_Init(Wire1);
#endif // USE_FT5206

    epd47_init_done = true;
    AddLog(LOG_LEVEL_INFO, PSTR("DSP: E-Paper 4.7"));
  }
}

/*********************************************************************************************/

#ifdef USE_FT5206
#ifdef USE_TOUCH_BUTTONS

uint8_t EPD47_ctouch_counter = 0;

// no rotation support
void EPD47_RotConvert(int16_t *x, int16_t *y) {
int16_t temp;
  if (renderer) {
    uint8_t rot=renderer->getRotation();
    switch (rot) {
      case 0:
        break;
      case 1:
        temp=*y;
        *y=renderer->height()-*x;
        *x=temp;
        break;
      case 2:
        *x=renderer->width()-*x;
        *y=renderer->height()-*y;
        break;
      case 3:
        temp=*y;
        *y=*x;
        *x=renderer->width()-temp;
        break;
    }
  }
}

// check digitizer hit
void EPD47_CheckTouch(void) {
  EPD47_ctouch_counter++;
  if (2 == EPD47_ctouch_counter) {
    // every 100 ms should be enough
    EPD47_ctouch_counter = 0;
    Touch_Check(EPD47_RotConvert);
  }
}
#endif // USE_TOUCH_BUTTONS
#endif // USE_FT5206


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdsp16(uint8_t function)
{
  bool result = false;

  if (FUNC_DISPLAY_INIT_DRIVER == function) {
    EpdInitDriver47();
  }
  else if (epd47_init_done && (XDSP_16 == Settings.display_model)) {
    switch (function) {
      case FUNC_DISPLAY_MODEL:
        result = true;
        break;
#if defined(USE_FT5206)
      case FUNC_DISPLAY_EVERY_50_MSECOND:
        if (FT5206_found) {
          EPD47_CheckTouch();
        }
        break;
#endif // USE_FT5206
    }
  }
  return result;
}

#endif  // USE_LILYGO47
#endif  // USE_DISPLAY
#endif  // ESP32
