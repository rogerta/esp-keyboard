
#include <unit_tests.h>

#include "ps2_keyboard.h"
#include "ps2_keyboard_manager.h"
#include "ps2_protocol.h"

#define numberof(a) (sizeof(a)/sizeof((a)[0]))

namespace {

const byte kBreak = 0xF0;

const byte kMakeCodeA = 0x1C;
const byte kMakeCodeB = 0x32;
const byte kMakeCodeC = 0x21;
const byte kMakeCodeD = 0x23;
const byte kMakeCodeE = 0x24;
const byte kMakeCodeF = 0x2B;
const byte kMakeCodeG = 0x34;
const byte kMakeCodeLSHFT = 0x12;
const byte kMakeCodeRSHFT = 0x59;
const byte kMakeCodeCapsLock = 0x58;

const byte kExtended = 0xE0;
const byte kMakeCodeHome = 0x6C;
const byte kMakeCodeRCTRL = 0x14;

// Used for generating the Pause key.
const byte kMakeCodeE1 = 0xE1;
const byte kMakeCode77 = 0x77;

}

class PS2KeyboardManagerBeginTests : public testing::TestCase {
 protected:
  PS2P_DECLARE(PS2KeyboardManagerBeginTests, protocol_);
  PS2Keyboard keyboard_;
  PS2KeyboardManager manager_;
 private:
  void SetUp() override {
    EXPECT_TRUE(protocol_.begin(2, 3));
    EXPECT_TRUE(keyboard_.begin(&protocol_));
  }
};

PS2P_IMPLEMENT(PS2KeyboardManagerBeginTests, protocol_);

TEST_F(PS2KeyboardManagerBeginTests, Enum) {
  EXPECT_LT(PS2Keyboard::KC_COUNT_NON_MODIFIER_KEYCODE,
            PS2Keyboard::KC_FIRST_MODIFIER_KEYCODE);
}

TEST_F(PS2KeyboardManagerBeginTests, Begin) {
  EXPECT_TRUE(manager_.begin(&keyboard_, 0));
}

TEST_F(PS2KeyboardManagerBeginTests, BeginWithNullKeyboard) {
  EXPECT_FALSE(manager_.begin(0, 0));
}

///////////////////////////////////////////////////////////////////////////////

class PS2KeyboardManagerTests : public testing::TestCase,
                                public arduino::mock::DelayHook {
 protected:
  PS2P_DECLARE(PS2KeyboardManagerTests, protocol_);
  PS2Keyboard keyboard_;
  PS2KeyboardManager manager_;
 private:
  void SetUp() override {
    EXPECT_TRUE(protocol_.begin(2, 3));
    EXPECT_TRUE(keyboard_.begin(&protocol_));
    EXPECT_TRUE(manager_.begin(&keyboard_, 0));
  }

  void RunDelayHook() override {
    // Generate a clock tick.  The parity bit may be wrong, but there is no
    // real device on the other end to check, so all is fine.
    protocol_.callIsrHandlerForTesting(LOW);
  }
};

PS2P_IMPLEMENT(PS2KeyboardManagerTests, protocol_);

TEST_F(PS2KeyboardManagerTests, Available) {
  EXPECT_EQ(0, manager_.available());
  keyboard_.processByteForTesting(kMakeCodeA);
  EXPECT_EQ(1, manager_.available());
}

TEST_F(PS2KeyboardManagerTests, ReportA) {
  EXPECT_EQ(0, manager_.available());
  keyboard_.processByteForTesting(kMakeCodeA);
  EXPECT_EQ(1, manager_.available());
  PS2KeyboardManager::Report report = manager_.read();
  EXPECT_EQ(0, manager_.available());
  EXPECT_TRUE(manager_.isKeyPressed(PS2Keyboard::KC_A));
}

TEST_F(PS2KeyboardManagerTests, ReportLSHFT) {
  keyboard_.processByteForTesting(kMakeCodeLSHFT);
  EXPECT_EQ(1, manager_.available());
  PS2KeyboardManager::Report report = manager_.read();
  EXPECT_EQ(0, manager_.available());
  EXPECT_TRUE(manager_.isShiftPressed());
}

TEST_F(PS2KeyboardManagerTests, ReportRSHFT) {
  keyboard_.processByteForTesting(kMakeCodeRSHFT);
  EXPECT_EQ(1, manager_.available());
  PS2KeyboardManager::Report report = manager_.read();
  EXPECT_EQ(0, manager_.available());
  EXPECT_TRUE(manager_.isShiftPressed());
}

TEST_F(PS2KeyboardManagerTests, PressedReleased) {
  keyboard_.processByteForTesting(kMakeCodeB);
  EXPECT_EQ(1, manager_.available());
  PS2KeyboardManager::Report report = manager_.read();
  EXPECT_TRUE(manager_.isKeyPressed(PS2Keyboard::KC_B));

  keyboard_.processByteForTesting(kBreak);
  keyboard_.processByteForTesting(kMakeCodeB);
  EXPECT_EQ(1, manager_.available());
  report = manager_.read();
  EXPECT_EQ(0, manager_.available());
  EXPECT_FALSE(manager_.isKeyPressed(PS2Keyboard::KC_B));
}

