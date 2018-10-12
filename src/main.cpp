/**
 * RF Wall Letters Firmware
 * By Krisztian Kurucz
 * 
 * 12/10/2018
 */

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <LowPower.h>
#include <printf.h>
#include <string.h>

// Method declarations
void check_radio();
void check_button();
void set_light(bool set_light_on);
void set_light_all(bool B, bool A, bool R);
void update_light_status();
void set_write_target(int node);
void send_command(int node, int cmd);
void process_command(char *cmd);

// Hardware configuration
RF24 radio(7, 8);                          // Set up nRF24L01 radio on SPI bus plus pins 7 & 8

// Node 1 = B
// Node 2 = A
// Node 3 = R                   
int node = 1;

// Light Mode 0 = Booting up
// Light Mode 1 = All flash at once
// Light Mode 2 = Flash one at a time
// Light Mode 3 = Flash one at a time, staying on
volatile int light_mode = 0;
volatile int last_light_mode = 0;

// Status boolean to keep track of node's light being on / off
bool light_on = false;
// Slaves use this to determine when they need to turn their light on / off
volatile bool requested_light_on = true;

// Keeps the status of each light for master
bool light_status[] = { false, false, false };
// Used in different programs to keep track of which light is on
int light_index = 0;
int delay_adjust = 0;

volatile long last_time_btn_pressed = 0;
volatile bool button_pressed = false;

// Pin definitions
int pin_button = 3;
int pin_light_on = 4;
int pin_light_off = 5;

char cmd_on[32] = "ON";
char cmd_off[32] = "OFF";
char cmd_test[32] = "TEST";

// Demonstrates another method of setting up the addresses
byte address[3][6] = { "00001", "00002", "00003" };

// Set up role.  This sketch uses the same software for all the nodes.
typedef enum { role_sender = 1, role_receiver = 2 } role_e; // The various roles supported by this sketch
const char* role_friendly_name[] = { "invalid", "Master", "Slave"}; // The debug-friendly names of those roles
role_e role; // The role of the current running sketch

static uint32_t message_count = 0;

/********************** Setup *********************/
void setup(){
    // Establish role based on node number    
    if (node == 1)
        role = role_sender;
    else
        role = role_receiver;

    // Set up light pins
    pinMode(pin_light_on, OUTPUT);
    pinMode(pin_light_off, OUTPUT);

    // Set up button pin on master
    if (node == 1)
        pinMode(pin_button, INPUT_PULLUP);

    Serial.begin(115200);
    printf_begin();
    Serial.print(F("\nROLE: "));
    Serial.println(role_friendly_name[role]);

    // Setup and configure rf radio
    radio.begin();  
    radio.setPALevel(RF24_PA_MIN);
    radio.enableAckPayload();
    radio.enableDynamicPayloads();
                                                    
    if (role == role_sender) {
        radio.openWritingPipe(address[1]);
        radio.openReadingPipe(1, address[0]);
        radio.stopListening();
    } else {
        if (node == 2) {
            radio.openWritingPipe(address[0]);
            radio.openReadingPipe(1, address[1]);
        } else if (node == 3) {
            radio.openWritingPipe(address[0]);
            radio.openReadingPipe(1, address[2]);
        }
        radio.startListening();
        radio.writeAckPayload(1, &message_count, sizeof(message_count));
        ++message_count;
    }

    radio.printDetails(); // Dump the configuration of the rf unit for debugging
    delay(50);

    // Set up interrupts
    attachInterrupt(0, check_radio, LOW); // Attach interrupt handler to interrupt #0 (using pin 2) on BOTH the sender and receiver
    if (node == 1) {
        attachInterrupt(1, check_button, LOW);
    }
}

