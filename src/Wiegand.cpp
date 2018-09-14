#include <Wiegand.h>
#include <Arduino.h>

#define PIN_0                        0x01
#define PIN_1                        0x02
#define DEVICE_CONNECTED             0x04
#define DEVICE_INITIALIZED           0x08

#define ERROR_TRANSMISSION           0x10
#define ERROR_TOO_BIG                0x20

#define MASK_PINS                    (PIN_0 | PIN_1)
#define MASK_STATE                   0x0F
#define MASK_ERRORS                  0xF0

/**
 * Sets the value of the `i`-th data bit
 */
inline void writeBit(uint8_t* data, uint8_t i, bool value) {
    if (value) {
        data[i>>3] |=  (0x80 >> (i&7));
    } else {
        data[i>>3] &= ~(0x80 >> (i&7));
    }
}

/**
 * Reads the value of the `i`-th data bit
 */
inline bool readBit(uint8_t* data, uint8_t i) {
    return bool(data[i>>3] & (0x80 >> (i&7)));
}

/**
 * Sign a subrange of data, shrinks and aligns the buffer to the right, inline
 *
 * returns the number of bits in the subrange
 */
inline uint8_t align_data(uint8_t* data, uint8_t start, uint8_t end) {
    uint8_t aligned_data[Wiegand::MAX_BYTES];
    uint8_t aligned_bits = end - start;
    uint8_t aligned_bytes = (aligned_bits + 7)/8;
    uint8_t aligned_offset = 8*aligned_bytes - aligned_bits;

    aligned_data[0] = 0;
    for (int bit=0; bit<aligned_bits; bit++) {
        writeBit(aligned_data, bit + aligned_offset, readBit(data, bit+start));
    }
    for (int i=0; i<aligned_bytes; i++) {
        data[i] = aligned_data[i];
    }
    return aligned_bits;
}


/**
 * Sets the device as "initialized" and resets it to wait a new message.
 *
 * If `expected_bits` is specified (usually 4, 8, 26 or 34), the data callback will
 * be notified immedialy aftern the last bit is received.
 *
 * Otherwise (`expected_bits=LENGTH_ANY`), you will need to call `flush()`
 * inside your main loop to receive notifications, and the end of the message will be
 * triggered after a few ms without communication.
 *
 *
 * if `decode_messages` is set, parity bits will be checked and removed
 * during preprocessing, otherwise the raw message will be sent to the callback.
 */
void Wiegand::begin(uint8_t expected_bits, bool decode_messages) {
    this->expected_bits = expected_bits;
    this->decode_messages = decode_messages;

    //Set state as "INVALID", so that data can only be received after a few millis in "ready" state
    bits=0;
    timestamp = millis();
    state = (state & MASK_STATE) | DEVICE_INITIALIZED | ERROR_TRANSMISSION;
}

/**
 * Sets the device as "not-initialized"
 */
void Wiegand::end() {
    expected_bits = 0;

    bits=0;
    timestamp = millis();
    state &= MASK_STATE & ~DEVICE_INITIALIZED;
}


/**
 * Resets the state so that it awaits a new message.
 *
 * If the data pins aren't high, it sets the `ERROR_TRANSMISSION` flag
 * to signal it is probably in the middle of a truncated message or something.
 */
void Wiegand::reset() {
    bits=0;
    state &= MASK_STATE;
    //A transmission must start with D0=1, D1=1
    if ((state & MASK_PINS) != MASK_PINS) {
        state |= ERROR_TRANSMISSION;
    }
}


/**
 * Returns if this device is initialized (with `begin()`) and a reader has been connected.
 *
 * A reader is considered connected when both D0 and D1 are high,
 * and it is considered disconnected when both are low.
 */
Wiegand::operator bool() {
    return (state & (DEVICE_CONNECTED|DEVICE_INITIALIZED)) == (DEVICE_CONNECTED|DEVICE_INITIALIZED);
}


/**
 * Verifies if the current buffer is valid and sends it to the data / error callbacks.
 * If the buffer is invalid, it is discarded
 */
