
#include <iostream>
#include <unit_tests.h>

#include "ps2_keyboard.h"
#include "ps2_protocol.h"

#define numberof(a) (sizeof(a)/sizeof((a)[0]))

TEST(Enums) {
  // If these expectations fail, then the KeyCode and/or EventType are too
  // large to store in prog_uchar or the Key class.  Reworking of the
  // PS2Keyboard implementation will be necessary.
  EXPECT_LT(PS2Keyboard::KC_LAST_KEYCODE, 0x100);
  EXPECT_LT(PS2Keyboard::KEY_PRESSED, 0x100);
  EXPECT_LT(PS2Keyboard::KEY_RELEASED, 0x100);
}

class PS2KeyboardBeginTests : public testing::TestCase {
 protected:
  PS2P_DECLARE(PS2KeyboardBeginTests, protocol_);
  PS2Keyboard keyboard_;
 private:
  void SetUp() override {
    EXPECT_TRUE(protocol_.begin(2, 3));
  }
};

PS2P_IMPLEMENT(PS2KeyboardBeginTests, protocol_);

TEST_F(PS2KeyboardBeginTests, Enum) {
  // Test a few random values to make sure the assigned enum values match the
  // USB codes.
  EXPECT_EQ(0x04, PS2Keyboard::KC_A);
  EXPECT_EQ(0x1E, PS2Keyboard::KC_1);
  EXPECT_EQ(0x28, PS2Keyboard::KC_ENTER);
  EXPECT_EQ(0x3A, PS2Keyboard::KC_F1);
  EXPECT_EQ(0x53, PS2Keyboard::KC_KP_NUM_LOCK);
  EXPECT_EQ(0x64, PS2Keyboard::KC_NON_US_BACKSLASH);
  EXPECT_EQ(0x74, PS2Keyboard::KC_EXECUTE);
  EXPECT_EQ(0x7F, PS2Keyboard::KC_MUTE);
  EXPECT_EQ(0x87, PS2Keyboard::KC_I18N_1);
  EXPECT_EQ(0x99, PS2Keyboard::KC_ALT_ERASE);
  EXPECT_EQ(0xE0, PS2Keyboard::KC_LCTRL);
  EXPECT_EQ(0xE7, PS2Keyboard::KC_RGUI);
}

TEST_F(PS2KeyboardBeginTests, Begin) {
  EXPECT_TRUE(keyboard_.begin(&protocol_));
}

TEST_F(PS2KeyboardBeginTests, BeginWithNullProtocol) {
  EXPECT_FALSE(keyboard_.begin(0));
}

///////////////////////////////////////////////////////////////////////////////

class PS2KeyboardTests : public testing::TestCase {
 public:
  // Sample bytes returned by PS2 keyboard for the given letters.
  // See http://www.computer-engineering.org/ps2keyboard/scancodes2.html.
  static const byte kMakeCodeA;
  static const byte kMakeCodeB;
  static const byte kMakeCodeC;
  static const byte kMakeCodeBad;
  static const byte kBreak;
  static const byte kExtended;
  static const byte kMakeCodeHome;
  static const byte kUp;
  static const byte kUp_KP8;

 protected:
  PS2P_DECLARE(PS2KeyboardTests, protocol_);
  PS2Keyboard keyboard_;
 private:
  void SetUp() override {
    EXPECT_TRUE(protocol_.begin(2, 3));
    EXPECT_TRUE(keyboard_.begin(&protocol_));
  }
};

const byte PS2KeyboardTests::kMakeCodeA = 0x1C;
const byte PS2KeyboardTests::kMakeCodeB = 0x32;
const byte PS2KeyboardTests::kMakeCodeC = 0x21;
const byte PS2KeyboardTests::kMakeCodeBad = 0x00;
const byte PS2KeyboardTests::kBreak = 0xF0;
const byte PS2KeyboardTests::kExtended = 0xE0;
const byte PS2KeyboardTests::kMakeCodeHome = 0x6C;
const byte PS2KeyboardTests::kUp_KP8 = 0x75;

PS2P_IMPLEMENT(PS2KeyboardTests, protocol_);

TEST_F(PS2KeyboardTests, Empty) {
  EXPECT_EQ(0, keyboard_.available());
}

TEST_F(PS2KeyboardTests, OneByteMakeCode) {
  keyboard_.processByteForTesting(kMakeCodeA);
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_A, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, OneByteBreakCode) {
  keyboard_.processByteForTesting(kBreak);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeA);
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_A, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_RELEASED, k.type());
}

TEST_F(PS2KeyboardTests, BadOneByteMakeCode) {
  keyboard_.processByteForTesting(kMakeCodeBad);
  EXPECT_EQ(0, keyboard_.available());
}

