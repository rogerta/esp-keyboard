#ifndef MOCK_ARDUINO_H_
#define MOCK_ARDUINO_H_

// This file mocks enough of the real Arduino.h file for testing purposes.

#include <cstring>  // for memset()

#define NOT_A_PIN 0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 2

#define CHANGE 1
#define FALLING 2
#define RISING 3

#define NOT_AN_INTERRUPT -1

typedef char __FlashStringHelper;
#define F(s) s

#define digitalPinToInterrupt(p) \
    ((p) == 2 ? 0 : ((p) == 3 ? 1 : NOT_AN_INTERRUPT))

#define LOW 0
#define HIGH 1

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

#ifdef __cplusplus
extern "C" {
#endif

#define PROGMEM
#define pgm_read_byte_near(addr) *(addr)
typedef short prog_uchar;

typedef unsigned char boolean;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef uint8_t byte;

void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, uint8_t value);

void interrupts();
void noInterrupts();
void attachInterrupt(uint8_t isr, void (*handler)(void), int mode);
void detachInterrupt(uint8_t isr);

unsigned long millis();
void delay(unsigned int msec);
void delayMicroseconds(unsigned int usec);

#ifdef __cplusplus
}
#endif

// The following functions, classes and types are not part of the Arduino SDK,
// but are only part of the mocking framwork

namespace arduino {

namespace mock {

// In tests, the last value written by digitalWrite() can be read by
// digitalRead().  The pin mode is essentually ignored.  Reading a pin that
// has not been written returns an undefined value.
uint8_t GetPinMode(uint8_t pin);

// Regsister hooks that will be called when either delay() or
// delayMicroseconds() is called.
class DelayHook {
 public:
  virtual ~DelayHook() {}
  virtual void RunDelayHook() = 0;
};

void RegisterDelayHook(DelayHook* delay_hook);
void UnregisterDelayHook(DelayHook* delay_hook);
void UnregisterAllDelayHooks();

// Prefer this class to automatically register and unregister delay hooks,
// instead of calling the above functions directly.
class ScopedDelayHook {
 public:
  ScopedDelayHook(DelayHook* delay_hook) : delay_hook_(delay_hook) {
    RegisterDelayHook(delay_hook_);
  }
  ~ScopedDelayHook() {
    UnregisterDelayHook(delay_hook_);
  }
 private:
  ScopedDelayHook(const ScopedDelayHook&);
  ScopedDelayHook& operator=(const ScopedDelayHook&);

  DelayHook* delay_hook_;
};

}  // namespace mock

}  // namespace arduino

#endif  // MOCK_ARDUINO_H_
