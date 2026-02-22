
// This example shows how to use the PS2Protocol class to send commands
// to a PS2 device.  It handles all the details of the protocol and
// interface, such as managing the state changes of the clock and data inputs
// and correctly generating or validing start, stop, and parity bits.
//
// This class support all PS2 devices, include keyboards and mice.
//
// If you are interfacing with a keyboard, it's more convenient to use the
// PS2KeyboardManager class.  See that example for details.

#include <ps2_protocol.h>

// Global object to handle PS2 protocol with keyboard.
static PS2P_GLOBAL(kbd);
static byte leds = 0;

void setup() {
  // Initialize debug console.
  Serial.begin(9600);

  // Initialze PS2 protocol handler for the keyboard.  In this example,
  // pin 2 is the clock input and pin 3 is the data input.
  if (!kbd.begin(2, 3)) {
    Serial.println(F("*** Unable to begin PS2 protocol"));
    return;
  }

  Serial.println(F("Ready"));
}

void loop() {
  static bool last_was_break = false;

  // Process each byte available.
  int count = kbd.available();
  while (count > 0) {
    byte b = kbd.read();
    Serial.print(F("Received: "));
    Serial.println(b, HEX);
    --count;

    // If this is CAPS_LOCK released, send command to toggle LED.
    if (b == 0x58 && last_was_break) {
      Serial.println(F("Toggling CAPS LOCK LED"));
      leds = leds ? 0 : 0x04;
      kbd.writeAndWait(0xED);
      kbd.writeAndWait(leds);
    }

    last_was_break = b == 0xF0;
  }
}