TEST_F(PS2KeyboardTests, BadOneByteBreakCode) {
  keyboard_.processByteForTesting(kBreak);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeBad);
  EXPECT_EQ(0, keyboard_.available());
}

TEST_F(PS2KeyboardTests, TwoByteMakeCode) {
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeHome);
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_HOME, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, TwoByteBreakCode) {
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kBreak);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeHome);
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_HOME, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_RELEASED, k.type());
}

TEST_F(PS2KeyboardTests, BadTwoByteMakeCode) {
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeBad);
  EXPECT_EQ(0, keyboard_.available());
}

TEST_F(PS2KeyboardTests, BadTwoByteBreakCode) {
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kBreak);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeBad);
  EXPECT_EQ(0, keyboard_.available());
}

TEST_F(PS2KeyboardTests, NoAvailableAftetEnd) {
  keyboard_.processByteForTesting(kMakeCodeA);
  EXPECT_EQ(1, keyboard_.available());
  keyboard_.end();
  EXPECT_EQ(0, keyboard_.available());
}

TEST_F(PS2KeyboardTests, BreakExtended_IsAMakeCode) {
  keyboard_.processByteForTesting(kBreak);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeHome);

  // The kExtended byte was chewed up as an invalid break code.  The "home"
  // make code then is interpreted as KP_7.  Not sure if this is correct
  // theough.
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_KP_7, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, BreakBreak_IsAMakeCode) {
  keyboard_.processByteForTesting(kBreak);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kBreak);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeHome);

  // The kExtended byte was chewed up as an invalid break code.  The "home"
  // make code then is interpreted as KP_7.  Not sure if this is correct
  // theough.
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_KP_7, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, ExtendedExtended_IsAMakeCode) {
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeHome);

  // The kExtended byte was chewed up as an invalid break code.  The "home"
  // make code then is interpreted as KP_7.  Not sure if this is correct
  // theough.
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_KP_7, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, ExtendedBreakExtended_IsAMakeCode) {
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kBreak);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeHome);

  // The kExtended byte was chewed up as an invalid break code.  The "home"
  // make code then is interpreted as KP_7.  Not sure if this is correct
  // theough.
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_KP_7, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, Up_KP8) {
  // The up arrow key and the keypad 8 key (which is also an up key when num
  // lock is off) share the same byte, except the former is preceeded by an
  // extended byte.  Make sure they generate the correct key codes.
  keyboard_.processByteForTesting(kExtended);
  keyboard_.processByteForTesting(kUp_KP8);
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_UP, k.code());

  keyboard_.processByteForTesting(kUp_KP8);
  EXPECT_EQ(1, keyboard_.available());
  k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_KP_8, k.code());

  keyboard_.processByteForTesting(kUp_KP8);
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(1, keyboard_.available());
  EXPECT_EQ(PS2Keyboard::KC_KP_8, k.code());
}

TEST_F(PS2KeyboardTests, ExtendedBreakBreak_IsAMakeCode) {
  keyboard_.processByteForTesting(kExtended);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kBreak);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kBreak);
  EXPECT_EQ(0, keyboard_.available());
  keyboard_.processByteForTesting(kMakeCodeHome);

  // The kExtended byte was chewed up as an invalid break code.  The "home"
  // make code then is interpreted as KP_7.  Not sure if this is correct
  // theough.
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_KP_7, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, BufferOverflow) {
  // An array that contains more one-byte make codes than can be stored in the
  // keyboard buffer.
  byte data[] = {
    0x1C, 0x32, 0x21, 0x23, 0x24, 0x2B, 0x34, 0x33,
    0x43, 0x3B, 0x42, 0x4B, 0x3A, 0x31, 0x44, 0x4D,
    0x15, 0x2d
  };
  EXPECT_GT(numberof(data), PS2Keyboard::kBufferSize);

  // Key codes that correspond to the make codes above.
  byte kc[] = {
    PS2Keyboard::KC_A, PS2Keyboard::KC_B, PS2Keyboard::KC_C, PS2Keyboard::KC_D,
    PS2Keyboard::KC_E, PS2Keyboard::KC_F, PS2Keyboard::KC_G, PS2Keyboard::KC_H,
    PS2Keyboard::KC_I, PS2Keyboard::KC_J, PS2Keyboard::KC_K, PS2Keyboard::KC_L,
    PS2Keyboard::KC_M, PS2Keyboard::KC_N, PS2Keyboard::KC_O, PS2Keyboard::KC_P
  };
  EXPECT_EQ(numberof(kc), PS2Keyboard::kBufferSize);

  for (int i = 0; i < numberof(data); ++i) {
    int expected = i;
    if (expected > PS2Keyboard::kBufferSize)
      expected = PS2Keyboard::kBufferSize;

    EXPECT_EQ(expected, keyboard_.available());
    keyboard_.processByteForTesting(data[i]);
  }
  EXPECT_EQ(PS2Keyboard::kBufferSize, keyboard_.available());

  for (int i = 0; i < PS2Keyboard::kBufferSize; ++i) {
    EXPECT_EQ(kc[i], keyboard_.read().code());
  }
}

