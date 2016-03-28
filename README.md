# Yet Another Arduino Wiegand Library

A library to received data from Wiegand RFID Card readers.

## Features
_Support multiple data formats_!
- It can detect the message size format automatically!
- 26 and 34 bits are tested and work fine
- Should work with most other formats (Let me know if you test it)

_It is event-driven_!
- You don't ask if there is a card, a callback will tell you when there is one.
- The extra `void*` parameter on the callbacks are useful if you are using instances in your code.

_It is hardware-agnostic_!
- It's up to you to detect input changes. You may use [External Interruptions](examples/interrupts/interrupts.ino), [Pooling](examples/pooling/pooling.ino), or something else.



# How to use it

```c++
/*
 * Example on how to use the Wiegand reader library with interruptions.
 */

#include <Wiegand.h>

// These are the pins connected to the Wiegand D0 and D1 signals.
// Ensure your board supports external Interruptions on these pins
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

  //initialize pins as INPUT and attaches interruptions
  pinMode(PIN_D0, INPUT);
  pinMode(PIN_D1, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_D0), pinStateChanged, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_D1), pinStateChanged, CHANGE);

  //Sends the initial pin state to the Wiegand library
  pinStateChanged();
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
void pinStateChanged() {
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
```



# Wiegand Protocol

The Wiegand protocol is very and easy to implement, but is poorly standarlized.


## Data transmission

The data is sent over 2 wires, `Data 0` and `Data 1`. Both wires are set to `HIGH` most of the time.

When a message is being transmitted, bits are sent one at a time:
- If the bit value is `0`, `Data 0` is set to `LOW` for a few microseconds (between 20µs and 100µs).
- If the bit value is `1`, `Data 1` is set to `LOW` for a few microseconds (between 20µs and 100µs).

After a small delay (between 200µs and 20 ms), the next bit is sent – until the message is complete.

Having both pins to `LOW` is an invalid state, and usually means that the card reader is disconnected.


## Message format

This is where things get hairy: Many vendors have defined their own message formats, with different sizes, fields, parity checks and layouts.

This library attemps to support different message sizes, but only supports the most common layout:
- The first and the last bits of the message are used for parity checks.
- The first half of the bits must have EVEN parity.
- The second half of the bits must have ODD parity.
- If the message has an odd number of bits, the center bit is used on both parity checks.

Also, Wiegand messages are often split in fields with different semantics. E.g., Facility code + Card Number.
This library ignores that and returns the data as one big data buffer (bits are received in big-endian order)



# This Library

## Hardware integration

Since the hardware changes a lot, this library doesn't assume anything on how to read the data pins.

When the state of a pin has changed, it's up to you to call `Wiegand.setPinState(pin, state)`.

There are examples on how to use it with [Interruptions](examples/interrupts/interrupts.ino) and [Pooling](examples/pooling/pooling.ino) on Arduinos.
(You probably want to use interruptions)


## Receiving Data

Use `Wiegand.onReceive()` to listen to messages. The listener receives the databuffer and number of bits (Parity bits are not present on the buffer and are not accounted on the number of bits).

Keep in mind that messages with unexpected sizes, invalid checksums, etc, are silently ignored.


### Automatic message size detection

If the message size is specified on `Wiegand.begin(size)`, your listener will be called during the last `Wiegand.setPinState()`. Easy!

On the other hand, if you are using automatic message size detections (`Wiegand.begin()`), the message will only be available a few milliseconds after the last bit is received, since the library must wait little longer to ensure the message is complete.

In this case, you should call `Wiegand.flush()` after suficient time (`WIEGAND_TIMEOUT` milliseconds) has elapsed. The easiest way is to call it from your main loop.

__This library is not thread safe__. If you are using interruptions to detect changes in pin state, call `Wiegand.flush()` with interruptions disabled.


## Device detection

This library supports detection of the card reader.

Use `Wiegand.onStateChange()` to listen to plug/unplug events.

To use this feature, you'll need to add a pull-down resistor on both data pins. This will set the input on a invalid state when the reader is unplugged.

