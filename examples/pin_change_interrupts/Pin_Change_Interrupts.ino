/*
 * Example on how to use the Wiegand reader library with YetAnotherPcInt library
 * https://github.com/paulo-raca/YetAnotherArduinoPcIntLibrary
 */

#include <Wiegand.h>
#include <YetAnotherPcInt.h>

// These are the pins connected to the Wiegand D0 and D1 signals.
// Ensure your board supports external Pin Change Interrupts on these pins
#define PIN_D0 9
#define PIN_D1 8

// The object that handles the wiegand protocol
Wiegand wiegand;

// Initialize Wiegand reader
void setup() {
  Serial.begin(9600);

  //Install listeners and initialize Wiegand reader
  wiegand.onReceive(receivedData, "Card readed: ");
  wiegand.onStateChange(stateChanged, "State changed: ");
  wiegand.begin(WIEGAND_LENGTH_AUTO);

  //initialize pins as INPUT and attaches interruptions
  pinMode(PIN_D0, INPUT);
  pinMode(PIN_D1, INPUT);
  PcInt::attachInterrupt(PIN_D0, wiegandD0Changed, &wiegand, CHANGE, true);
  PcInt::attachInterrupt(PIN_D1, wiegandD1Changed, &wiegand, CHANGE, true);
}

// Every few milliseconds, check for pending messages on the wiegand reader
// This executes with interruptions disabled, since the Wiegand library is not thread-safe
void loop() {
  noInterrupts();
  wiegand.flush();
  interrupts();
  //Sleep a little -- this doesn't have to run very often.
  delay(100);
}

// When any of the pins have changed, update the state of the wiegand library
void wiegandD0Changed(Wiegand *wiegand, bool value) {
  wiegand->setPin0State(value);
}
void wiegandD1Changed(Wiegand *wiegand, bool value) {
  wiegand->setPin1State(value);
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
    Serial.println(message);
  
    //Print value in HEX
    uint8_t bytes = (bits+7)/8;
    for (int i=0; i<bytes; i++) {
        Serial.print(data[i] >> 4, 16);
        Serial.print(data[i] & 0xF, 16);
    }
    Serial.println();
}
