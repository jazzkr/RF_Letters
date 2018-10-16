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

// Hardware configuration of RF radio
RF24 radio(7, 8);

// Node 1 = B
// Node 2 = A
// Node 3 = R                   
int node = 3;
bool PDN_ENABLED = true;
bool DEBUG = false;

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

// Light program variables
int light_index = 0;

// Button variables
volatile long last_time_btn_pressed = 0;
volatile bool button_pressed = false;
int loops_since_last_press = 0;

// Sleep variables
unsigned long sleep_start_time = 0;
unsigned long sleep_current_time = 0;
unsigned long loop_delay = 0;
unsigned long delay_adjust = 0;

// Pin definitions
int pin_button = 3;
int pin_light_on = 4;
int pin_light_off = 5;

// Commands
char cmd_on[32] = "ON";
char cmd_off[32] = "OFF";
char cmd_test[32] = "TEST";

// Address set-up
byte address[3][6] = { "00001", "00002", "00003" };
byte dummy_addr[6] = {"00000"};
// Set up role.  This sketch uses the same software for all the nodes.
// The various roles supported by this sketch
typedef enum { role_sender = 1, role_receiver = 2 } role_e;
// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Master", "Slave"};
role_e role; // The role of the current running sketch

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

    if (DEBUG) {
        Serial.begin(115200);
        printf_begin();
        Serial.print(F("\nROLE: "));
        Serial.println(role_friendly_name[role]);
    }

    // Setup and configure rf radio
    radio.begin();  
    radio.setPALevel(RF24_PA_LOW);
    radio.setAutoAck(true);
    //radio.enableDynamicAck();
    //radio.enableDynamicPayloads();
                                                    
    if (role == role_sender) {
        radio.openWritingPipe(address[1]);
        radio.openReadingPipe(0, dummy_addr);
        radio.openReadingPipe(1, address[0]);
        radio.stopListening();
    } else {
        if (node == 2) {
            radio.openWritingPipe(address[0]);
            radio.openReadingPipe(0, dummy_addr);
            radio.openReadingPipe(1, address[1]);
        } else if (node == 3) {
            radio.openWritingPipe(address[0]);
            radio.openReadingPipe(0, dummy_addr);
            radio.openReadingPipe(1, address[2]);
        }
        radio.startListening();
        radio.writeAckPayload(1, &node, sizeof(node));
    }

    radio.printDetails(); // Dump the configuration of the rf unit for debugging
    delay(50);

    // Set up interrupts
    if (node == 1) {
        attachInterrupt(1, check_button, LOW);
    } else {
        attachInterrupt(0, check_radio, LOW);
    }
}

