
// The first time you need to define the board model and version

// #define T4_V12

#ifndef T4_V13
#define T4_V13 // Also defined in build_flags (platformio.ini)
#endif
#include "T4_V13.h"
#include "mypwd.h"

// https://github.com/Bodmer/TFT_eSPI
#include <TFT_eSPI.h>            // Include the graphics library (this includes the sprite functions)

// https://github.com/Bodmer/TFT_eFEX
#include <TFT_eFEX.h>             // Include the extension graphics functions library


#include <SPI.h>
#include "WiFi.h"
#include <Wire.h>
#include <Ticker.h>
#include <Button2.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
//#include <Adafruit_GFX.h>

TFT_eSPI tft = TFT_eSPI();       // Create object "tft"
TFT_eFEX  fex = TFT_eFEX(&tft);  // Create TFT_eFEX object "fex" with pointer to "tft" object
WiFiUDP   ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
String httpGETRequest(const char* serverName);

unsigned long now = 0;
unsigned long EnergyUpdateTime;

uint8_t state = 2;
uint8_t oldstate = 0;
Button2 *pBtns = nullptr;
uint8_t g_btns[] =  BUTTONS_MAP;
Ticker btnscanT;

class EnergyValue {
  public:
    EnergyValue(int a_ypos, uint16_t a_color, const char * a_postfix, const char * a_prefix,
                const char* a_serverName, const char* a_queryName)
    {
      ypos = a_ypos;
      color = a_color;
      postfix = a_postfix;
      prefix = a_prefix;
      previousString = "";
      serverName = a_serverName;
      queryName = a_queryName;
    }

    ~EnergyValue() {}
  
    void clear()
    {
      int x = tft.width() / 2 ;
      int y = tft.height() / 2;
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);

      tft.setTextColor(TFT_BLACK, TFT_BLACK); // Overwrite old string with background color
      tft.drawString(previousString, x, y + ypos);
      previousString = "";
    }

    void display(String value) 
    {
      int x = tft.width() / 2 ;
      int y = tft.height() / 2;
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);

      String newString = String(postfix) + value + String(prefix);
      
      if (newString != previousString)  
      {
        tft.setTextColor(TFT_BLACK, TFT_BLACK); // Overwrite old string with background color
        tft.drawString(previousString, x, y + ypos);
        previousString = newString;

        tft.setTextColor(color, TFT_BLACK);  // Write new string
        tft.drawString(newString, x, y + ypos);
      }
    };

    void display(float value)
    {
      display(String(value));
    }
    
    void display(int value)
    {
      display(String(value));
    }
    
    String getValue()
    {
      DynamicJsonDocument doc(2048);
      String ValueToGet("");
      const char* result_0_Data = "";

      String ServerReply = httpGETRequest(serverName);
      if (ServerReply != "")
      {
        // Serial.println("ServerReply: " + ServerReply);   
        DeserializationError error = deserializeJson(doc, ServerReply);
        if (error)
        {
          Serial.println("Failed to read value:" + ServerReply);
        }
        else
        {
          result_0_Data = doc["result"][0][queryName]; // "400.299 Watt"
          // Serial.print("result_0_Data: ");
          // Serial.println(result_0_Data);
        }
        ValueToGet = String(result_0_Data);
      }
      else
      {
        Serial.println("httpGETRequest error.");
      }
      Serial.print( timeClient.getFormattedTime() + " ValueToGet:");
      Serial.println(ValueToGet);
      return ValueToGet;
    }

  private:
    int ypos;
    uint16_t color;
    const char * postfix; 
    const char * prefix;
    String previousString;
    const char* serverName;
    const char* queryName;
};

EnergyValue NetEnergy   = EnergyValue(-30, TFT_YELLOW,   "Energy=", "", "http://192.168.2.13:8080/json.htm?type=devices&rid=1627", "Usage");
EnergyValue SolarEnergy = EnergyValue(30,  TFT_GREEN,    "Solar=",  "", "http://192.168.2.13:8080/json.htm?type=devices&rid=3687", "Usage");
EnergyValue GasEnergy   = EnergyValue(90,  TFT_DARKCYAN, "Gas=",    "", "http://192.168.2.13:8080/json.htm?type=devices&rid=1631", "CounterToday");
EnergyValue Time        = EnergyValue(-120,TFT_RED,      "",        "", "", "");


void button_handle(uint8_t gpio)
{
  switch (gpio) {
    case BUTTON_1:  // Middle button
    {
  //    Serial.println("set state 1");
      state = 1;
    }
    break;

    case BUTTON_2:  // Left button
    {
  //    Serial.println("set state 2");
      state = 2;
    }
    break;

    case BUTTON_3: // Right button
    {
  //    Serial.println("set state 3");
      state = 3;
    }
    break;

    default:
  //    Serial.println("set state none");
      break;
  }
}

