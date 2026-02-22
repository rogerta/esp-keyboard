
// This example shows how to use the PS2Keyboard class to read key codes
// from a PS2 keyboard.  It uses a PS2Protocol to read the stream of bytes
// from the keyboard, interpret them as scan codes (also called make codes and
// breaks) and convert them to unique keycodes.
//
// It's more likely that you want to use the PS2KeyboardManager class if you
// are interfacing with a PS2 keyboard.

#include <ps2_debug.h>
#include <ps2_keyboard.h>
#include <ps2_protocol.h>

// Global object to handle PS2 keyboard.
static PS2P_GLOBAL(protocol);
static PS2Keyboard keyboard;
static PS2Debug debug;

void setup() {
  // Initialze PS2 protocol handler for the keyboard.  In this example,
  // pin 2 is the clock input and pin 3 is the data input.
  if (!protocol.begin(2, 3, &debug)) {
    Serial.println(F("*** Unable to begin PS2 protocol"));
    return;
  }

  // Initialze PS2 keyboard.
  if (!keyboard.begin(&protocol, &debug)) {
    Serial.println(F("*** Unable to begin PS2 keyboard"));
    return;
  }

  debug.begin(&keyboard);
}

void loop() {
  debug.dump();

  // Process each key available.
  int count = keyboard.available();
  while (count > 0) {
    PS2Keyboard::Key key = keyboard.read();
    Serial.print(key.isPressed() ? F("Pressed ") : F("Released "));
    Serial.println(key.code(), HEX);
    --count;
  }
}
