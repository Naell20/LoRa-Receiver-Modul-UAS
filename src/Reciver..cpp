#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <time.h>

#define BLYNK_TEMPLATE_ID "TMPL6kXi0v1Dd"
#define BLYNK_TEMPLATE_NAME "Monitoring ELC"
#define BLYNK_PRINT Serial

char auth[] = "nANAFvvACP0bm9-CALdfZBF0RzR1xr95";
char ssid[] = "pudding coklat";
char pass[] = "12233445";

#include <BlynkSimpleEsp32.h>

const int csPin = 25;
const int resetPin = 26;
const int DIO0 = 34;
const int ledPin = 2;

const unsigned long batasTimeout = 5000;
const unsigned long intervalRecovery = 2000;
const unsigned long intervalBlynkRetry = 20000;
const unsigned long intervalNtpRetry = 30000;

unsigned long lastRecoveryAttempt = 0;
unsigned long lastPacketReceivedAt = 0;
unsigned long lastWaitingMessageAt = 0;
unsigned long lastBlynkTry = 0;
unsigned long lastNtpAttempt = 0;

bool loraReady = false;
bool hasReceivedPacket = false;
bool rtcOk = false;
bool rtcSynced = false;
bool ntpAttempted = false;

RTC_DS3231 rtc;

const char* namaBulan[] = {
  "Januari", "Februari", "Maret", "April", "Mei", "Juni",
  "Juli", "Agustus", "September", "Oktober", "November", "Desember"
};


bool initializeLoRa() {
  LoRa.setPins(csPin, resetPin, DIO0);
  if (!LoRa.begin(433E6)) {
    return false;
  }

  LoRa.setSyncWord(0x11F3);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setPreambleLength(8);
  LoRa.setTxPower(17);
  LoRa.enableCrc();
  LoRa.receive();
  return true;
}

bool recoverLoRa() {
  LoRa.end();
  delay(10);
  return initializeLoRa();
}

void updateLedStatus() {
  bool paketBaruDiterima = hasReceivedPacket &&
                          millis() - lastPacketReceivedAt < batasTimeout;

  digitalWrite(ledPin, paketBaruDiterima ? HIGH : LOW);
}

bool sendAck() {
  if (!LoRa.beginPacket()) {
    return false;
  }

  LoRa.print("ACK_GEN1");
  if (!LoRa.endPacket()) {
    return false;
  }
  delay (5);

  LoRa.receive();
  return true;
}

String buatTimestamp() {
  if (!rtcOk) {
    return "RTC tidak terdeteksi";
  }

  DateTime now = rtc.now();
  String bulanIndo = namaBulan[now.month() - 1];
  return String(now.day()) + " " + bulanIndo + " " + String(now.year()) + " | " +
        (now.hour() < 10 ? "0" : "") + String(now.hour()) + ":" +
        (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" +
        (now.second() < 10 ? "0" : "") + String(now.second());
}

void synchronizeRtcFromNtp() {
  if (!rtcOk || rtcSynced || WiFi.status() != WL_CONNECTED ||
      (ntpAttempted && millis() - lastNtpAttempt < intervalNtpRetry)) {
    return;
  }

  ntpAttempted = true;
  lastNtpAttempt = millis();
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 1000)) {
    Serial.println("Gagal Sinkronisasi Waktu, Mencoba Kembali ");
    return;
  }

  rtc.adjust(DateTime(
      timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
      timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec));
  rtcSynced = true;
  Serial.println("Waktu RTC berhasil disinkronkan dari NTP (WIB/UTC+7)");
}

void handleBlynk() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (Blynk.connected()) {
    Blynk.run();
  } else if (millis() - lastBlynkTry > intervalBlynkRetry) {
    lastBlynkTry = millis();
    Blynk.connect(1000);
  }
}

