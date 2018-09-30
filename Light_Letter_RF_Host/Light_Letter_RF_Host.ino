/*
* Arduino Wireless Communication Tutorial
*     Example 1 - Transmitter Code
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
  radio.begin();
  radio.openWritingPipe(address_B);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();
}
void loop() {
  const char text1[] = "A";
  radio.write(&text1, sizeof(text1));
  delay(1000);
  const char text2[] = "B";
  radio.write(&text2, sizeof(text2));
  delay(1000);
  const char text3[] = "C";
  radio.write(&text3, sizeof(text3));
  delay(1000);
  const char text4[] = "D";
  radio.write(&text4, sizeof(text4));
  delay(1000);
  const char text5[] = "E";
  radio.write(&text5, sizeof(text5));
  delay(1000);
  const char text6[] = "F";
  radio.write(&text6, sizeof(text6));
  delay(1000);
  const char text7[] = "G";
  radio.write(&text7, sizeof(text7));
  delay(1000);
  const char text8[] = "H";
  radio.write(&text8, sizeof(text8));
  delay(1000);
}
