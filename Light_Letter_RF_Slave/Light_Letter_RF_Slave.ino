/*
* Arduino Wireless Communication Tutorial
*       Example 1 - Receiver Code
*                
* by Dejan Nedelkovski, www.HowToMechatronics.com
* 
* Library: TMRh20/RF24, https://github.com/tmrh20/RF24/
*/
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
RF24 radio(7, 8); // CE, CSN
const byte address_A[6] = "00001";
const byte address_B[6] = "00002";
const byte address_C[6] = "00003";
void setup() {
  Serial.begin(4800); //9600
  radio.begin();
  radio.openReadingPipe(0, address_B);
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening();
}
void loop() {
  if (radio.available()) {
    char cmd[32] = "";
    radio.read(&cmd, sizeof(cmd));
    Serial.println(cmd);
  }
  delay(5
  00);
}
