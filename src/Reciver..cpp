#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define BLYNK_TEMPLATE_ID "TMPL6kXi0v1Dd"
#define BLYNK_TEMPLATE_NAME "Monitoring ELC"
#define BLYNK_PRINT Serial

char auth[] = "nANAFvvACP0bm9-CALdfZBF0RzR1xr95";
char ssid[] = "StarLink 5G";
char pass[] = "20012008";

#include <BlynkSimpleEsp32.h>

const int csPin = 5;
const int resetPin = 14;
const int DIO0 = 34;

RTC_DS3231 rtc;

const char* namaBulan[] = {
  "Januari", "Februari", "Maret", "April", "Mei", "Juni",
  "Juli", "Agustus", "September", "Oktober", "November", "Desember"
};

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  delay(1000);

  Serial.println("LoRa Receiver Module Setup...");

  Blynk.begin(auth, ssid, pass);

  Wire.begin(22, 21);
  if (!rtc.begin()) {
    Serial.println("RTC ne matot");
  } else {
    if (rtc.lostPower()) {
      Serial.println("Batre Lepas woe, atur ulng wktu...");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  LoRa.setPins(csPin, resetPin, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Gagal Terhubung");
    while (1) {
      ;
    }
  }

  LoRa.setSyncWord(0x34);
  Serial.println("LoRa Receiver Ready");
}

void loop() {
  Blynk.run();

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incomingData = "";
    while (LoRa.available()) {
      incomingData += (char)LoRa.read();
    }

    int koma1 = incomingData.indexOf(',');
    int koma2 = incomingData.indexOf(',', koma1 + 1);
    int koma3 = incomingData.indexOf(',', koma2 + 1);

    String timestamp = "RTC ne ra urip";
    if (rtc.begin()) {
      DateTime now = rtc.now();
      String bulanIndo = namaBulan[now.month() - 1];
      timestamp = String(now.day()) + " " + bulanIndo + " " + String(now.year()) + " | " +
                  (now.hour() < 10 ? "0" : "") + String(now.hour()) + ":" +
                  (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" +
                  (now.second() < 10 ? "0" : "") + String(now.second());
    }

    if (koma1 != -1 && koma2 != -1 && koma3 != -1) {
      float vGen = incomingData.substring(0, koma1).toFloat();
      float iGen = incomingData.substring(koma1 + 1, koma2).toFloat();
      float vOut = incomingData.substring(koma2 + 1, koma3).toFloat();
      float iOut = incomingData.substring(koma3 + 1).toFloat();

      Serial.print("[Waktu] ");
      Serial.println(timestamp);
      Serial.print("  V_Generator: ");
      Serial.print(vGen);
      Serial.println(" V");
      Serial.print("  I_Generator: ");
      Serial.print(iGen);
      Serial.println(" A");
      Serial.print("  V_Output: ");
      Serial.print(vOut);
      Serial.println(" V");
      Serial.print("  I_Output: ");
      Serial.print(iOut);
      Serial.println(" A");

      Blynk.virtualWrite(V0, vGen);
      Blynk.virtualWrite(V1, iGen);
      Blynk.virtualWrite(V2, vOut);
      Blynk.virtualWrite(V3, iOut);
      Blynk.virtualWrite(V4, timestamp);

      Serial.print("Data Diterima :");
      Serial.println(incomingData);
    } else {
      Serial.println("Peringatan: Format data tidak valid!");
    }
  }
}