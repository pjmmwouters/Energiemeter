
// The first time you need to define the board model and version

// #define T4_V12

#ifndef T4_V13
#define T4_V13 // Also defined in build_flags (platformio.ini)
#endif
#include "T4_V13.h"

// https://github.com/Bodmer/TFT_eSPI
#include <TFT_eSPI.h>            // Include the graphics library (this includes the sprite functions)

// https://github.com/Bodmer/TFT_eFEX
//#include <TFT_eFEX.h>             // Include the extension graphics functions library


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

//TFT_eFEX  fex = TFT_eFEX(&tft);    // Create TFT_eFEX object "fex" with pointer to "tft" object


WiFiUDP   ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

unsigned long now = 0;
unsigned long SolarUpdateTime;
unsigned long EnergyUpdateTime;

#ifdef ENABLE_MPU9250
#include "sensor.h"
extern MPU9250 IMU;
#endif

SPIClass sdSPI(VSPI);
#define IP5306_ADDR         0X75
#define IP5306_REG_SYS_CTL0 0x00


uint8_t state = 0;
Button2 *pBtns = nullptr;
uint8_t g_btns[] =  BUTTONS_MAP;
Ticker btnscanT;

class EnergyValue {
  public:
    EnergyValue(int a_ypos, uint16_t a_color, const char * a_postfix, const char * a_prefix)
    {
      ypos = a_ypos;
      color = a_color;
      postfix = a_postfix;
      prefix = a_prefix;
      previousString = "";
    }

    ~EnergyValue() {}
  
    void display(String value) 
    {
      int x = tft.width() / 2 ;
      int y = tft.height() / 2;
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);

      String newString = String(postfix) + value + String(prefix);
      
      if (newString != previousString)
      {
        tft.setTextColor(TFT_BLACK, TFT_BLACK);
        tft.drawString(previousString, x, y + ypos);
        previousString = newString;
      }
      tft.setTextColor(color, TFT_BLACK);
      tft.drawString(newString, x, y + ypos);
    };

    void display(float value)
    {
      display(String(value));
    }
    
    void display(int value)
    {
      display(String(value));
    }
    
  private:
    int ypos;
    uint16_t color;
    const char * postfix; 
    const char * prefix;
    String previousString;

};

EnergyValue NetEnergy   = EnergyValue(-30, TFT_YELLOW,   "Energy=", "");
EnergyValue SolarEnergy = EnergyValue(30,  TFT_GREEN,    "Solar=",  " Watt");
EnergyValue GasEnergy   = EnergyValue(90,  TFT_DARKCYAN, "Gas=",    "");
EnergyValue Time        = EnergyValue(-120,TFT_RED,      "",        "");

void button_handle(uint8_t gpio)
{
  switch (gpio) {
#ifdef BUTTON_1
    case BUTTON_1: {
        state = 1;
      }
      break;
#endif

#ifdef BUTTON_2
    case BUTTON_2: {
        state = 2;
      }
      break;
#endif

#ifdef BUTTON_3
    case BUTTON_3: {
        state = 3;
      }
      break;
#endif

#ifdef BUTTON_4
    case BUTTON_4: {
        state = 4;
      }
      break;
#endif
    default:
      break;
  }
}

