// This file mocks enough of the real Arduino.h file for testing purposes.

#include "Arduino.h"

#include <algorithm> // for remove_if
#include <ctime>  // for time()
#include <functional> // for equal_to
#include <vector>

namespace {

const int kMaxPins = 32;

uint8_t g_pinMode[kMaxPins];
uint8_t g_pinValue[kMaxPins];
std::vector<arduino::mock::DelayHook*> g_delay_hooks;

class Init {
 public:
  Init() {
    memset(g_pinMode, INPUT, sizeof(g_pinMode));
    memset(g_pinValue, -1, sizeof(g_pinValue));
  }
} globalInit;

}  // namespace


namespace arduino {

namespace mock {

uint8_t GetPinMode(uint8_t pin) {
  return g_pinMode[pin];
}

void RegisterDelayHook(DelayHook* delay_hook) {
  g_delay_hooks.push_back(delay_hook);
}

void UnregisterDelayHook(DelayHook* delay_hook) {
  g_delay_hooks.erase(std::remove_if(g_delay_hooks.begin(),
                                     g_delay_hooks.end(),
                                     std::bind1st(std::equal_to<DelayHook*>(),
                                                  delay_hook)),
                      g_delay_hooks.end());
}

void UnregisterAllDelayHooks() {
  g_delay_hooks.clear();
}

}  // namespace mock

}  // namespace arduino


void pinMode(uint8_t pin, uint8_t mode) {
  g_pinMode[pin] = mode;
}

int digitalRead(uint8_t pin) {
  return g_pinValue[pin];
}

void digitalWrite(uint8_t pin, uint8_t value) {
  g_pinValue[pin] = value;
}

void interrupts() {}

void noInterrupts() {}

void attachInterrupt(uint8_t isr, void (*handler)(void), int mode) {}

void detachInterrupt(uint8_t isr) {}

unsigned long millis() {
  return time(0);
}

void delay(unsigned int msec) {
  for (auto it = g_delay_hooks.begin(); it != g_delay_hooks.end(); ++it) {
    (*it)->RunDelayHook();
  }
}

void delayMicroseconds(unsigned int usec) {
  for (auto it = g_delay_hooks.begin(); it != g_delay_hooks.end(); ++it) {
    (*it)->RunDelayHook();
  }
}
