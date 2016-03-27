#include <Wiegand.h>

Wiegand wiegand;

#define PIN_D0 18
#define PIN_D1 19

void setup()
{
  Serial.begin(9600);

  wiegand.onReceive(receivedData, "Card readed");
  wiegand.onStateChange(stateChanged, "State changed");
  wiegand.begin(WIEGAND_LENGTH_AUTO);

  pinMode(PIN_D0, INPUT);
  pinMode(PIN_D1, INPUT);
  wiegand.set_data0(digitalRead(PIN_D0));
  wiegand.set_data1(digitalRead(PIN_D1));
}

void loop() {
  wiegand.flush();
  wiegand.set_data0(digitalRead(PIN_D0));
  wiegand.set_data1(digitalRead(PIN_D1));
}

void stateChanged(bool plugged, const char* message) {
    Serial.print(message);
    Serial.print(": ");
    Serial.print(plugged ? "CONNECTED" : "DISCONNECTED");
    Serial.println();
    Serial.println();
}

void receivedData(uint8_t* data, uint8_t datalen, const char* message) {
    Serial.println(message);
    
    //Print value in HEX
    for (int i=0; i<datalen; i++) {
        Serial.print(data[i] >> 4, 16);
        Serial.print(data[i] & 0xF, 16);
    }
    Serial.println();

    //Print value in decimal, grouped every 2 bytes (Wiegand Notation)
    int i=0;
    if (datalen & 1) {
        Serial.print(data[0]);
        i++;
    }
    for(;i<datalen; i+=2) {
        if (i) {
            Serial.print("-");
        }
        Serial.print(data[i] << 8 | data[i+1]);
    }
    Serial.println();
    Serial.println();
}
