// #include <Arduino.h>
// #include <SPI.h>
// #include <LoRa.h>

// #define ss
// #define rst
// #define dio0

// int counter = 0;

// void setup() {
//   Serial.begin(115200);
//   while(!Serial);
//   Serial.println("LoRa Sender");

//   LoRa.setPins(ss, rst, dio0);

//   while (!LoRa.begin(433E6)){
//     Serial.println(".");
//     delay(500);
//   }

//   LoRa.setSyncWord(0xF3);
//   Serial.println("LoRa Initializing OK!");
// }

// void loop() {
//   Serial.print("Sending packet: ");
//   Serial.println(counter);

//   LoRa.beginPacket();
//   LoRa.print("hello ");
//   LoRa.print(counter);
//   LoRa.endPacket();

//   counter++;
//   delay(1000);
// }

