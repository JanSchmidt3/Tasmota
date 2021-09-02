#ifndef Powerwall_h
#define Powerwall_h

// include libraries
//#include <Arduino.h>
#include <WiFiClientSecure.h>
//#include <ArduinoJson.h>

//#include <math_tools.h>

// import config files
//#include <config.h>
//#include <secrets.h>


class Powerwall {
   private:
    const char* powerwall_ip;
    String tesla_email;
    String tesla_password;
    String authCookie;
    float lastSOCPerc;
    float lastPowers[4];

   public:
    Powerwall();
    String getAuthCookie();
    String powerwallGetRequest(String url, String authCookie);
    String powerwallGetRequest(String url);
    float currBattPerc(String authCookie);
    float* currPowers(String authCookie);
};


Powerwall::Powerwall() {
    powerwall_ip   = POWERWALL_IP_CONFIG;
    tesla_email    = TESLA_EMAIL;
    tesla_password = TESLA_PASSWORD;
    authCookie     = "";
    lastSOCPerc    = 0.0;

    for (int i = 0; i < 4; i++) {
        lastPowers[i] = 0.0;
    }
}


/**
 * This function returns a string with the authToken based on the basic login endpoint of
 * the powerwall in combination with the credentials from the secrets.h
 * @returns authToken to be used in an authCookie
 */
String Powerwall::getAuthCookie() {
    Serial.printf("(DEV: requesting new auth Cookie from %s)\n",powerwall_ip);
    String apiLoginURL = "/api/login/Basic";

    esp_log_level_set("*", ESP_LOG_VERBOSE);

    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    httpsClient.setTimeout(10000);
    int retry = 0;

#define PW_RETRIES 15
    sint32_t error = 0;

    while ((!httpsClient.connect(powerwall_ip, 443)) && (retry < PW_RETRIES)) {
    //while ((!httpsClient.connect("https://api.github.com", 443)) && (retry < PW_RETRIES)) {
        delay(100);
        Serial.print(".");
        retry++;
    }


    esp_log_level_set("*", ESP_LOG_NONE);

    if (retry >= PW_RETRIES) {
      Serial.printf("conn fail");
      return ("CONN-FAIL");
    }


    Serial.printf("connected");

    String dataString;

#ifdef AJSON
    StaticJsonDocument<192> authJsonDoc;
    authJsonDoc["username"] = "customer";
    authJsonDoc["email"]    = tesla_email;
    authJsonDoc["password"] = tesla_password;
    serializeJson(authJsonDoc, dataString);
#else
    dataString = "{\"username\":\"customer\",\"email\":\"" + tesla_email + "\",\"password\":\"" + tesla_password + "\"}";
#endif

    httpsClient.print(String("POST ") + apiLoginURL + " HTTP/1.1\r\n" +
                      "Host: " + powerwall_ip + "\r\n" +
                      "Connection: close" + "\r\n" +
                      "Content-Type: application/json" + "\r\n" +
                      "Content-Length: " + dataString.length() + "\r\n" +
                      "\r\n" + dataString + "\r\n\r\n");

    while (httpsClient.connected()) {
        String response = httpsClient.readStringUntil('\n');
        if (response == "\r") {
            break;
        }
    }

    String jsonInput = httpsClient.readStringUntil('\n');

#ifdef AJSON
    StaticJsonDocument<384> authJSON;
    DeserializationError error = deserializeJson(authJSON, jsonInput);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return "ERROR (getAuthCookie())";
    }
    String result = authJSON["token"];
#else
    String result = "";
#endif


  Serial.printf("result %s\n",jsonInput.c_str());

    if (result == NULL) {
        getAuthCookie();
    }

    authCookie = result;
    return result;
}

/**
 * This function does a GET-request on the local powerwall web server.
 * This is mainly used here to do API requests.
 * HTTP/1.0 is used because some responses are so big that this would encounter
 * chunked transfer encoding in HTTP/1.1 (https://en.wikipedia.org/wiki/Chunked_transfer_encoding)
 *
 * @param url relative URL on the Powerwall
 * @param authCookie optional, but recommended
 * @returns content of request
 */