TEST_F(PS2KeyboardManagerTests, ReportModifers) {
  keyboard_.processByteForTesting(kMakeCodeRSHFT);
  EXPECT_EQ(1, manager_.available());
  PS2KeyboardManager::Report report = manager_.read();
  EXPECT_TRUE(report.isShiftPressed());
  EXPECT_FALSE(report.isControlPressed());
  EXPECT_FALSE(report.isAltPressed());
  EXPECT_FALSE(report.isGuiPressed());
}

TEST_F(PS2KeyboardManagerTests, ReportKey) {
  keyboard_.processByteForTesting(kMakeCodeB);
  EXPECT_EQ(1, manager_.available());
  PS2KeyboardManager::Report report = manager_.read();
  EXPECT_TRUE(report.isKeyPressed(PS2Keyboard::KC_B));
  EXPECT_FALSE(report.isKeyPressed(PS2Keyboard::KC_C));
}

TEST_F(PS2KeyboardManagerTests, ReportPauseCleared) {
  // This sequence will generate a Pause key.
  keyboard_.processByteForTesting(kMakeCodeE1);
  keyboard_.processByteForTesting(kMakeCode77);
  keyboard_.processByteForTesting(kMakeCode77);
  EXPECT_EQ(1, manager_.available());

  PS2KeyboardManager::Report report = manager_.read();
  EXPECT_TRUE(report.isKeyPressed(PS2Keyboard::KC_PAUSE));

  // Since the Pause key has no break code, read() should automatically clear
  // the Pause key state.  Make sure it does.
  report = manager_.read();
  EXPECT_FALSE(report.isKeyPressed(PS2Keyboard::KC_PAUSE));
}

TEST_F(PS2KeyboardManagerTests, CapsLockSetLED) {
  // Register a delay hook because the call to available() will cause the
  // manager to try and send commands to the ketyboard, which will block.
  // The hook will let the test pump the clock line to complete the send
  // operation.
  arduino::mock::ScopedDelayHook hook(this);

  // LED starts out off.
  EXPECT_EQ(0, manager_.getLEDs());

  // Press the CAPS LOCK button.  LED stays off.
  keyboard_.processByteForTesting(kMakeCodeCapsLock);
  EXPECT_EQ(0, manager_.getLEDs());
  PS2KeyboardManager::Report report = manager_.read();
  EXPECT_EQ(0, (int)manager_.getLEDs());

  // Release the CAPS LOCK button.  Make sure LED turns on.
  keyboard_.processByteForTesting(kBreak);
  keyboard_.processByteForTesting(kMakeCodeCapsLock);
  EXPECT_EQ(0, (int)manager_.getLEDs());
  report = manager_.read();
  EXPECT_EQ(PS2KeyboardManager::LED_CAPS_LOCK, (int)manager_.getLEDs());

  // Press the CAPS LOCK button.  Make sure LED stays on.
  keyboard_.processByteForTesting(kMakeCodeCapsLock);
  EXPECT_EQ(PS2KeyboardManager::LED_CAPS_LOCK, (int)manager_.getLEDs());
  report = manager_.read();
  EXPECT_EQ(PS2KeyboardManager::LED_CAPS_LOCK, (int)manager_.getLEDs());

  // Release the CAPS LOCK button.  Make sure LED turns off.
  keyboard_.processByteForTesting(kBreak);
  keyboard_.processByteForTesting(kMakeCodeCapsLock);
  EXPECT_EQ(PS2KeyboardManager::LED_CAPS_LOCK, (int)manager_.getLEDs());
  report = manager_.read();
  EXPECT_EQ(0, (int)manager_.getLEDs());
}

// Keyboard manager that transforms all keys to A.
class TransformingKeyboardManager : public PS2KeyboardManager {
  PS2Keyboard::Key transformKey(PS2Keyboard::Key key) override {
    return PS2Keyboard::Key(PS2Keyboard::KC_A, key.type());
  }
};

TEST_F(PS2KeyboardManagerTests, TransformKey) {
  TransformingKeyboardManager transformer;
  transformer.begin(&keyboard_, 0);
  keyboard_.processByteForTesting(kMakeCodeB);
  EXPECT_EQ(1, transformer.available());
  PS2KeyboardManager::Report report = transformer.read();
  EXPECT_TRUE(transformer.isKeyPressed(PS2Keyboard::KC_A));
}

TEST_F(PS2KeyboardManagerTests, ReportMoreThan6Keys) {
  // Press down more than 6 keys.
  keyboard_.processByteForTesting(kMakeCodeA);
  PS2KeyboardManager::Report report = manager_.read();
  keyboard_.processByteForTesting(kMakeCodeB);
  report = manager_.read();
  keyboard_.processByteForTesting(kMakeCodeC);
  report = manager_.read();
  keyboard_.processByteForTesting(kMakeCodeD);
  report = manager_.read();
  keyboard_.processByteForTesting(kMakeCodeE);
  report = manager_.read();
  keyboard_.processByteForTesting(kMakeCodeF);
  report = manager_.read();
  keyboard_.processByteForTesting(kMakeCodeG);
  report = manager_.read();

  for (int i = 0; i < numberof(report.keycodes); ++i) {
    EXPECT_EQ(PS2Keyboard::KC_ERROR_ROLL_OVER, report.keycodes[i]);
  }
}

// Figure out why a max of 4 keys can be held down at once.