void Wiegand::flushData() {
    //Ignore empty messages
    if ((bits == 0) || (expected_bits == 0)) {
        return;
    }

    //Check for pending errors
    if (state & MASK_ERRORS) {
        if (func_data_error) {
            bits = align_data(data, 0, bits);
            if (state & ERROR_TOO_BIG) {
                func_data_error(DataError::SizeTooBig, data, bits, func_data_error_param);
            } else {
                func_data_error(DataError::Communication, data, bits, func_data_error_param);
            }
        }
        return;
    }

    //Validate the message size
    if ((expected_bits != bits) && (expected_bits != Wiegand::LENGTH_ANY)) {
        if (func_data_error) {
            bits = align_data(data, 0, bits);
            func_data_error(DataError::SizeUnexpected, data, bits, func_data_error_param);
        }
        return;
    }

    //Decode the message
    if (!decode_messages) {
        if (func_data) {
            bits = align_data(data, 0, bits);
            func_data(data, bits, func_data_param);
        }
    } else {
        //4-bit keycode: No check necessary
        if ((bits == 4)) {
            if (func_data) {
                bits = align_data(data, 0, bits);
                func_data(data, bits, func_data_param);
            }

        //8-bit keybode: UpperNibble = ~lowerNibble
        } else if ((bits == 8)) {
            uint8_t value = data[0] & 0xF;
            if (data[0] == (value | ((0xF & ~value)<<4))) {
                if (func_data) {
                    func_data(&value, 4, func_data_param);
                }
            } else {
                if (func_data_error) {
                    bits = align_data(data, 0, bits);
                    func_data_error(DataError::VerificationFailed, data, bits, func_data_error_param);
                }
            }

        //26 or 34-bits: First and last bits are used for parity
        } else if ((bits == 26) || (bits == 34)) {
            //FIXME: The parity check doesn't seem to work for a 34-bit reader I have,
            //but I suspect that the reader is non-complaint

            boolean left_parity = false;
            boolean right_parity = false;
            for (int i=0; i<(bits+1)/2; i++) {
                left_parity = (left_parity != readBit(data, i));
            }
            for (int i=bits/2; i<bits; i++) {
                right_parity = (right_parity != readBit(data, i));
            }

            if (!left_parity && right_parity) {
                if (func_data) {
                    bits = align_data(data, 1, bits-1);
                    func_data(data, bits, func_data_param);
                }
            } else {
                if (func_data_error) {
                    bits = align_data(data, 0, bits);
                    func_data_error(DataError::VerificationFailed, data, bits, func_data_error_param);
                }
            }

        } else {
            if (func_data_error) {
                bits = align_data(data, 0, bits);
                func_data_error(DataError::DecodeFailed, data, bits, func_data_error_param);
            }
        }
    }
}


/**
 * Clean up state after `TIMEOUT` milliseconds without events
 *
 * This means sending out any pending message and calling `reset()`
 */
void Wiegand::flush() {
    unsigned long elapsed = millis() - timestamp;
    // Resets state if nothing happened in a few milliseconds
    if (elapsed > TIMEOUT) {
        // Might have a pending data package
        flushData();
        reset();
    }
}

/**
 * Immediately cleans up state, sending out pending messages and calling `reset()`
 */
void Wiegand::flushNow() {
    flushData();
    reset();
}

/**
 * Adds a new bit to the payload
 */
void Wiegand::addBitInternal(bool value) {
    //Skip if we have too much data
    if (bits >= MAX_BITS) {
        state |= ERROR_TOO_BIG;
    } else {
        writeBit(data, bits++, value);
    }

    // If we know the number of bits, there is no need to wait for the timeout to send the data
    if (expected_bits > 0 && (bits == expected_bits)) {
        flushData();
        reset();
    }
}


/**
 * Updates the state of a pin.
 *
 * It will trigger adding bits to the payload, device connection/disconnection,
 * dispatching the payload to the callback, etc
 */
void Wiegand::setPinState(uint8_t pin, bool pin_state) {
    uint8_t pin_mask = pin ? PIN_1 : PIN_0;

    flush();

    //No change? Abort!
    if (bool(state & pin_mask) == pin_state) {
        return;
    }

    timestamp = millis();
    if (pin_state) {
        state |= pin_mask;
    } else {
        state &= ~pin_mask;
    }

    //Both pins on: bit received
    if ((state & MASK_PINS) == MASK_PINS) {
        //If the device wasn't ready before -- Enable it, and marks state as INVALID until is settles.
        if (state & DEVICE_CONNECTED) {
            addBitInternal(pin);
        } else {
            //Device connection was detected right now!
            //Set the device as connected, but unstable
            state = (state & MASK_STATE) | DEVICE_CONNECTED | ERROR_TRANSMISSION;
            if (func_state) {
                func_state(true, func_state_param);
            }
        }

    //Both pins off - Device is unplugged
    } else if ((state & MASK_PINS) == 0) {
        if (state & DEVICE_CONNECTED) {
            //Flush truncated message, if any, and resets state
            state |= ERROR_TRANSMISSION;
            flushNow();

            //Set the state as disconnected
            state = (state & MASK_STATE & ~DEVICE_CONNECTED);
            if (func_state) {
                func_state(false, func_state_param);
            }
        }
    }
}
