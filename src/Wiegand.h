#define WIEGAND_LENGTH_AUTO     0

#define WIEGAND_TIMEOUT        10
#define WIEGAND_MAX_BYTES       4

#include <stdint.h>

class Wiegand {
private:
  typedef void (*data_callback)(uint8_t* data, uint8_t datalen, void* param);
  typedef void (*state_callback)(bool plugged, void* param);
  
  uint8_t expected_bits;
  uint8_t bits;
  uint8_t state;
  unsigned long timestamp;
  uint8_t data[WIEGAND_MAX_BYTES];
  data_callback func_data;
  state_callback func_state;
  void* func_data_param;
  void* func_state_param;

  inline void write_bit(uint8_t i, bool value);
  inline bool read_bit(uint8_t i);
  void add_bit(bool value);
  void flush_data();
  void reset();
  
public:
  // Start things up
  // If the number of bits is specified (usually 26 or 34), the data Callback will be notified immedialy aftern the last bit.
  // Otherwise, you will need to call `flush()` inside your main loop to receive notifications.
  void begin(uint8_t bits=WIEGAND_LENGTH_AUTO);
  // Stops it
  void end();
  
  // Sends pending Data Received notifications.
  // When using automatic length detection, this method has to be called frequently.
  void flush();
  
  // Checks if the class has started and a scanner is detected
  operator bool();

  //Attaches a Data Receive Callback. This will be called whenever a message has been received without errors.
  template<typename T> void onReceive(void (*func)(uint8_t* data, uint8_t datalen, T* param), T* param) {
    func_data = (data_callback)func;
    func_data_param = (void*)param;
  }
  
  //Attaches a State Change Callback. This is called whenever a device is attached or dettached.
  //If you have a dettachable device, add pull down resistors to both data lines, otherwise random noise will produce lots of bogus State Change notifications (and a few Data Received notifications)
  template<typename T> void onStateChange(void (*func)(bool plugged, T* param), T* param) {
    func_state = (state_callback)func;
    func_state_param = (void*)param;
  }
  
  //Notifies the library that the pin Data-`pin_value` has changed to `pin_state`
  void set_pin(uint8_t pin_value, bool pin_state);
  
  //Notifies the library that the pin Data0 has changed to `pin_state`
  inline void set_data0(bool state) {
    set_pin(0, state);
  }
  
  //Notifies the library that the pin Data1 has changed to `pin_state`
  inline void set_data1(bool state) {
    set_pin(1, state);
  }
};