#include <Wiegand.h>
#include <Arduino.h>

#define PIN_0                0x01
#define PIN_1                0x02
#define SETUP_READY          0x04
#define SETUP_INITIALIZED    0x08
#define FLAG_PARITY_LEFT     0x10
#define FLAG_PARITY_RIGHT    0x20
#define FLAG_LAST_BIT        0x40
#define FLAG_INVALID         0x80

#define MASK_PINS            0x03
#define MASK_SETUP           0x0C
#define MASK_FLAGS           0xF0




void Wiegand::begin(uint8_t bits) {
    expected_bits = bits;

    //Set state as "INVALID", so that data can only be received after a few millis in "ready" state
    state &= MASK_PINS | SETUP_READY;
    state |= SETUP_INITIALIZED | FLAG_INVALID;
    timestamp = millis();
}


void Wiegand::end() { 
    state &= MASK_PINS | SETUP_READY;
}


void Wiegand::reset() {
    bits=0;
    state &= ~MASK_FLAGS;
}


// Returns if this device is initialized (with `begin()`) and a reader has been detected (Both D0 and D1 are pulled high)
Wiegand::operator bool() {
    return (state & (SETUP_READY|SETUP_INITIALIZED)) == (SETUP_READY|SETUP_INITIALIZED);
}




// If we have a complete payload on the buffer and parity checks out, sends a notification.
void Wiegand::flushData() {
    // If the frame has the expected size
    if ( (bits == expected_bits) || ( ((expected_bits == WIEGAND_LENGTH_AUTO) && ( (bits == 26) || (bits == 34) ) ) ) ) {
        // And it is valid: parity checks out, no overflow.
        if ( !(state & FLAG_INVALID) && ((state & (FLAG_PARITY_LEFT | FLAG_PARITY_RIGHT)) == FLAG_PARITY_RIGHT) ) {
            if (func_data) {
                func_data(data, bits-2, func_data_param);
            }
        }
    }
}


// Clean up state if no events happened for `WIEGAND_TIMEOUT` milliseconds.
// This means sending out any complete message and reseting the state machine
void Wiegand::flush() {
    // Ignore if this device is disabled
    if (!*this) {
        return;
    }
    
    unsigned long elapsed = millis() - timestamp;
    // Resets state if nothing happened in a few milliseconds
    if (elapsed > WIEGAND_TIMEOUT) {
        // Might have a pending data package, if expected_bits == WIEGAND_LENGTH_AUTO
        flushData();
        reset();
    }
}




// Sets the value of the `i`-th payload bit
inline void Wiegand::writeBit(uint8_t i, bool value) {
    if (value) {
        data[i>>3] |=  (0x80 >> (i&7));
    } else {
        data[i>>3] &= ~(0x80 >> (i&7));
    }
}


// Reads the value of the `i`-th payload bit
inline bool Wiegand::readBit(uint8_t i) {
    return bool(data[i>>3] & (0x80 >> (i&7)));
}


void Wiegand::addBit(bool value) {
    //Skip if it is not initialized / Not ready.
    if (!*this) {
        return;
    }
    //Skip if we have too much data
    if (bits >= 8*WIEGAND_MAX_BYTES+2) {
        state |= FLAG_INVALID;
    }
    if (state & FLAG_INVALID) {
        return;
    }
    
    // Special case for 1st bit
    if (bits == 0) {
        if (value) {
            state |= FLAG_PARITY_LEFT;
        }
    
    } else {
        // Store data bit
        // Notice that we perform a delayed write, because we have to ignore the first and last bits (the parity bits)
        if (bits >= 2) {
            writeBit(bits-2, state & FLAG_LAST_BIT);
        }
        
        // Updates parity
        if (value) {
            state ^= FLAG_PARITY_RIGHT;
        }
        // As data grows, the bits that were considered part of the "Right" become part of the "Left".
        // This chunk updates the parity flags accordingly
        if (bits&1 && bits>=3) {
            if (readBit((bits-3)/2)) {
                state ^= FLAG_PARITY_LEFT | FLAG_PARITY_RIGHT;
            }
        }
    }
    bits++;
    state = value ? (state | FLAG_LAST_BIT) : (state & ~FLAG_LAST_BIT);

    // If we know the number of bits, there is no need to wait for the timeout to send the data
    if (expected_bits && (bits == expected_bits)) {
        flushData();
        state |= FLAG_INVALID;
    }
}




// Updates the state of 
void Wiegand::setPinState(uint8_t pin, bool pin_state) {
    uint8_t pin_mask = pin? PIN_1 : PIN_0;
    
    //No change? Abort!
    if (bool(state & pin_mask) == pin_state) {
        return;
    } else if (pin_state) {
        state |= pin_mask;
    } else {
        state &= ~pin_mask;     
    }

    //Flush old events
    flush();   
    timestamp = millis();

    //Both pins on - Databit received
    if ( ( (state & MASK_PINS) == (PIN_0|PIN_1) ) ) {
        //If the device wasn't ready before -- Enable it, and marks state as INVALID until is settles.
        if (!(state & SETUP_READY)) {
            state |= SETUP_READY|FLAG_INVALID;
            if (func_state) {
              func_state(true, func_state_param);
            }
        }
        addBit(pin);

    //Both pins off - Device is unplugged
    } else if ( !(state & MASK_PINS) ) {
        if (state & SETUP_READY) {
            state &= ~SETUP_READY;
            state |= FLAG_INVALID;
            if (func_state) {
              func_state(false, func_state_param);
            }
        }
    }
}