TEST_F(PS2KeyboardTests, Pause) {
  keyboard_.processByteForTesting(0xE1);
  keyboard_.processByteForTesting(0x14);
  keyboard_.processByteForTesting(0x77);
  keyboard_.processByteForTesting(0xE1);
  keyboard_.processByteForTesting(0xF0);
  keyboard_.processByteForTesting(0x14);
  keyboard_.processByteForTesting(0xF0);
  keyboard_.processByteForTesting(0x77);
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_PAUSE, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, PauseDifferentOrder) {
  keyboard_.processByteForTesting(0xE1);
  keyboard_.processByteForTesting(0x14);
  keyboard_.processByteForTesting(0x14);
  keyboard_.processByteForTesting(0xF0);
  keyboard_.processByteForTesting(0xE1);
  keyboard_.processByteForTesting(0xF0);
  keyboard_.processByteForTesting(0x77);
  keyboard_.processByteForTesting(0x77);
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_PAUSE, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, PauseOnly77s) {
  keyboard_.processByteForTesting(0xE1);
  keyboard_.processByteForTesting(0x77);
  keyboard_.processByteForTesting(0x77);
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_PAUSE, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, PauseInvalidBeforeFirst77) {
  keyboard_.processByteForTesting(0xE1);
  keyboard_.processByteForTesting(kMakeCodeA);
  EXPECT_EQ(0, keyboard_.available());

  // Make sure that subsequent bytes are handled normally.
  keyboard_.processByteForTesting(kMakeCodeB);
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_B, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, PauseInvalidBeforeSecond77) {
  keyboard_.processByteForTesting(0xE1);
  keyboard_.processByteForTesting(0x77);
  keyboard_.processByteForTesting(kMakeCodeA);
  EXPECT_EQ(0, keyboard_.available());

  // Make sure that subsequent bytes are handled normally.
  keyboard_.processByteForTesting(kMakeCodeB);
  EXPECT_EQ(1, keyboard_.available());
  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_B, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());
}

TEST_F(PS2KeyboardTests, Shift_Up) {
  // Press L_SHFT, press up arrow, release up arrow, release L_SHFT
  keyboard_.processByteForTesting(0x12);  // LSHFT pressed
  keyboard_.processByteForTesting(0xE0);  // up arrow pressed
  keyboard_.processByteForTesting(0x75);
  keyboard_.processByteForTesting(0xE0);  // up arrow released
  keyboard_.processByteForTesting(0xF0);
  keyboard_.processByteForTesting(0x75);
  keyboard_.processByteForTesting(0xF0);  // LSHFT released
  keyboard_.processByteForTesting(0x12);

  EXPECT_EQ(4, keyboard_.available());

  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_LSHFT, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());

  k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_UP, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());

  k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_UP, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_RELEASED, k.type());

  k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_LSHFT, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_RELEASED, k.type());

  EXPECT_EQ(0, keyboard_.available());
}

TEST_F(PS2KeyboardTests, ShiftHold_Up_TestKbd) {
  // Press L_SHFT, press up arrow, release up arrow, release L_SHFT
  keyboard_.processByteForTesting(0x12);  // LSHFT pressed
  keyboard_.processByteForTesting(0x12);  // LSHFT pressed

  // Bogus sequence generated by test keyboard.  Is this a bug in the
  // keyboard?  This PS2Keyboard should ignore it.
  keyboard_.processByteForTesting(0xE0);
  keyboard_.processByteForTesting(0xF0);
  keyboard_.processByteForTesting(0x12);

  keyboard_.processByteForTesting(0xE0);  // up arrow pressed
  keyboard_.processByteForTesting(0x75);
  keyboard_.processByteForTesting(0xE0);  // up arrow released
  keyboard_.processByteForTesting(0xF0);
  keyboard_.processByteForTesting(0x75);
  keyboard_.processByteForTesting(0xF0);  // LSHFT released
  keyboard_.processByteForTesting(0x12);

  EXPECT_EQ(5, keyboard_.available());

  PS2Keyboard::Key k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_LSHFT, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());

  k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_LSHFT, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());

  k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_UP, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_PRESSED, k.type());

  k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_UP, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_RELEASED, k.type());

  k = keyboard_.read();
  EXPECT_EQ(PS2Keyboard::KC_LSHFT, k.code());
  EXPECT_EQ(PS2Keyboard::KEY_RELEASED, k.type());

  EXPECT_EQ(0, keyboard_.available());
}

