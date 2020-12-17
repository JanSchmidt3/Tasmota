/*
  xdrv_84_core2.ino - ESP32 m5stack core2 support for Tasmota

  Copyright (C) 2020  Gerhard Mutz and Theo Arends

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
#ifdef USE_M5STACK_CORE2

#include <AXP192.h>
#include <MPU6886.h>
#include <i2c_bus.h>
#include <soc/rtc.h>

#define XDRV_84          84

struct CORE2_globs {
  AXP192 Axp;
  MPU6886 Mpu;
} core2_globs;

struct CORE2_ADC {
  float vbus_v;
  float vbus_c;
  float batt_v;
  float batt_c;
  float batt_cc;
  float temp;
  uint16_t per;
} core2_adc;

// cause SC card is needed by scripter
void CORE2_Module_Init(void) {

  // m5stack uses pin 38 not selectable in tasmota
  SPI.setFrequency(40000000);
  SPI.begin(18, 38, 23, -1);
  // establish power chip on wire1 SDA 21, SCL 22
  core2_globs.Axp.begin();
  I2cSetActiveFound(AXP_ADDR, "AXP192");

  core2_globs.Axp.SetAdcState(true);

  core2_globs.Mpu.Init();
  I2cSetActiveFound(MPU6886_ADDRESS, "MPU6886");

}


void CORE2_Init(void) {

}

void CORE2_audio_power(bool power) {
  core2_globs.Axp.SetSpkEnable(power);
}

#ifdef USE_WEBSERVER
const char HTTP_CORE2[] PROGMEM =
 "{s}VBUS Voltage" "{m}%s V" "{e}"
// "{s}VBUS Current" "{m}%s A" "{e}"
 "{s}BATT Voltage" "{m}%s V" "{e}"
// "{s}BATT Current" "{m}%s A" "{e}"
// "{s}BATT Charge" "{m}%d mA" "{e}"
 "{s}Chip Temperature" "{m}%s C" "{e}";
#ifdef USE_MPU6886
const char HTTP_CORE2_MPU[] PROGMEM =
 "{s}MPU x" "{m}%d mg" "{e}"
 "{s}MPU y" "{m}%d mg" "{e}"
 "{s}MPU z" "{m}%d mg" "{e}";
#endif // USE_BMA423
#endif  // USE_WEBSERVER


void CORE2_loop(uint32_t flg) {
}

void CORE2_WebShow(uint32_t json) {

  CORE2_GetADC();

#ifdef USE_MPU6886
  float x;
  float y;
  float z;
  core2_globs.Mpu.getAccelData(&x, &y, &z);
  int16_t ix=x*1000;
  int16_t iy=y*1000;
  int16_t iz=z*1000;
#endif

  char vstring[32];
  //char cstring[32];
  char bvstring[32];
  //char bcstring[32];
  //char bccstring[32];
  char tstring[32];
  dtostrfd(core2_adc.vbus_v,3,vstring);
 //  dtostrfd(core2_adc.vbus_c,3,cstring);
  dtostrfd(core2_adc.batt_v,3,bvstring);
//  dtostrfd(core2_adc.batt_c,3,bcstring);
//  dtostrfd(core2_adc.batt_cc,3,bccstring);
  dtostrfd(core2_adc.temp,2,tstring);

  if (json) {
    //ResponseAppend_P(PSTR(",\"CORE2\":{\"VBV\":%s,\"VBC\":%s,\"BV\":%s,\"BC\":%s,\"BCC\":%s,\"CT\":%s"),
    //                 vstring, cstring, bvstring, bcstring, bccstring, tstring);
    ResponseAppend_P(PSTR(",\"CORE2\":{\"VBV\":%s,\"BV\":%s,\"CT\":%s"),
                                      vstring, bvstring, tstring);

#ifdef USE_MPU6886
    ResponseAppend_P(PSTR(",\"MPUX\":%d,\"MPUY\":%d,\"MPUZ\":%d"),ix,iy,iz);
#endif
    ResponseJsonEnd();
  } else {
    //WSContentSend_PD(HTTP_CORE2,vstring,cstring,bvstring,bcstring,bcstring,tstring);
    WSContentSend_PD(HTTP_CORE2,vstring, bvstring, tstring);

#ifdef USE_MPU6886
    WSContentSend_PD(HTTP_CORE2_MPU, ix, iy, iz);
#endif // USE_BMA423
  }
}

const char CORE2_Commands[] PROGMEM = "CORE2|"
  "LSLP";

void (* const CORE2_Command[])(void) PROGMEM = {
  &CORE2_LightSleep};

void CORE2_LightSleep(void) {
}

void core2_disp_pwr(uint8_t on) {
  core2_globs.Axp.SetDCDC3(on);
}

void CORE2_GetADC(void) {
    core2_adc.vbus_v = core2_globs.Axp.GetVBusVoltage();
    core2_adc.vbus_c = core2_globs.Axp.GetVBusCurrent();
    core2_adc.batt_v = core2_globs.Axp.GetBatVoltage();
    core2_adc.batt_c = core2_globs.Axp.GetBatCurrent();
    core2_adc.batt_cc = core2_globs.Axp.GetBatChargeCurrent();
    core2_adc.temp = core2_globs.Axp.GetTempInAXP192();
}



/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv84(uint8_t function) {
  bool result = false;

  switch (function) {

    case FUNC_WEB_SENSOR:
#ifdef USE_WEBSERVER
      CORE2_WebShow(0);
#endif
      break;
    case FUNC_JSON_APPEND:
      CORE2_WebShow(1);
      break;
    case FUNC_COMMAND:
      result = DecodeCommand(CORE2_Commands, CORE2_Command);
      break;
    case FUNC_MODULE_INIT:
      CORE2_Module_Init();
      break;
    case FUNC_INIT:
      CORE2_Init();
      break;
    case FUNC_LOOP:
      CORE2_loop(1);
      break;

  }
  return result;
}

#endif  // USE_M5STACK_CORE2
#endif  // ESP32
