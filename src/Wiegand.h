#include <stdint.h>

class Wiegand {
public:
    /**
     * Accepts messages of any size.
     *
     * Unfortunately, it also means waiting `TIMEOUT` after the last bit
     * and calling `flush` to make sure that the message is finished.
     */
    static const uint8_t LENGTH_ANY = 0xFF;

    /**
     * 2ms seems to be the accepted interval between bits, but since it is very poorly
     * standardized, so it's better to be on the safe side
     */
    static const uint8_t TIMEOUT = 25;

    /**
     * 34-bit is the maximum I've seen used for Wiegand
     */
    static const uint8_t MAX_BITS = 64;

    /**
     * 34-bit is the maximum I've seen used for Wiegand
     */
    static const uint8_t MAX_BYTES = ((MAX_BITS + 7)/8);

    /**
     * Possible communication errors sent to your `data_error_callback`
     */
    enum DataError { Communication, SizeTooBig, SizeUnexpected, DecodeFailed, VerificationFailed};

    /**
     * Gets the message associated with a `DataError`
     */
    static inline const char* DataErrorStr(DataError error) {
        switch (error) {
            case Communication:
                return "Communication Error";
            case SizeTooBig:
                return "Message size too big";
            case SizeUnexpected:
                return "Message size unexpected";
            case DecodeFailed:
                return "Unsupported message format";
            case VerificationFailed:
                return "Message verification failed";
            default:
                return "Unknown";
        }
    }

    typedef void (*data_callback)(uint8_t* data, uint8_t bits, void* param);
    typedef void (*data_error_callback)(DataError error, uint8_t* rawdata, uint8_t bits, void* param);
    typedef void (*state_callback)(bool plugged, void* param);

private:
    uint8_t expected_bits;
    bool decode_messages;
    uint8_t bits;
    uint8_t state;
    unsigned long timestamp;
    uint8_t data[MAX_BYTES];
    Wiegand::data_callback func_data;
    Wiegand::data_error_callback func_data_error;
    Wiegand::state_callback func_state;
    void* func_data_param;
    void* func_data_error_param;
    void* func_state_param;

    /**
     * Adds a new bit to the payload
     */
    void addBitInternal(bool value);

    /**
     * Verifies if the current buffer is valid and sends it to the data / error callbacks.
     * If the buffer is invalid, it is discarded
     */
    void flushData();

public:
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
    void begin(uint8_t expected_bits=LENGTH_ANY, bool decode_messages=true);

    /**
    * Sets the device as "not-initialized"
    */
    void end();

    /**
     * Resets the state so that it awaits a new message.
     *
     * If the data pins aren't high, it sets the `ERROR_TRANSMISSION` flag
     * to signal it is probably in the middle of a truncated message or something.
     */
    void reset();

    /**
     * Returns if this device is initialized (with `begin()`) and a reader has been connected.
     *
     * A reader is considered connected when both D0 and D1 are high,
     * and it is considered disconnected when both are low.
     */
    operator bool();

    /**
     * Clean up state after `WIEGAND_TIMEOUT` milliseconds without events
     *
     * This means sending out any pending message and calling `reset()`
     */
    void flush();

    /**
    * Immediately cleans up state, sending out pending messages and calling `reset()`
    */
    void flushNow();


    /**
     * Attaches a Data Receive Callback.
     *
     * This will be called whenever a message has been received without errors.
     */
    template<typename T> void onReceive(void (*func)(uint8_t* data, uint8_t bits, T* param), T* param=nullptr) {
      func_data = (data_callback)func;
      func_data_param = (void*)param;
    }


    /**
     * Attaches a Data Transmission Error Callback.
     *
     * This will be called whenever a message has been received without errors.
     */
    template<typename T> void onReceiveError(void (*func)(DataError error, uint8_t* data, uint8_t bits, T* param), T* param=nullptr) {
      func_data_error = (data_error_callback)func;
      func_data_error_param = (void*)param;
    }

    /**
     * Attaches a State Change Callback. This is called whenever a device is attached or dettached.
     *
     * If you have a dettachable device, add pull down resistors to both data lines, otherwise random
     * noise will produce lots of bogus State Change notifications (and maybe a few Data Received notifications)
     */
    template<typename T> void onStateChange(void (*func)(bool plugged, T* param), T* param=nullptr) {
      func_state = (state_callback)func;
      func_state_param = (void*)param;
    }

    /**
    * Updates the state of a pin.
    *
    * It will trigger adding bits to the payload, device connection/disconnection,
    * dispatching the payload to the callback, etc
    */
    void setPinState(uint8_t pin, bool pin_state);

    /**
     * Notifies the library that the pin Data0 has changed to `pin_state`
     */
    inline void setPin0State(bool state) {
      setPinState(0, state);
    }

    /**
     * Notifies the library that the pin Data1 has changed to `pin_state`
     */
    inline void setPin1State(bool state) {
      setPinState(1, state);
    }

    /**
     * Receives a data bit.
     *
     * This is meant for testing only
     */
    inline void receivedBit(bool bitValue) {
      setPinState(0, true);
      setPinState(1, true);
      setPinState(bitValue, false);
      setPinState(bitValue, true);
    }
};