String Powerwall::powerwallGetRequest(String url, String authCookie) {
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    httpsClient.setTimeout(10000);

    String tempAuthCookie;

    if (authCookie != "") {
        tempAuthCookie = authCookie;
    } else {
        tempAuthCookie = this->getAuthCookie();
    }

    Serial.println("(DEV: doing GET-request to " + String(powerwall_ip) + String(url) + ")");

    int retry = 0;

    while ((!httpsClient.connect(powerwall_ip, 443)) && (retry < 15)) {
        delay(100);
        Serial.print(".");
        retry++;
    }

    if (retry >= 15) {
        return ("CONN-FAIL");
    }

    // HTTP/1.0 is used because of Chunked transfer encoding
    httpsClient.print(String("GET ") + url + " HTTP/1.0" + "\r\n" +
                      "Host: " + powerwall_ip + "\r\n" +
                      "Cookie: " + "AuthCookie" + "=" + authCookie + "\r\n" +
                      "Connection: close\r\n\r\n");

    while (httpsClient.connected()) {
        String response = httpsClient.readStringUntil('\n');
        if (response == "\r") {
            break;
        }
    }

    return httpsClient.readStringUntil('\n');
}

/**
 * this is getting called if there was no provided authCookie in powerwallGetRequest(String url, String authCookie)
 */
String Powerwall::powerwallGetRequest(String url) {
    return (this->powerwallGetRequest(url, this->getAuthCookie()));
}

/**
 * This function returns the current state of charge of the Powerwall in percent.
 * @param authCookie - this is optional
 * @return percent --> double
 */
float Powerwall::currBattPerc(String authCookie = "") {
    String tempAuthCookie;

    if (authCookie != "") {
        tempAuthCookie = authCookie;
    } else {
        tempAuthCookie = this->getAuthCookie();
    }

    String socJson = this->powerwallGetRequest("/api/system_status/soe", tempAuthCookie);

#ifdef AJSON
    StaticJsonDocument<48> socJsonDoc;
    DeserializationError error = deserializeJson(socJsonDoc, socJson);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return lastSOCPerc;
    }
    float output = socJsonDoc["percentage"];
    output        = round_down(output, 2);
#else
    float output = 0;
#endif

    lastSOCPerc = output;

    Serial.println("Current SOC: " + String(output) + "%");

    return output;
}

/**
 * This function returns the current power consumption of several endpoints as an array.
 * Included are Power from Grid, Battery, Home and Solar. Have in mind that some of the
 * values might be negative, but keep in mind that you have solar :-)
 * @param authCookie - this is optional
 * @return array of current power flows
 */
float* Powerwall::currPowers(String authCookie = "") {
    static float powers[4];

    String tempAuthCookie;

    if (authCookie != "") {
        tempAuthCookie = authCookie;
    } else {
        tempAuthCookie = this->getAuthCookie();
    }

    const char* metersJson = this->powerwallGetRequest("/api/meters/aggregates", tempAuthCookie).c_str();

    // Serial.println(metersJson);
#ifdef AJSON
    DynamicJsonDocument powersJsonDoc(2048);
    DeserializationError error = deserializeJson(powersJsonDoc, metersJson);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return lastPowers;
    }
    powers[0] = powersJsonDoc["site"]["instant_power"];
    powers[1] = powersJsonDoc["battery"]["instant_power"];
    powers[2] = powersJsonDoc["load"]["instant_power"];
    powers[3] = powersJsonDoc["solar"]["instant_power"];
#else

    powers[0] = 0;
    powers[1] = 0;
    powers[2] = 0;
    powers[3] = 0;

#endif
    // // testing values
    // powers[0] = -10000.00;
    // powers[1] = 20000.00;
    // powers[2] = 955.00;
    // powers[3] = 0.00;

    lastPowers[0] = powers[0];
    lastPowers[1] = powers[1];
    lastPowers[2] = powers[2];
    lastPowers[3] = powers[3];

    // clang-format off
    Serial.println("Netz-Leistung: "      +  String(powers[0]));
    Serial.println("Batterie-Leistung: "  +  String(powers[1]));
    Serial.println("Haus-Leistung: "      +  String(powers[2]));
    Serial.println("Solar-Leistung: "     +  String(powers[3]));
    // clang-format on

    return powers;
}

#endif
