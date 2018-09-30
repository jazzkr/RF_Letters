#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "LowPower.h"

// Node number mapping
// B - [0]
// A - [1]
// R - [2]

RF24 radio(7, 8); // CE, CSN
const byte node_addresses[][6] = {"NodeB", "NodeA", "NodeR"};

const int radioIntPin = 2;
const int btnIntPin = 3;
const int setPin = 4;
const int unsetPin = 5;

// Constants to determine what letter we're dealing with
const bool host = true;
const int node_identity = 0;

// Booleans to keep track of what lights are on for host use
static bool light_status[] = {false, false, false};

// Light mode for host to determine how to control slaves and itself
static int light_mode = 0;

void setup() {
  // Set up pin modes
  pinMode(radioIntPin, INPUT);
  pinMode(setPin, OUTPUT);
  pinMode(unsetPin, OUTPUT);

  // Toggle relay to off position to start always
  control_lights(false);
  
  Serial.begin(4800); //9600
  radio.begin();
  radio.setPALevel(RF24_PA_MIN);
  radio.setAutoAck(true);
  
  if (host) {
    attachInterrupt(digitalPinToInterrupt(btnIntPin), check_radio, LOW);
  } else {
    slave_set_read_target();
  }
  radio.startListening();
  
  attachInterrupt(digitalPinToInterrupt(radioIntPin), check_btn, HIGH);
}

void loop() {
  // Host executes various light modes
  if (host) {
    if (light_mode > 3) light_mode = 0;
    
    if (light_mode == 0) { // All off
      control_lights(false);
      light_status[0] = false;
      host_send_cmd(1, "OFF");
      light_status[1] = false;
      host_send_cmd(2, "OFF");
      light_status[2] = false;
      
    } else if (light_mode == 1) { // All on, solid
      control_lights(true);
      light_status[0] = true;
      host_send_cmd(1, "ON");
      light_status[1] = true;
      host_send_cmd(2, "ON");
      light_status[2] = true;
      
    } else if (light_mode == 2) { // Synchronized flashing
      
    } else if (light_mode == 3) { // One letter on at a time
      
    }
    // Host radio enter low power mode
    radio.powerDown();
    // Hosts sleep for a second before re-evaluating light controls
    LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
  } else {
    // Slaves can sleep until radio communication from host
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  }
}

void check_radio(void) {
  bool tx,fail,rx;
  radio.whatHappened(tx,fail,rx);
  
  if (tx) { // Have we successfully transmitted?
      if (host) { 
        Serial.println(F("Host: Send OK"));
      } else {
        Serial.println(F("Slave: Ack Sent"));
      }
  }
  
  if (fail) { // Have we failed to transmit?
      if (host) {
        Serial.println(F("Host: Send Failed"));
      } else {
        Serial.println(F("Slave: Ack Failed"));
      }
  }
  
  if (rx || radio.available()) {
    if (host) {
      Serial.println(F("Host: Ack Received"));
    } else {
      char cmd[32] = "";
      radio.read(&cmd, sizeof(cmd));
      Serial.print(F("Slave: Got command "));
      Serial.println(cmd);
    }
  }
}

void check_btn(void) {
  light_mode = light_mode + 1;
}

void control_lights(bool state) {
  if (state == true) {
    digitalWrite(setPin, HIGH);
    delay(50);
    digitalWrite(setPin, LOW);
  } else {
    digitalWrite(unsetPin, HIGH);
    delay(50);
    digitalWrite(unsetPin, LOW);
  }
}

void host_set_write_target(int node_num) {
  radio.openWritingPipe(node_addresses[node_num]); // Write to target address
  radio.openReadingPipe(0, node_addresses[node_identity]); // Read on own address
}

void slave_set_read_target() {
  radio.openWritingPipe(node_addresses[0]); // Write to host
  radio.openReadingPipe(0, node_addresses[node_identity]); // Read on own address
}

void host_send_cmd(int node, char cmd[32]) {
  host_set_write_target(node);
  radio.stopListening();
  radio.write(&cmd, sizeof(cmd));
  Serial.print(F("Host: Wrote out "));
  Serial.println(cmd);
}