void setup() {
  Serial.begin(115200);
  SPI.begin(18, 19, 23, 25);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  delay(1000);

  Serial.println("LoRa Receiver Module Setup...");

  // wifi & blynk
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, pass);
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Blynk.config(auth);

  Wire.begin(22, 21);
  rtcOk = rtc.begin();
  if (!rtcOk) {
    Serial.println("RTC tidak terdeteksi");
  } else if (rtc.lostPower()) {
    Serial.println("Baterai RTC lepas, mengatur ulang waktu...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  while (!initializeLoRa()) {
    Serial.println("LoRa gagal diinisialisasi, mencoba kembali dalam 1 detik...");
    delay(intervalRecovery);
  }

  loraReady = true;
  lastWaitingMessageAt = millis();
  Serial.println("LoRa Receiver Ready");
}

void loop() {
  synchronizeRtcFromNtp();
  handleBlynk();
  updateLedStatus();

  if (!loraReady) {
    if (millis() - lastRecoveryAttempt >= intervalRecovery) {
      lastRecoveryAttempt = millis();
      Serial.println("Mencoba menginisialisasi ulang LoRa...");
      loraReady = recoverLoRa();

      if (loraReady) {
        lastWaitingMessageAt = millis();
        Serial.println("LoRa berhasil diinisialisasi ulang");
      } else {
        Serial.println("Inisialisasi LoRa gagal, mencoba kembali");
      }
    }
    return;
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incomingData = "";
    while (LoRa.available()) {
      incomingData += (char)LoRa.read();
    }
    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    if (!incomingData.startsWith("GEN_TX1,GEN_RX1,")) {
      Serial.print("Paket LoRa diabaikan (header tidak cocok): ");
      Serial.println(incomingData);
      LoRa.receive();
      return;
    }

    int koma0 = incomingData.indexOf(',');
    int koma1 = incomingData.indexOf(',', koma0 + 1);
    int koma2 = incomingData.indexOf(',', koma1 + 1);
    int koma3 = incomingData.indexOf(',', koma2 + 1);
    int koma4 = incomingData.indexOf(',', koma3 + 1);

    if (koma0 == -1 || koma1 == -1 || koma2 == -1 ||
        koma3 == -1 || koma4 == -1) {
      Serial.println("Peringatan: Format data tidak valid!");
      LoRa.receive();
      return;
    }

    lastPacketReceivedAt = millis();
    hasReceivedPacket = true;
    lastWaitingMessageAt = lastPacketReceivedAt;

    if (!sendAck()) {
      Serial.println("Gagal mengirim data, memulai recovery LoRa...");
      loraReady = recoverLoRa();
      if (!loraReady) {
        Serial.println("Recovery LoRa gagal, akan mencoba kembali");
        return;
      }
      Serial.println("Recovery LoRa berhasil");
    }

    Serial.print("Data Header Diterima: ");
    Serial.println(incomingData);
    Serial.printf("RSSI: %d dBm, SNR: %.1f dB\n", rssi, snr);

    float vGen = incomingData.substring(koma1 + 1, koma2).toFloat();
    float iGen = incomingData.substring(koma2 + 1, koma3).toFloat();
    float vOut = incomingData.substring(koma3 + 1, koma4).toFloat();
    float iOut = incomingData.substring(koma4 + 1).toFloat();

    String timestamp = buatTimestamp();

    Serial.print("[Waktu] ");
    Serial.println(timestamp);
    Serial.print("  V_BAT: ");
    Serial.print(vGen);
    Serial.println(" V DC");
    Serial.print("  I_BAT: ");
    Serial.print(iGen);
    Serial.println(" A DC");
    Serial.print("  V_AC: ");
    Serial.print(vOut);
    Serial.println(" V");
    Serial.print("  I_AC: ");
    Serial.print(iOut);
    Serial.println(" A");

    if (Blynk.connected()) {
      Blynk.virtualWrite(V0, vGen);
      Blynk.virtualWrite(V1, iGen);
      Blynk.virtualWrite(V2, vOut);
      Blynk.virtualWrite(V3, iOut);
      Blynk.virtualWrite(V4, timestamp);
    }
  } else {
    if (millis() - lastWaitingMessageAt >= batasTimeout) {
      lastWaitingMessageAt = millis();
      Serial.println("Menunggu paket LoRa...");
    }
  }
}