/********************** Main Loop *********************/
void loop() {
    if (role == role_sender)  {

        if (button_pressed && loops_since_last_press > 3)
        {
            loops_since_last_press = 0;
            last_light_mode = light_mode;
            light_mode = light_mode + 1;                    
            button_pressed = false;
            attachInterrupt(1, check_button, LOW);
        } else {
            loops_since_last_press += 1;
        }
        if (light_mode > 4)
            light_mode = 0;

        delay_adjust = 0;

        // Light mode transition logic
        if (last_light_mode != light_mode)
        {
            set_light_all(true, true, true);
            last_light_mode = light_mode;
            light_index = 0;
            if (DEBUG) {
                Serial.print("[Master] Light Mode: ");
                Serial.println(light_mode);
            }
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
            // Turn and keep on one at a time
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
            // Flash all quickly        
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
        loop_delay = 1000 + delay_adjust;
        if (PDN_ENABLED) {
            sleep_start_time = millis();
            sleep_current_time = sleep_start_time;
            while ((sleep_current_time - sleep_start_time) < loop_delay)
            {
                if (DEBUG) {
                    Serial.flush();
                }
                LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
                sleep_current_time += 250;
            }
        } else {
            delay(loop_delay);
        }
    }

    if(role == role_receiver) {
        update_light_status();
        attachInterrupt(0, check_radio, LOW);
        if (PDN_ENABLED) {
            if (DEBUG) {
                Serial.println("Sleeping forever!");
                Serial.flush();
            }
            LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
            detachInterrupt(0);
        } else {
            // Do nothing, loop constantly
        }
    }
}

void set_light(bool set_light_on)
{
    if (set_light_on) {
        digitalWrite(pin_light_on, HIGH);
        delay(20);
        digitalWrite(pin_light_on, LOW);
        light_on = true;
        if (DEBUG) {
            Serial.println("Light turned ON");
        }
    } else {
        digitalWrite(pin_light_off, HIGH);
        delay(20);
        digitalWrite(pin_light_off, LOW);
        light_on = false;
        if (DEBUG) {
            Serial.println("Light turned OFF");
        }
    }
    light_status[node - 1] = light_on;
}

void set_write_target(int node)
{
    //radio.startListening();
    //radio.flush_tx();
    radio.openWritingPipe(address[node-1]);
    radio.stopListening();
}
// Commands:
// 1 - ON
// 0 - OFF
// 2 - TEST
void send_command(int node, int cmd)
{
    set_write_target(node);
    Serial.print("[Master] Targetting node ");
    Serial.println(node);
    if (cmd == 1) {
        if (DEBUG) {
            Serial.print(F("[Master] Sending command: "));
            Serial.println(cmd_on);
        }
        bool result = radio.write(&cmd_on, 32);
        //bool result = radio.txStandBy(500);
        if (result || true)
            light_status[node - 1] = true;
    } else if (cmd == 0) {
        if (DEBUG) {
            Serial.print(F("[Master] Sending command: "));
            Serial.println(cmd_off);
        }
        bool result = radio.write(&cmd_off, 32);
        //bool result = radio.txStandBy(500);
        if (result || true)
            light_status[node - 1] = false;
    } else if (cmd == 2) {
        if (DEBUG) {
            Serial.print(F("[Master] Sending command: "));
            Serial.println(cmd_test);
        }
        radio.write(&cmd_test, 32);
        //radio.txStandBy(500);
    }
}

void set_light_all(bool B, bool A, bool R)
{
    if (light_status[0] != B)
        set_light(B);
    if (light_status[1] != A)
        send_command(2, (int)A);
    if (light_status[2] != R)
        send_command(3, (int)R);
}

void process_command(char *cmd)
{
    if (DEBUG) {
        Serial.print("[Slave] Processing command: ");
        Serial.println(cmd);
    }
    if (strcmp(cmd, cmd_on) == 0) {
        if (DEBUG) {
            Serial.println("[Slave] Setting light request = true");
        }
        requested_light_on = true;
        //set_light(true);
    } else if (strcmp(cmd, cmd_off) == 0) {
        if (DEBUG) {
            Serial.println("[Slave] Setting light request = false");
        }
        requested_light_on = false;
        //set_light(false);
    } else {
        if (DEBUG) {
            Serial.println("[Slave] Invalid command!");
        }
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

    if (DEBUG) {
        if (tx) { // Have we successfully transmitted?
            if (role == role_sender) { Serial.println(F("[Master] Send OK")); }
            if (role == role_receiver) { Serial.println(F("[Slave] Ack Payload Sent")); }
        }

        if (fail) { // Have we failed to transmit?
            if (role == role_sender) { Serial.println(F("[Master] Send Failed")); }
            if (role == role_receiver) { Serial.println(F("[Slave] Ack Payload Failed")); }
        }
    }

    if (rx || radio.available()) { // Did we receive a message?

        if (role == role_receiver) {
            static char cmd[32];
            radio.read(&cmd, sizeof(cmd));
            if (DEBUG) {
                Serial.print(F("[Slave] Received: "));
                Serial.println(cmd);
            }
            radio.writeAckPayload(1, &node, sizeof(node));
            process_command(cmd);
        }
    }
}

void check_button()
{
    detachInterrupt(1);
    button_pressed = true;
}