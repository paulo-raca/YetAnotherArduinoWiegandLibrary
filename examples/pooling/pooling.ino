/*
 * Example on how to use the Wiegand reader library with interruptions.
 */

#include <Wiegand.h>

// These are the pins connected to the Wiegand D0 and D1 signals.
#define PIN_D0 18
#define PIN_D1 19

// The object that handles the wiegand protocol
Wiegand wiegand;

// Initialize Wiegand reader
void setup() {
  Serial.begin(9600);

  //Install listeners and initialize Wiegand reader
  wiegand.onReceive(receivedData, "Card readed: ");
  wiegand.onStateChange(stateChanged, "State changed: ");
  wiegand.begin(WIEGAND_LENGTH_AUTO);

  //initialize pins as INPUT
  pinMode(PIN_D0, INPUT);
  pinMode(PIN_D1, INPUT);
}

// Continuously checks for pending messages and pools updates from the wiegand inputs
void loop() {
  // Checks for pending messages 
  wiegand.flush();
  
  // Check for changes on the the wiegand input pins
  wiegand.setPin0State(digitalRead(PIN_D0));
  wiegand.setPin1State(digitalRead(PIN_D1));
}

// Notifies when a reader has been connected or disconnected.
// Instead of a message, the seconds parameter can be anything you want -- Whatever you specify on `wiegand.onStateChange()`
void stateChanged(bool plugged, const char* message) {
    Serial.print(message);
    Serial.println(plugged ? "CONNECTED" : "DISCONNECTED");
}

// Notifies when a card was read.
// Instead of a message, the seconds parameter can be anything you want -- Whatever you specify on `wiegand.onReceive()`
void receivedData(uint8_t* data, uint8_t bits, const char* message) {
    Serial.print(message);    

    //Print value in HEX
    uint8_t bytes = (bits+7)/8;
    for (int i=0; i<bytes; i++) {
        Serial.print(data[i] >> 4, 16);
        Serial.print(data[i] & 0xF, 16);
    }
    Serial.println();
}
