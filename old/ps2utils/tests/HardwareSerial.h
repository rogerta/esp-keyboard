#ifndef MOCK_HARDWARE_SERIAL_H_
#define MOCK_HARDWARE_SERIAL_H_

// Mock of HardwareSerial Arduino class

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

class HardwareSerial {
 public:
  HardwareSerial() {}
  void begin(int baud_rate) {}
  void end() {}

  void print(const char* text) {}
  //void print(const char text) {}
  void print(const uint8_t text) {}
  void print(int i, int radix) {}

  void println(const char* text) {}
  //void println(const char text) {}
  void println(const uint8_t text) {}
  void println(int i, int radix) {}
  void println() {}
};

HardwareSerial Serial;

#endif  // MOCK_HARDWARE_SERIAL_H_
