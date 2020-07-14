#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <font.h>

#define SSID "SSID GOES HERE"
#define PW "PASSWORD GOES HERE"
#define avURL "http://api.airvisual.com/v2/nearest_city?key=a0b13731-7fe0-4fe4-aaa5-88abd8cb2dbc"
#define timeURL "http://worldtimeapi.org/api/ip"

#define margin 5
#define timerIntv 1800000

#define itrPin D1

SSD1306Wire display(0x3c, D3, D5);

int debugCt = 0;
int itrCtl = 1; //Interrupt control
int fetchErr = 0;
int alert = 0;
int timerFlag = 0;
long int lastMillis = 0;

void ICACHE_RAM_ATTR handleITR()
{
  itrCtl = 1;
}

void drawMargin()
{
  display.fillRect(0, 0, margin, display.getHeight());
  display.fillRect(0, 0, display.getWidth(), margin);
  display.fillRect(display.getWidth() - margin, 0, margin, display.getHeight());
  display.fillRect(0, display.getHeight() - margin, display.getWidth(), margin);

  // display.display();
}

void timePrint(String date, String time)
{
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);

  display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 20, date);
  display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 10, time);
}

void aqiPrint(int us, int cn)
{
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);

  String usStr = "US AQI= ";
  usStr.concat(String(us));
  String cnStr = "CN AQI= ";
  cnStr.concat(String(cn));

  display.drawString(display.getWidth() / 2, display.getHeight() / 2 + 15, usStr);
  display.drawString(display.getWidth() / 2, display.getHeight() / 2 + 5, cnStr);

  return;
}

void refreshPrint()
{
  display.clear();
  display.setFont(Lato_Regular_11);

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 5, "REFRESHING...");

  drawMargin();

  display.display();
  delay(300);
  return;
}

void netPrint(int st)
{
  display.clear();
  display.setFont(Lato_Regular_11);

  if (st)
  {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 15, "NETWORK");
    display.drawString(display.getWidth() / 2, display.getHeight() / 2, "CONNECTED");
  }
  else
  {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 15, "CONNECTION");
    display.drawString(display.getWidth() / 2, display.getHeight() / 2, "LOST");
  }

  drawMargin();

  display.display();
  return;
}

// void timer()
// {
//   display.clear();
//   display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
//   display.drawString(20, 20, String(millis()));
//   // display.display();
// }

void setup()
{

  Serial.begin(9600);

  pinMode(itrPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(itrPin), handleITR, FALLING);

  display.init();
  display.flipScreenVertically();

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PW);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("Connected to WLAN. ");
  netPrint(1);
  delay(3000); 
}

void loop()
{

  if (WiFi.status() != WL_CONNECTED)
  {
    netPrint(0);
  }

  if (itrCtl || timerFlag || fetchErr)
  {
    fetchErr = 0;

    WiFiClient client;
    HTTPClient http;
    http.useHTTP10(1);

    refreshPrint();

    http.begin(client, avURL);

    Serial.println();
    Serial.println();
    Serial.println("HTTP began");

    int httpCode = http.GET();
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK)
    {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, http.getStream());

      const char *status = doc["status"]; // "success"

      JsonObject data = doc["data"];

      JsonObject data_current_pollution = data["current"]["pollution"];
      int aqius = data_current_pollution["aqius"]; // 70
      int aqicn = data_current_pollution["aqicn"]; // 72

      Serial.print("status=");
      Serial.println(status);
      Serial.print("AQI_US=");
      Serial.println(String(aqius));
      Serial.print("AQI_CN=");
      Serial.println(aqicn);

      if (aqius > 99 || aqicn > 99)
        alert = 1;

      display.clear();
      aqiPrint(aqius, aqicn);

      http.end();
    }
    else
      fetchErr = 1;

    http.begin(client, timeURL);
    Serial.println("Fetching Time");

    httpCode = http.GET();
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK)
    {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, http.getStream());

      const char *datetime = doc["datetime"];
      String date, time;

      for (int i = 0; i < 10; i++)
      {
        date += datetime[i];
      }

      for (int i = 0; i < 8; i++)
      {
        time += datetime[11 + i];
      }

      timePrint(date, time);
    }
    else
      fetchErr = 1;

    if (alert)
      display.invertDisplay();

    drawMargin();
    display.display();

    Serial.println("ITR");
    itrCtl = 0;
    timerFlag = 0;
    debugCt++;
  }

  if (millis() - lastMillis >= timerIntv)
  {
    lastMillis = millis() - (millis() % timerIntv);
    Serial.print("millis: ");
    Serial.println(millis());
    Serial.print("lastMillis: ");
    Serial.println(lastMillis);
    timerFlag = 1;
  }


  // timer();
  display.display();
  delay(50);
}