/********************** Main Loop *********************/
void loop() {
    if (role == role_sender)  {

        if (button_pressed) {
            if ((millis() - last_time_btn_pressed) > 500){
                last_light_mode = light_mode;
                light_mode = light_mode + 1;                    
            }
            button_pressed = false;
            attachInterrupt(1, check_button, LOW);
        }

        if (light_mode > 4)
            light_mode = 0;

        Serial.print("[Master] Light Mode: ");
        Serial.println(light_mode);
        delay_adjust = 0;

        // Light mode transition logic
        if (last_light_mode != light_mode)
        {
            set_light_all(true, true, true);
            last_light_mode = light_mode;
            light_index = 0;
        } else {
            // Default on when first powered
            if (light_mode == 0 ) {
                set_light_all(true, true, true);
            // Flash all on and off
            } else if (light_mode == 1) {
                // Reusing requested_light_on here for master
                if (requested_light_on) {
                    set_light_all(true, true, true);
                } else { 
                    set_light_all(false, false, false);
                }
                requested_light_on = !requested_light_on;
            // Turn on one at a time
            } else if (light_mode == 2) {
                if (light_index == 0) set_light_all(false, false, false);
                else if (light_index == 1) set_light_all(true, false, false);
                else if (light_index == 2) set_light_all(false, true, false);
                else if (light_index == 3) set_light_all(false, false, true);
                light_index = light_index + 1;
                if (light_index > 3) {
                    light_index = 0;
                }
            } else if (light_mode == 3) {
                if (light_index == 0) set_light_all(false, false, false);
                else if (light_index == 1) set_light_all(true, false, false);
                else if (light_index == 2) set_light_all(true, true, false);
                else if (light_index == 3) {
                    set_light_all(true, true, true);
                    delay_adjust = 1000;
                }
                light_index = light_index + 1;
                if (light_index > 3) {
                    light_index = 0;
                }                
            } else if (light_mode == 4) {
                if (light_index == 0) set_light_all(false, false, false);
                else if (light_index == 1) {
                    set_light_all(true, true, true);
                    delay_adjust = -650;
                } else if (light_index == 2) {
                    set_light_all(false, false, false);
                    delay_adjust = -650;
                } else if (light_index == 3) {
                    set_light_all(true, true, true);
                    delay_adjust = -650;
                } else if (light_index == 4) {
                    set_light_all(false, false, false);
                    delay_adjust = 500;
                }
                light_index = light_index + 1;
                if (light_index > 4) {
                    light_index = 0;
                }     
            }
        }
        delay(1000 + delay_adjust);
    }

    if(role == role_receiver) {
        update_light_status();
        //delay(200);
    }
}

void set_light(bool set_light_on)
{
    if (set_light_on) {
        digitalWrite(pin_light_on, HIGH);
        delay(20);
        digitalWrite(pin_light_on, LOW);
        light_on = true;
        Serial.println("Light turned ON");
    } else {
        digitalWrite(pin_light_off, HIGH);
        delay(20);
        digitalWrite(pin_light_off, LOW);
        light_on = false;
        Serial.println("Light turned OFF");
    }
    light_status[node - 1] = light_on;
}

void set_write_target(int node)
{
    //radio.startListening();
    radio.openWritingPipe(address[node]);
    //radio.stopListening();
}
// Commands:
// 1 - ON
// 0 - OFF
// 2 - TEST
void send_command(int node, int cmd)
{
    //set_write_target(node);
    if (cmd == 1) {
        Serial.print(F("[Master] Sending command: "));
        Serial.println(cmd_on);
        radio.startWrite(&cmd_on, 32, 0);
        light_status[node - 1] = true;
    } else if (cmd == 0) {
        Serial.print(F("[Master] Sending command: "));
        Serial.println(cmd_off);
        radio.startWrite(&cmd_off, 32, 0);
        light_status[node - 1] = false;
    } else if (cmd == 2) {
        Serial.print(F("[Master] Sending command: "));
        Serial.println(cmd_test);
        radio.startWrite(&cmd_test, 32, 0);
    }
}

void set_light_all(bool B, bool A, bool R)
{
    if (light_status[0] != B)
        set_light(B);
    if (light_status[1] != A)
        send_command(2, (int)A);
    //if (light_status[2] != R)
        //send_command(3, (int)R);
}

void process_command(char *cmd)
{
    Serial.print("[Slave] Processing command: ");
    Serial.println(cmd);
    if (strcmp(cmd, cmd_on) == 0) {
        Serial.println("[Slave] Setting light request = true");
        requested_light_on = true;
        //set_light(true);
    } else if (strcmp(cmd, cmd_off) == 0) {
        Serial.println("[Slave] Setting light request = false");
        requested_light_on = false;
        //set_light(false);
    } else {
        Serial.println("[Slave] Invalid command!");
    }
}

void update_light_status()
{
    if (light_on != requested_light_on) {
        set_light(requested_light_on);
    }
}

/********************** Interrupts *********************/
void check_radio()
{
    bool tx, fail, rx;
    radio.whatHappened(tx, fail, rx); 

    if (tx) { // Have we successfully transmitted?
        if (role == role_sender) { Serial.println(F("[Master] Send OK")); }
        if (role == role_receiver) { Serial.println(F("[Slave] Ack Payload Sent")); }
    }

    if (fail) { // Have we failed to transmit?
        if (role == role_sender) { Serial.println(F("[Master] Send Failed")); }
        if (role == role_receiver) { Serial.println(F("[Slave] Ack Payload Failed")); }
    }

    if (rx || radio.available()) { // Did we receive a message?

        if (role == role_sender) { // If we're the sender, we've received an ack payload
            radio.read(&message_count, sizeof(message_count));
            Serial.print(F("[Master] Ack: "));
            Serial.println(message_count);
        }

        if (role == role_receiver) {
            static char cmd[32];
            radio.read(&cmd, sizeof(cmd));
            Serial.print(F("[Slave] Received: "));
            Serial.println(cmd);
            radio.writeAckPayload(1, &node, sizeof(node));
            ++message_count;
            process_command(cmd);
        }
    }
}

void check_button()
{
    detachInterrupt(1);
    button_pressed = true;
    last_time_btn_pressed = millis();
}