void button_callback(Button2 &b)
{
  for (int i = 0; i < sizeof(g_btns) / sizeof(g_btns[0]); ++i) {
    if (pBtns[i] == b) {
 //     Serial.printf("btn: %u press\n", pBtns[i].getAttachPin());
      button_handle(pBtns[i].getAttachPin());
    }
  }
} 

void button_init()
{
  uint8_t args = sizeof(g_btns) / sizeof(g_btns[0]);
  pBtns = new Button2 [args];
  for (int i = 0; i < args; ++i) {
    pBtns[i] = Button2(g_btns[i]);
    pBtns[i].setPressedHandler(button_callback);
  }

  // pBtns[0].setLongClickHandler([](Button2 & b) 
  // {
  //   #define ST7735_SLPIN   0x10
  //   #define ST7735_DISPOFF 0x28

  //   int r = digitalRead(TFT_BL);
  //   tft.writecommand(ST7735_SLPIN);
  //   tft.writecommand(ST7735_DISPOFF);
  //   digitalWrite(TFT_BL, !r);
  //   delay(500);
  //   // esp_sleep_enable_ext0_wakeup((gpio_num_t )BUTTON_1, LOW);
  //   esp_sleep_enable_ext1_wakeup(((uint64_t)(((uint64_t)1) << BUTTON_1)), ESP_EXT1_WAKEUP_ALL_LOW);
  //   esp_deep_sleep_start();
  // });
}

void button_loop() {
  for (int i = 0; i < sizeof(g_btns) / sizeof(g_btns[0]); ++i) {
    pBtns[i].loop();
  }
}

String httpGETRequest(const char* serverName)
{
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(serverName);

   // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "";

  if (httpResponseCode == 200)
  {
//    Serial.print("HTTP Response code: ");
//    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else
  {
    Serial.print(timeClient.getFormattedTime() + " HTTP Error : ");
    Serial.print(httpResponseCode );
    Serial.print(" ");
    Serial.println(http.errorToString(httpResponseCode));
  }
  // Free resources
  http.end();

  return payload;
}

  // --------------------------------------------------

void WifiConnect()
{
  WiFi.begin(MYROUTER, MYPWD);

  Serial.print(timeClient.getFormattedTime() + " Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n"+ timeClient.getFormattedTime() + " Connected to the WiFi network");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.printf("Setup\n");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  if (TFT_BL > 0) {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
  }

  button_init();
  tft.setTextFont(1);
  tft.setTextSize(1);

  if (I2C_SDA > 0) {
    Wire.begin(I2C_SDA, I2C_SCL);
  }

  btnscanT.attach_ms(30, button_loop);

  now = millis();
  EnergyUpdateTime = now;

  if (WiFi.status() != WL_CONNECTED)
  {
    WifiConnect();
  }
  
  timeClient.begin();
  while (!timeClient.forceUpdate())
  {
    Serial.printf("Waiting for timeClient update\n");
    delay(500);
  };
}
 
//--------------------------------------------------

void loop()
{
  now = millis();

  if (WiFi.status() != WL_CONNECTED)
  {
    WifiConnect();
  }

  if (oldstate != state)
  {
    switch (oldstate)
    {
      case 1:
        break;

      case 2:
        tft.fillScreen(TFT_BLACK);
        NetEnergy.clear();
        GasEnergy.clear();
        SolarEnergy.clear();
        break;

      case 3:
        break;

      default:
        break;
    }

    switch (state)
    {
      case 1:
        break;

      case 2:
        EnergyUpdateTime = now - 1; // Force immediate update
        break;

      case 3:
        break;

      default:
        break;
    }
  }
  
  switch (state)
  {
    case 1:  // Middle button
      Serial.println("case 1");
      break;

    case 2:  // Left button
      Serial.println("case 2");

      timeClient.update();
      Time.display(timeClient.getFormattedTime());

      if (now > EnergyUpdateTime)
      {
        String NetEnergyValue = NetEnergy.getValue();
        NetEnergy.display(NetEnergyValue);

        String GasVerbruikValue = GasEnergy.getValue();
        GasEnergy.display(GasVerbruikValue);

        String ZonpbrengstValue = SolarEnergy.getValue();
        SolarEnergy.display(ZonpbrengstValue);

        EnergyUpdateTime = now + (10 * 1000);  // 10 sec

        Serial.println("---------------");
      }
      break;

    case 3:  // Right button
      Serial.println("case 3");
      break;

    default:
      Serial.println("case default");
      break;
  }

  oldstate = state;
  delay(1000);
}
