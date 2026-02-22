
// This example shows how to use the PS2KeyboardManager class to keep track
// of the state of a PS2 keybaord.  At any time the sketch can ask if any
// key is pressed or not.
//
// It is important to call the avaiable() method from inside the loop()
// function to make sure that the PS2KeyboardManager class is always up to
// date.
//
// If you use this class to implement the "device side" of a USB or bluetooth
// keyboard, the report() method is very convenient for getting the information
// to be reported back to the host.  Its field can be used directly in the USB
// HID report structure.

#include <ps2_debug.h>
#include <ps2_keyboard.h>
#include <ps2_keyboard_manager.h>
#include <ps2_protocol.h>

// Global objects to handle PS2 keyboard.
static PS2P_GLOBAL(protocol);
static PS2Keyboard keyboard;
static PS2KeyboardManager manager;
static PS2Debug debug;

void setup() {
  // Initialize PS2 protocol handler for the keyboard.  In this example,
  // pin 2 is the clock input and pin 3 is the data input.
  if (!protocol.begin(2, 3, &debug)) {
    Serial.println(F("*** Unable to begin PS2 protocol"));
    return;
  }

  // Initialize PS2 keyboard.
  if (!keyboard.begin(&protocol, &debug)) {
    Serial.println(F("*** Unable to begin PS2 keyboard"));
    return;
  }

  // Initialize PS2 keyboard manager.
  if (!manager.begin(&keyboard, 0, &debug)) {
    Serial.println(F("*** Unable to begin PS2 keyboard manager"));
    return;
  }

  debug.begin(&manager);
}

void loop() {
  debug.dump();

  // Allow the keyboard manager to check the current state of the keyboard.
  // If the state has changed, available() returns true.
  int count = manager.available();
  while (count > 0) {
    PS2KeyboardManager::Report report = manager.read();
    debug.dumpReport(report);
    --count;
  }
}