void button_callback(Button2 &b)
{
  for (int i = 0; i < sizeof(g_btns) / sizeof(g_btns[0]); ++i) {
    if (pBtns[i] == b) {
      Serial.printf("btn: %u press\n", pBtns[i].getAttachPin());
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
  pBtns[0].setLongClickHandler([](Button2 & b) 
  {
    #define ST7735_SLPIN   0x10
    #define ST7735_DISPOFF 0x28

    int r = digitalRead(TFT_BL);
    tft.writecommand(ST7735_SLPIN);
    tft.writecommand(ST7735_DISPOFF);
    digitalWrite(TFT_BL, !r);
    delay(500);
    // esp_sleep_enable_ext0_wakeup((gpio_num_t )BUTTON_1, LOW);
    esp_sleep_enable_ext1_wakeup(((uint64_t)(((uint64_t)1) << BUTTON_1)), ESP_EXT1_WAKEUP_ALL_LOW);
    esp_deep_sleep_start();
  });
}

void button_loop() {
  for (int i = 0; i < sizeof(g_btns) / sizeof(g_btns[0]); ++i) {
    pBtns[i].loop();
  }
}

String httpGETRequest(const char* serverName)
{
   HTTPClient http;

  // // Your IP address with path or Domain name with URL path
  http.begin(serverName);

  // WiFiClient client;
  // HTTPClient http;
  // http.begin(client, serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "";

  if (httpResponseCode == 200) {
//    Serial.print("HTTP Response code: ");
//    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
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
  const char* ssid = "MyRouter";
  const char* password = "MijnNaamIsPeter";

  WiFi.begin(ssid, password);

  Serial.print(timeClient.getFormattedTime() + " Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n"+ timeClient.getFormattedTime() + " Connected to the WiFi network");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  //Pin out Dump
//  Serial.printf("Current select %s version\n", BOARD_VRESION);
//  Serial.printf("TFT_MISO:%d\n", TFT_MISO);
//  Serial.printf("TFT_MOSI:%d\n", TFT_MOSI);
//  Serial.printf("TFT_SCLK:%d\n", TFT_SCLK);
//  Serial.printf("TFT_CS:%d\n", TFT_CS);
//  Serial.printf("TFT_DC:%d\n", TFT_DC);
//  Serial.printf("TFT_RST:%d\n", TFT_RST);
//  Serial.printf("TFT_BL:%d\n", TFT_BL);
//  Serial.printf("SD_MISO:%d\n", SD_MISO);
//  Serial.printf("SD_MOSI:%d\n", SD_MOSI);
//  Serial.printf("SD_SCLK:%d\n", SD_SCLK);
//  Serial.printf("SD_CS:%d\n", SD_CS);
//  Serial.printf("I2C_SDA:%d\n", I2C_SDA);
//  Serial.printf("I2C_SCL:%d\n", I2C_SCL);
//  Serial.printf("SPEAKER_PWD:%d\n", SPEAKER_PWD);
//  Serial.printf("SPEAKER_OUT:%d\n", SPEAKER_OUT);
//  Serial.printf("ADC_IN:%d\n", ADC_IN);
//  Serial.printf("BUTTON_1:%d\n", BUTTON_1);
//  Serial.printf("BUTTON_2:%d\n", BUTTON_2);
//  Serial.printf("BUTTON_3:%d\n", BUTTON_3);
//#ifdef BUTTON_4
//  Serial.printf("BUTTON_4:%d\n", BUTTON_4);
//#endif
//#ifdef ILI9341_DRIVER
//  Serial.printf("ILI9341_DRIVER defined\n");
//#endif

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
#ifdef ENABLE_MPU9250
    setupMPU9250();
#endif
  }
  btnscanT.attach_ms(30, button_loop);

  now = millis();
  SolarUpdateTime = now - 1;
  EnergyUpdateTime = now - 1;

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

// void PrintSolarEnergy(float SolarEnergy)
// {
//     static String previousString("");
//     int x = tft.width() / 2 ;
//     int y = tft.height() / 2;
//     tft.setTextSize(2);
//     tft.setTextDatum(MC_DATUM);

//     String newString = String("Solar=") + String(SolarEnergy) + String(" Watt");
    
//     if (newString != previousString)
//     {
//       tft.setTextColor(TFT_BLACK, TFT_BLACK);
//       tft.drawString(previousString, x, y + 30);
//       previousString = newString;
//     }
//     tft.setTextColor(TFT_GREEN, TFT_BLACK);
//     tft.drawString(newString, x, y + 30);
// }

// void PrintNetEnergy(String NetEnergy)
// {
//     static String previousString("");
//     int x = tft.width() / 2 ;
//     int y = tft.height() / 2;
//     tft.setTextSize(2);
//     tft.setTextDatum(MC_DATUM);

//     String newString = String("Energy=") + NetEnergy;

//     if (newString != previousString)
//     {
//      tft.setTextColor(TFT_BLACK, TFT_BLACK);
//       tft.drawString(previousString, x, y - 30);
//       previousString = newString;
//     }
//     tft.setTextColor(TFT_YELLOW, TFT_BLACK);
//     tft.drawString(newString, x, y - 30);
// }

// void PrintGasVerbruik(String GasVerbruik)
// {
//   static String previousString("");
//   int x = tft.width() / 2 ;
//   int y = tft.height() / 2;
//   tft.setTextSize(2);
//   tft.setTextDatum(MC_DATUM);

//   String newString = String("Gas=") + GasVerbruik;

//   if (newString != previousString)
//   {
//     tft.setTextColor(TFT_BLACK, TFT_BLACK);
//     tft.drawString(previousString, x, y + 90);
//     previousString = newString;
//   }
//   tft.setTextColor(TFT_DARKCYAN, TFT_BLACK);
//   tft.drawString(newString, x, y + 90);
// }

// void PrintTime()
// {
//   static String previousString("");

//   timeClient.update();

//   int x = tft.width() / 2 ;
//   int y = tft.height() / 2;
//   tft.setTextSize(2);
//   tft.setTextDatum(MC_DATUM);

//   String newString = timeClient.getFormattedTime();

//   if (newString != previousString)
//   {
//     tft.setTextColor(TFT_BLACK, TFT_BLACK);
//     tft.drawString(previousString, x, y -120);
//     previousString = newString;
//   }
//   tft.setTextColor(TFT_RED, TFT_BLACK);
//   tft.drawString(newString, x, y - 120);
// }

float GetSolarEnergy()
{
  DynamicJsonDocument doc(512);
  float SolarEnergy = 0.0;
  String LastUpdateTime;
  const char* serverNameSolarEnergy = "https://monitoringapi.solaredge.com/site/1407682/overview?api_key=ODRHY51XSYXOV0UJOTNE1K8SXWZ00VYD";

  String SolarEnergyReply = httpGETRequest(serverNameSolarEnergy); //{"overview":{"lastUpdateTime":"2021-02-16 09:12:59",
                                                                   // "lifeTimeData":{"energy":3163596.0,"revenue":632.80164},
                                                                   // "lastYearData":{"energy":64260.0},"lastMonthData":{"energy":9912.0},
                                                                   // "lastDayData":{"energy":29.0},"currentPower":{"power":146.30037},
                                                                   // "measuredBy":"INVERTER"}}
  if (SolarEnergyReply != "")
  {
    Serial.println("SolarEnergy: " + SolarEnergyReply);
    DeserializationError error = deserializeJson(doc, SolarEnergyReply);
    if (error)
    {
      Serial.print("Failed to deserialize SolarEnergy. Trying again in 30 seconds : " + SolarEnergyReply);
      SolarUpdateTime = now + (30 * 1000);
    }
    else
    {
      SolarEnergy = doc["overview"]["currentPower"]["power"]; // 146.30037
      
      JsonObject result_0 = doc["overview"];     
      const char* result_0_Data = result_0["lastUpdateTime"]; // "2021-02-16 09:12:59"

      LastUpdateTime = String(result_0_Data);
      Serial.print("LastUpdateTime=");
      Serial.println(LastUpdateTime);
      // Serial.println(hr);
      // Serial.println(mi);
      // Serial.println(se);

      // Get hours, min and sec from lastUpdateTime
      char result[80];
      strcpy(result, result_0_Data);
      char* tmp = &result[0];
      tmp = strtok(tmp, " ");
      int hr = atoi(strtok(NULL, ":"));
      int mi = atoi(strtok(NULL, ":"));
      int se = atoi(strtok(NULL, ""));
      long LastUpdateTimeMs = ((hr * 3600) + (mi * 60) + se ) * 1000;

      // Calculate ms since startup for getting new solar energy. 
      // SolarUpdateTime = lastupdateTime + SolarEdgeUpdatetime(=15min) + 2min
      long TimeToNextUpdateMs = (15 + 2) * 60 * 1000;
      long ClockTimeMs = ((timeClient.getHours() * 3600) + (timeClient.getMinutes() * 60) + (timeClient.getSeconds())) * 1000;
      long dClockMs = (ClockTimeMs - LastUpdateTimeMs);

      SolarUpdateTime = (now - dClockMs) + TimeToNextUpdateMs;
      if (SolarUpdateTime <= now) // Probably data is not updated yet
      {
        Serial.println(timeClient.getFormattedTime() + " Data not updated yet. Trying again in 5 min");
        SolarUpdateTime = now + 5 * 60 * 100;  // Try again in 5 min
      }

      Serial.println("---------------------");
      Serial.println(LastUpdateTimeMs);
      Serial.println(TimeToNextUpdateMs);
      Serial.println(ClockTimeMs);
      Serial.println(dClockMs);
      Serial.println("---------------------");
    }
  }
  else
  {
    Serial.println("httpGETRequest error. Trying again in 30 seconds.");
    SolarUpdateTime = now + (30 * 1000);
  }
  Serial.print("SolarUpdateTime=");
  Serial.println(SolarUpdateTime);
  Serial.print("Now=");
  Serial.println(now);

  Serial.print(timeClient.getFormattedTime() + " SolarEnergy:");
  Serial.println(SolarEnergy);
 
  return SolarEnergy;
}

String getGasVerbruik()
{
  DynamicJsonDocument doc(2048);
  const char* serverNameGasVerbruik = "http://192.168.2.13:8080/json.htm?type=devices&rid=1631";
  String GasVerbruikVandaag("");
  const char* result_0_Data = "";

  String GasVerbruikReply = httpGETRequest(serverNameGasVerbruik);
  if (GasVerbruikReply != "")
  {
  //  Serial.println("GasVerbruikReply: " + GasVerbruikReply);   
    DeserializationError error = deserializeJson(doc, GasVerbruikReply);
    if (error)
    {
      Serial.println("Failed to read GasVerbruik:" + GasVerbruikReply);
    }
    else
    {
      JsonObject result_0 = doc["result"][0];
      result_0_Data = result_0["CounterToday"]; // "7.170 m3"
  //    Serial.print("result_0_Data: ");
  //    Serial.println(result_0_Data);
    }
  
    GasVerbruikVandaag = String(result_0_Data);
  }
  else
  {
    Serial.println("httpGETRequest error.");
  }
  Serial.print( timeClient.getFormattedTime() + " GasVerbruikVandaag:");
  Serial.println(GasVerbruikVandaag);
  return GasVerbruikVandaag;
}

String GetNetEnergy()
{
  DynamicJsonDocument doc(2048);
  const char* serverNameEnergy = "http://192.168.2.13:8080/json.htm?type=devices&rid=1627";
  String EnergyUsage("");
  const char* result_0_Data;

  String NetEnergyReply = httpGETRequest(serverNameEnergy);
  if (NetEnergyReply != "")
  {
//    Serial.println("NetEnergyReply: " + NetEnergyReply);
    
    DeserializationError error = deserializeJson(doc, NetEnergyReply);
    if (error)
    {
      Serial.println("Failed to read NetEnergy:" + NetEnergyReply);
    }
    else
    {
      JsonObject result_0 = doc["result"][0];
      result_0_Data = result_0["Usage"]; // "879 Watt"

      EnergyUsage = String(result_0_Data);
    }

//      JsonObject result_0 = doc["result"][0];
//      const char* result_0_Data = result_0["Data"]; // "3601338;2777420;526205;1363282;506;0"
//    //  Serial.print("result_0_Data: ");
//    //  Serial.println(result_0_Data);
//    
//      const char s[2] = ";";
//      char result[80];
//      strcpy(result, result_0_Data);
//      char* tmp = &result[0];
//      tmp = strtok(tmp, s);
//      tmp = strtok(NULL, s);
//      tmp = strtok(NULL, s);
//      tmp = strtok(NULL, s);
//      tmp = strtok(NULL, s);
//      Energy = atoi(tmp);
//    }

  }
  else
  {
    Serial.println("httpGETRequest error.");
  }

  Serial.print( timeClient.getFormattedTime() + " NetEnergy:");
  Serial.println(EnergyUsage);
  return EnergyUsage;
}


void loop()
{

  now = millis();

  if (WiFi.status() != WL_CONNECTED)
  {
    WifiConnect();
  }

  timeClient.update();
  Time.display(timeClient.getFormattedTime());

  Serial.print(now);
  Serial.print(" ");
  Serial.print(SolarUpdateTime);
  Serial.print("  ");
  Serial.print((SolarUpdateTime - now) / 1000);
  Serial.println(" sec");

  int len = ((SolarUpdateTime - now) * tft.width() / (17*60*1000));
  tft.drawLine(0, 200, len, 200, TFT_GREEN);
  tft.drawLine(len + 1, 200, tft.width(), 200, TFT_BLACK);

  if (now > SolarUpdateTime)
  {
//    Serial.println("Updating Solar");

    float SolarEnergyValue = GetSolarEnergy();
    SolarEnergy.display(SolarEnergyValue);

//    SolarUpdateTime = SolarUpdateTime + (15 * 60 * 1000); // 15 min
  }

  if (now > EnergyUpdateTime)
  {
//    Serial.println("Updating Energy");

    String NetEnergyValue = GetNetEnergy();
    NetEnergy.display(NetEnergyValue);

    String GasVerbruikValue = getGasVerbruik();
    GasEnergy.display(GasVerbruikValue);
    
    EnergyUpdateTime = EnergyUpdateTime + (10 * 1000);  // 10 sec
  }
  
  delay(1000);

}
