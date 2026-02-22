
// This example shows how to use the PS2Protocol class to read the raw byte
// stream from a PS2 device.  It handle all the details of the protocol and
// interface, such as managing the state changes of the clock and data inputs
// and correctly generating or validing start, stop, and parity bits.
//
// This class support all PS2 devices, include keyboards and mice.
//
// If you are interfacing with a keyboard, it's more convenient to use the
// PS2KeyboardManager class.  See that example for details.

#include <ps2_debug.h>
#include <ps2_protocol.h>

// Global object to handle PS2 protocol with keyboard.
static PS2P_GLOBAL(kbd);
static PS2Debug debug;

void setup() {
  // Initialze PS2 protocol handler for the keyboard.  In this example,
  // pin 2 is the clock input and pin 3 is the data input.
  if (!kbd.begin(2, 3, &debug)) {
    Serial.println(F("*** Unable to begin PS2 protocol"));
    return;
  }

  debug.begin(&kbd);
}

void loop() {
  debug.dump();

  // Process each byte available.
  int count = kbd.available();
  while (count > 0) {
    byte b = kbd.read();
    Serial.print(F("Received: "));
    Serial.println(b, HEX);
    --count;
  }
}
